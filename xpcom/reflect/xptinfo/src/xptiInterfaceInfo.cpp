/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Implementation of xptiInterfaceInfo. */

#include "xptiprivate.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(xptiInterfaceInfo, nsIInterfaceInfo)

void 
xptiInterfaceInfo::CopyName(const char* name,
                            xptiWorkingSet* aWorkingSet)
{
    NS_ASSERTION(name, "bad param!");
    NS_ASSERTION(aWorkingSet, "bad param!");
    NS_ASSERTION(!mName, "bad caller!");

    int len = PL_strlen(name);
    char* ptr = (char*) XPT_MALLOC(aWorkingSet->GetStringArena(), len+2);
    if(ptr)
    {
        mName = &ptr[1];
        memcpy(mName, name, len);    
        // XXX These are redundant as long as the underlying arena continues
        // to zero out all mallocs. But... 
        mName[-1] = mName[len] = 0;
    }

}        


xptiInterfaceInfo::xptiInterfaceInfo(const char* name,
                                     const nsID& iid,
                                     const xptiTypelib& typelib,
                                     xptiWorkingSet* aWorkingSet)
    :   mIID(iid),
        mName(nsnull),
        mTypelib(typelib)
{
    NS_INIT_REFCNT();
    CopyName(name, aWorkingSet);
}

xptiInterfaceInfo::xptiInterfaceInfo(const xptiInterfaceInfo& r,
                                     const xptiTypelib& typelib,
                                     xptiWorkingSet* aWorkingSet)
    :   mIID(r.mIID),
        mName(nsnull),
        mTypelib(typelib)
{
    NS_INIT_REFCNT();
    CopyName(r.mName, aWorkingSet);
    if(IsValid() && r.IsValid())
    {
        mName[-1] = r.mName[-1]; // copy any flags
        SetResolvedState(NOT_RESOLVED);   
    }
}

xptiInterfaceInfo::~xptiInterfaceInfo()
{
    if(HasInterfaceRecord())
        delete mInterface;        
}        

void 
xptiInterfaceInfo::Invalidate()
{ 
    if(IsValid())
    {
        // The order of operations here is important!
        xptiTypelib typelib = GetTypelibRecord();
        if(HasInterfaceRecord())
            delete mInterface;        
        mTypelib = typelib;
        mName = nsnull;
    }
}

PRBool 
xptiInterfaceInfo::Resolve(xptiWorkingSet* aWorkingSet /* = nsnull */)
{
    nsAutoLock lock(xptiInterfaceInfoManager::GetResolveLock());
    return ResolveLocked(aWorkingSet);
}

PRBool 
xptiInterfaceInfo::ResolveLocked(xptiWorkingSet* aWorkingSet /* = nsnull */)
{
    int resolvedState = GetResolveState();

    if(resolvedState == FULLY_RESOLVED)
        return PR_TRUE;
    if(resolvedState == RESOLVE_FAILED)
        return PR_FALSE;

    xptiInterfaceInfoManager* mgr = 
        xptiInterfaceInfoManager::GetInterfaceInfoManagerNoAddRef();

    if(!mgr)
        return PR_FALSE;

    if(!aWorkingSet)
    {
        aWorkingSet = mgr->GetWorkingSet();
    }

    if(resolvedState == NOT_RESOLVED)
    {
        LOG_RESOLVE(("! begin    resolve of %s\n", mName));
        // Make a copy of mTypelib because the underlying memory will change!
        xptiTypelib typelib = mTypelib;
        
        // We expect our PartiallyResolveLocked() to get called before 
        // this returns. 
        if(!mgr->LoadFile(typelib, aWorkingSet))
        {
            SetResolvedState(RESOLVE_FAILED);
            return PR_FALSE;    
        }
        // The state was changed by LoadFile to PARTIALLY_RESOLVED, so this 
        // ...falls through...
    }

    NS_ASSERTION(GetResolveState() == PARTIALLY_RESOLVED, "bad state!");    

    // Finish out resolution by finding parent and Resolving it so
    // we can set the info we get from it.

    PRUint16 parent_index = mInterface->mDescriptor->parent_interface;

    if(parent_index)
    {
        xptiInterfaceInfo* parent = 
            aWorkingSet->GetTypelibGuts(mInterface->mTypelib)->
                                GetInfoAtNoAddRef(parent_index - 1);
        
        if(!parent || !parent->EnsureResolvedLocked())
        {
            xptiTypelib aTypelib = mInterface->mTypelib;
            delete mInterface;
            mTypelib = aTypelib;
            SetResolvedState(RESOLVE_FAILED);
            return PR_FALSE;
        }

        NS_ADDREF(mInterface->mParent = parent);

        mInterface->mMethodBaseIndex =
            parent->mInterface->mMethodBaseIndex + 
            parent->mInterface->mDescriptor->num_methods;
        
        mInterface->mConstantBaseIndex =
            parent->mInterface->mConstantBaseIndex + 
            parent->mInterface->mDescriptor->num_constants;

    }
    LOG_RESOLVE(("+ complete resolve of %s\n", mName));

    SetResolvedState(FULLY_RESOLVED);
    return PR_TRUE;
}        

// This *only* gets called by xptiInterfaceInfoManager::LoadFile (while locked).
PRBool 
xptiInterfaceInfo::PartiallyResolveLocked(XPTInterfaceDescriptor*  aDescriptor,
                                          xptiWorkingSet*          aWorkingSet)
{
    NS_ASSERTION(GetResolveState() == NOT_RESOLVED, "bad state");

    LOG_RESOLVE(("~ partial  resolve of %s\n", mName));

    xptiInterfaceGuts* iface = 
        new xptiInterfaceGuts(aDescriptor, mTypelib, aWorkingSet);

    if(!iface)
        return PR_FALSE;

    mInterface = iface;

    if(!ScriptableFlagIsValid())
    {
        NS_ERROR("unexpected scriptable flag!");
        SetScriptableFlag(XPT_ID_IS_SCRIPTABLE(mInterface->mDescriptor->flags));
    }
    SetResolvedState(PARTIALLY_RESOLVED);
    return PR_TRUE;
}

/***************************************************************************/

NS_IMETHODIMP
xptiInterfaceInfo::GetName(char **name)
{
    NS_PRECONDITION(name, "bad param");

    if(!mName)
        return NS_ERROR_UNEXPECTED;

    char* ptr = *name = (char*) nsMemory::Clone(mName, PL_strlen(mName)+1);
    return ptr ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
xptiInterfaceInfo::GetIID(nsIID **iid)
{
    NS_PRECONDITION(iid, "bad param");

    nsIID* ptr = *iid = (nsIID*) nsMemory::Clone(&mIID, sizeof(nsIID));
    return ptr ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
xptiInterfaceInfo::IsScriptable(PRBool* result)
{
    NS_ASSERTION(result, "bad bad caller!");

    // It is not necessary to Resolve because this info is read from manifest.

    NS_ASSERTION(ScriptableFlagIsValid(), "scriptable flag out of sync!");   
    *result = GetScriptableFlag();
    return NS_OK;
}

NS_IMETHODIMP
xptiInterfaceInfo::IsFunction(PRBool* result)
{
    NS_ASSERTION(result, "bad bad caller!");

    if(!EnsureResolved())
        return NS_ERROR_UNEXPECTED;

    *result = XPT_ID_IS_FUNCTION(mInterface->mDescriptor->flags);
    return NS_OK;
}

NS_IMETHODIMP
xptiInterfaceInfo::GetParent(nsIInterfaceInfo** parent)
{
    NS_PRECONDITION(parent, "bad param");

    if(!EnsureResolved())
        return NS_ERROR_UNEXPECTED;

    NS_IF_ADDREF(*parent = mInterface->mParent);
    return NS_OK;
}

NS_IMETHODIMP
xptiInterfaceInfo::GetMethodCount(uint16* count)
{
    NS_PRECONDITION(count, "bad param");

    if(!EnsureResolved())
        return NS_ERROR_UNEXPECTED;

    *count = mInterface->mMethodBaseIndex + 
             mInterface->mDescriptor->num_methods;
    return NS_OK;
}

NS_IMETHODIMP
xptiInterfaceInfo::GetConstantCount(uint16* count)
{
    NS_PRECONDITION(count, "bad param");

    if(!EnsureResolved())
        return NS_ERROR_UNEXPECTED;

    *count = mInterface->mConstantBaseIndex + 
             mInterface->mDescriptor->num_constants;
    return NS_OK;
}

NS_IMETHODIMP
xptiInterfaceInfo::GetMethodInfo(uint16 index, const nsXPTMethodInfo** info)
{
    NS_PRECONDITION(info, "bad param");

    if(!EnsureResolved())
        return NS_ERROR_UNEXPECTED;

    if(index < mInterface->mMethodBaseIndex)
        return mInterface->mParent->GetMethodInfo(index, info);

    if(index >= mInterface->mMethodBaseIndex + 
                mInterface->mDescriptor->num_methods)
    {
        NS_ERROR("bad param");
        *info = NULL;
        return NS_ERROR_INVALID_ARG;
    }

    // else...
    *info = NS_REINTERPRET_CAST(nsXPTMethodInfo*,
                                &mInterface->mDescriptor->
                                    method_descriptors[index - 
                                        mInterface->mMethodBaseIndex]);
    return NS_OK;
}

NS_IMETHODIMP
xptiInterfaceInfo::GetMethodInfoForName(const char* methodName, uint16 *index,
                                      const nsXPTMethodInfo** result)
{
    NS_PRECONDITION(methodName, "bad param");
    NS_PRECONDITION(index, "bad param");
    NS_PRECONDITION(result, "bad param");

    if(!EnsureResolved())
        return NS_ERROR_UNEXPECTED;

    // This is a slow algorithm, but this is not expected to be called much.
    for(uint16 i = 0;
        i < mInterface->mDescriptor->num_methods;
        ++i)
    {
        const nsXPTMethodInfo* info;
        info = NS_REINTERPRET_CAST(nsXPTMethodInfo*,
                                   &mInterface->mDescriptor->
                                        method_descriptors[i]);
        if (PL_strcmp(methodName, info->GetName()) == 0) {
            *index = i + mInterface->mMethodBaseIndex;
            *result = info;
            return NS_OK;
        }
    }
    if(mInterface->mParent)
        return mInterface->mParent->GetMethodInfoForName(methodName,
                                                         index, result);
    else
    {
        *index = 0;
        *result = 0;
        return NS_ERROR_INVALID_ARG;
    }
}

NS_IMETHODIMP
xptiInterfaceInfo::GetConstant(uint16 index, const nsXPTConstant** constant)
{
    NS_PRECONDITION(constant, "bad param");

    if(!EnsureResolved())
        return NS_ERROR_UNEXPECTED;

    if(index < mInterface->mConstantBaseIndex)
        return mInterface->mParent->GetConstant(index, constant);

    if(index >= mInterface->mConstantBaseIndex + 
                mInterface->mDescriptor->num_constants)
    {
        NS_PRECONDITION(0, "bad param");
        *constant = NULL;
        return NS_ERROR_INVALID_ARG;
    }

    // else...
    *constant =
        NS_REINTERPRET_CAST(nsXPTConstant*,
                            &mInterface->mDescriptor->
                                const_descriptors[index -
                                    mInterface->mConstantBaseIndex]);
    return NS_OK;
}

NS_IMETHODIMP
xptiInterfaceInfo::GetInfoForParam(uint16 methodIndex,
                                 const nsXPTParamInfo *param,
                                 nsIInterfaceInfo** info)
{
    NS_PRECONDITION(param, "bad pointer");
    NS_PRECONDITION(info, "bad pointer");

    if(!EnsureResolved())
        return NS_ERROR_UNEXPECTED;

    if(methodIndex < mInterface->mMethodBaseIndex)
        return mInterface->mParent->GetInfoForParam(methodIndex, param, info);

    if(methodIndex >= mInterface->mMethodBaseIndex + 
                      mInterface->mDescriptor->num_methods)
    {
        NS_PRECONDITION(0, "bad param");
        *info = NULL;
        return NS_ERROR_INVALID_ARG;
    }

    const XPTTypeDescriptor *td = &param->type;

    while (XPT_TDP_TAG(td->prefix) == TD_ARRAY) {
        td = &mInterface->mDescriptor->
                                additional_types[td->type.additional_type];
    }

    if(XPT_TDP_TAG(td->prefix) != TD_INTERFACE_TYPE) {
        NS_ERROR("not an interface");
        return NS_ERROR_INVALID_ARG;
    }

    nsIInterfaceInfo* theInfo =
        mInterface->mWorkingSet->GetTypelibGuts(mInterface->mTypelib)->
            GetInfoAtNoAddRef(td->type.iface - 1);

    if(!theInfo)
        return NS_ERROR_FAILURE;

    NS_ADDREF(*info = theInfo);
    return NS_OK;
}

NS_IMETHODIMP
xptiInterfaceInfo::GetIIDForParam(uint16 methodIndex,
                                const nsXPTParamInfo* param, nsIID** iid)
{
    nsCOMPtr<nsIInterfaceInfo> ii;
    nsresult rv = GetInfoForParam(methodIndex, param, getter_AddRefs(ii));
    if(NS_FAILED(rv))
        return rv;
    return ii->GetIID(iid);
}

// this is a private helper
NS_IMETHODIMP
xptiInterfaceInfo::GetTypeInArray(const nsXPTParamInfo* param,
                                uint16 dimension,
                                const XPTTypeDescriptor** type)
{
    NS_ASSERTION(param, "bad state");
    NS_ASSERTION(type, "bad state");
    NS_ASSERTION(IsFullyResolved(), "bad state");

    const XPTTypeDescriptor *td = &param->type;
    const XPTTypeDescriptor *additional_types =
                mInterface->mDescriptor->additional_types;

    for (uint16 i = 0; i < dimension; i++) {
        if(XPT_TDP_TAG(td->prefix) != TD_ARRAY) {
            NS_ERROR("bad dimension");
            return NS_ERROR_INVALID_ARG;
        }
        td = &additional_types[td->type.additional_type];
    }

    *type = td;
    return NS_OK;
}

NS_IMETHODIMP
xptiInterfaceInfo::GetTypeForParam(uint16 methodIndex,
                                 const nsXPTParamInfo* param,
                                 uint16 dimension,
                                 nsXPTType* type)
{
    NS_PRECONDITION(param, "bad pointer");
    NS_PRECONDITION(type, "bad pointer");

    if(!EnsureResolved())
        return NS_ERROR_UNEXPECTED;

    if(methodIndex < mInterface->mMethodBaseIndex)
        return mInterface->mParent->GetTypeForParam(methodIndex, param, 
                                                    dimension, type);

    if(methodIndex >= mInterface->mMethodBaseIndex + 
                      mInterface->mDescriptor->num_methods)
    {
        NS_ERROR("bad index");
        return NS_ERROR_INVALID_ARG;
    }

    const XPTTypeDescriptor *td;

    if(dimension) {
        nsresult rv = GetTypeInArray(param, dimension, &td);
        if(NS_FAILED(rv))
            return rv;
    }
    else
        td = &param->type;

    *type = nsXPTType(td->prefix);
    return NS_OK;
}

NS_IMETHODIMP
xptiInterfaceInfo::GetSizeIsArgNumberForParam(uint16 methodIndex,
                                            const nsXPTParamInfo* param,
                                            uint16 dimension,
                                            uint8* argnum)
{
    NS_PRECONDITION(param, "bad pointer");
    NS_PRECONDITION(argnum, "bad pointer");

    if(!EnsureResolved())
        return NS_ERROR_UNEXPECTED;

    if(methodIndex < mInterface->mMethodBaseIndex)
        return mInterface->mParent->
                        GetSizeIsArgNumberForParam(methodIndex, param,
                                                   dimension, argnum);

    if(methodIndex >= mInterface->mMethodBaseIndex + 
                      mInterface->mDescriptor->num_methods)
    {
        NS_ERROR("bad index");
        return NS_ERROR_INVALID_ARG;
    }

    const XPTTypeDescriptor *td;

    if(dimension) {
        nsresult rv = GetTypeInArray(param, dimension, &td);
        if(NS_FAILED(rv))
            return rv;
    }
    else
        td = &param->type;

    // verify that this is a type that has size_is
    switch (XPT_TDP_TAG(td->prefix)) {
      case TD_ARRAY:
      case TD_PSTRING_SIZE_IS:
      case TD_PWSTRING_SIZE_IS:
        break;
      default:
        NS_ERROR("not a size_is");
        return NS_ERROR_INVALID_ARG;
    }

    *argnum = td->argnum;
    return NS_OK;
}

NS_IMETHODIMP
xptiInterfaceInfo::GetLengthIsArgNumberForParam(uint16 methodIndex,
                                              const nsXPTParamInfo* param,
                                              uint16 dimension,
                                              uint8* argnum)
{
    NS_PRECONDITION(param, "bad pointer");
    NS_PRECONDITION(argnum, "bad pointer");

    if(!EnsureResolved())
        return NS_ERROR_UNEXPECTED;

    if(methodIndex < mInterface->mMethodBaseIndex)
        return mInterface->mParent->
                        GetLengthIsArgNumberForParam(methodIndex, param,
                                                     dimension, argnum);

    if(methodIndex >= mInterface->mMethodBaseIndex + 
                      mInterface->mDescriptor->num_methods)
    {
        NS_ERROR("bad index");
        return NS_ERROR_INVALID_ARG;
    }

    const XPTTypeDescriptor *td;

    if(dimension) {
        nsresult rv = GetTypeInArray(param, dimension, &td);
        if(NS_FAILED(rv)) {
            return rv;
        }
    }
    else
        td = &param->type;

    // verify that this is a type that has length_is
    switch (XPT_TDP_TAG(td->prefix)) {
      case TD_ARRAY:
      case TD_PSTRING_SIZE_IS:
      case TD_PWSTRING_SIZE_IS:
        break;
      default:
        NS_ERROR("not a length_is");
        return NS_ERROR_INVALID_ARG;
    }

    *argnum = td->argnum2;
    return NS_OK;
}

NS_IMETHODIMP
xptiInterfaceInfo::GetInterfaceIsArgNumberForParam(uint16 methodIndex,
                                                 const nsXPTParamInfo* param,
                                                 uint8* argnum)
{
    NS_PRECONDITION(param, "bad pointer");
    NS_PRECONDITION(argnum, "bad pointer");

    if(!EnsureResolved())
        return NS_ERROR_UNEXPECTED;

    if(methodIndex < mInterface->mMethodBaseIndex)
        return mInterface->mParent->
                        GetInterfaceIsArgNumberForParam(methodIndex, param,
                                                        argnum);

    if(methodIndex >= mInterface->mMethodBaseIndex + 
                      mInterface->mDescriptor->num_methods)
    {
        NS_ERROR("bad index");
        return NS_ERROR_INVALID_ARG;
    }

    const XPTTypeDescriptor *td = &param->type;

    while (XPT_TDP_TAG(td->prefix) == TD_ARRAY) {
        td = &mInterface->mDescriptor->
                                additional_types[td->type.additional_type];
    }

    if(XPT_TDP_TAG(td->prefix) != TD_INTERFACE_IS_TYPE) {
        NS_ERROR("not an iid_is");
        return NS_ERROR_INVALID_ARG;
    }

    *argnum = td->argnum;
    return NS_OK;
}

/* PRBool isIID (in nsIIDPtr IID); */
NS_IMETHODIMP 
xptiInterfaceInfo::IsIID(const nsIID * IID, PRBool *_retval)
{
    NS_PRECONDITION(IID, "bad pointer");
    NS_PRECONDITION(_retval, "bad pointer");

    *_retval = mIID.Equals(*IID);
    return NS_OK;
}

/* void getNameShared ([shared, retval] out string name); */
NS_IMETHODIMP 
xptiInterfaceInfo::GetNameShared(const char **name)
{
    NS_PRECONDITION(name, "bad param");

    if(!mName)
        return NS_ERROR_UNEXPECTED;

    *name = mName;
    return NS_OK;
}

/* void getIIDShared ([shared, retval] out nsIIDPtrShared iid); */
NS_IMETHODIMP 
xptiInterfaceInfo::GetIIDShared(const nsIID * *iid)
{
    NS_PRECONDITION(iid, "bad param");

    *iid = &mIID;
    return NS_OK;
}

/* PRBool hasAncestor (in nsIIDPtr iid); */
NS_IMETHODIMP 
xptiInterfaceInfo::HasAncestor(const nsIID * iid, PRBool *_retval)
{
    NS_PRECONDITION(iid, "bad param");
    NS_PRECONDITION(_retval, "bad param");

    *_retval = PR_FALSE;

    for(xptiInterfaceInfo* current = this; 
        current;
        current = current->mInterface->mParent)
    {
        if(current->mIID.Equals(*iid))
        {
            *_retval = PR_TRUE;
            break;
        }
        if(!current->EnsureResolved())
            return NS_ERROR_UNEXPECTED;
    }

    return NS_OK;
}
