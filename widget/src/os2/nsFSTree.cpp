/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John 
 * Fairhurst.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All
 * Rights Reserved.
 *
 * Contributor(s):  John Fairhurst <john_fairhurst@iname.com>
 *
 */

//
// FSTree.cpp - asynchronous directory scanner
//
// mjf Sunday 06 June 1999
//
// TODO 
//    * (maybe) Abstract away filing system from scanner ('datasource' idea)
//
//    * Break out Dos* functions to some central useful place
//
//    * Add more fine-grained synchronous-ness (see DirPicker.cpp)
//

#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_DOSDEVIOCTL
#define INCL_WIN
#include <os2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
// disable asserts
#define NDEBUG    
#include <assert.h>
#include "nsFSTree.h"

#ifdef DEBUG
# define TRACE(x) printf x
#else
# define TRACE(x)
#endif

// helper Dos* style functions
APIRET DosQueryNumDrives( PULONG cDrives);
APIRET DosIsDriveRemovable( USHORT drive, PBOOL rem);
APIRET DosIsDriveAvailable( USHORT dirve, PBOOL yesno);

// Simple pipe-based queue class, templated to make some attempt at type-safety
template<class T>
class Queue
{
   HFILE mRead;
   HFILE mWrite;

 public:
   Queue() : mRead(0), mWrite(0) {}
  ~Queue() { Close(); }

   void Create()
   {
      DosCreatePipe( &mRead, &mWrite, 1024);
   }

   APIRET Write( T *aThing)
   {
      ULONG ulActual = 0;
      APIRET rc = DosWrite( mWrite, aThing, sizeof(T), &ulActual);
      assert( rc || ulActual == sizeof(T));
      return rc;
   }

   APIRET Read( T *aThing)
   {
      ULONG ulActual = 0;
      APIRET rc = DosRead( mRead, aThing, sizeof(T), &ulActual);
      assert( rc || ulActual == sizeof(T));
      return rc;
   }

   void Close()
   {
      DosClose( mRead);
      DosClose( mWrite);
   }
};

class fsFile;
class fsDir;

// messages sent from client to scanning thread
struct RQPacket
{
   enum Cmd
   {
      Expand,
      Trash,
      Foliate,
      Defoliate
   };

   Cmd    mCmd;
   fsDir *mDir;

   RQPacket( fsDir *aDir, RQPacket::Cmd aCmd) : mCmd(aCmd), mDir(aDir) {}
   RQPacket() {}
};

// messages sent from scanner to callback thread
struct CBPacket
{
   enum Cmd
   {
      CreateRoot,
      CreateNode,
      DestroyNode,
      CreateLeaf,
      DestroyLeaf,
      ScanComplete
   };

   Cmd mCmd;
   union
   {
      fsDir  *mDir;
      fsFile *mFile;
   } mData;

   CBPacket( fsDir *aDir, CBPacket::Cmd aCmd) : mCmd(aCmd)
   { mData.mDir = aDir; }
   CBPacket( fsFile *aFile, CBPacket::Cmd aCmd) : mCmd(aCmd)
   { mData.mFile = aFile; }
   CBPacket() : mCmd(CBPacket::ScanComplete)
   {}
};

// fsTree, implementation of main treebuilder class
class fsTree : public FS::ITreeBuilder
{
   FS::ICallbacks *mCallbacks;
   FS::BuildMode   mBuildMode;
   Queue<RQPacket> mRQQueue;
   Queue<CBPacket> mCBQueue;
   fsDir          *mRoot;
   HEV             mHEV;
   BOOL            mDestructing;
   HMTX            mMutex;
   BOOL            mNeedsPM;

   void SendMsg( fsDir *aDir, RQPacket::Cmd aCmd);
   void SendMsg( fsDir *aDir, CBPacket::Cmd aCmd);
   void SendMsg( fsFile *aFile, CBPacket::Cmd aCmd);
   void SendCompleteMsg();
   void Scan();
   void Notify();
   void CreateRootNode();
   void CreateDrives();
   void ScanForChildren( fsDir *aDir);
   void DestroyNode( fsDir *aDir);
   void Defoliate( fsDir *aDir);
   void Foliate( fsDir *aDir);

   friend void _Optlink scannerThread( void *arg)
   { ((fsTree*)arg)->Scan(); }

   friend void _Optlink notifyThread( void *arg)
   { ((fsTree*)arg)->Notify(); }

 public:
   fsTree() : mCallbacks(0), mRoot(0), mDestructing(0), mMutex(0), mNeedsPM(0) {}
   virtual ~fsTree();

   // ITreeBuilder
   BOOL Init( FS::ICallbacks *aCallbacks);
   BOOL RequiresPM( BOOL bSet) { mNeedsPM = bSet; return TRUE; }
   BOOL Build( FS::BuildMode aBuildMode);
   BOOL Lock();
   BOOL Unlock();
   BOOL DeleteInstance();

   // Called by fsDirs in response to user requests
   void ExpandNode( fsDir *aDir)
      { SendMsg( aDir, RQPacket::Expand); }
   void TrashNode( fsDir *aDir)
      { SendMsg( aDir, RQPacket::Trash); }
   void FoliateNode( fsDir *aDir)
      { SendMsg( aDir, RQPacket::Foliate); }
   void DefoliateNode( fsDir *aDir)
      { SendMsg( aDir, RQPacket::Defoliate); }
};


// Base fs entity class
class fsBase : public FS::IFileInfo
{
   void         *mUserData;
   FILEFINDBUF3  mFindBuf;
   PSZ           mPathname;

 protected:
   FS::IDir     *mParent;

 public:
   fsBase( FILEFINDBUF3 *aBuf, FS::IDir *aParent) : mUserData(0), mPathname(0),
                                                    mParent(aParent)
   {
      assert(aBuf);
      memcpy( &mFindBuf, aBuf, sizeof mFindBuf);
   }

   virtual ~fsBase()
   {
      delete [] mPathname;
   }

   PCSZ   GetLeafName()             { return mFindBuf.achName; }
   ULONG  GetAttributes()           { return mFindBuf.attrFile; }
   ULONG  GetSize()                 { return mFindBuf.cbFile; }
   FDATE  GetCreationDate()         { return mFindBuf.fdateCreation; }
   FTIME  GetCreationTime()         { return mFindBuf.ftimeCreation; }
   FDATE  GetLastAccessDate()       { return mFindBuf.fdateLastAccess; }
   FTIME  GetLastAccessTime()       { return mFindBuf.ftimeLastAccess; }
   FDATE  GetLastWriteDate()        { return mFindBuf.fdateLastWrite; }
   FTIME  GetLastWriteTime()        { return mFindBuf.ftimeLastWrite; }
   void  *GetUserData()             { return mUserData; }
   void   SetUserData( void *aData) { mUserData = aData; }

   PCSZ GetPathName()
   {
      if( !mPathname && mParent)
      {
         mPathname = new char [ CCHMAXPATH ];
         PCSZ pszParentPath = mParent->GetFileInfo()->GetPathName();
         if( pszParentPath)
         {
            strcpy( mPathname, pszParentPath);
            strcat( mPathname, "\\");
         }
         else
            *mPathname = 0;
         strcat( mPathname, GetLeafName());
      }
      return mPathname;
   }
};

// Files
class fsFile : public FS::IFile, public fsBase
{
   fsFile *mNext;

 public:
   fsFile( FILEFINDBUF3 *aBuf, FS::IDir *aParent) : fsBase(aBuf, aParent),
                                                    mNext(0)
   {}

   virtual ~fsFile()
   {}

   fsFile        *GetNext()     { return mNext; }
   FS::IDir      *GetParent()   { return mParent; } // XXX move into fsBase?
   FS::IFileInfo *GetFileInfo() { return this; }

   void           SetNext( fsFile *aNext) { mNext = aNext; }
};

// Directories
class fsDir : public FS::IDir, public fsBase
{
   fsFile *mFiles;
   fsDir  *mChild;
   fsDir  *mSibling;
   fsTree *mTree;

   #define FSDIR_FOLIATED 1
   #define FSDIR_EXPANDED 2

   ULONG   mFlags;

 public:
   fsDir( fsTree *aTree, FILEFINDBUF3 *aBuf, FS::IDir *aParent)
      : fsBase(aBuf, aParent),
        mFiles(0), mChild(0), mSibling(0), mTree(aTree), mFlags(0)
   {}

   virtual ~fsDir()
   {}

   FS::IDir      *GetParent()      { return mParent; } // XXX move into fsBase?
   fsDir         *GetFirstChild()  { return mChild; }
   fsDir         *GetNextSibling() { return mSibling; }
   void           Expand();
   void           Trash();

   fsFile        *GetFirstLeaf()   { return mFiles; }
   void           Foliate( BOOL aForce);

   fsBase        *GetFileInfo()    { return this; }

   void SetFiles( fsFile *aFile)   { mFiles = aFile; }
   void SetChild( fsDir *aDir)     { mChild = aDir; }
   void SetSibling( fsDir *aDir)   { mSibling = aDir; }

   virtual FS::IDrive *AsDrive()   { return 0; }
};

void fsDir::Expand()
{
   if( !(mFlags & FSDIR_EXPANDED))
   {
      mTree->ExpandNode( this);
      mFlags |= FSDIR_EXPANDED;
   }
}

void fsDir::Trash()
{
   if( mFlags & FSDIR_FOLIATED)
      mTree->DefoliateNode( this);
   mTree->TrashNode( this);
   mFlags = 0;
}

void fsDir::Foliate( BOOL aForce)
{
   if( !(mFlags & FSDIR_FOLIATED) || aForce)
   {
      if( aForce)
         mTree->DefoliateNode( this);
      mTree->FoliateNode( this);
      mFlags |= FSDIR_FOLIATED;
   }
}

// Drives
class fsDrive : public FS::IDrive, public fsDir
{
   BOOL   mIsRemovable;
   FSINFO mFSInfo;

 public:
   fsDrive( fsTree *aTree, FILEFINDBUF3 *aBuf, FS::IDir *aParent)
      : fsDir( aTree, aBuf, aParent),
        mIsRemovable((BOOL)-1)
   {}

   virtual ~fsDrive()
   {}

   BOOL IsRemovable()
   {
      if( mIsRemovable == (BOOL)-1)
         DosIsDriveRemovable( *GetLeafName() - 'A', &mIsRemovable);
      return mIsRemovable;
   }

   PCSZ GetVolumeLabel()
   {
      PCSZ   pcszLabel = 0;
      USHORT usDrive = *GetLeafName() - 'A';
      BOOL   bAvail = FALSE;

      // bit of a race here, but it's not critical.

      DosIsDriveAvailable( usDrive, &bAvail);

      if( bAvail)
      {
         APIRET rc = DosQueryFSInfo( usDrive + 1, FSIL_VOLSER,
                                     &mFSInfo, sizeof mFSInfo);

         if( !rc) pcszLabel = mFSInfo.vol.szVolLabel;
      }
      return pcszLabel;
   }

   fsDrive *AsDrive() { return this; }
};

// fsTree -----------------------------------------------------------------------

BOOL fsTree::DeleteInstance()
{
   delete this;
   return TRUE;
}

fsTree::~fsTree()
{
   // Execute synchronously to prevent leaky pain
   mDestructing = TRUE;   
   if( mRoot)
      DestroyNode( mRoot);

   mRQQueue.Close();
   mCBQueue.Close();

   if( mHEV)
      DosCloseEventSem( mHEV);

   if( mMutex)
      DosCloseMutexSem( mMutex);
}


BOOL fsTree::Init( FS::ICallbacks *aCallbacks)
{
   assert(!mCallbacks); // double-init is bad

   mCallbacks = aCallbacks;

   DosCreateMutexSem( 0, &mMutex, 0, 0);
   DosCreateEventSem( 0, &mHEV, 0, 0);

   return TRUE;
}

BOOL fsTree::Build( FS::BuildMode aBuildMode)
{
   assert( mCallbacks); // must have callbacks
   assert( !mRoot);     // can't call twice

   BOOL rc = FALSE;
   if( mCallbacks && !mRoot)
   {
      mBuildMode = aBuildMode;

      mRQQueue.Create();

      _beginthread( scannerThread, 0, 32768, this);

      if( mBuildMode == FS::Synchronous)
         DosWaitEventSem( mHEV, SEM_INDEFINITE_WAIT);

      rc = TRUE;
   }

   return rc;
}

BOOL fsTree::Lock()
{
   DosRequestMutexSem( mMutex, SEM_INDEFINITE_WAIT);
   return TRUE;
}

BOOL fsTree::Unlock()
{
   DosReleaseMutexSem( mMutex);
   return TRUE;
}

void fsTree::SendMsg( fsDir *aDir, RQPacket::Cmd aCmd)
{
   RQPacket packet( aDir, aCmd);
   APIRET rc = mRQQueue.Write( &packet);

   if( rc)
      TRACE(( "ERROR: fsTree::SendMsg (RQ) rc = %d\n", (int)rc));
}

void fsTree::SendMsg( fsDir *aDir, CBPacket::Cmd aCmd)
{
   CBPacket packet( aDir, aCmd);
   APIRET rc = mCBQueue.Write( &packet);

   if( rc)
      TRACE(( "ERROR: fsTree::SendMsg (CB 1) rc = %d\n", (int)rc));
}

void fsTree::SendMsg( fsFile *aFile, CBPacket::Cmd aCmd)
{
   CBPacket packet( aFile, aCmd);
   APIRET rc = mCBQueue.Write( &packet);

   if( rc)
      TRACE(( "ERROR: fsTree::SendMsg (CB 2) rc = %d\n", (int)rc));
}

void fsTree::SendCompleteMsg()
{
   CBPacket packet;
   APIRET rc = mCBQueue.Write( &packet);

   if( rc)
      TRACE(( "ERROR: fsTree::SendMsg (CB 3) rc = %d\n", (int)rc));
}

// Scanner thread ---------------------------------------------------------------
void fsTree::Scan()
{
   // Set up the notify thread
   mCBQueue.Create();
   _beginthread( notifyThread, 0, 32768, this);

   // Do root node
   CreateRootNode();

   // Do drives -- root dir for each drive
   // This will do *everything* if we're not in Lazy mode.
   CreateDrives();

   // Signal that we've done the initial scan.
   // This lets the user thread go if we're synchronous.
   SendCompleteMsg();

   // Process commands
   for(;;)
   {
      RQPacket pkt;
      APIRET rc = mRQQueue.Read( &pkt);
      if( rc)
      {
         TRACE(( "scanThread got rc %d, quitting\n", (int)rc));
         break;
      }

      switch( pkt.mCmd)
      {
         case RQPacket::Expand:
         {
            // scan on each child of pkt.mDir
            fsDir *pDir = pkt.mDir->GetFirstChild();
            while( pDir)
            {
               ScanForChildren( pDir);
               pDir = pDir->GetNextSibling();
            }
            break;
         }

         case RQPacket::Trash:
         {
            // Refresh a node - destroy tree below & rescan at top level.
            fsDir *pDir = pkt.mDir->GetFirstChild();
            pkt.mDir->SetChild(0);
            while( pDir)
            {
               fsDir *pTemp = pDir->GetNextSibling();
               Lock();
               DestroyNode( pDir); // pDir no longer valid
               Unlock();
               pDir = pTemp;
            }
            ScanForChildren( pkt.mDir);
            break;
         }

         case RQPacket::Foliate:
            // scan for files in pkt.mDir
            Foliate( pkt.mDir);
            break;

         case RQPacket::Defoliate:
            // remove all leaves from pkt.mDir
            Defoliate( pkt.mDir);
            break;
      }
   }
}

void fsTree::CreateRootNode()
{
   FILEFINDBUF3 buf = { 0 };
   strcpy( buf.achName, "$ROOT");
   buf.cchName = 5;
   mRoot = new fsDir( this, &buf, 0);
   SendMsg( mRoot, CBPacket::CreateRoot);
}

void fsTree::CreateDrives()
{
   ULONG ulDrives = 0;
   DosQueryNumDrives( &ulDrives);

   fsDrive *pPrev = 0;

   // go through the drives one by one
   for( UINT i = 0; i < ulDrives; i++)
   {
      APIRET       rc;
      FILESTATUS3  fs3 = { { 0, 0, 0 } };
      FILEFINDBUF3 ffb3 = { 0 };

      strcpy( ffb3.achName, "A:\\");
      ffb3.achName[0] += i;

      // create drive node from disk data
      DosError(0);
      rc = DosQueryPathInfo( ffb3.achName, FIL_STANDARD, &fs3, sizeof fs3);
      DosError(1);

      ffb3.achName[2] = 0;
      memcpy( &ffb3.fdateCreation, &fs3, sizeof fs3);

      // Insert drive node into tree
      fsDrive *pDrive = new fsDrive( this, &ffb3, mRoot);
      Lock();
      if( !pPrev) mRoot->SetChild( pDrive);
      else pPrev->SetSibling( pDrive);
      pPrev = pDrive;
      Unlock();

      SendMsg( pDrive, CBPacket::CreateNode);
   }

   // now scan for children.  Splitting the task up this way makes UIs more
   // responsive, as they can wap up the roots first before going digging.
   fsDir *pDir = mRoot->GetFirstChild();
   while( pDir)
   {
      ScanForChildren( pDir);
      pDir = pDir->GetNextSibling();
   }
}

void fsTree::ScanForChildren( fsDir *aParent)
{
   // Look through the directory indicated for child directories and
   // insert them into the tree.
   char         buffer[CCHMAXPATH] = "";
   APIRET       rc;
   HDIR         hDir = HDIR_CREATE;
   ULONG        ulCount = 1;
   FILEFINDBUF3 fb;

   fsDir       *pPrev = 0;

   strcpy( buffer, aParent->GetPathName());
   strcat( buffer, "\\*");

   // Scan for directories & build node-list

   DosError(0);
   rc = DosFindFirst( buffer, &hDir,
                      MUST_HAVE_DIRECTORY | FILE_ARCHIVED | FILE_SYSTEM |
                      FILE_HIDDEN | FILE_READONLY | FILE_DIRECTORY,
                      &fb, sizeof fb,
                      &ulCount, FIL_STANDARD);
   DosError(1);

   while( !rc)
   {
      if( strcmp( fb.achName, ".") && strcmp( fb.achName, ".."))
      {
         Lock();
         fsDir *pDir = new fsDir( this, &fb, aParent);
         if( !pPrev) aParent->SetChild( pDir);
         else pPrev->SetSibling( pDir);
         pPrev = pDir;
         Unlock();

         SendMsg( pDir, CBPacket::CreateNode);

         // If we're not in lazy mode, recurse (ahem!)
         if( mBuildMode != FS::Lazy)
            ScanForChildren( pDir);

      }
      rc = DosFindNext( hDir, &fb, sizeof fb, &ulCount);
   }

   DosFindClose( hDir);
}

// Destroy a node (recursively)
void fsTree::DestroyNode( fsDir *aDir)
{
   fsDir *aNotherDir = aDir->GetNextSibling();
   if( aNotherDir)
   {
      DestroyNode( aNotherDir);
      aDir->SetSibling( 0);
   }

   aNotherDir = aDir->GetFirstChild();
   if( aNotherDir)
   {           
      DestroyNode( aNotherDir);
      aDir->SetChild( 0);
   }

   if( mDestructing)
   {
      Defoliate( aDir);
      mCallbacks->DestroyDir( aDir);
      delete aDir;
   }
   else
      SendMsg( aDir, CBPacket::DestroyNode);
}

// Destroy a node's leaves
void fsTree::Defoliate( fsDir *aDir)
{
   Lock();

   fsFile *pFile = aDir->GetFirstLeaf();

   while( pFile)
   {
      fsFile *pTemp = pFile->GetNext();
      pFile->SetNext(0);

      if( mDestructing)
      {
         mCallbacks->DestroyFile( pFile);
         delete pFile;
      }
      else
         SendMsg( pFile, CBPacket::DestroyLeaf);

      pFile = pTemp;
   }

   aDir->SetFiles( 0);

   Unlock();
}

// Examine directory and add files to tree
void fsTree::Foliate( fsDir *aDir)
{
   FILEFINDBUF3 fb = { 0 };
   APIRET       rc = 0;
   ULONG        ulCount = 1;
   HDIR         hDir = HDIR_CREATE;
   char         path[ CCHMAXPATH];

   strcpy( path, aDir->GetPathName());
   strcat( path, "\\*");

   // Scan for files

   DosError(0);
   rc = DosFindFirst( path, &hDir,
                      FILE_ARCHIVED | FILE_SYSTEM |
                      FILE_HIDDEN | FILE_READONLY,
                      &fb, sizeof fb,
                      &ulCount,
                      FIL_STANDARD);
   DosError(1);
   fsFile *pLast = 0;

   while( !rc)
   {
      Lock();
      fsFile *pFile = new fsFile( &fb, aDir);
      if( !pLast) aDir->SetFiles( pFile);
      else pLast->SetNext( pFile);
      pLast = pFile;
      Unlock();

      SendMsg( pFile, CBPacket::CreateLeaf);

      rc = DosFindNext( hDir, &fb, sizeof fb, &ulCount);
   }

   DosFindClose( hDir);
}

// Notify thread ----------------------------------------------------------------
void fsTree::Notify()
{
   // Create PM if necessary (hackish, sorry)
   HAB hab;
   HMQ hmq;

   if( mNeedsPM)
   {
      hab = WinInitialize( 0);
      hmq = WinCreateMsgQueue( hab, 0);
   }

   for(;;)
   {
      CBPacket pkt;
      APIRET rc = mCBQueue.Read( &pkt);
      if( rc)
      {
         TRACE(( "notifyThread got rc %d, quitting\n", (int)rc));
         break;
      }

      switch( pkt.mCmd)
      {
         case CBPacket::CreateRoot:
            mCallbacks->CreateRoot( pkt.mData.mDir);
            break;

         case CBPacket::CreateNode:
            mCallbacks->CreateDir( pkt.mData.mDir);
            break;

         case CBPacket::DestroyNode:
            mCallbacks->DestroyDir( pkt.mData.mDir);
            delete pkt.mData.mDir;
            break;

         case CBPacket::CreateLeaf:
            mCallbacks->CreateFile( pkt.mData.mFile);
            break;

         case CBPacket::DestroyLeaf:
            mCallbacks->DestroyFile( pkt.mData.mFile);
            delete pkt.mData.mFile;
            break;

         case CBPacket::ScanComplete:
            mCallbacks->InitialScanComplete();
            if( mBuildMode == FS::Synchronous)
               DosPostEventSem( mHEV);
            break;

         default: 
            assert(0);
            break;
      }
   }

   if( mNeedsPM)
   {
      WinDestroyMsgQueue( hmq);
      WinTerminate( hab);
   }
}

// Bootstrap --------------------------------------------------------------------
FS::ITreeBuilder *FS::CreateTreeBuilder()
{
   return new fsTree;
}

// Dos-functions ----------------------------------------------------------------
APIRET DosQueryNumDrives( PULONG cDrives)
{
   int i;
   BOOL dummy;
   for( i = 0; i < 26; i++)
      if( DosIsDriveRemovable( i, &dummy)) break;
   *cDrives = i;
   return NO_ERROR;
}

APIRET DosIsDriveRemovable( USHORT drive, PBOOL rem)
{
   struct {
      BYTE commandInformation;
      BYTE driveUnit;
   }           driveRequest;
   ULONG       parmSize;
   BIOSPARAMETERBLOCK driveInfo;
   ULONG       dataSize;
   APIRET      rc;

   driveRequest.commandInformation = 0;
   driveRequest.driveUnit          = drive;
   parmSize = sizeof driveRequest;
   dataSize = sizeof driveInfo;
   memset( &driveInfo, 0, dataSize);
   DosError(0);
   rc = DosDevIOCtl( (HFILE)-1, IOCTL_DISK, DSK_GETDEVICEPARAMS,
                     &driveRequest, parmSize, &parmSize,
                     &driveInfo, dataSize, &dataSize);
   DosError(1);

   if( !rc && !driveInfo.bDeviceType) rc = ERROR_INVALID_DRIVE;
   else *rem = !(driveInfo.fsDeviceAttr & 1);

   return rc;
}

APIRET DosIsDriveAvailable( USHORT drive, PBOOL yesno)
{
   APIRET rc = 1;//DosIsDriveRemovable( drive, yesno);

   if( !rc && !*yesno)
   {
      *yesno = TRUE;     // bad assumption ?
   }
   else
   {
      #pragma pack(1)
   
      struct {
         UCHAR  commandInformation;
         USHORT driveunit;
      } paramPacket;
   
      #pragma pack()
   
      ULONG  paramSize;
      USHORT dataPacket;
      ULONG  dataSize;

      paramPacket.commandInformation = 0;
      paramPacket.driveunit = drive;
      paramSize = sizeof paramPacket;
      dataPacket = 0;
      dataSize = sizeof dataPacket;

      rc = DosDevIOCtl( (HFILE)-1, IOCTL_DISK, DSK_GETLOCKSTATUS,
                        &paramPacket, paramSize, &paramSize,
                        &dataPacket, dataSize, &dataSize);
   
      /* drive is available if (a) there is media (b) drive not locked */
      *yesno = !rc && (dataPacket & 0x4) && ((dataPacket & 3) != 1);
   }

   return rc;
}
