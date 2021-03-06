// 
// Stack frame: DEBUG_STACK_FRAME wrapper
// 

#pragma once

#include "dbgengine.h"
#include "context.h"
#include "symengine.h"

#include <boost/enable_shared_from_this.hpp>

////////////////////////////////////////////////////////////////////////////////

namespace pykd {

////////////////////////////////////////////////////////////////////////////////

class StackFrame;
typedef boost::shared_ptr<StackFrame> StackFramePtr;

class ScopeVars;
typedef boost::shared_ptr<ScopeVars>  ScopeVarsPtr;

////////////////////////////////////////////////////////////////////////////////

class StackFrame : public boost::enable_shared_from_this<StackFrame>
{
public:

    StackFrame( const STACK_FRAME_DESC& desc );

    ScopeVarsPtr getLocals();

    ScopeVarsPtr getParams();

    std::string print() const;

    ULONG64 getValue(RegRealativeId rri, LONG64 offset = 0) const;

    ULONG getLocalCount();

    ULONG getParamCount();

    python::object getLocalByName( const std::string& name );

    python::object getParamByName( const std::string& name );

    bool isContainsLocal( const std::string& name );

    bool isContainsParam( const std::string& name );

    python::object getLocalByIndex( ULONG index );

    python::object getParamByIndex( ULONG index );

public:

    ULONG  m_frameNumber;
    ULONG64  m_instructionOffset;
    ULONG64  m_returnOffset;
    ULONG64  m_frameOffset;
    ULONG64  m_stackOffset;

};

class ScopeVars {

public:

    ScopeVars( StackFramePtr &frame ) :
        m_frame( frame )
        {}

    virtual ULONG getVarCount() const = 0;
    virtual python::object getVarByName( const std::string &name ) = 0;
    virtual python::object getVarByIndex(ULONG index) const  = 0 ;
    virtual bool isContainsVar( const std::string &name ) = 0;

protected:

    StackFramePtr   m_frame;

};


class LocalVars : public ScopeVars {

public:

    LocalVars( StackFramePtr &frame ) :
        ScopeVars( frame )
        {}

private:

    ULONG getVarCount() const {
        return m_frame->getLocalCount();
    }

    python::object getVarByName( const std::string &name ) {
        return m_frame->getLocalByName( name );
    }

    python::object getVarByIndex(ULONG index) const {
        return m_frame->getLocalByIndex( index );
    }

    bool isContainsVar( const std::string &name ) {
        return m_frame->isContainsLocal(name);
    }
};


class FunctionParams : public ScopeVars {

public:

    FunctionParams( StackFramePtr &frame ) :
        ScopeVars( frame )
        {}

private:

    ULONG getVarCount() const {
        return m_frame->getParamCount();
    }

    python::object getVarByName( const std::string &name ) {
        return m_frame->getParamByName( name );
    }

    python::object getVarByIndex(ULONG index) const {
        return m_frame->getParamByIndex(index);
    }

    bool isContainsVar( const std::string &name ) {
        return m_frame->isContainsParam(name);
    }
};

///////////////////////////////////////////////////////////////////////////////

StackFramePtr getCurrentStackFrame();

python::list getCurrentStack();

python::list getCurrentStackWow64();

ScopeVarsPtr getLocals();

ScopeVarsPtr getParams();

///////////////////////////////////////////////////////////////////////////////

} // namespace pykd


