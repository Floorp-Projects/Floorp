/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* C++ wrappers for xpt_struct.h structures. */

#ifndef xpt_cpp_h___
#define xpt_cpp_h___

#include "xpt_struct.h"
#include "xptinfo.h"

// Everything here is dependent upon - and sensitive to changes in -
// xpcom/libxpt/xpt_struct.h!

class nsXPTType : public XPTTypeDescriptorPrefix
{
// NO DATA - this a flyweight wrapper
public:
    nsXPTType()
        {}    // random contents
    nsXPTType(const XPTTypeDescriptorPrefix& prefix)
        {*(XPTTypeDescriptorPrefix*)this = prefix;}

    nsXPTType(const uint8& prefix)
        {*(uint8*)this = prefix;}

    nsXPTType& operator=(uint8 val)
        {flags = val; return *this;}

    operator uint8() const
        {return flags;}

    PRBool IsPointer() const
        {return (PRBool) (XPT_TDP_IS_POINTER(flags));}

    PRBool IsUniquePointer() const
        {return (PRBool) (XPT_TDP_IS_UNIQUE_POINTER(flags));}

    PRBool IsReference() const
        {return (PRBool) (XPT_TDP_IS_REFERENCE(flags));}

    PRBool IsArithmetic() const     // terminology from Harbison/Steele
        {return flags <= T_WCHAR;}

    PRBool IsInterfacePointer() const
        {return (PRBool) (TagPart() == T_INTERFACE ||
                          TagPart() == T_INTERFACE_IS);}

    uint8 TagPart() const
        {return (uint8) (flags & XPT_TDP_TAGMASK);}

    enum
    {
        T_I8           = TD_INT8             ,
        T_I16          = TD_INT16            ,
        T_I32          = TD_INT32            ,
        T_I64          = TD_INT64            ,
        T_U8           = TD_UINT8            ,
        T_U16          = TD_UINT16           ,
        T_U32          = TD_UINT32           ,
        T_U64          = TD_UINT64           ,
        T_FLOAT        = TD_FLOAT            ,
        T_DOUBLE       = TD_DOUBLE           ,
        T_BOOL         = TD_BOOL             ,
        T_CHAR         = TD_CHAR             ,
        T_WCHAR        = TD_WCHAR            ,
        T_VOID         = TD_VOID             ,
        T_IID          = TD_PNSIID           ,
        T_BSTR         = TD_PBSTR            ,
        T_CHAR_STR     = TD_PSTRING          ,
        T_WCHAR_STR    = TD_PWSTRING         ,
        T_INTERFACE    = TD_INTERFACE_TYPE   ,
        T_INTERFACE_IS = TD_INTERFACE_IS_TYPE
    };
// NO DATA - this a flyweight wrapper
};

class nsXPTParamInfo : public XPTParamDescriptor
{
// NO DATA - this a flyweight wrapper
public:
    nsXPTParamInfo(const XPTParamDescriptor& desc)
        {*(XPTParamDescriptor*)this = desc;}


    PRBool IsIn()  const    {return (PRBool) (XPT_PD_IS_IN(flags));}
    PRBool IsOut() const    {return (PRBool) (XPT_PD_IS_OUT(flags));}
    PRBool IsRetval() const {return (PRBool) (XPT_PD_IS_RETVAL(flags));}
    const nsXPTType GetType() const {return type.prefix;}

    uint8 GetInterfaceIsArgNumber() const
    {
        NS_PRECONDITION(GetType().TagPart() == nsXPTType::T_INTERFACE_IS,"not an interface_is");
        return type.type.argnum;
    }
    // NOTE: gettting the interface or interface iid is done via methods on
    // nsIInterfaceInfo

private:
    nsXPTParamInfo();   // no implementation
// NO DATA - this a flyweight wrapper
};

class nsXPTMethodInfo : public XPTMethodDescriptor
{
// NO DATA - this a flyweight wrapper
public:
    nsXPTMethodInfo(const XPTMethodDescriptor& desc)
        {*(XPTMethodDescriptor*)this = desc;}

    PRBool IsGetter()      const {return (PRBool) (XPT_MD_IS_GETTER(flags) );}
    PRBool IsSetter()      const {return (PRBool) (XPT_MD_IS_SETTER(flags) );}
    PRBool IsVarArgs()     const {return (PRBool) (XPT_MD_IS_VARARGS(flags));}
    PRBool IsConstructor() const {return (PRBool) (XPT_MD_IS_CTOR(flags)   );}
    PRBool IsHidden()      const {return (PRBool) (XPT_MD_IS_HIDDEN(flags) );}
    const char* GetName()  const {return name;}
    uint8 GetParamCount()  const {return num_args;}
    const nsXPTParamInfo GetParam(uint8 index) const
        {
            NS_PRECONDITION(index < GetParamCount(),"bad arg");
            return params[index];
        }
    const nsXPTParamInfo GetResult() const
        {return *result;}
private:
    nsXPTMethodInfo();  // no implementation
// NO DATA - this a flyweight wrapper
};


// forward declaration
struct nsXPCMiniVariant;

class nsXPTConstant : public XPTConstDescriptor
{
// NO DATA - this a flyweight wrapper
public:
    nsXPTConstant(const XPTConstDescriptor& desc)
        {*(XPTConstDescriptor*)this = desc;}

    const char* GetName() const
        {return name;}

    const nsXPTType GetType() const
        {return type.prefix;}

    // XXX this is ugly
    const nsXPCMiniVariant* GetValue() const
        {return (nsXPCMiniVariant*) &value;}
private:
    nsXPTConstant();    // no implementation
// NO DATA - this a flyweight wrapper
};

#endif /* xpt_cpp_h___ */
