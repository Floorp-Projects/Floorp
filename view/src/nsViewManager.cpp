/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick C. Beard <beard@netscape.com>
 *   Kevin McCluskey  <kmcclusk@netscape.com>
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
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

#define PL_ARENA_CONST_ALIGN_MASK (sizeof(void*)-1)
#include "plarena.h"

#include "nsViewManager.h"
#include "nsUnitConversion.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsGfxCIID.h"
#include "nsIScrollableView.h"
#include "nsView.h"
#include "nsISupportsArray.h"
#include "nsICompositeListener.h"
#include "nsCOMPtr.h"
#include "nsIEventQueue.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsGUIEvent.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsRegion.h"
#include "nsInt64.h"
#include "nsScrollPortView.h"
#include "nsHashtable.h"

static NS_DEFINE_IID(kBlenderCID, NS_BLENDER_CID);
static NS_DEFINE_IID(kRegionCID, NS_REGION_CID);
static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

#define ARENA_ALLOCATE(var, pool, type) \
    {void *_tmp_; PL_ARENA_ALLOCATE(_tmp_, pool, sizeof(type)); \
    var = NS_REINTERPRET_CAST(type*, _tmp_); }
/**
   XXX TODO XXX

   DeCOMify newly private methods
   Optimize view storage
*/

/**
   A note about platform assumptions:

   We assume all native widgets are opaque.
   
   We assume that a widget is z-ordered on top of its parent.
   
   We do NOT assume anything about the relative z-ordering of sibling widgets. Even though
   we ask for a specific z-order, we don't assume that widget z-ordering actually works.
*/

// if defined widget changes like moves and resizes are defered until and done
// all in one pass.
//#define CACHE_WIDGET_CHANGES

// display list flags
#define VIEW_RENDERED     0x00000001
#define PUSH_CLIP         0x00000002
#define POP_CLIP          0x00000004
#define VIEW_TRANSPARENT  0x00000008
#define VIEW_TRANSLUCENT  0x00000010
#define VIEW_CLIPPED      0x00000020
// used only by CanScrollWithBitBlt
#define VIEW_ISSCROLLED   0x00000040
#define PUSH_FILTER       0x00000080
#define POP_FILTER        0x00000100

#define NSCOORD_NONE      PR_INT32_MIN

#define SUPPORT_TRANSLUCENT_VIEWS

// The following number is used in OptimizeDisplayList to decide when the
// opaque region is getting too complicated.  When we get to an opaque region
// with MAX_OPAQUE_REGION_COMPLEXITY rects in it, we stop adding views' rects
// to the opaque region.  On "normal" pages (those without hundreds to
// thousands of positioned elements) there are usually not that many opaque
// views around; 10 is plenty for such pages.
#define MAX_OPAQUE_REGION_COMPLEXITY 10

/*
  This class represents an offscreen buffer which may have an alpha channel.
  Currently, if an alpha channel is required, we implement it by rendering into
  two buffers: one with a black background, one with a white background. We can
  recover the alpha values by comparing corresponding final values for each pixel.
*/
class BlendingBuffers {
public:
  BlendingBuffers(nsIRenderingContext* aCleanupContext);
  ~BlendingBuffers();

  // used by the destructor to cleanup resources
  nsCOMPtr<nsIRenderingContext> mCleanupContext;
  // The primary rendering context. When an alpha channel is in use, this
  // holds the black background.
  nsCOMPtr<nsIRenderingContext> mBlackCX;
  // Only used when an alpha channel is required; holds the white background.
  nsCOMPtr<nsIRenderingContext> mWhiteCX;

  PRBool mOwnBlackSurface;
  // drawing surface for mBlackCX
  nsIDrawingSurface*  mBlack;
  // drawing surface for mWhiteCX
  nsIDrawingSurface*  mWhite;

  // The offset within the current widget at which this buffer will
  // eventually be composited
  nsPoint mOffset;
};

// A DisplayListElement2 records the information needed to paint one view.
// Note that child views get their own DisplayListElement2s; painting a view
// paints that view's frame and all its child frames EXCEPT for the child frames
// that have their own views.
struct DisplayListElement2 {
  nsView*       mView;        
  nsRect        mBounds;      // coordinates relative to the view manager root
  nscoord       mAbsX, mAbsY; // coordinates relative to the view that we're Refreshing 
  PRUint32      mFlags;       // see above
  nsInt64       mZIndex;      // temporary used during z-index processing (see below)
private: // Prevent new/delete of these elements
  DisplayListElement2();
  ~DisplayListElement2();
};

/*
  IMPLEMENTING Z-INDEX

  Implementing z-index:auto and negative z-indices properly is hard. Here's how we do it.

  In CreateDisplayList, the display list elements above are inserted into a tree rather
  than a simple list. The tree structure mimics the view hierarchy (except for fixed-position
  stuff, see below for more about that), except that in the tree, only leaf nodes can do
  actual painting (i.e., correspond to display list elements). Therefore every leaf view
  (i.e., no child views) has one corresponding leaf tree node containing its
  DisplayListElement2. OTOH, every non-leaf view corresponds to an interior tree node
  which contains the tree nodes for the child views, and which also contains a leaf tree
  node which does the painting for the non-leaf view. Initially sibling tree nodes are
  ordered in the same order as their views, which Layout should have arranged in document
  order.

  For example, given the view hierarchy and z-indices, and assuming lower-
  numbered views precede higher-numbered views in the document order,
    V0(auto) --> V1(0) --> V2(auto) --> V3(0)
     |            |         +---------> V4(2)
     |            +------> V5(1)
     +---------> V6(1)
  CreateDisplayList would create the following z-tree (z-order increases from top to bottom)
    Ta(V0) --> Tb*(V0)
     +-------> Tc(V1) --> Td*(V1)
     |          +-------> Te(V2) --> Tf*(V2)
     |          |          +-------> Tg*(V3)
     |          |          +-------> Th*(V4)
     |          +-------> Ti*(V5)
     +-------> Tj*(V6)
  (* indicates leaf nodes marked with display list elements)

  Once the Z-tree has been built we call SortByZOrder to compute a real linear display list.
  It recursively computes a display list for each tree node, by computing the display lists
  for all child nodes, then concatenating those lists and sorting them by z-index. The trick
  is that the z-indices for display list elements are updated during the process; after
  a display list is calculated for a tree node, the elements of the display list are all
  assigned the z-index specified for the tree node's view (unless the view has z-index
  'auto'). This ensures that a tree node's display list elements will be sorted correctly
  relative to the siblings of the tree node.

  The above example is processed as follows:
  The child nodes of Te(V2) are display list elements [ Tf*(V2)(0), Tg*(V3)(0), Th*(V4)(2) ].
  (The display list element for the frames of a non-leaf view always has z-index 0 relative
  to the children of the view.)
  Te(V2) is 'auto' so its display list is [ Tf*(V2)(0), Tg*(V3)(0), Th*(V4)(2) ].
  Tc(V1)'s child display list elements are [ Td*(V1)(0), Tf*(V2)(0), Tg*(V3)(0), Th*(V4)(2),
  Ti*(V5)(1) ].
  The nodes are sorted and then reassigned z-index 0, so Tc(V1) is replaced with the list
  [ Td*(V1)(0), Tf*(V2)(0), Tg*(V3)(0), Ti*(V5)(0), Th*(V4)(0) ].
  Finally we collect the elements for Ta(V0):
  [ Tb*(V0), Td*(V1)(0), Tf*(V2)(0), Tg*(V3)(0), Ti*(V5)(0), Th*(V4)(0), Tj*(V6)(1) ].

  The z-tree code is called frequently and was a heavy user of the heap.
  In order to reduce the amount of time spent allocating and deallocating
  memory, the code was changed so that all the memory used to build the z-tree,
  including the DisplayListElement2 elements, is allocated from an Arena.
  This greatly reduces the number of function calls to new and delete,
  and eleminates a final call to DestroyZTreeNode which was needed to
  recursivly walk and free the tree.

  In order to ensure that all the z-tree elements are allocated from the Arena,
  the DisplayZTreeNode and DisplayListElement2 structures have private (and
  unimplemented) constructors and destructors.  This will ensure a compile
  error if someone attempts to create or destroy one of these structures
  using new or delete.
*/

struct DisplayZTreeNode {
  nsView*              mView;            // Null for tree leaf nodes
  DisplayZTreeNode*    mZSibling;

  // We can't have BOTH an mZChild and an mDisplayElement
  DisplayZTreeNode*    mZChild;          // tree interior nodes
  DisplayListElement2* mDisplayElement;  // tree leaf nodes
private: // Prevent new/delete of these elements
  DisplayZTreeNode();
  ~DisplayZTreeNode();
};

#ifdef DEBUG
static PRInt32 PrintDisplayListElement(DisplayListElement2* element,
                                       PRInt32 aNestCount) {
    nsView*              view = element->mView;
    nsRect               rect = element->mBounds;
    PRUint32             flags = element->mFlags;
    PRUint32             viewFlags = view ? view->GetViewFlags() : 0;
    nsRect               dim;
    if (view) {
      view->GetDimensions(dim);
    }
    nsPoint              v = view ? view->GetPosition() : nsPoint(0, 0);
    nsView* parent = view ? view->GetParent() : nsnull;
    PRInt32 zindex = view ? view->GetZIndex() : 0;
    nsView* zParent = view ? view->GetZParent() : nsnull;
    nsViewManager* viewMan = view ? view->GetViewManager() : nsnull;

    printf("%*snsIView@%p{%d,%d,%d,%d @ %d,%d; p=%p,m=%p z=%d,zp=%p} [x=%d, y=%d, w=%d, h=%d, absX=%d, absY=%d]\n",
           aNestCount*2, "", (void*)view,
           dim.x, dim.y, dim.width, dim.height,
           v.x, v.y,
           (void*)parent, (void*)viewMan, zindex, (void*)zParent,
           rect.x, rect.y, rect.width, rect.height,
           element->mAbsX, element->mAbsY);

    PRInt32 newnestcnt = aNestCount;

    if (flags)
      {
        printf("%*s", aNestCount*2, "");

        if (flags & POP_CLIP) {
          printf("POP_CLIP ");
          newnestcnt--;
        }

        if (flags & PUSH_CLIP) {
          printf("PUSH_CLIP ");
          newnestcnt++;
        }

        if (flags & POP_FILTER) {
          printf("POP_FILTER ");
          newnestcnt--;
        }

        if (flags & PUSH_FILTER) {
          printf("PUSH_FILTER ");
          newnestcnt++;
        }

        if (flags & VIEW_RENDERED) 
          printf("VIEW_RENDERED ");

        if (flags & VIEW_ISSCROLLED)
          printf("VIEW_ISSCROLLED ");

        if (flags & VIEW_CLIPPED)
          printf("VIEW_ISCLIPPED ");

        if (flags & VIEW_TRANSLUCENT)
          printf("VIEW_ISTRANSLUCENT ");

        if (flags & VIEW_TRANSPARENT)
          printf("VIEW_ISTRANSPARENT ");

        if (viewFlags & NS_VIEW_FLAG_DONT_BITBLT)
          printf("NS_VIEW_FLAG_DONT_BITBLT ");

        printf("\n");
      }
    return newnestcnt;
}

static void PrintZTreeNode(DisplayZTreeNode* aNode, PRInt32 aIndent) 
{
  if (aNode) {
    printf("%*sDisplayZTreeNode@%p\n", aIndent*2, "", (void*)aNode);
    if (aNode->mDisplayElement) {
      PrintDisplayListElement(aNode->mDisplayElement, 0);
    }

    aIndent += 2;

    for (DisplayZTreeNode* child = aNode->mZChild; child;
         child = child->mZSibling) {
      PrintZTreeNode(child, aIndent);
    }
  }
}
#endif

#ifdef NS_VM_PERF_METRICS
#include "nsITimeRecorder.h"
#endif

//-------------- Begin Invalidate Event Definition ------------------------

struct nsViewManagerEvent : public PLEvent {
  nsViewManagerEvent(nsViewManager* aViewManager);
  
  virtual void HandleEvent() = 0;
  
  nsViewManager* ViewManager() {
    // |owner| is a weak pointer, but the view manager will destroy any
    // pending invalidate events in it's destructor.
    return NS_STATIC_CAST(nsViewManager*, owner);
  }
};

static void* PR_CALLBACK HandlePLEvent(PLEvent* aEvent)
{
  NS_ASSERTION(nsnull != aEvent,"Event is null");
  nsViewManagerEvent *event = NS_STATIC_CAST(nsViewManagerEvent*, aEvent);

  // Search for valid view manager before trying to access it.  This
  // is working around a bug in RevokeEvents.
  const nsVoidArray *vmArray = nsViewManager::GetViewManagerArray();
  NS_ENSURE_TRUE(vmArray && vmArray->IndexOf(event->ViewManager()) != -1,
                 nsnull);

  event->HandleEvent();
  return nsnull;
}

static void PR_CALLBACK DestroyPLEvent(PLEvent* aEvent)
{
  NS_ASSERTION(nsnull != aEvent,"Event is null");
  nsViewManagerEvent *event = NS_STATIC_CAST(nsViewManagerEvent*, aEvent);
  delete event;
}

nsViewManagerEvent::nsViewManagerEvent(nsViewManager* aViewManager)
{
  NS_ASSERTION(aViewManager, "null parameter");  
  PL_InitEvent(this, aViewManager, ::HandlePLEvent, ::DestroyPLEvent);  
}

struct nsInvalidateEvent : public nsViewManagerEvent {
  nsInvalidateEvent(nsViewManager* aViewManager)
    : nsViewManagerEvent(aViewManager) { }

  virtual void HandleEvent() {
    ViewManager()->ProcessInvalidateEvent();
  }
};

//-------------- End Invalidate Event Definition ---------------------------

// Compare two Z-index values taking into account topmost and 
// auto flags. the topmost flag is only used when both views are
// zindex:auto.  (XXXldb Lying!)
// 
// returns 0 if equal
//         > 0 if first z-index is greater than the second
//         < 0 if first z-index is less than the second

static PRInt32 CompareZIndex(PRInt32 aZIndex1, PRBool aTopMost1, PRBool aIsAuto1,
                             PRInt32 aZIndex2, PRBool aTopMost2, PRBool aIsAuto2) 
{
  NS_ASSERTION(!aIsAuto1 || aZIndex1 == 0,"auto is set and the z-index is not 0");
  NS_ASSERTION(!aIsAuto2 || aZIndex2 == 0,"auto is set and the z-index is not 0");

  if (aZIndex1 != aZIndex2) {
    return aZIndex1 - aZIndex2;
  } else {
    return aTopMost1 - aTopMost2;
  }
}

void
nsViewManager::PostInvalidateEvent()
{
  nsCOMPtr<nsIEventQueue> eventQueue;
  mEventQueueService->GetSpecialEventQueue(
    nsIEventQueueService::UI_THREAD_EVENT_QUEUE, getter_AddRefs(eventQueue));
  NS_ASSERTION(nsnull != eventQueue, "Event queue is null");

  if (eventQueue != mInvalidateEventQueue) {
    nsInvalidateEvent* ev = new nsInvalidateEvent(this);
    eventQueue->PostEvent(ev);
    mInvalidateEventQueue = eventQueue;
  }
}

#undef DEBUG_MOUSE_LOCATION

PRInt32 nsViewManager::mVMCount = 0;
nsIRenderingContext* nsViewManager::gCleanupContext = nsnull;

// Weakly held references to all of the view managers
nsVoidArray* nsViewManager::gViewManagers = nsnull;
PRUint32 nsViewManager::gLastUserEventTime = 0;

nsViewManager::nsViewManager()
  : mMouseLocation(NSCOORD_NONE, NSCOORD_NONE)
{
  if (gViewManagers == nsnull) {
    NS_ASSERTION(mVMCount == 0, "View Manager count is incorrect");
    // Create an array to hold a list of view managers
    gViewManagers = new nsVoidArray;
  }
 
  if (gCleanupContext == nsnull) {
    /* XXX: This should use a device to create a matching |nsIRenderingContext| object */
    CallCreateInstance(kRenderingContextCID, &gCleanupContext);
    NS_ASSERTION(gCleanupContext,
                 "Wasn't able to create a graphics context for cleanup");
  }

  gViewManagers->AppendElement(this);

  mVMCount++;
  // NOTE:  we use a zeroing operator new, so all data members are
  // assumed to be cleared here.
  mCachingWidgetChanges = 0;
  mDefaultBackgroundColor = NS_RGBA(0, 0, 0, 0);
  mAllowDoubleBuffering = PR_TRUE; 
  mHasPendingInvalidates = PR_FALSE;
  mRecursiveRefreshPending = PR_FALSE;
}

nsViewManager::~nsViewManager()
{
  if (mRootView) {
    // Destroy any remaining views
    mRootView->Destroy();
    mRootView = nsnull;
  }

  nsCOMPtr<nsIEventQueue> eventQueue;
  mEventQueueService->GetSpecialEventQueue(nsIEventQueueService::UI_THREAD_EVENT_QUEUE,
                                           getter_AddRefs(eventQueue));
  NS_ASSERTION(nsnull != eventQueue, "Event queue is null"); 
  eventQueue->RevokeEvents(this);
  mInvalidateEventQueue = nsnull;  
  mSynthMouseMoveEventQueue = nsnull;  

  mRootScrollable = nsnull;

  NS_ASSERTION((mVMCount > 0), "underflow of viewmanagers");
  --mVMCount;

#ifdef DEBUG
  PRBool removed =
#endif
    gViewManagers->RemoveElement(this);
  NS_ASSERTION(removed, "Viewmanager instance not was not in the global list of viewmanagers");

  if (0 == mVMCount) {
    // There aren't any more view managers so
    // release the global array of view managers
   
    NS_ASSERTION(gViewManagers != nsnull, "About to delete null gViewManagers");
    delete gViewManagers;
    gViewManagers = nsnull;

    // Cleanup all of the offscreen drawing surfaces if the last view manager
    // has been destroyed and there is something to cleanup

    // Note: A global rendering context is needed because it is not possible 
    // to create a nsIRenderingContext during the shutdown of XPCOM. The last
    // viewmanager is typically destroyed during XPCOM shutdown.

    if (gCleanupContext) {

      gCleanupContext->DestroyCachedBackbuffer();
    } else {
      NS_ASSERTION(PR_FALSE, "Cleanup of drawing surfaces + offscreen buffer failed");
    }

    NS_IF_RELEASE(gCleanupContext);
  }

  mObserver = nsnull;
  mContext = nsnull;

  if (nsnull != mCompositeListeners) {
    mCompositeListeners->Clear();
    NS_RELEASE(mCompositeListeners);
  }
}

NS_IMPL_ISUPPORTS1(nsViewManager, nsIViewManager)

nsresult
nsViewManager::CreateRegion(nsIRegion* *result)
{
  nsresult rv;

  if (!mRegionFactory) {
    nsCOMPtr<nsIComponentManager> compMgr;
    rv = NS_GetComponentManager(getter_AddRefs(compMgr));

    if (NS_SUCCEEDED(rv))
      rv = compMgr->GetClassObject(kRegionCID,
                                   NS_GET_IID(nsIFactory),
                                   getter_AddRefs(mRegionFactory));

    if (!mRegionFactory) {
      *result = nsnull;
      return NS_ERROR_FAILURE;
    }
  }

  nsIRegion* region = nsnull;
  rv = mRegionFactory->CreateInstance(nsnull, NS_GET_IID(nsIRegion), (void**)&region);
  if (NS_SUCCEEDED(rv)) {
    rv = region->Init();
    *result = region;
  }
  return rv;
}

// We don't hold a reference to the presentation context because it
// holds a reference to us.
NS_IMETHODIMP nsViewManager::Init(nsIDeviceContext* aContext)
{
  NS_PRECONDITION(nsnull != aContext, "null ptr");

  if (nsnull == aContext) {
    return NS_ERROR_NULL_POINTER;
  }
  if (nsnull != mContext) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }
  mContext = aContext;
  mTwipsToPixels = mContext->AppUnitsToDevUnits();
  mPixelsToTwips = mContext->DevUnitsToAppUnits();

  mRefreshEnabled = PR_TRUE;

  mMouseGrabber = nsnull;
  mKeyGrabber = nsnull;

  if (nsnull == mEventQueueService) {
    mEventQueueService = do_GetService(kEventQueueServiceCID);
    NS_ASSERTION(mEventQueueService, "couldn't get event queue service");
  }
  
  return NS_OK;
}

NS_IMETHODIMP nsViewManager::GetRootView(nsIView *&aView)
{
  aView = mRootView;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager::SetRootView(nsIView *aView)
{
  nsView* view = NS_STATIC_CAST(nsView*, aView);

  // Do NOT destroy the current root view. It's the caller's responsibility
  // to destroy it
  mRootView = view;

  if (mRootView) {
    nsView* parent = mRootView->GetParent();
    if (parent) {
      parent->InsertChild(mRootView, nsnull);
    }

    mRootView->SetZIndex(PR_FALSE, 0, PR_FALSE);
  }

  return NS_OK;
}

NS_IMETHODIMP nsViewManager::GetWindowDimensions(nscoord *aWidth, nscoord *aHeight)
{
  if (nsnull != mRootView) {
    nsRect dim;
    mRootView->GetDimensions(dim);
    *aWidth = dim.width;
    *aHeight = dim.height;
  }
  else
    {
      *aWidth = 0;
      *aHeight = 0;
    }
  return NS_OK;
}

NS_IMETHODIMP nsViewManager::SetWindowDimensions(nscoord aWidth, nscoord aHeight)
{
  // Resize the root view
  if (nsnull != mRootView) {
    nsRect dim(0, 0, aWidth, aHeight);
    mRootView->SetDimensions(dim);
  }

  //printf("new dims: %d %d\n", aWidth, aHeight);
  // Inform the presentation shell that we've been resized
  if (nsnull != mObserver)
    mObserver->ResizeReflow(mRootView, aWidth, aHeight);
  //printf("reflow done\n");

  return NS_OK;
}

NS_IMETHODIMP nsViewManager::ResetScrolling(void)
{
  if (nsnull != mRootScrollable)
    mRootScrollable->ComputeScrollOffsets(PR_TRUE);

  return NS_OK;
}

/* Check the prefs to see whether we should do double buffering or not... */
static
PRBool DoDoubleBuffering(void)
{
  static PRBool gotDoublebufferPrefs = PR_FALSE;
  static PRBool doDoublebuffering    = PR_TRUE;  /* Double-buffering is ON by default */
  
  if (!gotDoublebufferPrefs) {
    nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefBranch) {
      PRBool val;
      if (NS_SUCCEEDED(prefBranch->GetBoolPref("viewmanager.do_doublebuffering", &val))) {
        doDoublebuffering = val;
      }
    }

#ifdef DEBUG
    if (!doDoublebuffering) {
      printf("nsViewManager: Note: Double-buffering disabled via prefs.\n");
    }
#endif /* DEBUG */

    gotDoublebufferPrefs = PR_TRUE;
  }
  
  return doDoublebuffering;
}

static void ConvertNativeRegionToAppRegion(nsIRegion* aIn, nsRegion* aOut,
                                           nsIDeviceContext* context)
{
  nsRegionRectSet* rects = nsnull;
  aIn->GetRects(&rects);
  if (!rects)
    return;
  
  float  p2t;
  p2t = context->DevUnitsToAppUnits();

  PRUint32 i;
  for (i = 0; i < rects->mNumRects; i++) {
    const nsRegionRect& inR = rects->mRects[i];
    nsRect outR;
    outR.x = NSToIntRound(inR.x * p2t);
    outR.y = NSToIntRound(inR.y * p2t);
    outR.width = NSToIntRound(inR.width * p2t);
    outR.height = NSToIntRound(inR.height * p2t);
    aOut->Or(*aOut, outR);
  }

  aIn->FreeRects(rects);
}

/**
   aRegion is given in device coordinates!!
*/
void nsViewManager::Refresh(nsView *aView, nsIRenderingContext *aContext,
                            nsIRegion *aRegion, PRUint32 aUpdateFlags)
{
  NS_ASSERTION(aRegion != nsnull, "Null aRegion");

  if (PR_FALSE == mRefreshEnabled)
    return;

  nsRect viewRect;
  aView->GetDimensions(viewRect);

  // damageRegion is the damaged area, in twips, relative to the view origin
  nsRegion damageRegion;
  // convert pixels-relative-to-widget-origin to twips-relative-to-widget-origin
  ConvertNativeRegionToAppRegion(aRegion, &damageRegion, mContext);
  // move it from widget coordinates into view coordinates
  damageRegion.MoveBy(viewRect.x, viewRect.y);

  // Clip it to the view; shouldn't be necessary, but do it for sanity
  damageRegion.And(damageRegion, viewRect);
  if (damageRegion.IsEmpty()) {
#ifdef DEBUG_roc
    nsRect damageRect = damageRegion.GetBounds();
    printf("XXX Damage rectangle (%d,%d,%d,%d) does not intersect the widget's view (%d,%d,%d,%d)!\n",
           damageRect.x, damageRect.y, damageRect.width, damageRect.height,
           viewRect.x, viewRect.y, viewRect.width, viewRect.height);
#endif
    return;
  }

#ifdef NS_VM_PERF_METRICS
  MOZ_TIMER_DEBUGLOG(("Reset nsViewManager::Refresh(region), this=%p\n", this));
  MOZ_TIMER_RESET(mWatch);

  MOZ_TIMER_DEBUGLOG(("Start: nsViewManager::Refresh(region)\n"));
  MOZ_TIMER_START(mWatch);
#endif

  NS_ASSERTION(!mPainting, "recursive painting not permitted");
  if (mPainting) {
    mRecursiveRefreshPending = PR_TRUE;
    return;
  }  
  mPainting = PR_TRUE;

  // force double buffering in general
  aUpdateFlags |= NS_VMREFRESH_DOUBLE_BUFFER;

  if (!DoDoubleBuffering())
    aUpdateFlags &= ~NS_VMREFRESH_DOUBLE_BUFFER;

  // check if the rendering context wants double-buffering or not
  if (aContext) {
    PRBool contextWantsBackBuffer = PR_TRUE;
    aContext->UseBackbuffer(&contextWantsBackBuffer);
    if (!contextWantsBackBuffer)
      aUpdateFlags &= ~NS_VMREFRESH_DOUBLE_BUFFER;
  }
  
  if (PR_FALSE == mAllowDoubleBuffering) {
    // Turn off double-buffering of the display
    aUpdateFlags &= ~NS_VMREFRESH_DOUBLE_BUFFER;
  }

  nsCOMPtr<nsIRenderingContext> localcx;
  nsIDrawingSurface*    ds = nsnull;

  if (nsnull == aContext)
    {
      localcx = CreateRenderingContext(*aView);

      //couldn't get rendering context. this is ok at init time atleast
      if (nsnull == localcx) {
        mPainting = PR_FALSE;
        return;
      }
    } else {
      // plain assignment grabs another reference.
      localcx = aContext;
    }

  // notify the listeners.
  if (nsnull != mCompositeListeners) {
    PRUint32 listenerCount;
    if (NS_SUCCEEDED(mCompositeListeners->Count(&listenerCount))) {
      nsCOMPtr<nsICompositeListener> listener;
      for (PRUint32 i = 0; i < listenerCount; i++) {
        if (NS_SUCCEEDED(mCompositeListeners->QueryElementAt(i, NS_GET_IID(nsICompositeListener), getter_AddRefs(listener)))) {
          listener->WillRefreshRegion(this, aView, aContext, aRegion, aUpdateFlags);
        }
      }
    }
  }

  // damageRect is the clipped damage area bounds, in twips-relative-to-view-origin
  nsRect damageRect = damageRegion.GetBounds();
  // widgetDamageRectInPixels is the clipped damage area bounds,
  // in pixels-relative-to-widget-origin
  nsRect widgetDamageRectInPixels = damageRect;
  widgetDamageRectInPixels.MoveBy(-viewRect.x, -viewRect.y);
  float t2p;
  t2p = mContext->AppUnitsToDevUnits();
  widgetDamageRectInPixels.ScaleRoundOut(t2p);

  // On the Mac, we normally turn doublebuffering off because Quartz is
  // doublebuffering for us. But we need to turn it on anyway if we need
  // to use our blender, which requires access to the "current pixel values"
  // when it blends onto the canvas.
  nsAutoVoidArray displayList;
  PLArenaPool displayArena;
  PL_INIT_ARENA_POOL(&displayArena, "displayArena", 1024);
  PRBool anyTransparentPixels
    = BuildRenderingDisplayList(aView, damageRegion, &displayList, displayArena);
  if (!(aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER)) {
    for (PRInt32 i = 0; i < displayList.Count(); i++) {
      DisplayListElement2* element = NS_STATIC_CAST(DisplayListElement2*, displayList.ElementAt(i));
      if (element->mFlags & PUSH_FILTER) {
        aUpdateFlags |= NS_VMREFRESH_DOUBLE_BUFFER;
        break;
      }
    }
  } 

  if (aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER)
  {
    nsRect maxWidgetSize;
    GetMaxWidgetBounds(maxWidgetSize);

    nsRect r(0, 0, widgetDamageRectInPixels.width, widgetDamageRectInPixels.height);
    if (NS_FAILED(localcx->GetBackbuffer(r, maxWidgetSize, ds))) {
      //Failed to get backbuffer so turn off double buffering
      aUpdateFlags &= ~NS_VMREFRESH_DOUBLE_BUFFER;
    }
  }

  // painting will be done in aView's coordinates
  PRBool usingDoubleBuffer = (aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER) && ds;
  if (usingDoubleBuffer) {
    // Adjust translations because the backbuffer holds just the damaged area,
    // not the whole widget

    // RenderViews draws in view coordinates. We want (damageRect.x, damageRect.y)
    // to be translated to (0,0) in the backbuffer. So:
    localcx->Translate(-damageRect.x, -damageRect.y);
    // We're going to reset the clip region for the backbuffer. We can't
    // just use damageRegion because nsIRenderingContext::SetClipRegion doesn't
    // translate/scale the coordinates (grrrrrrrrrr)
    // So we have to translate the region before we use it. aRegion is in
    // pixels-relative-to-widget-origin, so:
    aRegion->Offset(-widgetDamageRectInPixels.x, -widgetDamageRectInPixels.y);
  } else {
    // RenderViews draws in view coordinates. We want (viewRect.x, viewRect.y)
    // to be translated to (0,0) in the widget. So:
    localcx->Translate(-viewRect.x, -viewRect.y);
  }

  // Note that nsIRenderingContext::SetClipRegion always works in pixel coordinates,
  // and nsIRenderingContext::SetClipRect always works in app coordinates. Stupid huh?
  // Also, SetClipRegion doesn't subject its argument to the current transform, but
  // SetClipRect does.
  localcx->SetClipRegion(*aRegion, nsClipCombine_kReplace);
  localcx->SetClipRect(damageRect, nsClipCombine_kIntersect);

  if (anyTransparentPixels) {
    // There are some bits here that aren't going to be completely painted unless we do it now.
    // XXX Which color should we use for these bits?
    localcx->SetColor(NS_RGB(128, 128, 128));
    localcx->FillRect(damageRegion.GetBounds());
  }
  RenderViews(aView, *localcx, damageRegion, ds, displayList);
  PL_FreeArenaPool(&displayArena);
  PL_FinishArenaPool(&displayArena);

  if (usingDoubleBuffer) {
    // Flush bits back to the screen

    // Restore aRegion to pixels-relative-to-widget-origin
    aRegion->Offset(widgetDamageRectInPixels.x, widgetDamageRectInPixels.y);
    // Restore translation to its previous state (so that (0,0) is the widget origin)
    localcx->Translate(damageRect.x, damageRect.y);
    // Make damageRect twips-relative-to-widget-origin
    damageRect.MoveBy(-viewRect.x, -viewRect.y);
    // Reset clip region to widget-relative
    localcx->SetClipRegion(*aRegion, nsClipCombine_kReplace);
    localcx->SetClipRect(damageRect, nsClipCombine_kIntersect);
    // neither source nor destination are transformed
    localcx->CopyOffScreenBits(ds, 0, 0, widgetDamageRectInPixels, NS_COPYBITS_USE_SOURCE_CLIP_REGION);
  } else {
    // undo earlier translation
    localcx->Translate(viewRect.x, viewRect.y);
  }

  mPainting = PR_FALSE;

  // notify the listeners.
  if (nsnull != mCompositeListeners) {
    PRUint32 listenerCount;
    if (NS_SUCCEEDED(mCompositeListeners->Count(&listenerCount))) {
      nsCOMPtr<nsICompositeListener> listener;
      for (PRUint32 i = 0; i < listenerCount; i++) {
        if (NS_SUCCEEDED(mCompositeListeners->QueryElementAt(i, NS_GET_IID(nsICompositeListener), getter_AddRefs(listener)))) {
          listener->DidRefreshRegion(this, aView, aContext, aRegion, aUpdateFlags);
        }
      }
    }
  }

  if (mRecursiveRefreshPending) {
    UpdateAllViews(aUpdateFlags);
    mRecursiveRefreshPending = PR_FALSE;
  }

  localcx->ReleaseBackbuffer();

#ifdef NS_VM_PERF_METRICS
  MOZ_TIMER_DEBUGLOG(("Stop: nsViewManager::Refresh(region), this=%p\n", this));
  MOZ_TIMER_STOP(mWatch);
  MOZ_TIMER_LOG(("vm2 Paint time (this=%p): ", this));
  MOZ_TIMER_PRINT(mWatch);
#endif

}

void nsViewManager::DefaultRefresh(nsView* aView, const nsRect* aRect)
{
  NS_PRECONDITION(aView, "Must have a view to work with!");
  nsIWidget* widget = aView->GetNearestWidget(nsnull);
  if (! widget)
    return;

  nsCOMPtr<nsIRenderingContext> context = CreateRenderingContext(*aView);

  if (! context)
    return;

  nscolor bgcolor = mDefaultBackgroundColor;

  if (NS_GET_A(mDefaultBackgroundColor) == 0) {
    NS_WARNING("nsViewManager: Asked to paint a default background, but no default background color is set!");
    return;
  }

  context->SetColor(bgcolor);
  context->FillRect(*aRect);
}

// Perform a *stable* sort of the buffer by increasing Z-index. The common case is
// when many or all z-indices are equal and the list is mostly sorted; make sure
// that's fast (should be linear time if all z-indices are equal).
static void ApplyZOrderStableSort(nsVoidArray &aBuffer, nsVoidArray &aMergeTmp, PRInt32 aStart, PRInt32 aEnd) {
  if (aEnd - aStart <= 6) {
    // do a fast bubble sort for the small sizes
    for (PRInt32 i = aEnd - 1; i > aStart; i--) {
      PRBool sorted = PR_TRUE;
      for (PRInt32 j = aStart; j < i; j++) {
        DisplayListElement2* e1 = NS_STATIC_CAST(DisplayListElement2*, aBuffer.ElementAt(j));
        DisplayListElement2* e2 = NS_STATIC_CAST(DisplayListElement2*, aBuffer.ElementAt(j + 1));
        if (e1->mZIndex > e2->mZIndex) {
          sorted = PR_FALSE;
          // We could use aBuffer.MoveElement(), but it wouldn't be much of
          // a win if any for swapping two elements.
          aBuffer.ReplaceElementAt(e2, j);
          aBuffer.ReplaceElementAt(e1, j + 1);
        }
      }
      if (sorted) {
        return;
      }
    }
  } else {
    // merge sort for the rest
    PRInt32 mid = (aEnd + aStart)/2;

    ApplyZOrderStableSort(aBuffer, aMergeTmp, aStart, mid);
    ApplyZOrderStableSort(aBuffer, aMergeTmp, mid, aEnd);

    DisplayListElement2* e1 = NS_STATIC_CAST(DisplayListElement2*, aBuffer.ElementAt(mid - 1));
    DisplayListElement2* e2 = NS_STATIC_CAST(DisplayListElement2*, aBuffer.ElementAt(mid));

    // fast common case: the list is already completely sorted
    if (e1->mZIndex <= e2->mZIndex) {
      return;
    }
    // we have some merging to do.

    PRInt32 i1 = aStart;
    PRInt32 i2 = mid;

    e1 = NS_STATIC_CAST(DisplayListElement2*, aBuffer.ElementAt(i1));
    e2 = NS_STATIC_CAST(DisplayListElement2*, aBuffer.ElementAt(i2));
    while (i1 < mid || i2 < aEnd) {
      if (i1 < mid && (i2 == aEnd || e1->mZIndex <= e2->mZIndex)) {
        aMergeTmp.AppendElement(e1);
        i1++;
        if (i1 < mid) {
          e1 = NS_STATIC_CAST(DisplayListElement2*, aBuffer.ElementAt(i1));
        }
      } else {
        aMergeTmp.AppendElement(e2);
        i2++;
        if (i2 < aEnd) {
          e2 = NS_STATIC_CAST(DisplayListElement2*, aBuffer.ElementAt(i2));
        }
      }
    }

    for (PRInt32 i = aStart; i < aEnd; i++) {
      aBuffer.ReplaceElementAt(aMergeTmp.ElementAt(i - aStart), i);
    }

    aMergeTmp.Clear();
  }
}

static nsInt64 BuildExtendedZIndex(nsView* aView) {
  return (nsInt64(aView->GetZIndex()) << 1) | nsInt64(aView->IsTopMost() ? 1 : 0);
}

// The display-element (indirect) children of aNode are extracted and appended to aBuffer in
// z-order, with the bottom-most elements first.
// Their z-index is set to the z-index they will have in aNode's parent.
// I.e. if aNode's view has "z-index: auto", the nodes will keep their z-index, otherwise
// their z-indices will all be equal to the z-index value of aNode's view.
static void SortByZOrder(DisplayZTreeNode *aNode, nsVoidArray &aBuffer, nsVoidArray &aMergeTmp,
                         PRBool aForceSort, PLArenaPool &aPool)
{
  PRBool autoZIndex = PR_TRUE;
  nsInt64 explicitZIndex = 0;

  if (nsnull != aNode->mView) {
    // Hixie says only non-translucent elements can have z-index:auto
    autoZIndex = aNode->mView->GetZIndexIsAuto() && aNode->mView->GetOpacity() == 1.0f;
    explicitZIndex = BuildExtendedZIndex(aNode->mView);
  }

  if (nsnull == aNode->mZChild) {
    if (nsnull != aNode->mDisplayElement) {
      aBuffer.AppendElement(aNode->mDisplayElement);
      aNode->mDisplayElement->mZIndex = explicitZIndex;
      aNode->mDisplayElement = nsnull;
    }
    return;
  }

  DisplayZTreeNode *child;
  PRInt32 childStartIndex = aBuffer.Count();
  for (child = aNode->mZChild; nsnull != child; child = child->mZSibling) {
    SortByZOrder(child, aBuffer, aMergeTmp, PR_FALSE, aPool);
  }
  PRInt32 childEndIndex = aBuffer.Count();
  PRInt32 sortStartIndex = childStartIndex;
  PRInt32 sortEndIndex = childEndIndex;
  PRBool hasClip = PR_FALSE;
  DisplayListElement2* ePush = nsnull;
  DisplayListElement2* ePop = nsnull;

  // When we sort the children by z-index, don't sort any PUSH_ or POP_ instructions
  // which are bracketing the children.
  while (sortEndIndex - sortStartIndex >= 2) {
    DisplayListElement2* childElem =
      NS_STATIC_CAST(DisplayListElement2*, aBuffer.ElementAt(sortStartIndex));
    if (childElem->mView == aNode->mView) {
      if (childElem->mFlags & PUSH_CLIP) {
        hasClip = PR_TRUE;
        // remember where the push and pop instructions are so we can
        // duplicate them later, if necessary
        ePush = NS_STATIC_CAST(DisplayListElement2*, aBuffer.ElementAt(sortStartIndex));
        ePop = NS_STATIC_CAST(DisplayListElement2*, aBuffer.ElementAt(sortEndIndex - 1));
        sortStartIndex++;
        sortEndIndex--;
      } else if (childElem->mFlags & PUSH_FILTER) {
        NS_ASSERTION(!autoZIndex, "FILTER cannot apply to z-index:auto view");
        sortStartIndex++;
        sortEndIndex--;
      } else {
        break;
      }
    } else {
      break;
    }
  }

  if (hasClip) {
    ApplyZOrderStableSort(aBuffer, aMergeTmp, sortStartIndex, sortEndIndex);
    
    if (autoZIndex && sortEndIndex - sortStartIndex >= 1) {
      // If we're an auto-z-index, then we have to worry about the possibility that some of
      // our children may be moved by the z-sorter beyond the bounds of the PUSH...POP clip
      // instructions. So basically, we ensure that around every group of children of
      // equal z-index, there is a PUSH...POP element pair with the same z-index. The stable
      // z-sorter will not break up such a group.
      // Note that if we're not an auto-z-index set, then our children will never be broken
      // up so we don't need to do this.
      // We also don't have to worry if we have no real children.
      // We don't have to do the same thing for PUSH_FILTER/POP_FILTER because
      // a filter always corresponds to non-auto z-index; there is no way children
      // can be sorted beyond the PUSH/POP instructions.
      DisplayListElement2* eFirstChild = NS_STATIC_CAST(DisplayListElement2*, aBuffer.ElementAt(sortStartIndex));

      ePush->mZIndex = eFirstChild->mZIndex;

      DisplayListElement2* eLastChild = NS_STATIC_CAST(DisplayListElement2*, aBuffer.ElementAt(sortEndIndex - 1));

      ePop->mZIndex = eLastChild->mZIndex;

      DisplayListElement2* e = eFirstChild;
      for (PRInt32 i = sortStartIndex; i < sortEndIndex - 1; i++) {
        DisplayListElement2* eNext = NS_STATIC_CAST(DisplayListElement2*, aBuffer.ElementAt(i + 1));
        NS_ASSERTION(e->mZIndex <= eNext->mZIndex, "Display Z-list is not sorted!!");
        if (e->mZIndex != eNext->mZIndex) {
          // need to insert a POP for the last sequence and a PUSH for the next sequence
          DisplayListElement2 *newPop, *newPush;

          ARENA_ALLOCATE(newPop, &aPool, DisplayListElement2);
          ARENA_ALLOCATE(newPush, &aPool, DisplayListElement2);

          *newPop = *ePop;
          newPop->mZIndex = e->mZIndex;
          *newPush = *ePush;
          newPush->mZIndex = eNext->mZIndex;
          aBuffer.InsertElementAt(newPop, i + 1);
          aBuffer.InsertElementAt(newPush, i + 2);
          i += 2;
          childEndIndex += 2;
          sortEndIndex += 2;
        }
        e = eNext;
      }
    }
  } else if (aForceSort || !autoZIndex) {
    ApplyZOrderStableSort(aBuffer, aMergeTmp, sortStartIndex, sortEndIndex);
  }

  for (PRInt32 i = childStartIndex; i < childEndIndex; i++) {
    DisplayListElement2* element = NS_STATIC_CAST(DisplayListElement2*, aBuffer.ElementAt(i));
    if (!autoZIndex) {
      element->mZIndex = explicitZIndex;
    } else if (aNode->mView->IsTopMost()) {
      // promote children to topmost if this view is topmost
      element->mZIndex |= nsInt64(1);
    }
  }
}

static void PushStateAndClip(nsIRenderingContext** aRCs, PRInt32 aCount, nsRect &aRect) {
  for (int i = 0; i < aCount; i++) {
    if (aRCs[i]) {
      aRCs[i]->PushState();
      aRCs[i]->SetClipRect(aRect, nsClipCombine_kIntersect);
    }
  }
}

static void PopState(nsIRenderingContext **aRCs, PRInt32 aCount) {
  for (int i = 0; i < aCount; i++) {
    if (aRCs[i])
      aRCs[i]->PopState();
  }
}

void nsViewManager::AddCoveringWidgetsToOpaqueRegion(nsRegion &aRgn, nsIDeviceContext* aContext,
                                                     nsView* aRootView) {
  NS_PRECONDITION(aRootView, "Must have root view");
  
  // We accumulate the bounds of widgets obscuring aRootView's widget into opaqueRgn.
  // In OptimizeDisplayList, display list elements which lie behind obscuring native
  // widgets are dropped.
  // This shouldn't really be necessary, since the GFX/Widget toolkit should remove these
  // covering widgets from the clip region passed into the paint command. But right now
  // they only give us a paint rect and not a region, so we can't access that information.
  // It's important to identifying areas that are covered by native widgets to avoid
  // painting their views many times as we process invalidates from the root widget all the
  // way down to the nested widgets.
  // 
  // NB: we must NOT add widgets that correspond to floating views!
  // We may be required to paint behind them
  aRgn.SetEmpty();

  nsIWidget* widget = aRootView->GetNearestWidget(nsnull);
  if (!widget) {
    return;
  }

  for (nsIWidget* childWidget = widget->GetFirstChild();
       childWidget;
       childWidget = childWidget->GetNextSibling()) {
    PRBool widgetVisible;
    childWidget->IsVisible(widgetVisible);
    if (widgetVisible) {
      nsView* view = nsView::GetViewFor(childWidget);
      if (view && view->GetVisibility() == nsViewVisibility_kShow
          && !view->GetFloating()) {
        nsRect bounds = view->GetBounds();
        if (bounds.width > 0 && bounds.height > 0) {
          nsView* viewParent = view->GetParent();
            
          while (viewParent && viewParent != aRootView) {
            viewParent->ConvertToParentCoords(&bounds.x, &bounds.y);
            viewParent = viewParent->GetParent();
          }
            
          // maybe we couldn't get the view into the coordinate
          // system of aRootView (maybe it's not a descendant
          // view of aRootView?); if so, don't use it
          if (viewParent) {
            aRgn.Or(aRgn, bounds);
          }
        }
      }
    }
  }
}

PRBool nsViewManager::BuildRenderingDisplayList(nsIView* aRootView,
  const nsRegion& aRegion, nsVoidArray* aDisplayList, PLArenaPool &aPool)
{
  BuildDisplayList(NS_STATIC_CAST(nsView*, aRootView),
                   aRegion.GetBounds(), PR_FALSE, PR_FALSE, aDisplayList, aPool);

  nsRegion opaqueRgn;
  AddCoveringWidgetsToOpaqueRegion(opaqueRgn, mContext,
                                   NS_STATIC_CAST(nsView*, aRootView));

  nsRect finalTransparentRect;
  OptimizeDisplayList(aDisplayList, aRegion, finalTransparentRect, opaqueRgn, PR_FALSE);

#ifdef DEBUG_roc
  if (!finalTransparentRect.IsEmpty()) {
    printf("XXX: Using final transparent rect, x=%d, y=%d, width=%d, height=%d\n",
           finalTransparentRect.x, finalTransparentRect.y, finalTransparentRect.width, finalTransparentRect.height);
  }
#endif

  return !finalTransparentRect.IsEmpty();
}

/*
  aRCSurface is the drawing surface being used to double-buffer aRC, or null
  if no double-buffering is happening. We pass this in here so that we can
  blend directly into the double-buffer offscreen memory.
*/
void nsViewManager::RenderViews(nsView *aRootView, nsIRenderingContext& aRC,
                                const nsRegion& aRegion, nsIDrawingSurface* aRCSurface,
                                const nsVoidArray& aDisplayList)
{
#ifdef DEBUG_roc
  if (getenv("MOZ_SHOW_DISPLAY_LIST")) ShowDisplayList(&aDisplayList);
#endif

  PRInt32 index = 0;
  nsRect fakeClipRect;
  PRBool anyRendered;
  OptimizeDisplayListClipping(&aDisplayList, PR_FALSE, fakeClipRect, index, anyRendered);

  index = 0;
  OptimizeTranslucentRegions(aDisplayList, &index, nsnull);

  nsIWidget* widget = aRootView->GetWidget();
  PRBool translucentWindow = PR_FALSE;
  if (widget) {
    widget->GetWindowTranslucency(translucentWindow);
    if (translucentWindow) {
      NS_WARNING("Transparent window enabled");
      NS_ASSERTION(aRCSurface, "Cannot support transparent windows with doublebuffering disabled");
    }
  }

  // Create a buffer wrapping aRC (which is usually the double-buffering offscreen buffer).
  BlendingBuffers* buffers =
    CreateBlendingBuffers(&aRC, PR_TRUE, aRCSurface, translucentWindow, aRegion.GetBounds());
  NS_ASSERTION(buffers, "Failed to create rendering buffers");
  if (!buffers)
    return;

  nsAutoVoidArray filterStack;

  // draw all views in the display list, from back to front.
  for (PRInt32 i = 0; i < aDisplayList.Count(); i++) {
    DisplayListElement2* element = NS_STATIC_CAST(DisplayListElement2*, aDisplayList.ElementAt(i));

    nsIRenderingContext* RCs[2] = { buffers->mBlackCX, buffers->mWhiteCX };

    if (element->mFlags & PUSH_CLIP) {
      PushStateAndClip(RCs, 2, element->mBounds);
    }
    if (element->mFlags & PUSH_FILTER) {
      NS_ASSERTION(aRCSurface,
                   "Cannot support translucent elements with doublebuffering disabled");

      // Save current buffer on the stack and start rendering into a new
      // offscreen buffer
      filterStack.AppendElement(buffers);
      buffers = CreateBlendingBuffers(&aRC, PR_FALSE, nsnull,
                                      (element->mFlags & VIEW_TRANSPARENT) != 0,
                                      element->mBounds);
    }

    if (element->mFlags & VIEW_RENDERED) {
      if (element->mFlags & VIEW_CLIPPED) {
        PushStateAndClip(RCs, 2, element->mBounds);
      }
      
      RenderDisplayListElement(element, RCs[0]);
      // RenderDisplayListElement won't do anything if the context is null
      RenderDisplayListElement(element, RCs[1]);

      if (element->mFlags & VIEW_CLIPPED) {
        PopState(RCs, 2);
      }
    }

    if (element->mFlags & POP_FILTER) {
      // Pop the last buffer off the stack and composite the current buffer into
      // the last buffer
      BlendingBuffers* doneBuffers = buffers;
      buffers = NS_STATIC_CAST(BlendingBuffers*,
                               filterStack.ElementAt(filterStack.Count() - 1));
      filterStack.RemoveElementAt(filterStack.Count() - 1);

      // perform the blend itself.
      nsRect damageRectInPixels = element->mBounds;
      damageRectInPixels -= buffers->mOffset;
      damageRectInPixels *= mTwipsToPixels;
      if (damageRectInPixels.width > 0 && damageRectInPixels.height > 0) {
        nsIRenderingContext* targets[2] = { buffers->mBlackCX, buffers->mWhiteCX };
        for (int j = 0; j < 2; j++) {
          if (targets[j]) {
            mBlender->Blend(0, 0,
                            damageRectInPixels.width, damageRectInPixels.height,
                            doneBuffers->mBlackCX, targets[j],
                            damageRectInPixels.x, damageRectInPixels.y,
                            element->mView->GetOpacity(), doneBuffers->mWhiteCX,
                            NS_RGB(0, 0, 0), NS_RGB(255, 255, 255));
          }
        }
      }
      // probably should recycle these so we don't eat the cost of graphics memory
      // allocation
      delete doneBuffers;
    }
    if (element->mFlags & POP_CLIP) {
      PopState(RCs, 2);
    }
      
    // The element is destroyed when the pool is finished
    // delete element;
  }
    
  if (translucentWindow) {
    // Get the alpha channel into an array so we can send it to the widget
    nsRect r = aRegion.GetBounds();
    r *= mTwipsToPixels;
    nsRect bufferRect(0, 0, r.width, r.height);
    PRUint8* alphas = nsnull;
    nsresult rv = mBlender->GetAlphas(bufferRect, buffers->mBlack,
                                      buffers->mWhite, &alphas);
    
    if (NS_SUCCEEDED(rv)) {
      widget->UpdateTranslucentWindowAlpha(r, alphas);
    }
    delete[] alphas;
  }

  delete buffers;
}

void nsViewManager::RenderDisplayListElement(DisplayListElement2* element,
                                             nsIRenderingContext* aRC) {
  if (!aRC)
    return;
  
  PRBool clipEmpty;
  nsRect r;
  nsView* view = element->mView;

  view->GetDimensions(r);

  aRC->PushState();

  nscoord x = element->mAbsX - r.x, y = element->mAbsY - r.y;
  aRC->Translate(x, y);

  nsRect drect(element->mBounds.x - x, element->mBounds.y - y,
               element->mBounds.width, element->mBounds.height);
  
  element->mView->Paint(*aRC, drect, 0, clipEmpty);
  
  aRC->PopState();
}

void nsViewManager::PaintView(nsView *aView, nsIRenderingContext &aRC, nscoord x, nscoord y,
                              const nsRect &aDamageRect)
{
  aRC.PushState();
  aRC.Translate(x, y);
  PRBool unused;
  aView->Paint(aRC, aDamageRect, 0, unused);
  aRC.PopState();
}

static nsresult NewOffscreenContext(nsIDeviceContext* deviceContext, nsIDrawingSurface* surface,
                                    const nsRect& aRect, nsIRenderingContext* *aResult)
{
  nsresult             rv;
  nsIRenderingContext *context = nsnull;

  rv = deviceContext->CreateRenderingContext(surface, context);
  if (NS_FAILED(rv))
    return rv;

  // always initialize clipping, linux won't draw images otherwise.
  nsRect clip(0, 0, aRect.width, aRect.height);
  context->SetClipRect(clip, nsClipCombine_kReplace);

  context->Translate(-aRect.x, -aRect.y);
  
  *aResult = context;
  return NS_OK;
}

BlendingBuffers::BlendingBuffers(nsIRenderingContext* aCleanupContext) {
  mCleanupContext = aCleanupContext;

  mOwnBlackSurface = PR_FALSE;
  mWhite = nsnull;
  mBlack = nsnull;
}

BlendingBuffers::~BlendingBuffers() {
  if (mWhite)
    mCleanupContext->DestroyDrawingSurface(mWhite);

  if (mBlack && mOwnBlackSurface)
    mCleanupContext->DestroyDrawingSurface(mBlack);
}

/*
@param aBorrowContext set to PR_TRUE if the BlendingBuffers' "black" context
  should be just aRC; set to PR_FALSE if we should create a new offscreen context
@param aBorrowSurface if aBorrowContext is PR_TRUE, then this is the offscreen surface
  corresponding to aRC, or nsnull if aRC doesn't have one; if aBorrowContext is PR_FALSE,
  this parameter is ignored
@param aNeedAlpha set to PR_FALSE if the caller guarantees that every pixel of the
  BlendingBuffers will be drawn with opacity 1.0, PR_TRUE otherwise
@param aRect the screen rectangle covered by the new BlendingBuffers, in app units, and
  relative to the origin of aRC
*/
BlendingBuffers*
nsViewManager::CreateBlendingBuffers(nsIRenderingContext *aRC,
                                     PRBool aBorrowContext,
                                     nsIDrawingSurface* aBorrowSurface,
                                     PRBool aNeedAlpha,
                                     const nsRect& aRect)
{
  nsresult rv;

  // create a blender, if none exists already.
  if (!mBlender) {
    mBlender = do_CreateInstance(kBlenderCID, &rv);
    if (NS_FAILED(rv))
      return nsnull;
    rv = mBlender->Init(mContext);
    if (NS_FAILED(rv))
      return nsnull;
  }

  BlendingBuffers* buffers = new BlendingBuffers(aRC);
  if (!buffers)
    return nsnull;

  buffers->mOffset = nsPoint(aRect.x, aRect.y);

  nsRect offscreenBounds(0, 0, aRect.width, aRect.height);
  offscreenBounds.ScaleRoundOut(mTwipsToPixels);

  if (aBorrowContext) {
    buffers->mBlackCX = aRC;
    buffers->mBlack = aBorrowSurface;
  } else {
    rv = aRC->CreateDrawingSurface(offscreenBounds, NS_CREATEDRAWINGSURFACE_FOR_PIXEL_ACCESS, buffers->mBlack);
    if (NS_FAILED(rv)) {
      delete buffers;
      return nsnull;
    }
    buffers->mOwnBlackSurface = PR_TRUE;
    
    rv = NewOffscreenContext(mContext, buffers->mBlack, aRect, getter_AddRefs(buffers->mBlackCX));
    if (NS_FAILED(rv)) {
      delete buffers;
      return nsnull;
    }
  }

  if (aNeedAlpha) {
    rv = aRC->CreateDrawingSurface(offscreenBounds, NS_CREATEDRAWINGSURFACE_FOR_PIXEL_ACCESS, buffers->mWhite);
    if (NS_FAILED(rv)) {
      delete buffers;
      return nsnull;
    }
    
    rv = NewOffscreenContext(mContext, buffers->mWhite, aRect, getter_AddRefs(buffers->mWhiteCX));
    if (NS_FAILED(rv)) {
      delete buffers;
      return nsnull;
    }
    
    // Note that we only need to fill mBlackCX with black when some pixels are going
    // to be transparent.
    buffers->mBlackCX->SetColor(NS_RGB(0, 0, 0));
    buffers->mBlackCX->FillRect(aRect);
    buffers->mWhiteCX->SetColor(NS_RGB(255, 255, 255));
    buffers->mWhiteCX->FillRect(aRect);
  }

  return buffers;
}

void nsViewManager::ProcessPendingUpdates(nsView* aView)
{
  // Protect against a null-view.
  if (!aView) {
    return;
  }
  if (aView->HasWidget()) {
    nsCOMPtr<nsIRegion> dirtyRegion;
    aView->GetDirtyRegion(*getter_AddRefs(dirtyRegion));
    if (dirtyRegion && !dirtyRegion->IsEmpty()) {
      aView->GetWidget()->InvalidateRegion(dirtyRegion, PR_FALSE);
      dirtyRegion->Init();
    }
  }

  // process pending updates in child view.
  for (nsView* childView = aView->GetFirstChild(); childView;
       childView = childView->GetNextSibling()) {
    if (childView->GetViewManager() == this) {
      ProcessPendingUpdates(childView);
    }
  }
}

NS_IMETHODIMP nsViewManager::Composite()
{
  if (mUpdateCnt > 0)
    {
      ForceUpdate();
      mUpdateCnt = 0;
    }

  return NS_OK;
}

NS_IMETHODIMP nsViewManager::UpdateView(nsIView *aView, PRUint32 aUpdateFlags)
{
  // Mark the entire view as damaged
  nsView* view = NS_STATIC_CAST(nsView*, aView);

  nsRect bounds = view->GetBounds();
  view->ConvertFromParentCoords(&bounds.x, &bounds.y);
  return UpdateView(view, bounds, aUpdateFlags);
}


// Invalidate all widgets which overlap the view, other than the view's own widgets.
void
nsViewManager::UpdateViewAfterScroll(nsIView *aView, PRInt32 aDX, PRInt32 aDY)
{
  nsView* view = NS_STATIC_CAST(nsView*, aView);

  // Look at the view's clipped rect. It may be that part of the view is clipped out
  // in which case we don't need to worry about invalidating the clipped-out part.
  nsRect damageRect = view->GetClippedRect();
  if (damageRect.IsEmpty()) {
    return;
  }
  damageRect.MoveBy(ComputeViewOffset(view));

  // if this is a floating view, it isn't covered by any widgets other than
  // its children, which are handled by the widget scroller.
  if (view->GetFloating()) {
    return;
  }

  nsView* realRoot = mRootView;
  while (realRoot->GetParent()) {
    realRoot = realRoot->GetParent();
  }

  UpdateWidgetArea(realRoot, damageRect, view);

  Composite();
}

// Returns true if this view's widget(s) completely cover the rectangle
// The specified rectangle, relative to aWidgetView, is invalidated in every widget child of aWidgetView,
// plus aWidgetView's own widget
// If non-null, the aIgnoreWidgetView's widget and its children are not updated.
PRBool nsViewManager::UpdateWidgetArea(nsView *aWidgetView, const nsRect &aDamagedRect, nsView* aIgnoreWidgetView)
{
  // If the bounds don't overlap at all, there's nothing to do
  nsRect bounds;
  aWidgetView->GetDimensions(bounds);

  PRBool overlap = bounds.IntersectRect(bounds, aDamagedRect);
  if (!overlap) {
    return PR_FALSE;
  }

  // If the widget is hidden, it don't cover nothing
  if (nsViewVisibility_kHide == aWidgetView->GetVisibility()) {
#ifdef DEBUG
    // Assert if view is hidden but widget is visible
    nsIWidget* widget = aWidgetView->GetNearestWidget(nsnull);
    if (widget) {
      PRBool visible;
      widget->IsVisible(visible);
      NS_ASSERTION(!visible, "View is hidden but widget is visible!");
    }
#endif
    return PR_FALSE;
  }

  PRBool noCropping = bounds == aDamagedRect;
  if (aWidgetView == aIgnoreWidgetView) {
    // the widget for aIgnoreWidgetView (and its children) should be treated as already updated.
    // We still need to report whether this widget covers the rectangle.
    return noCropping;
  }

  nsIWidget* widget = aWidgetView->GetNearestWidget(nsnull);
  if (!widget) {
    // The root view or a scrolling view might not have a widget
    // (for example, during printing). We get here when we scroll
    // during printing to show selected options in a listbox, for example.
    return PR_FALSE;
  }

  PRBool childCovers = PR_FALSE;
  for (nsIWidget* childWidget = widget->GetFirstChild();
       childWidget;
       childWidget = childWidget->GetNextSibling()) {
    nsView* view = nsView::GetViewFor(childWidget);
    if (nsnull != view) {
      nsRect damage = bounds;
      nsView* vp = view;
      while (vp != aWidgetView && nsnull != vp) {
        vp->ConvertFromParentCoords(&damage.x, &damage.y);
        vp = vp->GetParent();
      }
            
      if (nsnull != vp) { // vp == nsnull means it's in a different hierarchy so we ignore it
        if (UpdateWidgetArea(view, damage, aIgnoreWidgetView)) {
          childCovers = PR_TRUE;
        }
      }
    }
  }

  if (!childCovers) {
    nsViewManager* vm = aWidgetView->GetViewManager();
    ++vm->mUpdateCnt;

    if (!vm->mRefreshEnabled) {
      // accumulate this rectangle in the view's dirty region, so we can process it later.
      vm->AddRectToDirtyRegion(aWidgetView, bounds);
      vm->mHasPendingInvalidates = PR_TRUE;
    } else {
      ViewToWidget(aWidgetView, aWidgetView, bounds);
      widget->Invalidate(bounds, PR_FALSE);
    }
  }

  return noCropping;
}

NS_IMETHODIMP nsViewManager::UpdateView(nsIView *aView, const nsRect &aRect, PRUint32 aUpdateFlags)
{
  NS_PRECONDITION(nsnull != aView, "null view");

  nsView* view = NS_STATIC_CAST(nsView*, aView);

  // Only Update the rectangle region of the rect that intersects the view's non clipped rectangle
  nsRect clippedRect = view->GetClippedRect();
  if (clippedRect.IsEmpty()) {
    return NS_OK;
  }

  nsRect damagedRect;
  damagedRect.IntersectRect(aRect, clippedRect);

   // If the rectangle is not visible then abort
   // without invalidating. This is a performance 
   // enhancement since invalidating a native widget
   // can be expensive.
   // This also checks for silly request like damagedRect.width = 0 or damagedRect.height = 0
  nsRectVisibility rectVisibility;
  GetRectVisibility(view, damagedRect, 0, &rectVisibility);
  if (rectVisibility != nsRectVisibility_kVisible) {
    return NS_OK;
  }

  // if this is a floating view, it isn't covered by any widgets other than
  // its children. In that case we walk up to its parent widget and use
  // that as the root to update from. This also means we update areas that
  // may be outside the parent view(s), which is necessary for floats.
  if (view->GetFloating()) {
    nsView* widgetParent = view;

    while (!widgetParent->HasWidget()) {
      widgetParent->ConvertToParentCoords(&damagedRect.x, &damagedRect.y);
      widgetParent = widgetParent->GetParent();
    }

    UpdateWidgetArea(widgetParent, damagedRect, nsnull);
  } else {
    damagedRect.MoveBy(ComputeViewOffset(view));

    nsView* realRoot = mRootView;
    while (realRoot->GetParent()) {
      realRoot = realRoot->GetParent();
    }

    UpdateWidgetArea(realRoot, damagedRect, nsnull);
  }

  ++mUpdateCnt;

  if (!mRefreshEnabled) {
    return NS_OK;
  }

  // See if we should do an immediate refresh or wait
  if (aUpdateFlags & NS_VMREFRESH_IMMEDIATE) {
    Composite();
  } 

  return NS_OK;
}

NS_IMETHODIMP nsViewManager::UpdateAllViews(PRUint32 aUpdateFlags)
{
  UpdateViews(mRootView, aUpdateFlags);
  return NS_OK;
}

void nsViewManager::UpdateViews(nsView *aView, PRUint32 aUpdateFlags)
{
  // update this view.
  UpdateView(aView, aUpdateFlags);

  // update all children as well.
  nsView* childView = aView->GetFirstChild();
  while (nsnull != childView)  {
    if (childView->GetViewManager() == this) {
      UpdateViews(childView, aUpdateFlags);
    }
    childView = childView->GetNextSibling();
  }
}

NS_IMETHODIMP nsViewManager::DispatchEvent(nsGUIEvent *aEvent, nsEventStatus *aStatus)
{
  *aStatus = nsEventStatus_eIgnore;

  switch(aEvent->message)
    {
    case NS_SIZE:
      {
        nsView* view = nsView::GetViewFor(aEvent->widget);

        if (nsnull != view)
          {
            nscoord width = ((nsSizeEvent*)aEvent)->windowSize->width;
            nscoord height = ((nsSizeEvent*)aEvent)->windowSize->height;
            width = ((nsSizeEvent*)aEvent)->mWinWidth;
            height = ((nsSizeEvent*)aEvent)->mWinHeight;

            // The root view may not be set if this is the resize associated with
            // window creation

            if (view == mRootView)
              {
                // Convert from pixels to twips
                float p2t;
                p2t = mContext->DevUnitsToAppUnits();

                //printf("resize: (pix) %d, %d\n", width, height);
                SetWindowDimensions(NSIntPixelsToTwips(width, p2t),
                                    NSIntPixelsToTwips(height, p2t));
                *aStatus = nsEventStatus_eConsumeNoDefault;
              }
          }

        break;
      }

    case NS_PAINT:
      {
        nsView *view = nsView::GetViewFor(aEvent->widget);

        if (!view || !mContext)
          break;

        *aStatus = nsEventStatus_eConsumeNoDefault;

        // The rect is in device units, and it's in the coordinate space of its
        // associated window.
        nsCOMPtr<nsIRegion> region = ((nsPaintEvent*)aEvent)->region;
        if (!region) {
          if (NS_FAILED(CreateRegion(getter_AddRefs(region))))
            break;

          const nsRect& damrect = *((nsPaintEvent*)aEvent)->rect;
          region->SetTo(damrect.x, damrect.y, damrect.width, damrect.height);
        }
        
        if (region->IsEmpty())
          break;

        // Refresh the view
        if (mRefreshEnabled) {
          Refresh(view, ((nsPaintEvent*)aEvent)->renderingContext, region,
                  NS_VMREFRESH_DOUBLE_BUFFER);
        } else {
          // since we got an NS_PAINT event, we need to
          // draw something so we don't get blank areas.
          nsRect damRect;
          region->GetBoundingBox(&damRect.x, &damRect.y, &damRect.width, &damRect.height);
          float p2t;
          p2t = mContext->DevUnitsToAppUnits();
          damRect.ScaleRoundOut(p2t);
          DefaultRefresh(view, &damRect);
        
          // Clients like the editor can trigger multiple
          // reflows during what the user perceives as a single
          // edit operation, so it disables view manager
          // refreshing until the edit operation is complete
          // so that users don't see the intermediate steps.
          // 
          // Unfortunately some of these reflows can trigger
          // nsScrollPortView and nsScrollingView Scroll() calls
          // which in most cases force an immediate BitBlt and
          // synchronous paint to happen even if the view manager's
          // refresh is disabled. (Bug 97674)
          //
          // Calling UpdateView() here, is neccessary to add
          // the exposed region specified in the synchronous paint
          // event to  the view's damaged region so that it gets
          // painted properly when refresh is enabled.
          //
          // Note that calling UpdateView() here was deemed
          // to have the least impact on performance, since the
          // other alternative was to make Scroll() post an
          // async paint event for the *entire* ScrollPort or
          // ScrollingView's viewable area. (See bug 97674 for this
          // alternate patch.)
          
          UpdateView(view, damRect, NS_VMREFRESH_NO_SYNC);
        }

        break;
      }

    case NS_CREATE:
    case NS_DESTROY:
    case NS_SETZLEVEL:
    case NS_MOVE:
      /* Don't pass these events through. Passing them through
         causes performance problems on pages with lots of views/frames 
         @see bug 112861 */
      *aStatus = nsEventStatus_eConsumeNoDefault;
      break;


    case NS_DISPLAYCHANGED:

      //Destroy the cached backbuffer to force a new backbuffer
      //be constructed with the appropriate display depth.
      //@see bugzilla bug 6061
      *aStatus = nsEventStatus_eConsumeDoDefault;
      if (gCleanupContext) {
        gCleanupContext->DestroyCachedBackbuffer();
      }
      break;

    case NS_SYSCOLORCHANGED:
      {
        nsView *view = nsView::GetViewFor(aEvent->widget);
        nsCOMPtr<nsIViewObserver> obs;
        GetViewObserver(*getter_AddRefs(obs));
        if (obs) {
          PRBool handled;
          obs->HandleEvent(view, aEvent, aStatus, PR_TRUE, handled);
        }
      }
      break; 

    default:
      {
        if ((NS_IS_MOUSE_EVENT(aEvent) &&
             // Ignore moves that we synthesize.
             NS_STATIC_CAST(nsMouseEvent*,aEvent)->reason ==
               nsMouseEvent::eReal &&
             // Ignore mouse exit and enter (we'll get moves if the user
             // is really moving the mouse) since we get them when we
             // create and destroy widgets.
             aEvent->message != NS_MOUSE_EXIT &&
             aEvent->message != NS_MOUSE_ENTER) ||
            NS_IS_KEY_EVENT(aEvent) ||
            NS_IS_IME_EVENT(aEvent)) {
          gLastUserEventTime = PR_IntervalToMicroseconds(PR_IntervalNow());
        }

        if (aEvent->message == NS_DEACTIVATE) {
          PRBool result;
          GrabMouseEvents(nsnull, result);
          mKeyGrabber = nsnull;
        }

        //Find the view whose coordinates system we're in.
        nsView* baseView = nsView::GetViewFor(aEvent->widget);
        nsView* view = baseView;
        PRBool capturedEvent = PR_FALSE;

        //Find the view to which we're initially going to send the event 
        //for hittesting.
        if (NS_IS_MOUSE_EVENT(aEvent) || NS_IS_DRAG_EVENT(aEvent)) {
          nsView* mouseGrabber = GetMouseEventGrabber();
          if (mouseGrabber) {
            view = mouseGrabber;
            capturedEvent = PR_TRUE;
          }
        }
        else if (NS_IS_KEY_EVENT(aEvent) || NS_IS_IME_EVENT(aEvent)) {
          if (mKeyGrabber) {
            view = mKeyGrabber;
            capturedEvent = PR_TRUE;
          }
        }

        if (nsnull != view) {
          float t2p = mContext->AppUnitsToDevUnits();
          float p2t = mContext->DevUnitsToAppUnits();

          if ((aEvent->message == NS_MOUSE_MOVE &&
               NS_STATIC_CAST(nsMouseEvent*,aEvent)->reason ==
                 nsMouseEvent::eReal) ||
              aEvent->message == NS_MOUSE_ENTER) {
            nsPoint rootOffset(0, 0);
            for (nsView *v = baseView; v != mRootView; v = v->GetParent())
              v->ConvertToParentCoords(&rootOffset.x, &rootOffset.y);

            mMouseLocation.MoveTo(NSTwipsToIntPixels(rootOffset.x, t2p) +
                                    aEvent->point.x,
                                  NSTwipsToIntPixels(rootOffset.y, t2p) +
                                    aEvent->point.y);
#ifdef DEBUG_MOUSE_LOCATION
            if (aEvent->message == NS_MOUSE_ENTER)
              printf("[vm=%p]got mouse enter for %p\n",
                     this, aEvent->widget);
            printf("[vm=%p]setting mouse location to (%d,%d)\n",
                   this, mMouseLocation.x, mMouseLocation.y);
#endif
          } else if (aEvent->message == NS_MOUSE_EXIT) {
            // Although we only care about the mouse moving into an area
            // for which this view manager doesn't receive mouse move
            // events, we don't check which view the mouse exit was for
            // since this seems to vary by platform.  Hopefully this
            // won't matter at all since we'll get the mouse move or
            // enter after the mouse exit when the mouse moves from one
            // of our widgets into another.
            mMouseLocation.MoveTo(NSCOORD_NONE, NSCOORD_NONE);
#ifdef DEBUG_MOUSE_LOCATION
            printf("[vm=%p]got mouse exit for %p\n",
                   this, aEvent->widget);
            printf("[vm=%p]clearing mouse location\n",
                   this);
#endif
          }

          //Calculate the proper offset for the view we're going to
          nsPoint offset(0, 0);

          if (view != baseView) {
            //Get offset from root of baseView
            nsView *parent;
            for (parent = baseView; parent; parent = parent->GetParent())
              parent->ConvertToParentCoords(&offset.x, &offset.y);

            //Subtract back offset from root of view
            for (parent = view; parent; parent = parent->GetParent())
              parent->ConvertFromParentCoords(&offset.x, &offset.y);
          }

          //Dispatch the event
          //Before we start mucking with coords, make sure we know our baseline
          aEvent->refPoint.x = aEvent->point.x;
          aEvent->refPoint.y = aEvent->point.y;

          nsRect baseViewDimensions;
          if (baseView != nsnull) {
            baseView->GetDimensions(baseViewDimensions);
          }

          aEvent->point.x = baseViewDimensions.x + NSIntPixelsToTwips(aEvent->point.x, p2t);
          aEvent->point.y = baseViewDimensions.y + NSIntPixelsToTwips(aEvent->point.y, p2t);

          aEvent->point.x += offset.x;
          aEvent->point.y += offset.y;

          *aStatus = HandleEvent(view, aEvent, capturedEvent);

          // From here on out, "this" could have been deleted!!!

          aEvent->point.x -= offset.x;
          aEvent->point.y -= offset.y;

          aEvent->point.x = NSTwipsToIntPixels(aEvent->point.x - baseViewDimensions.x, t2p);
          aEvent->point.y = NSTwipsToIntPixels(aEvent->point.y - baseViewDimensions.y, t2p);

          //
          // if the event is an nsTextEvent, we need to map the reply back into platform coordinates
          //
          if (aEvent->message==NS_TEXT_TEXT) {
            ((nsTextEvent*)aEvent)->theReply.mCursorPosition.x=NSTwipsToIntPixels(((nsTextEvent*)aEvent)->theReply.mCursorPosition.x, t2p);
            ((nsTextEvent*)aEvent)->theReply.mCursorPosition.y=NSTwipsToIntPixels(((nsTextEvent*)aEvent)->theReply.mCursorPosition.y, t2p);
            ((nsTextEvent*)aEvent)->theReply.mCursorPosition.width=NSTwipsToIntPixels(((nsTextEvent*)aEvent)->theReply.mCursorPosition.width, t2p);
            ((nsTextEvent*)aEvent)->theReply.mCursorPosition.height=NSTwipsToIntPixels(((nsTextEvent*)aEvent)->theReply.mCursorPosition.height, t2p);
          }
          if((aEvent->message==NS_COMPOSITION_START) ||
             (aEvent->message==NS_COMPOSITION_QUERY)) {
            ((nsCompositionEvent*)aEvent)->theReply.mCursorPosition.x=NSTwipsToIntPixels(((nsCompositionEvent*)aEvent)->theReply.mCursorPosition.x,t2p);
            ((nsCompositionEvent*)aEvent)->theReply.mCursorPosition.y=NSTwipsToIntPixels(((nsCompositionEvent*)aEvent)->theReply.mCursorPosition.y,t2p);
            ((nsCompositionEvent*)aEvent)->theReply.mCursorPosition.width=NSTwipsToIntPixels(((nsCompositionEvent*)aEvent)->theReply.mCursorPosition.width,t2p);
            ((nsCompositionEvent*)aEvent)->theReply.mCursorPosition.height=NSTwipsToIntPixels(((nsCompositionEvent*)aEvent)->theReply.mCursorPosition.height,t2p);
          }
        }
    
        break;
      }
    }

  return NS_OK;
}

void nsViewManager::ReparentViews(DisplayZTreeNode* aNode,
                                  nsHashtable &aMapPlaceholderViewToZTreeNode)
{
  DisplayZTreeNode* child;
  DisplayZTreeNode** prev = &aNode->mZChild;
  for (child = aNode->mZChild; nsnull != child; child = *prev) {
    ReparentViews(child, aMapPlaceholderViewToZTreeNode);

    nsZPlaceholderView *zParent = nsnull;
    if (nsnull != child->mView) {
      zParent = child->mView->GetZParent();
    }
    if (nsnull != zParent) {
      nsVoidKey key(zParent);
      DisplayZTreeNode* placeholder = (DisplayZTreeNode *)aMapPlaceholderViewToZTreeNode.Get(&key);

      if (placeholder == child) {
        // don't do anything if we already reparented this node;
        // just advance to the next child
        prev = &child->mZSibling;
      } else {
        // unlink the child from the tree
        *prev = child->mZSibling;
        child->mZSibling = nsnull;
        
        if (nsnull != placeholder) {
          NS_ASSERTION((placeholder->mDisplayElement == nsnull), "placeholder already has elements?");
          NS_ASSERTION((placeholder->mZChild == nsnull), "placeholder already has Z-children?");
          placeholder->mDisplayElement = child->mDisplayElement;
          placeholder->mView = child->mView;
          placeholder->mZChild = child->mZChild;
          // We used to delete the child, but since we switched to Arenas just unreference it
          // delete child;
        } else {
          // the placeholder was not added to the display list
          // we don't need the real view then, either

          // We used to call DestroyZTreeNode which would delete the child
          // and its children/siblings and remove them from the Placeholder
          // hash.  However, we now use an Arena to build the tree.  This
          // means that we will never reuse a node (because it is never
          // freed), thus we can just leave it in the hash.  It will never
          // be looked up again.
          // DestroyZTreeNode(child);
        }
      }
    } else {
      prev = &child->mZSibling;
    }
  }
}

static PRBool ComputePlaceholderContainment(nsView* aView) {
  PRBool containsPlaceholder = aView->IsZPlaceholderView();

  nsView* child;
  for (child = aView->GetFirstChild(); child != nsnull; child = child->GetNextSibling()) {
    if (ComputePlaceholderContainment(child)) {
      containsPlaceholder = PR_TRUE;
    }
  }

  if (containsPlaceholder) {
    aView->SetViewFlags(aView->GetViewFlags() | NS_VIEW_FLAG_CONTAINS_PLACEHOLDER);
  } else {
    aView->SetViewFlags(aView->GetViewFlags() & ~NS_VIEW_FLAG_CONTAINS_PLACEHOLDER);
  }

  return containsPlaceholder;
}

/*
  Fills aDisplayList with DisplayListElement2* pointers. The caller is responsible
  for freeing these structs. The display list elements are ordered by z-order so
  that the first element of the array is at the bottom in z-order and the last element
  in the array is at the top in z-order.

  This should be changed so that the display list array is passed in as a parameter. There
  is no need to have the display list as a member of nsViewManager.

  aRect is the area in aView which we want to build a display list for.
  Set aEventProcesing when the list is required for event processing.
  Set aCaptured if the event is being captured by the given view.
*/
void nsViewManager::BuildDisplayList(nsView* aView, const nsRect& aRect, PRBool aEventProcessing,
                                     PRBool aCaptured, nsVoidArray* aDisplayList,
                                     PLArenaPool &aPool)
{
  // compute this view's origin
  nsPoint origin = ComputeViewOffset(aView);
    
  nsView *displayRoot = aView;
  if (!aCaptured) {
    for (;;) {
      nsView *displayParent = displayRoot->GetParent();

      if (nsnull == displayParent) {
        break;
      }

      if (displayRoot->GetFloating() && !displayParent->GetFloating()) {
        break;
      }
      displayRoot = displayParent;
    }
  }
    
  DisplayZTreeNode *zTree;

  nsPoint displayRootOrigin = ComputeViewOffset(displayRoot);
  displayRoot->ConvertFromParentCoords(&displayRootOrigin.x, &displayRootOrigin.y);
    
  // Determine, for each view, whether it is or contains a ZPlaceholderView
  ComputePlaceholderContainment(displayRoot);

  // Create the Z-ordered view tree
  PRBool paintFloats;
  if (aEventProcessing) {
    paintFloats = PR_TRUE;
  } else {
    paintFloats = displayRoot->GetFloating();
  }

  {
    nsHashtable       PlaceholderHash;

    CreateDisplayList(displayRoot, zTree, origin.x, origin.y,
                      aView, &aRect, displayRoot,
                      displayRootOrigin.x, displayRootOrigin.y,
                      paintFloats, aEventProcessing, PlaceholderHash, aPool);

    // Reparent any views that need reparenting in the Z-order tree
    if(zTree) {
      ReparentViews(zTree, PlaceholderHash);
    }
  }
    
  if (nsnull != zTree) {
    // Apply proper Z-order handling
    nsAutoVoidArray mergeTmp;

    SortByZOrder(zTree, *aDisplayList, mergeTmp, PR_TRUE, aPool);
  }
}

void nsViewManager::BuildEventTargetList(nsVoidArray &aTargets, nsView* aView, nsGUIEvent* aEvent,
                                         PRBool aCaptured, PLArenaPool &aPool)
{
  NS_ASSERTION(!mPainting, "View manager cannot handle events during a paint");
  if (mPainting) {
    return;
  }

  nsRect eventRect(aEvent->point.x, aEvent->point.y, 1, 1);
  nsAutoVoidArray displayList;
  BuildDisplayList(aView, eventRect, PR_TRUE, aCaptured, &displayList, aPool);

#ifdef DEBUG_roc
  if (getenv("MOZ_SHOW_DISPLAY_LIST")) ShowDisplayList(&displayList);
#endif

  // The display list is in order from back to front. We return the target list in order from
  // front to back.
  for (PRInt32 i = displayList.Count() - 1; i >= 0; --i) {
    DisplayListElement2* element = NS_STATIC_CAST(DisplayListElement2*, displayList.ElementAt(i));
    if (element->mFlags & VIEW_RENDERED) {
      aTargets.AppendElement(element);
    }
  }
}

nsEventStatus nsViewManager::HandleEvent(nsView* aView, nsGUIEvent* aEvent, PRBool aCaptured) {
//printf(" %d %d %d %d (%d,%d) \n", this, event->widget, event->widgetSupports, 
//       event->message, event->point.x, event->point.y);

  // Hold a refcount to the observer. The continued existence of the observer will
  // delay deletion of this view hierarchy should the event want to cause its
  // destruction in, say, some JavaScript event handler.
  nsCOMPtr<nsIViewObserver> obs;
  GetViewObserver(*getter_AddRefs(obs));

  // accessibility events and key events are dispatched directly to the focused view
  if (aEvent->eventStructType == NS_ACCESSIBLE_EVENT
      || aEvent->message == NS_CONTEXTMENU_KEY
      || aEvent->message == NS_MOUSE_EXIT
      || NS_IS_KEY_EVENT(aEvent) || NS_IS_IME_EVENT(aEvent) || NS_IS_FOCUS_EVENT(aEvent)) {
    nsEventStatus status = nsEventStatus_eIgnore;
    if (obs) {
       PRBool handled;
       obs->HandleEvent(aView, aEvent, &status, PR_TRUE, handled);
    }
    return status;
  }
    
  nsAutoVoidArray targetViews;
  nsAutoVoidArray heldRefCountsToOtherVMs;

  // In fact, we only need to take this expensive path when the event is a mouse event ... riiiight?
  PLArenaPool displayArena;
  PL_INIT_ARENA_POOL(&displayArena, "displayArena", 1024);
  BuildEventTargetList(targetViews, aView, aEvent, aCaptured, displayArena);

  nsEventStatus status = nsEventStatus_eIgnore;

  // get a death grip on any view managers' view observers (other than this one)
  PRInt32 i;
  for (i = 0; i < targetViews.Count(); i++) {
    DisplayListElement2* element = NS_STATIC_CAST(DisplayListElement2*, targetViews.ElementAt(i));
    nsView* v = element->mView;
    nsViewManager* vVM = v->GetViewManager();
    if (vVM != this) {
      nsIViewObserver* vobs = nsnull;
      vVM->GetViewObserver(vobs);
      if (nsnull != vobs) {
        heldRefCountsToOtherVMs.AppendElement(vobs);
      }
    }
  }

  for (i = 0; i < targetViews.Count(); i++) {
    DisplayListElement2* element = NS_STATIC_CAST(DisplayListElement2*, targetViews.ElementAt(i));
    nsView* v = element->mView;

    if (nsnull != v->GetClientData()) {
      PRBool handled = PR_FALSE;
      nsRect r;
      v->GetDimensions(r);

      nscoord x = element->mAbsX - r.x;
      nscoord y = element->mAbsY - r.y;

      aEvent->point.x -= x;
      aEvent->point.y -= y;

      nsViewManager* vVM = v->GetViewManager();
      if (vVM == this) {
        if (nsnull != obs) {
          obs->HandleEvent(v, aEvent, &status, i == targetViews.Count() - 1, handled);
        }
      } else {
        nsCOMPtr<nsIViewObserver> vobs;
        vVM->GetViewObserver(*getter_AddRefs(vobs));
        if (vobs) {
          vobs->HandleEvent(v, aEvent, &status, i == targetViews.Count() - 1, handled);
        }
      }

      aEvent->point.x += x;
      aEvent->point.y += y;

      if (handled) {
        break;
      }
      // if the child says "not handled" but did something which deleted the entire view hierarchy,
      // we'll crash in the next iteration. Oh well. The old code would have crashed too.
    }
  }

  PL_FreeArenaPool(&displayArena);
  PL_FinishArenaPool(&displayArena);

  // release death grips
  for (i = 0; i < heldRefCountsToOtherVMs.Count(); i++) {
    nsIViewObserver* element = NS_STATIC_CAST(nsIViewObserver*, heldRefCountsToOtherVMs.ElementAt(i));
    NS_RELEASE(element);
  }  

  return status;
}

NS_IMETHODIMP nsViewManager::GrabMouseEvents(nsIView *aView, PRBool &aResult)
{
  nsView* rootParent = mRootView ? mRootView->GetParent() : nsnull;
  if (rootParent) {
    return rootParent->GetViewManager()->GrabMouseEvents(aView, aResult);
  }

  // Along with nsView::SetVisibility, we enforce that the mouse grabber
  // can never be a hidden view.
  if (aView && NS_STATIC_CAST(nsView*, aView)->GetVisibility()
               == nsViewVisibility_kHide) {
    aView = nsnull;
  }

#ifdef DEBUG_mjudge
  if (aView)
    {
      printf("capturing mouse events for view %x\n",aView);
    }
  printf("removing mouse capture from view %x\n",mMouseGrabber);
#endif

  mMouseGrabber = NS_STATIC_CAST(nsView*, aView);
  aResult = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager::GrabKeyEvents(nsIView *aView, PRBool &aResult)
{
  mKeyGrabber = NS_STATIC_CAST(nsView*, aView);
  aResult = PR_TRUE;
  return NS_OK;
}

nsView* nsViewManager::GetMouseEventGrabber() const {
  nsView* root = mRootView;
  while (root && root->GetParent()) {
    nsViewManager* viewManager = root->GetParent()->GetViewManager();
    if (!viewManager)
      return nsnull;
    root = viewManager->mRootView;
  }
  if (!root)
    return nsnull;
  nsViewManager* viewManager = root->GetViewManager();
  return viewManager ? viewManager->mMouseGrabber : nsnull;
}

NS_IMETHODIMP nsViewManager::GetMouseEventGrabber(nsIView *&aView)
{
  aView = GetMouseEventGrabber();
  return NS_OK;
}

NS_IMETHODIMP nsViewManager::GetKeyEventGrabber(nsIView *&aView)
{
  aView = mKeyGrabber;
  return NS_OK;
}


// Recursively reparent widgets if necessary 

void nsViewManager::ReparentChildWidgets(nsIView* aView, nsIWidget *aNewWidget)
{
  if (aView->HasWidget()) {
    // Check to see if the parent widget is the
    // same as the new parent. If not then reparent
    // the widget, otherwise there is nothing more
    // to do for the view and its descendants
    nsIWidget* widget = aView->GetWidget();
    nsCOMPtr<nsIWidget> parentWidget = getter_AddRefs(widget->GetParent());
    if (parentWidget.get() != aNewWidget) {
      nsresult rv = widget->SetParent(aNewWidget);
      NS_ASSERTION(NS_SUCCEEDED(rv), "SetParent failed!");
    }
    return;
  }

  // Need to check each of the views children to see
  // if they have a widget and reparent it.

  nsView* view = NS_STATIC_CAST(nsView*, aView);
  for (nsView *kid = view->GetFirstChild(); kid; kid = kid->GetNextSibling()) {
    ReparentChildWidgets(kid, aNewWidget);
  }
}

// Reparent a view and its descendant views widgets if necessary

void nsViewManager::ReparentWidgets(nsIView* aView, nsIView *aParent)
{
  NS_PRECONDITION(aParent, "Must have a parent");
  NS_PRECONDITION(aView, "Must have a view");
  
  // Quickly determine whether the view has pre-existing children or a
  // widget. In most cases the view will not have any pre-existing 
  // children when this is called.  Only in the case
  // where a view has been reparented by removing it from
  // a reinserting it into a new location in the view hierarchy do we
  // have to consider reparenting the existing widgets for the view and
  // it's descendants.
  nsView* view = NS_STATIC_CAST(nsView*, aView);
  if (view->HasWidget() || view->GetFirstChild()) {
    nsIWidget* parentWidget = aParent->GetNearestWidget(nsnull);
    if (parentWidget) {
      ReparentChildWidgets(aView, parentWidget);
      return;
    }
    NS_WARNING("Can not find a widget for the parent view");
  }
}

NS_IMETHODIMP nsViewManager::InsertChild(nsIView *aParent, nsIView *aChild, nsIView *aSibling,
                                         PRBool aAfter)
{
  nsView* parent = NS_STATIC_CAST(nsView*, aParent);
  nsView* child = NS_STATIC_CAST(nsView*, aChild);
  nsView* sibling = NS_STATIC_CAST(nsView*, aSibling);
  
  NS_PRECONDITION(nsnull != parent, "null ptr");
  NS_PRECONDITION(nsnull != child, "null ptr");
  NS_ASSERTION(sibling == nsnull || sibling->GetParent() == parent,
               "tried to insert view with invalid sibling");
  NS_ASSERTION(!IsViewInserted(child), "tried to insert an already-inserted view");

  if ((nsnull != parent) && (nsnull != child))
    {
      // if aAfter is set, we will insert the child after 'prev' (i.e. after 'kid' in document
      // order, otherwise after 'kid' (i.e. before 'kid' in document order).

#if 1
      if (nsnull == aSibling) {
        if (aAfter) {
          // insert at end of document order, i.e., before first view
          // this is the common case, by far
          parent->InsertChild(child, nsnull);
          ReparentWidgets(child, parent);
        } else {
          // insert at beginning of document order, i.e., after last view
          nsView *kid = parent->GetFirstChild();
          nsView *prev = nsnull;
          while (kid) {
            prev = kid;
            kid = kid->GetNextSibling();
          }
          // prev is last view or null if there are no children
          parent->InsertChild(child, prev);
          ReparentWidgets(child, parent);
        }
      } else {
        nsView *kid = parent->GetFirstChild();
        nsView *prev = nsnull;
        while (kid && sibling != kid) {
          //get the next sibling view
          prev = kid;
          kid = kid->GetNextSibling();
        }
        NS_ASSERTION(kid != nsnull,
                     "couldn't find sibling in child list");
        if (aAfter) {
          // insert after 'kid' in document order, i.e. before in view order
          parent->InsertChild(child, prev);
          ReparentWidgets(child, parent);
        } else {
          // insert before 'kid' in document order, i.e. after in view order
          parent->InsertChild(child, kid);
          ReparentWidgets(child, parent);
        }
      }
#else // don't keep consistent document order, but order things by z-index instead
      // essentially we're emulating the old InsertChild(parent, child, zindex)
      PRInt32 zIndex = child->GetZIndex();
      while (nsnull != kid)
        {
          PRInt32 idx = kid->GetZIndex();

          if (CompareZIndex(zIndex, child->IsTopMost(), child->GetZIndexIsAuto(),
                            idx, kid->IsTopMost(), kid->GetZIndexIsAuto()) >= 0)
            break;

          prev = kid;
          kid = kid->GetNextSibling();
        }

      parent->InsertChild(child, prev);
      ReparentWidgets(child, parent);
#endif

      // if the parent view is marked as "floating", make the newly added view float as well.
      if (parent->GetFloating())
        child->SetFloating(PR_TRUE);

      //and mark this area as dirty if the view is visible...

      if (nsViewVisibility_kHide != child->GetVisibility())
        UpdateView(child, NS_VMREFRESH_NO_SYNC);
    }
  return NS_OK;
}

NS_IMETHODIMP nsViewManager::InsertZPlaceholder(nsIView *aParent, nsIView *aChild,
                                                nsIView *aSibling, PRBool aAfter)
{
  nsView* parent = NS_STATIC_CAST(nsView*, aParent);
  nsView* child = NS_STATIC_CAST(nsView*, aChild);

  NS_PRECONDITION(nsnull != parent, "null ptr");
  NS_PRECONDITION(nsnull != child, "null ptr");

  nsZPlaceholderView* placeholder = new nsZPlaceholderView();
  nsRect bounds(0, 0, 0, 0);
  // mark the placeholder as "shown" so that it will be included in a built display list
  placeholder->Init(this, bounds, parent, nsViewVisibility_kShow);
  placeholder->SetReparentedView(child);
  placeholder->SetZIndex(child->GetZIndexIsAuto(), child->GetZIndex(), child->IsTopMost());
  child->SetZParent(placeholder);
  
  return InsertChild(parent, placeholder, aSibling, aAfter);
}

NS_IMETHODIMP nsViewManager::InsertChild(nsIView *aParent, nsIView *aChild, PRInt32 aZIndex)
{
  // no-one really calls this with anything other than aZIndex == 0 on a fresh view
  // XXX this method should simply be eliminated and its callers redirected to the real method
  SetViewZIndex(aChild, PR_FALSE, aZIndex, PR_FALSE);
  return InsertChild(aParent, aChild, nsnull, PR_TRUE);
}

NS_IMETHODIMP nsViewManager::RemoveChild(nsIView *aChild)
{
  nsView* child = NS_STATIC_CAST(nsView*, aChild);

  NS_PRECONDITION(nsnull != child, "null ptr");

  nsView* parent = child->GetParent();

  if ((nsnull != parent) && (nsnull != child))
    {
      UpdateView(child, NS_VMREFRESH_NO_SYNC);
      parent->RemoveChild(child);
    }

  return NS_OK;
}

NS_IMETHODIMP nsViewManager::MoveViewBy(nsIView *aView, nscoord aX, nscoord aY)
{
  nsView* view = NS_STATIC_CAST(nsView*, aView);

  nsPoint pt = view->GetPosition();
  MoveViewTo(view, aX + pt.x, aY + pt.y);
  return NS_OK;
}

NS_IMETHODIMP nsViewManager::MoveViewTo(nsIView *aView, nscoord aX, nscoord aY)
{
  nsView* view = NS_STATIC_CAST(nsView*, aView);
  nsPoint oldPt = view->GetPosition();
  nsRect oldArea = view->GetBounds();
  view->SetPosition(aX, aY);

  // only do damage control if the view is visible

  if ((aX != oldPt.x) || (aY != oldPt.y)) {
    if (view->GetVisibility() != nsViewVisibility_kHide) {
      nsView* parentView = view->GetParent();
      UpdateView(parentView, oldArea, NS_VMREFRESH_NO_SYNC);
      UpdateView(parentView, view->GetBounds(), NS_VMREFRESH_NO_SYNC);
    }
  }
  return NS_OK;
}

void nsViewManager::InvalidateHorizontalBandDifference(nsView *aView, const nsRect& aRect, const nsRect& aCutOut,
  PRUint32 aUpdateFlags, nscoord aY1, nscoord aY2, PRBool aInCutOut) {
  nscoord height = aY2 - aY1;
  if (aRect.x < aCutOut.x) {
    nsRect r(aRect.x, aY1, aCutOut.x - aRect.x, height);
    UpdateView(aView, r, aUpdateFlags);
  }
  if (!aInCutOut && aCutOut.x < aCutOut.XMost()) {
    nsRect r(aCutOut.x, aY1, aCutOut.width, height);
    UpdateView(aView, r, aUpdateFlags);
  }
  if (aCutOut.XMost() < aRect.XMost()) {
    nsRect r(aCutOut.XMost(), aY1, aRect.XMost() - aCutOut.XMost(), height);
    UpdateView(aView, r, aUpdateFlags);
  }
}

void nsViewManager::InvalidateRectDifference(nsView *aView, const nsRect& aRect, const nsRect& aCutOut,
  PRUint32 aUpdateFlags) {
  if (aRect.y < aCutOut.y) {
    InvalidateHorizontalBandDifference(aView, aRect, aCutOut, aUpdateFlags, aRect.y, aCutOut.y, PR_FALSE);
  }
  if (aCutOut.y < aCutOut.YMost()) {
    InvalidateHorizontalBandDifference(aView, aRect, aCutOut, aUpdateFlags, aCutOut.y, aCutOut.YMost(), PR_TRUE);
  }
  if (aCutOut.YMost() < aRect.YMost()) {
    InvalidateHorizontalBandDifference(aView, aRect, aCutOut, aUpdateFlags, aCutOut.YMost(), aRect.YMost(), PR_FALSE);
  }
}

NS_IMETHODIMP nsViewManager::ResizeView(nsIView *aView, const nsRect &aRect, PRBool aRepaintExposedAreaOnly)
{
  nsView* view = NS_STATIC_CAST(nsView*, aView);
  nsRect oldDimensions;

  view->GetDimensions(oldDimensions);
  if (oldDimensions != aRect) {
    nsView* parentView = view->GetParent();
    if (parentView == nsnull)
      parentView = view;

    // resize the view.
    // Prevent Invalidation of hidden views 
    if (view->GetVisibility() == nsViewVisibility_kHide) {  
      view->SetDimensions(aRect, PR_FALSE);
    } else {
      if (!aRepaintExposedAreaOnly) {
        //Invalidate the union of the old and new size
        view->SetDimensions(aRect, PR_TRUE);

        UpdateView(view, aRect, NS_VMREFRESH_NO_SYNC);
        view->ConvertToParentCoords(&oldDimensions.x, &oldDimensions.y);
        UpdateView(parentView, oldDimensions, NS_VMREFRESH_NO_SYNC);
      } else {
        view->SetDimensions(aRect, PR_TRUE);

        InvalidateRectDifference(view, aRect, oldDimensions, NS_VMREFRESH_NO_SYNC);
        nsRect r = aRect;
        view->ConvertToParentCoords(&r.x, &r.y);
        view->ConvertToParentCoords(&oldDimensions.x, &oldDimensions.y);
        InvalidateRectDifference(parentView, oldDimensions, r, NS_VMREFRESH_NO_SYNC);
      } 
    }
  }

  // Note that if layout resizes the view and the view has a custom clip
  // region set, then we expect layout to update the clip region too. Thus
  // in the case where mClipRect has been optimized away to just be a null
  // pointer, and this resize is implicitly changing the clip rect, it's OK
  // because layout will change it back again if necessary.

  return NS_OK;
}

NS_IMETHODIMP nsViewManager::SetViewChildClipRegion(nsIView *aView, const nsRegion *aRegion)
{
  nsView* view = NS_STATIC_CAST(nsView*, aView);
 
  NS_ASSERTION(!(nsnull == view), "no view");

  const nsRect* oldClipRect = view->GetClipChildrenToRect();

  nsRect newClipRectStorage = view->GetDimensions();
  nsRect* newClipRect = nsnull;
  if (aRegion) {
    newClipRectStorage = aRegion->GetBounds();
    newClipRect = &newClipRectStorage;
  }

  if ((oldClipRect != nsnull) == (newClipRect != nsnull)
      && (!newClipRect || *newClipRect == *oldClipRect)) {
    return NS_OK;
  }
  nsRect oldClipRectStorage =
    oldClipRect ? *oldClipRect : view->GetDimensions();
 
  // Update the view properties
  view->SetClipChildrenToRect(newClipRect);

  if (IsViewInserted(view)) {
    // Invalidate changed areas
    // Paint (new - old) in the current view
    InvalidateRectDifference(view, newClipRectStorage, oldClipRectStorage, NS_VMREFRESH_NO_SYNC);
    // Paint (old - new) in the parent view, since it'll be clipped out of the current view
    nsView* parent = view->GetParent();
    if (parent) {
      oldClipRectStorage += view->GetPosition();
      newClipRectStorage += view->GetPosition();
      InvalidateRectDifference(parent, oldClipRectStorage, newClipRectStorage, NS_VMREFRESH_NO_SYNC);
    }
  }

  return NS_OK;
}

/*
  Returns PR_TRUE if and only if aView is a (possibly indirect) child of aAncestor.
*/
static PRBool IsAncestorOf(const nsView* aAncestor, const nsView* aView) 
{
  while (nsnull != aView) {
    aView = aView->GetParent();
    if (aView == aAncestor) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

/*
  This function returns TRUE only if we are sure that scrolling
  aView's widget and moving its child widgets, followed by
  UpdateViewAfterScroll, will result in correct painting (i.e. the
  same results as just invalidating the entire view). If we're not
  sure then we return FALSE to cause a full update of the view's area.

  The way we check this is quite subtle, because there are all sorts
  of complicated things that can happen.

  Conceptually what we do is compute the display list for the "after
  scrolling" situation and compare it to a translation of the display
  list for the "before scrolling" situation.  If the two lists are
  equal then we return TRUE.

  We could implement this directly, but that would be rather
  complex. In particular it's hard to build the display list for the
  "before scrolling" situation since this function is called after
  scrolling. So instead we just build the "after scrolling" display
  list and do some analysis of it, making some approximations along
  the way.

  Display list elements for aView's descendants will always be the
  same "before" (plus translation) and "after", because they get
  scrolled by the same amount as the translation. Display list
  elements for other views will be in different positions "before"
  (plus translation) and "after" because they don't get
  scrolled. These elements are the ones that force us to return
  PR_FALSE. Unfortunately we can't just scan the "after" display list
  and ensure no elements are not descendants of aView: there could
  have been some non-aView-descendant which appeared in the "before"
  list but does not show up in the "after" list because it ended up
  completely hidden by some opaque aView-descendant.

  So here's what we have to do: we compute an "after" display list but
  before performing the optimization pass to eliminate completely
  covered views, we mark all aView-descendant display list elements as
  transparent. Thus we can be sure that any non-aView-descendant
  display list elements that would have shown up in the translated "before"
  list will still show up in the "after" list (since the only views which
  can cover them are non-aView-descendants, which haven't moved). Then
  we check the final display list to make sure only aView-descendant
  display list elements are included.

  NOTE: We also scan the "after" display list to find views whose contents
  depend on their scroll position (e.g., CSS attachment-fixed backgrounds),
  which are flagged with NS_VIEW_FLAG_DONT_BITBLT. If any are found, we return
  PR_FALSE.

  XXX Post-1.0 we should consider instead of returning a boolean,
  actually computing a rectangle or region which can be safely
  scrolled with bitblt.  This would be the area which is aView's
  rectangle minus the area covered by any non-aView-descendant views
  in the translated "before" or "after" lists. Then we could scroll
  that area and explicitly invalidate the rest. This would give us
  high performance scrolling even in the presence of overlapping
  content and eliminate the need for native widgets for fixed position
  elements.
*/
PRBool nsViewManager::CanScrollWithBitBlt(nsView* aView)
{
  NS_ASSERTION(!mPainting, "View manager shouldn't be scrolling during a paint");
  if (mPainting) {
    return PR_FALSE; // do the safe thing
  }

  nsRect r = aView->GetClippedRect();
  // Only check the area that intersects the view's non clipped rectangle
  if (r.IsEmpty()) {
    return PR_TRUE; // nothing to scroll
  }

  nsAutoVoidArray displayList;
  PLArenaPool displayArena;
  PL_INIT_ARENA_POOL(&displayArena, "displayArena", 1024);
  BuildDisplayList(aView, r, PR_FALSE, PR_FALSE, &displayList, displayArena);

  PRInt32 i;
  for (i = 0; i < displayList.Count(); i++) {
    DisplayListElement2* element = NS_STATIC_CAST(DisplayListElement2*, displayList.ElementAt(i));
    if ((element->mFlags & VIEW_RENDERED) != 0) {
      if (IsAncestorOf(aView, element->mView)) {
        element->mFlags |= (VIEW_ISSCROLLED | VIEW_TRANSPARENT);
      }
    }
  }

  nsRect finalTransparentRect;

  // We DON'T use AddCoveringWidgetsToOpaqueRegion here. Our child widgets are going to be moved
  // as if they were scrolled, so we need to examine the display list elements that might be covered by
  // child widgets.

  // However, we do want to ignore areas that are covered by widgets which have not moved.
  // Unfortunately figuring out that area is not easy, because we don't yet trust the native
  // widget layer to tell us the correct z-order of native widgets.
  // We still have to handle at least one case well:
  // widgets for fixed-position elements when we are scrolling the root scrollable (or something inside
  // the root scrollable) are on top.
  nsRegion opaqueRegion;
  if (mRootScrollable != nsnull) {
    const nsIView* scrollableClipView;
    mRootScrollable->GetClipView(&scrollableClipView);
    if (IsAncestorOf(NS_STATIC_CAST(const nsView*, scrollableClipView), aView)) {
      // add areas of fixed views to the opaque area.
      // This is a bit of a hack. We should not be doing special case processing for fixed views.
      nsView* fixedView = mRootView->GetFirstChild();
      while (fixedView != nsnull) {
        if (fixedView->GetZParent() != nsnull && fixedView->GetZIndex() >= 0) {
          opaqueRegion.Or(opaqueRegion, fixedView->GetBounds());
        }
        fixedView = fixedView->GetNextSibling();
      }

      // get the region into the coordinates of aView
      nscoord deltaX = 0, deltaY = 0;
      for (nsView* v = aView; v; v = v->GetParent()) {
        v->ConvertToParentCoords(&deltaX, &deltaY);
      }
      opaqueRegion.MoveBy(-deltaX, -deltaY);
    }
  }

  // We DO need to use OptimizeDisplayList here to eliminate views
  // that are covered by views we know are opaque. Typically aView's
  // scrolled view is opaque and we want to eliminate views behind it,
  // such as aView itself, that aren't being moved and would otherwise
  // cause us to decide not to blit.  Note that if for some view,
  // view->HasUniformBackground(), that's also sufficient to ignore
  // views behind 'view'; we've been promised that they produce only a
  // uniform background, which is still blittable. So we can treat
  // 'view' as opaque.

  // (Note that it's possible for aView's parent to actually be in
  // front of aView (if aView has a negative z-index) but if so, this
  // code still does the right thing. Yay for the display list based
  // approach!)

  OptimizeDisplayList(&displayList, nsRegion(r), finalTransparentRect, opaqueRegion, PR_TRUE);

  PRBool anyUnscrolledViews = PR_FALSE;
  PRBool anyUnblittableViews = PR_FALSE;

  for (i = 0; i < displayList.Count(); i++) {
    DisplayListElement2* element = NS_STATIC_CAST(DisplayListElement2*, displayList.ElementAt(i));
    if ((element->mFlags & VIEW_RENDERED) != 0) {
      if ((element->mFlags & VIEW_ISSCROLLED) == 0 && element->mView != aView) {
        anyUnscrolledViews = PR_TRUE;
      } else if ((element->mView->GetViewFlags() & NS_VIEW_FLAG_DONT_BITBLT) != 0) {
        anyUnblittableViews = PR_TRUE;
      }
    }
  }

  PL_FreeArenaPool(&displayArena);
  PL_FinishArenaPool(&displayArena);

  return !anyUnscrolledViews && !anyUnblittableViews;
}

NS_IMETHODIMP nsViewManager::SetViewBitBltEnabled(nsIView *aView, PRBool aEnable)
{
  nsView* view = NS_STATIC_CAST(nsView*, aView);

  NS_ASSERTION(!(nsnull == view), "no view");

  if (aEnable) {
    view->SetViewFlags(view->GetViewFlags() & ~NS_VIEW_FLAG_DONT_BITBLT);
  } else {
    view->SetViewFlags(view->GetViewFlags() | NS_VIEW_FLAG_DONT_BITBLT);
  }

  return NS_OK;
}

NS_IMETHODIMP nsViewManager::SetViewCheckChildEvents(nsIView *aView, PRBool aEnable)
{
  nsView* view = NS_STATIC_CAST(nsView*, aView);

  NS_ASSERTION(!(nsnull == view), "no view");

  if (aEnable) {
    view->SetViewFlags(view->GetViewFlags() & ~NS_VIEW_FLAG_DONT_CHECK_CHILDREN);
  } else {
    view->SetViewFlags(view->GetViewFlags() | NS_VIEW_FLAG_DONT_CHECK_CHILDREN);
  }

  return NS_OK;
}

NS_IMETHODIMP nsViewManager::SetViewFloating(nsIView *aView, PRBool aFloating)
{
  nsView* view = NS_STATIC_CAST(nsView*, aView);

  NS_ASSERTION(!(nsnull == view), "no view");

  view->SetFloating(aFloating);

  return NS_OK;
}

NS_IMETHODIMP nsViewManager::SetViewVisibility(nsIView *aView, nsViewVisibility aVisible)
{
  nsView* view = NS_STATIC_CAST(nsView*, aView);

  if (aVisible != view->GetVisibility()) {
    view->SetVisibility(aVisible);

    if (IsViewInserted(view)) {
      if (!view->HasWidget()) {
        if (nsViewVisibility_kHide == aVisible) {
          nsView* parentView = view->GetParent();
          if (parentView) {
            UpdateView(parentView, view->GetBounds(), NS_VMREFRESH_NO_SYNC);
          }
        }
        else {
          UpdateView(view, NS_VMREFRESH_NO_SYNC);
        }
      }
    }

    // Any child views not associated with frames might not get their visibility
    // updated, so propagate our visibility to them. This is important because
    // hidden views should have all hidden children.
    for (nsView* childView = view->GetFirstChild(); childView;
         childView = childView->GetNextSibling()) {
      if (!childView->GetClientData()) {
        childView->SetVisibility(aVisible);
      }
    }
  }
  return NS_OK;
}

PRBool nsViewManager::IsViewInserted(nsView *aView)
{
  if (mRootView == aView) {
    return PR_TRUE;
  } else if (aView->GetParent() == nsnull) {
    return PR_FALSE;
  } else {
    nsView* view = aView->GetParent()->GetFirstChild();
    while (view != nsnull) {
      if (view == aView) {
        return PR_TRUE;
      }        
      view = view->GetNextSibling();
    }
    return PR_FALSE;
  }
}

NS_IMETHODIMP nsViewManager::SetViewZIndex(nsIView *aView, PRBool aAutoZIndex, PRInt32 aZIndex, PRBool aTopMost)
{
  nsView* view = NS_STATIC_CAST(nsView*, aView);
  nsresult  rv = NS_OK;

  NS_ASSERTION((view != nsnull), "no view");

  // don't allow the root view's z-index to be changed. It should always be zero.
  // This could be removed and replaced with a style rule, or just removed altogether, with interesting consequences
  if (aView == mRootView) {
    return rv;
  }

  PRBool oldTopMost = view->IsTopMost();
  PRBool oldIsAuto = view->GetZIndexIsAuto();

  if (aAutoZIndex) {
    aZIndex = 0;
  }

  PRInt32 oldidx = view->GetZIndex();
  view->SetZIndex(aAutoZIndex, aZIndex, aTopMost);

  if (CompareZIndex(oldidx, oldTopMost, oldIsAuto,
                    aZIndex, aTopMost, aAutoZIndex) != 0) {
    UpdateView(view, NS_VMREFRESH_NO_SYNC);
  }

  // Native widgets ultimately just can't deal with the awesome power
  // of CSS2 z-index. However, we set the z-index on the widget anyway
  // because in many simple common cases the widgets do end up in the
  // right order. Even if they don't, we'll still render correctly as
  // long as there are no plugins around (although there may be more
  // flickering and other perf issues than if the widgets were in a
  // good order).
  if (view->HasWidget()) {
    view->GetWidget()->SetZIndex(aZIndex);
  }

  nsZPlaceholderView* zParentView = view->GetZParent();
  if (nsnull != zParentView) {
    SetViewZIndex(zParentView, aAutoZIndex, aZIndex, aTopMost);
  }

  return rv;
}

NS_IMETHODIMP nsViewManager::SetViewContentTransparency(nsIView *aView, PRBool aTransparent)
{
  nsView* view = NS_STATIC_CAST(nsView*, aView);

  if (view->IsTransparent() != aTransparent) {
    view->SetContentTransparency(aTransparent);

    if (IsViewInserted(view)) {
      UpdateView(view, NS_VMREFRESH_NO_SYNC);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsViewManager::SetViewOpacity(nsIView *aView, float aOpacity)
{
  nsView* view = NS_STATIC_CAST(nsView*, aView);

  if (view->GetOpacity() != aOpacity)
    {
      view->SetOpacity(aOpacity);

      if (IsViewInserted(view)) {
        UpdateView(view, NS_VMREFRESH_NO_SYNC);
      }
    }

  return NS_OK;
}

NS_IMETHODIMP nsViewManager::SetViewObserver(nsIViewObserver *aObserver)
{
  mObserver = aObserver;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager::GetViewObserver(nsIViewObserver *&aObserver)
{
  if (nsnull != mObserver) {
    aObserver = mObserver;
    NS_ADDREF(mObserver);
    return NS_OK;
  } else
    return NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP nsViewManager::GetDeviceContext(nsIDeviceContext *&aContext)
{
  NS_IF_ADDREF(mContext);
  aContext = mContext;
  return NS_OK;
}

void nsViewManager::GetMaxWidgetBounds(nsRect& aMaxWidgetBounds) const
{
  // Go through the list of viewmanagers and get the maximum width and 
  // height of their widgets
  aMaxWidgetBounds.width = 0;
  aMaxWidgetBounds.height = 0;
  PRInt32 index = 0;
  for (index = 0; index < mVMCount; index++) {

    nsViewManager* vm = (nsViewManager*)gViewManagers->ElementAt(index);
    nsCOMPtr<nsIWidget> rootWidget;

    if(NS_SUCCEEDED(vm->GetWidget(getter_AddRefs(rootWidget))) && rootWidget)
      {
        nsRect widgetBounds;
        rootWidget->GetBounds(widgetBounds);
        aMaxWidgetBounds.width = PR_MAX(aMaxWidgetBounds.width, widgetBounds.width);
        aMaxWidgetBounds.height = PR_MAX(aMaxWidgetBounds.height, widgetBounds.height);
      }
  }

  //   printf("WIDGET BOUNDS %d %d\n", aMaxWidgetBounds.width, aMaxWidgetBounds.height);
}

PRInt32 nsViewManager::GetViewManagerCount()
{
  return mVMCount;
}

const nsVoidArray* nsViewManager::GetViewManagerArray() 
{
  return gViewManagers;
}

already_AddRefed<nsIRenderingContext>
nsViewManager::CreateRenderingContext(nsView &aView)
{
  nsView              *par = &aView;
  nsIWidget*          win;
  nsIRenderingContext *cx = nsnull;
  nscoord             ax = 0, ay = 0;

  do
    {
      win = par->GetWidget();
      if (win)
        break;

      //get absolute coordinates of view, but don't
      //add in view pos since the first thing you ever
      //need to do when painting a view is to translate
      //the rendering context by the views pos and other parts
      //of the code do this for us...

      if (par != &aView)
        {
          par->ConvertToParentCoords(&ax, &ay);
        }

      par = par->GetParent();
    }
  while (nsnull != par);

  if (nsnull != win)
    {
      mContext->CreateRenderingContext(&aView, cx);

      if (nsnull != cx)
        cx->Translate(ax, ay);
    }

  return cx;
}

void nsViewManager::AddRectToDirtyRegion(nsView* aView, const nsRect &aRect) const
{
  // Find a view with an associated widget. We'll transform this rect from the
  // current view's coordinate system to a "heavyweight" parent view, then convert
  // the rect to pixel coordinates, and accumulate the rect into that view's dirty region.
  nsView* widgetView = GetWidgetView(aView);
  if (widgetView != nsnull) {
    nsRect widgetRect = aRect;
    ViewToWidget(aView, widgetView, widgetRect);

    // Get the dirty region associated with the widget view
    nsCOMPtr<nsIRegion> dirtyRegion;
    if (NS_SUCCEEDED(widgetView->GetDirtyRegion(*getter_AddRefs(dirtyRegion)))) {
      // add this rect to the widget view's dirty region.
      dirtyRegion->Union(widgetRect.x, widgetRect.y, widgetRect.width, widgetRect.height);
    }
  }
}

NS_IMETHODIMP nsViewManager::DisableRefresh(void)
{
  if (mUpdateBatchCnt > 0)
    return NS_OK;

  mRefreshEnabled = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager::EnableRefresh(PRUint32 aUpdateFlags)
{
  if (mUpdateBatchCnt > 0)
    return NS_OK;

  mRefreshEnabled = PR_TRUE;

  if (aUpdateFlags & NS_VMREFRESH_IMMEDIATE) {
    ProcessPendingUpdates(mRootView);
    mHasPendingInvalidates = PR_FALSE;
    Composite();
  } else {
    PostInvalidateEvent();
  }

  return NS_OK;
}

NS_IMETHODIMP nsViewManager::BeginUpdateViewBatch(void)
{
  nsresult result = NS_OK;
  
  if (mUpdateBatchCnt == 0)
    result = DisableRefresh();

  if (NS_SUCCEEDED(result))
    ++mUpdateBatchCnt;

  return result;
}

NS_IMETHODIMP nsViewManager::EndUpdateViewBatch(PRUint32 aUpdateFlags)
{
  nsresult result = NS_OK;

  --mUpdateBatchCnt;

  NS_ASSERTION(mUpdateBatchCnt >= 0, "Invalid batch count!");

  if (mUpdateBatchCnt < 0)
    {
      mUpdateBatchCnt = 0;
      return NS_ERROR_FAILURE;
    }

  if (mUpdateBatchCnt == 0)
    result = EnableRefresh(aUpdateFlags);

  return result;
}

NS_IMETHODIMP nsViewManager::SetRootScrollableView(nsIScrollableView *aScrollable)
{
  if (mRootScrollable) {
    NS_STATIC_CAST(nsScrollPortView*, mRootScrollable)->
      SetClipPlaceholdersToBounds(PR_FALSE);
  }
  mRootScrollable = aScrollable;
  if (mRootScrollable) {
    NS_STATIC_CAST(nsScrollPortView*, mRootScrollable)->
      SetClipPlaceholdersToBounds(PR_TRUE);
  }
  return NS_OK;
}

NS_IMETHODIMP nsViewManager::GetRootScrollableView(nsIScrollableView **aScrollable)
{
  *aScrollable = mRootScrollable;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager::Display(nsIView* aView, nscoord aX, nscoord aY, const nsRect& aClipRect)
{
  nsView              *view = NS_STATIC_CAST(nsView*, aView);
  nsIRenderingContext *localcx = nsnull;

  if (PR_FALSE == mRefreshEnabled)
    return NS_OK;

  NS_ASSERTION(!(PR_TRUE == mPainting), "recursive painting not permitted");

  mPainting = PR_TRUE;

  mContext->CreateRenderingContext(localcx);

  //couldn't get rendering context. this is ok if at startup
  if (nsnull == localcx)
    {
      mPainting = PR_FALSE;
      return NS_ERROR_FAILURE;
    }

  nsRect trect = view->GetBounds();
  view->ConvertFromParentCoords(&trect.x, &trect.y);

  localcx->Translate(aX, aY);

  localcx->SetClipRect(aClipRect, nsClipCombine_kReplace);

  // Paint the view. The clipping rect was set above set don't clip again.
  //aView->Paint(*localcx, trect, NS_VIEW_FLAG_CLIP_SET, result);
  nsAutoVoidArray displayList;
  PLArenaPool displayArena;
  PL_INIT_ARENA_POOL(&displayArena, "displayArena", 1024);
  BuildRenderingDisplayList(view, nsRegion(trect), &displayList, displayArena);
  RenderViews(view, *localcx, nsRegion(trect), PR_FALSE, displayList);
  PL_FreeArenaPool(&displayArena);
  PL_FinishArenaPool(&displayArena);

  NS_RELEASE(localcx);

  mPainting = PR_FALSE;

  return NS_OK;

}

NS_IMETHODIMP nsViewManager::AddCompositeListener(nsICompositeListener* aListener)
{
  if (nsnull == mCompositeListeners) {
    nsresult rv = NS_NewISupportsArray(&mCompositeListeners);
    if (NS_FAILED(rv))
      return rv;
  }
  return mCompositeListeners->AppendElement(aListener);
}

NS_IMETHODIMP nsViewManager::RemoveCompositeListener(nsICompositeListener* aListener)
{
  if (nsnull != mCompositeListeners) {
    return mCompositeListeners->RemoveElement(aListener);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsViewManager::GetWidget(nsIWidget **aWidget)
{
  *aWidget = GetWidget();
  NS_IF_ADDREF(*aWidget);
  return NS_OK;
}

NS_IMETHODIMP nsViewManager::ForceUpdate()
{
  nsIWidget* widget = GetWidget();
  if (widget)
    widget->Update();
  return NS_OK;
}

static nsresult EnsureZTreeNodeCreated(nsView* aView, DisplayZTreeNode* &aNode, PLArenaPool &aPool)
{
  if (nsnull == aNode) {
    ARENA_ALLOCATE(aNode, &aPool, DisplayZTreeNode);

    if (nsnull == aNode) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    aNode->mView = aView;
    aNode->mDisplayElement = nsnull;
    aNode->mZChild = nsnull;
    aNode->mZSibling = nsnull;
  }
  return NS_OK;
}

/**
 * XXX this needs major simplification and cleanup
 * @param aView the view are visiting to create display list element(s) for
 * @param aResult insert display list elements under here
 * @param aOriginX/aOriginY the offset from the origin of aTopView
 * to the origin of the view that is being painted (aRealView)
 * @param aRealView the view that is being painted
 * @param aDamageRect the rect that needs to be painted, relative to the origin
 * of aRealView
 * @param aTopView the displayRoot, the root of the collection of views
 * to be painted
 * @param aX/aY the offset from aTopView to the origin of the parent of aView
 * @param aPaintFloats PR_TRUE if we should paint floating views, PR_FALSE
 * if we should avoid descending into any floating views
 * @param aEventProcessing PR_TRUE if we intend to do event processing with
 * this display list
 * @param aPool the arena to allocate the aResults elements from
 */
PRBool nsViewManager::CreateDisplayList(nsView *aView,
                                        DisplayZTreeNode* &aResult,
                                        nscoord aOriginX, nscoord aOriginY, nsView *aRealView,
                                        const nsRect *aDamageRect, nsView *aTopView,
                                        nscoord aX, nscoord aY, PRBool aPaintFloats,
                                        PRBool aEventProcessing,
                                        nsHashtable &aMapPlaceholderViewToZTreeNode,
                                        PLArenaPool &aPool)
{
  PRBool retval = PR_FALSE;

  aResult = nsnull;

  NS_ASSERTION((aView != nsnull), "no view");

  if (nsViewVisibility_kHide == aView->GetVisibility()) {
    // bail out if the view is marked invisible; all children are invisible
    return retval;
  }

  nsRect bounds = aView->GetBounds();
  nsPoint pos = aView->GetPosition();

  // -> to global coordinates (relative to aTopView)
  bounds.x += aX;
  bounds.y += aY;
  pos.MoveBy(aX, aY);

  // does this view clip all its children?
  PRBool isClipView =
    (aView->GetClipChildrenToBounds(PR_FALSE)
     && !(aView->GetViewFlags() & NS_VIEW_FLAG_CONTAINS_PLACEHOLDER))
    || aView->GetClipChildrenToBounds(PR_TRUE);
  PRBool overlap;
  nsRect irect;
    
  // -> to refresh-frame coordinates (relative to aRealView)
  bounds.x -= aOriginX;
  bounds.y -= aOriginY;
  if (aDamageRect) {
    overlap = irect.IntersectRect(bounds, *aDamageRect);
    if (isClipView) {
      aDamageRect = &irect;
    }
    if (aEventProcessing && aRealView == aView) {
      // Always deliver an event somewhere, at least to the top-level target.
      // There may be mouse capturing going on.
      overlap = PR_TRUE;
    }
  }
  else
    overlap = PR_TRUE;

  // -> to global coordinates (relative to aTopView)
  bounds.x += aOriginX;
  bounds.y += aOriginY;

  // if there's no overlap between the dirty area and the bounds of
  // the view, we should be able to ignore the view and all its
  // children. Unfortunately there is the possibility of reparenting:
  // some other view in the tree might want to be reparented to one of
  // our children, and that view might lie outside our
  // bounds. Typically we need to go ahead and add this view and its
  // children to the ZTree in case someone wants to reparent into it.
  //
  // We can avoid this in two cases: if we clip our children to our
  // bounds, then even if a view is reparented under us, none of its
  // visible area can lie outside our bounds, so it can't intersect
  // the dirty area.  Also, if we don't contain any placeholder views,
  // then there is no way for anyone to reparent below us.
  if (!overlap && !(aView->GetViewFlags() & NS_VIEW_FLAG_CONTAINS_PLACEHOLDER)) {
    return PR_FALSE;
  }

  // Don't paint floating views unless the root view being painted is a floating view.
  // This is important because we may be asked to paint
  // a window that's behind a transient float; in this case we must paint the real window
  // contents, not the float contents (bug 63496)
  if (!aPaintFloats && aView->GetFloating()) {
    return PR_FALSE;
  }

  PRBool anyChildren = aView->GetFirstChild() != nsnull;

  if (aEventProcessing
      && (aView->GetViewFlags() & NS_VIEW_FLAG_DONT_CHECK_CHILDREN) != 0) {
    anyChildren = PR_FALSE;
  }

  PRBool hasFilter = aView->GetOpacity() != 1.0f;
  if (hasFilter) {
    // -> to refresh-frame coordinates (relative to aRealView)
    bounds.x -= aOriginX;
    bounds.y -= aOriginY;
    
    // Add POP first because the z-tree is in reverse order
    retval = AddToDisplayList(aView, aResult, bounds, bounds,
                              POP_FILTER, aX - aOriginX, aY - aOriginY, PR_TRUE, aPool);
    if (retval)
      return retval;
    
    // -> to global coordinates (relative to aTopView)
    bounds.x += aOriginX;
    bounds.y += aOriginY;
  }

  if (anyChildren) {
    if (isClipView) {
      // -> to refresh-frame coordinates (relative to aRealView)
      bounds.x -= aOriginX;
      bounds.y -= aOriginY;

      // Add POP first because the z-tree is in reverse order
      retval = AddToDisplayList(aView, aResult, bounds, bounds,
                                POP_CLIP, aX - aOriginX, aY - aOriginY, PR_TRUE, aPool);

      if (retval)
        return retval;

      // -> to global coordinates (relative to aTopView)
      bounds.x += aOriginX;
      bounds.y += aOriginY;
    }

    // Put in all the children before we add this view itself.
    // This preserves document order.
    nsView *childView = nsnull;
    for (childView = aView->GetFirstChild(); nsnull != childView;
         childView = childView->GetNextSibling()) {
      DisplayZTreeNode* createdNode;
      retval = CreateDisplayList(childView, createdNode,
                                 aOriginX, aOriginY, aRealView, aDamageRect, aTopView,
                                 pos.x, pos.y, aPaintFloats, aEventProcessing,
                                 aMapPlaceholderViewToZTreeNode, aPool);
      if (createdNode != nsnull) {
        EnsureZTreeNodeCreated(aView, aResult, aPool);
        createdNode->mZSibling = aResult->mZChild;
        aResult->mZChild = createdNode;
      }

      if (retval)
        break;
    }
  }

  if (!retval) {
    if (overlap) {
      // -> to refresh-frame coordinates (relative to aRealView)
      bounds.x -= aOriginX;
      bounds.y -= aOriginY;

      if (aEventProcessing || aView->GetOpacity() > 0.0f) {
        PRUint32 flags = VIEW_RENDERED;
        if (aView->IsTransparent())
          flags |= VIEW_TRANSPARENT;
        retval = AddToDisplayList(aView, aResult, bounds, irect, flags,
                                  aX - aOriginX, aY - aOriginY,
                                  aEventProcessing && aTopView == aView, aPool);
        // We're forcing AddToDisplayList to pick up the view only
        // during event processing, and only when aView is back at the
        // root of the tree of acceptable views (note that when event
        // capturing is in effect, aTopView is the capturing view)
      }

      // -> to global coordinates (relative to aTopView)
      bounds.x += aOriginX;
      bounds.y += aOriginY;
    } else {
      if (aView->IsZPlaceholderView()) {
        EnsureZTreeNodeCreated(aView, aResult, aPool);
        nsVoidKey key(aView);
        aMapPlaceholderViewToZTreeNode.Put(&key, aResult);
      }
    }
  }

  if (anyChildren && isClipView) {
    // -> to refresh-frame coordinates (relative to aRealView)
    bounds.x -= aOriginX;
    bounds.y -= aOriginY;

    if (AddToDisplayList(aView, aResult, bounds, bounds, PUSH_CLIP,
                         aX - aOriginX, aY - aOriginY, PR_TRUE, aPool)) {
      retval = PR_TRUE;
    }
    
    // -> to global coordinates (relative to aTopView)
    bounds.x += aOriginX;
    bounds.y += aOriginY;
  }

  if (hasFilter) {
    // -> to refresh-frame coordinates (relative to aRealView)
    bounds.x -= aOriginX;
    bounds.y -= aOriginY;
    
    retval = AddToDisplayList(aView, aResult, bounds, bounds,
                              PUSH_FILTER, aX - aOriginX, aY - aOriginY, PR_TRUE, aPool);
    if (retval)
      return retval;
    
    // -> to global coordinates (relative to aTopView)
    bounds.x += aOriginX;
    bounds.y += aOriginY;
  }

  return retval;
}

PRBool nsViewManager::AddToDisplayList(nsView *aView,
                                       DisplayZTreeNode* &aParent, nsRect &aClipRect,
                                       nsRect& aDirtyRect, PRUint32 aFlags,
                                       nscoord aAbsX, nscoord aAbsY,
                                       PRBool aAssumeIntersection, PLArenaPool &aPool)
{
  nsRect clipRect = aView->GetClippedRect();
  PRBool clipped = clipRect != aView->GetDimensions();

  // get clipRect into the coordinate system of aView's parent. aAbsX and
  // aAbsY give an offset to the origin of aView's parent.
  clipRect.MoveBy(aView->GetPosition());
  clipRect.x += aAbsX;
  clipRect.y += aAbsY;

  if (!clipped) {
    // XXX this code can't be right. If we're only clipped by one pixel,
    // then that's no reason to use a whole different rectangle.
    NS_ASSERTION(clipRect == aClipRect, "gah!!!");
    clipRect = aClipRect;
  }

  PRBool overlap = clipRect.IntersectRect(clipRect, aDirtyRect);
  if (!overlap && !aAssumeIntersection) {
    return PR_FALSE;
  }

  DisplayListElement2* element;
  ARENA_ALLOCATE(element, &aPool, DisplayListElement2);
  if (element == nsnull) {
    return PR_TRUE;
  }
  DisplayZTreeNode* node;
  ARENA_ALLOCATE(node, &aPool, DisplayZTreeNode);
  if (nsnull == node) {
    return PR_TRUE;
  }

  EnsureZTreeNodeCreated(aView, aParent, aPool);

  node->mDisplayElement = element;
  node->mView = nsnull;
  node->mZChild = nsnull;
  node->mZSibling = aParent->mZChild;
  aParent->mZChild = node;

  element->mView = aView;
  element->mBounds = clipRect;
  element->mAbsX = aClipRect.x;
  element->mAbsY = aClipRect.y;
  element->mFlags = aFlags;
  if (clipped) { 
    element->mFlags |= VIEW_CLIPPED;
  }
  
  return PR_FALSE;
}

/**
   Walk the display list, looking for opaque views, and remove any views that are behind them
   and totally occluded.

   @param aFinalTransparentRect
       Receives a rectangle enclosing all pixels in the damage rectangle
       which will not be opaquely painted over by the display list.
       Usually this will be empty, but nothing really prevents someone
       from creating a set of views that are (for example) all transparent.
*/
void nsViewManager::OptimizeDisplayList(const nsVoidArray* aDisplayList, const nsRegion& aDamageRegion,
                                        nsRect& aFinalTransparentRect,
                                        nsRegion &aOpaqueRegion, PRBool aTreatUniformAsOpaque)
{
  // Mark all views inside a PUSH_FILTER/POP_FILTER as being translucent.
  // If we don't do this, we'll incorrectly optimize by thinking views are
  // opaque when they really aren't.
  PRInt32 i;
  PRInt32 filterDepth = 0;
  for (i = 0; i < aDisplayList->Count(); ++i) {
    DisplayListElement2* element = NS_STATIC_CAST(DisplayListElement2*,
                                                  aDisplayList->ElementAt(i));
    if (element->mFlags & PUSH_FILTER) {
      ++filterDepth;
    }
    if (filterDepth > 0 && (element->mFlags & VIEW_RENDERED)) {
      element->mFlags |= VIEW_TRANSLUCENT;
    }
    if (element->mFlags & POP_FILTER) {
      --filterDepth;
    }
  }

  for (i = aDisplayList->Count() - 1; i >= 0; --i) {
    DisplayListElement2* element = NS_STATIC_CAST(DisplayListElement2*, aDisplayList->ElementAt(i));
    if (element->mFlags & VIEW_RENDERED) {
      nsRegion tmpRgn;
      tmpRgn.Sub(element->mBounds, aOpaqueRegion);
      tmpRgn.And(tmpRgn, aDamageRegion);

      if (tmpRgn.IsEmpty()) {
        element->mFlags &= ~VIEW_RENDERED;
      } else {
        element->mBounds = tmpRgn.GetBounds();

        // See whether we should add this view's bounds to aOpaqueRegion
        if (aOpaqueRegion.GetNumRects() <= MAX_OPAQUE_REGION_COMPLEXITY &&
            // a view is opaque if it is neither transparent nor transluscent
            (!(element->mFlags & (VIEW_TRANSPARENT | VIEW_TRANSLUCENT))
            // also, treat it as opaque if it's drawn onto a uniform background
            // and we're doing scrolling analysis; if the background is uniform,
            // we don't care what's under it. But the background might be translucent
            // because of some enclosing opacity group, in which case we do care
            // what's under it. Unfortunately we don't know exactly where the
            // background comes from ... it may not even have a view ... but
            // the side condition on SetHasUniformBackground ensures that
            // if the background is translucent, then this view is also marked
            // translucent.
            || (element->mView->HasUniformBackground() && aTreatUniformAsOpaque
                && !(element->mFlags & VIEW_TRANSLUCENT)))) {
          aOpaqueRegion.Or(aOpaqueRegion, element->mBounds);
        }
      }
    }
  }

  nsRegion tmpRgn;
  tmpRgn.Sub(aDamageRegion, aOpaqueRegion);
  aFinalTransparentRect = tmpRgn.GetBounds();
}

// Remove redundant PUSH/POP_CLIP pairs. These could be expensive.
void nsViewManager::OptimizeDisplayListClipping(const nsVoidArray* aDisplayList,
                                                PRBool aHaveClip, nsRect& aClipRect, PRInt32& aIndex,
                                                PRBool& aAnyRendered)
{   
  aAnyRendered = PR_FALSE;

  while (aIndex < aDisplayList->Count()) {
    DisplayListElement2* element =
      NS_STATIC_CAST(DisplayListElement2*, aDisplayList->ElementAt(aIndex));
    aIndex++;

    if (element->mFlags & VIEW_RENDERED) {
      aAnyRendered = PR_TRUE;

      if (aHaveClip && (element->mFlags & VIEW_CLIPPED)) {
        nsRect newClip;
        newClip.IntersectRect(aClipRect, element->mBounds);
        // no need to clip if the clip rect doesn't change
        if (newClip == aClipRect) {
          element->mFlags &= ~VIEW_CLIPPED;
        }
      }
    }

    if (element->mFlags & PUSH_CLIP) {
      nsRect newClip;
      if (aHaveClip) {
        newClip.IntersectRect(aClipRect, element->mBounds);
      } else {
        newClip = element->mBounds;
      }

      PRBool anyRenderedViews = PR_FALSE;
      OptimizeDisplayListClipping(aDisplayList, PR_TRUE, newClip, aIndex, anyRenderedViews);
      DisplayListElement2* popElement =
        NS_STATIC_CAST(DisplayListElement2*, aDisplayList->ElementAt(aIndex - 1));
      NS_ASSERTION(popElement->mFlags & POP_CLIP, "Must end with POP!");

      if (anyRenderedViews) {
        aAnyRendered = PR_TRUE;
      }
      if (!anyRenderedViews || (aHaveClip && newClip == aClipRect)) {
        // no need to clip if nothing's going to be rendered
        // ... or if the clip rect didn't change
        element->mFlags &= ~PUSH_CLIP;
        popElement->mFlags &= ~POP_CLIP;
      }
    }

    if (element->mFlags & POP_CLIP) {
      return;
    }
  }
}

// Compute the bounds what's rendered between each PUSH_FILTER/POP_FILTER. Also
// compute the region of what's rendered that's actually opaque. Then we can
// decide for each PUSH_FILTER/POP_FILTER pair whether the contents are partially
// transparent or fully opaque. Detecting the fully opaque case is important
// because it's much faster to composite a fully opaque element (i.e., all alphas
// 255).
nsRect nsViewManager::OptimizeTranslucentRegions(
  const nsVoidArray& aDisplayList, PRInt32* aIndex, nsRegion* aOpaqueRegion)
{
  nsRect r;
  while (*aIndex < aDisplayList.Count()) {
    DisplayListElement2* element =
      NS_STATIC_CAST(DisplayListElement2*, aDisplayList.ElementAt(*aIndex));
    (*aIndex)++;

    if (element->mFlags & VIEW_RENDERED) {
      r.UnionRect(r, element->mBounds);
      // the bounds of a non-transparent element are added to the opaque
      // region
      if (!element->mView->IsTransparent() && aOpaqueRegion) {
        aOpaqueRegion->Or(*aOpaqueRegion, element->mBounds);
      }
    }

    if (element->mFlags & PUSH_FILTER) {
      // the region inside the PUSH/POP pair that's painted opaquely
      nsRegion opaqueRegion;
      // save the bounds of the area that's painted by elements between the PUSH/POP
      element->mBounds = OptimizeTranslucentRegions(aDisplayList, aIndex,
                                                    &opaqueRegion);
      DisplayListElement2* popElement =
        NS_STATIC_CAST(DisplayListElement2*, aDisplayList.ElementAt(*aIndex - 1));
      popElement->mBounds = element->mBounds;
      NS_ASSERTION(popElement->mFlags & POP_FILTER, "Must end with POP!");

      // don't bother with filters if nothing is visible inside the filter
      if (element->mBounds.IsEmpty()) {
        element->mFlags &= ~PUSH_FILTER;
        popElement->mFlags &= ~POP_FILTER;
      } else {
        nsRegion tmpRegion;
        tmpRegion.Sub(element->mBounds, opaqueRegion);
        if (!tmpRegion.IsEmpty()) {
          // remember whether this PUSH_FILTER/POP_FILTER contents are fully opaque or not
          element->mFlags |= VIEW_TRANSPARENT;
        }
      }

      // The filter doesn't paint opaquely, so don't add anything to aOpaqueRegion
      r.UnionRect(r, element->mBounds);
    }
    if (element->mFlags & POP_FILTER) {
      return r;
    }
  }

  return r;
}

void nsViewManager::ShowDisplayList(const nsVoidArray* aDisplayList)
{
#ifdef NS_DEBUG
  printf("### display list length=%d ###\n", aDisplayList->Count());

  PRInt32 nestcnt = 0;
  for (PRInt32 cnt = 0; cnt < aDisplayList->Count(); cnt++) {
    DisplayListElement2* element = (DisplayListElement2*)aDisplayList->ElementAt(cnt);
    nestcnt = PrintDisplayListElement(element, nestcnt);
  }
#endif
}

nsPoint nsViewManager::ComputeViewOffset(const nsView *aView)
{
  nsPoint origin(0, 0);
  while (aView) {
    origin += aView->GetPosition();
    aView = aView->GetParent();
  }
  return origin;
}

PRBool nsViewManager::DoesViewHaveNativeWidget(nsView* aView)
{
  if (aView->HasWidget())
    return (nsnull != aView->GetWidget()->GetNativeData(NS_NATIVE_WIDGET));
  return PR_FALSE;
}

nsView* nsViewManager::GetWidgetView(nsView *aView)
{
  while (aView) {
    if (aView->HasWidget())
      return aView;
    aView = aView->GetParent();
  }
  return nsnull;
}

void nsViewManager::ViewToWidget(nsView *aView, nsView* aWidgetView, nsRect &aRect) const
{
  while (aView != aWidgetView) {
    aView->ConvertToParentCoords(&aRect.x, &aRect.y);
    aView = aView->GetParent();
  }
  
  // intersect aRect with bounds of aWidgetView, to prevent generating any illegal rectangles.
  nsRect bounds;
  aWidgetView->GetDimensions(bounds);
  aRect.IntersectRect(aRect, bounds);
  // account for the view's origin not lining up with the widget's
  aRect.x -= bounds.x;
  aRect.y -= bounds.y;
  
  // finally, convert to device coordinates.
  float t2p;
  t2p = mContext->AppUnitsToDevUnits();
  aRect.ScaleRoundOut(t2p);
}

nsresult nsViewManager::GetVisibleRect(nsRect& aVisibleRect)
{
  nsresult rv = NS_OK;

  // Get the viewport scroller
  nsIScrollableView* scrollingView;
  GetRootScrollableView(&scrollingView);

  if (scrollingView) {   
    // Determine the visible rect in the scrolled view's coordinate space.
    // The size of the visible area is the clip view size
    const nsIView*  clipViewI;
    scrollingView->GetClipView(&clipViewI);

    const nsView* clipView = NS_STATIC_CAST(const nsView*, clipViewI);
    clipView->GetDimensions(aVisibleRect);

    scrollingView->GetScrollPosition(aVisibleRect.x, aVisibleRect.y);
  } else {
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}

nsresult nsViewManager::GetAbsoluteRect(nsView *aView, const nsRect &aRect, 
                                        nsRect& aAbsRect)
{
  nsIScrollableView* scrollingView = nsnull;
  GetRootScrollableView(&scrollingView);
  if (nsnull == scrollingView) { 
    return NS_ERROR_FAILURE;
  }

  nsIView* scrolledIView = nsnull;
  scrollingView->GetScrolledView(scrolledIView);
  
  nsView* scrolledView = NS_STATIC_CAST(nsView*, scrolledIView);

  // Calculate the absolute coordinates of the aRect passed in.
  // aRects values are relative to aView
  aAbsRect = aRect;
  nsView *parentView = aView;
  while ((parentView != nsnull) && (parentView != scrolledView)) {
    parentView->ConvertToParentCoords(&aAbsRect.x, &aAbsRect.y);
    parentView = parentView->GetParent();
  }

  if (parentView != scrolledView) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


NS_IMETHODIMP nsViewManager::GetRectVisibility(nsIView *aView, 
                                               const nsRect &aRect,
                                               PRUint16 aMinTwips, 
                                               nsRectVisibility *aRectVisibility)
{
  nsView* view = NS_STATIC_CAST(nsView*, aView);

  // The parameter aMinTwips determines how many rows/cols of pixels must be visible on each side of the element,
  // in order to be counted as visible

  *aRectVisibility = nsRectVisibility_kZeroAreaRect;
  if (aRect.width == 0 || aRect.height == 0) {
    return NS_OK;
  }

  // is this view even visible?
  if (view->GetVisibility() == nsViewVisibility_kHide) {
    return NS_OK; 
  }

  // Calculate the absolute coordinates for the visible rectangle   
  nsRect visibleRect;
  if (GetVisibleRect(visibleRect) == NS_ERROR_FAILURE) {
    *aRectVisibility = nsRectVisibility_kVisible;
    return NS_OK;
  }

  // Calculate the absolute coordinates of the aRect passed in.
  // aRects values are relative to aView
  nsRect absRect;
  if ((GetAbsoluteRect(view, aRect, absRect)) == NS_ERROR_FAILURE) {
    *aRectVisibility = nsRectVisibility_kVisible;
    return NS_OK;
  }
 
  /*
   * If aMinTwips > 0, ensure at least aMinTwips of space around object is visible
   * The object is not visible if:
   * ((objectTop     < windowTop    && objectBottom < windowTop) ||
   *  (objectBottom  > windowBottom && objectTop    > windowBottom) ||
   *  (objectLeft    < windowLeft   && objectRight  < windowLeft) ||
   *  (objectRight   > windowRight  && objectLeft   > windowRight))
   */

  if (absRect.y < visibleRect.y  && 
      absRect.y + absRect.height < visibleRect.y + aMinTwips)
    *aRectVisibility = nsRectVisibility_kAboveViewport;
  else if (absRect.y + absRect.height > visibleRect.y + visibleRect.height &&
           absRect.y > visibleRect.y + visibleRect.height - aMinTwips)
    *aRectVisibility = nsRectVisibility_kBelowViewport;
  else if (absRect.x < visibleRect.x && 
           absRect.x + absRect.width < visibleRect.x + aMinTwips)
    *aRectVisibility = nsRectVisibility_kLeftOfViewport;
  else if (absRect.x + absRect.width > visibleRect.x  + visibleRect.width &&
           absRect.x > visibleRect.x + visibleRect.width - aMinTwips)
    *aRectVisibility = nsRectVisibility_kRightOfViewport;
  else
    *aRectVisibility = nsRectVisibility_kVisible;

  return NS_OK;
}


NS_IMETHODIMP
nsViewManager::IsCachingWidgetChanges(PRBool* aCaching)
{
#ifdef CACHE_WIDGET_CHANGES
  *aCaching = (mCachingWidgetChanges > 0);
#else
  *aCaching = PR_FALSE;
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsViewManager::CacheWidgetChanges(PRBool aCache)
{

#ifdef CACHE_WIDGET_CHANGES
  if (aCache == PR_TRUE)
    mCachingWidgetChanges++;
  else
    mCachingWidgetChanges--;

  NS_ASSERTION(mCachingWidgetChanges >= 0, "One too many decrements");

  // if we turned it off. Then move and size all the widgets.
  if (mCachingWidgetChanges == 0)
    ProcessWidgetChanges(mRootView);
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsViewManager::AllowDoubleBuffering(PRBool aDoubleBuffer)
{
  mAllowDoubleBuffering = aDoubleBuffer;
  return NS_OK;
}

NS_IMETHODIMP
nsViewManager::IsPainting(PRBool& aIsPainting)
{
  aIsPainting = mPainting;
  return NS_OK;
}

NS_IMETHODIMP
nsViewManager::FlushPendingInvalidates()
{
  if (mHasPendingInvalidates) {
    ProcessPendingUpdates(mRootView);
    mHasPendingInvalidates = PR_FALSE;
  }
  return NS_OK;
}

void
nsViewManager::ProcessInvalidateEvent()
{
  FlushPendingInvalidates();
  mInvalidateEventQueue = nsnull;
}

nsresult
nsViewManager::ProcessWidgetChanges(nsView* aView)
{
  //printf("---------Begin Sync----------\n");
  nsresult rv = aView->SynchWidgetSizePosition();
  if (NS_FAILED(rv))
    return rv;

  nsView *child = aView->GetFirstChild();
  while (nsnull != child) {
    if (child->GetViewManager() == this) {
      rv = ProcessWidgetChanges(child);
      if (NS_FAILED(rv))
        return rv;
    }

    child = child->GetNextSibling();
  }

  //printf("---------End Sync----------\n");

  return NS_OK;
}

NS_IMETHODIMP
nsViewManager::SetDefaultBackgroundColor(nscolor aColor)
{
  mDefaultBackgroundColor = aColor;
  return NS_OK;
}

NS_IMETHODIMP
nsViewManager::GetDefaultBackgroundColor(nscolor* aColor)
{
  *aColor = mDefaultBackgroundColor;
  return NS_OK;
}


NS_IMETHODIMP
nsViewManager::GetLastUserEventTime(PRUint32& aTime)
{
  aTime = gLastUserEventTime;
  return NS_OK;
}

struct nsSynthMouseMoveEvent : public nsViewManagerEvent {
  nsSynthMouseMoveEvent(nsViewManager *aViewManager, PRBool aFromScroll)
    : nsViewManagerEvent(aViewManager),
      mFromScroll(aFromScroll)
  {
  }

  virtual void HandleEvent() {
    ViewManager()->ProcessSynthMouseMoveEvent(mFromScroll);
  }

  PRBool mFromScroll;
};

NS_IMETHODIMP
nsViewManager::SynthesizeMouseMove(PRBool aFromScroll)
{
  if (mMouseLocation == nsPoint(NSCOORD_NONE, NSCOORD_NONE))
    return NS_OK;

  nsCOMPtr<nsIEventQueue> eventQueue;
  mEventQueueService->GetSpecialEventQueue(
    nsIEventQueueService::UI_THREAD_EVENT_QUEUE, getter_AddRefs(eventQueue));
  NS_ASSERTION(nsnull != eventQueue, "Event queue is null");

  if (eventQueue != mSynthMouseMoveEventQueue) {
    nsSynthMouseMoveEvent *ev = new nsSynthMouseMoveEvent(this, aFromScroll);
    eventQueue->PostEvent(ev);
    mSynthMouseMoveEventQueue = eventQueue;
  }

  return NS_OK;
}

void
nsViewManager::ProcessSynthMouseMoveEvent(PRBool aFromScroll)
{
  // allow new event to be posted while handling this one only if the
  // source of the event is a scroll (to prevent infinite reflow loops)
  if (aFromScroll)
    mSynthMouseMoveEventQueue = nsnull;

  if (mMouseLocation == nsPoint(NSCOORD_NONE, NSCOORD_NONE) || !mRootView) {
    mSynthMouseMoveEventQueue = nsnull;
    return;
  }

#ifdef DEBUG_MOUSE_LOCATION
  printf("[vm=%p]synthesizing mouse move to (%d,%d)\n",
         this, mMouseLocation.x, mMouseLocation.y);
#endif

  nsMouseEvent event(NS_MOUSE_MOVE, mRootView->GetWidget(),
                     nsMouseEvent::eSynthesized);
  event.point = mMouseLocation;
  event.time = PR_IntervalNow();
  // XXX set event.isShift, event.isControl, event.isAlt, event.isMeta ?

  nsEventStatus status;
  DispatchEvent(&event, &status);

  if (!aFromScroll)
    mSynthMouseMoveEventQueue = nsnull;
}
