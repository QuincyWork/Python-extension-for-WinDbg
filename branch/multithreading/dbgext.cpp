#include "stdafx.h"

#include <wdbgexts.h>
#include <process.h>

#include <vector>
#include <string>
#include <map>

#include <boost/python/module.hpp>
#include <boost/python/def.hpp>
#include <boost/tokenizer.hpp>
#include <boost/python/overloads.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>

#include "dbgext.h"
#include "dbgprint.h"
#include "dbgreg.h"
#include "dbgtype.h"
#include "dbgmodule.h"
#include "dbgsym.h"
#include "dbgmem.h"
#include "dbgsystem.h"
#include "dbgcmd.h"
#include "dbgdump.h"
#include "dbgexcept.h"
#include "dbgeventcb.h"
#include "dbgsession.h"
#include "dbgcallback.h"
#include "dbgpath.h"
#include "dbginput.h"
#include "dbgprocess.h"
#include "dbgsynsym.h"

/////////////////////////////////////////////////////////////////////////////////

// указатель на текущий интерфейс
__declspec(thread) DbgExt    *dbgExt = NULL;

struct Interpreter
{
    std::vector<std::string> args;
    HANDLE thread;
    PyThreadState * globalInterpreter;
    PyThreadState * localInterpreter;

    Interpreter(HANDLE thread, const std::vector<std::string> & args)
        : thread(thread)
        , args(args)
        , globalInterpreter(0)
        , localInterpreter(0)
    {}
};

// TODO: use multi_index
typedef std::map<UINT, Interpreter> PyInterpretersMap;

CRITICAL_SECTION py_interpreters_cs;
PyInterpretersMap py_interpreters;

/////////////////////////////////////////////////////////////////////////////////

class WindbgGlobalSession 
{
public:

    WindbgGlobalSession() {
    
        m_globalState = PyThreadState_Get();
    
        boost::python::import( "pykd" );
        
        main = boost::python::import("__main__");
        
        // перенаправление стандартных потоков ВВ
        boost::python::object       sys = boost::python::import( "sys");
        
        dbgOut                      dout;
        sys.attr("stdout") = boost::python::object( dout );

        dbgIn                       din;
        sys.attr("stdin") = boost::python::object( din );
    }
    
    boost::python::object
    global() {
        return main.attr("__dict__");
    }
    
    PyThreadState*
    globalState() {
        return m_globalState;
    }

private:
   
    boost::python::object       main;
   
    PyThreadState*              m_globalState;
};   

static WindbgGlobalSession     *windbgGlobalSession = NULL; 

/////////////////////////////////////////////////////////////////////////////////
BOOST_PYTHON_FUNCTION_OVERLOADS( dprint, DbgPrint::dprint, 1, 2 )
BOOST_PYTHON_FUNCTION_OVERLOADS( dprintln, DbgPrint::dprintln, 1, 2 )

BOOST_PYTHON_FUNCTION_OVERLOADS( loadBytes, loadArray<unsigned char>, 2, 3 )
BOOST_PYTHON_FUNCTION_OVERLOADS( loadWords, loadArray<unsigned short>, 2, 3 )
BOOST_PYTHON_FUNCTION_OVERLOADS( loadDWords, loadArray<unsigned long>, 2, 3 )
BOOST_PYTHON_FUNCTION_OVERLOADS( loadQWords, loadArray<unsigned __int64> , 2, 3 )
BOOST_PYTHON_FUNCTION_OVERLOADS( loadSignBytes, loadArray<char> , 2, 3 )
BOOST_PYTHON_FUNCTION_OVERLOADS( loadSignWords, loadArray<short> , 2, 3 )
BOOST_PYTHON_FUNCTION_OVERLOADS( loadSignDWords, loadArray<long> , 2, 3 )
BOOST_PYTHON_FUNCTION_OVERLOADS( loadSignQWords, loadArray<__int64>, 2, 3 )

BOOST_PYTHON_FUNCTION_OVERLOADS( compareMemoryOver, compareMemory, 3, 4 )


BOOST_PYTHON_MODULE( pykd )
{
    boost::python::def( "go", &setExecutionStatus<DEBUG_STATUS_GO> );
    boost::python::def( "trace", &setExecutionStatus<DEBUG_STATUS_STEP_INTO> );
    boost::python::def( "step", &setExecutionStatus<DEBUG_STATUS_STEP_OVER> );   
    boost::python::def( "expr", &evaluate ); 
    boost::python::def( "createSession", &dbgCreateSession );   // deprecated
    boost::python::def( "isSessionStart", &dbgIsSessionStart );
    boost::python::def( "symbolsPath", &dbgSymPath );
    boost::python::def( "dprint", &DbgPrint::dprint, dprint( boost::python::args( "str", "dml" ), ""  ) );
    boost::python::def( "dprintln", &DbgPrint::dprintln, dprintln( boost::python::args( "str", "dml" ), ""  ) );
    boost::python::def( "loadDump", &dbgLoadDump );
    boost::python::def( "startProcess", &startProcess );
    boost::python::def( "dbgCommand", &dbgCommand );
    boost::python::def( "isValid", &isOffsetValid );
    boost::python::def( "is64bitSystem", &is64bitSystem );
    boost::python::def( "isKernelDebugging", &isKernelDebugging );
    boost::python::def( "ptrSize", ptrSize );
    boost::python::def( "reg", &loadRegister );
    boost::python::def( "typedVar", &loadTypedVar );
    boost::python::def( "typedVarList", &loadTypedVarList );
    boost::python::def( "typedVarArray", &loadTypedVarArray );
    boost::python::def( "containingRecord", &containingRecord );
    boost::python::def( "getTypeClass", &getTypeClass );
    boost::python::def( "sizeof", &sizeofType );
    boost::python::def( "loadModule", &loadModule );
    boost::python::def( "findSymbol", &findSymbolForAddress );
    boost::python::def( "getOffset", &findAddressForSymbol );
    boost::python::def( "findModule", &findModule );
    boost::python::def( "addr64", &addr64 );
    boost::python::def( "loadBytes", &loadArray<unsigned char>, loadBytes( boost::python::args( "address", "number",  "phyAddr"  ), "" ) );
    boost::python::def( "loadWords", &loadArray<unsigned short>, loadWords( boost::python::args( "address", "number",  "phyAddr"  ), "" ) );
    boost::python::def( "loadDWords", &loadArray<unsigned long>, loadDWords( boost::python::args( "address", "number",  "phyAddr"  ), "" ) );
    boost::python::def( "loadQWords", &loadArray<unsigned __int64>, loadQWords( boost::python::args( "address", "number",  "phyAddr"  ), "" ) );
    boost::python::def( "loadSignBytes", &loadArray<char>, loadSignBytes( boost::python::args( "address", "number",  "phyAddr"  ), "" ) );
    boost::python::def( "loadSignWords", &loadArray<short>, loadSignWords( boost::python::args( "address", "number",  "phyAddr"  ), "" ) );
    boost::python::def( "loadSignDWords", &loadArray<long>, loadSignDWords( boost::python::args( "address", "number",  "phyAddr"  ), "" ) );
    boost::python::def( "loadSignQWords", &loadArray<__int64>, loadSignQWords( boost::python::args( "address", "number",  "phyAddr"  ), "" ) );
    boost::python::def( "loadPtrs", &loadPtrArray );
    boost::python::def( "loadUnicodeString", &loadUnicodeStr );
    boost::python::def( "loadAnsiString", &loadAnsiStr );   
    boost::python::def( "loadCStr", &loadCStr );
    boost::python::def( "loadWStr", &loadWStr );
    boost::python::def( "loadLinkedList", &loadLinkedList ); 
    boost::python::def( "ptrByte", &loadByPtr<unsigned char> );
    boost::python::def( "ptrSignByte", &loadByPtr<char> );
    boost::python::def( "ptrWord", &loadByPtr<unsigned short> );
    boost::python::def( "ptrSignWord", &loadByPtr<short> );
    boost::python::def( "ptrDWord", &loadByPtr<unsigned long> );
    boost::python::def( "ptrSignDWord", &loadByPtr<long> );
    boost::python::def( "ptrQWord", &loadByPtr<unsigned __int64> );
    boost::python::def( "ptrSignQWord", &loadByPtr<__int64> );
    boost::python::def( "ptrPtr", &loadPtrByPtr );   
    boost::python::def( "ptrMWord", &loadMWord );
    boost::python::def( "ptrSignMWord", &loadSignMWord );
    boost::python::def( "compareMemory", &compareMemory, compareMemoryOver( boost::python::args( "addr1", "addr2", "length", "phyAddr" ), "" ) );
    boost::python::def( "getCurrentStack", &getCurrentStack );
    boost::python::def( "locals", &getLocals );
    boost::python::def( "reloadModule", &reloadModule );
    boost::python::def( "getPdbFile", &getPdbFile );
    boost::python::def( "getImplicitThread", &getImplicitThread );
    boost::python::def( "setImplicitThread", &setImplicitThread );
    boost::python::def( "getThreadList", &getThreadList );
    boost::python::def( "getCurrentProcess", &getCurrentProcess );
    boost::python::def( "setCurrentProcess", &setCurrentProcess );
    boost::python::def( "getProcessorMode", &getProcessorMode );
    boost::python::def( "setProcessorMode", &setProcessorMode );
    boost::python::def( "addSynSymbol", &addSyntheticSymbol );
    boost::python::def( "delAllSynSymbols", &delAllSyntheticSymbols);
    boost::python::def( "delSynSymbol", &delSyntheticSymbol );
    boost::python::def( "delSynSymbolsMask", &delSyntheticSymbolsMask);
    boost::python::class_<typeClass, boost::shared_ptr<typeClass> >( "typeClass" )
        .def("sizeof", &typeClass::size )
        .def("offset", &typeClass::getOffset )
        .def("__str__", &typeClass::print);
    boost::python::class_<typedVarClass, boost::python::bases<typeClass>, boost::shared_ptr<typedVarClass> >( "typedVarClass" )
        .def("getAddress", &typedVarClass::getAddress );
    boost::python::class_<dbgModuleClass>( "dbgModuleClass" )
        .def("begin", &dbgModuleClass::getBegin )
        .def("end", &dbgModuleClass::getEnd )
        .def("name", &dbgModuleClass::getName )
        .def("contain", &dbgModuleClass::contain )
        .def("image", &dbgModuleClass::getImageSymbolName )
        .def("pdb", &dbgModuleClass::getPdbName )
        .def("addSynSymbol", &dbgModuleClass::addSyntheticSymbol )
        .def("delAllSynSymbols", &dbgModuleClass::delAllSyntheticSymbols )
        .def("delSynSymbol", &dbgModuleClass::delSyntheticSymbol )
        .def("delSynSymbolsMask", &dbgModuleClass::delSyntheticSymbolsMask )
        .def("__getattr__", &dbgModuleClass::getOffset )
		.def("__str__", &dbgModuleClass::print );
    boost::python::class_<dbgExtensionClass>( 
            "ext",
            "windbg extension",
             boost::python::init<const char*>( boost::python::args("path"), "__init__  dbgExtensionClass" ) ) 
        .def("call", &dbgExtensionClass::call )
		.def("__str__", &dbgExtensionClass::print );
    boost::python::class_<dbgStackFrameClass>( "dbgStackFrameClass", "dbgStackFrameClass" )
        .def_readonly( "instructionOffset", &dbgStackFrameClass::InstructionOffset )
        .def_readonly( "returnOffset", &dbgStackFrameClass::ReturnOffset )
        .def_readonly( "frameOffset", &dbgStackFrameClass::FrameOffset )
        .def_readonly( "stackOffset", &dbgStackFrameClass::StackOffset )
        .def_readonly( "frameNumber", &dbgStackFrameClass::FrameNumber )
		.def( "__str__", &dbgStackFrameClass::print );
    boost::python::class_<dbgOut>( "windbgOut", "windbgOut" )
        .def( "write", &dbgOut::write );
    boost::python::class_<dbgFileOut>( 
            "windbgFileOut", 
            "windbgFileOut", 
            boost::python::init<const char *>( boost::python::args("fname"), "__init__  dbgFileOut" ) )
        .def( "write", &dbgFileOut::write );
    boost::python::class_<dbgIn>( "windbgIn", "windbgIn" )
        .def( "readline", &dbgIn::readline );                
    boost::python::class_<dbgBreakpointClass>( 
         "bp",
         "break point",
         boost::python::init<ULONG64>( boost::python::args("offset"), "__init__  dbgBreakpointClass" ) ) 
        .def( "set", &dbgBreakpointClass::set )
        .def( "remove", &dbgBreakpointClass::remove )
		.def( "__str__", &dbgBreakpointClass::print );
        
}    

/////////////////////////////////////////////////////////////////////////////////

HRESULT
CALLBACK
DebugExtensionInitialize(
    OUT PULONG  Version,
    OUT PULONG  Flags )
{
    *Version = DEBUG_EXTENSION_VERSION( 1, 0 );
    *Flags = 0;

    PyImport_AppendInittab("pykd", initpykd ); 

    Py_Initialize();

    PyEval_InitThreads();
    
    PyEval_ReleaseLock();

    windbgGlobalSession = new WindbgGlobalSession();

    ::InitializeCriticalSectionAndSpinCount(&py_interpreters_cs, 4000);

    return setDbgSessionStarted();
}


VOID
CALLBACK
DebugExtensionUninitialize()
{
    // TODO: send a signal to unload the threads.
    ::EnterCriticalSection(&py_interpreters_cs);

    BOOST_FOREACH (const PyInterpretersMap::value_type i, py_interpreters)
    {
        ::TerminateThread(i.second.thread, 0);
        ::CloseHandle(i.second.thread);
    }
    py_interpreters.clear();

    ::LeaveCriticalSection(&py_interpreters_cs);

    ::DeleteCriticalSection(&py_interpreters_cs);

    DbgEventCallbacks::Stop();

    delete windbgGlobalSession;
    windbgGlobalSession = NULL;
    
    PyEval_AcquireLock();    

    Py_Finalize();
}


void
SetupDebugEngine( IDebugClient4 *client, DbgExt *dbgExt  )
{
    client->QueryInterface( __uuidof(IDebugClient), (void **)&dbgExt->client );
    client->QueryInterface( __uuidof(IDebugClient4), (void **)&dbgExt->client4 );
    
    
    client->QueryInterface( __uuidof(IDebugControl), (void **)&dbgExt->control );
    client->QueryInterface( __uuidof(IDebugControl4), (void **)&dbgExt->control4 );
    
    client->QueryInterface( __uuidof(IDebugRegisters), (void **)&dbgExt->registers );
    
    client->QueryInterface( __uuidof(IDebugSymbols), (void ** )&dbgExt->symbols );
    client->QueryInterface( __uuidof(IDebugSymbols2), (void ** )&dbgExt->symbols2 );    
    client->QueryInterface( __uuidof(IDebugSymbols3), (void ** )&dbgExt->symbols3 );      
    
    client->QueryInterface( __uuidof(IDebugDataSpaces), (void **)&dbgExt->dataSpaces );
    client->QueryInterface( __uuidof(IDebugDataSpaces4), (void **)&dbgExt->dataSpaces4 );
    
    client->QueryInterface( __uuidof(IDebugAdvanced2), (void **)&dbgExt->advanced2 );
    
    client->QueryInterface( __uuidof(IDebugSystemObjects), (void**)&dbgExt->system );
    client->QueryInterface( __uuidof(IDebugSystemObjects2), (void**)&dbgExt->system2 );
}

DbgExt::~DbgExt()
{
    if ( client )
        client->Release();
        
    if ( client4 )
        client4->Release();
        
    if ( control )
        control->Release();
        
    if ( control4 )
        control4->Release();
        
    if ( registers )
        registers->Release();
            
    if ( symbols )
        symbols->Release();
        
    if ( symbols2 )
        symbols2->Release();
        
    if ( symbols3 )
        symbols3->Release();
        
    if ( dataSpaces )
        dataSpaces->Release();
        
    if ( dataSpaces4 )
        dataSpaces4->Release();
        
    if ( advanced2 )
        advanced2->Release();
        
    if ( system )
        system->Release();
        
    if ( system2 )
        system2->Release();
}

/////////////////////////////////////////////////////////////////////////////////    

struct py_params
{
    PDEBUG_CLIENT4 client;
    std::vector<std::string> args;
    std::string redirect_output;
    bool thread;
    UINT id;

    py_params(PDEBUG_CLIENT4 client, 
              const std::vector<std::string> & args,
              bool thread, 
              const std::string & redirect_output = "", 
              UINT id = 0)
        : client(client)
        , args(args)
        , thread(thread)
        , redirect_output(redirect_output)
        , id(id)
    {}
};

unsigned __stdcall py_thread(void * param)
{
    std::auto_ptr<py_params> params(reinterpret_cast<py_params *>(param));

    PyEval_AcquireLock();

    PyThreadState   *prevState = NULL;
    if ( !params->thread )
        prevState = PyThreadState_Swap( NULL );
        //prevState = PyThreadState_Get();
   
    PyThreadState   *localInterpreter = Py_NewInterpreter();

    if (params->thread)
    {
        PyInterpretersMap::iterator i = py_interpreters.find(params->id);
        if (i != py_interpreters.end())
        {
            i->second.globalInterpreter = localInterpreter;
            i->second.localInterpreter = localInterpreter;
        }
    }

    PDEBUG_CLIENT4 parent_client = params->client;
    const std::vector<std::string> & argsList = params->args;

    try 
    {
        PDEBUG_CLIENT4 client = 0;

        if (params->thread) {
            PDEBUG_CLIENT client1;

            if (FAILED(parent_client->CreateClient(&client1)))
                throw DbgException( "IDebugClient4::CreateClient failed" ); 

            if (FAILED(client1->QueryInterface(__uuidof(IDebugClient4), (void **)&client)))
                throw DbgException( "IDebugClient::QueryInterface failed" ); 
        } else {
            client = parent_client;
        }

        DbgExt ext;
        ZeroMemory(&ext, sizeof(ext));

        SetupDebugEngine(client, &ext);
        dbgExt = &ext;

        boost::python::import( "pykd" ); 
        
        boost::python::object       main =  boost::python::import("__main__");

        boost::python::object       global(main.attr("__dict__"));
        
        // перенаправление стандартных потоков ВВ
        boost::python::object       sys = boost::python::import("sys");
        
        dbgOut                      dout;
        dbgFileOut                  dfileout(params->redirect_output.c_str());
        if (params->redirect_output.empty())
            sys.attr("stdout") = boost::python::object( dout );
        else
            sys.attr("stdout") = boost::python::object( dfileout );

        dbgIn                       din;
        sys.attr("stdin") = boost::python::object( din );   

        {
            std::vector<char *> pythonArgs(argsList.size());
     
            for ( size_t  i = 0; i < argsList.size(); ++i )
                pythonArgs[i] = const_cast<char*>( argsList[i].c_str() );
                
            PySys_SetArgv( (int)pythonArgs.size(), &pythonArgs[0] );
        }

        // найти путь к файлу
        std::string     fullFileName;
        std::string     filePath;
        DbgPythonPath   dbgPythonPath;
        
        if ( dbgPythonPath.findPath( argsList[0], fullFileName, filePath ) )
        {
            try {                  
            
                boost::python::object       result;
        
                result =  boost::python::exec_file( fullFileName.c_str(), global, global );
                
            }                
            catch( boost::python::error_already_set const & )
            {
                // ошибка в скрипте
                PyObject    *errtype = NULL, *errvalue = NULL, *traceback = NULL;
                
                PyErr_Fetch( &errtype, &errvalue, &traceback );
                
                if(errvalue != NULL) 
                {
                    PyObject *s = PyObject_Str(errvalue);
                    
                    dbgExt->control->Output( DEBUG_OUTPUT_ERROR, "%s\n", PyString_AS_STRING( s ) );

                    Py_DECREF(s);
                }

                Py_XDECREF(errvalue);
                Py_XDECREF(errtype);
                Py_XDECREF(traceback);        
            }  
        }
        else
        {
      		dbgExt->control->Output( DEBUG_OUTPUT_ERROR, "script file not found\n" );
        }           
    }
    catch (std::exception & e)
    {
        dbgExt->control->Output( DEBUG_OUTPUT_ERROR, "pykd error: %s\n", e.what() );
    }
    catch (...)
    {
    }

    if (params->thread) {
        PyInterpretersMap::iterator i = py_interpreters.find(params->id);
        if (i != py_interpreters.end())
        {
            ::EnterCriticalSection(&py_interpreters_cs);
            ::CloseHandle(i->second.thread);
            py_interpreters.erase(i);
            ::LeaveCriticalSection(&py_interpreters_cs);
        }
    }
    
    Py_EndInterpreter( localInterpreter ); 
    
    if ( prevState )
        PyThreadState_Swap( prevState );
    
    PyEval_ReleaseLock();    

    return 0;
}

/////////////////////////////////////////////////////////////////////////////////    
    
HRESULT 
CALLBACK
py( PDEBUG_CLIENT4 client, PCSTR args)
{
    try {
        // разбор параметров
        typedef  boost::escaped_list_separator<char>    char_separator_t;
        typedef  boost::tokenizer< char_separator_t >   char_tokenizer_t;

        // запуск интерпретатора в отдельном потоке
        bool threaded = false;
        
        std::string                 argsStr( args );
        
        char_tokenizer_t            token( argsStr , char_separator_t( "", " \t", "\"" ) );
        std::vector<std::string>    argsList;

        std::string                 output_fname;
        
        for ( char_tokenizer_t::iterator   it = token.begin(); it != token.end(); ++it )
        {
            if ( *it == "/t" || *it == "-t" )
            {
	            threaded = true;
	            continue;
            }
            if ( *it == "/f" || *it == "-f" )
            {
                if (++it != token.end())
                    output_fname = *it;
                continue;
            }
            if ( *it != "" )
                argsList.push_back( *it );
        }            
            
        if ( argsList.empty() )
            return S_OK;

        UINT id = 0;
        if (threaded)
        {
            // TODO: maybe lock here? 
            id = (UINT)py_interpreters.size();
        }

        std::auto_ptr<py_params> params(new py_params(client, argsList, threaded, output_fname, id));

        if (threaded)
        {
            HANDLE h = reinterpret_cast<HANDLE>(_beginthreadex(0, 0, &py_thread, params.get(), 0, 0));
            if (!h)
            {
                dbgExt->control->Output( DEBUG_OUTPUT_ERROR, "can't create new thread\n" );
            }
            else
            {
                params.release();
                ::EnterCriticalSection(&py_interpreters_cs);
                py_interpreters.insert(std::make_pair( (UINT)py_interpreters.size(), Interpreter(h, argsList)));
                ::LeaveCriticalSection(&py_interpreters_cs);
            }
        } 
        else
        {
            (void)py_thread(params.release());
        }
    }
   
    catch(...)
    {           
    }     
    
    return S_OK;  
}

/////////////////////////////////////////////////////////////////////////////////  

HRESULT 
CALLBACK
pycmd( PDEBUG_CLIENT4 client, PCSTR args )
{
    DbgExt      ext;

    SetupDebugEngine( client, &ext );
    dbgExt = &ext;
    
    PyEval_AcquireLock();

    PyThreadState*    prevState =  PyThreadState_Swap( windbgGlobalSession->globalState() );

    try {

        if ( !std::string( args ).empty() )
        {
            try
            {
                boost::python::exec( args, windbgGlobalSession->global(), windbgGlobalSession->global() );
            }
            catch( boost::python::error_already_set const & )
            {
                // ошибка в скрипте
                PyObject    *errtype = NULL, *errvalue = NULL, *traceback = NULL;
                
                PyErr_Fetch( &errtype, &errvalue, &traceback );
                
                if(errvalue != NULL) 
                {
                    PyObject *s = PyObject_Str(errvalue);
                    
                    dbgExt->control->Output( DEBUG_OUTPUT_ERROR, "%s\n", PyString_AS_STRING( s )  );
                    
                    Py_DECREF(s);
                }

                Py_XDECREF(errvalue);
                Py_XDECREF(errtype);
                Py_XDECREF(traceback);        
            }  
        }
        else
        {
            char        str[100];
            ULONG       inputSize;
            bool        stopInput = false;

            do {
            
                std::string     output;
                
                dbgExt->control->Output( DEBUG_OUTPUT_NORMAL, ">>>" );
                    
                do {    
                                
                    OutputReader        outputReader( dbgExt->client );                    
                    
                    HRESULT   hres = dbgExt->control->Input( str, sizeof(str), &inputSize );
                
                    if ( FAILED( hres ) || std::string( str ) == "" )
                    {
                       stopInput = true;
                       break;
                    }                       
                    
                } while( FALSE );                    
                
                if ( !stopInput )
                    try {
                        boost::python::exec( str, windbgGlobalSession->global(), windbgGlobalSession->global() );
                    }
                    catch( boost::python::error_already_set const & )
                    {
                        // ошибка в скрипте
                        PyObject    *errtype = NULL, *errvalue = NULL, *traceback = NULL;
                        
                        PyErr_Fetch( &errtype, &errvalue, &traceback );
                        
                        if(errvalue != NULL) 
                        {
                            PyObject *s = PyObject_Str(errvalue);
                            
                            dbgExt->control->Output( DEBUG_OUTPUT_ERROR, "%s/n", PyString_AS_STRING( s ) );
                            
                            Py_DECREF(s);
                        }

                        Py_XDECREF(errvalue);
                        Py_XDECREF(errtype);
                        Py_XDECREF(traceback);        
                    }  
                    
            } while( !stopInput );                                
        }
    }
  
    catch(...)
    {           
    }     
    
    PyThreadState_Swap( prevState );
    
    PyEval_ReleaseLock();
    
    return S_OK;          
            
}

///////////////////////////////////////////////////////////////////////////////// 

HRESULT 
CALLBACK
pythonpath( PDEBUG_CLIENT4 client, PCSTR args )
{
    DbgExt      ext;

    SetupDebugEngine( client, &ext );  
    dbgExt = &ext;

    //DbgPrint::dprintln( dbgPythonPath.getStr() );

    return S_OK;
}

///////////////////////////////////////////////////////////////////////////////// 

// Displays a list of running interpreters.
HRESULT 
CALLBACK
pylist(PDEBUG_CLIENT4 client, PCSTR args)
{
    DbgExt ext;

    SetupDebugEngine(client, &ext);
    dbgExt = &ext;

    BOOST_FOREACH (const PyInterpretersMap::value_type i, py_interpreters)
    {
        std::string str(boost::algorithm::join(i.second.args, " "));
        boost::format fmt("%1$2d \"%2%\"\r\n");
        fmt % i.first % str;
        dbgExt->control4->ControlledOutput(DEBUG_OUTCTL_AMBIENT_TEXT, DEBUG_OUTPUT_NORMAL, fmt.str().c_str());
    }

    return S_OK;
}

///////////////////////////////////////////////////////////////////////////////// 

// 
HRESULT 
CALLBACK
pyresume(PDEBUG_CLIENT4 client, PCSTR args)
{
    DbgExt ext;

    SetupDebugEngine(client, &ext);
    dbgExt = &ext;

    std::string arg(args);
    if (!arg.empty())
    {
        try {
            int id = boost::lexical_cast<int>(arg);

            const PyInterpretersMap::const_iterator i = py_interpreters.find(id);
            if (i != py_interpreters.end())
            {
                ::ResumeThread(i->second.thread);
            }

        } catch (boost::bad_lexical_cast &) {
            dbgExt->control->Output( DEBUG_OUTPUT_ERROR, "Usage: !pyresume thread-id\n" );
        }
    } else {
        dbgExt->control->Output( DEBUG_OUTPUT_ERROR, "Usage: !pyresume thread-id\n" );
    }

    return S_OK;
}

///////////////////////////////////////////////////////////////////////////////// 

// 
HRESULT 
CALLBACK
pysuspend(PDEBUG_CLIENT4 client, PCSTR args)
{
    DbgExt ext;

    SetupDebugEngine(client, &ext);
    dbgExt = &ext;

    std::string arg(args);
    if (!arg.empty())
    {
        try {
            int id = boost::lexical_cast<int>(arg);

            const PyInterpretersMap::const_iterator i = py_interpreters.find(id);
            if (i != py_interpreters.end())
            {
                ::SuspendThread(i->second.thread);
            }

        } catch (boost::bad_lexical_cast &) {
            dbgExt->control->Output( DEBUG_OUTPUT_ERROR, "Usage: !pysuspend thread-id\n" );
        }
    } else {
        dbgExt->control->Output( DEBUG_OUTPUT_ERROR, "Usage: !pysuspend thread-id\n" );
    }

    return S_OK;
}

///////////////////////////////////////////////////////////////////////////////// 

HRESULT 
CALLBACK
pykill(PDEBUG_CLIENT4 client, PCSTR args)
{
    DbgExt ext;

    SetupDebugEngine(client, &ext);
    dbgExt = &ext;

    std::string arg(args);
    if (!arg.empty())
    {
        try {
            int id = boost::lexical_cast<int>(arg);

            const PyInterpretersMap::iterator i = py_interpreters.find(id);
            if (i != py_interpreters.end())
            {
                Py_EndInterpreter( i->second.localInterpreter ); 
                PyThreadState_Swap( i->second.globalInterpreter );

                HANDLE h = i->second.thread;
                ::TerminateThread(h, 0);
                ::CloseHandle(h);
                ::EnterCriticalSection(&py_interpreters_cs);
                py_interpreters.erase(i);
                ::LeaveCriticalSection(&py_interpreters_cs);
            }

        } catch (boost::bad_lexical_cast &) {
            dbgExt->control->Output( DEBUG_OUTPUT_ERROR, "Usage: !pykill thread-id\n" );
        }
    } else {
        dbgExt->control->Output( DEBUG_OUTPUT_ERROR, "Usage: !pykill thread-id\n" );
    }

    return S_OK;
}

///////////////////////////////////////////////////////////////////////////////// 
