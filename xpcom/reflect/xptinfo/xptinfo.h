/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* XPTI_PUBLIC_API and XPTI_GetInterfaceInfoManager declarations. */

#ifndef xptiinfo_h___
#define xptiinfo_h___

#include "nscore.h"
#include "xpt_struct.h"

class nsIInterfaceInfoManager;

// Flyweight wrapper classes for xpt_struct.h structs. 
// Everything here is dependent upon - and sensitive to changes in -
// xpcom/typelib/xpt/xpt_struct.h!

class nsXPTType : public XPTTypeDescriptorPrefix
{
// NO DATA - this a flyweight wrapper
public:
    nsXPTType()
        {}    // random contents
    MOZ_IMPLICIT nsXPTType(const XPTTypeDescriptorPrefix& prefix)
        {*(XPTTypeDescriptorPrefix*)this = prefix;}

    MOZ_IMPLICIT nsXPTType(const uint8_t& prefix)
        {*(uint8_t*)this = prefix;}

    nsXPTType& operator=(uint8_t val)
        {flags = val; return *this;}

    nsXPTType& operator=(const nsXPTType& other)
        {flags = other.flags; return *this;}

    operator uint8_t() const
        {return flags;}

    // 'Arithmetic' here roughly means that the value is self-contained and
    // doesn't depend on anything else in memory (ie: not a pointer, not an
    // XPCOM object, not a jsval, etc).
    //
    // Supposedly this terminology comes from Harbison/Steele, but it's still
    // a rather crappy name. We'd change it if it wasn't used all over the
    // place in xptcall. :-(
    bool IsArithmetic() const
        {return flags <= T_WCHAR;}

    // We used to abuse 'pointer' flag bit in typelib format quite extensively.
    // We've gotten rid of most of the cases, but there's still a fair amount
    // of refactoring to be done in XPCWrappedJSClass before we can safely stop
    // asking about this. In the mean time, we've got a temporary version of
    // IsPointer() that should be equivalent to what's in the typelib.
    bool deprecated_IsPointer() const
        {return !IsArithmetic() && TagPart() != T_JSVAL;}

    bool IsInterfacePointer() const
        {  switch (TagPart()) {
             default:
               return false;
             case T_INTERFACE:
             case T_INTERFACE_IS:
               return true;
           }
        }

    bool IsArray() const
        {return TagPart() == T_ARRAY;}

    // 'Dependent' means that params of this type are dependent upon other 
    // params. e.g. an T_INTERFACE_IS is dependent upon some other param at 
    // runtime to say what the interface type of this param really is.
    bool IsDependent() const
        {  switch (TagPart()) {
             default:
               return false;
             case T_INTERFACE_IS:
             case TD_ARRAY:
             case T_PSTRING_SIZE_IS:
             case T_PWSTRING_SIZE_IS:
               return true;
           }
        }

    uint8_t TagPart() const
        {return (uint8_t) (flags & XPT_TDP_TAGMASK);}

    enum
    {
        T_I8                = TD_INT8             ,
        T_I16               = TD_INT16            ,
        T_I32               = TD_INT32            ,
        T_I64               = TD_INT64            ,
        T_U8                = TD_UINT8            ,
        T_U16               = TD_UINT16           ,
        T_U32               = TD_UINT32           ,
        T_U64               = TD_UINT64           ,
        T_FLOAT             = TD_FLOAT            ,
        T_DOUBLE            = TD_DOUBLE           ,
        T_BOOL              = TD_BOOL             ,
        T_CHAR              = TD_CHAR             ,
        T_WCHAR             = TD_WCHAR            ,
        T_VOID              = TD_VOID             ,
        T_IID               = TD_PNSIID           ,
        T_DOMSTRING         = TD_DOMSTRING        ,
        T_CHAR_STR          = TD_PSTRING          ,
        T_WCHAR_STR         = TD_PWSTRING         ,
        T_INTERFACE         = TD_INTERFACE_TYPE   ,
        T_INTERFACE_IS      = TD_INTERFACE_IS_TYPE,
        T_ARRAY             = TD_ARRAY            ,
        T_PSTRING_SIZE_IS   = TD_PSTRING_SIZE_IS  ,
        T_PWSTRING_SIZE_IS  = TD_PWSTRING_SIZE_IS ,
        T_UTF8STRING        = TD_UTF8STRING       ,
        T_CSTRING           = TD_CSTRING          ,
        T_ASTRING           = TD_ASTRING          ,
        T_JSVAL             = TD_JSVAL
    };
// NO DATA - this a flyweight wrapper
};

class nsXPTParamInfo : public XPTParamDescriptor
{
// NO DATA - this a flyweight wrapper
public:
    MOZ_IMPLICIT nsXPTParamInfo(const XPTParamDescriptor& desc)
        {*(XPTParamDescriptor*)this = desc;}


    bool IsIn()  const    {return 0 != (XPT_PD_IS_IN(flags));}
    bool IsOut() const    {return 0 != (XPT_PD_IS_OUT(flags));}
    bool IsRetval() const {return 0 != (XPT_PD_IS_RETVAL(flags));}
    bool IsShared() const {return 0 != (XPT_PD_IS_SHARED(flags));}

    // Dipper types are one of the more inscrutable aspects of xpidl. In a
    // nutshell, dippers are empty container objects, created and passed by
    // the caller, and filled by the callee. The callee receives a fully-
    // formed object, and thus does not have to construct anything. But
    // the object is functionally empty, and the callee is responsible for
    // putting something useful inside of it.
    //
    // XPIDL decides which types to make dippers. The list of these types
    // is given in the isDipperType() function in typelib.py, and is currently
    // limited to 4 string types.
    //
    // When a dipper type is declared as an 'out' parameter, xpidl internally
    // converts it to an 'in', and sets the XPT_PD_DIPPER flag on it. For this
    // reason, dipper types are sometimes referred to as 'out parameters
    // masquerading as in'. The burden of maintaining this illusion falls mostly
    // on XPConnect, which creates the empty containers, and harvest the results
    // after the call.
    bool IsDipper() const {return 0 != (XPT_PD_IS_DIPPER(flags));}
    bool IsOptional() const {return 0 != (XPT_PD_IS_OPTIONAL(flags));}
    const nsXPTType GetType() const {return type.prefix;}

    bool IsStringClass() const {
      switch (GetType().TagPart()) {
        case nsXPTType::T_ASTRING:
        case nsXPTType::T_DOMSTRING:
        case nsXPTType::T_UTF8STRING:
        case nsXPTType::T_CSTRING:
          return true;
        default:
          return false;
      }
    }

    // Whether this parameter is passed indirectly on the stack. This mainly
    // applies to out/inout params, but we use it unconditionally for certain
    // types.
    bool IsIndirect() const {return IsOut() ||
                               GetType().TagPart() == nsXPTType::T_JSVAL;}

    // NOTE: other activities on types are done via methods on nsIInterfaceInfo

private:
    nsXPTParamInfo();   // no implementation
// NO DATA - this a flyweight wrapper
};

class nsXPTMethodInfo : public XPTMethodDescriptor
{
// NO DATA - this a flyweight wrapper
public:
    MOZ_IMPLICIT nsXPTMethodInfo(const XPTMethodDescriptor& desc)
        {*(XPTMethodDescriptor*)this = desc;}

    bool IsGetter()      const {return 0 != (XPT_MD_IS_GETTER(flags) );}
    bool IsSetter()      const {return 0 != (XPT_MD_IS_SETTER(flags) );}
    bool IsNotXPCOM()    const {return 0 != (XPT_MD_IS_NOTXPCOM(flags));}
    bool IsConstructor() const {return 0 != (XPT_MD_IS_CTOR(flags)   );}
    bool IsHidden()      const {return 0 != (XPT_MD_IS_HIDDEN(flags) );}
    bool WantsOptArgc()  const {return 0 != (XPT_MD_WANTS_OPT_ARGC(flags));}
    bool WantsContext()  const {return 0 != (XPT_MD_WANTS_CONTEXT(flags));}
    const char* GetName()  const {return name;}
    uint8_t GetParamCount()  const {return num_args;}
    /* idx was index before I got _sick_ of the warnings on Unix, sorry jband */
    const nsXPTParamInfo GetParam(uint8_t idx) const
        {
            NS_PRECONDITION(idx < GetParamCount(),"bad arg");
            return params[idx];
        }
    const nsXPTParamInfo GetResult() const
        {return result;}
private:
    nsXPTMethodInfo();  // no implementation
// NO DATA - this a flyweight wrapper
};


// forward declaration
struct nsXPTCMiniVariant;

class nsXPTConstant : public XPTConstDescriptor
{
// NO DATA - this a flyweight wrapper
public:
    MOZ_IMPLICIT nsXPTConstant(const XPTConstDescriptor& desc)
        {*(XPTConstDescriptor*)this = desc;}

    const char* GetName() const
        {return name;}

    const nsXPTType GetType() const
        {return type.prefix;}

    // XXX this is ugly. But sometimes you gotta do what you gotta do.
    // A reinterpret_cast won't do the trick here. And this plain C cast
    // works correctly and is safe enough.
    // See http://bugzilla.mozilla.org/show_bug.cgi?id=49641
    const nsXPTCMiniVariant* GetValue() const
        {return (nsXPTCMiniVariant*) &value;}
private:
    nsXPTConstant();    // no implementation
// NO DATA - this a flyweight wrapper
};

#endif /* xptiinfo_h___ */
