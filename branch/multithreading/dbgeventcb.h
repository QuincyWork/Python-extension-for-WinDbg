
#pragma once

// monitoring and processing debug events
class DbgEventCallbacks : public DebugBaseEventCallbacks
{
public:

    static HRESULT Start();
    static void Stop();

private:

    // may generate HRESULT exception if not registered
    DbgEventCallbacks();
    void Deregister();

    /////////////////////////////////////////////////////////////////////////////////
    // IUnknown interface implementation

    STDMETHOD_(ULONG, AddRef)();
    STDMETHOD_(ULONG, Release)();

    /////////////////////////////////////////////////////////////////////////////////
    // IDebugEventCallbacks interface implementation

    STDMETHOD(GetInterestMask)(
        __out PULONG Mask
    );

    STDMETHOD(ChangeSymbolState)(
        __in ULONG Flags,
        __in ULONG64 Argument
    );

    /////////////////////////////////////////////////////////////////////////////////

    HRESULT doSymbolsLoaded(
        ULONG64 moduleBase
    );

    /////////////////////////////////////////////////////////////////////////////////

    volatile LONG m_ReferenceCount;

    IDebugClient *m_dbgClient;
    IDebugSymbols3 *m_dbgSymbols3;

    /////////////////////////////////////////////////////////////////////////////////
    // global singleton

    static DbgEventCallbacks *dbgEventCallbacks;

    /////////////////////////////////////////////////////////////////////////////////
};
