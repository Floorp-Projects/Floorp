/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst.  All Rights Reserved.
 *
 * Contributor(s): 
 *
 */

//
// FSTree -- filing-system-tree-builder
//
// This is in mozilla to provide an adaquate 'choose directory' dialog.
// It could be used to provide a nice 'choose file' dialog as well.
//
//      * Lazy or eager
//      * Synchronous or asynchronous
//      * UI-free
//
// mjf Sunday 06 June 1999, rehashing various versions of this code written for
// various purposes written over the past few years into one beauteous whole.
//
// The interface scheme is something of an experiment.
//

#ifndef _fstree_h
#define _fstree_h

// Name-space for interfaces
struct FS
{
   // Base class for filing-system entities
   class IFileInfo
   { 
    public:
      virtual PCSZ   GetLeafName() = 0;
      virtual PCSZ   GetPathName() = 0;
      virtual ULONG  GetAttributes() = 0;
      virtual ULONG  GetSize() = 0;
      virtual FDATE  GetCreationDate() = 0;
      virtual FTIME  GetCreationTime() = 0;
      virtual FDATE  GetLastAccessDate() = 0;
      virtual FTIME  GetLastAccessTime() = 0;
      virtual FDATE  GetLastWriteDate() = 0;
      virtual FTIME  GetLastWriteTime() = 0;

      virtual void  *GetUserData() = 0;
      virtual void   SetUserData( void *aUserData) = 0;
   };

   class IDir;

   // Files
   class IFile
   {
    public:
      virtual IFile     *GetNext() = 0;
      virtual IDir      *GetParent() = 0;
      virtual IFileInfo *GetFileInfo() = 0;
   };

   class IDrive;

   // Directories
   class IDir
   {
    public:
      virtual IDir      *GetParent() = 0;
      virtual IDir      *GetFirstChild() = 0;
      virtual IDir      *GetNextSibling() = 0;
      virtual void       Expand() = 0;
      virtual void       Trash() = 0;

      virtual IFile     *GetFirstLeaf() = 0;
      virtual void       Foliate( BOOL bForceRefresh) = 0;

      virtual IFileInfo *GetFileInfo() = 0;

      virtual IDrive    *AsDrive() = 0;
   };

   // Drives
   class IDrive
   {
    public:
      virtual BOOL IsRemovable() = 0;
      virtual PCSZ GetVolumeLabel() = 0;
   };

   // Callbacks (clients implement this interface)
   class ICallbacks
   {
    public:
      // The meta node, parent of all drives
      virtual void CreateRoot( IDir *aRootNode) = 0;
      // Callback to notify completion of initial scan
      virtual void InitialScanComplete() = 0;
      // All directories named here have parents
      virtual void CreateDir( IDir *aNewNode) = 0;
      // Clean up user data if necessary
      virtual void DestroyDir( IDir *aNode) = 0;
      virtual void CreateFile( IFile *aNewLeaf) = 0;
      // Clean up user data if necessary
      virtual void DestroyFile( IFile *aLeaf) = 0;
   };

   enum BuildMode // ways in which tree can be built
   {
      Lazy,         // keep 1 level ahead of expand requests
      Eager,        // expand until there's nothing left to do
      Synchronous   // do everything (as for Eager) before Build() returns
   };

   // Tree builder
   class ITreeBuilder
   {
    public:
      virtual BOOL Init( ICallbacks *aCallbacks) = 0;
      virtual BOOL RequiresPM( BOOL bSet) = 0;
      virtual BOOL Build( BuildMode aBuildMode) = 0;
      virtual BOOL Lock() = 0;
      virtual BOOL Unlock() = 0;
      virtual BOOL DeleteInstance() = 0;
   };

   // Bootstrap (use DeleteInstance() to destroy)
   static ITreeBuilder *CreateTreeBuilder();
};

#endif
