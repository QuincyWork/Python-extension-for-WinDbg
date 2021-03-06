
#include "stdafx.h"

#include <dbgeng.h>

#include "win/dbgio.h"
#include "win/dbgeng.h"
#include "win/dbgpath.h"
#include "win/windbg.h"

using namespace pykd;

////////////////////////////////////////////////////////////////////////////////

extern "C" void initpykd();

////////////////////////////////////////////////////////////////////////////////

WindbgGlobalSession::WindbgGlobalSession()
{
    PyImport_AppendInittab("pykd", initpykd ); 

    PyEval_InitThreads();

    Py_Initialize();

    main = boost::python::import("__main__");

    python::object   main_namespace = main.attr("__dict__");

    python::object   pykd = boost::python::import( "pykd" );

    main_namespace["globalEventHandler"] = EventHandlerPtr( new EventHandlerImpl() );

   // ������ ������ from pykd import *
    python::dict     pykd_namespace( pykd.attr("__dict__") ); 

    python::list     iterkeys( pykd_namespace.iterkeys() );

    for (int i = 0; i < boost::python::len(iterkeys); i++)
    {
        std::string     key = boost::python::extract<std::string>(iterkeys[i]);

        main_namespace[ key ] = pykd_namespace[ key ];
    }

    // ��������������� ����������� ������� ��
    python::object       sys = python::import("sys");

    sys.attr("stdout") = python::object( DbgOut() );
    sys.attr("stderr") = python::object( DbgErr() );
    sys.attr("stdin") = python::object( DbgIn() );

    pyState = PyEval_SaveThread();
}
volatile LONG            WindbgGlobalSession::sessionCount = 0;

WindbgGlobalSession     *WindbgGlobalSession::windbgGlobalSession = NULL; 

////////////////////////////////////////////////////////////////////////////////

class InterruptWatch
{
public:
    InterruptWatch( PDEBUG_CLIENT4 client, python::object& global ) 
    {
        m_debugControl = client;

        m_global = global;

        m_stopEvent = CreateEvent( NULL, TRUE, FALSE, NULL );

        m_thread = 
            CreateThread(
                NULL,
                0,
                threadRoutine,
                this,
                0,
                NULL );
    }

    ~InterruptWatch()
    {
        SetEvent( m_stopEvent );

        WaitForSingleObject( m_thread, INFINITE );

        CloseHandle( m_stopEvent );

        CloseHandle( m_thread );
    }

private:


    static int quit(void *) {
        eprintln(L"CTRL+BREAK");
        PyErr_SetString( PyExc_SystemExit, "" );
        return -1;
    }

    DWORD workRoutine(){

        while( WAIT_TIMEOUT == WaitForSingleObject( m_stopEvent, 250 ) )
        {
            HRESULT  hres =  m_debugControl->GetInterrupt();
            if ( hres == S_OK )
            {
                PyGILState_STATE state = PyGILState_Ensure();
                Py_AddPendingCall(&quit, NULL);
                PyGILState_Release(state);

                break;
            }
        }

        return 0;
    }

    static DWORD WINAPI threadRoutine(LPVOID lpParameter) {
        return  static_cast<InterruptWatch*>(lpParameter)->workRoutine();
    }

    HANDLE  m_thread;

    HANDLE  m_stopEvent;

    python::object m_global;

    CComQIPtr<IDebugControl> m_debugControl;
};

////////////////////////////////////////////////////////////////////////////////

BOOL WINAPI DllMain(
  __in  HINSTANCE /*hinstDLL*/,
  __in  DWORD fdwReason,
  __in  LPVOID /*lpvReserved*/
)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        CoInitialize(NULL);
        break;

    case DLL_PROCESS_DETACH:
        CoUninitialize();
        break;

    case DLL_THREAD_ATTACH:
        CoInitialize(NULL);
        break;

    case DLL_THREAD_DETACH:
        CoUninitialize();
        break;

    }
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////

HRESULT
CALLBACK
DebugExtensionInitialize(
    OUT PULONG  Version,
    OUT PULONG  Flags )
{
    *Version = DEBUG_EXTENSION_VERSION( 1, 0 );
    *Flags = 0;

    WindbgGlobalSession::StartWindbgSession();

    return S_OK;
}

////////////////////////////////////////////////////////////////////////////////

VOID
CALLBACK
DebugExtensionUninitialize()
{
    WindbgGlobalSession::StopWindbgSession();
}

////////////////////////////////////////////////////////////////////////////////

HRESULT 
CALLBACK
py( PDEBUG_CLIENT4 client, PCSTR args )
{

    ULONG    mask = 0;
    client->GetOutputMask( &mask );
    client->SetOutputMask( mask & ~DEBUG_OUTPUT_PROMPT ); // ������ ��� �����

    WindbgGlobalSession::RestorePyState();

    PyThreadState   *globalInterpreter = PyThreadState_Swap( NULL );

    PyThreadState   *localState = Py_NewInterpreter();

    try {

        // �������� ������ � ����������� ���� ( ����� ��� ������ exec_file )
        python::object       main =  python::import("__main__");

        python::object       global(main.attr("__dict__"));

        InterruptWatch   interruptWatch( client, global );

        // ����������� ����/����� ( ����� � ������� ����� ���� ������ print )

        python::object       sys = python::import("sys");

        sys.attr("stdout") = python::object( DbgOut() );
        sys.attr("stderr") = python::object( DbgErr() );
        sys.attr("stdin") = python::object( DbgIn() );

        python::object   pykd = python::import( "pykd" );

        global["globalEventHandler"] = EventHandlerPtr( new EventHandlerImpl() );

        // ������ ����������
        typedef  boost::escaped_list_separator<char>    char_separator_t;
        typedef  boost::tokenizer< char_separator_t >   char_tokenizer_t;  

        std::string                 argsStr( args );

        char_tokenizer_t            token( argsStr , char_separator_t( "", " \t", "\"" ) );
        std::vector<std::string>    argsList;

        for ( char_tokenizer_t::iterator   it = token.begin(); it != token.end(); ++it )
        {
            if ( *it != "" )
                argsList.push_back( *it );
        }            
            
        if ( argsList.size() == 0 )
        {
            Py_EndInterpreter( localState ); 

            PyThreadState_Swap( globalInterpreter );

            WindbgGlobalSession::SavePyState();

            return S_OK;
        }
            
       // ����� ���� � �����
        std::string     fullScriptName;
        DbgPythonPath   dbgPythonPath;

        if ( dbgPythonPath.getFullFileName( argsList[0], fullScriptName ) )
        {
            char    **pythonArgs = new char* [ argsList.size() ];

            for ( size_t  i = 1; i < argsList.size(); ++i )
                pythonArgs[i] = const_cast<char*>( argsList[i].c_str() );

             pythonArgs[0] = const_cast<char*>( fullScriptName.c_str() );

             PySys_SetArgv( (int)argsList.size(), pythonArgs );

             delete[]  pythonArgs;

            try {

                python::object       result;

                result =  python::exec_file( fullScriptName.c_str(), global, global );
            }
            catch( boost::python::error_already_set const & )
            {
                printException();
            }
        }
        else
        {
            eprintln( L"script file not found" );
        }
    }
    catch(...)
    {
       eprintln( L"unexpected error" );
    }

    PyInterpreterState  *interpreter = localState->interp;

    while( interpreter->tstate_head != NULL )
    {
        PyThreadState   *threadState = (PyThreadState*)(interpreter->tstate_head);

        PyThreadState_Clear(threadState);

        PyThreadState_Swap( NULL );

        PyThreadState_Delete(threadState);
    }

    PyInterpreterState_Clear(interpreter);

    PyInterpreterState_Delete(interpreter);

    PyThreadState_Swap( globalInterpreter );

    WindbgGlobalSession::SavePyState();

    client->SetOutputMask( mask );

    return S_OK;
}

////////////////////////////////////////////////////////////////////////////////

HRESULT 
CALLBACK
pycmd( PDEBUG_CLIENT4 client, PCSTR args )
{
    WindbgGlobalSession::RestorePyState();

    ULONG    mask = 0;
    client->GetOutputMask( &mask );
    client->SetOutputMask( mask & ~DEBUG_OUTPUT_PROMPT ); // ������ ��� �����

    InterruptWatch   interruptWatch( client, WindbgGlobalSession::global() );

    try {

        python::exec( 
            "__import__('code').InteractiveConsole(__import__('__main__').__dict__).interact()\n",
            WindbgGlobalSession::global(),
            WindbgGlobalSession::global()
            );
    }
    catch(...)
    {
        if ( !PyErr_ExceptionMatches( PyExc_SystemExit ) )
        {
            eprintln( L"unexpected error" );
        }
    }

    // ����� �� �������������� ���������� ����� ���������� raise SystemExit(code)
    // ������� ����� ����� �������� ���������� callback ��
    PyErr_Clear();

    client->SetOutputMask( mask );

    WindbgGlobalSession::SavePyState();

    return S_OK;
}

////////////////////////////////////////////////////////////////////////////////
