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
 * Fairhurst. Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All
 * Rights Reserved.
 *
 * Contributor(s): John Fairhurst <john_fairhurst@iname.com>
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 03/23/2000       IBM Corp.      Fixed error in displaying dialog box.
 *
 */

// To do
//        * Use the correct icons on drives
//          This requires beefing up DosIsRemovable() from FSTree.cpp to a
//          general 'DosQueryDriveType', and providing a handy array of icons
//          in the resource module, OR doing something funky with the WPS.
//
//        * Helptext
//
// (dialog is usable at this point; those below are just for bonus points)
//
//        * Make dialog resizable
//          Just requires coding.
//
//        * Add a 1-pixel bevel-in border around the container
//
//        * When user supplies a pathname in their DIRPICKER, hilight it in
//          the tree.  This is a bit hard 'cos of asynchronous FSTree.  Maybe
//          should fix that to allow "synchronous calls", so Expand() and Trash()
//          would have completed by the time they return (easy).
//
//        * Save preferences (cnr colour, dialog position & size) somewhere
//
//        * Directory history (replace typein at bottom with an entryfield)
//
//        * More options?  Subclassing?

#define INCL_WIN
#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "nsFSTree.h"
#include "resid.h"
#include "nsDirPicker.h"

// XXX kludges
static HMODULE res_handle;

// Container records

struct TREENODE
{
   MINIRECORDCORE m;
   FS::IDir      *pDir;
};

typedef TREENODE *PTREENODE;

#define DIR2REC(dir) ((PTREENODE)dir->GetFileInfo()->GetUserData())

static void insertTree( HWND hwndCnr, PTREENODE pParent, PTREENODE pRecord,
                        PRECORDINSERT pRI);

APIRET DosReallyCreateDir( PSZ dirpath);

void CenterWindow( HWND hwnd, HWND hwndOver);

// Window-word data for the dialog; implement fs callbacks to dynamically
// build the tree (and stuff)

struct DirPicker : public FS::ICallbacks
{
   HWND              hwndDlg;
   HWND              hwndCnr;
   HWND              hwndCBox;
   HWND              hwndEf;
   FS::ITreeBuilder *fsTree;
   FS::IDir         *pMetaNode;
   BOOL              initted;
   PTREENODE         pCurrRoot;
   PDIRPICKER        pDirPicker;
   HPOINTER          icoFolder;

   DirPicker( HWND dlg, PDIRPICKER aPicker) : hwndDlg(dlg), pMetaNode(0),
                                              initted(FALSE), pCurrRoot(0),
                                              pDirPicker(aPicker)
   {
      hwndCnr = WinWindowFromID( hwndDlg, IDD_TREECNR);
      hwndCBox = WinWindowFromID( hwndDlg, IDD_CBDRIVES);
      hwndEf = WinWindowFromID( hwndDlg, IDD_EFPATH);
      fsTree = FS::CreateTreeBuilder();
      fsTree->Init( this);
      fsTree->RequiresPM( TRUE);
      icoFolder = WinLoadPointer( HWND_DESKTOP, res_handle, ID_ICO_FOLDER);
   }

   virtual ~DirPicker()
   {
      WinDestroyPointer( icoFolder);
   }

   void CreateRoot( FS::IDir *aRootNode)
   {
      pMetaNode = aRootNode;
   }

   void CreateDir( FS::IDir *aNewNode)
   {
      // Create a new node
      PTREENODE pRec = (PTREENODE) WinSendMsg( hwndCnr, CM_ALLOCRECORD,
                                               MPFROMLONG(4), MPFROMLONG(1));
      if( !pRec) return;
      // find some strings; note that fstree owns them
      pRec->m.pszIcon = (char*) aNewNode->GetFileInfo()->GetLeafName();
      if( aNewNode->AsDrive())
      {
         // Now add the drive to the combobox
         char listitem[40];
         strcpy( listitem, pRec->m.pszIcon);
         PCSZ vollbl = aNewNode->AsDrive()->GetVolumeLabel();
         if( vollbl && *vollbl)
         {
            strcat( listitem, " [");
            strcat( listitem, vollbl);
            strcat( listitem, "]");
         }
         MRESULT index = WinSendMsg( hwndCBox, LM_INSERTITEM,
                                     MPFROMSHORT(LIT_END), MPFROMP(listitem));
         WinSendMsg( hwndCBox, LM_SETITEMHANDLE, index, MPFROMP(pRec));

         // display the current drive first; if the user gave us a seed, we'll
         // do that in InitialScanComplete().
         ULONG ulDriveNum, ulDriveMap;
         DosQueryCurrentDisk( &ulDriveNum, &ulDriveMap);
         char buffer[] = "A:";
         buffer[0] += ulDriveNum - 1;

         // Filter out all but the root of this drive
         if( stricmp( pRec->m.pszIcon, buffer))
            pRec->m.flRecordAttr = CRA_FILTERED;
         else
         {
            pCurrRoot = pRec;
            WinSendMsg( hwndCBox, LM_SELECTITEM, index, MPFROMSHORT(TRUE));
         }

         // XXX use the right icon
         pRec->m.hptrIcon = icoFolder;
      }
      else
      {
         PCSZ pcszPathname = aNewNode->GetFileInfo()->GetPathName();
         // don't need to doserror protect this call 'cos we'll only get
         // callbacks if the directory has been seen.
         pRec->m.hptrIcon = WinLoadFileIcon( pcszPathname, FALSE);

         // once we start getting straight directories, toplevel is done
         if( !initted)
         {
            initted = TRUE;
            WinPostMsg( hwndCnr, CM_EXPANDTREE, MPFROMP(pCurrRoot), 0);
         }
      }

      // Tie up both ways
      pRec->pDir = aNewNode;
      aNewNode->GetFileInfo()->SetUserData( pRec);

      // Now insert the record into the container
      RECORDINSERT rI = { sizeof(RECORDINSERT), (PRECORDCORE) CMA_END, 0,
                          TRUE, CMA_TOP, 1 };
      // Set parent if appropriate
      if( aNewNode->GetParent()->GetParent())
      {
         rI.pRecordParent = (PRECORDCORE) DIR2REC(aNewNode->GetParent());
      }

      WinSendMsg( hwndCnr, CM_INSERTRECORD, MPFROMP(pRec), MPFROMP(&rI));
   }

   void DestroyDir( FS::IDir *aNode)
   {
      if( aNode != pMetaNode)
      {
         PTREENODE pNode = DIR2REC(aNode);
         if( pNode)
         {
            if( pNode == pCurrRoot)
               WinSendMsg( hwndCnr, CM_REMOVERECORD,
                           MPFROMP(&pNode), MPFROM2SHORT(1, 0));
            WinSendMsg( hwndCnr, CM_FREERECORD, MPFROMP(&pNode), MPFROMSHORT(1));
         }
      }
   }

   void InitialScanComplete()
   {
      // Mismatch between our gui requirements and the way that FSTree provides
      // us with data.  Really ought to alter FSTree to build drives selectively;
      // this would be done 
      //
      // Anyhow, now we know that the initial scan has completed, we can take
      // out of the container those drives which have been filtered away.
      // They needed to be in the container in order to build the tree.

      FS::IDir *pNode = pMetaNode->GetFirstChild();
      while( pNode)
      {
         if( pNode != pCurrRoot->pDir)
         {
            PTREENODE pRecord = DIR2REC(pNode);
            if( pRecord)
            {
               WinSendMsg( hwndCnr, CM_REMOVERECORD,
                           MPFROMP(&pRecord), MPFROM2SHORT(1, 0));
               pRecord->m.flRecordAttr &= ~CRA_FILTERED;
            }
         }

         pNode = pNode->GetNextSibling();
      }

      // Now we know where we are.
      // If the user has given a seed directory, try & find it (!)
      if( pDirPicker->szFullFile[0])
      {
         // XXX write me
      }
   }

   void CreateFile( FS::IFile *aNewLeaf) {}
   void DestroyFile( FS::IFile *aLeaf) {}
};

MRESULT EXPENTRY fndpDirPicker( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   DirPicker *pData = (DirPicker *) WinQueryWindowPtr( hwnd, QWL_USER);

   switch( msg)
   {
      case WM_INITDLG:
      {
         PDIRPICKER pPicker = (PDIRPICKER) mp2;
         // create data area & save it away
         pData = new DirPicker( hwnd, pPicker);
         WinSetWindowPtr( hwnd, QWL_USER, pData);

         // prep window
         CenterWindow( hwnd, WinQueryWindow( hwnd, QW_OWNER));

         // XXX colour for container bg?
         // XXX Do help later
         WinEnableControl( hwnd, IDD_HELPBUTTON, FALSE);

         // sort out container
         CNRINFO cnrInfo = { sizeof(CNRINFO), 0 };
         cnrInfo.flWindowAttr = CV_TREE | CV_ICON | CV_MINI | CA_TREELINE;
         WinSendMsg( pData->hwndCnr, CM_SETCNRINFO,
                     MPFROMP( &cnrInfo), MPFROMLONG( CMA_FLWINDOWATTR));
         WinSetWindowBits( pData->hwndCnr, QWL_STYLE, 0, WS_CLIPSIBLINGS);

         // allow entryfield to be long enough to display a full pathname
         WinSendMsg( pData->hwndEf, EM_SETTEXTLIMIT, MPFROMSHORT(CCHMAXPATH), 0);
         if( *pPicker->szFullFile)
            WinSetWindowText( pData->hwndEf, pPicker->szFullFile);

         if( pPicker->pszTitle)
            WinSetWindowText( hwnd, pPicker->pszTitle);
         WinShowWindow( hwnd, TRUE);

         // begin scan (returns immediately)
         pData->fsTree->Build( FS::Lazy);

         break;
      }

     case WM_CONTROL:
        switch( SHORT1FROMMP(mp1))
        {
           case IDD_TREECNR:
              switch( SHORT2FROMMP(mp1))
              {
                 case CN_EXPANDTREE:
                 {
                    PTREENODE pNode = (PTREENODE) mp2;
                    pNode->pDir->Expand();
                    return 0;
                 }
                 case CN_ENTER:
                 {
                    // Trigger an expand
                    PNOTIFYRECORDENTER pThing = (PNOTIFYRECORDENTER) mp2;
                    if( pThing->pRecord) // trap double-click on background
                       WinPostMsg( pData->hwndCnr, CM_EXPANDTREE,
                                   MPFROMP(pThing->pRecord), 0);
                    return 0;
                 }
                 case CN_EMPHASIS:
                 {
                    PNOTIFYRECORDEMPHASIS pThing = (PNOTIFYRECORDEMPHASIS) mp2;
                    if( pThing->fEmphasisMask & CRA_SELECTED)
                    {
                       PTREENODE pNode = (PTREENODE) pThing->pRecord;
                       // check this node has GAINED not LOST selection
                       if( pNode->m.flRecordAttr & CRA_SELECTED)
                       {
                          // set path into entryfield, specialcase root dirs
                          // to get the backslash
                          char  buffer[4] = "A:\\";
                          char *pname = 
                                (char*) pNode->pDir->GetFileInfo()->GetPathName();
                          if( pNode->pDir->AsDrive())
                          {
                             buffer[0] = pname[0];
                             pname = buffer;
                          }
                          WinSetWindowText( pData->hwndEf, pname);
                       }
                    }
                    return 0;
                 }
              }
              break;

           case IDD_CBDRIVES:
              switch( SHORT2FROMMP(mp1))
              {
                 case CBN_EFCHANGE:
                 {
                    // change drives (potentially)
                    MRESULT index;
                    index = WinSendMsg( pData->hwndCBox, LM_QUERYSELECTION,
                                        MPFROMSHORT(LIT_FIRST),0);
                    PTREENODE pNode = (PTREENODE)
                       WinSendMsg( pData->hwndCBox, LM_QUERYITEMHANDLE, index, 0);

                    // actually a change?
                    if( pNode != pData->pCurrRoot)
                    {
                       // remove old drive tree
                       WinSendMsg( pData->hwndCnr, CM_REMOVERECORD,
                                   MPFROMP(&pData->pCurrRoot), MPFROM2SHORT(1,0));

                       // insert new one
                       //
                       // I could have *sworn* that you could insert an entire
                       // tree just by inserting the root.  Unfortunately this
                       // isn't so :-(

                       // Single RI on the stack
                       RECORDINSERT rI = { sizeof(RECORDINSERT),
                                           (PRECORDCORE) CMA_END,
                                           0, TRUE, CMA_TOP, 1 };

                       // if the new drive is removable, generate a fresh tree
                       FS::IDrive *pDrive = pNode->pDir->AsDrive();
                       if( pDrive->IsRemovable())
                       {
                          WinSendMsg( pData->hwndCnr, CM_INSERTRECORD,
                                      MPFROMP(pNode), MPFROMP(&rI));
                          pNode->pDir->Trash();
                       }
                       else
                          insertTree( pData->hwndCnr, 0, pNode, &rI);

                       pData->pCurrRoot = pNode;
                       WinPostMsg( pData->hwndCnr, CM_EXPANDTREE,
                                   MPFROMP(pData->pCurrRoot), 0);
                    }

                    return 0;
                 }
              }
              break;
        }
        break;

      case WM_COMMAND:
         switch( SHORT1FROMMP(mp1))
         {
            case DID_CANCEL:
            case DID_OK:
            {
               WinQueryWindowText( pData->hwndEf, CCHMAXPATH,
                                   pData->pDirPicker->szFullFile);
               pData->pDirPicker->lReturn = SHORT1FROMMP(mp1);

               if( SHORT1FROMMP(mp1) == DID_OK)
               {
                  // Check if directory exists etc.
                  FILESTATUS3 fs3;
                  APIRET rc = DosQueryPathInfo( pData->pDirPicker->szFullFile,
                                                FIL_STANDARD, &fs3, sizeof fs3);
                  if( rc)
                  {
                     char msg1[256], buffer[500];
                     WinLoadString( 0, res_handle, ID_STR_HMMDIR, 256, msg1);
                     sprintf( buffer, msg1, pData->pDirPicker->szFullFile);
                     ULONG ret = WinMessageBox( HWND_DESKTOP, hwnd, buffer,
                                                0, 0, MB_YESNO | MB_QUERY |
                                                MB_APPLMODAL);
                     if( ret != MBID_YES) return 0;
                     else
                     {
                        rc = DosReallyCreateDir( pData->pDirPicker->szFullFile);
                        if( rc)
                        {
                           WinLoadString( 0, res_handle, ID_STR_NOCDIR, 256, msg1);
                           WinMessageBox( HWND_DESKTOP, hwnd, msg1, 0, 0,
                                          MB_OK | MB_ERROR | MB_APPLMODAL);
                           return 0;
                        }
                     }
                  }
               }
               // fall out, defdlgproc will close the window.
            }
         }
         pData->fsTree->DeleteInstance();
         delete pData;
         break;

      case WM_CLOSE:
         WinPostMsg( hwnd, WM_COMMAND, MPFROM2SHORT(DID_CANCEL,0), 0);
         return 0;
   }

   return WinDefDlgProc( hwnd, msg, mp1, mp2);
}

// Insert an FSTree tree (DFS, recursive; ought to do iterative magic, or
// at least optimize the 'insert children' case to insert them all at once)
static void insertTree( HWND hwndCnr, PTREENODE pParent, PTREENODE pRecord,
                        PRECORDINSERT pRI)
{
   // insert this node
   pRI->pRecordParent = (PRECORDCORE) pParent;
   WinSendMsg( hwndCnr, CM_INSERTRECORD, MPFROMP(pRecord), MPFROMP(pRI));

   // do each child
   FS::IDir *pDir = pRecord->pDir->GetFirstChild();
   while( pDir)
   {
      insertTree( hwndCnr, pRecord, DIR2REC(pDir), pRI);
      pDir = pDir->GetNextSibling();
   }
}

// Entrypoint
HWND APIENTRY FS_PickDirectory( HWND       hwndParent,
                                HWND       hwndOwner,
                                HMODULE    hModResources,
                                PDIRPICKER pDirPicker)
{
   HWND hwndRet = 0;

   if( pDirPicker)
   {
      // XXX fix this somehow; FS_SetResourceHandle(), or move whole lot
      // XXX into a dll (not for mozilla, tho')
      res_handle = hModResources;

      if( pDirPicker->bModal)
      {
         ULONG rc = WinDlgBox( hwndParent, hwndOwner,
                               fndpDirPicker, hModResources,
                               DID_DIRPICKER, pDirPicker);
         hwndRet = (rc != DID_ERROR);
      }
      else
      {
         hwndRet = WinLoadDlg( hwndParent, hwndOwner,
                               fndpDirPicker, hModResources,
                               DID_DIRPICKER, pDirPicker);
      }
   }

   return hwndRet;
}

// create all segments in the path
APIRET DosReallyCreateDir( PSZ dirpath)
{
   APIRET rc = 0;
   int    depth = 0;
   char  *c;
   char   buff[CCHMAXPATH];

   strcpy( buff, dirpath);

   for(;;)
   {
      rc = DosCreateDir( dirpath, 0);
      if( (rc == NO_ERROR && depth == 0) ||
          (rc && rc != ERROR_PATH_NOT_FOUND)) break;
      // if rc = 0 go down a level, else go up a level
      if( rc == ERROR_PATH_NOT_FOUND)
      {
         c = strrchr( buff, '\\');
         if( !c) break;
         depth++;
         *c = '\0';
      }
      else
      {
         depth--;
         buff[ strlen( buff)] = '\\';
      }
   }

   return rc;
}

// Center hwnd over hwndOver
void CenterWindow( HWND hwnd, HWND hwndOver)
{
   SWP swp1, swp2;
   if( !hwndOver) hwndOver = HWND_DESKTOP;

   WinQueryWindowPos( hwnd, &swp1);
   WinQueryWindowPos( hwndOver, &swp2);
   swp1.x = (swp2.cx - swp1.cx) / 2;
   swp1.y = (swp2.cy - swp1.cy) / 2;
   WinSetWindowPos( hwnd, 0, swp1.x, swp1.y, 0, 0, SWP_MOVE);
}
