#pragma once

#include "dbgexcept.h"
#include "synsymbol.h"

namespace pykd {

///////////////////////////////////////////////////////////////////////////////////

class DbgObject {

protected:

    DbgObject( IDebugClient4 *client )
    {

        m_client = client;

        HRESULT    hres;
        hres = client->QueryInterface( __uuidof(IDebugClient5), (void **)&m_client5 );
        if ( FAILED( hres ) )
            throw DbgException("QueryInterface IDebugClient5 failed");

        hres = client->QueryInterface( __uuidof(IDebugControl4), (void**)&m_control );
        if ( FAILED( hres ) )
            throw DbgException("QueryInterface IDebugControl4  failed");    

        hres = client->QueryInterface( __uuidof(IDebugSymbols3), (void**)&m_symbols );
        if ( FAILED( hres ) )
            throw DbgException("QueryInterface IDebugSymbols3  failed");  

        hres = client->QueryInterface( __uuidof(IDebugAdvanced2), (void**)&m_advanced );
        if ( FAILED( hres ) )
            throw DbgException("QueryInterface IDebugAdvanced2  failed");  

        hres = client->QueryInterface( __uuidof(IDebugDataSpaces4), (void**)&m_dataSpaces );
        if ( FAILED( hres ) )
            throw DbgException("QueryInterface IDebugDataSpaces  failed"); 

        hres = client->QueryInterface( __uuidof(IDebugRegisters2), (void**)&m_registers );
        if ( FAILED( hres ) )
            throw DbgException("QueryInterface IDebugDataSpaces  failed");

        hres = client->QueryInterface( __uuidof(IDebugSystemObjects), (void**)&m_system );
        if ( FAILED( hres ) )
            throw DbgException("QueryInterface IDebugSystem2 failed");
    }
    
    virtual ~DbgObject() {};

    CComPtr<IDebugClient5>          m_client5;
    CComPtr<IDebugClient4>          m_client;
    CComPtr<IDebugControl4>         m_control;
    CComPtr<IDebugSymbols3>         m_symbols;
    CComPtr<IDebugAdvanced2>        m_advanced;
    CComPtr<IDebugDataSpaces4>      m_dataSpaces;
    CComPtr<IDebugRegisters2>       m_registers;
    CComPtr<IDebugSystemObjects2>   m_system;
};



///////////////////////////////////////////////////////////////////////////////////

}; // end of namespace pykd