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

/* Library-private header for Interface Info system. */

#ifndef xptiprivate_h___
#define xptiprivate_h___

#include "nscore.h"
#include NEW_H
#include "nsISupports.h"

// this after nsISupports, to pick up IID
// so that xpt stuff doesn't try to define it itself...
#include "xpt_struct.h"
#include "xpt_xdr.h"

#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "xptinfo.h"

#include "nsIServiceManager.h"
#include "nsILocalFile.h"
#include "nsIDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIWeakReference.h"

#include "nsCRT.h"
#include "nsMemory.h"

#include "nsCOMArray.h"
#include "nsQuickSort.h"

#include "nsXPIDLString.h"

#include "nsIInputStream.h"

#include "nsAutoLock.h"

#include "nsHashKeys.h"
#include "nsDataHashtable.h"
#include "plstr.h"
#include "prprf.h"
#include "prio.h"
#include "prtime.h"
#include "prenv.h"

#include <stdio.h>
#include <stdarg.h>

/***************************************************************************/

#if 0 && defined(DEBUG_jband)
#define LOG_RESOLVE(x) printf x
#define LOG_LOAD(x)    printf x
#define LOG_AUTOREG(x) do{printf x; xptiInterfaceInfoManager::WriteToLog x;}while(0)
#else
#define LOG_RESOLVE(x) ((void)0)
#define LOG_LOAD(x)    ((void)0)
#define LOG_AUTOREG(x) ((void)0)
#endif

#if 1 && defined(DEBUG_jband)
#define SHOW_INFO_COUNT_STATS
#endif

/***************************************************************************/

class xptiInterfaceInfo;
class xptiInterfaceInfoManager;
class xptiInterfaceEntry;
class xptiTypelibGuts;
class xptiWorkingSet;

extern XPTArena* gXPTIStructArena;

/***************************************************************************/

/***************************************************************************/

// No virtuals.
// These are always constructed in the struct arena using placement new.
// dtor need not be called.

class xptiTypelibGuts
{
public:
    static xptiTypelibGuts* Create(XPTHeader* aHeader);

    XPTHeader*          GetHeader()           {return mHeader;}
    PRUint16            GetEntryCount() const {return mHeader->num_interfaces;}
    
    void                SetEntryAt(PRUint16 i, xptiInterfaceEntry* ptr)
    {
        NS_ASSERTION(mHeader,"bad state!");
        NS_ASSERTION(i < GetEntryCount(),"bad param!");
        mEntryArray[i] = ptr;
    }        

    xptiInterfaceEntry* GetEntryAt(PRUint16 i);

private:
    xptiTypelibGuts(XPTHeader* aHeader)
        : mHeader(aHeader)
    { }
    ~xptiTypelibGuts();

private:
    XPTHeader*           mHeader;        // hold pointer into arena
    xptiInterfaceEntry*  mEntryArray[1]; // Always last. Sized to fit.
};

/***************************************************************************/

class xptiWorkingSet
{
public:
    xptiWorkingSet();
    ~xptiWorkingSet();
    
    PRBool IsValid() const;

    void InvalidateInterfaceInfos();
    void ClearHashTables();

    // utility methods...

    enum {NOT_FOUND = 0xffffffff};

    // Directory stuff...

    PRUint32 GetDirectoryCount();
    nsresult GetCloneOfDirectoryAt(PRUint32 i, nsILocalFile** dir);
    nsresult GetDirectoryAt(PRUint32 i, nsILocalFile** dir);
    PRBool   FindDirectory(nsILocalFile* dir, PRUint32* index);
    PRBool   FindDirectoryOfFile(nsILocalFile* file, PRUint32* index);
    PRBool   DirectoryAtMatchesPersistentDescriptor(PRUint32 i, const char* desc);

private:
    PRUint32        mFileCount;
    PRUint32        mMaxFileCount;

public:
    // XXX make these private with accessors
    nsDataHashtable<nsIDHashKey, xptiInterfaceEntry*> mIIDTable;
    nsDataHashtable<nsDepCharHashKey, xptiInterfaceEntry*> mNameTable;
};

/***************************************************************************/

// This class exists to help xptiInterfaceInfo store a 4-state (2 bit) value 
// and a set of bitflags in one 8bit value. See below.

class xptiInfoFlags
{
    enum {STATE_MASK = 3};
public:
    xptiInfoFlags(PRUint8 n) : mData(n) {}
    xptiInfoFlags(const xptiInfoFlags& r) : mData(r.mData) {}

    static PRUint8 GetStateMask()
        {return PRUint8(STATE_MASK);}
    
    void Clear()
        {mData = 0;}

    PRUint8 GetData() const
        {return mData;}

    PRUint8 GetState() const 
        {return mData & GetStateMask();}

    void SetState(PRUint8 state) 
        {mData &= ~GetStateMask(); mData |= state;}                                   

    void SetFlagBit(PRUint8 flag, PRBool on) 
        {if(on)
            mData |= ~GetStateMask() & flag;
         else
            mData &= GetStateMask() | ~flag;}

    PRBool GetFlagBit(PRUint8 flag) const 
        {return (mData & flag) ? PR_TRUE : PR_FALSE;}

private:
    PRUint8 mData;    
};

/****************************************************/

// No virtual methods.
// We always create in the struct arena and construct using "placement new".
// No members need dtor calls.

class xptiInterfaceEntry
{
public:
    static xptiInterfaceEntry* Create(const char* name,
                                      const nsID& iid,
                                      XPTInterfaceDescriptor* aDescriptor,
                                      xptiTypelibGuts* aTypelib);

    enum {
        PARTIALLY_RESOLVED    = 1,
        FULLY_RESOLVED        = 2,
        RESOLVE_FAILED        = 3
    };
    
    // Additional bit flags...
    enum {SCRIPTABLE = 4};

    PRUint8 GetResolveState() const {return mFlags.GetState();}
    
    PRBool IsFullyResolved() const 
        {return GetResolveState() == (PRUint8) FULLY_RESOLVED;}

    void   SetScriptableFlag(PRBool on)
                {mFlags.SetFlagBit(PRUint8(SCRIPTABLE),on);}
    PRBool GetScriptableFlag() const
                {return mFlags.GetFlagBit(PRUint8(SCRIPTABLE));}

    const nsID* GetTheIID()  const {return &mIID;}
    const char* GetTheName() const {return mName;}

    PRBool EnsureResolved()
        {return IsFullyResolved() ? PR_TRUE : Resolve();}

    nsresult GetInterfaceInfo(xptiInterfaceInfo** info);
    PRBool   InterfaceInfoEquals(const xptiInterfaceInfo* info) const 
        {return info == mInfo;}
    
    void     LockedInvalidateInterfaceInfo();
    void     LockedInterfaceInfoDeathNotification() {mInfo = nsnull;}

    xptiInterfaceEntry* Parent() const {
        NS_ASSERTION(IsFullyResolved(), "Parent() called while not resolved?");
        return mParent;
    }

    const nsID& IID() const { return mIID; }

    //////////////////////
    // These non-virtual methods handle the delegated nsIInterfaceInfo methods.

    nsresult GetName(char * *aName);
    nsresult GetIID(nsIID * *aIID);
    nsresult IsScriptable(PRBool *_retval);
    // Except this one.
    //nsresult GetParent(nsIInterfaceInfo * *aParent);
    nsresult GetMethodCount(PRUint16 *aMethodCount);
    nsresult GetConstantCount(PRUint16 *aConstantCount);
    nsresult GetMethodInfo(PRUint16 index, const nsXPTMethodInfo * *info);
    nsresult GetMethodInfoForName(const char *methodName, PRUint16 *index, const nsXPTMethodInfo * *info);
    nsresult GetConstant(PRUint16 index, const nsXPTConstant * *constant);
    nsresult GetInfoForParam(PRUint16 methodIndex, const nsXPTParamInfo * param, nsIInterfaceInfo **_retval);
    nsresult GetIIDForParam(PRUint16 methodIndex, const nsXPTParamInfo * param, nsIID * *_retval);
    nsresult GetTypeForParam(PRUint16 methodIndex, const nsXPTParamInfo * param, PRUint16 dimension, nsXPTType *_retval);
    nsresult GetSizeIsArgNumberForParam(PRUint16 methodIndex, const nsXPTParamInfo * param, PRUint16 dimension, PRUint8 *_retval);
    nsresult GetLengthIsArgNumberForParam(PRUint16 methodIndex, const nsXPTParamInfo * param, PRUint16 dimension, PRUint8 *_retval);
    nsresult GetInterfaceIsArgNumberForParam(PRUint16 methodIndex, const nsXPTParamInfo * param, PRUint8 *_retval);
    nsresult IsIID(const nsIID * IID, PRBool *_retval);
    nsresult GetNameShared(const char **name);
    nsresult GetIIDShared(const nsIID * *iid);
    nsresult IsFunction(PRBool *_retval);
    nsresult HasAncestor(const nsIID * iid, PRBool *_retval);
    nsresult GetIIDForParamNoAlloc(PRUint16 methodIndex, const nsXPTParamInfo * param, nsIID *iid);

private:
    xptiInterfaceEntry(const char* name,
                       size_t nameLength,
                       const nsID& iid,
                       XPTInterfaceDescriptor* aDescriptor,
                       xptiTypelibGuts* aTypelib);
    ~xptiInterfaceEntry();

    void SetResolvedState(int state) 
        {mFlags.SetState(PRUint8(state));}

    PRBool Resolve();

    // We only call these "*Locked" variants after locking. This is done to 
    // allow reentrace as files are loaded and various interfaces resolved 
    // without having to worry about the locked state.

    PRBool EnsureResolvedLocked()
        {return IsFullyResolved() ? PR_TRUE : ResolveLocked();}
    PRBool ResolveLocked();

    // private helpers

    nsresult GetEntryForParam(PRUint16 methodIndex, 
                              const nsXPTParamInfo * param,
                              xptiInterfaceEntry** entry);

    nsresult GetTypeInArray(const nsXPTParamInfo* param,
                            PRUint16 dimension,
                            const XPTTypeDescriptor** type);

private:
    nsID                    mIID;
    XPTInterfaceDescriptor* mDescriptor;

    PRUint16 mMethodBaseIndex;
    PRUint16 mConstantBaseIndex;
    xptiTypelibGuts* mTypelib;

    xptiInterfaceEntry*     mParent;      // Valid only when fully resolved

    xptiInterfaceInfo*      mInfo;        // May come and go.
    xptiInfoFlags           mFlags;
    char                    mName[1];     // Always last. Sized to fit.
};

class xptiInterfaceInfo : public nsIInterfaceInfo
{
public:
    NS_DECL_ISUPPORTS

    // Use delegation to implement (most!) of nsIInterfaceInfo.
    NS_IMETHOD GetName(char * *aName) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetName(aName); }
    NS_IMETHOD GetInterfaceIID(nsIID * *aIID) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetIID(aIID); }
    NS_IMETHOD IsScriptable(PRBool *_retval) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->IsScriptable(_retval); }
    // Except this one.
    NS_IMETHOD GetParent(nsIInterfaceInfo * *aParent) 
    {
        if(!EnsureResolved() || !EnsureParent())
            return NS_ERROR_UNEXPECTED;
        NS_IF_ADDREF(*aParent = mParent);
        return NS_OK;
    }
    NS_IMETHOD GetMethodCount(PRUint16 *aMethodCount) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetMethodCount(aMethodCount); }
    NS_IMETHOD GetConstantCount(PRUint16 *aConstantCount) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetConstantCount(aConstantCount); }
    NS_IMETHOD GetMethodInfo(PRUint16 index, const nsXPTMethodInfo * *info) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetMethodInfo(index, info); }
    NS_IMETHOD GetMethodInfoForName(const char *methodName, PRUint16 *index, const nsXPTMethodInfo * *info) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetMethodInfoForName(methodName, index, info); }
    NS_IMETHOD GetConstant(PRUint16 index, const nsXPTConstant * *constant) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetConstant(index, constant); }
    NS_IMETHOD GetInfoForParam(PRUint16 methodIndex, const nsXPTParamInfo * param, nsIInterfaceInfo **_retval) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetInfoForParam(methodIndex, param, _retval); }
    NS_IMETHOD GetIIDForParam(PRUint16 methodIndex, const nsXPTParamInfo * param, nsIID * *_retval) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetIIDForParam(methodIndex, param, _retval); }
    NS_IMETHOD GetTypeForParam(PRUint16 methodIndex, const nsXPTParamInfo * param, PRUint16 dimension, nsXPTType *_retval) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetTypeForParam(methodIndex, param, dimension, _retval); }
    NS_IMETHOD GetSizeIsArgNumberForParam(PRUint16 methodIndex, const nsXPTParamInfo * param, PRUint16 dimension, PRUint8 *_retval) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetSizeIsArgNumberForParam(methodIndex, param, dimension, _retval); }
    NS_IMETHOD GetLengthIsArgNumberForParam(PRUint16 methodIndex, const nsXPTParamInfo * param, PRUint16 dimension, PRUint8 *_retval) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetLengthIsArgNumberForParam(methodIndex, param, dimension, _retval); }
    NS_IMETHOD GetInterfaceIsArgNumberForParam(PRUint16 methodIndex, const nsXPTParamInfo * param, PRUint8 *_retval) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetInterfaceIsArgNumberForParam(methodIndex, param, _retval); }
    NS_IMETHOD IsIID(const nsIID * IID, PRBool *_retval) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->IsIID(IID, _retval); }
    NS_IMETHOD GetNameShared(const char **name) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetNameShared(name); }
    NS_IMETHOD GetIIDShared(const nsIID * *iid) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetIIDShared(iid); }
    NS_IMETHOD IsFunction(PRBool *_retval) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->IsFunction(_retval); }
    NS_IMETHOD HasAncestor(const nsIID * iid, PRBool *_retval) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->HasAncestor(iid, _retval); }
    NS_IMETHOD GetIIDForParamNoAlloc(PRUint16 methodIndex, const nsXPTParamInfo * param, nsIID *iid) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetIIDForParamNoAlloc(methodIndex, param, iid); }

public:
    xptiInterfaceInfo(xptiInterfaceEntry* entry);

    void Invalidate() 
        {NS_IF_RELEASE(mParent); mEntry = nsnull;}

private:

    ~xptiInterfaceInfo();

    // Note that mParent might still end up as nsnull if we don't have one.
    PRBool EnsureParent()
    {
        NS_ASSERTION(mEntry && mEntry->IsFullyResolved(), "bad EnsureParent call");
        return mParent || !mEntry->Parent() || BuildParent();
    }
    
    PRBool EnsureResolved()
    {
        return mEntry && mEntry->EnsureResolved();
    }

    PRBool BuildParent()
    {
        NS_ASSERTION(mEntry && 
                     mEntry->IsFullyResolved() && 
                     !mParent &&
                     mEntry->Parent(),
                    "bad BuildParent call");
        return NS_SUCCEEDED(mEntry->Parent()->GetInterfaceInfo(&mParent));
    }

    xptiInterfaceInfo();  // not implemented

private:
    xptiInterfaceEntry* mEntry;
    xptiInterfaceInfo*  mParent;
};

/***************************************************************************/

class xptiInterfaceInfoManager 
    : public nsIInterfaceInfoSuperManager
{
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINTERFACEINFOMANAGER
    NS_DECL_NSIINTERFACEINFOSUPERMANAGER

public:
    static xptiInterfaceInfoManager* GetSingleton();
    static void FreeInterfaceInfoManager();

    void RegisterFile(nsILocalFile* aFile);
    void RegisterInputStream(nsIInputStream* aStream);

    xptiWorkingSet*  GetWorkingSet() {return &mWorkingSet;}

    static PRLock* GetResolveLock(xptiInterfaceInfoManager* self = nsnull) 
        {if(!self && !(self = GetSingleton())) 
            return nsnull;
         return self->mResolveLock;}

    static PRLock* GetAutoRegLock(xptiInterfaceInfoManager* self = nsnull) 
        {if(!self && !(self = GetSingleton())) 
            return nsnull;
         return self->mAutoRegLock;}

    static PRMonitor* GetInfoMonitor(xptiInterfaceInfoManager* self = nsnull) 
        {if(!self && !(self = GetSingleton())) 
            return nsnull;
         return self->mInfoMonitor;}

    xptiInterfaceEntry* GetInterfaceEntryForIID(const nsIID *iid);

private:
    xptiInterfaceInfoManager();
    ~xptiInterfaceInfoManager();

    void RegisterXPTHeader(XPTHeader* aHeader);
                          
    XPTHeader* ReadXPTFile(nsILocalFile* aFile);
    XPTHeader* ReadXPTFileFromInputStream(nsIInputStream *stream);

    // idx is the index of this interface in the XPTHeader
    void VerifyAndAddEntryIfNew(XPTInterfaceDirectoryEntry* iface,
                                PRUint16 idx,
                                xptiTypelibGuts* typelib);

private:
    xptiWorkingSet               mWorkingSet;
    PRLock*                      mResolveLock;
    PRLock*                      mAutoRegLock;
    PRMonitor*                   mInfoMonitor;
    PRLock*                      mAdditionalManagersLock;
    nsCOMArray<nsISupports>      mAdditionalManagers;
};

#endif /* xptiprivate_h___ */
