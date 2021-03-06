#include "stdafx.h"

#include <iomanip>

#include "typedvar.h"
#include "dbgclient.h"
#include "dbgmem.h"

namespace pykd {

///////////////////////////////////////////////////////////////////////////////////

TypedVarPtr   TypedVar::getTypedVar( IDebugClient4 *client, const TypeInfoPtr& typeInfo, VarDataPtr varData )
{
    TypedVarPtr     tv;

    if ( typeInfo->isBasicType() )
    {
        tv.reset( new BasicTypedVar( client, typeInfo, varData) );
        return tv;
    }

    if ( typeInfo->isPointer() )
    {
        tv.reset( new PtrTypedVar( client, typeInfo, varData ) );
        return tv;
    }

    if ( typeInfo->isArray() )
    {
        tv.reset( new ArrayTypedVar( client, typeInfo, varData  ) );
        return tv;
    }

    if ( typeInfo->isUserDefined() )
    {
        tv.reset( new UdtTypedVar( client, typeInfo, varData ) );
        return tv;
    }

    if ( typeInfo->isBitField() )
    {
        tv.reset( new BitFieldVar( client, typeInfo, varData ) );
        return tv;
    }

    if ( typeInfo->isEnum() )
    {
        tv.reset( new EnumTypedVar( client, typeInfo, varData ) );
        return tv;
    }

    throw DbgException( "can not create typedVar for this type" );

    return tv;
}

///////////////////////////////////////////////////////////////////////////////////

TypedVarPtr TypedVar::getTypedVarByName( const std::string &varName )
{
    return g_dbgClient->getTypedVarByName( varName );
}

TypedVarPtr TypedVar::getTypedVarByTypeName( const std::string &typeName, ULONG64 addr )
{
    return g_dbgClient->getTypedVarByTypeName( typeName, addr );
}

TypedVarPtr TypedVar::getTypedVarByTypeInfo( const TypeInfoPtr &typeInfo, ULONG64 addr )
{
    return g_dbgClient->getTypedVarByTypeInfo( typeInfo, addr );
}

///////////////////////////////////////////////////////////////////////////////////

TypedVar::TypedVar ( IDebugClient4 *client, const TypeInfoPtr& typeInfo, VarDataPtr varData ) :
    DbgObject( client ),
    m_typeInfo( typeInfo ),
    m_varData( varData ),
    m_dataKind( DataIsGlobal )
{
    m_size = m_typeInfo->getSize();
}
///////////////////////////////////////////////////////////////////////////////////

BaseTypeVariant BasicTypedVar::getValue()
{
    ULONG64     val = 0;
    m_varData->read(&val, getSize());

    if ( m_typeInfo->getName() == "Char" )
        return (LONG)*(PCHAR)&val;

    if ( m_typeInfo->getName() == "WChar" )
        return (LONG)*(PWCHAR)&val;

    if ( m_typeInfo->getName() == "Int1B" )
        return (LONG)*(PCHAR)&val;

    if ( m_typeInfo->getName() == "UInt1B" )
        return (ULONG)*(PUCHAR)&val;

    if ( m_typeInfo->getName() == "Int2B" )
        return (LONG)*(PSHORT)&val;

    if ( m_typeInfo->getName() == "UInt2B" )
        return (ULONG)*(PUSHORT)&val;

    if ( m_typeInfo->getName() == "Int4B" )
        return *(PLONG)&val;

    if ( m_typeInfo->getName() == "UInt4B" )
        return *(PULONG)&val;

    if ( m_typeInfo->getName() == "Int8B" )
        return (LONG64)*(PLONG64)&val;

    if ( m_typeInfo->getName() == "UInt8B" )
        return (ULONG64)*(PULONG64)&val;

    if ( m_typeInfo->getName() == "Long" )
        return *(PLONG)&val;

    if ( m_typeInfo->getName() == "ULong" )
        return *(PULONG)&val;

    if ( m_typeInfo->getName() == "Bool" )
        return *(bool*)&val;
    
    throw DbgException( "failed get value " );
}

///////////////////////////////////////////////////////////////////////////////////

std::string BasicTypedVar::print()
{
    std::stringstream       sstr;

    sstr << m_typeInfo->getName() << " " << m_varData->asString();
    sstr << " Value: " << printValue();

    return sstr.str();
}

///////////////////////////////////////////////////////////////////////////////////

std::string  BasicTypedVar::printValue()
{
    std::stringstream       sstr;
    
    sstr << "0x" << boost::apply_visitor( VariantToHex(), getValue() );
    sstr << " (" << boost::apply_visitor( VariantToStr(), getValue() ) << ")";

    return sstr.str();
}

///////////////////////////////////////////////////////////////////////////////////

BaseTypeVariant PtrTypedVar::getValue()
{
    return m_varData->readPtr();
}

///////////////////////////////////////////////////////////////////////////////////

TypedVarPtr PtrTypedVar::deref()
{
    VarDataPtr varData = VarDataMemory::factory( m_dataSpaces, m_varData->readPtr() );
    return TypedVar::getTypedVar( m_client, m_typeInfo->deref(), varData );
}

///////////////////////////////////////////////////////////////////////////////////

std::string PtrTypedVar::print()
{
    std::stringstream   sstr;

    sstr << "Ptr " << m_typeInfo->getName() << " " << m_varData->asString();
    sstr << " Value: " << printValue();

    return sstr.str();
}

///////////////////////////////////////////////////////////////////////////////////

std::string  PtrTypedVar::printValue()
{
    std::stringstream   sstr;    

    sstr << "0x" << boost::apply_visitor( VariantToHex(), getValue() );

    return sstr.str();
}

///////////////////////////////////////////////////////////////////////////////////

std::string ArrayTypedVar::print()
{
    std::stringstream   sstr;

    sstr << m_typeInfo->getName() << " " << m_varData->asString();

    return sstr.str();
}

///////////////////////////////////////////////////////////////////////////////////

std::string  ArrayTypedVar::printValue()
{
    return "";
}

///////////////////////////////////////////////////////////////////////////////////

python::object ArrayTypedVar::getElementByIndex( ULONG  index ) 
{
    if ( index >= m_typeInfo->getCount() )
        throw PyException( PyExc_IndexError, "Index out of range" );

    TypeInfoPtr     elementType = m_typeInfo->getElementType();

    return python::object( TypedVar::getTypedVar( m_client, elementType, m_varData->fork(elementType->getSize()*index) ) );
}

///////////////////////////////////////////////////////////////////////////////////

TypedVarPtr
UdtTypedVar::getField( const std::string &fieldName ) 
{
    TypeInfoPtr fieldType = m_typeInfo->getField( fieldName );

    if ( fieldType->isStaticMember() )
    {
        if ( fieldType->getStaticOffset() == 0 )
            throw ImplementException( __FILE__, __LINE__, "Fix ME");

        return  TypedVar::getTypedVar( m_client, fieldType, VarDataMemory::factory(m_dataSpaces, fieldType->getStaticOffset() ) );
    }

    ULONG   fieldOffset = 0;

    fieldOffset = m_typeInfo->getFieldOffsetByNameRecirsive(fieldName);

    if ( fieldType->isVirtualMember() )
    {
        fieldOffset += getVirtualBaseDisplacement( fieldType );
    }

    return  TypedVar::getTypedVar( m_client, fieldType, m_varData->fork(fieldOffset) );
}

///////////////////////////////////////////////////////////////////////////////////

python::object
UdtTypedVar::getElementByIndex( ULONG  index )
{
    TypeInfoPtr     fieldType = m_typeInfo->getFieldByIndex(index);

    if ( fieldType->isStaticMember() )
    {
        if ( fieldType->getStaticOffset() == 0 )
            throw ImplementException( __FILE__, __LINE__, "Fix ME");

        return python::make_tuple(
            m_typeInfo->getFieldNameByIndex(index), 
            TypedVar::getTypedVar( m_client, fieldType, VarDataMemory::factory(m_dataSpaces, fieldType->getStaticOffset() ) ) );
    }

    ULONG   fieldOffset = m_typeInfo->getFieldOffsetByIndex(index);

    if ( fieldType->isVirtualMember() )
    {
        fieldOffset += getVirtualBaseDisplacement( fieldType );
    }

    return python::make_tuple( 
            m_typeInfo->getFieldNameByIndex(index), 
            TypedVar::getTypedVar( m_client, fieldType, m_varData->fork(fieldOffset) ) );
}

///////////////////////////////////////////////////////////////////////////////////

LONG UdtTypedVar::getVirtualBaseDisplacement( TypeInfoPtr& typeInfo )
{
    ULONG virtualBasePtr, virtualDispIndex, virtualDispSize;
    typeInfo->getVirtualDisplacement( virtualBasePtr, virtualDispIndex, virtualDispSize );

    ULONG64     vbtableOffset = m_varData->fork( virtualBasePtr )->readPtr();

    VarDataPtr   vbtable = VarDataMemory::factory(m_dataSpaces, vbtableOffset);

    LONG   displacement = 0;

    vbtable->read( &displacement, sizeof(displacement), virtualDispIndex*virtualDispSize );

    return virtualBasePtr + displacement;
}

///////////////////////////////////////////////////////////////////////////////////

std::string UdtTypedVar::print()
{
    std::stringstream  sstr;

    sstr << "struct/class: " << m_typeInfo->getName() << " " << m_varData->asString() << std::endl;
    
    for ( ULONG i = 0; i < m_typeInfo->getFieldCount(); ++i )
    {
        TypeInfoPtr     fieldType = m_typeInfo->getFieldByIndex(i);
        TypedVarPtr     fieldVar;

        if ( fieldType->isStaticMember() )
        {
            if ( fieldType->getStaticOffset() != 0 )
               fieldVar = TypedVar::getTypedVar( m_client, fieldType, VarDataMemory::factory(m_dataSpaces, fieldType->getStaticOffset() ) );

            sstr << "   =" << std::right << std::setw(10) << std::setfill('0') << std::hex << fieldType->getStaticOffset();
            sstr << " " << std::left << std::setw(18) << std::setfill(' ') << m_typeInfo->getFieldNameByIndex(i) << ':';
        }
        else
        {
            ULONG   fieldOffset = m_typeInfo->getFieldOffsetByIndex(i);

            if ( fieldType->isVirtualMember() )
            {
                fieldOffset += getVirtualBaseDisplacement( fieldType );
            }

            fieldVar = TypedVar::getTypedVar( m_client, fieldType, m_varData->fork(fieldOffset) );
            sstr << "   +" << std::right << std::setw(4) << std::setfill('0') << std::hex << fieldOffset;
            sstr << " " << std::left << std::setw(24) << std::setfill(' ') << m_typeInfo->getFieldNameByIndex(i) << ':';
        }

        sstr << " " << std::left << fieldType->getName();

        if ( fieldVar )
            sstr << "   " << fieldVar->printValue();
        else
            sstr << "   failed to get value";

        sstr << std::endl;
    }

    return sstr.str();
}

///////////////////////////////////////////////////////////////////////////////////

std::string  UdtTypedVar::printValue()
{
    return "";        
}

///////////////////////////////////////////////////////////////////////////////////

BaseTypeVariant BitFieldVar::getValue()
{
    ULONG64     val = 0;

    m_varData->read( &val, getSize() );

    val >>= m_typeInfo->getBitOffset();
    val &= ( 1 << m_typeInfo->getBitWidth() ) - 1;

    switch ( m_typeInfo->getSize() )
    {
    case 1:
        return (ULONG)*(PUCHAR)&val;

    case 2:
        return (ULONG)*(PUSHORT)&val;

    case 4:
        return *(PULONG)&val;

    case 8:
        return *(PULONG64)&val;
    }

    throw DbgException( "failed get value " );
}


///////////////////////////////////////////////////////////////////////////////////

std::string  BitFieldVar::printValue()
{
    std::stringstream       sstr;
    
    sstr << "0x" << boost::apply_visitor( VariantToHex(), getValue() );
    sstr << " (" << boost::apply_visitor( VariantToStr(), getValue() ) << ")";

    return sstr.str();
}

///////////////////////////////////////////////////////////////////////////////////

BaseTypeVariant EnumTypedVar::getValue()
{
    ULONG       val = 0;

    m_varData->read( &val, getSize() );

    return val;
};

///////////////////////////////////////////////////////////////////////////////////

std::string EnumTypedVar::print()
{
    std::stringstream       sstr;

    sstr << "enum: " << m_typeInfo->getName() << " " << m_varData->asString();
    sstr << " Value: " << printValue();

    return sstr.str();
}

///////////////////////////////////////////////////////////////////////////////////

std::string EnumTypedVar::printValue()
{  
    std::stringstream   sstr;    

    ULONG       val = boost::apply_visitor( VariantToULong(), getValue() );

    for ( ULONG i = 0; i < m_typeInfo->getFieldCount(); ++i )
    {
       ULONG       val1 = boost::apply_visitor( VariantToULong(), m_typeInfo->getFieldByIndex(i)->getValue() );

       if ( val == val1 )
       {
           sstr << m_typeInfo->getFieldNameByIndex(i);
           sstr << "(0x" << std::hex << val << ")";

           return sstr.str();
       }
    }

    sstr << "0x" << std::hex << val;
    sstr << " ( No matching name )";

    return sstr.str();
}

///////////////////////////////////////////////////////////////////////////////////

} // end pykd namespace