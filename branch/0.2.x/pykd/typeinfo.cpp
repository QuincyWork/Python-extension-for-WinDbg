#include "stdafx.h"

#include <iomanip>

#include "typeinfo.h"
#include "dbgengine.h"
#include "module.h"

namespace pykd {

/////////////////////////////////////////////////////////////////////////////////////

static const boost::regex moduleSymMatch("^(?:([^!]*)!)?([^!]+)$"); 

void splitSymName( const std::string &fullName, std::string &moduleName, std::string &symbolName )
{
    boost::cmatch    matchResult;

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

    ULONG64  baseOffset = 0;
    baseOffset = findModuleBySymbol( symbolName );

    moduleName = getModuleName( baseOffset );
}

/////////////////////////////////////////////////////////////////////////////////////

TypeInfoPtr TypeInfo::getTypeInfoByName( const std::string &typeName )
{
    std::string     moduleName;
    std::string     symName;

    if ( TypeInfo::isBaseType( typeName ) )
        return TypeInfo::getBaseTypeInfo( typeName, pykd::ptrSize() );

    splitSymName( typeName, moduleName, symName );

    ModulePtr   module = Module::loadModuleByName( moduleName );

    return module->getTypeByName( symName );
}

/////////////////////////////////////////////////////////////////////////////////////

ULONG64 TypeInfo::getSymbolSize( const std::string &fullName )
{
    std::string     moduleName;
    std::string     symName;

    if ( TypeInfo::isBaseType( fullName ) )
        return TypeInfo::getBaseTypeInfo( fullName, pykd::ptrSize() )->getSize();

    splitSymName( fullName, moduleName, symName );

    ModulePtr   module = Module::loadModuleByName( moduleName );

    return module->getSymbolSize( symName );
}

/////////////////////////////////////////////////////////////////////////////////////

std::string TypeInfo::findSymbol( ULONG64 offset, bool showDisplacement)
{
    try {

        ModulePtr   module = Module::loadModuleByOffset( offset );

        try {
            std::string  symbolName =  module->getSymbolNameByVa( offset, showDisplacement );
            return module->getName() + '!' + symbolName;
        }
        catch( DbgException& )
        {
            std::stringstream sstr;
            sstr <<  module->getName() << '+' << std::hex << ( offset - module->getBase() );
            return sstr.str();
        }

    } 
    catch( DbgException& )
    {
        std::stringstream sstr;
        sstr << std::hex << offset;
        return sstr.str();
    }
}

/////////////////////////////////////////////////////////////////////////////////////

void TypeInfo::findSymbolAndDisp( ULONG64 offset, std::string &symbolName, LONG &displacement )
{
    ModulePtr   module = Module::loadModuleByOffset( offset );

    module->getSymbolAndDispByVa( offset, symbolName, displacement );

    symbolName = module->getName() + '!' + symbolName;
}

/////////////////////////////////////////////////////////////////////////////////////

ULONG64 TypeInfo::getOffset( const std::string &fullName )
{
    std::string     moduleName;
    std::string     symName;

    splitSymName( fullName, moduleName, symName );

    ModulePtr   module = Module::loadModuleByName( moduleName );

    return module->getSymbolOffset( symName );
}

/////////////////////////////////////////////////////////////////////////////////////

inline ULONG getTypePointerSize( SymbolPtr &typeSym )
{
    ULONG symTag = typeSym->getSymTag();
    if ( symTag != SymTagPointerType )
        return (typeSym->getMachineType() == IMAGE_FILE_MACHINE_AMD64) ? 8 : 4;

    return (ULONG)typeSym->getSize();
}

/////////////////////////////////////////////////////////////////////////////////////

TypeInfoPtr  TypeInfo::getTypeInfo( SymbolPtr &typeSym )
{
    ULONG symTag = typeSym->getSymTag();
    TypeInfoPtr  ptr;

    switch( symTag )
    {
    case SymTagData:

        if ( typeSym->getLocType() == LocIsBitField )
        {
            ptr = TypeInfoPtr( new BitFieldTypeInfo(typeSym) );
            break;
        }

        if ( typeSym->getDataKind() == DataIsConstant )
        {
            BaseTypeVariant     constVal;
            typeSym->getValue( constVal );
            ptr = getTypeInfo( typeSym->getType() );
            ptr->setConstant( constVal );
            break;
        }

       return getTypeInfo( typeSym->getType() );

    case SymTagBaseType:
        return getBaseTypeInfo( typeSym );

    case SymTagUDT:
    case SymTagBaseClass:
        ptr = TypeInfoPtr( new UdtTypeInfo( typeSym ) );
        break;

    case SymTagArrayType:
        ptr = TypeInfoPtr( new ArrayTypeInfo( typeSym ) );
        break;

    case SymTagPointerType:
        return TypeInfoPtr( new PointerTypeInfo( typeSym ) );

    case SymTagVTable:
        ptr = TypeInfoPtr( new PointerTypeInfo( typeSym->getType() ) );
        break;

    case SymTagEnum:
        ptr = TypeInfoPtr( new EnumTypeInfo( typeSym ) );
        break;

    case SymTagTypedef:
        ptr = getTypeInfo( typeSym->getType() );
        break;

    default:
        throw TypeException( typeSym->getName(), "this type is not supported" );
    }

    if ( ptr )
        ptr->m_ptrSize = getTypePointerSize(typeSym);

    return ptr;
}

/////////////////////////////////////////////////////////////////////////////////////

BaseTypeVariant  TypeInfo::getValue() 
{
    if ( !m_constant )
        throw TypeException( getName(), "this type is not a constant and has not a value" );

    return m_constantValue;
}

/////////////////////////////////////////////////////////////////////////////////////

TypeInfoPtr  TypeInfo::getTypeInfo( SymbolPtr &symScope, const std::string &symName )
{
    TypeInfoPtr    basePtr = getBaseTypeInfo( symName, getTypePointerSize(symScope) );
    if ( basePtr != 0 )
        return basePtr;

    SymbolPtr  symType;

    try {

        symType = symScope->getChildByName( symName );

    }catch(SymbolException&)
    {}

    if ( symType )
    {
        if ( symType->getSymTag() == SymTagData )
        {
            if ( symType->getLocType() == LocIsBitField )
            {
                return TypeInfoPtr( new BitFieldTypeInfo(symType) );
            }

            if ( symType->getDataKind() == DataIsConstant )
            {
                BaseTypeVariant     constVal;
                symType->getValue( constVal );
                TypeInfoPtr ptr = getTypeInfo( symType->getType() );
                ptr->setConstant( constVal );
                return ptr;
            }

            symType = symType->getType();
        }

        return getTypeInfo( symType );
    }

    size_t pos = symName.find_first_of( "*[" );

    if ( pos != std::string::npos )
        return  getComplexType( symScope, symName );

    throw TypeException( symName, "symbol name is not found" );
}

/////////////////////////////////////////////////////////////////////////////////////

static const boost::regex baseMatch("^(Char)|(WChar)|(Int1B)|(UInt1B)|(Int2B)|(UInt2B)|(Int4B)|(UInt4B)|(Int8B)|(UInt8B)|(Long)|(ULong)|(Float)|(Bool)|(Double)|(Hresult)$" );

bool 
TypeInfo::isBaseType( const std::string &symName )
{
    boost::cmatch    baseMatchResult;

    return boost::regex_match( symName.c_str(), baseMatchResult, baseMatch );
}

TypeInfoPtr 
TypeInfo::getBaseTypeInfo( const std::string &symName, ULONG pointerSize )
{
    boost::cmatch    baseMatchResult;

    if ( boost::regex_match( symName.c_str(), baseMatchResult, baseMatch ) )
    {
        if ( baseMatchResult[1].matched )
            return TypeInfoPtr( new TypeInfoWrapper<char>("Char", pointerSize) );

        if ( baseMatchResult[2].matched )
            return TypeInfoPtr( new TypeInfoWrapper<wchar_t>("WChar", pointerSize) );

        if ( baseMatchResult[3].matched )
            return TypeInfoPtr( new TypeInfoWrapper<char>("Int1B", pointerSize) );

        if ( baseMatchResult[4].matched )
            return TypeInfoPtr( new TypeInfoWrapper<unsigned char>("UInt1B", pointerSize) );

        if ( baseMatchResult[5].matched )
            return TypeInfoPtr( new TypeInfoWrapper<short>("Int2B", pointerSize) );

        if ( baseMatchResult[6].matched )
            return TypeInfoPtr( new TypeInfoWrapper<unsigned short>("UInt2B", pointerSize) );

        if ( baseMatchResult[7].matched )
            return TypeInfoPtr( new TypeInfoWrapper<long>("Int4B", pointerSize) );

        if ( baseMatchResult[8].matched )
            return TypeInfoPtr( new TypeInfoWrapper<unsigned long>("UInt4B", pointerSize) );

        if ( baseMatchResult[9].matched )
            return TypeInfoPtr( new TypeInfoWrapper<__int64>("Int8B", pointerSize) );

        if ( baseMatchResult[10].matched )
            return TypeInfoPtr( new TypeInfoWrapper<unsigned __int64>("UInt8B", pointerSize) );

        if ( baseMatchResult[11].matched )
            return TypeInfoPtr( new TypeInfoWrapper<long>("Long", pointerSize) );

        if ( baseMatchResult[12].matched )
            return TypeInfoPtr( new TypeInfoWrapper<unsigned long>("ULong", pointerSize) );

        if ( baseMatchResult[13].matched )
            return TypeInfoPtr( new TypeInfoWrapper<float>("Float", pointerSize) );

        if ( baseMatchResult[14].matched )
            return TypeInfoPtr( new TypeInfoWrapper<bool>("Bool", pointerSize) );

        if ( baseMatchResult[15].matched )
            return TypeInfoPtr( new TypeInfoWrapper<double>("Double", pointerSize) );

        if ( baseMatchResult[16].matched )
            return TypeInfoPtr( new TypeInfoWrapper<unsigned long>("Hresult", pointerSize) );
   }

    return TypeInfoPtr();
}

/////////////////////////////////////////////////////////////////////////////////////

TypeInfoPtr 
TypeInfo::getBaseTypeInfo( SymbolPtr &symbol )  
{
    std::string     symName = getBasicTypeName( symbol->getBaseType() );

    if ( symName == "Int" || symName == "UInt" ) 
    {
        std::stringstream   sstr;
        sstr << symName << symbol->getSize() << "B";

        return getBaseTypeInfo( sstr.str(), getTypePointerSize(symbol) );
    }

    if ( symName == "Float" && symbol->getSize() == 8  )
    {
        symName = "Double";
    }

    return getBaseTypeInfo( symName, getTypePointerSize(symbol) );
}

/////////////////////////////////////////////////////////////////////////////////////

BitFieldTypeInfo::BitFieldTypeInfo( SymbolPtr &symbol )
{
    m_bitWidth = (ULONG)symbol->getSize();
    m_bitPos = (ULONG)symbol->getBitPosition();

    TypeInfoPtr    typeInfo = TypeInfo::getBaseTypeInfo( symbol->getType() );

    m_size = (ULONG)typeInfo->getSize();
    m_signed = typeInfo->isSigned();

    std::stringstream   sstr;

    sstr << typeInfo->getName() << ":" << (ULONG)m_bitWidth;
    m_name = sstr.str();
}

///////////////////////////////////////////////////////////////////////////////////

std::string PointerTypeInfo::VoidTypeName( "Void" );

///////////////////////////////////////////////////////////////////////////////////

PointerTypeInfo::PointerTypeInfo( SymbolPtr &symbol  ) 
{
    SymbolPtr pointTo = symbol->getType();
    try
    {
        m_derefType = TypeInfo::getTypeInfo( pointTo );
    }
    catch (const SymbolException &)
    {
        m_derefType.swap( TypeInfoPtr() );
    }
    if (!derefPossible())
    {
        // special cases:
        const ULONG symTag = pointTo->getSymTag();
        switch (symTag)
        {
        //  * pointer to function
        case SymTagFunctionType:
            m_derefName = "<function>";
            break;

        case SymTagBaseType:
            //  * pointer to Void
            if (btVoid == static_cast<BasicType>(pointTo->getBaseType()))
                m_derefName = VoidTypeName;
            break;

        case SymTagVTableShape:
            m_derefName = "VTable";
            break;
        }
    }

    m_size = (ULONG)symbol->getSize();
    m_ptrSize = m_size;
}

/////////////////////////////////////////////////////////////////////////////////////

PointerTypeInfo::PointerTypeInfo( SymbolPtr &symScope, const std::string &symName ) 
{
    try
    {
        m_derefType = TypeInfo::getTypeInfo( symScope, symName );
    }
    catch (const SymbolException &)
    {
        m_derefType.swap( TypeInfoPtr() );
    }
    m_size = getTypePointerSize(symScope);
}

/////////////////////////////////////////////////////////////////////////////////////

PointerTypeInfo::PointerTypeInfo( ULONG size )
    : m_size(size)
    , m_derefName(VoidTypeName)
{
}

/////////////////////////////////////////////////////////////////////////////////////

std::string PointerTypeInfo::getName()
{
    return getComplexName();
}

/////////////////////////////////////////////////////////////////////////////////////

ULONG PointerTypeInfo::getSize()
{
    return m_size;
}

/////////////////////////////////////////////////////////////////////////////////////

ArrayTypeInfo::ArrayTypeInfo( SymbolPtr &symbol  ) 
{
    m_derefType = TypeInfo::getTypeInfo( symbol->getType() );
    m_count = symbol->getCount();
}

/////////////////////////////////////////////////////////////////////////////////////

ArrayTypeInfo::ArrayTypeInfo( SymbolPtr &symScope, const std::string &symName, ULONG count ) 
{
    m_derefType = TypeInfo::getTypeInfo( symScope, symName );
    m_count = count;
}

/////////////////////////////////////////////////////////////////////////////////////

std::string ArrayTypeInfo::getName()
{
    return getComplexName();
}

/////////////////////////////////////////////////////////////////////////////////////

ULONG ArrayTypeInfo::getSize()
{
    return m_derefType->getSize() * m_count;
}

/////////////////////////////////////////////////////////////////////////////////////

std::string TypeInfo::getComplexName()
{
    std::string       name;
    TypeInfo          *typeInfo = this;

    std::string tiName;
    do {

        if ( typeInfo->isArray() )
        {
            std::vector<ULONG>  indices;

            do {
                indices.push_back( typeInfo->getCount() );
            }
            while( ( typeInfo = dynamic_cast<ArrayTypeInfo*>(typeInfo)->getDerefType().get() )->isArray() );

            if ( !name.empty() )
            {
                name.insert( 0, 1, '(' );    
                name.insert( name.size(), 1, ')' );
            }

            std::stringstream       sstr;

            for ( std::vector<ULONG>::iterator  it = indices.begin(); it != indices.end(); ++it )
                sstr << '[' << *it << ']';

            name += sstr.str();

            continue;
        }
        else
        if ( typeInfo->isPointer() )
        {
            name.insert( 0, 1, '*' );

            PointerTypeInfo *ptrTypeInfo = dynamic_cast<PointerTypeInfo*>(typeInfo);
            if (!ptrTypeInfo->derefPossible())
            {
                tiName = ptrTypeInfo->getDerefName();
                break;
            }

            typeInfo = ptrTypeInfo->getDerefType().get();

            continue;
        }

        tiName = typeInfo->getName();
        break;

    } while ( true );

    name.insert( 0, tiName );

    return name;
}

/////////////////////////////////////////////////////////////////////////////////////

static const boost::regex bracketMatch("^([^\\(]*)\\((.*)\\)([^\\)]*)$"); 

static const boost::regex typeMatch("^([^\\(\\)\\*\\[\\]]*)([\\(\\)\\*\\[\\]\\d]*)$"); 

static const boost::regex ptrMatch("^\\*(.*)$");

static const boost::regex arrayMatch("^(.*)\\[(\\d+)\\]$");

static const boost::regex symbolMatch("^([\\*]*)([^\\(\\)\\*\\[\\]]*)([\\(\\)\\*\\[\\]\\d]*)$");

TypeInfoPtr TypeInfo::getComplexType( SymbolPtr &symScope, const std::string &symName )
{
    ULONG  ptrSize = getTypePointerSize(symScope);

    boost::cmatch    matchResult;

    if ( !boost::regex_match( symName.c_str(), matchResult, symbolMatch ) )
        throw TypeException( symName, "symbol name is invalid" );

    std::string  innerSymName = std::string( matchResult[2].first, matchResult[2].second );

    TypeInfoPtr    basePtr = getBaseTypeInfo( innerSymName, getTypePointerSize(symScope) );
    if ( basePtr != 0 )
    {
        return getRecurciveComplexType( basePtr, std::string( matchResult[3].first, matchResult[3].second ), ptrSize );
    }
            
    SymbolPtr lowestSymbol = symScope->getChildByName( innerSymName );

    if ( lowestSymbol->getSymTag() == SymTagData )
    {
        throw TypeException( symName, "symbol name can not be an expresion" );
    }
   
    return getRecurciveComplexType( getTypeInfo( lowestSymbol ), std::string( matchResult[3].first, matchResult[3].second ), ptrSize );
}

/////////////////////////////////////////////////////////////////////////////////////

TypeInfoPtr TypeInfo::getRecurciveComplexType( TypeInfoPtr &lowestType, std::string &suffix, ULONG ptrSize )
{
    boost::cmatch    matchResult;

    std::string     bracketExpr;

    if ( boost::regex_match( suffix.c_str(), matchResult, bracketMatch  ) )
    {
        bracketExpr = std::string( matchResult[2].first, matchResult[2].second );
        
        suffix = "";

        if ( matchResult[1].matched )
            suffix += std::string( matchResult[1].first, matchResult[1].second );

        if ( matchResult[3].matched )
            suffix += std::string( matchResult[3].first, matchResult[3].second );        
    }

    while( !suffix.empty() )
    {
        if ( boost::regex_match( suffix.c_str(), matchResult, ptrMatch  ) )
        {
            lowestType = TypeInfoPtr( new PointerTypeInfo( lowestType, ptrSize ) );
            suffix = std::string(matchResult[1].first, matchResult[1].second );
            continue;
        }

        if ( boost::regex_match( suffix.c_str(), matchResult, arrayMatch  ) )
        {
            lowestType = TypeInfoPtr( new ArrayTypeInfo( lowestType, std::atoi( matchResult[2].first ) ) );
            suffix = std::string(matchResult[1].first, matchResult[1].second );
            continue;
        }
    }
    
    if ( !bracketExpr.empty() )
        return getRecurciveComplexType( lowestType, bracketExpr, ptrSize );

    return lowestType;
}

/////////////////////////////////////////////////////////////////////////////////////

TypeInfoPtr TypeInfo::ptrTo()
{
    return TypeInfoPtr( new PointerTypeInfo(shared_from_this(), m_ptrSize) );
}

/////////////////////////////////////////////////////////////////////////////////////

TypeInfoPtr TypeInfo::arrayOf( ULONG count )
{
    return TypeInfoPtr( new ArrayTypeInfo(shared_from_this(), count ) );
}

/////////////////////////////////////////////////////////////////////////////////////

ULONG UdtTypeInfoBase::getFieldOffsetByNameRecursive( const std::string &fieldName )
{
    // "m_field1.m_field2" -> ["m_field1", "m_field2"]
    typedef boost::char_separator<char> CharSep;
    boost::tokenizer< CharSep > tokenizer(fieldName, CharSep("."));
    if (tokenizer.begin() == tokenizer.end())
        throw TypeException( getName(), fieldName + ": invalid field name");

    ULONG fieldOffset = 0;

    TypeInfoPtr typeInfo = shared_from_this();

    boost::tokenizer< CharSep >::iterator it = tokenizer.begin();
    for (; it != tokenizer.end(); ++it)
    {
        const std::string &name = *it;
        fieldOffset += typeInfo->getFieldOffsetByNameNotRecursively(name);
        typeInfo = typeInfo->getField(name);
    }

    return fieldOffset;
}

/////////////////////////////////////////////////////////////////////////////////////

TypeInfoPtr UdtTypeInfoBase::getFieldRecursive(const std::string &fieldName )
{
    // "m_field1.m_field2" -> ["m_field1", "m_field2"]
    typedef boost::char_separator<char> CharSep;
    boost::tokenizer< CharSep > tokenizer(fieldName, CharSep("."));
    if (tokenizer.begin() == tokenizer.end())
        throw TypeException( getName(), fieldName + ": invalid field name");

    TypeInfoPtr typeInfo = shared_from_this();

    boost::tokenizer< CharSep >::iterator it = tokenizer.begin();
    for (; it != tokenizer.end(); ++it)
    {
        const std::string &name = *it;
        typeInfo = typeInfo->getField(name);
    }

    return typeInfo;
}

/////////////////////////////////////////////////////////////////////////////////////

std::string UdtTypeInfoBase::print()
{
    std::stringstream  sstr;

    sstr << "class/struct " << ": " << getName() << " Size: 0x" << std::hex << getSize() << " (" << std::dec << getSize() << ")" << std::endl;
    
    ULONG       fieldCount = getFieldCount();

    for ( ULONG i = 0; i < fieldCount; ++i )
    {
        UdtFieldPtr&  udtField = lookupField(i);

        if ( udtField->isStaticMember() )
        {
            sstr << "   =" << std::right << std::setw(10) << std::setfill('0') << std::hex << udtField->getStaticOffset();
            sstr << " " << std::left << std::setw(18) << std::setfill(' ') << udtField->getName() << ':';
        }
        else
        {
            if ( udtField->isVirtualMember() )
            {
                sstr << "   virtual base " << udtField->getVirtualBaseClassName();
                sstr << " +" << std::right << std::setw(4) << std::setfill('0') << std::hex << udtField->getOffset();
                sstr << " " << udtField->getName() << ':';
            }
            else
            {
                sstr << "   +" << std::right << std::setw(4) << std::setfill('0') << std::hex << udtField->getOffset();
                sstr << " " << std::left << std::setw(24) << std::setfill(' ') << udtField->getName() << ':';
            }
        }

        sstr << " " << std::left << udtField->getTypeInfo()->getName();
        sstr << std::endl;
    }

    return sstr.str();
}

/////////////////////////////////////////////////////////////////////////////////////

void UdtTypeInfoBase::getVirtualDisplacement( const std::string& fieldName, ULONG &virtualBasePtr, ULONG &virtualDispIndex, ULONG &virtualDispSize )
{
    const UdtFieldPtr &field = lookupField(fieldName);

    if ( !field->isVirtualMember() )
        throw TypeException( getName(), "field is not a virtual member" );

    field->getVirtualDisplacement( virtualBasePtr, virtualDispIndex, virtualDispSize );
}

/////////////////////////////////////////////////////////////////////////////////////

void UdtTypeInfoBase::getVirtualDisplacementByIndex( ULONG index, ULONG &virtualBasePtr, ULONG &virtualDispIndex, ULONG &virtualDispSize )
{
    const UdtFieldPtr &field = lookupField(index);

    if ( !field->isVirtualMember() )
        throw TypeException( getName(), "field is not a virtual member" );

    field->getVirtualDisplacement( virtualBasePtr, virtualDispIndex, virtualDispSize );
}

/////////////////////////////////////////////////////////////////////////////////////

ULONG UdtTypeInfoBase::getAlignReq()
{
    ULONG alignReq = 1;
    for (ULONG i = 0; i < getFieldCount(); ++i)
    {
        const ULONG fieldAlignReq = getFieldByIndex(i)->getAlignReq();
        alignReq = (fieldAlignReq > alignReq) ? fieldAlignReq : alignReq;
    }
    return alignReq;
}

/////////////////////////////////////////////////////////////////////////////////////

void UdtTypeInfo::getFields( 
        SymbolPtr &rootSym, 
        SymbolPtr &baseVirtualSym,
        ULONG startOffset,
        LONG virtualBasePtr,
        ULONG virtualDispIndex,
        ULONG virtualDispSize )
{
    ULONG   childCount = rootSym->getChildCount();

    for ( ULONG i = 0; i < childCount; ++i )
    {
        SymbolPtr  childSym = rootSym->getChildByIndex( i );

        ULONG  symTag = childSym->getSymTag();

        if ( symTag == SymTagBaseClass )
        {
            if ( !childSym->isVirtualBaseClass() )
            {
                getFields( childSym, SymbolPtr(), startOffset + childSym->getOffset() );
            }
        }
        else
        if ( symTag == SymTagData )
        {
            SymbolUdtField  *fieldPtr = new SymbolUdtField( childSym, childSym->getName() );

            switch ( childSym->getDataKind() )
            {
            case DataIsMember:
                fieldPtr->setOffset( startOffset + childSym->getOffset() );
                break;
            case DataIsStaticMember:
                fieldPtr->setStaticOffset( childSym->getVa() );
                break;
            }

            if ( baseVirtualSym )
                fieldPtr->setVirtualDisplacement( virtualBasePtr, virtualDispIndex, virtualDispSize );

            m_fields.push_back( UdtFieldPtr( fieldPtr ) );
        }
        else
        if ( symTag == SymTagVTable )
        {
            SymbolUdtField  *fieldPtr = new SymbolUdtField( childSym, "__VFN_table" );

            fieldPtr->setOffset( startOffset + childSym->getOffset() );

            if ( baseVirtualSym )
                fieldPtr->setVirtualDisplacement( virtualBasePtr, virtualDispIndex, virtualDispSize );

            m_fields.push_back( UdtFieldPtr( fieldPtr ) );
        }
    }  
}

/////////////////////////////////////////////////////////////////////////////////////

void UdtTypeInfo::getVirtualFields()
{
    ULONG   childCount = m_dia->getChildCount(SymTagBaseClass);

    for ( ULONG i = 0; i < childCount; ++i )
    {
        SymbolPtr  childSym = m_dia->getChildByIndex( i );

        if ( !childSym->isVirtualBaseClass() )
            continue;

        getFields( 
            childSym,
            childSym,
            0,
            childSym->getVirtualBasePointerOffset(),
            childSym->getVirtualBaseDispIndex(),
            childSym->getVirtualBaseDispSize() );
    }
}

/////////////////////////////////////////////////////////////////////////////////////

void UdtTypeInfo::refreshFields()
{
    getFields( m_dia, SymbolPtr() );
    getVirtualFields();
}

/////////////////////////////////////////////////////////////////////////////////////

TypeInfoPtr EnumTypeInfo::getField( const std::string &fieldName )
{
    return TypeInfo::getTypeInfo( m_dia, fieldName );
}

/////////////////////////////////////////////////////////////////////////////////////

TypeInfoPtr EnumTypeInfo::getFieldByIndex( ULONG index ) 
{
    if ( index >= m_dia->getChildCount() )
         throw PyException( PyExc_IndexError, "index out of range" );

    SymbolPtr field = m_dia->getChildByIndex(index);

    if ( !field )
         throw PyException( PyExc_IndexError, "index out of range" );
    
    return TypeInfo::getTypeInfo( m_dia, field->getName() );
}

/////////////////////////////////////////////////////////////////////////////////////

std::string EnumTypeInfo::getFieldNameByIndex( ULONG index )
{
    if ( index >= m_dia->getChildCount() )
         throw PyException( PyExc_IndexError, "index out of range" );

    SymbolPtr field = m_dia->getChildByIndex(index);

    if ( !field )
         throw PyException( PyExc_IndexError, "index out of range" );

    return field->getName();
}

/////////////////////////////////////////////////////////////////////////////////////

ULONG EnumTypeInfo::getFieldCount()
{
    return m_dia->getChildCount();
}

/////////////////////////////////////////////////////////////////////////////////////

python::dict EnumTypeInfo::asMap()
{
    python::dict            dct;

    SymbolPtrList    symbolsList = m_dia->findChildren(SymTagData);

    for ( SymbolPtrList::iterator  it = symbolsList.begin(); it != symbolsList.end(); it++ )
    {
         BaseTypeVariant     val;

         (*it)->getValue( val );

         dct[ boost::apply_visitor( VariantToULong(), val ) ] = (*it)->getName();
    }

    return dct;
}

/////////////////////////////////////////////////////////////////////////////////////

std::string EnumTypeInfo::print()
{
    std::stringstream  sstr;

    sstr << "enum: " << getName() << std::endl;

   SymbolPtrList    symbolsList = m_dia->findChildren(SymTagData);

    for ( SymbolPtrList::iterator  it = symbolsList.begin(); it != symbolsList.end(); it++ )
    {
         BaseTypeVariant   val;
         (*it)->getValue( val );

         sstr << "   " << (*it)->getName();
         sstr << " = " << boost::apply_visitor( VariantToHex(), val ) << " (" << boost::apply_visitor( VariantToStr(), val )<< ')';
         sstr << std::endl;
    }

    return sstr.str();
}

///////////////////////////////////////////////////////////////////////////////

python::tuple getSourceLine( ULONG64 offset )
{
    if ( offset == 0 )
        offset = getRegInstructionPointer();
    else
        offset = addr64( offset );

    ModulePtr module = Module::loadModuleByOffset( offset );

    std::string  fileName;
    ULONG  lineNo;
    LONG  displacement;
    
    module->getSourceLine( offset, fileName, lineNo, displacement );

    return python::make_tuple( fileName, lineNo, displacement );
}

///////////////////////////////////////////////////////////////////////////////

std::string getSourceFile( ULONG64 offset )
{
    if ( offset == 0 )
        offset = getRegInstructionPointer();
    else
        offset = addr64( offset );

    ModulePtr module = Module::loadModuleByOffset( offset );
    
    return module->getSourceFile( offset );
}

///////////////////////////////////////////////////////////////////////////////

}; // end namespace pykd