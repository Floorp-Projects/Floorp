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
 *   Mike McCabe <mccabe@netscape.com>
 *   John Bandhauer <jband@netscape.com>
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

/* Implementation of xptiInterfaceEntry and xptiInterfaceInfo. */

#include "xptiprivate.h"

/***************************************************************************/
// Debug Instrumentation...

#ifdef SHOW_INFO_COUNT_STATS
static int DEBUG_TotalInfos = 0;
static int DEBUG_CurrentInfos = 0;
static int DEBUG_MaxInfos = 0;
static int DEBUG_MonitorEntryCount = 0;

#define LOG_INFO_CREATE(t)                                                  \
    DEBUG_TotalInfos++;                                                     \
    DEBUG_CurrentInfos++;                                                   \
    if(DEBUG_MaxInfos < DEBUG_CurrentInfos)                                 \
        DEBUG_MaxInfos = DEBUG_CurrentInfos /* no ';' */

#define LOG_INFO_DESTROY(t)                                                 \
    DEBUG_CurrentInfos-- /* no ';' */

#define LOG_INFO_MONITOR_ENTRY                                              \
    DEBUG_MonitorEntryCount++ /* no ';' */

#else /* SHOW_INFO_COUNT_STATS */

#define LOG_INFO_CREATE(t)     ((void)0)
#define LOG_INFO_DESTROY(t)    ((void)0)
#define LOG_INFO_MONITOR_ENTRY ((void)0)
#endif /* SHOW_INFO_COUNT_STATS */

#ifdef DEBUG
// static 
void xptiInterfaceInfo::DEBUG_ShutdownNotification()
{
#ifdef SHOW_INFO_COUNT_STATS
    printf("iiii %d total xptiInterfaceInfos created\n", DEBUG_TotalInfos);      
    printf("iiii %d max xptiInterfaceInfos alive at one time\n", DEBUG_MaxInfos);       
    printf("iiii %d xptiInterfaceInfos still alive\n", DEBUG_CurrentInfos);       
    printf("iiii %d times locked\n", DEBUG_MonitorEntryCount);       
#endif
}
#endif /* DEBUG */

/***************************************************************************/

// static 
xptiInterfaceEntry* 
xptiInterfaceEntry::NewEntry(const char* name,
                             int nameLength,
                             const nsID& iid,
                             const xptiTypelib& typelib,
                             xptiWorkingSet* aWorkingSet)
{
    void* place = XPT_MALLOC(aWorkingSet->GetStructArena(),
                             sizeof(xptiInterfaceEntry) + nameLength);
    if(!place)
        return nsnull;
    return new(place) xptiInterfaceEntry(name, nameLength, iid, typelib);
}

// static 
xptiInterfaceEntry* 
xptiInterfaceEntry::NewEntry(const xptiInterfaceEntry& r,
                             const xptiTypelib& typelib,
                             xptiWorkingSet* aWorkingSet)
{
    size_t nameLength = PL_strlen(r.mName);
    void* place = XPT_MALLOC(aWorkingSet->GetStructArena(),
                             sizeof(xptiInterfaceEntry) + nameLength);
    if(!place)
        return nsnull;
    return new(place) xptiInterfaceEntry(r, nameLength, typelib);
}


xptiInterfaceEntry::xptiInterfaceEntry(const char* name,
                                       size_t nameLength,
                                       const nsID& iid,
                                       const xptiTypelib& typelib)
    :   mIID(iid),
        mTypelib(typelib),
        mInfo(nsnull),
        mFlags(uint8(0))
{
    memcpy(mName, name, nameLength);
}

xptiInterfaceEntry::xptiInterfaceEntry(const xptiInterfaceEntry& r,
                                       size_t nameLength,
                                       const xptiTypelib& typelib)
    :   mIID(r.mIID),
        mTypelib(typelib),
        mInfo(nsnull),
        mFlags(r.mFlags)
{
    SetResolvedState(NOT_RESOLVED);
    memcpy(mName, r.mName, nameLength);
}

PRBool 
xptiInterfaceEntry::Resolve(xptiWorkingSet* aWorkingSet /* = nsnull */)
{
    nsAutoLock lock(xptiInterfaceInfoManager::GetResolveLock());
    return ResolveLocked(aWorkingSet);
}

PRBool 
xptiInterfaceEntry::ResolveLocked(xptiWorkingSet* aWorkingSet /* = nsnull */)
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
        xptiInterfaceEntry* parent = 
            aWorkingSet->GetTypelibGuts(mInterface->mTypelib)->
                                GetEntryAt(parent_index - 1);
        
        if(!parent || !parent->EnsureResolvedLocked())
        {
            xptiTypelib aTypelib = mInterface->mTypelib;
            mInterface = nsnull;
            mTypelib = aTypelib;
            SetResolvedState(RESOLVE_FAILED);
            return PR_FALSE;
        }

        mInterface->mParent = parent;

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
xptiInterfaceEntry::PartiallyResolveLocked(XPTInterfaceDescriptor*  aDescriptor,
                                           xptiWorkingSet*          aWorkingSet)
{
    NS_ASSERTION(GetResolveState() == NOT_RESOLVED, "bad state");

    LOG_RESOLVE(("~ partial  resolve of %s\n", mName));

    xptiInterfaceGuts* iface = 
        xptiInterfaceGuts::NewGuts(aDescriptor, mTypelib, aWorkingSet);

    if(!iface)
        return PR_FALSE;

    mInterface = iface;

#ifdef DEBUG
    if(!DEBUG_ScriptableFlagIsValid())
    {
        NS_ERROR("unexpected scriptable flag!");
        SetScriptableFlag(XPT_ID_IS_SCRIPTABLE(mInterface->mDescriptor->flags));
    }
#endif

    SetResolvedState(PARTIALLY_RESOLVED);
    return PR_TRUE;
}        

/**************************************************/
// These non-virtual methods handle the delegated nsIInterfaceInfo methods.

nsresult
xptiInterfaceEntry::GetName(char **name)
{
    // It is not necessary to Resolve because this info is read from manifest.
    *name = (char*) nsMemory::Clone(mName, PL_strlen(mName)+1);
    return *name ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult
xptiInterfaceEntry::GetIID(nsIID **iid)
{
    // It is not necessary to Resolve because this info is read from manifest.
    *iid = (nsIID*) nsMemory::Clone(&mIID, sizeof(nsIID));
    return *iid ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult
xptiInterfaceEntry::IsScriptable(PRBool* result)
{
    // It is not necessary to Resolve because this info is read from manifest.
    NS_ASSERTION(DEBUG_ScriptableFlagIsValid(), "scriptable flag out of sync!");   
    *result = GetScriptableFlag();
    return NS_OK;
}

nsresult
xptiInterfaceEntry::IsFunction(PRBool* result)
{
    if(!EnsureResolved())
        return NS_ERROR_UNEXPECTED;

    *result = XPT_ID_IS_FUNCTION(GetInterfaceGuts()->mDescriptor->flags);
    return NS_OK;
}

nsresult
xptiInterfaceEntry::GetMethodCount(uint16* count)
{
    if(!EnsureResolved())
        return NS_ERROR_UNEXPECTED;
    
    *count = mInterface->mMethodBaseIndex + 
             mInterface->mDescriptor->num_methods;
    return NS_OK;
}

nsresult
xptiInterfaceEntry::GetConstantCount(uint16* count)
{
    if(!EnsureResolved())
        return NS_ERROR_UNEXPECTED;

    *count = mInterface->mConstantBaseIndex + 
             mInterface->mDescriptor->num_constants;
    return NS_OK;
}

nsresult
xptiInterfaceEntry::GetMethodInfo(uint16 index, const nsXPTMethodInfo** info)
{
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

nsresult
xptiInterfaceEntry::GetMethodInfoForName(const char* methodName, uint16 *index,
                                         const nsXPTMethodInfo** result)
{
    if(!EnsureResolved())
        return NS_ERROR_UNEXPECTED;

    // This is a slow algorithm, but this is not expected to be called much.
    for(uint16 i = 0; i < mInterface->mDescriptor->num_methods; ++i)
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
        return mInterface->mParent->GetMethodInfoForName(methodName, index, result);
    else
    {
        *index = 0;
        *result = 0;
        return NS_ERROR_INVALID_ARG;
    }
}

nsresult
xptiInterfaceEntry::GetConstant(uint16 index, const nsXPTConstant** constant)
{
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

// this is a private helper

nsresult 
xptiInterfaceEntry::GetEntryForParam(PRUint16 methodIndex, 
                                     const nsXPTParamInfo * param,
                                     xptiInterfaceEntry** entry)
{
    if(!EnsureResolved())
        return NS_ERROR_UNEXPECTED;

    if(methodIndex < mInterface->mMethodBaseIndex)
        return mInterface->mParent->GetEntryForParam(methodIndex, param, entry);

    if(methodIndex >= mInterface->mMethodBaseIndex + 
                      mInterface->mDescriptor->num_methods)
    {
        NS_ERROR("bad param");
        return NS_ERROR_INVALID_ARG;
    }

    const XPTTypeDescriptor *td = &param->type;

    while (XPT_TDP_TAG(td->prefix) == TD_ARRAY) {
        td = &mInterface->mDescriptor->additional_types[td->type.additional_type];
    }

    if(XPT_TDP_TAG(td->prefix) != TD_INTERFACE_TYPE) {
        NS_ERROR("not an interface");
        return NS_ERROR_INVALID_ARG;
    }

    xptiInterfaceEntry* theEntry = 
            mInterface->mWorkingSet->GetTypelibGuts(mInterface->mTypelib)->
                GetEntryAt(td->type.iface - 1);
    
    // This can happen if a declared interface is not available at runtime.
    if(!theEntry)
    {
        NS_WARNING("Declared InterfaceInfo not found");
        *entry = nsnull;
        return NS_ERROR_FAILURE;
    }

    *entry = theEntry;
    return NS_OK;
}

nsresult
xptiInterfaceEntry::GetInfoForParam(uint16 methodIndex,
                                    const nsXPTParamInfo *param,
                                    nsIInterfaceInfo** info)
{
    xptiInterfaceEntry* entry;
    nsresult rv = GetEntryForParam(methodIndex, param, &entry);
    if(NS_FAILED(rv))
        return rv;

    xptiInterfaceInfo* theInfo;
    rv = entry->GetInterfaceInfo(&theInfo);    
    if(NS_FAILED(rv))
        return rv;

    *info = NS_STATIC_CAST(nsIInterfaceInfo*, theInfo);
    return NS_OK;
}

nsresult
xptiInterfaceEntry::GetIIDForParam(uint16 methodIndex,
                                   const nsXPTParamInfo* param, nsIID** iid)
{
    xptiInterfaceEntry* entry;
    nsresult rv = GetEntryForParam(methodIndex, param, &entry);
    if(NS_FAILED(rv))
        return rv;
    return entry->GetIID(iid);
}

nsresult
xptiInterfaceEntry::GetIIDForParamNoAlloc(PRUint16 methodIndex, 
                                          const nsXPTParamInfo * param, 
                                          nsIID *iid)
{
    xptiInterfaceEntry* entry;
    nsresult rv = GetEntryForParam(methodIndex, param, &entry);
    if(NS_FAILED(rv))
        return rv;
    *iid = entry->mIID;    
    return NS_OK;
}

// this is a private helper
nsresult
xptiInterfaceEntry::GetTypeInArray(const nsXPTParamInfo* param,
                                  uint16 dimension,
                                  const XPTTypeDescriptor** type)
{
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

nsresult
xptiInterfaceEntry::GetTypeForParam(uint16 methodIndex,
                                    const nsXPTParamInfo* param,
                                    uint16 dimension,
                                    nsXPTType* type)
{
    if(!EnsureResolved())
        return NS_ERROR_UNEXPECTED;

    if(methodIndex < mInterface->mMethodBaseIndex)
        return mInterface->mParent->
            GetTypeForParam(methodIndex, param, dimension, type);

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

nsresult
xptiInterfaceEntry::GetSizeIsArgNumberForParam(uint16 methodIndex,
                                               const nsXPTParamInfo* param,
                                               uint16 dimension,
                                               uint8* argnum)
{
    if(!EnsureResolved())
        return NS_ERROR_UNEXPECTED;

    if(methodIndex < mInterface->mMethodBaseIndex)
        return mInterface->mParent->
            GetSizeIsArgNumberForParam(methodIndex, param, dimension, argnum);

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

nsresult
xptiInterfaceEntry::GetLengthIsArgNumberForParam(uint16 methodIndex,
                                                 const nsXPTParamInfo* param,
                                                 uint16 dimension,
                                                 uint8* argnum)
{
    if(!EnsureResolved())
        return NS_ERROR_UNEXPECTED;

    if(methodIndex < mInterface->mMethodBaseIndex)
        return mInterface->mParent->
            GetLengthIsArgNumberForParam(methodIndex, param, dimension, argnum);

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

nsresult
xptiInterfaceEntry::GetInterfaceIsArgNumberForParam(uint16 methodIndex,
                                                    const nsXPTParamInfo* param,
                                                    uint8* argnum)
{
    if(!EnsureResolved())
        return NS_ERROR_UNEXPECTED;

    if(methodIndex < mInterface->mMethodBaseIndex)
        return mInterface->mParent->
            GetInterfaceIsArgNumberForParam(methodIndex, param, argnum);

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
nsresult 
xptiInterfaceEntry::IsIID(const nsIID * IID, PRBool *_retval)
{
    // It is not necessary to Resolve because this info is read from manifest.
    *_retval = mIID.Equals(*IID);
    return NS_OK;
}

/* void getNameShared ([shared, retval] out string name); */
nsresult 
xptiInterfaceEntry::GetNameShared(const char **name)
{
    // It is not necessary to Resolve because this info is read from manifest.
    *name = mName;
    return NS_OK;
}

/* void getIIDShared ([shared, retval] out nsIIDPtrShared iid); */
nsresult 
xptiInterfaceEntry::GetIIDShared(const nsIID * *iid)
{
    // It is not necessary to Resolve because this info is read from manifest.
    *iid = &mIID;
    return NS_OK;
}

/* PRBool hasAncestor (in nsIIDPtr iid); */
nsresult 
xptiInterfaceEntry::HasAncestor(const nsIID * iid, PRBool *_retval)
{
    *_retval = PR_FALSE;

    for(xptiInterfaceEntry* current = this; 
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

/***************************************************/

nsresult 
xptiInterfaceEntry::GetInterfaceInfo(xptiInterfaceInfo** info)
{
    nsAutoMonitor lock(xptiInterfaceInfoManager::GetInfoMonitor());
    LOG_INFO_MONITOR_ENTRY;

#ifdef SHOW_INFO_COUNT_STATS
    static int callCount = 0;
    if(!(++callCount%100))
        printf("iiii %d xptiInterfaceInfos currently alive\n", DEBUG_CurrentInfos);       
#endif

    if(!mInfo)
    {
        mInfo = new xptiInterfaceInfo(this);
        if(!mInfo)
        {
            *info = nsnull;    
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }
    
    NS_ADDREF(*info = mInfo);
    return NS_OK;    
}
    
void     
xptiInterfaceEntry::LockedInvalidateInterfaceInfo()
{
    if(mInfo)
    {
        mInfo->Invalidate(); 
        mInfo = nsnull;
    }
}

/***************************************************************************/

NS_IMPL_QUERY_INTERFACE1(xptiInterfaceInfo, nsIInterfaceInfo)

xptiInterfaceInfo::xptiInterfaceInfo(xptiInterfaceEntry* entry)
    : mEntry(entry), mParent(nsnull)
{
    LOG_INFO_CREATE(this);
}

xptiInterfaceInfo::~xptiInterfaceInfo() 
{
    LOG_INFO_DESTROY(this);
    NS_IF_RELEASE(mParent); 
    NS_ASSERTION(!mEntry, "bad state in dtor");
}

nsrefcnt
xptiInterfaceInfo::AddRef(void)
{
    nsrefcnt cnt = (nsrefcnt) PR_AtomicIncrement((PRInt32*)&mRefCnt);
    NS_LOG_ADDREF(this, cnt, "xptiInterfaceInfo", sizeof(*this));
    return cnt;
}

nsrefcnt
xptiInterfaceInfo::Release(void)
{
    xptiInterfaceEntry* entry = mEntry;
    nsrefcnt cnt = (nsrefcnt) PR_AtomicDecrement((PRInt32*)&mRefCnt);
    NS_LOG_RELEASE(this, cnt, "xptiInterfaceInfo");
    if(!cnt)
    {
        nsAutoMonitor lock(xptiInterfaceInfoManager::GetInfoMonitor());
        LOG_INFO_MONITOR_ENTRY;
        
        // If GetInterfaceInfo added and *released* a reference before we 
        // acquired the monitor then 'this' might already be dead. In that
        // case we would not want to try to access any instance data. We
        // would want to bail immediately. If 'this' is already dead then the
        // entry will no longer have a pointer to 'this'. So, we can protect 
        // ourselves from danger without more aggressive locking.
        if(entry && !entry->InterfaceInfoEquals(this))
            return 0;

        // If GetInterfaceInfo added a reference before we acquired the monitor
        // then we want to bail out of here without destorying the object.
        if(mRefCnt)
            return 1;
        
        if(mEntry)
        {
            mEntry->LockedInterfaceInfoDeathNotification();
            mEntry = nsnull;
        }

        NS_DELETEXPCOM(this);
        return 0;    
    }
    return cnt;
}

/***************************************************************************/
