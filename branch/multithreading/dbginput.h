#pragma once

#include "dbgprint.h"
#include <boost/python.hpp>
#include <boost/python/object.hpp>
#include <boost/shared_ptr.hpp>

/////////////////////////////////////////////////////////////////////////////////

class dbgOut {

public:

    void
    write( const boost::python::object  &str ) {
        DbgPrint::dprint( str );
    }         
    
};

/////////////////////////////////////////////////////////////////////////////////

class dbgFileOut 
{
public:

    explicit dbgFileOut(const char * fname)
        : m_fname(fname)
    {}

    void
    write(const boost::python::object & obj)
    {
        std::wstring str = boost::python::extract<std::wstring>(obj);
#pragma warning(suppress: 4996) // 'fopen': This function or variable may be unsafe. Consider using fopen_s instead. 
        boost::shared_ptr<FILE> fp(fopen(m_fname.c_str(), "a+"), &fclose);
        if (fp)
        {
            fprintf(fp.get(), "%S", str.c_str());
        }
    }

    std::string m_fname;
};

/////////////////////////////////////////////////////////////////////////////////

class dbgIn {

public:

    std::string
    readline() {
    
        char        str[100];
        ULONG       inputSize;
        
        OutputReader        outputReader( dbgExt->client );
    
        dbgExt->control->Input( str, sizeof(str), &inputSize );
    
        return std::string( str );
    }    

};

/////////////////////////////////////////////////////////////////////////////////

