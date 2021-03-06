
#include "stdafx.h"
#include "dbgengine.h"

////////////////////////////////////////////////////////////////////////////////

namespace pykd {

////////////////////////////////////////////////////////////////////////////////

ExceptionInfo::ExceptionInfo(ULONG FirstChance_, const EXCEPTION_RECORD64 &Exception)
{
    FirstChance = FirstChance_ != 0;

    ExceptionCode = Exception.ExceptionCode;
    ExceptionFlags = Exception.ExceptionFlags;
    ExceptionRecord = Exception.ExceptionRecord;
    ExceptionAddress = Exception.ExceptionAddress;

    Parameters.resize(Exception.NumberParameters);
    for (ULONG i = 0; i < Exception.NumberParameters; ++i)
        Parameters[i] = Exception.ExceptionInformation[i];
}


python::list ExceptionInfo::getParameters() const
{
    python::list ret;
    std::vector<ULONG64>::const_iterator itParam = Parameters.begin();
    for (; itParam != Parameters.end(); ++itParam)
        ret.append(*itParam);
    return ret;
}

std::string ExceptionInfo::print() const
{
    std::stringstream sstream;

    sstream << "FirstChance= " << (FirstChance ? "True" : "False") << std::endl;

    sstream << "ExceptionCode= 0x" << std::hex << ExceptionCode << std::endl;
    sstream << "ExceptionFlags= 0x" << std::hex <<  ExceptionFlags << std::endl;
    sstream << "ExceptionRecord= 0x" << std::hex <<  ExceptionRecord << std::endl;
    sstream << "ExceptionAddress= 0x" << std::hex <<  ExceptionAddress << std::endl;

    for (ULONG i = 0; i < Parameters.size(); ++i)
    {
        sstream << "Param["  << std::dec << i << "]= 0x";
        sstream << std::hex << Parameters[i] << std::endl;
    }

    return sstream.str();
}

////////////////////////////////////////////////////////////////////////////////

}

////////////////////////////////////////////////////////////////////////////////
