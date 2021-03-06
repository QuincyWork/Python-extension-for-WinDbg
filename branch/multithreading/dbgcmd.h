#pragma once

#include <string>

/////////////////////////////////////////////////////////////////////////////////

std::string
dbgCommand( const std::string &command );

template <ULONG status>
void
setExecutionStatus()
{
    HRESULT     hres;
    
    try {
    
        hres = dbgExt->control->SetExecutionStatus( status );
        
        if ( FAILED( hres ) )
            throw  DbgException( "IDebugControl::SetExecutionStatus  failed" ); 
            
        ULONG    currentStatus;                
          
        do {
            
            hres = dbgExt->control->WaitForEvent( 0, INFINITE );
        
            if ( FAILED( hres ) )
                throw  DbgException( "IDebugControl::SetExecutionStatus  failed" ); 
                            
            hres = dbgExt->control->GetExecutionStatus( &currentStatus );
                
            if ( FAILED( hres ) )
                throw  DbgException( "IDebugControl::GetExecutionStatus  failed" ); 
               
                
        } while( currentStatus != DEBUG_STATUS_BREAK && currentStatus != DEBUG_STATUS_NO_DEBUGGEE );                    
            
    } 
	catch( std::exception  &e )
	{
		dbgExt->control->Output( DEBUG_OUTPUT_ERROR, "pykd error: %s\n", e.what() );
	}
	catch(...)
	{
		dbgExt->control->Output( DEBUG_OUTPUT_ERROR, "pykd unexpected error\n" );
	}	
}

/////////////////////////////////////////////////////////////////////////////////

class dbgExtensionClass {

public:

    dbgExtensionClass() :
        m_handle( NULL )
        {}
    
    dbgExtensionClass( const char* path );
    
    ~dbgExtensionClass();
    
    std::string
    call( const std::string &command, const std::string param );

    std::string
    print() const;
    
private:

    ULONG64         m_handle;  
	std::string     m_path;
};

/////////////////////////////////////////////////////////////////////////////////


class  dbgBreakpointClass {

public:

    dbgBreakpointClass( ULONG64 offset );
    
    ~dbgBreakpointClass();
    
    bool
    set();
    
    void
    remove();    

    std::string
    print() const;

private:

    ULONG64                 m_offset;

    IDebugBreakpoint        *m_breakpoint;

};

/////////////////////////////////////////////////////////////////////////////////

ULONG64
evaluate( const std::string  &expression );
    
/////////////////////////////////////////////////////////////////////////////////