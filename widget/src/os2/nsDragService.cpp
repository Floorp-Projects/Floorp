/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
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
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 */

// Drag'n'drop manager

#include "nsWidgetDefs.h"
#include "nsDragService.h"
#include "nsITransferable.h"
#include "nsVoidArray.h"

#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsSpecialSystemDirectory.h"

#include "resid.h"

#include <stdio.h> // for sscanf
#include <stdlib.h> // realloc/free

//
// There are three major cases to consider:
//
// 1. Drag from xptoolkit to non-xptoolkit
//
//    Use DRM_OS2FILE.  Source render to get right rf used.
//
// 2. Drag from xptoolkit to xptoolkit
//
//    Use a custom shared-memory RM; special-case same process
//
// 3. Drag from non-xptoolkit to xptoolkit
//
//    Use DRM_OS2FILE and create nsFileListTransferable's
//    Problem: we need to copy the file because we have no way of knowing
//             when client has done with it & we can DM_ENDCONVERSATION
//
// Plus: xptoolkit will eventually acquire text/URL (or text/URI or something)
//       which we can handle.
//
// Apart from this, the major thing not implemented is the initiation of
// post-drop conversations for DRM_OS2FILE.  This is because of various reasons,
// see comment in nsDragService::DoPushedOS2FILE() below.
//

// Quick utility class to clean up files we create at some later point...
class nsGCFileSpec : public nsFileSpec
{
 public:
   nsGCFileSpec(nsFileSpec &fs) : nsFileSpec(fs) {}
  ~nsGCFileSpec()
   {
      Delete( FALSE);
   }
};

static char *DecodeStrHandle( HSTR hstr);
static const char *MimeTypeToRF( const char *pcsz);
static const char *RFToMimeType( const char *pcsz);
static BOOL FindFile( PDRAGITEM pItem, nsFileSpec &aFileSpec);
static void GetTempFile( nsFileSpec &tempfile);

// convert between pm & xptoolkit drag operation types
#define DragOperation(at)                                               \
  (at == nsIDragService::DRAGDROP_ACTION_NONE ? DO_DEFAULT :            \
   (at == nsIDragService::DRAGDROP_ACTION_COPY ? DO_COPY :              \
    (at == nsIDragService::DRAGDROP_ACTION_MOVE ? DO_MOVE : DO_LINK)))

#define DragDropAction(do)                                   \
  (do == DO_LINK ? nsIDragService::DRAGDROP_ACTION_LINK :    \
   (do == DO_COPY ? nsIDragService::DRAGDROP_ACTION_COPY :   \
    nsIDragService::DRAGDROP_ACTION_MOVE))

nsDragService::nsDragService() : mDragInfo(0), mDragItems(0)
{
   // XXX temporary icon until xptoolkit realises it needs to give us one
   mIcon = WinLoadPointer( HWND_DESKTOP, gModuleData.hModResources,
                           ID_ICO_DRAGITEM);

   // Window for doing things
   mDragWnd = WinCreateWindow( HWND_DESKTOP, WC_STATIC, 0, 0, 0, 0, 0, 0,
                               HWND_DESKTOP, HWND_BOTTOM, 0, 0, 0);
   mWndProc = WinSubclassWindow( mDragWnd, fnwpDragSource);
}

nsDragService::~nsDragService()
{
   WinDestroyPointer( mIcon);
   WinDestroyWindow( mDragWnd);
}

nsresult nsDragService::InvokeDragSession( nsIDOMNode       *aDOMNode,
					   nsISupportsArray *aTransArray,
                                           nsIRegion        *aRegion,
                                           PRUint32          aActionType)
{
   nsBaseDragService::InvokeDragSession ( aDOMNode, aTransArray, aRegion, aActionType );
  
   // This is horribly multidimensional -- we have an array of dragitems, fine.
   // But -- each dragitem may be a file list, which itself is several dragitems.
   PDRAGITEM pDragItems = 0;
   ULONG     cDragItems = 0;

   PRUint32 cItems;
   aTransArray->Count( &cItems);
   for( PRUint32 i = 0; i < cItems; i++)
   {
      nsCOMPtr<nsISupports> pThing;
      pThing = dont_AddRef( aTransArray->ElementAt(i)); // XXX check this doesn't leak
      if( pThing)
      {
         // Get dragitems for this array element
         nsCOMPtr<nsITransferable> pXfer = do_QueryInterface(pThing);
         PDRAGITEM                 pDItems = 0;
         ULONG                     cDItems = 0;

         CreateDragItems( &cDItems, &pDItems, pXfer);

         // Now extend & copy up into the local array
         pDragItems = (PDRAGITEM)
                    realloc( pDragItems, cDragItems + cDItems * sizeof(DRAGITEM));
         memcpy( pDragItems + cDragItems, pDItems, cDItems * sizeof(DRAGITEM));
         cDragItems += cDItems;

         // free local data
         delete [] pDItems;
      }
   }

   nsresult rc = NS_ERROR_FAILURE;

   if( cDragItems)
   {
      rc = InvokeDrag( pDragItems, cDragItems, aActionType);
      free( pDragItems);
   }

   return rc;
}


nsresult nsDragService::GetData( nsITransferable *aTransferable,
                                 PRUint32         aItemIndex)
{
   // Fill the transferable with data from the given dragitem.
   if( !aTransferable || aItemIndex >= mDragInfo->cditem)
      return NS_ERROR_FAILURE;

   PDRAGITEM pItem = DrgQueryDragitemPtr( mDragInfo, aItemIndex);

   // Problem here.  Unless we're xptoolkit<->xptoolkit we can only
   // get data in *one* flavour.
   //
   // Oh well; do our best.

   nsVoidArray *pFormats = nsnull;
   aTransferable->FlavorsTransferableCanImport( &pFormats);
   PRUint32 cFormats = pFormats->Count();

   for( PRUint32 i = 0; i < cFormats; i++)
   {
      nsString *pFlavour = (nsString*) pFormats->ElementAt( i);
      char      buff[40];
      gModuleData.ConvertFromUcs( *pFlavour, buff, 40);
      const char *rf = MimeTypeToRF( buff);
      if( rf && DrgVerifyRMF( pItem, 0, rf))
      {
         // Okay, have something to do.  Now pick a rendering mechanism:
         void     *pData = 0;
         PRUint32  cData = 0;

         BOOL      source_dry_p = FALSE; // can only get single flavour

         // 1.  Some xptoolkit - use DRM_MOZILLA
         if( DrgVerifyRMF( pItem, "DRM_MOZILLA", 0))
         {
            DoMozillaXfer( pItem, buff, &pData, &cData);
         }
         // 2.  Some random process - use DRM_OS2FILE
         else if( DrgVerifyRMF( pItem, "DRM_OS2FILE", 0))
         {
            // 2a. `normal' rendering - load it, break out
            if( pItem->hstrSourceName && pItem->hstrContainerName)
            {
               nsFileSpec file;

               if( !FindFile( pItem, file))
                  printf( "Can't find dropped file\n");
               else
               {
                  cData = file.GetFileSize();
                  pData = new char [ cData ];
                  nsInputFileStream istream( file);
                  istream.read( pData, (PRInt32) cData);
                  istream.close();

                  source_dry_p = TRUE;
               }
            }
            // 2b. DRM_OS2FILE push - faff around endlessly, break out
            //     This is a bit tricky 'cos this method needs to be synchronous.
            else
            {
               DoPushedOS2FILE( pItem, rf, &pData, &cData);
               source_dry_p = TRUE;
            }
         }
         else
         {
            const char *rmf = DecodeStrHandle( pItem->hstrRMF);
            printf( "Incomprehensible DRM (%s)\n", rmf);
         }

         if( pData && cData)
            aTransferable->SetTransferData( pFlavour, pData, cData);

         if( source_dry_p)
            break;
      }
      else if( pFlavour->Equals( kDropFilesMime))
      {
         // Moan if this isn't a filelisttransferable.
         nsCOMPtr<nsIFileListTransferable> pFileList = do_QueryInterface(aTransferable);

         if( !pFileList)
            printf( "kDropFilesMime requested but no filelisttransferable!\n");
         else
         {
            // Need a file.
            nsFileSpec *pFileSpec = 0;

            if( DrgVerifyRMF( pItem, "DRM_MOZILLA", 0))
            {
               void     *pData = 0;
               PRUint32  cData;
               DoMozillaXfer( pItem, buff, &pData, &cData);

               if( pData)
               {
                  nsFileSpec tempfile;
                  GetTempFile( tempfile);
   
                  nsOutputFileStream stream(tempfile);
                  stream.write( pData, (PRInt32)cData);
                  stream.close();
   
                  // Make sure this temp file is erased eventually.
                  pFileSpec = new nsGCFileSpec( tempfile);
               }
            }
            // 2.  Some random process - use DRM_OS2FILE
            else if( DrgVerifyRMF( pItem, "DRM_OS2FILE", 0))
            {
               if( pItem->hstrSourceName && pItem->hstrContainerName)
               {
                  nsFileSpec file;
   
                  if( !FindFile( pItem, file))
                     printf( "Can't find dropped file\n");
                  else
                     pFileSpec = new nsFileSpec(file);
               }
               else
               {
                  // (can't actually do source rendering yet)
                  DoPushedOS2FILE(0,0,0,0);
               }
            }
            else
            {
               const char *rmf = DecodeStrHandle( pItem->hstrRMF);
               printf( "Incomprehensible DRM -> file (%s)\n", rmf);
            }

            // Did we get one?
            if( pFileSpec)
            {
               nsVoidArray array;
               array.AppendElement(pFileSpec);
               pFileList->SetFileList(&array);
            }
         }
      }
   }

   return NS_OK;
}

nsresult nsDragService::GetNumDropItems( PRUint32 *aNumItems)
{
   if( !aNumItems)
      return NS_ERROR_NULL_POINTER;

   *aNumItems = mDragInfo->cditem;

   return NS_OK;
}

nsresult nsDragService::IsDataFlavorSupported( nsString *aDataFlavour)
{
   // The idea here is to return NS_OK if any of the dragitems supports
   // this flavour (yeah, hmm...)
   //
   // Maybe we should change it so they all have to be (which is what CUA
   // says we should do...)

   nsresult rc = NS_ERROR_FAILURE;

   char buff[40];
   gModuleData.ConvertFromUcs( *aDataFlavour, buff, 40);

   const char *rf = MimeTypeToRF( buff);

#ifdef DEBUG
   printf( "IsDataFlavorSupported %s\n", buff);
   printf( "RF for that is %s\n", rf);
#endif

   if( rf)
   {
      for( PRUint32 i = 0; i < mDragInfo->cditem; i++)
      {
         PDRAGITEM pItem = DrgQueryDragitemPtr( mDragInfo, i);
         // this checks for ANY rm, which is a bit dubious.
         if( DrgVerifyRMF( pItem, 0, rf))
         {
            rc = NS_OK;
            break;
         }
      }
   }

#ifdef DEBUG
   printf( "Flavor is %ssupported.\n", rc == NS_OK ? "" : "not ");
#endif

   return rc;
}

// Starting drag-over event block.
void nsDragService::InitDragOver( PDRAGINFO aDragInfo)
{
   // If the drag's from another process, grab it
   if( !mDragInfo)
   {
      DrgAccessDraginfo( aDragInfo);
      mDragInfo = aDragInfo;
   }

   // Set xp flags
   SetCanDrop( PR_FALSE);
   SetDragAction( DragDropAction(mDragInfo->usOperation));
   StartDragSession();
}

// end of drag-over event block; get xp settings & convert.
MRESULT nsDragService::TermDragOver()
{
   MRESULT  rc;
   PRBool   bCanDrop;
   PRUint32 action;

   EndDragSession();
   GetCanDrop( &bCanDrop);
   GetDragAction( &action);

   rc = MPFROM2SHORT( bCanDrop ? DOR_DROP : DOR_NODROP, DragOperation(action));

   // ...factor code...
   TermDragExit();

   return rc;
}

void nsDragService::InitDragExit( PDRAGINFO aDragInfo)
{
   // Nothing else to do
   InitDragOver( aDragInfo);
}

void nsDragService::TermDragExit()
{
   // release draginfo if appropriate.  Note we use the slightly icky way
   // of looking at the value of mDragItems to see if we started the drag.
   if( !mDragItems)
   {
      DrgFreeDraginfo( mDragInfo);
      mDragInfo = 0;
   }
}

void nsDragService::InitDrop( PDRAGINFO aDragInfo)
{
   // again, doesn't look like there's anything else to do
   InitDragOver( aDragInfo);
}

void nsDragService::TermDrop()
{
   // do an end-conversation for each dragitem.
   // Any actual rendering is done in response to the GetData method above.
   for( PRUint32 i = 0; i < mDragInfo->cditem; i++)
   {
      PDRAGITEM pItem = DrgQueryDragitemPtr( mDragInfo, i);
      DrgSendTransferMsg( pItem->hwndItem,
                          DM_ENDCONVERSATION,
                          MPFROMLONG(pItem->ulItemID),
                          MPFROMSHORT(DMFL_TARGETSUCCESSFUL)); // I suppose
   }

   TermDragExit();
}

// access the singleton
nsresult NS_GetDragService( nsISupports **aDragService)
{
   if( !aDragService)
      return NS_ERROR_NULL_POINTER;

   *aDragService = (nsIDragService*)gModuleData.dragService;
   NS_ADDREF(*aDragService);

   return NS_OK;
}

// Examine a transferable and allocate & fill in appropriate DRAGITEMs
void nsDragService::CreateDragItems( PULONG pCount, PDRAGITEM *ppItems,
                                     nsITransferable *aTransferable)
{
   nsCOMPtr<nsIFileListTransferable> pFileList = do_QueryInterface(aTransferable);

   if( pFileList)
   {
      nsVoidArray aFiles;
      pFileList->GetFileList( &aFiles);
      *pCount = aFiles.Count();
      if( *pCount)
      {
         // Create a dragitem for each filespec
         *ppItems = new DRAGITEM [*pCount];
         for( PRUint32 i = 0; i < *pCount; i++)
         {
            nsFileSpec *pFile = (nsFileSpec*) aFiles.ElementAt(i);
            FillDragItem( *ppItems + i, pFile);
         }
      }
   }
   else
   {
      *ppItems = new DRAGITEM [1]; // alloc w' new [] for uniform deletion
      *pCount = 1;
      FillDragItem( *ppItems, aTransferable);
   }
}

void nsDragService::FillDragItem( PDRAGITEM aItem, nsFileSpec *aFilespec)
{
   // We don't have to source-render these, 'cos they're unique files which
   // already exist (we hope).
   //
   // On the down side, I have to trust nsFileSpec...
   //

   aItem->hwndItem = mDragWnd; // just for completeness
   aItem->ulItemID = 0;
   aItem->hstrType = DrgAddStrHandle( DRT_UNKNOWN);
   // XXX print & discard to come ?  Maybe not, actually.
   aItem->hstrRMF = DrgAddStrHandle( "<DRM_OS2FILE,DRF_UNKNOWN>");

   // (this is a really messy, unwieldy api)
   nsFileSpec parentDir;
   aFilespec->GetParent( parentDir);
   aItem->hstrContainerName = DrgAddStrHandle( nsNSPRPath(parentDir));
   char *pszLeaf = aFilespec->GetLeafName();
   aItem->hstrSourceName = DrgAddStrHandle( pszLeaf);
   aItem->hstrTargetName = DrgAddStrHandle( pszLeaf);
   nsCRT::free( pszLeaf);

   aItem->cxOffset = aItem->cyOffset = 0;
   aItem->fsControl = 0;
   aItem->fsSupportedOps = DO_COPYABLE | DO_MOVEABLE | DO_LINKABLE;
}

// Transferables passed in here are guaranteed not to be nsIFileListTransferables
//
void nsDragService::FillDragItem( PDRAGITEM aItem, nsITransferable *aTransferable)
{
   aItem->hwndItem = mDragWnd;

   // ref the transferable to write out the data when we know which is needed
   aItem->ulItemID = (ULONG) aTransferable;
   NS_ADDREF(aTransferable);

   // Now go through transferable building things
   nsVoidArray *pFormats = nsnull;
   aTransferable->FlavorsTransferableCanExport( &pFormats);

   // XXX DRM_DISCARD and DRM_PRINTFILE to come when xptoolkit decides how
   //     (whether...) to handle them

   char rmf[200] = "(DRM_OS2FILE,DRM_MOZILLA) X (DRF_UNKNOWN";
   char buff[40];

   PRUint32 cFormats = pFormats->Count();

   for( PRUint32 i = 0; i < cFormats; i++)
   {
      nsString *pFlavour = (nsString*) pFormats->ElementAt( i);
      gModuleData.ConvertFromUcs( *pFlavour, buff, 40);
      const char *rf = MimeTypeToRF( buff);
      if( rf)
      {
         strcat( rmf, ",");
         strcat( rmf, rf);
      }
#if 0
      else if( pFlavour->Equals( kURLMime))
      {
         // Abort any processing already done; the idea here is to provide
         // the URL format so the WPS can do the Right Thing; we also need to
         // provide some kind of text format for insertion into a program.
         aItem->hstrType = DrgAddStrHandle( DRT_URL","DRT_TEXT);
         aItem->hstrRMF = DrgAddStrHandle( "<DRM_OS2FILE,DRF_TEXT>,<DRM_STRHANDLES,DRF_URL>");
         aItem->hstrContainerName = 0;
         aItem->hstrSourceName = DrgAddStrHandle( full url );
         aItem->hstrTargetName = DrgAddStrHandle( title for object );
         aItem->cxOffset = aItem->cyOffset = 0;
         aItem->fsControl = 0;
         aItem->fsSupportedOps = DO_COPYABLE | DO_MOVEABLE | DO_LINKABLE;

         return;
      }
#endif
      // Not sure what to do about 'Type'.  Setting everything is wrong,
      // but picking an arbitary one is also bad.
      //
      // So leave it as unknown for now.
   }

   delete pFormats;

   strcat( rmf, ")");

   aItem->hstrType = DrgAddStrHandle( DRT_UNKNOWN);
   aItem->hstrRMF = DrgAddStrHandle( rmf);

   // For source-rendering, don't supply `source name'
   nsSpecialSystemDirectory tmpDir(nsSpecialSystemDirectory::OS_TemporaryDirectory);
   aItem->hstrContainerName = DrgAddStrHandle( nsNSPRPath(tmpDir));
   aItem->hstrSourceName = 0;
   aItem->hstrTargetName = DrgAddStrHandle( "ATempFile");
   aItem->cxOffset = aItem->cyOffset = 0; // DrgDrag() sets these
   aItem->fsControl = 0;
   aItem->fsSupportedOps = DO_COPYABLE | DO_MOVEABLE | DO_LINKABLE;
}


// Actually do the drag
nsresult nsDragService::InvokeDrag( PDRAGITEM aItems, ULONG aCItems,
                                    PRUint32 aActionType)
{
   // keep track of allocated draginfos
   NS_ASSERTION(!mDragInfo,"Drag info leaked");

   nsresult  rc = NS_ERROR_FAILURE;
   PDRAGINFO pDragInfo = DrgAllocDraginfo( aCItems);

   if( pDragInfo)
   {
      pDragInfo->usOperation = DragOperation(aActionType);
      pDragInfo->hwndSource = mDragWnd;
      for( PRUint32 i = 0; i < aCItems; i++)
         DrgSetDragitem( pDragInfo, aItems + i, sizeof(DRAGITEM), i);

      // XXX Need to make a dragimage from somewhere.  There ought to be an
      //     nsIImage passed in here, but let's just make something up for now.
      //
      // XXX also need to handle the multiple-dragitem case correctly
      //
      DRAGIMAGE dimage = { sizeof(DRAGIMAGE), 0, mIcon,
                           { 0, 0 }, DRG_ICON, 0, 0 };

      // Set draginfo pointer for reentrancy
      mDragInfo = pDragInfo;
      mDragItems = aCItems;

      HWND hwndDest = DrgDrag( mDragWnd, pDragInfo, &dimage, 1, VK_ENDDRAG,
#ifdef DEBUG
                               (void*) 0x80000000L // makes IPMD happier
#else
                               0
#endif
                                );
     rc = NS_OK; // hwndDest == 0 may be error or just cancelled; shrug.

     if( !hwndDest)
     {
        // Clean up everything here; no async. case to consider.
        DrgDeleteDraginfoStrHandles( pDragInfo);
        DrgFreeDraginfo( pDragInfo);
        mDragInfo = 0;
        mDragItems = 0;
     }

     // We don't DrgFreeDragInfo() here if there is a transfer of some sort;
     // instead this is done when we get the appropriate DM_ENDCONVERSATION.
   }

   return rc;
}

// Window-proc. for the drag-source window
MRESULT EXPENTRY fnwpDragSource( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   MRESULT mr;

   if( gModuleData.dragService)
      mr = gModuleData.dragService->HandleMessage( msg, mp1, mp2);
   else
      mr = WinDefWindowProc( hwnd, msg, mp1, mp2);

   return mr;
}

MRESULT nsDragService::HandleMessage( ULONG msg, MPARAM mp1, MPARAM mp2)
{
   switch(msg)
   {
      case DM_RENDER:
      {
         NS_ASSERTION(mDragItems && mDragInfo, "Spurious DM_RENDER");

         // The target wants us to put some data somewhere.
         PDRAGTRANSFER pXFer = (PDRAGTRANSFER) mp1;
         BOOL          rc = FALSE;

         // This is vitally important, apparently.
         pXFer->pditem->hstrSourceName = DrgAddStrHandle( "Dummy.fil");

         if( !pXFer->pditem->ulItemID)
         {
            // the target (silly boy) has asked us to source-render something
            // it's completely capable of rendering all by itself.
            pXFer->fsReply = DMFL_NATIVERENDER;
            DrgFreeDragtransfer( pXFer);
         }
         else
         {
            // Need to actually return from this call before we can render the
            // thing...
            WinPostMsg( mDragWnd, WM_USER, mp1, mp2);
            rc = TRUE;
         }

         return MRFROMLONG(rc);
      }

      //
      // Very possibly this should be in a fresh thread.
      //
      // This rather large block is the source-side of the source-render
      // protocol; the target picks which format (from those provided by
      // the transferable) it would like & where we should put it.
      //
      case WM_USER:
      {
         // Posted to ourselves in order to carry out a target's desired
         // rendering.
         PDRAGTRANSFER    pXFer = (PDRAGTRANSFER) mp1;
         nsITransferable *pTrans = (nsITransferable*) pXFer->pditem->ulItemID;
         USHORT           usFlags = DMFL_RENDERFAIL;

         NS_ASSERTION(pTrans, "No transferable to source-render");

#ifdef DEBUG
         char *target = DecodeStrHandle( pXFer->hstrRenderToName);
         printf( "Source-rendering to %s\n", target);
         nsFileSpec dest( target);
#else
         nsFileSpec dest( DecodeStrHandle( pXFer->hstrRenderToName));
#endif
         // Now, the fun bit is working out what format to use.
         char *rmf = DecodeStrHandle( pXFer->hstrSelectedRMF);
         char rm[50], rf[50];
         int tokens = sscanf( rmf, "<%s,%s>", rm, rf);

         NS_ASSERTION(tokens == 2, "Couldn't parse hstrSelectedRMF");
#ifdef DEBUG
         printf( "%d - %s %s\n", tokens, rm, rf);
#endif
         if( !strcmp( rm, "DRM_OS2FILE"))
         {
            // Go through the transferable's flavours looking for one which
            // matches the requested rf.
            nsVoidArray *pFormats = nsnull;
            char         buff[40];
            pTrans->FlavorsTransferableCanExport( &pFormats);
            PRUint32 cFormats = pFormats->Count(), i;

            for( i = 0; i < cFormats; i++)
            {
               nsString *pFlavour = (nsString*) pFormats->ElementAt( i);
               gModuleData.ConvertFromUcs( *pFlavour, buff, 40);
               const char *this_rf = MimeTypeToRF( buff);
               if( this_rf && !strcmp( this_rf, rf))
               {
                  // Found it!
                  void    *pData;
                  PRUint32 cData;
                  pTrans->GetTransferData( pFlavour, &pData, &cData);

                  // (uh-oh)
                  nsOutputFileStream stream(dest);
                  stream.write( pData, (PRInt32)cData);
                  stream.close();
                  usFlags = DMFL_RENDEROK;
                  break;
               }
            }

            delete pFormats;
            if( i == cFormats)
#ifdef DEBUG
               printf( "Target asked for format %s which we can't do.\n", rf);
#endif
         }
         else
         {
            printf( "Unexpected rendering mechanism\n");
         }

         // Tell the target we're done.
         DrgPostTransferMsg( pXFer->hwndClient,
                             DM_RENDERCOMPLETE,
                             pXFer,
                             usFlags,
                             0,
                             TRUE);

         DrgFreeDragtransfer( pXFer);

         // Note that the transferable we have here will be release'd in
         // the DM_ENDCONVERSATION for the dragitem in question.

         return 0;
      }

      // DRM_MOZILLA messages; see nsWidgetDefs.h for details
      case WMU_GETFLAVOURLEN:
      case WMU_GETFLAVOURDATA:
      {
         char      buffer[40] = "";
         PRUint32  cData = 0;
         void     *pData;

         nsITransferable *pTrans = (nsITransferable*) mp1;

         if( pTrans)
         {
            PWZDROPXFER pXFer = (PWZDROPXFER) mp2;
            ATOM        atom;

            if( msg == WMU_GETFLAVOURLEN) atom = LONGFROMMP(mp2);
            else atom = pXFer->hAtomFlavour;

            WinQueryAtomName( WinQuerySystemAtomTable(), atom, buffer, 40);
            nsAutoString str(buffer);
            if( NS_SUCCEEDED(pTrans->GetTransferData( &str, &pData, &cData)))
            {
               if( msg == WMU_GETFLAVOURDATA)
               {
                  memcpy( &pXFer->data[0], pData, cData);
                  DosFreeMem( pXFer);
                  return MPFROMLONG(TRUE);
               }
            }
         }
         
         return MRFROMLONG(cData);
      }

      case DM_ENDCONVERSATION:
      {
         NS_ASSERTION(mDragItems && mDragInfo, "Unexpected DM_ENDCONVERSATION");

         // If it was necessary (for source-rendering), we kept a reference
         // to the transferable in the dragitem.  Now it's safe to release.
         nsITransferable *pTransferable = (nsITransferable*) mp1;
         NS_IF_RELEASE(pTransferable);

         mDragItems--;
         if( mDragItems == 0)
         {
            // the last of the dragitems has been ack'ed, so free the draginfo
            DrgFreeDraginfo( mDragInfo);
            mDragInfo = 0;
         }
#ifdef DEBUG
         printf( "DM_ENDCONVERSATION, mDragItems = %d\n", (int)mDragItems);
#endif
         return 0;
      }
   }

   return (*mWndProc)( mDragWnd, msg, mp1, mp2);
}

// Various forms of source-rendering, DRM_MOZILLA and "pushed-file" -------------
//
// Custom shared-memory rendering mechanism
//
// Probably ought to just get creative with DRAGTRANSFER as is, but that (a)
// confuses the DRM_OS2FILE source-rendering & (b) imposes unhelpful constraints

void nsDragService::DoMozillaXfer( PDRAGITEM pItem, char *szFlavour,
                                   void **ppData, PRUint32 *cData)
{
   // First check if this is an intra-process transfer!
   if( mDragItems)
   {
      // Yes.
      nsITransferable *pSource = (nsITransferable*) pItem->ulItemID;
      if( !pSource)
         printf( "intra-process xfer fails due to null ulItemID\n");
      else
      {
         nsAutoString flavour(szFlavour);
         if( NS_SUCCEEDED(pSource->GetTransferData( &flavour, ppData, cData)))
         {
            // need to make a copy...
            char *tmp = new char [ *cData ];
            memcpy( tmp, *ppData, *cData);
            *ppData = tmp;
         }
      }
      return;
   }

   HATOMTBL hAtomTbl = WinQuerySystemAtomTable();
   ATOM     hAtom = WinAddAtom( hAtomTbl, szFlavour);

   ULONG ulLen = (ULONG) WinSendMsg( pItem->hwndItem, WMU_GETFLAVOURLEN,
                                     MPFROMLONG(pItem->ulItemID),
                                     MPFROMLONG(hAtom));
   if( ulLen)
   {
      void *shmem = 0;
      if( !DosAllocSharedMem( &shmem, 0, ulLen + sizeof(ATOM),
                              PAG_COMMIT | OBJ_GIVEABLE | PAG_WRITE))
      {
         // Find the tid of the source so we can give it the memory
         PID pid;
         TID tid;
         WinQueryWindowProcess( pItem->hwndItem, &pid, &tid);
         DosGiveSharedMem( shmem, pid, PAG_WRITE);

         PWZDROPXFER pWzData = (PWZDROPXFER) shmem;
         pWzData->hAtomFlavour = hAtom;

         BOOL ok = (BOOL) WinSendMsg( pItem->hwndItem, WMU_GETFLAVOURDATA,
                                      MPFROMLONG(pItem->ulItemID),
                                      MPFROMP(pWzData));
         if( ok)
         {
            // now allocate (too many copies I know, but transferable has
            // restrictions...)
            char *buf = new char [ ulLen ];
            memcpy( buf, &pWzData->data[0], ulLen);
            *cData = ulLen;
            *ppData = buf;
         }

         // free shared memory
         DosFreeMem( shmem);
      }
   }

   WinDeleteAtom( hAtomTbl, hAtom);
}

void nsDragService::DoPushedOS2FILE( PDRAGITEM pItem, const char *szRf,
                                     void **pData, PRUint32 *cData)
{
   // Unfortunately there's no way we can do this: we must return from DM_DROP
   // before having a "post-drop conversation" involving DM_RENDER.
   //
   // BUT we need to fill the transferable NOW, before we return from DM_DROP
   // so that gecko (or whoever's underneath us).
   //
   // So we're stuck until there's some asynch. way of proceeding.
   //
   // But it's not all bad: not many people use source rendering; mozilla
   // does, but we can use DRM_MOZILLA to do that.

   printf( "\n\nSorry, source-rendering of DRM_OS2FILE not working.\n");
   printf( "(see mozilla/widget/src/os2/nsDragService::DoPushedOS2FILE)\n\n");
}

// Quick utility functions ------------------------------------------------------

static char *DecodeStrHandle( HSTR hstr)
{
   static char buf[CCHMAXPATH];
   DrgQueryStrName( hstr, CCHMAXPATH, buf);
   return buf;
}

// Table to map mozilla "mimetypes" to PM "rendering formats"
// Could optimize I guess, but not really important.

static const char *gFormatMap[][2] =
{
   { "DRF_TEXT",    kTextMime      },
   { "DRF_UNICODE", kUnicodeMime   },
   { "DRF_HTML",    kHTMLMime      },
   { "DRF_PNG",     kPNGImageMime  },
   { "DRF_GIF",     kGIFImageMime  },
   { "DRF_JPEG",    kJPEGImageMime },
   { "DRF_AOLMAIL", kAOLMailMime   },
   { 0, 0 }
};

static const char *MimeTypeToRF( const char *pcsz)
{
   int i = 0;
   while( gFormatMap[i][0])
      if( !strcmp( pcsz, gFormatMap[i][1])) break;
   return gFormatMap[i][0];
}

static const char *RFToMimeType( const char *pcsz)
{
   int i = 0;
   while( gFormatMap[i][0])
      if( !strcmp( pcsz, gFormatMap[i][0])) break;
   return gFormatMap[i][1];
}

static BOOL FindFile( PDRAGITEM pItem, nsFileSpec &aFileSpec)
{
   const char *str = DecodeStrHandle( pItem->hstrContainerName);
#ifdef DEBUG
   printf( "Getting drag data from `%s'", str);
#endif
   aFileSpec = str;
   str = DecodeStrHandle( pItem->hstrSourceName);
#ifdef DEBUG
   printf( "`%s'\n", str);
#endif
   aFileSpec += str;

   return aFileSpec.Exists();
}

static void GetTempFile( nsFileSpec &tempfile)
{
   nsSpecialSystemDirectory tmpDir(nsSpecialSystemDirectory::OS_TemporaryDirectory);
   tmpDir += "tmpfile";
   tmpDir.MakeUnique();
   tempfile = tmpDir;
}
