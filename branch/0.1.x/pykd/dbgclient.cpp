#include "stdafx.h"
#include <vector>

#include "dbgclient.h"
#include "stkframe.h"

namespace pykd {

///////////////////////////////////////////////////////////////////////////////////

DebugClientPtr  g_dbgClient( DebugClient::createDbgClient() );

///////////////////////////////////////////////////////////////////////////////////

DebugClient::DebugClient( IDebugClient4 *client )
    : DbgObject( client )
    , m_symSymbols( new SyntheticSymbols(*m_symbols, *this) )
    , m_internalDbgEventHandler(client, this, m_symSymbols, m_bpCallbacks)
{
}

///////////////////////////////////////////////////////////////////////////////////

DebugClientPtr DebugClient::createDbgClient() {

    HRESULT                  hres;
    CComPtr<IDebugClient4>   client = NULL;

    hres = DebugCreate( __uuidof(IDebugClient4), (void **)&client );
    if ( FAILED( hres ) )
        throw DbgException("DebugCreate failed");

    return  createDbgClient( client );
}

///////////////////////////////////////////////////////////////////////////////////

DebugClientPtr DebugClient::createDbgClient( IDebugClient4 *client ) {

    //HRESULT                 hres;
    //CComPtr<IDebugClient>   newClient = NULL;

    //hres = client->CreateClient( &newClient );
    //if ( FAILED( hres ) )
    //    throw DbgException("DebugCreate failed");

    //CComQIPtr<IDebugClient4>  client4=  newClient;

    //return DebugClientPtr( new DebugClient(client4) );

    return DebugClientPtr( new DebugClient(client) );
}

///////////////////////////////////////////////////////////////////////////////////

DebugClientPtr  DebugClient::setDbgClientCurrent( DebugClientPtr  newDbgClient ) {
    DebugClientPtr  oldClient = g_dbgClient;
    g_dbgClient = newDbgClient;
    return oldClient;
}

///////////////////////////////////////////////////////////////////////////////////

python::tuple DebugClient::getDebuggeeType()
{
    HRESULT         hres;
    ULONG           debugClass, debugQualifier;
    
    hres = m_control->GetDebuggeeType( &debugClass, &debugQualifier );
    
    if ( FAILED( hres ) )
        throw DbgException( "IDebugControl::GetDebuggeeType  failed" );   

    return python::make_tuple( debugClass, debugQualifier );
}

python::tuple getDebuggeeType()
{
    return g_dbgClient->getDebuggeeType();
}

///////////////////////////////////////////////////////////////////////////////////

ULONG DebugClient::getExecutionStatus()
{
    ULONG       currentStatus;
    HRESULT     hres;

    hres = m_control->GetExecutionStatus( &currentStatus );

    if ( FAILED( hres ) )
        throw  DbgException( "IDebugControl::GetExecutionStatus  failed" ); 

    return currentStatus;
}

ULONG getExecutionStatus()
{
    return  g_dbgClient->getExecutionStatus();
}

///////////////////////////////////////////////////////////////////////////////////

bool DebugClient::is64bitSystem()
{
    HRESULT     hres;
    
    hres = m_control->IsPointer64Bit();
    if ( FAILED( hres ) )
        throw DbgException( "IDebugControl::IsPointer64Bit failed" );
        
    return hres == S_OK;    
}

bool
is64bitSystem()
{
    return g_dbgClient->is64bitSystem();
}


///////////////////////////////////////////////////////////////////////////////////

bool DebugClient::isDumpAnalyzing()
{
    HRESULT         hres;
    ULONG           debugClass, debugQualifier;
    
    hres = m_control->GetDebuggeeType( &debugClass, &debugQualifier );
    
    if ( FAILED( hres ) )
        throw DbgException( "IDebugControl::GetDebuggeeType  failed" );   
         
    return debugQualifier >= DEBUG_DUMP_SMALL;
}

bool isDumpAnalyzing() 
{
    return g_dbgClient->isDumpAnalyzing();
}

///////////////////////////////////////////////////////////////////////////////////

bool DebugClient::isKernelDebugging()
{
    HRESULT     hres;
    ULONG       debugClass, debugQualifier;
    
    hres = m_control->GetDebuggeeType( &debugClass, &debugQualifier );
    
    if ( FAILED( hres ) )
        throw DbgException( "IDebugControl::GetDebuggeeType  failed" );   
         
    return debugClass == DEBUG_CLASS_KERNEL;
}

bool isKernelDebugging() 
{
    return g_dbgClient->isKernelDebugging();
}

//////////////////////////////////////////////////////////////////////////////////

void DebugClient::loadDump( const std::wstring &fileName )
{
    HRESULT     hres;
     
    hres = m_client->OpenDumpFileWide( fileName.c_str(), NULL );
    if ( FAILED( hres ) )
        throw DbgException( "IDebugClient4::OpenDumpFileWide failed" );

    hres = safeWaitForEvent();
    if ( FAILED( hres ) )
        throw DbgException( "IDebugControl::WaitForEvent failed" );
}

void loadDump( const std::wstring &fileName ) {
    g_dbgClient->loadDump( fileName );    
}

///////////////////////////////////////////////////////////////////////////////////

void DebugClient::startProcess( const std::wstring  &processName )
{
    HRESULT     hres;

    ULONG       opt;
    hres = m_control->GetEngineOptions( &opt );
    if ( FAILED( hres ) )
        throw DbgException( "IDebugControl::GetEngineOptions failed" );

    opt |= DEBUG_ENGOPT_INITIAL_BREAK;
    hres = m_control->SetEngineOptions( opt );
    if ( FAILED( hres ) )
        throw DbgException( "IDebugControl::SetEngineOptions failed" );

    std::vector< std::wstring::value_type >      cmdLine( processName.size() + 1 );
    wcscpy_s( &cmdLine[0], cmdLine.size(), processName.c_str() );

    hres = m_client->CreateProcessWide( 0, &cmdLine[0], DEBUG_PROCESS | DETACHED_PROCESS );
    if ( FAILED( hres ) )
        throw DbgException( "IDebugClient4::CreateProcessWide failed" );

    hres = safeWaitForEvent();
    if ( FAILED( hres ) )
        throw DbgException( "IDebugControl::WaitForEvent failed" );
}

void startProcess( const std::wstring  &processName ) {
    g_dbgClient->startProcess( processName );
}

///////////////////////////////////////////////////////////////////////////////////

void DebugClient::attachProcess( ULONG  processId )
{
    HRESULT     hres;
    
    hres = m_client->AttachProcess( 0, processId, 0 );
    if ( FAILED( hres ) )
        throw DbgException( "IDebugClient::AttachProcess failed" );
}

void attachProcess( ULONG  processId ) {
    g_dbgClient->attachProcess( processId );
}

///////////////////////////////////////////////////////////////////////////////////

void DebugClient::detachProcess()
{
    HRESULT     hres;
    
    hres = m_client->DetachCurrentProcess();
    if ( FAILED( hres ) )
        throw DbgException( "IDebugClient::DetachCurrentProcess failed" );
}

void detachProcess() {
    g_dbgClient->detachProcess();
}

///////////////////////////////////////////////////////////////////////////////////

void DebugClient::attachKernel( const std::wstring  &param )
{
    HRESULT     hres;

    hres = m_client5->AttachKernelWide( DEBUG_ATTACH_KERNEL_CONNECTION, param.c_str() );
    if ( FAILED( hres ) )
        throw DbgException( "IDebugClient5::AttachKernelWide failed" );
}

void attachKernel( const std::wstring  &param ) {
    g_dbgClient->attachKernel( param );
}

///////////////////////////////////////////////////////////////////////////////////

std::string DebugClient::findSymbol( ULONG64 offset ) 
{
    HRESULT     hres;

    offset = addr64( offset );

    char        symbolName[0x100];
    ULONG64     displace = 0;
    hres = m_symbols->GetNameByOffset( offset, symbolName, sizeof(symbolName), NULL, &displace );
    if ( FAILED( hres ) )
        throw DbgException( "IDebugSymbol::GetNameByOffset  failed" );

    std::stringstream      ss;
    displace == 0 ?  ss << symbolName : ss << symbolName << '+' << std::hex << displace;

    return ss.str();
}

std::string findSymbol( ULONG64 offset ) 
{
    return g_dbgClient->findSymbol( offset );
}

///////////////////////////////////////////////////////////////////////////////////

ULONG64 DebugClient::getOffset( const std::wstring  symbolname )
{
    HRESULT     hres;
    ULONG64     offset;

    hres = m_symbols->GetOffsetByNameWide( symbolname.c_str(), &offset );
    if ( FAILED( hres ) )
        throw DbgException( "failed to find offset for symbol" );  

    return offset;
}

ULONG64 getOffset( const std::wstring  symbolname )
{
    return g_dbgClient->getOffset( symbolname );
}

///////////////////////////////////////////////////////////////////////////////////

std::string DebugClient::getPdbFile( ULONG64 moduleBase )
{
    HRESULT                 hres;
    IMAGEHLP_MODULEW64      imageHelpInfo = { 0 };

    hres = 
        m_advanced->GetSymbolInformation(
            DEBUG_SYMINFO_IMAGEHLP_MODULEW64,
            moduleBase,
            0,
            &imageHelpInfo,
            sizeof( imageHelpInfo ),
            NULL,
            NULL,
            0,
            NULL );

    if ( FAILED( hres ) )
        throw DbgException( "IDebugAdvanced2::GetSymbolInformation  failed" );

    char  fileName[ 256 ];                
    WideCharToMultiByte( CP_ACP, 0, imageHelpInfo.LoadedPdbName, 256, fileName, 256, NULL, NULL );
        
    return std::string( fileName );      
}

std::string getPdbFile( ULONG64 moduleBase )
{
    return g_dbgClient->getPdbFile( moduleBase );
}

///////////////////////////////////////////////////////////////////////////////////

std::string DebugClient::dbgSymPath()
{
    HRESULT         hres;
    std::string     pathStr;
    
    ULONG    size;
    m_symbols->GetSymbolPath( NULL, 0, &size );
        
    std::vector<char> path(size);
    hres = m_symbols->GetSymbolPath( &path[0], size, NULL );
    if ( FAILED( hres ) )
        throw DbgException( "IDebugSymbols::GetSymbolPath  failed" ); 
        
    return std::string(&path[0],size);
}

std::string dbgSymPath()
{
    return g_dbgClient->dbgSymPath();
}

///////////////////////////////////////////////////////////////////////////////////

void DebugClient::setExecutionStatus( ULONG status )
{
    HRESULT     hres;

    hres = m_control->SetExecutionStatus( status );

    if ( FAILED( hres ) )
        throw DbgException( "IDebugControl::SetExecutionStatus failed" );
}

void setExecutionStatus( ULONG status )
{
    g_dbgClient->setExecutionStatus( status );
}

///////////////////////////////////////////////////////////////////////////////////

HRESULT DebugClient::safeWaitForEvent(ULONG timeout /*= INFINITE*/, ULONG flags /*= DEBUG_WAIT_DEFAULT*/)
{
    PyThread_StateRestore pyThreadRestore( m_pyThreadState );
    return m_control->WaitForEvent( flags, timeout );
}

///////////////////////////////////////////////////////////////////////////////////

void DebugClient::waitForEvent()
{
    HRESULT hres = safeWaitForEvent();

    if ( FAILED( hres ) )
    {
        if (E_UNEXPECTED == hres)
            throw WaitEventException();

        throw  DbgException( "IDebugControl::WaitForEvent  failed" );
    }
}

void waitForEvent()
{
    g_dbgClient->waitForEvent();
}

///////////////////////////////////////////////////////////////////////////////////

ULONG DebugClient::ptrSize()
{
    HRESULT     hres;

    hres = m_control->IsPointer64Bit();

    if ( FAILED( hres ) )
        throw  DbgException( "IDebugControl::IsPointer64Bit  failed" );
    
    return S_OK == hres ? 8 : 4;
}

///////////////////////////////////////////////////////////////////////////////////

ULONG ptrSize()
{ 
    return g_dbgClient->ptrSize();
}

///////////////////////////////////////////////////////////////////////////////////

void DebugClient::terminateProcess()
{
    HRESULT     hres;

    hres = m_client->TerminateCurrentProcess();
    if ( FAILED( hres ) )
        throw DbgException( "IDebugClient::TerminateCurrentProcess", hres );
}

void terminateProcess()
{
    g_dbgClient->terminateProcess();
}

///////////////////////////////////////////////////////////////////////////////////

static const boost::regex moduleSymMatch("^(?:([^!]*)!)?([^!]+)$"); 

void DebugClient::splitSymName( const std::string &fullName, std::string &moduleName, std::string &symbolName )
{
    boost::cmatch    matchResult;

    OutputReader     outputDiscard( m_client );

    if ( !boost::regex_match( fullName.c_str(), matchResult, moduleSymMatch ) )
    {
        std::stringstream   sstr;
        sstr << "invalid symbol name: " << fullName;
        throw SymbolException( sstr.str() );
    }

    symbolName = std::string( matchResult[2].first, matchResult[2].second );

    if ( matchResult[1].matched )
    {
        moduleName = std::string( matchResult[1].first, matchResult[1].second );

        return;
    }

    HRESULT     hres;
    ULONG64     base;

    hres = m_symbols->GetSymbolModule( ( std::string("!") + symbolName ).c_str(), &base );
    if ( FAILED( hres ) )
    {
        std::stringstream   sstr;
        sstr << "failed to find module for symbol: " << symbolName;
        throw SymbolException( sstr.str() );
    }

    char       nameBuffer[0x100];

    hres = 
        m_symbols->GetModuleNameString(
            DEBUG_MODNAME_MODULE,
            DEBUG_ANY_ID,
            base,
            nameBuffer,
            sizeof(nameBuffer),
            NULL );

    if ( FAILED( hres ) )
    {
        std::stringstream   sstr;
        sstr << "failed to find module for symbol: " << symbolName;
        throw SymbolException( sstr.str() );
    }

    moduleName = std::string( nameBuffer );
}

///////////////////////////////////////////////////////////////////////////////////

ULONG64 DebugClient::getSymbolSize( const std::string &fullName )
{
    std::string     moduleName;
    std::string     symName;

	if ( TypeInfo::isBaseType( fullName ) )
		return  TypeInfo::getBaseTypeInfo( fullName )->getSize();

    splitSymName( fullName, moduleName, symName );

    ModulePtr   module = loadModuleByName( moduleName );   

    return module->getSymbolSize(symName);
}

ULONG64 getSymbolSize( const std::string &symName )
{
    return g_dbgClient->getSymbolSize(symName);
}

///////////////////////////////////////////////////////////////////////////////////

TypeInfoPtr DebugClient::getTypeInfoByName( const std::string &typeName )
{
    std::string     moduleName;
    std::string     symName;

	if ( TypeInfo::isBaseType( typeName ) )
		return TypeInfo::getBaseTypeInfo( typeName );

    splitSymName( typeName, moduleName, symName );

    ModulePtr   module = loadModuleByName( moduleName );   

    return module->getTypeByName( symName );
}

///////////////////////////////////////////////////////////////////////////////////

TypedVarPtr DebugClient::getTypedVarByName( const std::string &varName )
{
    std::string     moduleName;
    std::string     symName;

    splitSymName( varName, moduleName, symName );

    ModulePtr   module = loadModuleByName( moduleName );

    return module->getTypedVarByName( symName );
}

///////////////////////////////////////////////////////////////////////////////////

TypedVarPtr DebugClient::getTypedVarByTypeName( const std::string &typeName, ULONG64 addr )
{
    addr = addr64( addr );

    std::string     moduleName;
    std::string     symName;

    splitSymName( typeName, moduleName, symName );

    ModulePtr   module = loadModuleByName( moduleName );

    return module->getTypedVarByTypeName( symName, addr );
}

///////////////////////////////////////////////////////////////////////////////////

TypedVarPtr DebugClient::containingRecordByName( ULONG64 addr, const std::string &typeName, const std::string &fieldName )
{
    addr = addr64( addr );

    std::string     moduleName;
    std::string     symName;

    splitSymName( typeName, moduleName, symName );

    ModulePtr   module = loadModuleByName( moduleName );

    return module->containingRecordByName( addr, symName, fieldName );
}

TypedVarPtr containingRecordByName( ULONG64 addr, const std::string &typeName, const std::string &fieldName )
{
    return g_dbgClient->containingRecordByName( addr, typeName, fieldName );
}

///////////////////////////////////////////////////////////////////////////////////

TypedVarPtr DebugClient::containingRecordByType( ULONG64 addr, const TypeInfoPtr &typeInfo, const std::string &fieldName )
{
    addr = addr64(addr); 

    VarDataPtr varData = VarDataMemory::factory( m_dataSpaces, addr - typeInfo->getFieldOffsetByNameRecirsive(fieldName) );

    return TypedVar::getTypedVar( m_client, typeInfo, varData );
}

TypedVarPtr containingRecordByType( ULONG64 addr, const TypeInfoPtr &typeInfo, const std::string &fieldName )
{
    return g_dbgClient->containingRecordByType( addr, typeInfo, fieldName );
}

///////////////////////////////////////////////////////////////////////////////////

python::list DebugClient::getTypedVarListByType( ULONG64 listHeadAddress, const TypeInfoPtr &typeInfo, const std::string &listEntryName )\
{
    python::list    lst;

    listHeadAddress = addr64( listHeadAddress );

    ULONG64                 entryAddress = 0;

    TypeInfoPtr             fieldTypeInfo = typeInfo->getField( listEntryName );

    if ( fieldTypeInfo->getName() == ( typeInfo->getName() + "*" ) )
    {
        for( entryAddress = ptrPtr( listHeadAddress ); addr64(entryAddress) != listHeadAddress && entryAddress != NULL; entryAddress = ptrPtr( entryAddress + typeInfo->getFieldOffsetByNameRecirsive(listEntryName) ) )
            lst.append( getTypedVarByTypeInfo( typeInfo, entryAddress ) );
    }
    else
    {
        for( entryAddress = ptrPtr( listHeadAddress ); addr64(entryAddress) != listHeadAddress && entryAddress != NULL; entryAddress = ptrPtr( entryAddress ) )
            lst.append( containingRecordByType( entryAddress, typeInfo, listEntryName ) );
    }

    return lst;
}

python::list getTypedVarListByType( ULONG64 listHeadAddress, const TypeInfoPtr &typeInfo, const std::string &listEntryName )
{
    return g_dbgClient->getTypedVarListByType( listHeadAddress, typeInfo, listEntryName );
}

///////////////////////////////////////////////////////////////////////////////////

python::list DebugClient::getTypedVarListByTypeName( ULONG64 listHeadAddress, const std::string &typeName, const std::string &listEntryName )
{
    std::string     moduleName;
    std::string     symName;

    splitSymName( typeName, moduleName, symName );

    ModulePtr   module = loadModuleByName( moduleName );

    return module->getTypedVarListByTypeName( listHeadAddress, symName, listEntryName );
}

python::list getTypedVarListByTypeName( ULONG64 listHeadAddress, const std::string &typeName, const std::string &listEntryName )
{
    return g_dbgClient->getTypedVarListByTypeName( listHeadAddress, typeName, listEntryName );
}

///////////////////////////////////////////////////////////////////////////////////

python::list DebugClient::getTypedVarArrayByType( ULONG64 address, const TypeInfoPtr &typeInfo, ULONG number )
{
    address = addr64(address); 
       
    python::list     lst;
    
    for( ULONG i = 0; i < number; ++i )
        lst.append( getTypedVarByTypeInfo( typeInfo, address + i * typeInfo->getSize() ) );
   
    return lst;
}

python::list getTypedVarArrayByType( ULONG64 address, const TypeInfoPtr &typeInfo, ULONG number )
{
    return g_dbgClient->getTypedVarArrayByType( address, typeInfo, number );
}

///////////////////////////////////////////////////////////////////////////////////

python::list DebugClient::getTypedVarArrayByTypeName( ULONG64 addr, const std::string  &typeName, ULONG number )
{
    std::string     moduleName;
    std::string     symName;

    splitSymName( typeName, moduleName, symName );

    ModulePtr   module = loadModuleByName( moduleName );

    return module->getTypedVarArrayByTypeName( addr, symName, number );
}

python::list getTypedVarArrayByTypeName( ULONG64 addr, const std::string  &typeName, ULONG number )
{
    return g_dbgClient->getTypedVarArrayByTypeName( addr, typeName, number );
}

///////////////////////////////////////////////////////////////////////////////////

python::tuple DebugClient::getSourceLine( ULONG64 offset )
{
    HRESULT     hres;

    if ( offset == 0 )
    {
        hres = m_registers->GetInstructionOffset( &offset );
        if ( FAILED( hres ) )
            throw DbgException( "IDebugRegisters::GetInstructionOffset failed" );
    }

    ULONG       fileNameLength = 0;

    m_symbols->GetLineByOffset(
        offset,
        NULL,
        NULL,
        0,
        &fileNameLength,
        NULL );

    if ( fileNameLength == 0 )
        throw DbgException( "Failed to find source file" );

    std::vector<CHAR>   fileNameBuf( fileNameLength );

    ULONG       lineNo;
    ULONG64     displacement;

    hres = 
        m_symbols->GetLineByOffset(
            offset,
            &lineNo,
            &fileNameBuf[0],
            fileNameLength,
            NULL,
            &displacement );
    
    if ( FAILED( hres ) )
        throw DbgException( "IDebugSymbol::GetLineByOffset method failed" );

    return python::make_tuple( std::string( &fileNameBuf[0] ), lineNo, displacement );
}

///////////////////////////////////////////////////////////////////////////////////

}; // end of namespace pykd
