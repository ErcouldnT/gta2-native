// Minimal COM vocabulary so the original DirectX SDK headers declare their
// interfaces as plain abstract C++ classes.
#pragma once
#include "windows.h"

#define interface struct
#define PURE = 0
#define THIS_
#define THIS void
#define STDMETHOD(method) virtual HRESULT method
#define STDMETHOD_(type, method) virtual type method
#define STDMETHODIMP HRESULT
#define STDMETHODIMP_(type) type
#define DECLARE_INTERFACE(iface) struct iface
#define DECLARE_INTERFACE_(iface, baseiface) struct iface : public baseiface
#define EXTERN_C extern "C"
#define STDAPI EXTERN_C HRESULT
#define STDAPI_(type) EXTERN_C type
#define FACILITY_WIN32 7
#define FACILITY_ITF 4
#define SEVERITY_SUCCESS 0
#define SEVERITY_ERROR 1
#define CLSCTX_INPROC_SERVER 0x1
#define REGDB_E_CLASSNOTREG ((HRESULT)0x80040154)
#define CLASS_E_NOAGGREGATION ((HRESULT)0x80040110)
#define CLSCTX_ALL 0x17
#define COINIT_MULTITHREADED 0

#ifndef __IUnknown_INTERFACE_DEFINED__
#define __IUnknown_INTERFACE_DEFINED__
struct IUnknown
{
    virtual HRESULT QueryInterface(REFIID riid, void** ppvObject) = 0;
    virtual ULONG AddRef() = 0;
    virtual ULONG Release() = 0;
    virtual ~IUnknown() {}
};
typedef IUnknown* LPUNKNOWN;
#endif

HRESULT CoInitialize(LPVOID);
HRESULT CoInitializeEx(LPVOID, DWORD);
void CoUninitialize(void);
HRESULT CoCreateInstance(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID*);
