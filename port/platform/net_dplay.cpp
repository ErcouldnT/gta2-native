// DirectPlay replacement: implements the COM interfaces NetPlay.cpp uses on top of one TCP
// connection to the relay server in server/. Everything runs on the game's thread: replies
// are awaited with poll(), and messages that arrive meanwhile wait in a queue for Receive().
#include "port_internal.hpp"
#include "net_port.hpp"
#include <objbase.h>
#include <dplay.h>
#include <dplobby.h>
#include <arpa/inet.h>
#include <deque>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#ifndef E_NOTIMPL
#define E_NOTIMPL ((HRESULT)0x80004001L)
#endif
#undef DPERR_CONNECTIONLOST
#define DPERR_CONNECTIONLOST DPERR_GENERIC

namespace
{
enum
{
    opList = 1, opCreate, opJoin, opClose, opCreatePlayer, opDestroyPlayer, opCreateGroup, opDestroyGroup,
    opAddToGroup, opDelFromGroup, opEnum, opSetGroupData, opSetFlags, opSend, opChat, opPing,
};
enum
{
    rOK = 100, rErr, rList, rEnum, evCreatePlayer, evCreateGroup, evDestroy, evAddToGroup, evDelFromGroup,
    evSetGroupData, evChat, evMessage, evSessionLost,
};
typedef std::vector<unsigned char> Bytes;

int gSocket = -1;
Bytes gIn;
std::deque<Bytes> gQueue;
std::string gError;

struct Writer
{
    Bytes b;
    explicit Writer(int op) { b.push_back(static_cast<unsigned char>(op)); }
    Writer& u32(DWORD v)
    {
        for (int i = 0; i < 4; i++)
        {
            b.push_back(static_cast<unsigned char>(v >> (8 * i)));
        }
        return *this;
    }
    Writer& raw(const void* p, size_t n)
    {
        const unsigned char* c = static_cast<const unsigned char*>(p);
        b.insert(b.end(), c, c + n);
        return *this;
    }
    Writer& bytes(const void* p, size_t n)
    {
        u32(static_cast<DWORD>(n));
        return raw(p, n);
    }
    Writer& wstr(const wchar_t* s)
    {
        size_t n = 0;
        while (s && s[n])
        {
            n++;
        }
        return bytes(s, (n + 1) * sizeof(wchar_t));
    }
};

struct Reader
{
    const unsigned char* p;
    size_t left;
    explicit Reader(const Bytes& b) : p(b.data() + 1), left(b.size() - 1) {}
    DWORD u32()
    {
        if (left < 4)
        {
            left = 0;
            return 0;
        }
        DWORD v = p[0] | p[1] << 8 | p[2] << 16 | static_cast<DWORD>(p[3]) << 24;
        p += 4;
        left -= 4;
        return v;
    }
    Bytes bytes()
    {
        size_t n = u32();
        if (n > left)
        {
            n = left;
        }
        Bytes v(p, p + n);
        p += n;
        left -= n;
        return v;
    }
    void raw(void* out, size_t n)
    {
        if (n <= left)
        {
            memcpy(out, p, n);
            p += n;
            left -= n;
        }
    }
};

void Disconnect(const char* why)
{
    if (gSocket >= 0)
    {
        port::Log("net: disconnected: %s\n", why);
        close(gSocket);
        gSocket = -1;
        gError = why;
        gIn.clear();
        Bytes lost(1, static_cast<unsigned char>(evSessionLost));
        gQueue.push_back(lost);
    }
}

// Moves whatever the socket has into gIn; returns false once the connection is gone
bool Pump(int timeoutMs)
{
    if (gSocket < 0)
    {
        return false;
    }
    pollfd pf = {gSocket, POLLIN, 0};
    int r = poll(&pf, 1, timeoutMs);
    if (r <= 0)
    {
        return r == 0;
    }
    unsigned char buf[8192];
    ssize_t n = recv(gSocket, buf, sizeof(buf), 0);
    if (n <= 0)
    {
        if (n < 0 && errno == EAGAIN)
        {
            return true;
        }
        Disconnect("server closed the connection");
        return false;
    }
    gIn.insert(gIn.end(), buf, buf + n);
    return true;
}

bool PopFrame(Bytes& out)
{
    if (gIn.size() < 4)
    {
        return false;
    }
    size_t n = gIn[0] | gIn[1] << 8 | gIn[2] << 16 | static_cast<size_t>(gIn[3]) << 24;
    if (gIn.size() < 4 + n)
    {
        return false;
    }
    out.assign(gIn.begin() + 4, gIn.begin() + 4 + n);
    gIn.erase(gIn.begin(), gIn.begin() + 4 + n);
    return !out.empty();
}

void SendFrame(const Bytes& payload)
{
    if (gSocket < 0)
    {
        return;
    }
    Bytes f;
    f.reserve(payload.size() + 4);
    size_t n = payload.size();
    for (int i = 0; i < 4; i++)
    {
        f.push_back(static_cast<unsigned char>(n >> (8 * i)));
    }
    f.insert(f.end(), payload.begin(), payload.end());
    size_t sent = 0;
    while (sent < f.size())
    {
        ssize_t w = send(gSocket, f.data() + sent, f.size() - sent, 0);
        if (w < 0 && (errno == EAGAIN || errno == EINTR))
        {
            pollfd pf = {gSocket, POLLOUT, 0};
            poll(&pf, 1, 100);
            continue;
        }
        if (w <= 0)
        {
            Disconnect("send failed");
            return;
        }
        sent += static_cast<size_t>(w);
    }
}

// Sends a request and waits for its reply, queueing anything else that arrives first
bool Call(const Writer& w, Bytes& reply)
{
    SendFrame(w.b);
    Uint64 start = SDL_GetTicks();
    while (SDL_GetTicks() - start < 8000)
    {
        Bytes f;
        while (PopFrame(f))
        {
            if (f[0] >= rOK && f[0] <= rEnum)
            {
                reply.swap(f);
                return reply[0] != rErr;
            }
            gQueue.push_back(f);
        }
        if (!Pump(20))
        {
            return false;
        }
    }
    Disconnect("server did not answer");
    return false;
}

bool CallOK(const Writer& w, DWORD* value = NULL)
{
    Bytes r;
    if (!Call(w, r))
    {
        return false;
    }
    if (value)
    {
        Reader rd(r);
        *value = rd.u32();
    }
    return true;
}

size_t Align8(size_t v)
{
    return (v + 7) & ~static_cast<size_t>(7);
}

// A system message: the fixed struct at the start of the buffer, its strings and data after it
template <class T>
T* BeginMsg(void* buf, size_t& used)
{
    used = Align8(sizeof(T));
    memset(buf, 0, used);
    return static_cast<T*>(buf);
}

void* AppendBlob(void* buf, size_t& used, const Bytes& b)
{
    void* out = static_cast<char*>(buf) + used;
    if (!b.empty())
    {
        memcpy(out, b.data(), b.size());
    }
    used = Align8(used + b.size());
    return out;
}

size_t NeededSize(const Bytes& f)
{
    return f.size() + 256; // always enough for the expansion into a DPMSG_* struct
}

class NetDirectPlay : public IDirectPlay3
{
  public:
    ULONG refs = 1;
    DWORD localPlayer = 0;

    HRESULT QueryInterface(REFIID, LPVOID* out) override
    {
        AddRef();
        *out = this;
        return S_OK;
    }
    ULONG AddRef() override { return ++refs; }
    ULONG Release() override
    {
        ULONG r = --refs;
        if (!r)
        {
            delete this;
        }
        return r;
    }

    HRESULT AddPlayerToGroup(DPID g, DPID p) override { return CallOK(Writer(opAddToGroup).u32(g).u32(p)) ? DP_OK : DPERR_GENERIC; }
    HRESULT Close() override
    {
        CallOK(Writer(opClose));
        return DP_OK;
    }
    HRESULT CreateGroup(LPDPID id, LPDPNAME name, LPVOID data, DWORD len, DWORD) override
    {
        return CallOK(Writer(opCreateGroup).wstr(name->lpszShortName).bytes(data, len), id) ? DP_OK : DPERR_GENERIC;
    }
    HRESULT CreatePlayer(LPDPID id, LPDPNAME name, HANDLE, LPVOID data, DWORD len, DWORD) override
    {
        if (!CallOK(Writer(opCreatePlayer).wstr(name->lpszShortName).bytes(data, len), id))
        {
            return DPERR_GENERIC;
        }
        localPlayer = *id;
        return DP_OK;
    }
    HRESULT DeletePlayerFromGroup(DPID g, DPID p) override { return CallOK(Writer(opDelFromGroup).u32(g).u32(p)) ? DP_OK : DPERR_GENERIC; }
    HRESULT DestroyGroup(DPID g) override { return CallOK(Writer(opDestroyGroup).u32(g)) ? DP_OK : DPERR_GENERIC; }
    HRESULT DestroyPlayer(DPID p) override { return CallOK(Writer(opDestroyPlayer).u32(p)) ? DP_OK : DPERR_GENERIC; }

    struct EnumPlayer
    {
        DWORD id;
        Bytes name, data;
    };
    struct EnumGroup
    {
        DWORD id;
        Bytes name, data;
        std::vector<DWORD> members;
    };
    std::vector<EnumPlayer> players;
    std::vector<EnumGroup> groups;

    bool Refresh()
    {
        Bytes r;
        if (!Call(Writer(opEnum), r) || r[0] != rEnum)
        {
            return false;
        }
        Reader rd(r);
        groups.clear();
        players.clear();
        for (DWORD n = rd.u32(); n; n--)
        {
            EnumGroup g;
            g.id = rd.u32();
            g.name = rd.bytes();
            g.data = rd.bytes();
            for (DWORD m = rd.u32(); m; m--)
            {
                g.members.push_back(rd.u32());
            }
            groups.push_back(g);
        }
        for (DWORD n = rd.u32(); n; n--)
        {
            EnumPlayer p;
            p.id = rd.u32();
            p.name = rd.bytes();
            p.data = rd.bytes();
            players.push_back(p);
        }
        return true;
    }

    static DPNAME MakeName(const Bytes& b)
    {
        DPNAME n;
        memset(&n, 0, sizeof(n));
        n.dwSize = sizeof(DPNAME);
        n.lpszShortName = reinterpret_cast<LPWSTR>(const_cast<unsigned char*>(b.data()));
        return n;
    }

    HRESULT EnumGroupPlayers(DPID g, LPGUID, LPDPENUMPLAYERSCALLBACK2 cb, LPVOID ctx, DWORD) override
    {
        if (!Refresh())
        {
            return DPERR_GENERIC;
        }
        for (const EnumGroup& grp : groups)
        {
            if (grp.id != g)
            {
                continue;
            }
            for (DWORD m : grp.members)
            {
                for (const EnumPlayer& p : players)
                {
                    if (p.id == m)
                    {
                        DPNAME n = MakeName(p.name);
                        cb(p.id, DPPLAYERTYPE_PLAYER, &n, 0, ctx);
                    }
                }
            }
        }
        return DP_OK;
    }
    HRESULT EnumGroups(LPGUID, LPDPENUMPLAYERSCALLBACK2 cb, LPVOID ctx, DWORD) override
    {
        if (!Refresh())
        {
            return DPERR_GENERIC;
        }
        for (const EnumGroup& g : groups)
        {
            DPNAME n = MakeName(g.name);
            cb(g.id, DPPLAYERTYPE_GROUP, &n, 0, ctx);
        }
        return DP_OK;
    }
    HRESULT EnumPlayers(LPGUID, LPDPENUMPLAYERSCALLBACK2, LPVOID, DWORD) override { return DPERR_UNSUPPORTED; }

    HRESULT EnumSessions(LPDPSESSIONDESC2 filter, DWORD, LPDPENUMSESSIONSCALLBACK2 cb, LPVOID ctx, DWORD) override
    {
        Bytes r;
        if (!Call(Writer(opList), r) || r[0] != rList)
        {
            return DPERR_GENERIC;
        }
        Reader rd(r);
        for (DWORD n = rd.u32(); n; n--)
        {
            DPSESSIONDESC2 d;
            memset(&d, 0, sizeof(d));
            d.dwSize = sizeof(d);
            d.guidApplication = filter->guidApplication;
            rd.raw(&d.guidInstance, sizeof(GUID));
            Bytes name = rd.bytes();
            d.dwFlags = rd.u32();
            d.dwMaxPlayers = rd.u32();
            d.dwCurrentPlayers = rd.u32();
            d.lpszSessionName = reinterpret_cast<LPWSTR>(name.data());
            DWORD timeout = 0;
            cb(&d, &timeout, 0, ctx);
        }
        return DP_OK;
    }

    HRESULT GetCaps(LPDPCAPS, DWORD) override { return DPERR_UNSUPPORTED; }

    static HRESULT CopyOut(const Bytes& b, LPVOID out, LPDWORD len)
    {
        if (!out || *len < b.size())
        {
            bool tooSmall = !b.empty();
            *len = static_cast<DWORD>(b.size());
            return tooSmall ? DPERR_BUFFERTOOSMALL : DP_OK;
        }
        if (!b.empty())
        {
            memcpy(out, b.data(), b.size());
        }
        *len = static_cast<DWORD>(b.size());
        return DP_OK;
    }

    HRESULT GetGroupData(DPID g, LPVOID out, LPDWORD len, DWORD) override
    {
        if (!Refresh())
        {
            return DPERR_GENERIC;
        }
        for (const EnumGroup& grp : groups)
        {
            if (grp.id == g)
            {
                return CopyOut(grp.data, out, len);
            }
        }
        return DPERR_INVALIDGROUP;
    }
    HRESULT GetGroupName(DPID, LPVOID, LPDWORD) override { return DPERR_UNSUPPORTED; }
    HRESULT GetMessageCount(DPID, LPDWORD n) override
    {
        *n = static_cast<DWORD>(gQueue.size());
        return DP_OK;
    }
    HRESULT GetPlayerAddress(DPID, LPVOID, LPDWORD) override { return DPERR_UNAVAILABLE; }
    HRESULT GetPlayerCaps(DPID, LPDPCAPS, DWORD) override { return DPERR_UNSUPPORTED; }
    HRESULT GetPlayerData(DPID p, LPVOID out, LPDWORD len, DWORD) override
    {
        if (!Refresh())
        {
            return DPERR_GENERIC;
        }
        for (const EnumPlayer& e : players)
        {
            if (e.id == p)
            {
                return CopyOut(e.data, out, len);
            }
        }
        return DPERR_INVALIDPLAYER;
    }
    HRESULT GetPlayerName(DPID, LPVOID, LPDWORD) override { return DPERR_UNSUPPORTED; }
    HRESULT GetSessionDesc(LPVOID, LPDWORD) override { return DPERR_UNSUPPORTED; }
    HRESULT Initialize(LPGUID) override { return DP_OK; }

    HRESULT Open(LPDPSESSIONDESC2 d, DWORD flags) override
    {
        if (flags & DPOPEN_CREATE)
        {
            Bytes r;
            if (!Call(Writer(opCreate).wstr(d->lpszSessionName).u32(d->dwFlags).u32(d->dwMaxPlayers), r))
            {
                return DPERR_GENERIC;
            }
            Reader rd(r);
            rd.raw(&d->guidInstance, sizeof(GUID));
            return DP_OK;
        }
        Writer w(opJoin);
        w.raw(&d->guidInstance, sizeof(GUID));
        return CallOK(w) ? DP_OK : DPERR_NOSESSIONS;
    }

    HRESULT Receive(LPDPID from, LPDPID to, DWORD, LPVOID buf, LPDWORD len) override
    {
        Bytes f;
        while (PopFrame(f))
        {
            gQueue.push_back(f);
        }
        if (gQueue.empty())
        {
            Pump(1);
            while (PopFrame(f))
            {
                gQueue.push_back(f);
            }
            if (gQueue.empty())
            {
                return DPERR_NOMESSAGES;
            }
        }
        if (*len < NeededSize(gQueue.front()))
        {
            port::Log("net: message too big for the receive buffer, dropped\n");
            gQueue.pop_front();
            return DPERR_NOMESSAGES;
        }
        f.swap(gQueue.front());
        gQueue.pop_front();
        Reader r(f);
        size_t used = 0;
        *from = DPID_SYSMSG;
        *to = localPlayer;
        switch (f[0])
        {
            case evMessage:
            {
                *from = r.u32();
                *to = r.u32();
                Bytes data = r.bytes();
                if (!*to)
                {
                    *to = localPlayer;
                }
                if (!data.empty())
                {
                    memcpy(buf, data.data(), data.size());
                }
                *len = static_cast<DWORD>(data.size());
                return DP_OK;
            }
            case evCreatePlayer:
            case evCreateGroup:
            {
                DPMSG_CREATEPLAYERORGROUP* m = BeginMsg<DPMSG_CREATEPLAYERORGROUP>(buf, used);
                m->dwType = DPSYS_CREATEPLAYERORGROUP;
                m->dwPlayerType = f[0] == evCreatePlayer ? DPPLAYERTYPE_PLAYER : DPPLAYERTYPE_GROUP;
                m->dpId = r.u32();
                Bytes name = r.bytes(), data = r.bytes();
                m->dpnName.dwSize = sizeof(DPNAME);
                m->dpnName.lpszShortName = static_cast<LPWSTR>(AppendBlob(buf, used, name));
                m->lpData = AppendBlob(buf, used, data);
                m->dwDataSize = static_cast<DWORD>(data.size());
                break;
            }
            case evDestroy:
            {
                DPMSG_DESTROYPLAYERORGROUP* m = BeginMsg<DPMSG_DESTROYPLAYERORGROUP>(buf, used);
                m->dwType = DPSYS_DESTROYPLAYERORGROUP;
                m->dpId = r.u32();
                m->dwPlayerType = r.u32() ? DPPLAYERTYPE_PLAYER : DPPLAYERTYPE_GROUP;
                break;
            }
            case evAddToGroup:
            case evDelFromGroup:
            {
                DPMSG_ADDPLAYERTOGROUP* m = BeginMsg<DPMSG_ADDPLAYERTOGROUP>(buf, used);
                m->dwType = f[0] == evAddToGroup ? DPSYS_ADDPLAYERTOGROUP : DPSYS_DELETEPLAYERFROMGROUP;
                m->dpIdGroup = r.u32();
                m->dpIdPlayer = r.u32();
                break;
            }
            case evSetGroupData:
            {
                DPMSG_SETPLAYERORGROUPDATA* m = BeginMsg<DPMSG_SETPLAYERORGROUPDATA>(buf, used);
                m->dwType = DPSYS_SETPLAYERORGROUPDATA;
                m->dwPlayerType = DPPLAYERTYPE_GROUP;
                m->dpId = r.u32();
                Bytes data = r.bytes();
                m->lpData = AppendBlob(buf, used, data);
                m->dwDataSize = static_cast<DWORD>(data.size());
                break;
            }
            case evChat:
            {
                DPMSG_CHAT* m = BeginMsg<DPMSG_CHAT>(buf, used);
                m->dwType = DPSYS_CHAT;
                m->idFromPlayer = r.u32();
                m->idToGroup = r.u32();
                Bytes text = r.bytes();
                DPCHAT* c = static_cast<DPCHAT*>(static_cast<void*>(static_cast<char*>(buf) + used));
                used = Align8(used + sizeof(DPCHAT));
                memset(c, 0, sizeof(DPCHAT));
                c->dwSize = sizeof(DPCHAT);
                c->lpszMessage = static_cast<LPWSTR>(AppendBlob(buf, used, text));
                m->lpChat = c;
                break;
            }
            case evSessionLost:
            {
                DPMSG_SESSIONLOST* m = BeginMsg<DPMSG_SESSIONLOST>(buf, used);
                m->dwType = DPSYS_SESSIONLOST;
                port::NetNotifySessionLost();
                break;
            }
            default:
                return DPERR_NOMESSAGES;
        }
        *len = static_cast<DWORD>(used);
        return DP_OK;
    }

    HRESULT Send(DPID from, DPID to, DWORD, LPVOID data, DWORD len) override
    {
        SendFrame(Writer(opSend).u32(from).u32(to).bytes(data, len).b);
        return gSocket >= 0 ? DP_OK : DPERR_CONNECTIONLOST;
    }
    HRESULT SetGroupData(DPID g, LPVOID data, DWORD len, DWORD) override
    {
        return CallOK(Writer(opSetGroupData).u32(g).bytes(data, len)) ? DP_OK : DPERR_GENERIC;
    }
    HRESULT SetGroupName(DPID, LPDPNAME, DWORD) override { return DPERR_UNSUPPORTED; }
    HRESULT SetPlayerData(DPID, LPVOID, DWORD, DWORD) override { return DPERR_UNSUPPORTED; }
    HRESULT SetPlayerName(DPID, LPDPNAME, DWORD) override { return DPERR_UNSUPPORTED; }
    HRESULT SetSessionDesc(LPDPSESSIONDESC2 d, DWORD) override { return CallOK(Writer(opSetFlags).u32(d->dwFlags)) ? DP_OK : DPERR_GENERIC; }

    // IDirectPlay3
    HRESULT AddGroupToGroup(DPID, DPID) override { return DPERR_UNSUPPORTED; }
    HRESULT CreateGroupInGroup(DPID, LPDPID, LPDPNAME, LPVOID, DWORD, DWORD) override { return DPERR_UNSUPPORTED; }
    HRESULT DeleteGroupFromGroup(DPID, DPID) override { return DPERR_UNSUPPORTED; }
    HRESULT EnumConnections(LPCGUID, LPDPENUMCONNECTIONSCALLBACK cb, LPVOID ctx, DWORD) override
    {
        static unsigned char connection[16] = {'G', 'T', 'A', '2', 'R', 'E', 'L', 'A', 'Y'};
        static wchar_t name[] = {'R', 'e', 'l', 'a', 'y', 0};
        DPNAME n;
        memset(&n, 0, sizeof(n));
        n.dwSize = sizeof(DPNAME);
        n.lpszShortName = name;
        GUID sp = DPSPGUID_IPX; // the one NetPlay looks for when no connection is configured
        cb(&sp, connection, sizeof(connection), &n, 0, ctx);
        return DP_OK;
    }
    HRESULT EnumGroupsInGroup(DPID, LPGUID, LPDPENUMPLAYERSCALLBACK2, LPVOID, DWORD) override { return DPERR_UNSUPPORTED; }
    HRESULT GetGroupConnectionSettings(DWORD, DPID, LPVOID, LPDWORD) override { return DPERR_UNSUPPORTED; }
    HRESULT InitializeConnection(LPVOID, DWORD) override { return gSocket >= 0 ? DP_OK : DPERR_GENERIC; }
    HRESULT SecureOpen(LPCDPSESSIONDESC2, DWORD, LPCDPSECURITYDESC, LPCDPCREDENTIALS) override { return DPERR_UNSUPPORTED; }
    HRESULT SendChatMessage(DPID from, DPID to, DWORD, LPDPCHAT chat) override
    {
        SendFrame(Writer(opChat).u32(from).u32(to).wstr(chat->lpszMessage).b);
        return gSocket >= 0 ? DP_OK : DPERR_CONNECTIONLOST;
    }
    HRESULT SetGroupConnectionSettings(DWORD, DPID, LPDPLCONNECTION) override { return DPERR_UNSUPPORTED; }
    HRESULT StartSession(DWORD, DPID) override { return DPERR_UNSUPPORTED; }
    HRESULT GetGroupFlags(DPID, LPDWORD) override { return DPERR_UNSUPPORTED; }
    HRESULT GetGroupParent(DPID, LPDPID) override { return DPERR_UNSUPPORTED; }
    HRESULT GetPlayerAccount(DPID, DWORD, LPVOID, LPDWORD) override { return DPERR_UNSUPPORTED; }
    HRESULT GetPlayerFlags(DPID, LPDWORD) override { return DPERR_UNSUPPORTED; }
};

class NetLobby : public IDirectPlayLobby2
{
  public:
    ULONG refs = 1;
    HRESULT QueryInterface(REFIID, LPVOID* out) override
    {
        AddRef();
        *out = this;
        return S_OK;
    }
    ULONG AddRef() override { return ++refs; }
    ULONG Release() override
    {
        ULONG r = --refs;
        if (!r)
        {
            delete this;
        }
        return r;
    }
    HRESULT Connect(DWORD, LPDIRECTPLAY2*, IUnknown*) override { return DPERR_UNSUPPORTED; }
    HRESULT CreateAddress(REFGUID, REFGUID, LPCVOID, DWORD, LPVOID, LPDWORD) override { return DPERR_UNSUPPORTED; }
    HRESULT EnumAddress(LPDPENUMADDRESSCALLBACK, LPCVOID, DWORD, LPVOID) override { return DP_OK; }
    HRESULT EnumAddressTypes(LPDPLENUMADDRESSTYPESCALLBACK, REFGUID, LPVOID, DWORD) override { return DPERR_UNSUPPORTED; }
    HRESULT EnumLocalApplications(LPDPLENUMLOCALAPPLICATIONSCALLBACK, LPVOID, DWORD) override { return DPERR_UNSUPPORTED; }
    HRESULT GetConnectionSettings(DWORD, LPVOID, LPDWORD) override { return DPERR_UNSUPPORTED; }
    HRESULT ReceiveLobbyMessage(DWORD, DWORD, LPDWORD, LPVOID, LPDWORD) override { return DPERR_UNSUPPORTED; }
    HRESULT RunApplication(DWORD, LPDWORD, LPDPLCONNECTION, HANDLE) override { return DPERR_UNSUPPORTED; }
    HRESULT SendLobbyMessage(DWORD, DWORD, LPVOID, DWORD) override { return DPERR_UNSUPPORTED; }
    HRESULT SetConnectionSettings(DWORD, DWORD, LPDPLCONNECTION) override { return DPERR_UNSUPPORTED; }
    HRESULT SetLobbyMessageEvent(DWORD, DWORD, HANDLE) override { return DPERR_UNSUPPORTED; }
    HRESULT CreateCompoundAddress(LPCDPCOMPOUNDADDRESSELEMENT, DWORD, LPVOID, LPDWORD) override { return DPERR_UNSUPPORTED; }
};
} // namespace

namespace port
{
bool NetConnect(const std::string& hostPort, std::string& error)
{
    NetClose();
    std::string host = hostPort, service = "28900";
    size_t colon = hostPort.rfind(':');
    if (colon != std::string::npos)
    {
        host = hostPort.substr(0, colon);
        service = hostPort.substr(colon + 1);
    }
    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* list = NULL;
    if (getaddrinfo(host.c_str(), service.c_str(), &hints, &list) != 0)
    {
        error = "Sunucu adresi bulunamadi";
        return false;
    }
    for (addrinfo* a = list; a && gSocket < 0; a = a->ai_next)
    {
        int s = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (s < 0)
        {
            continue;
        }
        fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);
        int r = connect(s, a->ai_addr, a->ai_addrlen);
        if (r != 0 && errno == EINPROGRESS)
        {
            pollfd pf = {s, POLLOUT, 0};
            int err = 0;
            socklen_t len = sizeof(err);
            if (poll(&pf, 1, 5000) == 1 && getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &len) == 0 && err == 0)
            {
                r = 0;
            }
        }
        if (r != 0)
        {
            close(s);
            continue;
        }
        int one = 1;
        setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
        gSocket = s;
    }
    freeaddrinfo(list);
    if (gSocket < 0)
    {
        error = "Sunucuya baglanilamadi";
        return false;
    }
    gQueue.clear();
    gIn.clear();
    Log("net: connected to %s\n", hostPort.c_str());
    return true;
}

void NetClose()
{
    if (gSocket >= 0)
    {
        close(gSocket);
        gSocket = -1;
    }
    gQueue.clear();
    gIn.clear();
}

bool NetConnected()
{
    return gSocket >= 0;
}

void NetKeepAlive()
{
    static Uint64 last = 0;
    if (gSocket >= 0 && SDL_GetTicks() - last > 20000)
    {
        last = SDL_GetTicks();
        Bytes f;
        SendFrame(Writer(opPing).b);
    }
}
} // namespace port

extern "C" HRESULT WINAPI DirectPlayCreate(LPGUID, LPDIRECTPLAY* out, IUnknown*)
{
    *out = NULL;
    return DPERR_UNAVAILABLE; // only used to list modems
}

extern "C" HRESULT WINAPI DirectPlayLobbyCreateW(LPGUID, LPDIRECTPLAYLOBBY* out, IUnknown*, LPVOID, DWORD)
{
    *out = reinterpret_cast<LPDIRECTPLAYLOBBY>(new NetLobby());
    return DP_OK;
}

HRESULT CreateNetDirectPlay(LPVOID* out)
{
    *out = static_cast<IDirectPlay3*>(new NetDirectPlay());
    return S_OK;
}

// The SDK header forgets "PURE" on these, so the base class needs bodies (NetDirectPlay overrides them)
HRESULT IDirectPlay3::SendChatMessage(DPID, DPID, DWORD, LPDPCHAT) { return E_NOTIMPL; }
HRESULT IDirectPlay3::StartSession(DWORD, DPID) { return E_NOTIMPL; }
HRESULT IDirectPlay3::GetGroupFlags(DPID, LPDWORD) { return E_NOTIMPL; }
HRESULT IDirectPlay3::GetGroupParent(DPID, LPDPID) { return E_NOTIMPL; }
HRESULT IDirectPlay3::GetPlayerFlags(DPID, LPDWORD) { return E_NOTIMPL; }
