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
 *   Mike McCabe <mccabe@netscape.com>
 *   John Bandhauer <jband@netscape.com>
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

/* Library-private header for Interface Info system. */

#ifndef xptiprivate_h___
#define xptiprivate_h___

#include "nscore.h"
#include "nsISupports.h"

// this after nsISupports, to pick up IID
// so that xpt stuff doesn't try to define it itself...
#include "xpt_struct.h"
#include "xpt_xdr.h"

#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "xptinfo.h"
#include "nsIXPTLoader.h"

#include "nsIServiceManager.h"
#include "nsILocalFile.h"
#include "nsIDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIWeakReference.h"

#include "nsCRT.h"
#include "nsMemory.h"

#include "nsISupportsArray.h"
#include "nsSupportsArray.h"
#include "nsInt64.h"

#include "nsQuickSort.h"

#include "nsXPIDLString.h"

#include "nsIInputStream.h"

#include "nsAutoLock.h"

#include "pldhash.h"
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
#define LOG_AUTOREG(x) xptiInterfaceInfoManager::WriteToLog x
#endif

#if 1 && defined(DEBUG_jband)
#define SHOW_INFO_COUNT_STATS
#endif

/***************************************************************************/

class xptiFile;
class xptiInterfaceInfo;
class xptiInterfaceInfoManager;
class xptiInterfaceEntry;
class xptiInterfaceGuts;
class xptiTypelibGuts;
class xptiWorkingSet;

/***************************************************************************/

class xptiTypelib
{
public:
    // No ctors or dtors so that we can be in a union in xptiInterfaceInfo.
    // Allow automatic shallow copies.

    uint16 GetFileIndex()    const {return mFileIndex;}
    uint16 GetZipItemIndex() const {return mZipItemIndex;}

    enum {NOT_ZIP = 0xffff};

    PRBool IsZip() const {return mZipItemIndex != NOT_ZIP;}

    void Init(uint16 aFileIndex, uint16 aZipItemIndex = NOT_ZIP)
        {mFileIndex = aFileIndex; mZipItemIndex = aZipItemIndex;}

    PRBool Equals(const xptiTypelib& r) const
        {return mFileIndex == r.mFileIndex && 
                mZipItemIndex == r.mZipItemIndex;}

private:
    uint16 mFileIndex;
    uint16 mZipItemIndex;
};

/***************************************************************************/

// No virtuals.
// These are always constructed in the struct arena using placement new.
// dtor need not be called.

class xptiTypelibGuts
{
public:
    static xptiTypelibGuts* NewGuts(XPTHeader* aHeader,
                                    xptiWorkingSet* aWorkingSet);

    XPTHeader*          GetHeader()           {return mHeader;}
    PRUint16            GetEntryCount() const {return mHeader->num_interfaces;}
    
    void                SetEntryAt(PRUint16 i, xptiInterfaceEntry* ptr)
    {
        NS_ASSERTION(mHeader,"bad state!");
        NS_ASSERTION(i < GetEntryCount(),"bad param!");
        mEntryArray[i] = ptr;
    }        

    xptiInterfaceEntry* GetEntryAt(PRUint16 i) const
    {
        NS_ASSERTION(mHeader,"bad state!");
        NS_ASSERTION(i < GetEntryCount(),"bad param!");
        return mEntryArray[i];
    }        

private:
    xptiTypelibGuts(); // not implemented
    xptiTypelibGuts(XPTHeader* aHeader);
    ~xptiTypelibGuts() {}
    void* operator new(size_t, void* p) CPP_THROW_NEW {return p;}

private:
    XPTHeader*           mHeader;        // hold pointer into arena
    xptiInterfaceEntry*  mEntryArray[1]; // Always last. Sized to fit.
};

/***************************************************************************/

class xptiFile
{
public:
    const nsInt64&          GetSize()      const {return mSize;}
    const nsInt64&          GetDate()      const {return mDate;}
    const char*             GetName()      const {return mName;}
    const PRUint32          GetDirectory() const {return mDirectory;}
          xptiTypelibGuts*  GetGuts()            {return mGuts;}

    xptiFile();

    xptiFile(const nsInt64&  aSize,
             const nsInt64&  aDate,
             PRUint32        aDirectory,
             const char*     aName,
             xptiWorkingSet* aWorkingSet);

    xptiFile(const xptiFile& r, xptiWorkingSet* aWorkingSet);

    ~xptiFile();

    PRBool SetHeader(XPTHeader* aHeader, xptiWorkingSet* aWorkingSet);

    PRBool Equals(const xptiFile& r) const
    {
        return  mDirectory == r.mDirectory &&
                mSize == r.mSize &&
                mDate == r.mDate &&
                0 == PL_strcmp(mName, r.mName);
    }

    xptiFile(const xptiFile& r) {CopyFields(r);}
    xptiFile& operator= (const xptiFile& r)
    {
        if(this != &r)
            CopyFields(r);
        return *this;
    }

private:
    void CopyFields(const xptiFile& r)
    {
#ifdef DEBUG
        // If 'this' has a workingset then it better match that of the assigner. 
        NS_ASSERTION(!mDEBUG_WorkingSet || 
                     mDEBUG_WorkingSet == r.mDEBUG_WorkingSet,
                     "illegal xptiFile assignment");
        mDEBUG_WorkingSet = r.mDEBUG_WorkingSet;
#endif

        mSize      = r.mSize;
        mDate      = r.mDate;
        mName      = r.mName;
        mDirectory = r.mDirectory;
        mGuts      = r.mGuts;
    }

private:
#ifdef DEBUG
    xptiWorkingSet*  mDEBUG_WorkingSet;
#endif
    nsInt64          mSize;
    nsInt64          mDate;
    const char*      mName; // hold pointer into arena from initializer
    xptiTypelibGuts* mGuts; // hold pointer into arena
    PRUint32         mDirectory;
};

/***************************************************************************/

class xptiZipItem
{
public:
    const char*             GetName() const {return mName;}
          xptiTypelibGuts*  GetGuts()       {return mGuts;}

    xptiZipItem();

    xptiZipItem(const char*     aName,
                xptiWorkingSet* aWorkingSet);

    xptiZipItem(const xptiZipItem& r, xptiWorkingSet* aWorkingSet);

    ~xptiZipItem();

    PRBool SetHeader(XPTHeader* aHeader, xptiWorkingSet* aWorkingSet);

    PRBool Equals(const xptiZipItem& r) const
    {
        return 0 == PL_strcmp(mName, r.mName);
    }

    xptiZipItem(const xptiZipItem& r) {CopyFields(r);}
    xptiZipItem& operator= (const xptiZipItem& r)
    {
        if(this != &r)
            CopyFields(r);
        return *this;
    }

private:
    void CopyFields(const xptiZipItem& r)
    {
#ifdef DEBUG
        // If 'this' has a workingset then it better match that of the assigner. 
        NS_ASSERTION(!mDEBUG_WorkingSet || 
                     mDEBUG_WorkingSet == r.mDEBUG_WorkingSet,
                     "illegal xptiFile assignment");
        mDEBUG_WorkingSet = r.mDEBUG_WorkingSet;
#endif

        mName = r.mName;
        mGuts = r.mGuts;
    }

private:
#ifdef DEBUG
    xptiWorkingSet*  mDEBUG_WorkingSet;
#endif
    const char*      mName; // hold pointer into arena from initializer
    xptiTypelibGuts* mGuts; // hold pointer into arena
};

/***************************************************************************/

class xptiWorkingSet
{
public:
    xptiWorkingSet(); // not implmented
    xptiWorkingSet(nsISupportsArray* aDirectories);
    ~xptiWorkingSet();
    
    PRBool IsValid() const;

    void InvalidateInterfaceInfos();
    void ClearHashTables();
    void ClearFiles();
    void ClearZipItems();

    // utility methods...

    xptiTypelibGuts* GetTypelibGuts(const xptiTypelib& typelib)
    { 
        return typelib.IsZip() ?
            GetZipItemAt(typelib.GetZipItemIndex()).GetGuts() :
            GetFileAt(typelib.GetFileIndex()).GetGuts();
    }
    
    enum {NOT_FOUND = 0xffffffff};

    // FileArray stuff...

    PRUint32  GetFileCount() const {return mFileCount;}
    PRUint32  GetFileFreeSpace()
        {return mFileArray ? mMaxFileCount - mFileCount : 0;} 
    
    PRUint32 FindFile(PRUint32 dir, const char* name);

    PRUint32 GetTypelibDirectoryIndex(const xptiTypelib& typelib)
    {
        return GetFileAt(typelib.GetFileIndex()).GetDirectory();
    }

    const char* GetTypelibFileName(const xptiTypelib& typelib)
    {
        return GetFileAt(typelib.GetFileIndex()).GetName();
    }

    xptiFile& GetFileAt(PRUint32 i) const 
    {
        NS_ASSERTION(mFileArray, "bad state!");
        NS_ASSERTION(i < mFileCount, "bad param!");
        return mFileArray[i];
    }

    void SetFileAt(PRUint32 i, const xptiFile& r)
    { 
        NS_ASSERTION(mFileArray, "bad state!");
        NS_ASSERTION(i < mFileCount, "bad param!");
        mFileArray[i] = r;
    }

    void AppendFile(const xptiFile& r)
    { 
        NS_ASSERTION(mFileArray, "bad state!");
        NS_ASSERTION(mFileCount < mMaxFileCount, "bad param!");
        mFileArray[mFileCount++] = r;
    }

    PRBool NewFileArray(PRUint32 count);
    PRBool ExtendFileArray(PRUint32 count);

    // ZipItemArray stuff...

    PRUint32  GetZipItemCount() const {return mZipItemCount;}
    PRUint32  GetZipItemFreeSpace()
        {return mZipItemArray ? mMaxZipItemCount - mZipItemCount : 0;} 

    PRUint32 FindZipItemWithName(const char* name);

    xptiZipItem& GetZipItemAt(PRUint32 i) const 
    {
        NS_ASSERTION(mZipItemArray, "bad state!");
        NS_ASSERTION(i < mZipItemCount, "bad param!");
        return mZipItemArray[i];
    }

    void SetZipItemAt(PRUint32 i, const xptiZipItem& r)
    { 
        NS_ASSERTION(mZipItemArray, "bad state!");
        NS_ASSERTION(i < mZipItemCount, "bad param!");
        mZipItemArray[i] = r;
    }

    void AppendZipItem(const xptiZipItem& r)
    { 
        NS_ASSERTION(mZipItemArray, "bad state!");
        NS_ASSERTION(mZipItemCount < mMaxZipItemCount, "bad param!");
        mZipItemArray[mZipItemCount++] = r;
    }

    PRBool NewZipItemArray(PRUint32 count);
    PRBool ExtendZipItemArray(PRUint32 count);

    // Directory stuff...

    PRUint32 GetDirectoryCount();
    nsresult GetCloneOfDirectoryAt(PRUint32 i, nsILocalFile** dir);
    nsresult GetDirectoryAt(PRUint32 i, nsILocalFile** dir);
    PRBool   FindDirectory(nsILocalFile* dir, PRUint32* index);
    PRBool   FindDirectoryOfFile(nsILocalFile* file, PRUint32* index);
    PRBool   DirectoryAtMatchesPersistentDescriptor(PRUint32 i, const char* desc);

    // Arena stuff...

    XPTArena*  GetStringArena() {return mStringArena;}
    XPTArena*  GetStructArena() {return mStructArena;}


private:
    PRUint32        mFileCount;
    PRUint32        mMaxFileCount;
    xptiFile*       mFileArray;         // using new[] and delete[]

    PRUint32        mZipItemCount;
    PRUint32        mMaxZipItemCount;
    xptiZipItem*    mZipItemArray;      // using new[] and delete[]

    XPTArena*       mStringArena;
    XPTArena*       mStructArena;

    nsCOMPtr<nsISupportsArray> mDirectories;

public:
    // XXX make these private with accessors
    PLDHashTable*   mNameTable;
    PLDHashTable*   mIIDTable;
    PRUint32*       mFileMergeOffsetMap;    // always in an arena
    PRUint32*       mZipItemMergeOffsetMap; // always in an arena
};

/***************************************************************************/

class xptiInterfaceGuts
{
public:
    uint16                      mMethodBaseIndex;
    uint16                      mConstantBaseIndex;
    xptiInterfaceEntry*         mParent;
    XPTInterfaceDescriptor*     mDescriptor;
    xptiTypelib                 mTypelib;
    xptiWorkingSet*             mWorkingSet;

    static xptiInterfaceGuts* NewGuts(XPTInterfaceDescriptor* aDescriptor,
                                      const xptiTypelib&      aTypelib,
                                      xptiWorkingSet*         aWorkingSet)
    {
        void* place = XPT_MALLOC(aWorkingSet->GetStructArena(),
                                 sizeof(xptiInterfaceGuts));
        if(!place)
            return nsnull;
        return new(place) xptiInterfaceGuts(aDescriptor, aTypelib, aWorkingSet);
    }

private:
    void* operator new(size_t, void* p) CPP_THROW_NEW {return p;}
    xptiInterfaceGuts(XPTInterfaceDescriptor* aDescriptor,
                      const xptiTypelib&      aTypelib,
                      xptiWorkingSet*         aWorkingSet)
        :   mMethodBaseIndex(0),
            mConstantBaseIndex(0),
            mParent(nsnull),
            mDescriptor(aDescriptor),
            mTypelib(aTypelib),
            mWorkingSet(aWorkingSet) {}

    ~xptiInterfaceGuts() {}
};

/***************************************************************************/

// This class exists to help xptiInterfaceInfo store a 4-state (2 bit) value 
// and a set of bitflags in one 8bit value. See below.

class xptiInfoFlags
{
    enum {STATE_MASK = 3};
public:
    xptiInfoFlags(uint8 n) : mData(n) {}
    xptiInfoFlags(const xptiInfoFlags& r) : mData(r.mData) {}

    static uint8 GetStateMask()
        {return uint8(STATE_MASK);}
    
    void Clear()
        {mData = 0;}

    uint8 GetData() const
        {return mData;}

    uint8 GetState() const 
        {return mData & GetStateMask();}

    void SetState(uint8 state) 
        {mData &= ~GetStateMask(); mData |= state;}                                   

    void SetFlagBit(uint8 flag, PRBool on) 
        {if(on)
            mData |= ~GetStateMask() & flag;
         else
            mData &= GetStateMask() | ~flag;}

    PRBool GetFlagBit(uint8 flag) const 
        {return (mData & flag) ? PR_TRUE : PR_FALSE;}

private:
    uint8 mData;    
};

/****************************************************/

// No virtual methods.
// We always create in the struct arena and construct using "placement new".
// No members need dtor calls.

class xptiInterfaceEntry
{
public:
    static xptiInterfaceEntry* NewEntry(const char* name,
                                        int nameLength,
                                        const nsID& iid,
                                        const xptiTypelib& typelib,
                                        xptiWorkingSet* aWorkingSet);

    static xptiInterfaceEntry* NewEntry(const xptiInterfaceEntry& r,
                                        const xptiTypelib& typelib,
                                        xptiWorkingSet* aWorkingSet);

    enum {
        NOT_RESOLVED          = 0,
        PARTIALLY_RESOLVED    = 1,
        FULLY_RESOLVED        = 2,
        RESOLVE_FAILED        = 3
    };
    
    // Additional bit flags...
    enum {SCRIPTABLE = 4};

    uint8 GetResolveState() const {return mFlags.GetState();}
    
    PRBool IsFullyResolved() const 
        {return GetResolveState() == (uint8) FULLY_RESOLVED;}

    PRBool HasInterfaceRecord() const
        {int s = (int) GetResolveState(); 
         return (s == PARTIALLY_RESOLVED || s == FULLY_RESOLVED) && mInterface;}

    const xptiTypelib&  GetTypelibRecord() const
        {return HasInterfaceRecord() ? mInterface->mTypelib : mTypelib;}

    xptiInterfaceGuts*  GetInterfaceGuts() const
        {return HasInterfaceRecord() ? mInterface : nsnull;}

#ifdef DEBUG
    PRBool DEBUG_ScriptableFlagIsValid() const
        {int s = (int) GetResolveState(); 
         if((s == PARTIALLY_RESOLVED || s == FULLY_RESOLVED) && mInterface)
            {
                if(XPT_ID_IS_SCRIPTABLE(mInterface->mDescriptor->flags))
                    return GetScriptableFlag();
                return !GetScriptableFlag();
            }
         return PR_TRUE;
        }
#endif

    void   SetScriptableFlag(PRBool on)
                {mFlags.SetFlagBit(uint8(SCRIPTABLE),on);}
    PRBool GetScriptableFlag() const
                {return mFlags.GetFlagBit(uint8(SCRIPTABLE));}

    const nsID* GetTheIID()  const {return &mIID;}
    const char* GetTheName() const {return mName;}

    PRBool EnsureResolved(xptiWorkingSet* aWorkingSet = nsnull)
        {return IsFullyResolved() ? PR_TRUE : Resolve(aWorkingSet);}

    PRBool PartiallyResolveLocked(XPTInterfaceDescriptor*  aDescriptor,
                                  xptiWorkingSet*          aWorkingSet);

    nsresult GetInterfaceInfo(xptiInterfaceInfo** info);
    PRBool   InterfaceInfoEquals(const xptiInterfaceInfo* info) const 
        {return info == mInfo;}
    
    void     LockedInvalidateInterfaceInfo();
    void     LockedInterfaceInfoDeathNotification() {mInfo = nsnull;}

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

    //////////////////////

private:
    xptiInterfaceEntry();   // not implemented

    xptiInterfaceEntry(const char* name,
                       size_t nameLength,
                       const nsID& iid,
                       const xptiTypelib& typelib);

    xptiInterfaceEntry(const xptiInterfaceEntry& r,
                       size_t nameLength,
                       const xptiTypelib& typelib);
    ~xptiInterfaceEntry();

    void* operator new(size_t, void* p) CPP_THROW_NEW {return p;}

    void SetResolvedState(int state) 
        {mFlags.SetState(uint8(state));}

    PRBool Resolve(xptiWorkingSet* aWorkingSet = nsnull);

    // We only call these "*Locked" variants after locking. This is done to 
    // allow reentrace as files are loaded and various interfaces resolved 
    // without having to worry about the locked state.

    PRBool EnsureResolvedLocked(xptiWorkingSet* aWorkingSet = nsnull)
        {return IsFullyResolved() ? PR_TRUE : ResolveLocked(aWorkingSet);}
    PRBool ResolveLocked(xptiWorkingSet* aWorkingSet = nsnull);

    // private helpers

    nsresult GetEntryForParam(PRUint16 methodIndex, 
                              const nsXPTParamInfo * param,
                              xptiInterfaceEntry** entry);

    nsresult GetTypeInArray(const nsXPTParamInfo* param,
                            uint16 dimension,
                            const XPTTypeDescriptor** type);

private:
    nsID                    mIID;
    union {
        xptiTypelib         mTypelib;     // Valid only until resolved.
        xptiInterfaceGuts*  mInterface;   // Valid only after resolved.
    };
    xptiInterfaceInfo*      mInfo;        // May come and go.
    xptiInfoFlags           mFlags;
    char                    mName[1];     // Always last. Sized to fit.
};

struct xptiHashEntry : public PLDHashEntryHdr
{
    xptiInterfaceEntry* value;    
};

/****************************************************/

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

#ifdef DEBUG
    static void DEBUG_ShutdownNotification();
#endif

private:

    ~xptiInterfaceInfo();

    // Note that mParent might still end up as nsnull if we don't have one.
    PRBool EnsureParent(xptiWorkingSet* aWorkingSet = nsnull)
    {
        NS_ASSERTION(mEntry && mEntry->IsFullyResolved(), "bad EnsureParent call");
        return mParent || !mEntry->GetInterfaceGuts()->mParent || BuildParent();
    }
    
    PRBool EnsureResolved(xptiWorkingSet* aWorkingSet = nsnull)
    {
        return mEntry && mEntry->EnsureResolved(aWorkingSet);
    }

    PRBool BuildParent()
    {
        NS_ASSERTION(mEntry && 
                     mEntry->IsFullyResolved() && 
                     !mParent &&
                     mEntry->GetInterfaceGuts()->mParent,
                    "bad BuildParent call");
        return NS_SUCCEEDED(mEntry->GetInterfaceGuts()->mParent->
                                        GetInterfaceInfo(&mParent));
    }

    xptiInterfaceInfo();  // not implemented

private:
    xptiInterfaceEntry* mEntry;
    xptiInterfaceInfo*  mParent;
};

/***************************************************************************/

class xptiManifest
{
public:
    static PRBool Read(xptiInterfaceInfoManager* aMgr,
                       xptiWorkingSet*           aWorkingSet);

    static PRBool Write(xptiInterfaceInfoManager* aMgr,
                        xptiWorkingSet*           aWorkingSet);

    static PRBool Delete(xptiInterfaceInfoManager* aMgr);

private:
    xptiManifest(); // no implementation
};

/***************************************************************************/

class xptiZipLoaderSink : public nsIXPTLoaderSink
{
public:
    xptiZipLoaderSink(xptiInterfaceInfoManager* aMgr,
                      xptiWorkingSet* aWorkingSet) :
        mManager(aMgr),
        mWorkingSet(aWorkingSet) {}
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPTLOADERSINK
    
private:
    ~xptiZipLoaderSink() {}

    xptiInterfaceInfoManager* mManager;
    xptiWorkingSet* mWorkingSet;

};

class xptiZipLoader
{
public:
    xptiZipLoader();  // not implemented

    static XPTHeader*
    ReadXPTFileFromInputStream(nsIInputStream *stream,
                               xptiWorkingSet* aWorkingSet);

};


/***************************************************************************/

class xptiFileType
{
public:
    enum Type {UNKNOWN = -1, XPT = 0, ZIP = 1 };

    static Type GetType(const char* name);

    static PRBool IsUnknown(const char* name)
        {return GetType(name) == UNKNOWN;}

    static PRBool IsXPT(const char* name)
        {return GetType(name) == XPT;}

    static PRBool IsZip(const char* name)
        {return GetType(name) == ZIP;}
private:
    xptiFileType(); // no implementation
};

/***************************************************************************/

// We use this is as a fancy way to open a logfile to be used within the scope
// of some given function where it is instantiated.
 
class xptiAutoLog
{
public:    
    xptiAutoLog();  // not implemented
    xptiAutoLog(xptiInterfaceInfoManager* mgr,
                nsILocalFile* logfile, PRBool append);
    ~xptiAutoLog();
private:
    void WriteTimestamp(PRFileDesc* fd, const char* msg);

    xptiInterfaceInfoManager* mMgr;
    PRFileDesc* mOldFileDesc;
#ifdef DEBUG
    PRFileDesc* m_DEBUG_FileDesc;
#endif
};

/***************************************************************************/

class xptiInterfaceInfoManager 
    : public nsIInterfaceInfoSuperManager
{
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINTERFACEINFOMANAGER
    NS_DECL_NSIINTERFACEINFOSUPERMANAGER

    // helper
    PRBool 
    FoundZipEntry(const char* entryName,
                  int index,
                  XPTHeader* header,
                  xptiWorkingSet* aWorkingSet);

public:
    static xptiInterfaceInfoManager* GetInterfaceInfoManagerNoAddRef();
    static void FreeInterfaceInfoManager();

    xptiWorkingSet*  GetWorkingSet() {return &mWorkingSet;}
    PRFileDesc*      GetOpenLogFile() {return mOpenLogFile;}
    PRFileDesc*      SetOpenLogFile(PRFileDesc* fd) 
        {PRFileDesc* temp = mOpenLogFile; mOpenLogFile = fd; return temp;}

    PRBool LoadFile(const xptiTypelib& aTypelibRecord,
                    xptiWorkingSet* aWorkingSet = nsnull);

    PRBool GetApplicationDir(nsILocalFile** aDir);
    PRBool GetCloneOfManifestLocation(nsILocalFile** aDir);

    void   GetSearchPath(nsISupportsArray** aSearchPath)
        {NS_ADDREF(*aSearchPath = mSearchPath);}

    static PRLock* GetResolveLock(xptiInterfaceInfoManager* self = nsnull) 
        {if(!self && !(self = GetInterfaceInfoManagerNoAddRef())) 
            return nsnull;
         return self->mResolveLock;}

    static PRLock* GetAutoRegLock(xptiInterfaceInfoManager* self = nsnull) 
        {if(!self && !(self = GetInterfaceInfoManagerNoAddRef())) 
            return nsnull;
         return self->mAutoRegLock;}

    static PRMonitor* GetInfoMonitor(xptiInterfaceInfoManager* self = nsnull) 
        {if(!self && !(self = GetInterfaceInfoManagerNoAddRef())) 
            return nsnull;
         return self->mInfoMonitor;}

    static void WriteToLog(const char *fmt, ...);

private:
    ~xptiInterfaceInfoManager();
    xptiInterfaceInfoManager(); // not implmented
    xptiInterfaceInfoManager(nsISupportsArray* aSearchPath);

    enum AutoRegMode {
        NO_FILES_CHANGED = 0,
        FILES_ADDED_ONLY,
        FULL_VALIDATION_REQUIRED
    };

    PRBool IsValid();

    PRBool BuildFileList(nsISupportsArray* aSearchPath,
                         nsISupportsArray** aFileList);

    nsILocalFile** BuildOrderedFileArray(nsISupportsArray* aSearchPath,
                                         nsISupportsArray* aFileList,
                                         xptiWorkingSet* aWorkingSet);

    XPTHeader* ReadXPTFile(nsILocalFile* aFile, xptiWorkingSet* aWorkingSet);
    
    AutoRegMode DetermineAutoRegStrategy(nsISupportsArray* aSearchPath,
                                         nsISupportsArray* aFileList,
                                         xptiWorkingSet* aWorkingSet);

    PRBool AddOnlyNewFilesFromFileList(nsISupportsArray* aSearchPath,
                                       nsISupportsArray* aFileList,
                                       xptiWorkingSet* aWorkingSet);

    PRBool DoFullValidationMergeFromFileList(nsISupportsArray* aSearchPath,
                                             nsISupportsArray* aFileList,
                                             xptiWorkingSet* aWorkingSet);

    PRBool VerifyAndAddEntryIfNew(xptiWorkingSet* aWorkingSet,
                                  XPTInterfaceDirectoryEntry* iface,
                                  const xptiTypelib& typelibRecord,
                                  xptiInterfaceEntry** entryAdded);

    PRBool MergeWorkingSets(xptiWorkingSet* aDestWorkingSet,
                            xptiWorkingSet* aSrcWorkingSet);

    void   LogStats(); 

    PRBool DEBUG_DumpFileList(nsISupportsArray* aFileList);
    PRBool DEBUG_DumpFileArray(nsILocalFile** aFileArray, PRUint32 count);
    PRBool DEBUG_DumpFileListInWorkingSet(xptiWorkingSet* aWorkingSet);

    static PRBool BuildFileSearchPath(nsISupportsArray** aPath);

private:
    xptiWorkingSet               mWorkingSet;
    nsCOMPtr<nsILocalFile>       mStatsLogFile;
    nsCOMPtr<nsILocalFile>       mAutoRegLogFile;
    PRFileDesc*                  mOpenLogFile;
    PRLock*                      mResolveLock;
    PRLock*                      mAutoRegLock;
    PRMonitor*                   mInfoMonitor;
    PRLock*                      mAdditionalManagersLock;
    nsSupportsArray              mAdditionalManagers;
    nsCOMPtr<nsISupportsArray>   mSearchPath;
};

/***************************************************************************/
// utilities...

nsresult xptiCloneLocalFile(nsILocalFile*  aLocalFile,
                            nsILocalFile** aCloneLocalFile);

nsresult xptiCloneElementAsLocalFile(nsISupportsArray* aArray, PRUint32 aIndex,
                                     nsILocalFile** aLocalFile);

#endif /* xptiprivate_h___ */
