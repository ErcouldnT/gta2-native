// Libraries the native port doesn't provide: DirectPlay. Each reports failure so the game takes its "not available" path.
#include "port_internal.hpp"
#include <objbase.h>
#include "3rdParty/mss.h"
#include <dplay.h>
#include <dplobby.h>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// DirectPlay: no network play
// ---------------------------------------------------------------------------
extern "C" HRESULT WINAPI DirectPlayCreate(LPGUID, LPDIRECTPLAY* out, IUnknown*)
{
    if (out)
    {
        *out = NULL;
    }
    return DPERR_UNAVAILABLE;
}

extern "C" HRESULT WINAPI DirectPlayLobbyCreateW(LPGUID, LPDIRECTPLAYLOBBY* out, IUnknown*, LPVOID, DWORD)
{
    if (out)
    {
        *out = NULL;
    }
    return DPERR_UNAVAILABLE;
}
