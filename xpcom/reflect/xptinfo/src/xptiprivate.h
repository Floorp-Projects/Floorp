/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 */

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

#include "nsIServiceManager.h"
#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsSpecialSystemDirectory.h"

#include "nsISupportsArray.h"
#include "nsInt64.h"

#include "nsQuickSort.h"
#include "nsIZipReader.h"
#include "nsIInputStream.h"

#include "plstr.h"
#include "prprf.h"

#include <stdio.h>
#include <stdarg.h>

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
    
    // FileArray stuff...

    PRUint32  GetFileCount() const {return mFileCount;}
    PRUint32  GetFileFreeSpace()
        {return mFileArray ? mMaxFileCount - mFileCount : 0;} 

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
    PRUint32*       mMergeOffsetMap;    // always in an arena
};

/***************************************************************************/

// XXX could move this class to xptiInterfaceInfo.cpp.

class xptiInterfaceGuts
{
public:

    uint16                      mMethodBaseIndex;
    uint16                      mConstantBaseIndex;
    nsIInterfaceInfo*           mParent;
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

    ~xptiInterfaceGuts() {NS_IF_RELEASE(mParent);}
};

/***************************************************************************/

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
                      xptiWorkingSet& aWorkingSet);

    virtual ~xptiInterfaceInfo();

    enum {
        NOT_RESOLVED          = 0,
        PARTIALLY_RESOLVED    = 1,
        FULLY_RESOLVED        = 2,
        RESOLVE_FAILED        = 3
    };

    PRBool IsValid() const 
        {return mName != nsnull;}
    
    char GetResolveState() const 
        {return IsValid() ? mName[-1] : (char) RESOLVE_FAILED;}
    
    PRBool IsFullyResolved() const 
        {return GetResolveState() == (char) FULLY_RESOLVED;}

    PRBool HasInterfaceRecord() const
        {int s = (int) GetResolveState(); 
         return 
            (s == PARTIALLY_RESOLVED || s == FULLY_RESOLVED) && mInterface;
        }

    const xptiTypelib&  GetTypelibRecord() const
        {return HasInterfaceRecord() ? mInterface->mTypelib : mTypelib;}

    const nsID* GetTheIID() const {return &mIID;}
    const char* GetTheName() const {return mName;}

    PRBool PartiallyResolve(XPTInterfaceDescriptor*  aDescriptor,
                            xptiWorkingSet*          aWorkingSet);

    void Invalidate()
        { 
            if(IsValid())
            {
                xptiTypelib typelib = GetTypelibRecord();
                mTypelib = typelib;
                mName = nsnull;
            }
        }

private:
    void SetResolvedState(int state) 
        {NS_ASSERTION(IsValid(),"bad state"); mName[-1] = (char) state;}

    PRBool Resolve(xptiWorkingSet* aWorkingSet = nsnull);

    PRBool EnsureResolved(xptiWorkingSet* aWorkingSet = nsnull)
        {return IsFullyResolved() ? PR_TRUE : Resolve(aWorkingSet);}

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
                       xptiWorkingSet&           aWorkingSet);

    static PRBool Write(xptiInterfaceInfoManager* aMgr,
                        xptiWorkingSet&           aWorkingSet);

    xptiManifest(); // no implementation
};

/***************************************************************************/

class xptiInterfaceInfoManager : public nsIInterfaceInfoManager
{
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINTERFACEINFOMANAGER

public:
    virtual ~xptiInterfaceInfoManager();
    static xptiInterfaceInfoManager* GetInterfaceInfoManager();
    static void FreeInterfaceInfoManager();

    xptiWorkingSet*  GetWorkingSet() {return &mWorkingSet;}

    PRBool LoadFile(const xptiTypelib& aTypelibRecord,
                    xptiWorkingSet* aWorkingSet = nsnull);

    PRBool GetComponentsDir(nsILocalFile** aDir);

private:
    xptiInterfaceInfoManager();

    enum AutoRegMode {
        NO_FILES_CHANGED = 0,
        MAYBE_ONLY_ADDITIONS,
        FULL_VALIDATION_REQUIRED
    };

    PRBool BuildFileList(nsISupportsArray** aFileList);

    PRBool PopulateWorkingSetFromManifest(xptiWorkingSet& aWorkingSet);
    PRBool WriteManifest(xptiWorkingSet& aWorkingSet);

    XPTHeader* ReadXPTFile(nsILocalFile* aFile, xptiWorkingSet& aWorkingSet);

    XPTHeader* ReadXPTFileFromZip(nsILocalFile* file,
                                  const char* itemName,
                                  xptiWorkingSet& aWorkingSet);

    XPTHeader* ReadXPTFileFromOpenZip(nsIZipReader * zip,
                                      nsIZipEntry* entry,
                                      const char* itemName,
                                      xptiWorkingSet& aWorkingSet);

    AutoRegMode DetermineAutoRegStrategy(nsISupportsArray* aFileList,
                                         xptiWorkingSet& aWorkingSet);

    PRBool AttemptAddOnlyFromFileList(nsISupportsArray* aFileList,
                                      xptiWorkingSet& aWorkingSet,
                                      AutoRegMode* pmode);

    PRBool DoFullValidationMergeFromFileList(nsISupportsArray* aFileList,
                                             xptiWorkingSet& aWorkingSet);

    PRBool VerifyAndAddInterfaceIfNew(xptiWorkingSet& aWorkingSet,
                                      XPTInterfaceDirectoryEntry* iface,
                                      const xptiTypelib& typelibRecord,
                                      xptiInterfaceInfo** infoAdded);

    PRBool MergeWorkingSets(xptiWorkingSet& aDestWorkingSet,
                            xptiWorkingSet& aSrcWorkingSet);

    PRBool DEBUG_DumpFileList(nsISupportsArray* aFileList);
    PRBool DEBUG_DumpFileListInWorkingSet(xptiWorkingSet& aWorkingSet);

private:
    xptiWorkingSet          mWorkingSet;
};

#if 0



#endif


#endif /* xptiprivate_h___ */
