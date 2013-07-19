/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Library-private header for Interface Info system. */

#ifndef xptiprivate_h___
#define xptiprivate_h___

#include "nscore.h"
#include <new>
#include "nsISupports.h"

// this after nsISupports, to pick up IID
// so that xpt stuff doesn't try to define it itself...
#include "xpt_struct.h"
#include "xpt_xdr.h"

#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "xptinfo.h"

#include "nsIServiceManager.h"
#include "nsIFile.h"
#include "nsIDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIWeakReference.h"

#include "mozilla/ReentrantMonitor.h"
#include "mozilla/Mutex.h"
#include "mozilla/Attributes.h"

#include "nsCRT.h"
#include "nsMemory.h"

#include "nsCOMArray.h"
#include "nsQuickSort.h"

#include "nsXPIDLString.h"

#include "nsIInputStream.h"

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
    uint16_t            GetEntryCount() const {return mHeader->num_interfaces;}
    
    void                SetEntryAt(uint16_t i, xptiInterfaceEntry* ptr)
    {
        NS_ASSERTION(mHeader,"bad state!");
        NS_ASSERTION(i < GetEntryCount(),"bad param!");
        mEntryArray[i] = ptr;
    }        

    xptiInterfaceEntry* GetEntryAt(uint16_t i);

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

/***************************************************************************/

// This class exists to help xptiInterfaceInfo store a 4-state (2 bit) value 
// and a set of bitflags in one 8bit value. See below.

class xptiInfoFlags
{
    enum {STATE_MASK = 3};
public:
    xptiInfoFlags(uint8_t n) : mData(n) {}
    xptiInfoFlags(const xptiInfoFlags& r) : mData(r.mData) {}

    static uint8_t GetStateMask()
        {return uint8_t(STATE_MASK);}
    
    void Clear()
        {mData = 0;}

    uint8_t GetData() const
        {return mData;}

    uint8_t GetState() const 
        {return mData & GetStateMask();}

    void SetState(uint8_t state) 
        {mData &= ~GetStateMask(); mData |= state;}                                   

    void SetFlagBit(uint8_t flag, bool on) 
        {if(on)
            mData |= ~GetStateMask() & flag;
         else
            mData &= GetStateMask() | ~flag;}

    bool GetFlagBit(uint8_t flag) const 
        {return (mData & flag) ? true : false;}

private:
    uint8_t mData;    
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
    enum {SCRIPTABLE = 4, BUILTINCLASS = 8};

    uint8_t GetResolveState() const {return mFlags.GetState();}
    
    bool IsFullyResolved() const 
        {return GetResolveState() == (uint8_t) FULLY_RESOLVED;}

    void   SetScriptableFlag(bool on)
                {mFlags.SetFlagBit(uint8_t(SCRIPTABLE),on);}
    bool GetScriptableFlag() const
                {return mFlags.GetFlagBit(uint8_t(SCRIPTABLE));}
    void   SetBuiltinClassFlag(bool on)
                {mFlags.SetFlagBit(uint8_t(BUILTINCLASS),on);}
    bool GetBuiltinClassFlag() const
                {return mFlags.GetFlagBit(uint8_t(BUILTINCLASS));}

    const nsID* GetTheIID()  const {return &mIID;}
    const char* GetTheName() const {return mName;}

    bool EnsureResolved()
        {return IsFullyResolved() ? true : Resolve();}

    already_AddRefed<xptiInterfaceInfo> InterfaceInfo();
    bool     InterfaceInfoEquals(const xptiInterfaceInfo* info) const 
        {return info == mInfo;}
    
    void     LockedInvalidateInterfaceInfo();
    void     LockedInterfaceInfoDeathNotification() {mInfo = nullptr;}

    xptiInterfaceEntry* Parent() const {
        NS_ASSERTION(IsFullyResolved(), "Parent() called while not resolved?");
        return mParent;
    }

    const nsID& IID() const { return mIID; }

    //////////////////////
    // These non-virtual methods handle the delegated nsIInterfaceInfo methods.

    nsresult GetName(char * *aName);
    nsresult GetIID(nsIID * *aIID);
    nsresult IsScriptable(bool *_retval);
    nsresult IsBuiltinClass(bool *_retval) {
        *_retval = GetBuiltinClassFlag();
        return NS_OK;
    }
    // Except this one.
    //nsresult GetParent(nsIInterfaceInfo * *aParent);
    nsresult GetMethodCount(uint16_t *aMethodCount);
    nsresult GetConstantCount(uint16_t *aConstantCount);
    nsresult GetMethodInfo(uint16_t index, const nsXPTMethodInfo * *info);
    nsresult GetMethodInfoForName(const char *methodName, uint16_t *index, const nsXPTMethodInfo * *info);
    nsresult GetConstant(uint16_t index, const nsXPTConstant * *constant);
    nsresult GetInfoForParam(uint16_t methodIndex, const nsXPTParamInfo * param, nsIInterfaceInfo **_retval);
    nsresult GetIIDForParam(uint16_t methodIndex, const nsXPTParamInfo * param, nsIID * *_retval);
    nsresult GetTypeForParam(uint16_t methodIndex, const nsXPTParamInfo * param, uint16_t dimension, nsXPTType *_retval);
    nsresult GetSizeIsArgNumberForParam(uint16_t methodIndex, const nsXPTParamInfo * param, uint16_t dimension, uint8_t *_retval);
    nsresult GetInterfaceIsArgNumberForParam(uint16_t methodIndex, const nsXPTParamInfo * param, uint8_t *_retval);
    nsresult IsIID(const nsIID * IID, bool *_retval);
    nsresult GetNameShared(const char **name);
    nsresult GetIIDShared(const nsIID * *iid);
    nsresult IsFunction(bool *_retval);
    nsresult HasAncestor(const nsIID * iid, bool *_retval);
    nsresult GetIIDForParamNoAlloc(uint16_t methodIndex, const nsXPTParamInfo * param, nsIID *iid);

private:
    xptiInterfaceEntry(const char* name,
                       size_t nameLength,
                       const nsID& iid,
                       XPTInterfaceDescriptor* aDescriptor,
                       xptiTypelibGuts* aTypelib);
    ~xptiInterfaceEntry();

    void SetResolvedState(int state) 
        {mFlags.SetState(uint8_t(state));}

    bool Resolve();

    // We only call these "*Locked" variants after locking. This is done to 
    // allow reentrace as files are loaded and various interfaces resolved 
    // without having to worry about the locked state.

    bool EnsureResolvedLocked()
        {return IsFullyResolved() ? true : ResolveLocked();}
    bool ResolveLocked();

    // private helpers

    nsresult GetEntryForParam(uint16_t methodIndex, 
                              const nsXPTParamInfo * param,
                              xptiInterfaceEntry** entry);

    nsresult GetTypeInArray(const nsXPTParamInfo* param,
                            uint16_t dimension,
                            const XPTTypeDescriptor** type);

private:
    nsID                    mIID;
    XPTInterfaceDescriptor* mDescriptor;

    uint16_t mMethodBaseIndex;
    uint16_t mConstantBaseIndex;
    xptiTypelibGuts* mTypelib;

    xptiInterfaceEntry*     mParent;      // Valid only when fully resolved

    xptiInterfaceInfo*      mInfo;        // May come and go.
    xptiInfoFlags           mFlags;
    char                    mName[1];     // Always last. Sized to fit.
};

class xptiInterfaceInfo MOZ_FINAL : public nsIInterfaceInfo
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS

    // Use delegation to implement (most!) of nsIInterfaceInfo.
    NS_IMETHOD GetName(char * *aName) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetName(aName); }
    NS_IMETHOD GetInterfaceIID(nsIID * *aIID) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetIID(aIID); }
    NS_IMETHOD IsScriptable(bool *_retval) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->IsScriptable(_retval); }
    NS_IMETHOD IsBuiltinClass(bool *_retval) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->IsBuiltinClass(_retval); }
    // Except this one.
    NS_IMETHOD GetParent(nsIInterfaceInfo * *aParent) 
    {
        if(!EnsureResolved() || !EnsureParent())
            return NS_ERROR_UNEXPECTED;
        NS_IF_ADDREF(*aParent = mParent);
        return NS_OK;
    }
    NS_IMETHOD GetMethodCount(uint16_t *aMethodCount) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetMethodCount(aMethodCount); }
    NS_IMETHOD GetConstantCount(uint16_t *aConstantCount) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetConstantCount(aConstantCount); }
    NS_IMETHOD GetMethodInfo(uint16_t index, const nsXPTMethodInfo * *info) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetMethodInfo(index, info); }
    NS_IMETHOD GetMethodInfoForName(const char *methodName, uint16_t *index, const nsXPTMethodInfo * *info) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetMethodInfoForName(methodName, index, info); }
    NS_IMETHOD GetConstant(uint16_t index, const nsXPTConstant * *constant) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetConstant(index, constant); }
    NS_IMETHOD GetInfoForParam(uint16_t methodIndex, const nsXPTParamInfo * param, nsIInterfaceInfo **_retval) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetInfoForParam(methodIndex, param, _retval); }
    NS_IMETHOD GetIIDForParam(uint16_t methodIndex, const nsXPTParamInfo * param, nsIID * *_retval) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetIIDForParam(methodIndex, param, _retval); }
    NS_IMETHOD GetTypeForParam(uint16_t methodIndex, const nsXPTParamInfo * param, uint16_t dimension, nsXPTType *_retval) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetTypeForParam(methodIndex, param, dimension, _retval); }
    NS_IMETHOD GetSizeIsArgNumberForParam(uint16_t methodIndex, const nsXPTParamInfo * param, uint16_t dimension, uint8_t *_retval) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetSizeIsArgNumberForParam(methodIndex, param, dimension, _retval); }
    NS_IMETHOD GetInterfaceIsArgNumberForParam(uint16_t methodIndex, const nsXPTParamInfo * param, uint8_t *_retval) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetInterfaceIsArgNumberForParam(methodIndex, param, _retval); }
    NS_IMETHOD IsIID(const nsIID * IID, bool *_retval) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->IsIID(IID, _retval); }
    NS_IMETHOD GetNameShared(const char **name) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetNameShared(name); }
    NS_IMETHOD GetIIDShared(const nsIID * *iid) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetIIDShared(iid); }
    NS_IMETHOD IsFunction(bool *_retval) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->IsFunction(_retval); }
    NS_IMETHOD HasAncestor(const nsIID * iid, bool *_retval) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->HasAncestor(iid, _retval); }
    NS_IMETHOD GetIIDForParamNoAlloc(uint16_t methodIndex, const nsXPTParamInfo * param, nsIID *iid) { return !mEntry ? NS_ERROR_UNEXPECTED : mEntry->GetIIDForParamNoAlloc(methodIndex, param, iid); }

public:
    xptiInterfaceInfo(xptiInterfaceEntry* entry);

    void Invalidate() 
        {NS_IF_RELEASE(mParent); mEntry = nullptr;}

private:

    ~xptiInterfaceInfo();

    // Note that mParent might still end up as nullptr if we don't have one.
    bool EnsureParent()
    {
        NS_ASSERTION(mEntry && mEntry->IsFullyResolved(), "bad EnsureParent call");
        return mParent || !mEntry->Parent() || BuildParent();
    }
    
    bool EnsureResolved()
    {
        return mEntry && mEntry->EnsureResolved();
    }

    bool BuildParent();

    xptiInterfaceInfo();  // not implemented

private:
    xptiInterfaceEntry* mEntry;
    xptiInterfaceInfo*  mParent;
};

/***************************************************************************/

#endif /* xptiprivate_h___ */
