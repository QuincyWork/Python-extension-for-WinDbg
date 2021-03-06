#pragma once

#include <string>
#include <dbghelp.h>

#include <boost\smart_ptr\scoped_ptr.hpp>
#include <boost\enable_shared_from_this.hpp>

/////////////////////////////////////////////////////////////////////////////////

#include "dbgobj.h"
#include "dbgexcept.h"
#include "module.h"
#include "dbgio.h"
#include "dbgcmd.h"
#include "pyaux.h"
#include "disasm.h"
#include "cpureg.h"
#include "inteventhandler.h"
#include "synsymbol.h"
#include "context.h"

/////////////////////////////////////////////////////////////////////////////////

namespace pykd {

/////////////////////////////////////////////////////////////////////////////////

class DebugClient;
typedef boost::shared_ptr<DebugClient>  DebugClientPtr;

/////////////////////////////////////////////////////////////////////////////////

typedef ULONG BPOINT_ID;
typedef python::object BpCallback;
typedef std::map<BPOINT_ID, BpCallback> BpCallbackMapIml;
struct BpCallbackMap {
    boost::shared_ptr<boost::recursive_mutex> m_lock;
    BpCallbackMapIml m_map;

    BpCallbackMap() : m_lock(new boost::recursive_mutex) {}
};

/////////////////////////////////////////////////////////////////////////////////

class DebugClient   : private DbgObject
                    , public boost::enable_shared_from_this<DebugClient> {

private:
    // simple IDebugControl4 call wrapper
    template <typename T>
    T getDbgControlT(
        HRESULT (STDMETHODCALLTYPE IDebugControl4::*method)(T *),
        const char *methodName
    )
    {
        T retValue;
        HRESULT hres = (m_control->*method)(&retValue);
        if (S_OK != hres)
            throw DbgException( methodName, hres);
        return retValue;
    }
    #define getDbgControl(method)  \
        getDbgControlT( &IDebugControl4::##method, "IDebugControl4::" #method )

public:

    virtual ~DebugClient() {}

    static
    DebugClientPtr createDbgClient() ;

    static
    DebugClientPtr createDbgClient( IDebugClient4 *client );

    static
    DebugClientPtr  setDbgClientCurrent( DebugClientPtr  newDbgClient );

public:

    ULONG64  addr64( ULONG64 addr );

    void breakin();

    bool compareMemory( ULONG64 addr1, ULONG64 addr2, ULONG length, bool phyAddr = FALSE );

    void detachProcess();

    DbgOut  dout() {
        return DbgOut( m_client );
    }

    DbgIn din() {
        return DbgIn( m_client );
    }

    std::string dbgCommand( const std::wstring  &command );

    void startProcess( const std::wstring  &processName );

    void attachProcess( ULONG  processId );

    void attachKernel( const std::wstring  &param );

    Disasm disasm( ULONG offset = 0 ) {
        return Disasm( m_client, offset );
    }

    void dprint( const std::wstring &str, bool dml = false );

    void dprintln( const std::wstring &str, bool dml = false );

    void eprint( const std::wstring &str );

    void eprintln( const std::wstring &str );

    ULONG64 evaluate( const std::wstring  &expression );

    std::string findSymbol( ULONG64 offset );

    ULONG64 getCurrentProcess();

    python::list getCurrentStack();

    python::tuple getDebuggeeType();

    ULONG64 getImplicitThread();

    ULONG getExecutionStatus();

    ULONG64 getOffset( const std::wstring  symbolname );

    std::string getPdbFile( ULONG64 moduleBase );

    std::string getProcessorMode();

    std::string getProcessorType();

    std::string dbgSymPath();

    python::tuple getSourceLine( ULONG64 offset = 0);

    python::list getThreadList();

    template<ULONG status>
    void changeDebuggerStatus();

    bool is64bitSystem();

    bool isKernelDebugging();

    bool isDumpAnalyzing();

    bool isVaValid( ULONG64 addr );

    void loadDump( const std::wstring &fileName );

    ModulePtr loadModuleByName( const std::string  &moduleName ) {
        return  ModulePtr( new Module( m_client, m_symSymbols, moduleName ) );
    }

    ModulePtr loadModuleByOffset( ULONG64  offset ) {
        return  ModulePtr( new Module( m_client, m_symSymbols, offset ) );
    }

    DbgExtensionPtr loadExtension( const std::wstring &extPath ) {
        return DbgExtensionPtr( new DbgExtension( m_client, extPath ) );
    }

    python::list loadBytes( ULONG64 offset, ULONG count, bool phyAddr = FALSE );

    python::list loadWords( ULONG64 offset, ULONG count, bool phyAddr = FALSE );

    python::list loadDWords( ULONG64 offset, ULONG count, bool phyAddr = FALSE );

    python::list loadQWords( ULONG64 offset, ULONG count, bool phyAddr = FALSE );

    python::list loadSignBytes( ULONG64 offset, ULONG count, bool phyAddr = FALSE );

    python::list loadSignWords( ULONG64 offset, ULONG count, bool phyAddr = FALSE );

    python::list loadSignDWords( ULONG64 offset, ULONG count, bool phyAddr = FALSE );

    python::list loadSignQWords( ULONG64 offset, ULONG count, bool phyAddr = FALSE );

    std::string loadChars( ULONG64 offset, ULONG count, bool phyAddr = FALSE );

    std::wstring loadWChars( ULONG64 offset, ULONG count, bool phyAddr = FALSE );

    std::string loadCStr( ULONG64 offset );

    std::wstring loadWStr( ULONG64 offset );

    std::string loadAnsiStr( ULONG64 address );

    std::wstring loadUnicodeStr( ULONG64 address );

    python::list loadPtrList( ULONG64 address );

    python::list loadPtrArray( ULONG64 address, ULONG  number );

    ULONG64 loadMSR( ULONG  msr );

    ULONG ptrSize();

    ULONG64 ptrByte( ULONG64 offset );

    ULONG64 ptrWord( ULONG64 offset );

    ULONG64 ptrDWord( ULONG64 offset );

    ULONG64 ptrQWord( ULONG64 offset );

    ULONG64 ptrMWord( ULONG64 offset );

    LONG64 ptrSignByte( ULONG64 offset );

    LONG64 ptrSignWord( ULONG64 offset );

    LONG64 ptrSignDWord( ULONG64 offset );

    LONG64 ptrSignQWord( ULONG64 offset );

    LONG64 ptrSignMWord( ULONG64 offset );

    ULONG64 ptrPtr( ULONG64 offset );

    void setMSR( ULONG msr, ULONG64 value);
    
    CpuReg getRegByName( const std::string &regName );

    CpuReg getRegByIndex( ULONG index );

    void setCurrentProcess( ULONG64 addr );

    void setExecutionStatus( ULONG status );

    void setImplicitThread( ULONG64 threadAddr );

    void setProcessorMode( const std::wstring &mode );

    void terminateProcess();
    
    void waitForEvent();

    ULONG getNumberProcessors() { 
        return getDbgControl(GetNumberProcessors);
    }

    ULONG getPageSize() {
        return getDbgControl(GetPageSize);
    }

    ContextPtr getThreadContext() {
        return ContextPtr( new ThreadContext(m_client) );
    }
    ContextPtr getThreadWow64Context() {
        return ThreadContext::getWow64Context(m_client);
    }

    python::dict getLocals(
        ContextPtr ctx = ContextPtr( reinterpret_cast<ThreadContext*>(0) ) 
    );
public:

    CComPtr<IDebugClient4>&
    client() {
        return m_client;    
    }

    CComPtr<IDebugClient5>&
    client5() {
        return m_client5;    
    }

    CComPtr<IDebugControl4>&
    control() {
        return m_control;    
    }

    CComPtr< IDebugDataSpaces4>&
    dataSpace() {
        return m_dataSpaces;
    }

    PyThreadStateSaver&
    getThreadState() {
        return m_pyThreadState;
    }

    void addSyntheticSymbol(
        ULONG64 addr,
        ULONG size,
        const std::string &symName
    )
    {
        return m_symSymbols->add(addr, size, symName);
    }

    void delAllSyntheticSymbols()
    {
        return m_symSymbols->clear();
    }

    ULONG delSyntheticSymbol(
        ULONG64 addr
    )
    {
        return m_symSymbols->remove(addr);
    }

    ULONG delSyntheticSymbolsMask(
        const std::string &moduleName,
        const std::string &symName
    )
    {
        return m_symSymbols->removeByMask(moduleName, symName);
    }

    SynSymbolsPtr getSynSymbols() {
        return m_symSymbols;
    }

    // breakpoints management
    BPOINT_ID setSoftwareBp(ULONG64 addr, BpCallback &callback = BpCallback());
    BPOINT_ID setHardwareBp(ULONG64 addr, ULONG size, ULONG accessType, BpCallback &callback = BpCallback());

    python::list getAllBp();

    void removeBp(BPOINT_ID Id);
    void removeAllBp();

    void splitSymName( const std::string &fullName, std::string &moduleName, std::string &symbolName );

    TypeInfoPtr getTypeInfoByName( const std::string &typeName );

    TypedVarPtr getTypedVarByName( const std::string &varName );

    TypedVarPtr getTypedVarByTypeName( const std::string &typeName, ULONG64 addr );

    TypedVarPtr getTypedVarByTypeInfo( const TypeInfoPtr &typeInfo, ULONG64 addr ) {
        return TypedVar::getTypedVar( m_client, typeInfo, VarDataMemory::factory(m_dataSpaces, addr) );
    }

    TypedVarPtr containingRecordByName( ULONG64 addr, const std::string &typeName, const std::string &fieldName );

    TypedVarPtr containingRecordByType( ULONG64 addr, const TypeInfoPtr &typeInfo, const std::string &fieldName );


    python::list getTypedVarListByTypeName( ULONG64 listHeadAddres, const std::string  &typeName, const std::string &listEntryName );

    python::list getTypedVarListByType( ULONG64 listHeadAddres, const TypeInfoPtr &typeInfo, const std::string &listEntryName );

    python::list getTypedVarArrayByTypeName( ULONG64 addr, const std::string  &typeName, ULONG number );

    python::list getTypedVarArrayByType( ULONG64 addr, const TypeInfoPtr &typeInfo, ULONG number );

    ULONG64 getSymbolSize( const std::string &symName );


private:
    HRESULT safeWaitForEvent(ULONG timeout = INFINITE, ULONG flags = DEBUG_WAIT_DEFAULT);

    template<typename T>
    python::list
    loadArray( ULONG64 offset, ULONG count, bool phyAddr );

    BpCallbackMap m_bpCallbacks;

    SynSymbolsPtr m_symSymbols; // DebugClient is creator
    InternalDbgEventHandler m_internalDbgEventHandler;

    DebugClient( IDebugClient4 *client );

    PyThreadStateSaver      m_pyThreadState;
};

/////////////////////////////////////////////////////////////////////////////////

extern DebugClientPtr     g_dbgClient;

void loadDump( const std::wstring &fileName );

void startProcess( const std::wstring  &processName );

void attachProcess( ULONG  processId );

void attachKernel( const std::wstring  &param );

void detachProcess();

std::string findSymbol( ULONG64 offset );

python::tuple getDebuggeeType();

ULONG getExecutionStatus();

ULONG64 getOffset( const std::wstring  symbolname );

std::string getPdbFile( ULONG64 moduleBase );

std::string dbgSymPath();

bool is64bitSystem();

bool isKernelDebugging();

bool isDumpAnalyzing();

ULONG ptrSize();

void setExecutionStatus( ULONG status );

void terminateProcess();

void waitForEvent();

TypedVarPtr containingRecordByName( ULONG64 addr, const std::string &typeName, const std::string &fieldName );

TypedVarPtr containingRecordByType( ULONG64 addr, const TypeInfoPtr &typeInfo, const std::string &fieldName );

python::list getTypedVarListByTypeName( ULONG64 listHeadAddres, const std::string  &typeName, const std::string &listEntryName );

python::list getTypedVarListByType( ULONG64 listHeadAddres, const TypeInfoPtr &typeInfo, const std::string &listEntryName );

python::list getTypedVarArrayByTypeName( ULONG64 addr, const std::string  &typeName, ULONG number );

python::list getTypedVarArrayByType( ULONG64 addr, const TypeInfoPtr &typeInfo, ULONG number );

ULONG64 getSymbolSize( const std::string &symName );

/////////////////////////////////////////////////////////////////////////////////
// Synthetic symbols global finctions:

inline void addSyntheticSymbol(
    ULONG64 addr,
    ULONG size,
    const std::string &symName
)
{
    return g_dbgClient->addSyntheticSymbol(addr, size, symName);
}

inline void delAllSyntheticSymbols()
{
    return g_dbgClient->delAllSyntheticSymbols();
}

inline ULONG delSyntheticSymbol(
    ULONG64 addr
)
{
    return g_dbgClient->delSyntheticSymbol(addr);
}

inline ULONG delSyntheticSymbolsMask(
    const std::string &moduleName,
    const std::string &symName
)
{
    return g_dbgClient->delSyntheticSymbolsMask(moduleName, symName);
}

/////////////////////////////////////////////////////////////////////////////////

inline ULONG getNumberProcessors() { 
    return g_dbgClient->getNumberProcessors();
}

inline ULONG getPageSize() {
    return g_dbgClient->getPageSize();
}

inline ContextPtr getThreadContext() {
    return g_dbgClient->getThreadContext();
}

inline ContextPtr getThreadWow64Context() {
    return g_dbgClient->getThreadWow64Context();
}

inline python::dict getLocals(
    ContextPtr ctx = ContextPtr()
)
{
    return g_dbgClient->getLocals(ctx);
}

/////////////////////////////////////////////////////////////////////////////////

template<ULONG status>
void DebugClient::changeDebuggerStatus()
{
    HRESULT     hres;

    hres = m_control->SetExecutionStatus( status );

    if ( FAILED( hres ) )
        throw DbgException( "IDebugControl::SetExecutionStatus failed" );

    ULONG    currentStatus;

    do {

        waitForEvent();

        hres = m_control->GetExecutionStatus( &currentStatus );

        if ( FAILED( hres ) )
            throw  DbgException( "IDebugControl::GetExecutionStatus  failed" ); 

    } while( currentStatus != DEBUG_STATUS_BREAK && currentStatus != DEBUG_STATUS_NO_DEBUGGEE );
}

template<ULONG status>
void changeDebuggerStatus()
{
    g_dbgClient->changeDebuggerStatus<status>();
}

inline python::tuple getSourceLine( ULONG64 offset = 0 )
{
    return g_dbgClient->getSourceLine(offset);
}
/////////////////////////////////////////////////////////////////////////////////


};  // namespace pykd


