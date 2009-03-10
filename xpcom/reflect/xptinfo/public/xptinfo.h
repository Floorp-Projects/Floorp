/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* XPTI_PUBLIC_API and XPTI_GetInterfaceInfoManager declarations. */

#ifndef xptiinfo_h___
#define xptiinfo_h___

#include "prtypes.h"
#include "nscore.h"
#include "xpt_struct.h"

class nsIInterfaceInfoManager;

// Flyweight wrapper classes for xpt_struct.h structs. 
// Everything here is dependent upon - and sensitive to changes in -
// xpcom/typelib/xpt/public/xpt_struct.h!

class nsXPTType : public XPTTypeDescriptorPrefix
{
// NO DATA - this a flyweight wrapper
public:
    nsXPTType()
        {}    // random contents
    nsXPTType(const XPTTypeDescriptorPrefix& prefix)
        {*(XPTTypeDescriptorPrefix*)this = prefix;}

    nsXPTType(const PRUint8& prefix)
        {*(PRUint8*)this = prefix;}

    nsXPTType& operator=(PRUint8 val)
        {flags = val; return *this;}

    nsXPTType& operator=(const nsXPTType& other)
        {flags = other.flags; return *this;}

    operator PRUint8() const
        {return flags;}

    PRBool IsPointer() const
        {return 0 != (XPT_TDP_IS_POINTER(flags));}

    PRBool IsUniquePointer() const
        {return 0 != (XPT_TDP_IS_UNIQUE_POINTER(flags));}

    PRBool IsReference() const
        {return 0 != (XPT_TDP_IS_REFERENCE(flags));}

    PRBool IsArithmetic() const     // terminology from Harbison/Steele
        {return flags <= T_WCHAR;}

    PRBool IsInterfacePointer() const
        {  switch (TagPart()) {
             default:
               return PR_FALSE;
             case T_INTERFACE:
             case T_INTERFACE_IS:
               return PR_TRUE;
           }
        }

    PRBool IsArray() const
        {return TagPart() == T_ARRAY;}

    // 'Dependent' means that params of this type are dependent upon other 
    // params. e.g. an T_INTERFACE_IS is dependent upon some other param at 
    // runtime to say what the interface type of this param really is.
    PRBool IsDependent() const
        {  switch (TagPart()) {
             default:
               return PR_FALSE;
             case T_INTERFACE_IS:
             case TD_ARRAY:
             case T_PSTRING_SIZE_IS:
             case T_PWSTRING_SIZE_IS:
               return PR_TRUE;
           }
        }

    PRUint8 TagPart() const
        {return (PRUint8) (flags & XPT_TDP_TAGMASK);}

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
        T_ASTRING           = TD_ASTRING
    };
// NO DATA - this a flyweight wrapper
};

class nsXPTParamInfo : public XPTParamDescriptor
{
// NO DATA - this a flyweight wrapper
public:
    nsXPTParamInfo(const XPTParamDescriptor& desc)
        {*(XPTParamDescriptor*)this = desc;}


    PRBool IsIn()  const    {return 0 != (XPT_PD_IS_IN(flags));}
    PRBool IsOut() const    {return 0 != (XPT_PD_IS_OUT(flags));}
    PRBool IsRetval() const {return 0 != (XPT_PD_IS_RETVAL(flags));}
    PRBool IsShared() const {return 0 != (XPT_PD_IS_SHARED(flags));}
    PRBool IsDipper() const {return 0 != (XPT_PD_IS_DIPPER(flags));}
    PRBool IsOptional() const {return 0 != (XPT_PD_IS_OPTIONAL(flags));}
    const nsXPTType GetType() const {return type.prefix;}

    // NOTE: other activities on types are done via methods on nsIInterfaceInfo

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

    PRBool IsGetter()      const {return 0 != (XPT_MD_IS_GETTER(flags) );}
    PRBool IsSetter()      const {return 0 != (XPT_MD_IS_SETTER(flags) );}
    PRBool IsNotXPCOM()    const {return 0 != (XPT_MD_IS_NOTXPCOM(flags));}
    PRBool IsConstructor() const {return 0 != (XPT_MD_IS_CTOR(flags)   );}
    PRBool IsHidden()      const {return 0 != (XPT_MD_IS_HIDDEN(flags) );}
    const char* GetName()  const {return name;}
    PRUint8 GetParamCount()  const {return num_args;}
    /* idx was index before I got _sick_ of the warnings on Unix, sorry jband */
    const nsXPTParamInfo GetParam(PRUint8 idx) const
        {
            NS_PRECONDITION(idx < GetParamCount(),"bad arg");
            return params[idx];
        }
    const nsXPTParamInfo GetResult() const
        {return *result;}
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
    nsXPTConstant(const XPTConstDescriptor& desc)
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
