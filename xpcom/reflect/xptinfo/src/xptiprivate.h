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

/* Library-private header for Interface Info system. */

#ifndef xptiprivate_h___
#define xptiprivate_h___

#ifndef XPCOM_STANDALONE
#define XPTI_HAS_ZIP_SUPPORT 1
#endif /* XPCOM_STANDALONE */

#include "nscore.h"
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
#include "nsILocalFile.h"
#include "nsIDirectoryService.h"
#include "nsDirectoryServiceDefs.h"

#include "nsCRT.h"
#include "nsMemory.h"

#include "nsISupportsArray.h"
#include "nsInt64.h"

#include "nsQuickSort.h"

#ifdef XPTI_HAS_ZIP_SUPPORT
#include "nsIZipReader.h"
#endif /* XPTI_HAS_ZIP_SUPPORT */

#include "nsIInputStream.h"

#include "nsAutoLock.h"

#include "plhash.h"
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

/***************************************************************************/

class xptiFile;
class xptiInterfaceInfo;
class xptiInterfaceInfoManager;
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

class xptiTypelibGuts
{
public:
    xptiTypelibGuts(); // not implemented
    xptiTypelibGuts(XPTHeader* aHeader);
    ~xptiTypelibGuts();

    PRBool      IsValid() const      {return mHeader != nsnull;}
    XPTHeader*  GetHeader()          {return mHeader;}
    PRUint16    GetInfoCount() const {return mHeader->num_interfaces;}
    nsresult    SetInfoAt(PRUint16 i, xptiInterfaceInfo* ptr);
    nsresult    GetInfoAt(PRUint16 i, xptiInterfaceInfo** ptr);
    xptiInterfaceInfo* GetInfoAtNoAddRef(PRUint16 i);
    xptiTypelibGuts* Clone();

private:
    XPTHeader*          mHeader;    // hold pointer into arena
    xptiInterfaceInfo** mInfoArray;
};

/***************************************************************************/

class xptiFile
{
public:
    const nsInt64&          GetSize() const {return mSize;}
    const nsInt64&          GetDate() const {return mDate;}
    const char*             GetName() const {return mName;}
          xptiTypelibGuts*  GetGuts()       {return mGuts;}

    xptiFile();

    xptiFile(const nsInt64&  aSize,
             const nsInt64&  aDate,
             const char*     aName,
             xptiWorkingSet* aWorkingSet,
             XPTHeader*      aHeader = nsnull);

    xptiFile(const xptiFile& r, xptiWorkingSet* aWorkingSet, 
             PRBool cloneGuts);

    ~xptiFile();

    PRBool SetHeader(XPTHeader* aHeader);

    PRBool Equals(const xptiFile& r) const
    {
        return  mSize == r.mSize &&
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
        mSize = r.mSize;
        mDate = r.mDate;
        mName = r.mName;
        if(mGuts)
            delete mGuts;
        if(r.mGuts)
            mGuts = r.mGuts->Clone();
        else
            mGuts = nsnull;
    }

private:
    nsInt64          mSize;
    nsInt64          mDate;
    const char*      mName; // hold pointer into arena from initializer
    xptiTypelibGuts* mGuts; // new/delete
};

/***************************************************************************/

class xptiZipItem
{
public:
    const char*             GetName() const {return mName;}
          xptiTypelibGuts*  GetGuts()       {return mGuts;}

    xptiZipItem();

    xptiZipItem(const char*     aName,
                xptiWorkingSet* aWorkingSet,
                XPTHeader*      aHeader = nsnull);

    xptiZipItem(const xptiZipItem& r, xptiWorkingSet* aWorkingSet, 
                PRBool cloneGuts);

    ~xptiZipItem();

    PRBool SetHeader(XPTHeader* aHeader);

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
        mName = r.mName;
        if(mGuts)
            delete mGuts;
        if(r.mGuts)
            mGuts = r.mGuts->Clone();
        else
            mGuts = nsnull;
    }

private:
    const char*      mName; // hold pointer into arena from initializer
    xptiTypelibGuts* mGuts; // new/delete
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
    
    PRUint32 FindFileWithName(const char* name);

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

public:
    // XXX make these private with accessors
    PLHashTable*    mNameTable;
    PLHashTable*    mIIDTable;
    PRUint32*       mFileMergeOffsetMap;    // always in an arena
    PRUint32*       mZipItemMergeOffsetMap; // always in an arena
};

/***************************************************************************/

// XXX could move this class to xptiInterfaceInfo.cpp.

class xptiInterfaceGuts
{
public:

    uint16                      mMethodBaseIndex;
    uint16                      mConstantBaseIndex;
    xptiInterfaceInfo*          mParent;
    XPTInterfaceDescriptor*     mDescriptor;
    xptiTypelib                 mTypelib;
    xptiWorkingSet*             mWorkingSet;

    xptiInterfaceGuts(XPTInterfaceDescriptor* aDescriptor,
                      const xptiTypelib&      aTypelib,
                      xptiWorkingSet*         aWorkingSet)
        :   mMethodBaseIndex(0),
            mConstantBaseIndex(0),
            mParent(nsnull),
            mDescriptor(aDescriptor),
            mTypelib(aTypelib),
            mWorkingSet(aWorkingSet) {}

    // method definition below to avoid use before declaring xptiInterfaceInfo
    inline ~xptiInterfaceGuts();
};

/***************************************************************************/

// This class exists to help xptiInterfaceInfo store a 4-state (2 bit) value 
// and a set of bitflags in one 8bit value. See below.

class xptiInfoFlags
{
    enum {STATE_MASK = 3};
public:
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

class xptiInterfaceInfo : public nsIInterfaceInfo
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINTERFACEINFO

public:
    xptiInterfaceInfo();  // not implemented

    xptiInterfaceInfo(const char* name,
                      const nsID& iid,
                      const xptiTypelib& typelib,
                      xptiWorkingSet* aWorkingSet);

    xptiInterfaceInfo(const xptiInterfaceInfo& r,
                      const xptiTypelib& typelib,
                      xptiWorkingSet* aWorkingSet);

    virtual ~xptiInterfaceInfo();

    // We use mName[-1] (cast as a xptiInfoFlags) to hold the two bit state
    // below and also the bit flags that follow. If the states ever grow beyond 
    // 2 bits then these flags need to be adjusted along with STATE_MASK in
    // xptiInfoFlags.

    enum {
        NOT_RESOLVED          = 0,
        PARTIALLY_RESOLVED    = 1,
        FULLY_RESOLVED        = 2,
        RESOLVE_FAILED        = 3
    };
    
    // Additional bit flags...
    enum {SCRIPTABLE = 4};

    PRBool IsValid() const 
        {return mName != nsnull;}

    uint8 GetResolveState() const 
        {return IsValid() ? GetFlags().GetState() : (uint8) RESOLVE_FAILED;}
    
    PRBool IsFullyResolved() const 
        {return GetResolveState() == (uint8) FULLY_RESOLVED;}

    PRBool HasInterfaceRecord() const
        {int s = (int) GetResolveState(); 
         return (s == PARTIALLY_RESOLVED || s == FULLY_RESOLVED) && mInterface;}

    const xptiTypelib&  GetTypelibRecord() const
        {return HasInterfaceRecord() ? mInterface->mTypelib : mTypelib;}

    void   SetScriptableFlag(PRBool on)
                {GetFlags().SetFlagBit(uint8(SCRIPTABLE),on);}
    PRBool GetScriptableFlag() const
                {return GetFlags().GetFlagBit(uint8(SCRIPTABLE));}

    const nsID* GetTheIID() const {return &mIID;}
    const char* GetTheName() const {return mName;}

    PRBool PartiallyResolveLocked(XPTInterfaceDescriptor*  aDescriptor,
                                  xptiWorkingSet*          aWorkingSet);

    void Invalidate();

private:
    void CopyName(const char* name,
                  xptiWorkingSet* aWorkingSet);

    xptiInfoFlags& GetFlags() const {return (xptiInfoFlags&) mName[-1];}    
    xptiInfoFlags& GetFlags()       {return (xptiInfoFlags&) mName[-1];}    

    void SetResolvedState(int state) 
        {NS_ASSERTION(IsValid(),"bad state");
         GetFlags().SetState(uint8(state));}

    PRBool EnsureResolved(xptiWorkingSet* aWorkingSet = nsnull)
        {return IsFullyResolved() ? PR_TRUE : Resolve(aWorkingSet);}
    PRBool Resolve(xptiWorkingSet* aWorkingSet = nsnull);

    // We only call these "*Locked" varients after locking. This is done to 
    // allow reentrace as files are loaded and various interfaces resolved 
    // without having to worry about the locked state.

    PRBool EnsureResolvedLocked(xptiWorkingSet* aWorkingSet = nsnull)
        {return IsFullyResolved() ? PR_TRUE : ResolveLocked(aWorkingSet);}
    PRBool ResolveLocked(xptiWorkingSet* aWorkingSet = nsnull);

    PRBool ScriptableFlagIsValid() const
        {int s = (int) GetResolveState(); 
         if((s == PARTIALLY_RESOLVED || s == FULLY_RESOLVED) && mInterface)
            {
                if(XPT_ID_IS_SCRIPTABLE(mInterface->mDescriptor->flags))
                    return GetScriptableFlag();
                return !GetScriptableFlag();
            }
         else return PR_TRUE;
        }
    
    NS_IMETHOD GetTypeInArray(const nsXPTParamInfo* param,
                              uint16 dimension,
                              const XPTTypeDescriptor** type);
private:
    nsID   mIID;
    char*  mName;   // This is in arena (not free'd) and mName[-1] is a flag.

    union {
        xptiTypelib         mTypelib;     // Valid only until resolved.
        xptiInterfaceGuts*  mInterface;   // Valid only after resolved.
    };
};

/***************************************************************************/

class xptiManifest
{
public:
    static PRBool Read(xptiInterfaceInfoManager* aMgr,
                       xptiWorkingSet*           aWorkingSet);

    static PRBool Write(xptiInterfaceInfoManager* aMgr,
                        xptiWorkingSet*           aWorkingSet);

    xptiManifest(); // no implementation
};

/***************************************************************************/

class xptiEntrySink
{
public:

    virtual PRBool 
    FoundEntry(const char* entryName,
               int index,
               XPTHeader* header,
               xptiWorkingSet* aWorkingSet) = 0;
};

#ifdef XPTI_HAS_ZIP_SUPPORT

class xptiZipLoader
{
public:
    xptiZipLoader();  // not implemented

    static PRBool 
    EnumerateZipEntries(nsILocalFile* file,
                        xptiEntrySink* sink,
                        xptiWorkingSet* aWorkingSet);

    static XPTHeader* 
    ReadXPTFileFromZip(nsILocalFile* file,
                       const char* entryName,
                       xptiWorkingSet* aWorkingSet);

    static void
    Shutdown();

private:    
    static XPTHeader* 
    ReadXPTFileFromOpenZip(nsIZipReader* zip,
                           nsIZipEntry* entry,
                           const char* entryName,
                           xptiWorkingSet* aWorkingSet);

    static nsIZipReader*
    GetZipReader(nsILocalFile* file);

    static nsCOMPtr<nsIZipReaderCache> gCache;
};

#endif /* XPTI_HAS_ZIP_SUPPORT */

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
    : public nsIInterfaceInfoManager,
      public xptiEntrySink
{
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINTERFACEINFOMANAGER

    // implement xptiEntrySink
    PRBool 
    FoundEntry(const char* entryName,
               int index,
               XPTHeader* header,
               xptiWorkingSet* aWorkingSet);

public:
    virtual ~xptiInterfaceInfoManager();
    static xptiInterfaceInfoManager* GetInterfaceInfoManagerNoAddRef();
    static void FreeInterfaceInfoManager();

    xptiWorkingSet*  GetWorkingSet() {return &mWorkingSet;}
    PRFileDesc*      GetOpenLogFile() {return mOpenLogFile;}
    PRFileDesc*      SetOpenLogFile(PRFileDesc* fd) 
        {PRFileDesc* temp = mOpenLogFile; mOpenLogFile = fd; return temp;}

    PRBool LoadFile(const xptiTypelib& aTypelibRecord,
                    xptiWorkingSet* aWorkingSet = nsnull);

    PRBool GetComponentsDir(nsILocalFile** aDir);
    PRBool GetManifestDir(nsILocalFile** aDir);
    
    static PRLock* GetResolveLock(xptiInterfaceInfoManager* self = nsnull) 
        {if(!self && !(self = GetInterfaceInfoManagerNoAddRef())) 
            return nsnull;
         return self->mResolveLock;}

    static PRLock* GetAutoRegLock(xptiInterfaceInfoManager* self = nsnull) 
        {if(!self && !(self = GetInterfaceInfoManagerNoAddRef())) 
            return nsnull;
         return self->mAutoRegLock;}

    static void WriteToLog(const char *fmt, ...);

private:
    xptiInterfaceInfoManager();

    enum AutoRegMode {
        NO_FILES_CHANGED = 0,
        FILES_ADDED_ONLY,
        FULL_VALIDATION_REQUIRED
    };

    PRBool IsValid();

    PRBool BuildFileList(nsISupportsArray** aFileList);

    nsILocalFile** BuildOrderedFileArray(nsISupportsArray* aFileList,
                                         xptiWorkingSet* aWorkingSet);

    XPTHeader* ReadXPTFile(nsILocalFile* aFile, xptiWorkingSet* aWorkingSet);
    
    AutoRegMode DetermineAutoRegStrategy(nsISupportsArray* aFileList,
                                         xptiWorkingSet* aWorkingSet);

    PRBool AddOnlyNewFileFromFileList(nsISupportsArray* aFileList,
                                      xptiWorkingSet* aWorkingSet);

    PRBool DoFullValidationMergeFromFileList(nsISupportsArray* aFileList,
                                             xptiWorkingSet* aWorkingSet);

    PRBool VerifyAndAddInterfaceIfNew(xptiWorkingSet* aWorkingSet,
                                      XPTInterfaceDirectoryEntry* iface,
                                      const xptiTypelib& typelibRecord,
                                      xptiInterfaceInfo** infoAdded);

    PRBool MergeWorkingSets(xptiWorkingSet* aDestWorkingSet,
                            xptiWorkingSet* aSrcWorkingSet);

    void   LogStats(); 

    PRBool DEBUG_DumpFileList(nsISupportsArray* aFileList);
    PRBool DEBUG_DumpFileArray(nsILocalFile** aFileArray, PRUint32 count);
    PRBool DEBUG_DumpFileListInWorkingSet(xptiWorkingSet* aWorkingSet);

private:
    xptiWorkingSet          mWorkingSet;
    nsCOMPtr<nsILocalFile>  mStatsLogFile;
    nsCOMPtr<nsILocalFile>  mAutoRegLogFile;
    PRFileDesc*             mOpenLogFile;
    PRLock*                 mResolveLock;
    PRLock*                 mAutoRegLock;
};

/***************************************************************************/
//inlines...

inline xptiInterfaceGuts::~xptiInterfaceGuts() {NS_IF_RELEASE(mParent);}


#endif /* xptiprivate_h___ */
