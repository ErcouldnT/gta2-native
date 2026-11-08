// 16-bit wchar_t string functions (the port is built with -fshort-wchar so GXT text keeps its
// UTF-16 layout; the C library's wide functions assume 32-bit wchar_t and can't be used).
// Format strings follow MSVC rules: in a wide format %s is a wide string, %S/%hs a narrow one.
#include "port_prelude.h"
#include <string>

extern "C" {

size_t port_wcslen(const wchar_t* s)
{
    size_t n = 0;
    while (s[n])
    {
        n++;
    }
    return n;
}

wchar_t* port_wcscpy(wchar_t* d, const wchar_t* s)
{
    wchar_t* r = d;
    while ((*d++ = *s++))
    {
    }
    return r;
}

wchar_t* port_wcsncpy(wchar_t* d, const wchar_t* s, size_t n)
{
    size_t i = 0;
    for (; i < n && s[i]; i++)
    {
        d[i] = s[i];
    }
    for (; i < n; i++)
    {
        d[i] = 0;
    }
    return d;
}

wchar_t* port_wcscat(wchar_t* d, const wchar_t* s)
{
    port_wcscpy(d + port_wcslen(d), s);
    return d;
}

wchar_t* port_wcsncat(wchar_t* d, const wchar_t* s, size_t n)
{
    wchar_t* p = d + port_wcslen(d);
    size_t i = 0;
    for (; i < n && s[i]; i++)
    {
        p[i] = s[i];
    }
    p[i] = 0;
    return d;
}

int port_wcscmp(const wchar_t* a, const wchar_t* b)
{
    while (*a && *a == *b)
    {
        a++;
        b++;
    }
    return static_cast<int>(static_cast<unsigned short>(*a)) - static_cast<int>(static_cast<unsigned short>(*b));
}

int port_wcsncmp(const wchar_t* a, const wchar_t* b, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        if (a[i] != b[i] || !a[i])
        {
            return static_cast<int>(static_cast<unsigned short>(a[i])) - static_cast<int>(static_cast<unsigned short>(b[i]));
        }
    }
    return 0;
}

wchar_t port_towupper(wchar_t c)
{
    if (c >= 'a' && c <= 'z')
    {
        return c - 32;
    }
    if (c >= 0xE0 && c <= 0xFE && c != 0xF7)
    {
        return c - 32;
    }
    return c;
}

wchar_t port_towlower(wchar_t c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return c + 32;
    }
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7)
    {
        return c + 32;
    }
    return c;
}

int port_wcsicmp(const wchar_t* a, const wchar_t* b)
{
    while (*a && port_towlower(*a) == port_towlower(*b))
    {
        a++;
        b++;
    }
    return static_cast<int>(port_towlower(*a)) - static_cast<int>(port_towlower(*b));
}

wchar_t* port_wcschr(const wchar_t* s, wchar_t c)
{
    for (;; s++)
    {
        if (*s == c)
        {
            return const_cast<wchar_t*>(s);
        }
        if (!*s)
        {
            return NULL;
        }
    }
}

wchar_t* port_wcsrchr(const wchar_t* s, wchar_t c)
{
    const wchar_t* r = NULL;
    for (;; s++)
    {
        if (*s == c)
        {
            r = s;
        }
        if (!*s)
        {
            return const_cast<wchar_t*>(r);
        }
    }
}

wchar_t* port_wcsstr(const wchar_t* s, const wchar_t* n)
{
    size_t nl = port_wcslen(n);
    if (!nl)
    {
        return const_cast<wchar_t*>(s);
    }
    for (; *s; s++)
    {
        if (port_wcsncmp(s, n, nl) == 0)
        {
            return const_cast<wchar_t*>(s);
        }
    }
    return NULL;
}

int port_iswspace(wchar_t c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

int port_iswdigit(wchar_t c)
{
    return c >= '0' && c <= '9';
}

static void append_padded(std::u16string& out, const std::u16string& s, int width, bool left)
{
    int pad = width > static_cast<int>(s.size()) ? width - static_cast<int>(s.size()) : 0;
    if (!left)
    {
        out.append(pad, u' ');
    }
    out += s;
    if (left)
    {
        out.append(pad, u' ');
    }
}

static std::u16string widen(const char* s)
{
    std::u16string r;
    if (!s)
    {
        s = "(null)";
    }
    while (*s)
    {
        r.push_back(static_cast<unsigned char>(*s++));
    }
    return r;
}

static std::u16string format_wide(const wchar_t* fmt, va_list ap)
{
    std::u16string out;
    while (*fmt)
    {
        if (*fmt != '%')
        {
            out.push_back(static_cast<char16_t>(*fmt++));
            continue;
        }
        const wchar_t* specStart = fmt++;
        if (*fmt == '%')
        {
            out.push_back(u'%');
            fmt++;
            continue;
        }
        std::string flags;
        while (*fmt == '-' || *fmt == '+' || *fmt == ' ' || *fmt == '#' || *fmt == '0')
        {
            flags.push_back(static_cast<char>(*fmt++));
        }
        int width = -1;
        if (*fmt == '*')
        {
            width = va_arg(ap, int);
            fmt++;
        }
        else
        {
            while (*fmt >= '0' && *fmt <= '9')
            {
                width = (width < 0 ? 0 : width) * 10 + (*fmt++ - '0');
            }
        }
        int prec = -1;
        if (*fmt == '.')
        {
            fmt++;
            prec = 0;
            if (*fmt == '*')
            {
                prec = va_arg(ap, int);
                fmt++;
            }
            else
            {
                while (*fmt >= '0' && *fmt <= '9')
                {
                    prec = prec * 10 + (*fmt++ - '0');
                }
            }
        }
        // length modifiers
        int lenH = 0, lenL = 0, len64 = 0;
        for (;;)
        {
            if (*fmt == 'h')
            {
                lenH++;
                fmt++;
            }
            else if (*fmt == 'l')
            {
                lenL++;
                fmt++;
            }
            else if (*fmt == 'I' && fmt[1] == '6' && fmt[2] == '4')
            {
                len64 = 1;
                fmt += 3;
            }
            else if (*fmt == 'L' || *fmt == 'q' || *fmt == 'z' || *fmt == 'j' || *fmt == 't')
            {
                if (*fmt == 'L' || *fmt == 'q')
                {
                    len64 = 1;
                }
                fmt++;
            }
            else
            {
                break;
            }
        }
        wchar_t conv = *fmt ? *fmt++ : 0;
        bool left = flags.find('-') != std::string::npos;
        switch (conv)
        {
            case 's':
            case 'S':
            {
                bool narrow = (conv == 'S' && !lenL) || lenH;
                std::u16string s;
                if (narrow)
                {
                    s = widen(va_arg(ap, const char*));
                }
                else
                {
                    const wchar_t* ws = va_arg(ap, const wchar_t*);
                    if (!ws)
                    {
                        s = u"(null)";
                    }
                    else
                    {
                        s.assign(reinterpret_cast<const char16_t*>(ws), port_wcslen(ws));
                    }
                }
                if (prec >= 0 && static_cast<int>(s.size()) > prec)
                {
                    s.resize(prec);
                }
                append_padded(out, s, width, left);
                break;
            }
            case 'c':
            case 'C':
            {
                std::u16string s(1, static_cast<char16_t>(va_arg(ap, int)));
                append_padded(out, s, width, left);
                break;
            }
            case 'd':
            case 'i':
            case 'u':
            case 'x':
            case 'X':
            case 'o':
            case 'p':
            case 'f':
            case 'F':
            case 'e':
            case 'E':
            case 'g':
            case 'G':
            {
                char spec[64];
                std::string nfmt = "%" + flags;
                if (width >= 0)
                {
                    nfmt += std::to_string(width);
                }
                if (prec >= 0)
                {
                    nfmt += "." + std::to_string(prec);
                }
                if (conv == 'p')
                {
                    nfmt += "p";
                    snprintf(spec, sizeof(spec), nfmt.c_str(), va_arg(ap, void*));
                }
                else if (conv == 'f' || conv == 'F' || conv == 'e' || conv == 'E' || conv == 'g' || conv == 'G')
                {
                    nfmt.push_back(static_cast<char>(conv));
                    snprintf(spec, sizeof(spec), nfmt.c_str(), va_arg(ap, double));
                }
                else if (len64 || lenL >= 2)
                {
                    nfmt += "ll";
                    nfmt.push_back(static_cast<char>(conv));
                    snprintf(spec, sizeof(spec), nfmt.c_str(), va_arg(ap, long long));
                }
                else
                {
                    // MSVC long is 32-bit, so %ld takes an int-sized argument here too
                    nfmt.push_back(static_cast<char>(conv));
                    int v = va_arg(ap, int);
                    if (lenH == 1)
                    {
                        v = (conv == 'd' || conv == 'i') ? static_cast<short>(v) : static_cast<unsigned short>(v);
                    }
                    snprintf(spec, sizeof(spec), nfmt.c_str(), v);
                }
                out += widen(spec);
                break;
            }
            default:
                // unknown spec: copy verbatim
                for (const wchar_t* p = specStart; p < fmt; p++)
                {
                    out.push_back(static_cast<char16_t>(*p));
                }
                break;
        }
    }
    return out;
}

int port_vswprintf(wchar_t* buf, const wchar_t* fmt, va_list ap)
{
    std::u16string s = format_wide(fmt, ap);
    memcpy(buf, s.data(), s.size() * sizeof(wchar_t));
    buf[s.size()] = 0;
    return static_cast<int>(s.size());
}

int port_swprintf(wchar_t* buf, const wchar_t* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = port_vswprintf(buf, fmt, ap);
    va_end(ap);
    return r;
}

int port_snwprintf(wchar_t* buf, size_t n, const wchar_t* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    std::u16string s = format_wide(fmt, ap);
    va_end(ap);
    if (n == 0)
    {
        return -1;
    }
    size_t c = s.size() < n ? s.size() : n;
    memcpy(buf, s.data(), c * sizeof(wchar_t));
    if (s.size() < n)
    {
        buf[s.size()] = 0;
        return static_cast<int>(s.size());
    }
    return -1;
}

int port_wprintf(const wchar_t* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    std::u16string s = format_wide(fmt, ap);
    va_end(ap);
    for (char16_t c : s)
    {
        putchar(c < 0x80 ? static_cast<char>(c) : '?');
    }
    return static_cast<int>(s.size());
}

wchar_t* port_itow(int value, wchar_t* str, int radix)
{
    char tmp[40];
    port_itoa(value, tmp, radix);
    size_t i = 0;
    for (; tmp[i]; i++)
    {
        str[i] = static_cast<unsigned char>(tmp[i]);
    }
    str[i] = 0;
    return str;
}

long port_wcstol(const wchar_t* s, wchar_t** end, int base)
{
    char tmp[64];
    size_t i = 0;
    for (; s[i] && i < sizeof(tmp) - 1; i++)
    {
        tmp[i] = s[i] < 0x80 ? static_cast<char>(s[i]) : '?';
    }
    tmp[i] = 0;
    char* e = NULL;
    long v = strtol(tmp, &e, base);
    if (end)
    {
        *end = const_cast<wchar_t*>(s) + (e - tmp);
    }
    return v;
}

int port_wtoi(const wchar_t* s)
{
    return static_cast<int>(port_wcstol(s, NULL, 10));
}

size_t port_mbstowcs(wchar_t* d, const char* s, size_t n)
{
    size_t i = 0;
    for (; s[i] && (!d || i < n); i++)
    {
        if (d)
        {
            d[i] = static_cast<unsigned char>(s[i]);
        }
    }
    if (d && i < n)
    {
        d[i] = 0;
    }
    return i;
}

size_t port_wcstombs(char* d, const wchar_t* s, size_t n)
{
    size_t i = 0;
    for (; s[i] && (!d || i < n); i++)
    {
        if (d)
        {
            d[i] = s[i] < 0x100 ? static_cast<char>(s[i]) : '?';
        }
    }
    if (d && i < n)
    {
        d[i] = 0;
    }
    return i;
}

// ---- narrow MSVC CRT extras ----

char* port_strupr(char* s)
{
    for (char* p = s; *p; p++)
    {
        *p = static_cast<char>(toupper(static_cast<unsigned char>(*p)));
    }
    return s;
}

char* port_strlwr(char* s)
{
    for (char* p = s; *p; p++)
    {
        *p = static_cast<char>(tolower(static_cast<unsigned char>(*p)));
    }
    return s;
}

static char* utoa_radix(unsigned long long v, char* str, int radix, bool neg)
{
    char tmp[72];
    int n = 0;
    do
    {
        int d = static_cast<int>(v % radix);
        tmp[n++] = static_cast<char>(d < 10 ? '0' + d : 'a' + d - 10);
        v /= radix;
    } while (v);
    char* p = str;
    if (neg)
    {
        *p++ = '-';
    }
    while (n)
    {
        *p++ = tmp[--n];
    }
    *p = 0;
    return str;
}

char* port_itoa(int value, char* str, int radix)
{
    if (radix == 10 && value < 0)
    {
        return utoa_radix(static_cast<unsigned long long>(-static_cast<long long>(value)), str, radix, true);
    }
    return utoa_radix(static_cast<unsigned int>(value), str, radix, false);
}

char* port_ltoa(long value, char* str, int radix)
{
    return port_itoa(static_cast<int>(value), str, radix);
}

char* port_ultoa(unsigned long value, char* str, int radix)
{
    return utoa_radix(static_cast<unsigned int>(value), str, radix, false);
}

} // extern "C"
