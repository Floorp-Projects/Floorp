/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: true; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Contributor(s):  Patrick C. Beard <beard@netscape.com>
 *                  Kevin McCluskey  <kmcclusk@netscape.com>
 *                  Robert O'Callahan <roc+@cs.cmu.edu>
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

#include "nsViewManager.h"
#include "nsUnitConversion.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsGfxCIID.h"
#include "nsIScrollableView.h"
#include "nsView.h"
#include "nsIScrollbar.h"
#include "nsIClipView.h"
#include "nsISupportsArray.h"
#include "nsICompositeListener.h"
#include "nsCOMPtr.h"
#include "nsIEventQueue.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsGUIEvent.h"

static NS_DEFINE_IID(kBlenderCID, NS_BLENDER_CID);
static NS_DEFINE_IID(kRegionCID, NS_REGION_CID);
static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);


/**
  A note about platform assumptions:

     We assume all native widgets are opaque.

     We assume that a widget is z-ordered on top of its parent.

     We do NOT assume anything about the relative z-ordering of sibling widgets.
     In particular, nsIWidget->SetZIndex is basically ignored.
     Setting the widget z-index "correctly" (so that plugins etc have the correct
     z-order) and then exploiting that z-order to better optimize invalidation
     and refresh is a hard problem and remains future work. (Not that this has
     been broken --- this has NEVER worked.)
*/

//#define NO_DOUBLE_BUFFER

// if defined widget changes like moves and resizes are defered until and done
// all in one pass.
//#define CACHE_WIDGET_CHANGES

// display list flags
#define VIEW_RENDERED		0x00000001
#define PUSH_CLIP			0x00000002
#define POP_CLIP			0x00000004
#define VIEW_TRANSPARENT	0x00000008
#define VIEW_TRANSLUCENT	0x00000010
#define VIEW_CLIPPED 	 0x00000020

// compositor per-view flags
#define IS_PARENT_OF_REFRESHED_VIEW  0x00000001
#define IS_Z_PLACEHOLDER_VIEW        0x80000000

#define SUPPORT_TRANSLUCENT_VIEWS

// display list elements
struct DisplayListElement2 {
  nsIView*      mView;        // coordinates relative to the repainting view
  nsRect        mBounds;
  nscoord       mAbsX, mAbsY;
  PRUint32      mFlags;
  PRInt32       mZIndex;
};

struct DisplayZTreeNode {
  nsIView*             mView;    // May be null
  DisplayZTreeNode*    mZSibling;

  // We can't have BOTH an mZChild and an mDisplayElement
  DisplayZTreeNode*    mZChild;
  DisplayListElement2* mDisplayElement; 
};

static void DestroyZTreeNode(DisplayZTreeNode* aNode) {
  if (nsnull != aNode) {
    DestroyZTreeNode(aNode->mZChild);
    DestroyZTreeNode(aNode->mZSibling);
    delete aNode->mDisplayElement;
    delete aNode;
  }
}


#ifdef NS_VM_PERF_METRICS
#include "nsITimeRecorder.h"
#endif

//-------------- Begin Invalidate Event Definition ------------------------

struct nsInvalidateEvent : public PLEvent {
  nsInvalidateEvent(nsIViewManager* aViewManager);
  ~nsInvalidateEvent() { }

  void HandleEvent() {  
    NS_ASSERTION(nsnull != mViewManager,"ViewManager is null");
    // Search for valid view manager before trying to access it
    // This is just a safety check. We should never have a circumstance
    // where the view manager has been destroyed and the invalidate event
    // which it owns is still around. The invalidate event should be destroyed
    // by the RevokeEvent in the viewmanager's destructor.
    PRBool found = PR_FALSE;
    PRInt32 index;
    PRInt32 count = nsViewManager::GetViewManagerCount();
    const nsVoidArray* viewManagers = nsViewManager::GetViewManagerArray();
    for (index = 0; index < count; index++) {
      nsIViewManager* vm = (nsIViewManager*)viewManagers->ElementAt(index);
      if (vm == mViewManager) {
         found = PR_TRUE;
      }
    }

    if (found) {
    ((nsViewManager *)mViewManager)->ProcessInvalidateEvent();
    } else {
      NS_ASSERTION(PR_FALSE, "bad view manager asked to process invalidate event");
    }

  };
 
  nsIViewManager* mViewManager; // Weak Reference. The viewmanager will destroy any pending
                                // invalidate events in it's destructor.
};

static void PR_CALLBACK HandlePLEvent(nsInvalidateEvent* aEvent)
{
  NS_ASSERTION(nsnull != aEvent,"Event is null");
  aEvent->HandleEvent();
}

static void PR_CALLBACK DestroyPLEvent(nsInvalidateEvent* aEvent)
{
  NS_ASSERTION(nsnull != aEvent,"Event is null");
  delete aEvent;
}

nsInvalidateEvent::nsInvalidateEvent(nsIViewManager* aViewManager)
{
  NS_ASSERTION(aViewManager, "null parameter");  
  mViewManager = aViewManager; // Assign weak reference
  PL_InitEvent(this, aViewManager,
               (PLHandleEventProc) ::HandlePLEvent,
               (PLDestroyEventProc) ::DestroyPLEvent);  
}

//-------------- End Invalidate Event Definition ---------------------------


void
nsViewManager::PostInvalidateEvent()
{
  if (!mPendingInvalidateEvent) {
    nsInvalidateEvent* ev = new nsInvalidateEvent(NS_STATIC_CAST(nsIViewManager*, this));
    NS_ASSERTION(nsnull != ev,"InvalidateEvent is null");
    NS_ASSERTION(nsnull != mEventQueue,"Event queue is null");
    mEventQueue->PostEvent(ev);
    mPendingInvalidateEvent = PR_TRUE;  
  }
}

class nsZPlaceholderView : public nsIView
{
public:
  nsZPlaceholderView(nsIView* aParent) { mParent = aParent; }

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports
  NS_IMETHOD  QueryInterface(const nsIID& aIID, void** aInstancePtr)
  { if (nsnull == aInstancePtr) {
      return NS_ERROR_NULL_POINTER;
    }
  
    *aInstancePtr = nsnull;
  
    if (aIID.Equals(NS_GET_IID(nsIView)) || (aIID.Equals(NS_GET_IID(nsISupports)))) {
      *aInstancePtr = (void*)(nsIView*)this;
      return NS_OK;
    }

    return NS_NOINTERFACE;
  }

  // nsIView
  NS_IMETHOD  Init(nsIViewManager* aManager,
                   const nsRect &aBounds,
                   const nsIView *aParent,
                   nsViewVisibility aVisibilityFlag = nsViewVisibility_kShow)
    { return NS_OK; }

  NS_IMETHOD  Destroy() 
    { delete this; return NS_OK; }
  NS_IMETHOD  GetViewManager(nsIViewManager *&aViewMgr) const
    { NS_ASSERTION(PR_FALSE, "Unimplemented"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD  Paint(nsIRenderingContext& rc, const nsRect& rect,
                    PRUint32 aPaintFlags, PRBool &aResult)
    { aResult = PR_TRUE; return NS_OK; }
  NS_IMETHOD  Paint(nsIRenderingContext& rc, const nsIRegion& region,
                    PRUint32 aPaintFlags, PRBool &aResult)
    { aResult = PR_TRUE; return NS_OK; }
  NS_IMETHOD  HandleEvent(nsGUIEvent *event, PRUint32 aEventFlags, nsEventStatus* aStatus, PRBool aForceHandle, PRBool& aHandled)
    { aHandled = PR_FALSE; return NS_OK; }
  NS_IMETHOD  SetPosition(nscoord x, nscoord y)
    { NS_ASSERTION(PR_FALSE, "Unimplemented"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD  GetPosition(nscoord *x, nscoord *y) const
    { *x = 0; *y = 0; return NS_OK; }
  NS_IMETHOD  SetDimensions(nscoord width, nscoord height, PRBool aPaint = PR_TRUE)
    { NS_ASSERTION(PR_FALSE, "Unimplemented"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD  GetDimensions(nscoord *width, nscoord *height) const
    { *width = 0; *height = 0; return NS_OK; }
  NS_IMETHOD  SetBounds(const nsRect &aBounds, PRBool aPaint = PR_TRUE)
    { NS_ASSERTION(PR_FALSE, "Unimplemented"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD  SetBounds(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight, PRBool aPaint = PR_TRUE)
    { NS_ASSERTION(PR_FALSE, "Unimplemented"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD  GetBounds(nsRect &aBounds) const
    { aBounds.SetRect(0, 0, 0, 0); return NS_OK; }
  NS_IMETHOD  SetChildClip(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
    { NS_ASSERTION(PR_FALSE, "Unimplemented"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD  GetChildClip(nscoord *aLeft, nscoord *aTop, nscoord *aRight, nscoord *aBottom) const
    { *aLeft = 0; *aTop = 0; *aRight = 0; *aBottom = 0; return NS_OK; }
  NS_IMETHOD  SetVisibility(nsViewVisibility visibility)
    { NS_ASSERTION(PR_FALSE, "Unimplemented"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD  GetVisibility(nsViewVisibility &aVisibility) const
    { aVisibility = nsViewVisibility_kHide; return NS_OK; }
  NS_IMETHOD  SetZParent(nsIView *aZParent)
    { NS_ASSERTION(PR_FALSE, "Unimplemented"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD  GetZParent(nsIView *&aZParent) const
    { aZParent = nsnull; return NS_OK; }
  NS_IMETHOD  SetZIndex(PRInt32 aZIndex)
    { mZindex = aZIndex; return NS_OK; }
  NS_IMETHOD  GetZIndex(PRInt32 &aZIndex) const
    { aZIndex = mZindex; return NS_OK; }
  NS_IMETHOD  SetAutoZIndex(PRBool aAutoZIndex)
    { if (aAutoZIndex)
        mVFlags |= NS_VIEW_PUBLIC_FLAG_AUTO_ZINDEX;
	  else
		mVFlags &= ~NS_VIEW_PUBLIC_FLAG_AUTO_ZINDEX;
	  return NS_OK;
    }
  NS_IMETHOD  GetAutoZIndex(PRBool &aAutoZIndex) const
    { aAutoZIndex = ((mVFlags & NS_VIEW_PUBLIC_FLAG_AUTO_ZINDEX) != 0);
	  return NS_OK;
    }
  NS_IMETHOD  SetFloating(PRBool aFloatingView)
    { NS_ASSERTION(PR_FALSE, "Unimplemented"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD  GetFloating(PRBool &aFloatingView) const
    { aFloatingView = PR_FALSE; return NS_OK; }
  NS_IMETHOD  SetParent(nsIView *aParent)
    { mParent = aParent; return NS_OK; }
  NS_IMETHOD  GetParent(nsIView *&aParent) const
    { aParent = mParent; return NS_OK; }
  NS_IMETHOD  GetNextSibling(nsIView *&aNextSibling) const
    { aNextSibling = mNextSibling; return NS_OK; }
  NS_IMETHOD  SetNextSibling(nsIView* aNextSibling)
    { mNextSibling = aNextSibling; return NS_OK; }
  NS_IMETHOD  InsertChild(nsIView *child, nsIView *sibling)
    { NS_ASSERTION(PR_FALSE, "Unimplemented"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD  RemoveChild(nsIView *child)
    { NS_ASSERTION(mReparentedChild == child, "Removing incorrect child");
      mReparentedChild = nsnull;
      return NS_ERROR_NOT_IMPLEMENTED;
    }
  NS_IMETHOD  GetChildCount(PRInt32 &aCount) const
    { aCount = 0; return NS_OK; }
  NS_IMETHOD  GetChild(PRInt32 index, nsIView*& aChild) const
    { aChild = nsnull; return NS_OK; }
  NS_IMETHOD  SetTransform(nsTransform2D &aXForm)
    { return NS_OK; }
  NS_IMETHOD  GetTransform(nsTransform2D &aXForm) const
    { NS_ASSERTION(PR_FALSE, "Unimplemented"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD  SetOpacity(float opacity)
    { NS_ASSERTION(PR_FALSE, "Unimplemented"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD  GetOpacity(float &aOpacity) const
    { return NS_OK; }
  NS_IMETHOD  HasTransparency(PRBool &aTransparent) const
    { return NS_OK; }
  NS_IMETHOD  SetContentTransparency(PRBool aTransparent)
    { NS_ASSERTION(PR_FALSE, "Unimplemented"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD  SetClientData(void *aData)
    { return NS_OK; }
  NS_IMETHOD  GetClientData(void *&aData) const
    { NS_ASSERTION(PR_FALSE, "Unimplemented"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD  GetOffsetFromWidget(nscoord *aDx, nscoord *aDy, nsIWidget *&aWidget)
    { NS_ASSERTION(PR_FALSE, "Unimplemented"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD  GetDirtyRegion(nsIRegion*& aRegion) const
    { NS_ASSERTION(PR_FALSE, "Unimplemented"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD  CreateWidget(const nsIID &aWindowIID,
                           nsWidgetInitData *aWidgetInitData = nsnull,
                           nsNativeWidget aNative = nsnull,
                           PRBool aEnableDragDrop = PR_TRUE)
    { NS_ASSERTION(PR_FALSE, "Unimplemented"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD  SetWidget(nsIWidget *aWidget)
    { return NS_OK; }
  NS_IMETHOD  GetWidget(nsIWidget *&aWidget) const
    { aWidget = nsnull; return NS_OK; }
  NS_IMETHOD  HasWidget(PRBool *aHasWidget) const
    { *aHasWidget = PR_FALSE; return NS_OK; }
  NS_IMETHOD  List(FILE* out, PRInt32 aIndent) const
    { return NS_OK; }
  NS_IMETHOD  SetViewFlags(PRUint32 aFlags)
    { NS_ASSERTION(PR_FALSE, "Unimplemented"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD  ClearViewFlags(PRUint32 aFlags)
    { NS_ASSERTION(PR_FALSE, "Unimplemented"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD  GetViewFlags(PRUint32 *aFlags) const
    { NS_ASSERTION(PR_FALSE, "Unimplemented"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD  SetCompositorFlags(PRUint32 aFlags)
    { NS_ASSERTION(PR_FALSE, "Unimplemented"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD  GetCompositorFlags(PRUint32 *aFlags)
    { *aFlags = IS_Z_PLACEHOLDER_VIEW; return NS_OK; }
  NS_IMETHOD  GetExtents(nsRect *aExtents)
    { NS_ASSERTION(PR_FALSE, "Unimplemented"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD  GetClippedRect(nsRect& aClippedRect, PRBool& aIsClipped, PRBool& aEmpty) const
    { aClippedRect.SetRect(0, 0, 0, 0); aIsClipped = PR_TRUE; aEmpty = PR_TRUE; return NS_OK; }

  NS_IMETHOD IgnoreSetPosition(PRBool aShouldIgnore)
    { return NS_OK; }
  NS_IMETHOD SynchWidgetSizePosition()
    { return NS_OK; }

  NS_IMETHOD SetReparentedZChild(nsIView *aChild)
    { mReparentedChild = aChild; return NS_OK; }

protected:
  virtual ~nsZPlaceholderView()
  { if (nsnull != mReparentedChild) {
      mReparentedChild->SetZParent(nsnull);
    }
    if (nsnull != mParent)
    {
      nsCOMPtr<nsIViewManager> vm;
      mParent->GetViewManager(*getter_AddRefs(vm));
      if (vm)
      {
        vm->RemoveChild(mParent, this);
      }
      else
      {
        mParent->RemoveChild(this);
      }
    }
  }

protected:
  nsIView  *mParent;

  nsIView  *mNextSibling;
  nsIView  *mReparentedChild;
  PRInt32  mZindex;
  PRInt32  mVFlags;

private:
  NS_IMETHOD_(nsrefcnt) AddRef(void)
    { NS_ASSERTION(PR_FALSE, "Unimplemented"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD_(nsrefcnt) Release(void)
    { NS_ASSERTION(PR_FALSE, "Unimplemented"); return NS_ERROR_NOT_IMPLEMENTED; }
};



PRInt32 nsViewManager::mVMCount = 0;
nsDrawingSurface nsViewManager::mDrawingSurface = nsnull;
nsRect nsViewManager::mDSBounds = nsRect(0, 0, 0, 0);

nsIRenderingContext* nsViewManager::gCleanupContext = nsnull;
nsDrawingSurface nsViewManager::gOffScreen = nsnull;
nsDrawingSurface nsViewManager::gBlack = nsnull;
nsDrawingSurface nsViewManager::gWhite = nsnull;
nsSize nsViewManager::gOffScreenSize = nsSize(0, 0);
nsSize nsViewManager::gLargestRequestedSize = nsSize(0, 0);


// Weakly held references to all of the view managers
nsVoidArray* nsViewManager::gViewManagers = nsnull;
PRUint32 nsViewManager::gLastUserEventTime = 0;

nsViewManager::nsViewManager()
{
	NS_INIT_REFCNT();

  if (gViewManagers == nsnull) {
    NS_ASSERTION(mVMCount == 0, "View Manager count is incorrect");
    // Create an array to hold a list of view managers
    gViewManagers = new nsVoidArray;
  }
 
  if (gCleanupContext == nsnull) {
    nsComponentManager::CreateInstance(kRenderingContextCID, 
    nsnull, NS_GET_IID(nsIRenderingContext), (void**)&gCleanupContext);
    NS_ASSERTION(gCleanupContext != nsnull, "Wasn't able to create a graphics context for cleanup");
  }

  gViewManagers->AppendElement(this);

	mVMCount++;
	// NOTE:  we use a zeroing operator new, so all data members are
	// assumed to be cleared here.
  mX = 0;
  mY = 0;
  mCachingWidgetChanges = 0;
  mDefaultBackgroundColor = NS_RGBA(0, 0, 0, 0);
  mAllowDoubleBuffering = PR_TRUE; 
  mHasPendingInvalidates = PR_FALSE;
  mPendingInvalidateEvent = PR_FALSE;
  mRecursiveRefreshPending = PR_FALSE;
}

nsViewManager::~nsViewManager()
{
      // Revoke pending invalidate events
  if (mPendingInvalidateEvent) {
    NS_ASSERTION(nsnull != mEventQueue,"Event queue is null"); 
    mPendingInvalidateEvent = PR_FALSE;
    mEventQueue->RevokeEvents(this);
  }

	NS_IF_RELEASE(mRootWindow);

	mRootScrollable = nsnull;

	NS_ASSERTION((mVMCount > 0), "underflow of viewmanagers");
	--mVMCount;

  PRBool removed = gViewManagers->RemoveElement(this);
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
        if (nsnull != mDrawingSurface)
          gCleanupContext->DestroyDrawingSurface(mDrawingSurface);

        if (nsnull != gOffScreen)
          gCleanupContext->DestroyDrawingSurface(gOffScreen);

        if (nsnull != gWhite)
          gCleanupContext->DestroyDrawingSurface(gWhite);

        if (nsnull != gBlack)
          gCleanupContext->DestroyDrawingSurface(gBlack);

      } else {
        NS_ASSERTION(PR_FALSE, "Cleanup of drawing surfaces + offscreen buffer failed");
      }

      mDrawingSurface = nsnull;
      gOffScreen = nsnull;
      gWhite = nsnull;
      gBlack = nsnull;
      gOffScreenSize.SizeTo(0, 0);

      NS_IF_RELEASE(gCleanupContext);
  }

	mObserver = nsnull;
	mContext = nsnull;

	NS_IF_RELEASE(mBlender);
	NS_IF_RELEASE(mOpaqueRgn);
	NS_IF_RELEASE(mTmpRgn);

	NS_IF_RELEASE(mOffScreenCX);
	NS_IF_RELEASE(mBlackCX);
	NS_IF_RELEASE(mWhiteCX);

	if (nsnull != mCompositeListeners) {
		mCompositeListeners->Clear();
		NS_RELEASE(mCompositeListeners);
	}
}

NS_IMPL_QUERY_INTERFACE1(nsViewManager, nsIViewManager)

	NS_IMPL_ADDREF(nsViewManager);

nsrefcnt nsViewManager::Release(void)
{
	/* Note funny ordering of use of mRefCnt. We were seeing a problem
	   during the deletion of a view hierarchy where child views,
	   while being destroyed, referenced this view manager and caused
	   the Destroy part of this method to be re-entered. Waiting until
	   the destruction has finished to decrement the refcount
	   prevents that.
  */
	NS_LOG_RELEASE(this, mRefCnt - 1, "nsViewManager");
	if (mRefCnt == 1)
		{
			if (nsnull != mRootView) {
				// Destroy any remaining views
				mRootView->Destroy();
				mRootView = nsnull;
			}
			delete this;
			return 0;
		}
	mRefCnt--;
	return mRefCnt;
}

static nsresult CreateRegion(nsIComponentManager* componentManager, nsIRegion* *result)
{
	*result = nsnull;
	nsIRegion* region = nsnull;
	nsresult rv = componentManager->CreateInstance(kRegionCID, nsnull, NS_GET_IID(nsIRegion), (void**)&region);
	if (NS_SUCCEEDED(rv)) {
		rv = region->Init();
		*result = region;
	}
	return rv;
}

// We don't hold a reference to the presentation context because it
// holds a reference to us.
NS_IMETHODIMP nsViewManager::Init(nsIDeviceContext* aContext, nscoord aX, nscoord aY)
{
	nsresult rv;

	NS_PRECONDITION(nsnull != aContext, "null ptr");

	if (nsnull == aContext) {
		return NS_ERROR_NULL_POINTER;
	}
	if (nsnull != mContext) {
		return NS_ERROR_ALREADY_INITIALIZED;
	}
	mContext = aContext;
	mContext->GetAppUnitsToDevUnits(mTwipsToPixels);
	mContext->GetDevUnitsToAppUnits(mPixelsToTwips);

	mTransCnt = 0;

	mLastRefresh = PR_IntervalNow();

	mRefreshEnabled = PR_TRUE;

	mMouseGrabber = nsnull;
	mKeyGrabber = nsnull;

	// create regions
    mOpaqueRgn = nsnull;
    mTmpRgn = nsnull;

	nsIComponentManager* componentManager = nsnull;
	rv = NS_GetGlobalComponentManager(&componentManager);
	if (NS_SUCCEEDED(rv)) {
		CreateRegion(componentManager, &mOpaqueRgn);
	    CreateRegion(componentManager, &mTmpRgn);
	}

  mX = aX;
  mY = aY;

  if (nsnull == mEventQueue) {
    // Cache the event queue of the current UI thread
    nsCOMPtr<nsIEventQueueService> eventService = 
             do_GetService(kEventQueueServiceCID, &rv);
    if (NS_SUCCEEDED(rv) && (nsnull != eventService)) {                  // XXX this implies that the UI is the current thread.
      rv = eventService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(mEventQueue));
    }

    NS_ASSERTION(nsnull != mEventQueue, "event queue is null");
  }
  
	return rv;
}

NS_IMETHODIMP nsViewManager::GetRootView(nsIView *&aView)
{
	aView = mRootView;
	return NS_OK;
}

NS_IMETHODIMP nsViewManager :: SetRootView(nsIView *aView, nsIWidget* aWidget)
{
	UpdateTransCnt(mRootView, aView);
	// Do NOT destroy the current root view. It's the caller's responsibility
	// to destroy it
	mRootView = aView;

	//now get the window too.
	NS_IF_RELEASE(mRootWindow);

	// The window must be specified through one of the following:
	//* a) The aView has a nsIWidget instance or
	//* b) the aWidget parameter is an nsIWidget instance to render into 
	//*    that is not owned by a view.
	//* c) aView has a parent view managed by a different view manager or

	if (nsnull != aWidget) {
		mRootWindow = aWidget;
		NS_ADDREF(mRootWindow);
		return NS_OK;
	}

	// case b) The aView has a nsIWidget instance
	if (nsnull != mRootView) {
		mRootView->GetWidget(mRootWindow);
		if (nsnull != mRootWindow) {
			return NS_OK;
		}
	}

	// case c)  aView has a parent view managed by a different view manager

	return NS_OK;
}

NS_IMETHODIMP nsViewManager::GetWindowDimensions(nscoord *width, nscoord *height)
{
	if (nsnull != mRootView)
		mRootView->GetDimensions(width, height);
	else
		{
			*width = 0;
			*height = 0;
		}
	return NS_OK;
}

NS_IMETHODIMP nsViewManager::SetWindowDimensions(nscoord width, nscoord height)
{
	// Resize the root view
	if (nsnull != mRootView)
		mRootView->SetDimensions(width, height);

//printf("new dims: %d %d\n", width, height);
  // Inform the presentation shell that we've been resized
	if (nsnull != mObserver)
		mObserver->ResizeReflow(mRootView, width, height);
	//printf("reflow done\n");

	return NS_OK;
}

NS_IMETHODIMP nsViewManager::ResetScrolling(void)
{
	if (nsnull != mRootScrollable)
		mRootScrollable->ComputeScrollOffsets(PR_TRUE);

	return NS_OK;
}

void nsViewManager::Refresh(nsIView *aView, nsIRenderingContext *aContext, nsIRegion *region, PRUint32 aUpdateFlags)
{
	nsRect              wrect;
	nsCOMPtr<nsIRenderingContext> localcx;
	nsDrawingSurface    ds = nsnull;

	if (PR_FALSE == mRefreshEnabled)
		return;

#ifdef NS_VM_PERF_METRICS
  MOZ_TIMER_DEBUGLOG(("Reset nsViewManager::Refresh(region), this=%p\n", this));
  MOZ_TIMER_RESET(mWatch);

  MOZ_TIMER_DEBUGLOG(("Start: nsViewManager::Refresh(region)\n"));
  MOZ_TIMER_START(mWatch);
#endif

	NS_ASSERTION(!(PR_TRUE == mPainting), "recursive painting not permitted");
	if (mPainting) {
		mRecursiveRefreshPending = PR_TRUE;
		return;
	}  

	mPainting = PR_TRUE;

	//printf("refreshing region...\n");
	//force double buffering because of non-opaque views?

	if (mTransCnt > 0)
		aUpdateFlags |= NS_VMREFRESH_DOUBLE_BUFFER;

#ifdef NO_DOUBLE_BUFFER
    aUpdateFlags &= ~NS_VMREFRESH_DOUBLE_BUFFER;
#endif

  if (PR_FALSE == mAllowDoubleBuffering) {
    // Turn off double-buffering of the display
    aUpdateFlags &= ~NS_VMREFRESH_DOUBLE_BUFFER;
  }

	if (nsnull == aContext)
		{
			localcx = getter_AddRefs(CreateRenderingContext(*aView));

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
					listener->WillRefreshRegion(this, aView, aContext, region, aUpdateFlags);
				}
			}
		}
	}

	if (aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER)
		{
			nsCOMPtr<nsIWidget> widget;
			aView->GetWidget(*getter_AddRefs(widget));
			widget->GetClientBounds(wrect);
			wrect.x = wrect.y = 0;
			ds = GetDrawingSurface(*localcx, wrect);
		}

	PRBool  result;
	nsRect  trect;

	if (nsnull != region)
		localcx->SetClipRegion(*region, nsClipCombine_kUnion, result);

	aView->GetBounds(trect);

	localcx->SetClipRect(trect, nsClipCombine_kIntersect, result);

    RenderViews(aView, *localcx, trect, result);

	if ((aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER) && ds)
		localcx->CopyOffScreenBits(ds, wrect.x, wrect.y, wrect, NS_COPYBITS_USE_SOURCE_CLIP_REGION);

	// Subtract the area we just painted from the dirty region
	if ((nsnull != region) && !region->IsEmpty()) {
		nsRect  pixrect = trect;
		float   t2p;
		
		mContext->GetAppUnitsToDevUnits(t2p);
		
		pixrect.ScaleRoundIn(t2p);
		region->Subtract(pixrect.x, pixrect.y, pixrect.width, pixrect.height);
	}

	mLastRefresh = PR_IntervalNow();

	mPainting = PR_FALSE;

	// notify the listeners.
	if (nsnull != mCompositeListeners) {
		PRUint32 listenerCount;
		if (NS_SUCCEEDED(mCompositeListeners->Count(&listenerCount))) {
			nsCOMPtr<nsICompositeListener> listener;
			for (PRUint32 i = 0; i < listenerCount; i++) {
				if (NS_SUCCEEDED(mCompositeListeners->QueryElementAt(i, NS_GET_IID(nsICompositeListener), getter_AddRefs(listener)))) {
					listener->DidRefreshRegion(this, aView, aContext, region, aUpdateFlags);
				}
			}
		}
	}

	if (mRecursiveRefreshPending) {
		UpdateAllViews(aUpdateFlags);
		mRecursiveRefreshPending = PR_FALSE;
	}

#ifdef NS_VM_PERF_METRICS
  MOZ_TIMER_DEBUGLOG(("Stop: nsViewManager::Refresh(region), this=%p\n", this));
  MOZ_TIMER_STOP(mWatch);
  MOZ_TIMER_LOG(("vm2 Paint time (this=%p): ", this));
  MOZ_TIMER_PRINT(mWatch);
#endif

}

void nsViewManager::Refresh(nsIView *aView, nsIRenderingContext *aContext, const nsRect *rect, PRUint32 aUpdateFlags)
{
	nsRect              wrect, brect;
	nsCOMPtr<nsIRenderingContext> localcx;
	nsDrawingSurface    ds = nsnull;

	if (PR_FALSE == mRefreshEnabled)
		return;

#ifdef NS_VM_PERF_METRICS
  MOZ_TIMER_DEBUGLOG(("Reset nsViewManager::Refresh(region), this=%p\n", this));
  MOZ_TIMER_RESET(mWatch);

  MOZ_TIMER_DEBUGLOG(("Start: nsViewManager::Refresh(region)\n"));
  MOZ_TIMER_START(mWatch);
#endif

	NS_ASSERTION(!(PR_TRUE == mPainting), "recursive painting not permitted");
	if (mPainting) {
		mRecursiveRefreshPending = PR_TRUE;
		return;
	}  

	mPainting = PR_TRUE;

	//force double buffering because of non-opaque views?

	//printf("refreshing rect... ");
	//stdout << *rect;
	//printf("\n");
	if (mTransCnt > 0)
		aUpdateFlags |= NS_VMREFRESH_DOUBLE_BUFFER;

#ifdef NO_DOUBLE_BUFFER
    aUpdateFlags &= ~NS_VMREFRESH_DOUBLE_BUFFER;
#endif

  if (PR_FALSE == mAllowDoubleBuffering) {
    // Turn off double-buffering of the display
    aUpdateFlags &= ~NS_VMREFRESH_DOUBLE_BUFFER;
  }

	if (nsnull == aContext)
		{
			localcx = getter_AddRefs(CreateRenderingContext(*aView));

			//couldn't get rendering context. this is ok if at startup
			if (nsnull == localcx) {
				mPainting = PR_FALSE;
				return;
			}
		} else {
			// plain assignment adds a ref
			localcx = aContext;
		}

	// notify the listeners.
	if (nsnull != mCompositeListeners) {
		PRUint32 listenerCount;
		if (NS_SUCCEEDED(mCompositeListeners->Count(&listenerCount))) {
			nsCOMPtr<nsICompositeListener> listener;
			for (PRUint32 i = 0; i < listenerCount; i++) {
				if (NS_SUCCEEDED(mCompositeListeners->QueryElementAt(i, NS_GET_IID(nsICompositeListener), getter_AddRefs(listener)))) {
					listener->WillRefreshRect(this, aView, aContext, rect, aUpdateFlags);
				}
			}
		}
	}

	if (aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER)
		{
			nsCOMPtr<nsIWidget> widget;
			aView->GetWidget(*getter_AddRefs(widget));
			widget->GetClientBounds(wrect);
			brect = wrect;
			wrect.x = wrect.y = 0;
			ds = GetDrawingSurface(*localcx, wrect);
		}

	nsRect trect = *rect;

	PRBool  result;

	localcx->SetClipRect(trect, nsClipCombine_kReplace, result);

	RenderViews(aView, *localcx, trect, result);

	if ((aUpdateFlags & NS_VMREFRESH_DOUBLE_BUFFER) && ds)
		localcx->CopyOffScreenBits(ds, wrect.x, wrect.y, wrect, NS_COPYBITS_USE_SOURCE_CLIP_REGION);

#if 0
	// Subtract the area we just painted from the dirty region
	nsIRegion* dirtyRegion;
	aView->GetDirtyRegion(dirtyRegion);

	if ((nsnull != dirtyRegion) && !dirtyRegion->IsEmpty())
		{
			nsRect  pixrect = trect;
			float   t2p;

			mContext->GetAppUnitsToDevUnits(t2p);

			pixrect.ScaleRoundIn(t2p);
			dirtyRegion->Subtract(pixrect.x, pixrect.y, pixrect.width, pixrect.height);
			NS_RELEASE(dirtyRegion);
		}
#endif

	mLastRefresh = PR_IntervalNow();

	mPainting = PR_FALSE;

	// notify the listeners.
	if (nsnull != mCompositeListeners) {
		PRUint32 listenerCount;
		if (NS_SUCCEEDED(mCompositeListeners->Count(&listenerCount))) {
			nsCOMPtr<nsICompositeListener> listener;
			for (PRUint32 i = 0; i < listenerCount; i++) {
				if (NS_SUCCEEDED(mCompositeListeners->QueryElementAt(i, NS_GET_IID(nsICompositeListener), getter_AddRefs(listener)))) {
					listener->DidRefreshRect(this, aView, aContext, rect, aUpdateFlags);
				}
			}
		}
	}

	if (mRecursiveRefreshPending) {
		UpdateAllViews(aUpdateFlags);
		mRecursiveRefreshPending = PR_FALSE;
	}

#ifdef NS_VM_PERF_METRICS
  MOZ_TIMER_DEBUGLOG(("Stop: nsViewManager::Refresh(region), this=%p\n", this));
  MOZ_TIMER_STOP(mWatch);
  MOZ_TIMER_LOG(("vm2 Paint time (this=%p): ", this));
  MOZ_TIMER_PRINT(mWatch);
#endif
}

void nsViewManager::DefaultRefresh(nsIView* aView, const nsRect* aRect)
{
    nsCOMPtr<nsIWidget> widget;
    GetWidgetForView(aView, getter_AddRefs(widget));
    if (! widget)
        return;

    nsCOMPtr<nsIRenderingContext> context
        = getter_AddRefs(CreateRenderingContext(*aView));

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

// The display-element (indirect) children of aNode are extracted and appended to aBuffer in
// z-order, with the bottom-most elements first.
// Their z-index is set to the z-index they will have in aNode's parent.
// I.e. if aNode's view has "z-index: auto", the nodes will keep their z-index, otherwise
// their z-indices will all be equal to the z-index value of aNode's view.
static void SortByZOrder(DisplayZTreeNode *aNode, nsVoidArray &aBuffer, nsVoidArray &aMergeTmp, PRBool aForceSort) {
  PRBool autoZIndex = PR_TRUE;
  PRInt32 explicitZIndex = 0;

  if (nsnull != aNode->mView) {
    aNode->mView->GetAutoZIndex(autoZIndex);
    if (!autoZIndex) {
      aNode->mView->GetZIndex(explicitZIndex);
    }
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
    SortByZOrder(child, aBuffer, aMergeTmp, PR_FALSE);
  }
  PRInt32 childEndIndex = aBuffer.Count();
  PRBool hasClip = PR_FALSE;
  
  if (childEndIndex - childStartIndex >= 2) {
    DisplayListElement2* firstChild = NS_STATIC_CAST(DisplayListElement2*, aBuffer.ElementAt(childStartIndex));
    if (0 != (firstChild->mFlags & PUSH_CLIP) && firstChild->mView == aNode->mView) {
      hasClip = PR_TRUE;
    }
  }

  if (hasClip) {
    ApplyZOrderStableSort(aBuffer, aMergeTmp, childStartIndex + 1, childEndIndex - 1);
    
    if (autoZIndex && childEndIndex - childStartIndex >= 3) {
      // If we're an auto-z-index, then we have to worry about the possibility that some of
      // our children may be moved by the z-sorter beyond the bounds of the PUSH...POP clip
      // instructions. So basically, we ensure that around every group of children of
      // equal z-index, there is a PUSH...POP element pair with the same z-index. The stable
      // z-sorter will not break up such a group.
      // Note that if we're not an auto-z-index set, then our children will never be broken
      // up so we don't need to do this.
      // We also don't have to worry if we have no real children.
      DisplayListElement2* ePush = NS_STATIC_CAST(DisplayListElement2*, aBuffer.ElementAt(childStartIndex));
      DisplayListElement2* eFirstChild = NS_STATIC_CAST(DisplayListElement2*, aBuffer.ElementAt(childStartIndex + 1));

      ePush->mZIndex = eFirstChild->mZIndex;

      DisplayListElement2* ePop = NS_STATIC_CAST(DisplayListElement2*, aBuffer.ElementAt(childEndIndex - 1));
      DisplayListElement2* eLastChild = NS_STATIC_CAST(DisplayListElement2*, aBuffer.ElementAt(childEndIndex - 2));

      ePop->mZIndex = eLastChild->mZIndex;

      DisplayListElement2* e = eFirstChild;
      for (PRInt32 i = childStartIndex + 1; i < childEndIndex - 2; i++) {
        DisplayListElement2* eNext = NS_STATIC_CAST(DisplayListElement2*, aBuffer.ElementAt(i + 1));
        NS_ASSERTION(e->mZIndex <= eNext->mZIndex, "Display Z-list is not sorted!!");
        if (e->mZIndex != eNext->mZIndex) {
          // need to insert a POP for the last sequence and a PUSH for the next sequence
          DisplayListElement2* newPop = new DisplayListElement2;
          DisplayListElement2* newPush = new DisplayListElement2;

          *newPop = *ePop;
          newPop->mZIndex = e->mZIndex;
          *newPush = *ePush;
          newPush->mZIndex = eNext->mZIndex;
          aBuffer.InsertElementAt(newPop, i + 1);
          aBuffer.InsertElementAt(newPush, i + 2);
          i += 2;
          childEndIndex += 2;
        }
        e = eNext;
      }
    }
  } else if (aForceSort || !autoZIndex) {
    ApplyZOrderStableSort(aBuffer, aMergeTmp, childStartIndex, childEndIndex);
  }

  if (!autoZIndex) {
    for (PRInt32 i = childStartIndex; i < childEndIndex; i++) {
      DisplayListElement2* element = NS_STATIC_CAST(DisplayListElement2*, aBuffer.ElementAt(i));
      element->mZIndex = explicitZIndex;
    }
  }
}

static void PushStateAndClip(nsIRenderingContext **aRCs, PRInt32 aRCCount, nsRect &aRect,
                             PRInt32 aDX, PRInt32 aDY) {
  PRBool clipEmpty;
  nsRect rect = aRect;
  for (PRInt32 i = 0; i < aRCCount; i++) {
    aRCs[i]->PushState();
    if (i == 1) {
      rect.x -= aDX;
      rect.y -= aDY;
    }
    aRCs[i]->SetClipRect(rect, nsClipCombine_kIntersect, clipEmpty);
  }
}

static void PopState(nsIRenderingContext **aRCs, PRInt32 aRCCount) {
  PRBool clipEmpty;
  for (PRInt32 i = 0; i < aRCCount; i++) {
    aRCs[i]->PopState(clipEmpty);
  }
}

void nsViewManager::AddCoveringWidgetsToOpaqueRegion(nsIRegion* aRgn, nsIDeviceContext* aContext,
                                                     nsIView* aRootView) {
    // We accumulate the bounds of widgets obscuring aRootView's widget into mOpaqueRgn.
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
    if (aRgn) {
      aRgn->SetTo(0, 0, 0, 0);
      nsCOMPtr<nsIWidget> widget;
      GetWidgetForView(aRootView, getter_AddRefs(widget));
      if (widget) {
        nsCOMPtr<nsIEnumerator> children(dont_AddRef(widget->GetChildren()));
        if (children) {
          children->First();
          do {
            nsCOMPtr<nsISupports> child;
            if (NS_SUCCEEDED(children->CurrentItem(getter_AddRefs(child)))) {
              nsCOMPtr<nsIWidget> childWidget = do_QueryInterface(child);
              if (childWidget) {
                nsIView* view = nsView::GetViewFor(childWidget);
                if (view) {
                  nsViewVisibility visible = nsViewVisibility_kHide;
                  view->GetVisibility(visible);
                  if (visible == nsViewVisibility_kShow) {
                    PRBool floating = PR_FALSE;
                    view->GetFloating(floating);
                    if (!floating) {
                      nsRect bounds;
                      view->GetBounds(bounds);
                      if (bounds.width > 0 && bounds.height > 0) {
                        nsIView* viewParent = nsnull;
                        view->GetParent(viewParent);

                        while (viewParent && viewParent != aRootView) {
                          nsRect parentBounds;

                          viewParent->GetBounds(parentBounds);
                          bounds.x += parentBounds.x;
                          bounds.y += parentBounds.y;
                          viewParent->GetParent(viewParent);
                        }

                        // maybe we couldn't get the view into the coordinate
                        // system of aRootView (maybe it's not a descendant
                        // view of aRootView?); if so, don't use it
                        if (viewParent) {
                          aRgn->Union(bounds.x, bounds.y, bounds.width, bounds.height);
                        }
                      }
                    }
                  }
                }
              }
            }
          } while (NS_SUCCEEDED(children->Next()));
        }
      }
    }
}

void nsViewManager::RenderViews(nsIView *aRootView, nsIRenderingContext& aRC, const nsRect& aRect, PRBool &aResult)
{
    // compute this view's origin
    nsPoint origin(0, 0);
    ComputeViewOffset(aRootView, &origin);
    
    nsIView *displayRoot = aRootView;
    for (;;) {
      // Mark the view as a parent of the rendered view.
      displayRoot->SetCompositorFlags(IS_PARENT_OF_REFRESHED_VIEW);

      nsIView *displayParent = nsnull;
      displayRoot->GetParent(displayParent);

      if (nsnull == displayParent) {
        break;
      }
      PRBool isFloating = PR_FALSE;
      displayRoot->GetFloating(isFloating);
      PRBool isParentFloating = PR_FALSE;
      displayParent->GetFloating(isParentFloating);

      if (isFloating && !isParentFloating) {
        break;
      }
      displayRoot = displayParent;

      nsRect bounds;
      displayRoot->GetBounds(bounds);
    }
    
    DisplayZTreeNode *zTree;

    if (nsnull != mOpaqueRgn) {
      mOpaqueRgn->SetTo(0, 0, 0, 0);
      AddCoveringWidgetsToOpaqueRegion(mOpaqueRgn, mContext, aRootView);
    }

    nsPoint displayRootOrigin(0, 0);
    ComputeViewOffset(displayRoot, &displayRootOrigin);
    
    // Create the Z-ordered view tree
    PRBool paintFloaters;
    displayRoot->GetFloating(paintFloaters);
    CreateDisplayList(displayRoot, PR_FALSE, zTree, PR_FALSE, origin.x, origin.y,
      aRootView, &aRect, nsnull, displayRootOrigin.x, displayRootOrigin.y, paintFloaters);
    mMapPlaceholderViewToZTreeNode.Reset();
    
    if (nsnull != zTree) {
      // Apply proper Z-order handling
      nsAutoVoidArray mergeTmp;

      SortByZOrder(zTree, mDisplayList, mergeTmp, PR_TRUE);
      // This builds the display list in reverse order to the way the old
      // nsViewManager did it. C'est la vie, it's easier this way.
    }
    
    mDisplayListCount = mDisplayList.Count();
    
    DestroyZTreeNode(zTree);
    
    nsRect fakeClipRect;
    PRInt32 index = 0;
    PRBool anyRendered;
    nsRect finalTransparentRect;

    ReapplyClipInstructions(PR_FALSE, fakeClipRect, index);
    
    OptimizeDisplayList(aRect, finalTransparentRect);

    if (!finalTransparentRect.IsEmpty()) {
      // There are some bits here that aren't going to be completely painted unless we do it now.
      // XXX Which color should we use for these bits?
      aRC.SetColor(NS_RGB(128, 128, 128));
      aRC.FillRect(finalTransparentRect);
#ifdef DEBUG_roc
      printf("XXX: Using final transparent rect, x=%d, y=%d, width=%d, height=%d\n",
        finalTransparentRect.x, finalTransparentRect.y, finalTransparentRect.width, finalTransparentRect.height);
#endif
    }
    
    // initialize various counters. These are updated in OptimizeDisplayListClipping.
    mTranslucentViewCount = 0;
    mTranslucentArea.SetRect(0, 0, 0, 0);
    
    index = 0;
    OptimizeDisplayListClipping(PR_FALSE, fakeClipRect, index, anyRendered);
    
    // We keep a list of all the rendering contexts whose clip rects
    // need to be updated.
    nsIRenderingContext* RCList[4];
    PRInt32 RCCount = 1;
    RCList[0] = &aRC;
    
    // create blending buffers, if necessary.
    if (mTranslucentViewCount > 0) {
      nsresult rv = CreateBlendingBuffers(aRC);
      NS_ASSERTION((rv == NS_OK), "not enough memory to blend");
      if (NS_FAILED(rv)) {
        // fall back by just rendering with transparency.
        mTranslucentViewCount = 0;
        for (PRInt32 i = mDisplayListCount - 1; i>= 0; --i) {
          DisplayListElement2* element = NS_STATIC_CAST(DisplayListElement2*, mDisplayList.ElementAt(i));
          element->mFlags &= ~VIEW_TRANSLUCENT;
        }
      } else {
        RCCount = 4;
        RCList[1] = mBlackCX;
        RCList[2] = mWhiteCX;
        RCList[3] = mOffScreenCX;
      }
      
      if (!finalTransparentRect.IsEmpty()) {
        // There are some bits that aren't going to be completely painted, so
        // make sure we don't leave garbage in the offscreen context 
        mOffScreenCX->SetColor(NS_RGB(128, 128, 128));
        mOffScreenCX->FillRect(nsRect(0, 0, gOffScreenSize.width, gOffScreenSize.height));
      }
      // DEBUGGING:  fill in complete offscreen image in green, to see if we've got a blending bug.
      //mOffScreenCX->SetColor(NS_RGB(0, 255, 0));
      //mOffScreenCX->FillRect(nsRect(0, 0, gOffScreenSize.width, gOffScreenSize.height));
    }
    
    // draw all views in the display list, from back to front.
    for (PRInt32 i = 0; i < mDisplayListCount; i++) {
      DisplayListElement2* element = NS_STATIC_CAST(DisplayListElement2*, mDisplayList.ElementAt(i));
      if (element->mFlags & VIEW_RENDERED) {
        // typical case, just rendering a view.
        // RenderView(element->mView, aRC, aRect, element->mBounds, aResult);
        if (element->mFlags & VIEW_CLIPPED) {
          //Render the view using the clip rect set by it's ancestors
          PushStateAndClip(RCList, RCCount, element->mBounds, mTranslucentArea.x, mTranslucentArea.y);
          RenderDisplayListElement(element, aRC);
          PopState(RCList, RCCount);
        } else {
          RenderDisplayListElement(element, aRC);
        }
        
      } else {
        // special case, pushing or popping clipping.
        if (element->mFlags & PUSH_CLIP) {
          PushStateAndClip(RCList, RCCount, element->mBounds, mTranslucentArea.x, mTranslucentArea.y);
        } else {
          if (element->mFlags & POP_CLIP) {
            PopState(RCList, RCCount);
          }
        }
      }
      
      delete element;
    }
    
    // flush bits back to screen.
    // Must flush back when no clipping is in effect.
    if (mTranslucentViewCount > 0) {
      // DEBUG: is this getting through?
      // mOffScreenCX->SetColor(NS_RGB(0, 0, 0));
      // mOffScreenCX->DrawRect(nsRect(0, 0, mTranslucentArea.width, mTranslucentArea.height));
      aRC.CopyOffScreenBits(gOffScreen, 0, 0, mTranslucentArea,
        NS_COPYBITS_XFORM_DEST_VALUES |
        NS_COPYBITS_TO_BACK_BUFFER);
      // DEBUG: what rectangle are we blitting?
      // aRC.SetColor(NS_RGB(0, 0, 0));
      // aRC.DrawRect(mTranslucentArea);
    }
    
    mDisplayList.Clear();
    
    nsIView *marker = aRootView;
    while (marker != nsnull) {
      // Mark the view with specified flags.
      marker->SetCompositorFlags(0);
      marker->GetParent(marker);
    }
}

void nsViewManager::RenderView(nsIView *aView, nsIRenderingContext &aRC, const nsRect &aDamageRect, nsRect &aGlobalRect, PRBool &aResult)
{
	nsRect  drect;

	NS_ASSERTION((nsnull != aView), "no view");

	aRC.PushState();

	aRC.Translate(aGlobalRect.x, aGlobalRect.y);

	drect.IntersectRect(aDamageRect, aGlobalRect);

	drect.x -= aGlobalRect.x;
	drect.y -= aGlobalRect.y;

	// should use blender here if opacity < 1.0

	aView->Paint(aRC, drect, NS_VIEW_FLAG_JUST_PAINT, aResult);

	aRC.PopState(aResult);
}

void nsViewManager::RenderDisplayListElement(DisplayListElement2* element, nsIRenderingContext &aRC)
{
	PRBool isTranslucent = (element->mFlags & VIEW_TRANSLUCENT) != 0;
    PRBool clipEmpty;
	if (!isTranslucent) {
		aRC.PushState();

		nscoord x = element->mAbsX, y = element->mAbsY;
		aRC.Translate(x, y);

		nsRect drect(element->mBounds.x - x, element->mBounds.y - y,
		             element->mBounds.width, element->mBounds.height);

		element->mView->Paint(aRC, drect, NS_VIEW_FLAG_JUST_PAINT, clipEmpty);
		
		aRC.PopState(clipEmpty);
	}

#if defined(SUPPORT_TRANSLUCENT_VIEWS)	
	if (mTranslucentViewCount > 0 && (isTranslucent || mTranslucentArea.Intersects(element->mBounds))) {
		// transluscency case. if this view is transluscent, have to use the nsIBlender, otherwise, just
		// render in the offscreen. when we reach the last transluscent view, then we flush the bits
		// to the onscreen rendering context.
		
		// compute the origin of the view, relative to the offscreen buffer, which has the
		// same dimensions as mTranslucentArea.
        nscoord x = element->mAbsX, y = element->mAbsY;
        nscoord viewX = x - mTranslucentArea.x, viewY = y - mTranslucentArea.y;

		nsRect damageRect(element->mBounds);
		damageRect.IntersectRect(damageRect, mTranslucentArea);
        // -> coordinates relative to element->mView origin
        damageRect.x -= x, damageRect.y -= y;
		
		if (element->mFlags & VIEW_TRANSLUCENT) {
			nsIView* view = element->mView;

			// paint the view twice, first in the black buffer, then the white;
			// the blender will pick up the touched pixels only.
			PaintView(view, *mBlackCX, viewX, viewY, damageRect);
			// DEBUGGING ONLY
			//aRC.CopyOffScreenBits(gBlack, 0, 0, element->mBounds,
            //					  NS_COPYBITS_XFORM_DEST_VALUES | NS_COPYBITS_TO_BACK_BUFFER);

			PaintView(view, *mWhiteCX, viewX, viewY, damageRect);
			// DEBUGGING ONLY
			//aRC.CopyOffScreenBits(gWhite, 0, 0, element->mBounds,
			//					  NS_COPYBITS_XFORM_DEST_VALUES | NS_COPYBITS_TO_BACK_BUFFER);
			//mOffScreenCX->CopyOffScreenBits(gWhite, 0, 0, nsRect(viewX, viewY, damageRect.width, damageRect.height),
			//					  NS_COPYBITS_XFORM_DEST_VALUES | NS_COPYBITS_TO_BACK_BUFFER);

			float opacity;
			view->GetOpacity(opacity);

            // -> coordinates relative to mTranslucentArea origin
            damageRect.x += viewX, damageRect.y += viewY;

			// perform the blend itself.
            nsRect damageRectInPixels = damageRect;
			damageRectInPixels *= mTwipsToPixels;
			if (damageRectInPixels.width > 0 && damageRectInPixels.height > 0) {
    			mBlender->Blend(damageRectInPixels.x, damageRectInPixels.y,
                                damageRectInPixels.width, damageRectInPixels.height,
    							mBlackCX, mOffScreenCX,
    							damageRectInPixels.x, damageRectInPixels.y,
    							opacity, mWhiteCX,
    							NS_RGB(0, 0, 0), NS_RGB(255, 255, 255));
    		}
 
            // Set the contexts back to their default colors
            // We do that here because we know that whatever the clip region is,
            // everything we just painted is within the clip region so we are
            // sure to be able to overwrite it now.
			mBlackCX->SetColor(NS_RGB(0, 0, 0));
			mBlackCX->FillRect(damageRect);
			mWhiteCX->SetColor(NS_RGB(255, 255, 255));
			mWhiteCX->FillRect(damageRect);
		} else {
			PaintView(element->mView, *mOffScreenCX, viewX, viewY, damageRect);
		}
	}
#endif
}

void nsViewManager::PaintView(nsIView *aView, nsIRenderingContext &aRC, nscoord x, nscoord y,
							   const nsRect &aDamageRect)
{
	aRC.PushState();
	aRC.Translate(x, y);
	PRBool unused;
	aView->Paint(aRC, aDamageRect, NS_VIEW_FLAG_JUST_PAINT, unused);
	aRC.PopState(unused);
}

inline PRInt32 nextPowerOf2(PRInt32 value)
{
	PRInt32 result = 1;
	while (value > result)
		result <<= 1;
	return result;
}

static nsresult NewOffscreenContext(nsIDeviceContext* deviceContext, nsDrawingSurface surface,
									const nsSize& size, nsIRenderingContext* *aResult)
{
	nsresult rv;
	nsIRenderingContext* context;
	rv = nsComponentManager::CreateInstance(kRenderingContextCID, nsnull,
											NS_GET_IID(nsIRenderingContext),
											(void **)&context);
	if (NS_FAILED(rv))
		return rv;
	rv = context->Init(deviceContext, surface);
	if (NS_FAILED(rv))
		return rv;

	// always initialize clipping, linux won't draw images otherwise.
	PRBool clipEmpty;
	nsRect clip(0, 0, size.width, size.height);
	context->SetClipRect(clip, nsClipCombine_kReplace, clipEmpty);
	
	*aResult = context;
	return NS_OK;
}

nsresult nsViewManager::CreateBlendingBuffers(nsIRenderingContext &aRC)
{
	nsresult rv = NS_OK;

	// create a blender, if none exists already.
	if (nsnull == mBlender) {
		rv = nsComponentManager::CreateInstance(kBlenderCID, nsnull, NS_GET_IID(nsIBlender), (void **)&mBlender);
		if (NS_FAILED(rv))
			return rv;
		rv = mBlender->Init(mContext);
		if (NS_FAILED(rv))
			return rv;
	}

	// ensure that the global drawing surfaces are large enough.
	if (mTranslucentArea.width > gOffScreenSize.width || mTranslucentArea.height > gOffScreenSize.height) {
    	nsRect offscreenBounds(0, 0, mTranslucentArea.width, mTranslucentArea.height);
    	offscreenBounds.ScaleRoundOut(mTwipsToPixels);
    	offscreenBounds.width = nextPowerOf2(offscreenBounds.width);
    	offscreenBounds.height = nextPowerOf2(offscreenBounds.height);

    	NS_IF_RELEASE(mOffScreenCX);
    	NS_IF_RELEASE(mBlackCX);
    	NS_IF_RELEASE(mWhiteCX);

    	if (nsnull != gOffScreen) {
    		aRC.DestroyDrawingSurface(gOffScreen);
    		gOffScreen = nsnull;
    	}
    	rv = aRC.CreateDrawingSurface(&offscreenBounds, NS_CREATEDRAWINGSURFACE_FOR_PIXEL_ACCESS, gOffScreen);
    	if (NS_FAILED(rv))
    		return rv;

    	if (nsnull != gBlack) {
    		aRC.DestroyDrawingSurface(gBlack);
    		gBlack = nsnull;
    	}
    	aRC.CreateDrawingSurface(&offscreenBounds, NS_CREATEDRAWINGSURFACE_FOR_PIXEL_ACCESS, gBlack);
    	if (NS_FAILED(rv))
    		return rv;

    	if (nsnull != gWhite) {
    		aRC.DestroyDrawingSurface(gWhite);
    		gWhite = nsnull;
    	}
    	aRC.CreateDrawingSurface(&offscreenBounds, NS_CREATEDRAWINGSURFACE_FOR_PIXEL_ACCESS, gWhite);
    	if (NS_FAILED(rv))
    		return rv;

		offscreenBounds.ScaleRoundIn(mPixelsToTwips);
        gOffScreenSize.width = offscreenBounds.width;
        gOffScreenSize.height = offscreenBounds.height;
    }

	// recreate local offscreen & blending contexts, if necessary.
	if (mOffScreenCX == nsnull) {
		rv = NewOffscreenContext(mContext, gOffScreen, gOffScreenSize, &mOffScreenCX);
    	if (NS_FAILED(rv))
    		return rv;
	}
	if (mBlackCX == nsnull) {
		rv = NewOffscreenContext(mContext, gBlack, gOffScreenSize, &mBlackCX);
    	if (NS_FAILED(rv))
    		return rv;
	}
	if (mWhiteCX == nsnull) {
		rv = NewOffscreenContext(mContext, gWhite, gOffScreenSize, &mWhiteCX);
    	if (NS_FAILED(rv))
    		return rv;
	}

    nsRect fillArea = mTranslucentArea;
    fillArea.x = 0;
    fillArea.y = 0;

    mBlackCX->SetColor(NS_RGB(0, 0, 0));
    mBlackCX->FillRect(fillArea);
    mWhiteCX->SetColor(NS_RGB(255, 255, 255));
    mWhiteCX->FillRect(fillArea);

	return NS_OK;
}

void nsViewManager::ProcessPendingUpdates(nsIView* aView)
{
  // Protect against a null-view.
  if (nsnull == aView) {
    return;
  }
	PRBool hasWidget;
	aView->HasWidget(&hasWidget);
	if (hasWidget) {
		nsCOMPtr<nsIRegion> dirtyRegion;
		aView->GetDirtyRegion(*getter_AddRefs(dirtyRegion));
		if (dirtyRegion != nsnull && !dirtyRegion->IsEmpty()) {
			nsCOMPtr<nsIWidget> widget;
			aView->GetWidget(*getter_AddRefs(widget));
			if (widget) {
				widget->InvalidateRegion(dirtyRegion, PR_FALSE);
			}
			dirtyRegion->Init();
		}
	}

	// process pending updates in child view.
	nsIView* childView = nsnull;
	aView->GetChild(0, childView);
	while (nsnull != childView)	{
		ProcessPendingUpdates(childView);
		childView->GetNextSibling(childView);
	}

}

NS_IMETHODIMP nsViewManager::Composite()
{
	if (mUpdateCnt > 0)
		{
			if (nsnull != mRootWindow)
				mRootWindow->Update();

			mUpdateCnt = 0;
		}

	return NS_OK;
}

NS_IMETHODIMP nsViewManager::UpdateView(nsIView *aView, PRUint32 aUpdateFlags)
{
	// Mark the entire view as damaged
	nsRect bounds;
	aView->GetBounds(bounds);
	bounds.x = bounds.y = 0;
	return UpdateView(aView, bounds, aUpdateFlags);
}


// Invalidate all widgets which overlap the view, other than the view's own widgets.
NS_IMETHODIMP
nsViewManager::UpdateViewAfterScroll(nsIView *aView, PRInt32 aDX, PRInt32 aDY)
{
    nsPoint origin(0, 0);
	ComputeViewOffset(aView, &origin);
    nsRect damageRect;
    aView->GetBounds(damageRect);
    damageRect.x = origin.x;
    damageRect.y = origin.y;

    // if this is a floating view, it isn't covered by any widgets other than
    // its children, which are handled by the widget scroller.
    PRBool viewIsFloating = PR_FALSE;
    aView->GetFloating(viewIsFloating);
    if (viewIsFloating) {
      return NS_OK;
    }

    UpdateAllCoveringWidgets(mRootView, aView, damageRect, PR_FALSE);
    Composite();
    return NS_OK;
}

// Returns true if this view's widget(s) completely cover the rectangle
// The specified rectangle, relative to aView, is invalidated in every widget child of aView.
// If non-null, aTarget and its children are ignored and only widgets above aTarget's widget
// in Z-order are invalidated (if possible).
PRBool nsViewManager::UpdateAllCoveringWidgets(nsIView *aView, nsIView *aTarget,
    nsRect &aDamagedRect, PRBool aRepaintOnlyUnblittableViews)
{
    if (aView == aTarget) {
        aRepaintOnlyUnblittableViews = PR_TRUE;
    }

    nsRect bounds;
    aView->GetBounds(bounds);
    bounds.x = 0;  // aDamagedRect is already in this view's coordinate system
    bounds.y = 0;
    PRBool overlap = bounds.IntersectRect(bounds, aDamagedRect);
    
    if (!overlap) {
        return PR_FALSE;
    }

    PRBool noCropping = bounds == aDamagedRect;
    
    PRBool hasWidget = PR_FALSE;
    if (mRootView == aView) {
      hasWidget = PR_TRUE;
    } else {
      aView->HasWidget(&hasWidget);
    }

    PRUint32 flags = 0;
    aView->GetViewFlags(&flags);
    PRBool isBlittable = (flags & NS_VIEW_PUBLIC_FLAG_DONT_BITBLT) == 0;
    
    nsIView* childView = nsnull;
    aView->GetChild(0, childView);
    PRBool childCovers = PR_FALSE;
    while (nsnull != childView) {
        nsRect childRect = bounds;
        nsRect childBounds;
        childView->GetBounds(childBounds);
        childRect.x -= childBounds.x;
        childRect.y -= childBounds.y;
        if (UpdateAllCoveringWidgets(childView, aTarget, childRect, aRepaintOnlyUnblittableViews)) {
          childCovers = PR_TRUE;
          // we can't stop here. We're not making any assumptions about how the child
          // widgets are z-ordered, and we can't risk failing to invalidate the top-most
          // one.
        }
		childView->GetNextSibling(childView);
	}

    if (!childCovers && (!isBlittable || (hasWidget && !aRepaintOnlyUnblittableViews))) {
        ++mUpdateCnt;

 	    if (!mRefreshEnabled) {
		    // accumulate this rectangle in the view's dirty region, so we can process it later.
		    AddRectToDirtyRegion(aView, bounds);
            mHasPendingInvalidates = PR_TRUE;
        } else {
     	    nsIView* widgetView = GetWidgetView(aView);
	        if (widgetView != nsnull) {
		        ViewToWidget(aView, widgetView, bounds);

                nsCOMPtr<nsIWidget> widget;
                GetWidgetForView(widgetView, getter_AddRefs(widget));
                widget->Invalidate(bounds, PR_FALSE);
			}
        }
    }

    return noCropping && (hasWidget || childCovers);
}

NS_IMETHODIMP nsViewManager::UpdateView(nsIView *aView, const nsRect &aRect, PRUint32 aUpdateFlags)
{
	NS_PRECONDITION(nsnull != aView, "null view");

    // Only Update the rectangle region of the rect that intersects the view's non clipped rectangle
  nsRect clippedRect;
  PRBool isClipped;
  PRBool isEmpty;
  aView->GetClippedRect(clippedRect, isClipped, isEmpty);
  if (isEmpty) {
    return NS_OK;
  }

  nsRect damagedRect;
  damagedRect.x = aRect.x;
  damagedRect.y = aRect.y;
  damagedRect.width = aRect.width;
  damagedRect.height = aRect.height;
  clippedRect.x = 0;
  clippedRect.y = 0;
  damagedRect.IntersectRect(aRect, clippedRect);

   // If the rectangle is not visible then abort
   // without invalidating. This is a performance 
   // enhancement since invalidating a native widget
   // can be expensive.
   // This also checks for silly request like damagedRect.width = 0 or damagedRect.height = 0
  PRBool isVisible;
  IsRectVisible(aView, damagedRect, PR_FALSE, &isVisible);
  if (!isVisible) {
    return NS_OK;
  }

    // if this is a floating view, it isn't covered by any widgets other than
    // its children. In that case we walk up to its parent widget and use
    // that as the root to update from. This also means we update areas that
    // may be outside the parent view(s), which is necessary for floaters.
    PRBool viewIsFloating = PR_FALSE;
    aView->GetFloating(viewIsFloating);
    if (viewIsFloating) {
      nsIView* widgetParent = aView;
      PRBool hasWidget = PR_FALSE;
      widgetParent->HasWidget(&hasWidget);

      while (!hasWidget) {
        nsRect bounds;
        widgetParent->GetBounds(bounds);
        damagedRect.x += bounds.x;
        damagedRect.y += bounds.y;

        widgetParent->GetParent(widgetParent);
        widgetParent->HasWidget(&hasWidget);
      }

      UpdateAllCoveringWidgets(widgetParent, nsnull, damagedRect, PR_FALSE);
    } else {
      nsPoint origin(damagedRect.x, damagedRect.y);
      ComputeViewOffset(aView, &origin);
      damagedRect.x = origin.x;
      damagedRect.y = origin.y;

      UpdateAllCoveringWidgets(mRootView, nsnull, damagedRect, PR_FALSE);
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

void nsViewManager::UpdateViews(nsIView *aView, PRUint32 aUpdateFlags)
{
	// update this view.
	UpdateView(aView, aUpdateFlags);

	// update all children as well.
	nsIView* childView = nsnull;
	aView->GetChild(0, childView);
	while (nsnull != childView)	{
		UpdateViews(childView, aUpdateFlags);
		childView->GetNextSibling(childView);
	}
}

NS_IMETHODIMP nsViewManager::DispatchEvent(nsGUIEvent *aEvent, nsEventStatus *aStatus)
{
	*aStatus = nsEventStatus_eIgnore;

	switch(aEvent->message)
		{
		case NS_SIZE:
			{
				nsIView*      view = nsView::GetViewFor(aEvent->widget);

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
								mContext->GetDevUnitsToAppUnits(p2t);

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
				nsIView *view = nsView::GetViewFor(aEvent->widget);

				if (nsnull != view)
					{
						// The rect is in device units, and it's in the coordinate space of its
						// associated window.
						nsRect  damrect = *((nsPaintEvent*)aEvent)->rect;

						float   p2t;
						mContext->GetDevUnitsToAppUnits(p2t);
						damrect.ScaleRoundOut(p2t);

						// Do an immediate refresh
						if (nsnull != mContext)
							{
								nsRect  viewrect;
								float   varea;

								// Check that there's actually something to paint
								view->GetBounds(viewrect);
								viewrect.x = viewrect.y = 0;
								varea = (float)viewrect.width * viewrect.height;

								if (varea > 0.0000001f)
									{
										// nsRect     arearect;
										PRUint32   updateFlags = 0;

										// Auto double buffering logic.
										// See if the paint region is greater than .25 the area of our view.
										// If so, enable double buffered painting.
             
										// XXX These two lines cause a lot of flicker for drag-over re-drawing - rods
										//arearect.IntersectRect(damrect, viewrect);
  
										//if ((((float)arearect.width * arearect.height) / varea) >  0.25f)
										// XXX rods
										updateFlags |= NS_VMREFRESH_DOUBLE_BUFFER;

										//printf("refreshing: view: %x, %d, %d, %d, %d\n", view, damrect.x, damrect.y, damrect.width, damrect.height);
										// Refresh the view
										if (mRefreshEnabled) {
											Refresh(view, ((nsPaintEvent*)aEvent)->renderingContext, &damrect, updateFlags);
										}
										else {
                                            // since we got an NS_PAINT event, we need to
                                            // draw something so we don't get blank areas.
                                            DefaultRefresh(view, &damrect);
											}
										}
									}
                  *aStatus = nsEventStatus_eConsumeNoDefault;
							}

				break;
			}

		case NS_DESTROY:
			*aStatus = nsEventStatus_eConsumeNoDefault;
			break;


    case NS_DISPLAYCHANGED:

      // Reset the offscreens width and height
      // so it will be reallocated the next time it needs to
      // draw. It needs to be reallocated because it's depth
      // has changed. @see bugzilla bug 6061
      *aStatus = nsEventStatus_eConsumeDoDefault;
      mDSBounds.width = 0;
      mDSBounds.height = 0;
      break;



		default:
			{
				nsIView* baseView;
				nsIView* view;
				nsPoint offset;
				nsIScrollbar* sb;

        if (NS_IS_MOUSE_EVENT(aEvent) || NS_IS_KEY_EVENT(aEvent)) {
          gLastUserEventTime = PR_IntervalToMicroseconds(PR_IntervalNow());
        }

				//Find the view whose coordinates system we're in.
				baseView = nsView::GetViewFor(aEvent->widget);
        
				//Find the view to which we're initially going to send the event 
				//for hittesting.
				if (nsnull != mMouseGrabber && (NS_IS_MOUSE_EVENT(aEvent) || (NS_IS_DRAG_EVENT(aEvent)))) {
					view = mMouseGrabber;
				}
				else if (nsnull != mKeyGrabber && NS_IS_KEY_EVENT(aEvent)) {
					view = mKeyGrabber;
				}
				else if (NS_OK == aEvent->widget->QueryInterface(NS_GET_IID(nsIScrollbar), (void**)&sb)) {
					view = baseView;
					NS_RELEASE(sb);
				}
				else {
					view = mRootView;
				}

				if (nsnull != view) {
					//Calculate the proper offset for the view we're going to
					offset.x = offset.y = 0;
					if (baseView != view) {
						//Get offset from root of baseView
						nsIView *parent;
						nsRect bounds;

						parent = baseView;
						while (nsnull != parent) {
							parent->GetBounds(bounds);
							offset.x += bounds.x;
							offset.y += bounds.y;
							parent->GetParent(parent);
						}

						//Subtract back offset from root of view
						parent = view;
						while (nsnull != parent) {
							parent->GetBounds(bounds);
							offset.x -= bounds.x;
							offset.y -= bounds.y;
							parent->GetParent(parent);
						}
      
					}

					//Dispatch the event
					float p2t, t2p;

					mContext->GetDevUnitsToAppUnits(p2t);
					mContext->GetAppUnitsToDevUnits(t2p);

					//Before we start mucking with coords, make sure we know our baseline
					aEvent->refPoint.x = aEvent->point.x;
					aEvent->refPoint.y = aEvent->point.y;

					aEvent->point.x = NSIntPixelsToTwips(aEvent->point.x, p2t);
					aEvent->point.y = NSIntPixelsToTwips(aEvent->point.y, p2t);

					aEvent->point.x += offset.x;
					aEvent->point.y += offset.y;
 
					PRBool handled = PR_FALSE;
					view->HandleEvent(aEvent, NS_VIEW_FLAG_CHECK_CHILDREN | 
									  NS_VIEW_FLAG_CHECK_PARENT |
									  NS_VIEW_FLAG_CHECK_SIBLINGS,
									  aStatus,
                    PR_TRUE,
									  handled);

					aEvent->point.x -= offset.x;
					aEvent->point.y -= offset.y;

					aEvent->point.x = NSTwipsToIntPixels(aEvent->point.x, t2p);
					aEvent->point.y = NSTwipsToIntPixels(aEvent->point.y, t2p);

					//
					// if the event is an nsTextEvent, we need to map the reply back into platform coordinates
					//
					if (aEvent->message==NS_TEXT_EVENT) {
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

NS_IMETHODIMP nsViewManager::GrabMouseEvents(nsIView *aView, PRBool &aResult)
{
#ifdef DEBUG_mjudge
  if (aView)
  {
    printf("capturing mouse events for view %x\n",aView);
  }
  printf("removing mouse capture from view %x\n",mMouseGrabber);
#endif

	mMouseGrabber = aView;
	aResult = PR_TRUE;
	return NS_OK;
}

NS_IMETHODIMP nsViewManager::GrabKeyEvents(nsIView *aView, PRBool &aResult)
{
	mKeyGrabber = aView;
	aResult = PR_TRUE;
	return NS_OK;
}

NS_IMETHODIMP nsViewManager::GetMouseEventGrabber(nsIView *&aView)
{
	aView = mMouseGrabber;
	return NS_OK;
}

NS_IMETHODIMP nsViewManager::GetKeyEventGrabber(nsIView *&aView)
{
	aView = mKeyGrabber;
	return NS_OK;
}

NS_IMETHODIMP nsViewManager::InsertChild(nsIView *parent, nsIView *child, nsIView *sibling,
										  PRBool above)
{
	NS_PRECONDITION(nsnull != parent, "null ptr");
	NS_PRECONDITION(nsnull != child, "null ptr");

	if ((nsnull != parent) && (nsnull != child))
		{
			nsIView *kid;
			nsIView *prev = nsnull;

			//verify that the sibling exists...

			parent->GetChild(0, kid);

			while (nsnull != kid)
				{
					if (kid == sibling)
						break;

					//get the next sibling view

					prev = kid;
					kid->GetNextSibling(kid);
				}

			if (nsnull != kid)
				{
					//it's there, so do the insertion

					if (PR_TRUE == above)
						parent->InsertChild(child, prev);
					else
						parent->InsertChild(child, sibling);
				}

			UpdateTransCnt(nsnull, child);

			// if the parent view is marked as "floating", make the newly added view float as well.
			PRBool isFloating = PR_FALSE;
			parent->GetFloating(isFloating);
			if (isFloating)
				child->SetFloating(isFloating);

			//and mark this area as dirty if the view is visible...

			nsViewVisibility  visibility;
			child->GetVisibility(visibility);

			if (nsViewVisibility_kHide != visibility)
				UpdateView(child, NS_VMREFRESH_NO_SYNC);
		}
	return NS_OK;
}

NS_IMETHODIMP nsViewManager::InsertZPlaceholder(nsIView *parent, nsIView *child, PRInt32 zindex)
{
	NS_PRECONDITION(nsnull != parent, "null ptr");
	NS_PRECONDITION(nsnull != child, "null ptr");

    if ((nsnull != parent) && (nsnull != child))
    {
      nsIView *kid;
      nsIView *prev = nsnull;
      
      parent->GetChild(0, kid);
      while (nsnull != kid)
      {
        PRInt32 idx;
        kid->GetZIndex(idx);
        
        if (zindex >= idx)
          break;
        
        prev = kid;
        kid->GetNextSibling(kid);
      }

      nsZPlaceholderView* placeholder = new nsZPlaceholderView(parent);
      placeholder->SetReparentedZChild(child);
      child->SetZParent(placeholder);

      parent->InsertChild(placeholder, prev);
    }
    return NS_OK;
}

NS_IMETHODIMP nsViewManager::InsertChild(nsIView *parent, nsIView *child, PRInt32 zindex)
{
	NS_PRECONDITION(nsnull != parent, "null ptr");
	NS_PRECONDITION(nsnull != child, "null ptr");

	if ((nsnull != parent) && (nsnull != child))
		{
			nsIView *kid;
			nsIView *prev = nsnull;

			//find the right insertion point...

			parent->GetChild(0, kid);

			while (nsnull != kid)
				{
					PRInt32 idx;

					kid->GetZIndex(idx);

					if (zindex >= idx)
						break;

					//get the next sibling view

					prev = kid;
					kid->GetNextSibling(kid);
				}

			//in case this hasn't been set yet... maybe we should not do this? MMP

			child->SetZIndex(zindex);
			parent->InsertChild(child, prev);

			UpdateTransCnt(nsnull, child);

			// if the parent view is marked as "floating", make the newly added view float as well.
			PRBool isFloating = PR_FALSE;
			parent->GetFloating(isFloating);
			if (isFloating)
				child->SetFloating(isFloating);

			//and mark this area as dirty if the view is visible...
			nsViewVisibility  visibility;
			child->GetVisibility(visibility);

			if (nsViewVisibility_kHide != visibility)
				UpdateView(child, NS_VMREFRESH_NO_SYNC);
		}
	return NS_OK;
}

NS_IMETHODIMP nsViewManager::RemoveChild(nsIView *parent, nsIView *child)
{
	NS_PRECONDITION(nsnull != parent, "null ptr");
	NS_PRECONDITION(nsnull != child, "null ptr");

	if ((nsnull != parent) && (nsnull != child))
		{
			UpdateTransCnt(child, nsnull);
			UpdateView(child, NS_VMREFRESH_NO_SYNC);
			parent->RemoveChild(child);
		}

	return NS_OK;
}

NS_IMETHODIMP nsViewManager::MoveViewBy(nsIView *aView, nscoord aX, nscoord aY)
{
	nscoord x, y;

	aView->GetPosition(&x, &y);
	MoveViewTo(aView, aX + x, aY + y);
	return NS_OK;
}

NS_IMETHODIMP nsViewManager::MoveViewTo(nsIView *aView, nscoord aX, nscoord aY)
{
	nscoord oldX, oldY;
	aView->GetPosition(&oldX, &oldY);
	aView->SetPosition(aX, aY);

	// only do damage control if the view is visible

	if ((aX != oldX) || (aY != oldY)) {
		nsViewVisibility  visibility;
		aView->GetVisibility(visibility);
		if (visibility != nsViewVisibility_kHide) {
			nsRect  bounds;
			aView->GetBounds(bounds);
			nsRect oldArea(oldX, oldY, bounds.width, bounds.height);
			nsIView* parentView;
			aView->GetParent(parentView);
			UpdateView(parentView, oldArea, NS_VMREFRESH_NO_SYNC);
			nsRect newArea(aX, aY, bounds.width, bounds.height); 
			UpdateView(parentView, newArea, NS_VMREFRESH_NO_SYNC);
		}
	}
	return NS_OK;
}

NS_IMETHODIMP nsViewManager::ResizeView(nsIView *aView, nscoord width, nscoord height, PRBool aRepaintExposedAreaOnly)
{
  nscoord oldWidth, oldHeight;
  aView->GetDimensions(&oldWidth, &oldHeight);
  if ((width != oldWidth) || (height != oldHeight)) {
    nscoord x = 0, y = 0;
    nsIView* parentView = nsnull;
      aView->GetParent(parentView);
      if (parentView != nsnull)
      aView->GetPosition(&x, &y);
    else
      parentView = aView;

     // resize the view.
     nsViewVisibility  visibility;
     aView->GetVisibility(visibility);

     // Prevent Invalidation of hidden views 
     if (visibility == nsViewVisibility_kHide) {  
       aView->SetDimensions(width, height, PR_FALSE);
     } else {
       if (!aRepaintExposedAreaOnly) {
          //Invalidate the union of the old and new size
         aView->SetDimensions(width, height, PR_TRUE);
         nscoord maxWidth = (oldWidth < width ? width : oldWidth);
         nscoord maxHeight = (oldHeight < height ? height : oldHeight);
         nsRect boundingArea(x, y, maxWidth, maxHeight);
         UpdateView(parentView, boundingArea, NS_VMREFRESH_NO_SYNC);
       } else {
         // Invalidate only the newly exposed or contracted region
         nscoord shortWidth, longWidth, shortHeight, longHeight;
         if (width < oldWidth) { 
           shortWidth = width; 
           longWidth = oldWidth; 
         } 
         else { 
           shortWidth = oldWidth; 
           longWidth = width; 
         } 
  
         if (height < oldHeight) { 
           shortHeight = height; 
           longHeight = oldHeight; 
         } 
         else { 
           shortHeight = oldHeight; 
           longHeight = height; 
         } 

         nsRect  damageRect; 
     
         //damage the right edge of the parent's view
         damageRect.x = x + shortWidth; 
         damageRect.y = y; 
         damageRect.width = longWidth - shortWidth; 
         damageRect.height = longHeight; 
         UpdateView(parentView, damageRect, NS_VMREFRESH_NO_SYNC); 
            
         //damage the bottom edge of the parent's view
         damageRect.x = x; 
         damageRect.y = y + shortHeight; 
         damageRect.width = longWidth; 
         damageRect.height = longHeight - shortHeight; 
         UpdateView(parentView, damageRect, NS_VMREFRESH_NO_SYNC); 
         

         aView->SetDimensions(width, height);
       } 
     }
  }
  
  return NS_OK;
}

NS_IMETHODIMP nsViewManager::SetViewChildClip(nsIView *aView, nsRect *aRect)
{
	NS_ASSERTION(!(nsnull == aView), "no view");
	NS_ASSERTION(!(nsnull == aRect), "no clip");

	aView->SetChildClip(aRect->x, aRect->y, aRect->XMost(), aRect->YMost());

	UpdateView(aView, *aRect, NS_VMREFRESH_NO_SYNC);

	return NS_OK;
}

NS_IMETHODIMP nsViewManager::SetViewVisibility(nsIView *aView, nsViewVisibility aVisible)
{
	nsViewVisibility  oldVisible;
	aView->GetVisibility(oldVisible);
	if (aVisible != oldVisible) {
		aView->SetVisibility(aVisible);

        PRBool hasWidget = PR_FALSE;
        aView->HasWidget(&hasWidget);
        if (!hasWidget) {
    		if (nsViewVisibility_kHide == aVisible) {
	    		nsIView* parentView = nsnull;
		    	aView->GetParent(parentView);
			    if (parentView) {
				    nsRect  bounds;
    				aView->GetBounds(bounds);
	    			UpdateView(parentView, bounds, NS_VMREFRESH_NO_SYNC);
                }
            }
		    else {
			    UpdateView(aView, NS_VMREFRESH_NO_SYNC);
            }
        }
	}
	return NS_OK;
}

NS_IMETHODIMP nsViewManager::SetViewZIndex(nsIView *aView, PRInt32 aZIndex)
{
	nsresult  rv = NS_OK;

	NS_ASSERTION((aView != nsnull), "no view");

#if 0
	// a little hack to check out a theory:  don't let floating views have any other z-index.
	PRBool isFloating = PR_FALSE;
	aView->GetFloating(isFloating);
	if (isFloating) {
		NS_ASSERTION((aZIndex == 0x7FFFFFFF), "floating view's z-index messed up");
		aZIndex = 0x7FFFFFFF;
	}
#endif

	PRInt32 oldidx;
	aView->GetZIndex(oldidx);

	if (oldidx != aZIndex) {
		nsIView *parent;
		aView->GetParent(parent);
		if (nsnull != parent) {
			//we don't just call the view manager's RemoveChild()
			//so that we can avoid two trips trough the UpdateView()
			//code (one for removal, one for insertion). MMP
			parent->RemoveChild(aView);
			UpdateTransCnt(aView, nsnull);
			rv = InsertChild(parent, aView, aZIndex);
		}

	// XXX The following else block is a workaround and should be cleaned up (bug 43410)
	} else {
		nsCOMPtr<nsIWidget> widget;
		aView->GetWidget(*getter_AddRefs(widget));
		if (widget) {
			widget->SetZIndex(aZIndex);
		}
	}

    nsIView* zParentView = nsnull;
    aView->GetZParent(zParentView);
    if (nsnull != zParentView) {
      SetViewZIndex(zParentView, aZIndex);
    }

	return rv;
}

NS_IMETHODIMP nsViewManager::SetViewAutoZIndex(nsIView *aView, PRBool aAutoZIndex)
{
	return aView->SetAutoZIndex(aAutoZIndex);
}

NS_IMETHODIMP nsViewManager::MoveViewAbove(nsIView *aView, nsIView *aOther)
{
	nsresult  rv;

	NS_ASSERTION(!(nsnull == aView), "no view");
	NS_ASSERTION(!(nsnull == aOther), "no view");

	nsIView *nextview;

	aView->GetNextSibling(nextview);
 
	if (nextview != aOther)
		{
			nsIView *parent;

			aView->GetParent(parent);

			if (nsnull != parent)
				{
					//we don't just call the view manager's RemoveChild()
					//so that we can avoid two trips trough the UpdateView()
					//code (one for removal, one for insertion). MMP

					parent->RemoveChild(aView);
					UpdateTransCnt(aView, nsnull);
					rv = InsertChild(parent, aView, aOther, PR_TRUE);
				}
			else
				rv = NS_OK;
		}
	else
		rv = NS_OK;

	return rv;
}

NS_IMETHODIMP nsViewManager::MoveViewBelow(nsIView *aView, nsIView *aOther)
{
	nsresult  rv;

	NS_ASSERTION(!(nsnull == aView), "no view");
	NS_ASSERTION(!(nsnull == aOther), "no view");

	nsIView *nextview;

	aOther->GetNextSibling(nextview);
 
	if (nextview != aView)
		{
			nsIView *parent;

			aView->GetParent(parent);

			if (nsnull != parent)
				{
					//we don't just call the view manager's RemoveChild()
					//so that we can avoid two trips trough the UpdateView()
					//code (one for removal, one for insertion). MMP

					parent->RemoveChild(aView);
					UpdateTransCnt(aView, nsnull);
					rv = InsertChild(parent, aView, aOther, PR_FALSE);
				}
			else
				rv = NS_OK;
		}
	else
		rv = NS_OK;

	return rv;
}

NS_IMETHODIMP nsViewManager::IsViewShown(nsIView *aView, PRBool &aResult)
{
	aResult = PR_TRUE;
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsViewManager::GetViewClipAbsolute(nsIView *aView, nsRect *rect, PRBool &aResult)
{
	aResult = PR_TRUE;
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsViewManager::SetViewContentTransparency(nsIView *aView, PRBool aTransparent)
{
	PRBool trans;

	aView->HasTransparency(trans);

	if (trans != aTransparent)
		{
			UpdateTransCnt(aView, nsnull);
			aView->SetContentTransparency(aTransparent);
			UpdateTransCnt(nsnull, aView);
			UpdateView(aView, NS_VMREFRESH_NO_SYNC);
		}

	return NS_OK;
}

NS_IMETHODIMP nsViewManager::SetViewOpacity(nsIView *aView, float aOpacity)
{
	float opacity;

	aView->GetOpacity(opacity);

	if (opacity != aOpacity)
		{
			UpdateTransCnt(aView, nsnull);
			aView->SetOpacity(aOpacity);
			UpdateTransCnt(nsnull, aView);
			UpdateView(aView, NS_VMREFRESH_NO_SYNC);
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

    nsIViewManager* vm = (nsIViewManager*)gViewManagers->ElementAt(index);
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

PRBool nsViewManager::RectFitsInside(nsRect& aRect, PRInt32 aWidth, PRInt32 aHeight) const
{
  if (aRect.width > aWidth)
    return (PR_FALSE);

  if (aRect.height > aHeight)
    return (PR_FALSE);

  return PR_TRUE;
}

PRBool nsViewManager::BothRectsFitInside(nsRect& aRect1, nsRect& aRect2, PRInt32 aWidth, PRInt32 aHeight, nsRect& aNewSize) const
{
  if (PR_FALSE == RectFitsInside(aRect1, aWidth, aHeight)) {
    return PR_FALSE;
  }

  if (PR_FALSE == RectFitsInside(aRect2, aWidth, aHeight)) {
    return PR_FALSE;
  }

  aNewSize.width = aWidth;
  aNewSize.height = aHeight;

  return PR_TRUE;
}


void nsViewManager::CalculateDiscreteSurfaceSize(nsRect& aRequestedSize, nsRect& aSurfaceSize) const
{
  nsRect aMaxWidgetSize;
  GetMaxWidgetBounds(aMaxWidgetSize);
 
    // Get the height and width of the screen
	PRInt32 height;
  PRInt32 width;
  NS_ASSERTION(mContext != nsnull, "The device context is null");
  mContext->GetDeviceSurfaceDimensions(width, height);

	float devUnits;
 	mContext->GetDevUnitsToAppUnits(devUnits);
	PRInt32 screenHeight = NSToIntRound(float( height) / devUnits );
  PRInt32 screenWidth = NSToIntRound(float( width) / devUnits );

  // These tests must go from smallest rectangle to largest rectangle.

  // 1/8 screen
  if (BothRectsFitInside(aRequestedSize, aMaxWidgetSize, screenWidth / 8, screenHeight / 8, aSurfaceSize)) {
    return;
  }

  // 1/4 screen
  if (BothRectsFitInside(aRequestedSize, aMaxWidgetSize, screenWidth / 4, screenHeight / 4, aSurfaceSize)) {
    return;
  }

  // 1/2 screen
  if (BothRectsFitInside(aRequestedSize, aMaxWidgetSize, screenWidth / 2, screenHeight / 2, aSurfaceSize)) {
    return;
  }

  // 3/4 screen
  if (BothRectsFitInside(aRequestedSize, aMaxWidgetSize, (screenWidth * 3) / 4, (screenHeight * 3) / 4, aSurfaceSize)) {
    return;
  }

  // 3/4 screen width full screen height
  if (BothRectsFitInside(aRequestedSize, aMaxWidgetSize, (screenWidth * 3) / 4, screenHeight, aSurfaceSize)) {
    return;
  }

  // Full screen
  if (BothRectsFitInside(aRequestedSize, aMaxWidgetSize, screenWidth, screenHeight, aSurfaceSize)) {
    return;
  }

   // Bigger than Full Screen use the largest request every made.
  if (BothRectsFitInside(aRequestedSize, aMaxWidgetSize, gLargestRequestedSize.width, gLargestRequestedSize.height, aSurfaceSize)) {
    return;
  } else {
     gLargestRequestedSize.width = PR_MAX(aRequestedSize.width, aMaxWidgetSize.width);
     gLargestRequestedSize.height = PR_MAX(aRequestedSize.height, aMaxWidgetSize.height);
     aSurfaceSize.width = gLargestRequestedSize.width;
     aSurfaceSize.height = gLargestRequestedSize.height;
  //   printf("Expanding the largested requested size to %d %d\n", gLargestRequestedSize.width, gLargestRequestedSize.height);
  }
}

void nsViewManager::GetDrawingSurfaceSize(nsRect& aRequestedSize, nsRect& aNewSize) const
{ 
  CalculateDiscreteSurfaceSize(aRequestedSize, aNewSize);
  aNewSize.MoveTo(aRequestedSize.x, aRequestedSize.y);
}

PRInt32 nsViewManager::GetViewManagerCount()
{
  return mVMCount;
}


const nsVoidArray* nsViewManager::GetViewManagerArray() 
{
  return gViewManagers;
}


nsDrawingSurface nsViewManager::GetDrawingSurface(nsIRenderingContext &aContext, nsRect& aBounds) 
{
  nsRect newBounds;
  GetDrawingSurfaceSize(aBounds, newBounds);

	if ((nsnull == mDrawingSurface)
		|| (mDSBounds.width != newBounds.width)
		|| (mDSBounds.height != newBounds.height))
		{
			if (mDrawingSurface) {
				//destroy existing DS
				aContext.DestroyDrawingSurface(mDrawingSurface);
				mDrawingSurface = nsnull;
			}

			nsresult rv = aContext.CreateDrawingSurface(&newBounds, 0, mDrawingSurface);
   //   printf("Allocating a new drawing surface %d %d\n", newBounds.width, newBounds.height);
			if (NS_SUCCEEDED(rv)) {
				mDSBounds = newBounds;
				aContext.SelectOffScreenDrawingSurface(mDrawingSurface);
			} else {
				mDSBounds.SetRect(0,0,0,0);
				mDrawingSurface = nsnull;
			}
		} else {
			aContext.SelectOffScreenDrawingSurface(mDrawingSurface);

			float p2t;
			mContext->GetDevUnitsToAppUnits(p2t);
			nsRect bounds = aBounds;
			bounds *= p2t;

			PRBool clipEmpty;
			aContext.SetClipRect(bounds, nsClipCombine_kReplace, clipEmpty);

      // This is not be needed. Only the part of the offscreen that has been
      // rendered to should be displayed so there no need to
      // clear it out.
			//nscolor col = NS_RGB(255,255,255);
			//aContext.SetColor(col);
			//aContext.FillRect(bounds);
		}

	return mDrawingSurface;
}

NS_IMETHODIMP nsViewManager::ShowQuality(PRBool aShow)
{
	if (nsnull != mRootScrollable)
		mRootScrollable->ShowQuality(aShow);

	return NS_OK;
}

NS_IMETHODIMP nsViewManager::GetShowQuality(PRBool &aResult)
{
	if (nsnull != mRootScrollable)
		mRootScrollable->GetShowQuality(aResult);

	return NS_OK;
}

NS_IMETHODIMP nsViewManager::SetQuality(nsContentQuality aQuality)
{
	if (nsnull != mRootScrollable)
		mRootScrollable->SetQuality(aQuality);

	return NS_OK;
}

nsIRenderingContext * nsViewManager::CreateRenderingContext(nsIView &aView)
{
	nsIView             *par = &aView;
	nsCOMPtr<nsIWidget> win;
	nsIRenderingContext *cx = nsnull;
	nscoord             x, y, ax = 0, ay = 0;

	do
		{
			par->GetWidget(*getter_AddRefs(win));
			if (nsnull != win)
				break;

			//get absolute coordinates of view, but don't
			//add in view pos since the first thing you ever
			//need to do when painting a view is to translate
			//the rendering context by the views pos and other parts
			//of the code do this for us...

			if (par != &aView)
				{
					par->GetPosition(&x, &y);

					ax += x;
					ay += y;
				}

			par->GetParent(par);
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

void nsViewManager::AddRectToDirtyRegion(nsIView* aView, const nsRect &aRect) const
{
	// Find a view with an associated widget. We'll transform this rect from the
	// current view's coordinate system to a "heavyweight" parent view, then convert
	// the rect to pixel coordinates, and accumulate the rect into that view's dirty region.
	nsIView* widgetView = GetWidgetView(aView);
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

void nsViewManager::UpdateTransCnt(nsIView *oldview, nsIView *newview)
{
	if (nsnull != oldview)
		{
			PRBool  hasTransparency;
			float   opacity;

			oldview->HasTransparency(hasTransparency);
			oldview->GetOpacity(opacity);

			if (hasTransparency || (1.0f != opacity))
				mTransCnt--;
		}

	if (nsnull != newview)
		{
			PRBool  hasTransparency;
			float   opacity;

			newview->HasTransparency(hasTransparency);
			newview->GetOpacity(opacity);

			if (hasTransparency || (1.0f != opacity))
				mTransCnt++;
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
  } else {
    PostInvalidateEvent();
  }

	if (aUpdateFlags & NS_VMREFRESH_IMMEDIATE) {
    Composite();
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
	mRootScrollable = aScrollable;

	//XXX this needs to go away when layout start setting this bit on it's own. MMP
	if (mRootScrollable)
		mRootScrollable->SetScrollProperties(NS_SCROLL_PROPERTY_ALWAYS_BLIT);

	return NS_OK;
}

NS_IMETHODIMP nsViewManager::GetRootScrollableView(nsIScrollableView **aScrollable)
{
	*aScrollable = mRootScrollable;
	return NS_OK;
}

NS_IMETHODIMP nsViewManager::Display(nsIView* aView, nscoord aX, nscoord aY)
{
  nsIRenderingContext *localcx = nsnull;
  nsRect              trect;

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

  aView->GetBounds(trect);

  localcx->Translate(aX, aY);

  PRBool  result;

  trect.x = trect.y = 0;
  localcx->SetClipRect(trect, nsClipCombine_kReplace, result);

  // Paint the view. The clipping rect was set above set don't clip again.
  //aView->Paint(*localcx, trect, NS_VIEW_FLAG_CLIP_SET, result);
  RenderViews(aView,*localcx,trect,result);

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

NS_IMETHODIMP nsViewManager::GetWidgetForView(nsIView *aView, nsIWidget **aWidget)
{
	*aWidget = nsnull;
	nsIView *view = aView;
	PRBool hasWidget = PR_FALSE;
	while (!hasWidget && view)
		{
			view->HasWidget(&hasWidget);
			if (!hasWidget)
				view->GetParent(view);
		}

	if (hasWidget) {
		// Widget was found in the view hierarchy
		view->GetWidget(*aWidget);
	} else {
		// No widget was found in the view hierachy, so use try to use the mRootWindow
		if (nsnull != mRootWindow) {
#ifdef NS_DEBUG
			nsCOMPtr<nsIViewManager> vm;
			nsCOMPtr<nsIViewManager> thisInstance(this);
			aView->GetViewManager(*getter_AddRefs(vm));
			NS_ASSERTION(thisInstance == vm, "Must use the view instances view manager when calling GetWidgetForView");
#endif
			*aWidget = mRootWindow;
			NS_ADDREF(mRootWindow);
		}
	}

	return NS_OK;
}


NS_IMETHODIMP nsViewManager::GetWidget(nsIWidget **aWidget)
{
	NS_IF_ADDREF(mRootWindow);
	*aWidget = mRootWindow;
	return NS_OK;
}

NS_IMETHODIMP nsViewManager::ForceUpdate()
{
	if (mRootWindow) {
		mRootWindow->Update();
	}
	return NS_OK;
}

NS_IMETHODIMP nsViewManager::GetOffset(nscoord *aX, nscoord *aY)
{
  NS_ASSERTION(aX != nsnull, "aX pointer is null");
  NS_ASSERTION(aY != nsnull, "aY pointer is null");
  *aX = mX;
  *aY = mY;
  return NS_OK;
}

static nsresult EnsureZTreeNodeCreated(nsIView* aView, DisplayZTreeNode* &aNode) {
  if (nsnull == aNode) {
    aNode = new DisplayZTreeNode;

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

PRBool nsViewManager::CreateDisplayList(nsIView *aView, PRBool aReparentedViewsPresent,
                                         DisplayZTreeNode* &aResult, PRBool aInsideRealView,
                                         nscoord aOriginX, nscoord aOriginY, nsIView *aRealView,
                                         const nsRect *aDamageRect, nsIView *aTopView,
                                         nscoord aX, nscoord aY, PRBool aPaintFloaters)
{
	PRBool retval = PR_FALSE;

    aResult = nsnull;

	NS_ASSERTION((aView != nsnull), "no view");

	if (!aTopView)
		aTopView = aView;

	nsRect bounds;
    aView->GetBounds(bounds);

	if (aView == aTopView) {
		bounds.x = 0;
		bounds.y = 0;
	}

    // -> to global coordinates (relative to aTopView)
	bounds.x += aX;
	bounds.y += aY;

	// is this a clip view?
	PRBool isClipView = IsClipView(aView);
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
    }
    else
      overlap = PR_TRUE;

    // -> to global coordinates (relative to aTopView)
	bounds.x += aOriginX;
	bounds.y += aOriginY;

    if (!overlap && isClipView) {
      return PR_FALSE;
    }

    // Don't paint floating views unless the root view being painted is a floating view.
    // This is important because we may be asked to paint
    // a window that's behind a transient floater; in this case we must paint the real window
    // contents, not the floater contents (bug 63496)
    if (!aPaintFloaters) {
      PRBool isFloating = PR_FALSE;
      aView->GetFloating(isFloating);
      if (isFloating) {
        return PR_FALSE;
      }
    }

    if (!aReparentedViewsPresent) {
      nsIView *childView = nsnull;
      for (aView->GetChild(0, childView); nsnull != childView; childView->GetNextSibling(childView)) {
        nsIView *zParent = nsnull;
        childView->GetZParent(zParent);
        if (nsnull != zParent) {
          aReparentedViewsPresent = PR_TRUE;
          break;
        }
      }

      if (!overlap && !aReparentedViewsPresent) {
        return PR_FALSE;
      }
    }

	PRInt32 childCount;
    nsIView *childView = nsnull;
	aView->GetChildCount(childCount);
	if (childCount > 0) {
		if (isClipView) {
            // -> to refresh-frame coordinates (relative to aRealView)
			bounds.x -= aOriginX;
			bounds.y -= aOriginY;

			retval = AddToDisplayList(aView, aResult, bounds, bounds, POP_CLIP, aX - aOriginX, aY - aOriginY);

			if (retval)
				return retval;

            // -> to global coordinates (relative to aTopView)
			bounds.x += aOriginX;
		bounds.y += aOriginY;
		}

        for (aView->GetChild(0, childView); nsnull != childView; childView->GetNextSibling(childView)) {
          PRInt32 zindex;
          childView->GetZIndex(zindex);
          if (zindex < 0)
            break;

          DisplayZTreeNode* createdNode;
          retval = CreateDisplayList(childView, aReparentedViewsPresent, createdNode,
            aInsideRealView || aRealView == aView,
            aOriginX, aOriginY, aRealView, aDamageRect, aTopView, bounds.x, bounds.y, aPaintFloaters);
          if (createdNode != nsnull) {
            EnsureZTreeNodeCreated(aView, aResult);
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

  		  nsViewVisibility  visibility;
		  float             opacity;
		  PRBool            transparent;

  		  aView->GetVisibility(visibility);
		  aView->GetOpacity(opacity);
		  aView->HasTransparency(transparent);

          if ((nsViewVisibility_kShow == visibility) && (opacity > 0.0f)) {
			PRUint32 flags = VIEW_RENDERED;
			if (transparent)
				flags |= VIEW_TRANSPARENT;
#if defined(SUPPORT_TRANSLUCENT_VIEWS)
			if (opacity < 1.0f)
				flags |= VIEW_TRANSLUCENT;
#endif
			retval = AddToDisplayList(aView, aResult, bounds, irect, flags, aX - aOriginX, aY - aOriginY);
          }

          // -> to global coordinates (relative to aTopView)
          bounds.x += aOriginX;
		  bounds.y += aOriginY;
        } else {
          PRUint32 compositorFlags = 0;
   	      aView->GetCompositorFlags(&compositorFlags);

          if (0 != (compositorFlags & IS_Z_PLACEHOLDER_VIEW)) {
            EnsureZTreeNodeCreated(aView, aResult);
            mMapPlaceholderViewToZTreeNode.Put(new nsVoidKey(aView), aResult);
          }
        }

		// any children with negative z-indices?
		if (!retval && nsnull != childView) {
            for (; nsnull != childView; childView->GetNextSibling(childView)) {
              DisplayZTreeNode* createdNode;
              retval = CreateDisplayList(childView, aReparentedViewsPresent, createdNode,
                aInsideRealView || aRealView == aView,
                aOriginX, aOriginY, aRealView, aDamageRect, aTopView, bounds.x, bounds.y, aPaintFloaters);
              if (createdNode != nsnull) {
                EnsureZTreeNodeCreated(aView, aResult);
                createdNode->mZSibling = aResult->mZChild;
                aResult->mZChild = createdNode;
              }
              
              if (retval)
                break;
            }
		}
	}

    if (childCount > 0 && isClipView) {
        // -> to refresh-frame coordinates (relative to aRealView)
        bounds.x -= aOriginX;
        bounds.y -= aOriginY;

        if (AddToDisplayList(aView, aResult, bounds, bounds, PUSH_CLIP, aX - aOriginX, aY - aOriginY)) {
            retval = PR_TRUE;
        }
    }

    // Reparent any views that need reparenting in the Z-order tree
    if (nsnull != aResult) {
      DisplayZTreeNode* child;
      DisplayZTreeNode** prev = &aResult->mZChild;
      for (child = aResult->mZChild; nsnull != child; child = *prev) {
        nsIView *zParent = nsnull;
        if (nsnull != child->mView) {
          child->mView->GetZParent(zParent);
        }
        if (nsnull != zParent) {
          // unlink the child from the tree
          *prev = child->mZSibling;
          child->mZSibling = nsnull;

          nsVoidKey key(zParent);
          DisplayZTreeNode* placeholder = (DisplayZTreeNode *)mMapPlaceholderViewToZTreeNode.Remove(&key);

          if (nsnull != placeholder) {
          	NS_ASSERTION((placeholder->mDisplayElement == nsnull), "placeholder already has elements?");
	        NS_ASSERTION((placeholder->mZChild == nsnull), "placeholder already has Z-children?");
            placeholder->mDisplayElement = child->mDisplayElement;
            placeholder->mView = child->mView;
            placeholder->mZChild = child->mZChild;
            delete child;
          } else {
            // the placeholder was never added to the display list ...
            // we don't need to display, then
            DestroyZTreeNode(child);
          }
        } else {
          prev = &child->mZSibling;
        }
      }
    }
 
    return retval;
}

PRBool nsViewManager::AddToDisplayList(nsIView *aView, DisplayZTreeNode* &aParent, nsRect &aClipRect, nsRect& aDirtyRect, PRUint32 aFlags,nscoord aAbsX, nscoord aAbsY)
{
  PRBool empty;
  PRBool clipped;
  nsRect clipRect;

    aView->GetClippedRect(clipRect, clipped, empty);
    if (empty) {
      return PR_FALSE;
    }
    clipRect.x += aAbsX;
    clipRect.y += aAbsY;

    if (!clipped) {
      clipRect = aClipRect;
    }

    PRBool overlap = clipRect.IntersectRect(clipRect, aDirtyRect);
    if (!overlap) {
      return PR_FALSE;
    }

	DisplayListElement2* element = new DisplayListElement2;
    if (element == nsnull) {
	  return PR_TRUE;
	}
    DisplayZTreeNode* node = new DisplayZTreeNode;
    if (nsnull == node) {
      delete element;
      return PR_TRUE;
    }

    EnsureZTreeNodeCreated(aView, aParent);

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

// Make sure that all PUSH_CLIP/POP_CLIP pairs are honored.
// They might not be because of the Z-reparenting mess: a fixed-position view might have
// created a display element with bounds that do not reflect the clipping instructions that now
// surround the element. This would cause problems in the optimizer.
void nsViewManager::ReapplyClipInstructions(PRBool aHaveClip, nsRect& aClipRect, PRInt32& aIndex)
{   
  while (aIndex < mDisplayListCount) {
    DisplayListElement2* element = NS_STATIC_CAST(DisplayListElement2*, mDisplayList.ElementAt(aIndex));
    PRInt32 curIndex = aIndex;
    aIndex++;

    if (element->mFlags & VIEW_RENDERED) {
      if (aHaveClip && !element->mBounds.IntersectRect(aClipRect, element->mBounds)) {
        element->mFlags &= ~VIEW_RENDERED;
      }
    }

    if (element->mFlags & PUSH_CLIP) {
      nsRect newClip;
      if (aHaveClip) {
        newClip.IntersectRect(aClipRect, element->mBounds);
      } else {
        newClip = element->mBounds;
      }

      ReapplyClipInstructions(PR_TRUE, newClip, aIndex);
    }

    if (element->mFlags & POP_CLIP) {
      return;
    }
  }
}

/**
   Walk the display list, looking for opaque views, and remove any views that are behind them
   and totally occluded.
   We rely on a good region implementation. If nsIRegion doesn't cut it, we can disable this
   optimization ... or better still, fix nsIRegion on that platform.
   It seems to be good on Windows.

   @param aFinalTransparentRect
       Receives a rectangle enclosing all pixels in the damage rectangle
       which will not be opaquely painted over by the display list.
       Usually this will be empty, but nothing really prevents someone
       from creating a set of views that are (for example) all transparent.
*/
nsresult nsViewManager::OptimizeDisplayList(const nsRect& aDamageRect, nsRect& aFinalTransparentRect)
{
    aFinalTransparentRect = aDamageRect;

    if (nsnull == mOpaqueRgn || nsnull == mTmpRgn) {
        return NS_OK;
    }

	PRInt32 count = mDisplayListCount;
	for (PRInt32 i = count - 1; i >= 0; i--) {
		DisplayListElement2* element = NS_STATIC_CAST(DisplayListElement2*, mDisplayList.ElementAt(i));
		if (element->mFlags & VIEW_RENDERED) {
            mTmpRgn->SetTo(element->mBounds.x, element->mBounds.y, element->mBounds.width, element->mBounds.height);
            mTmpRgn->Subtract(*mOpaqueRgn);

            if (mTmpRgn->IsEmpty()) {
                element->mFlags &= ~VIEW_RENDERED;
            } else {
                mTmpRgn->GetBoundingBox(&element->mBounds.x, &element->mBounds.y,
                  &element->mBounds.width, &element->mBounds.height);

                // a view is opaque if it is neither transparent nor transluscent
                if (!(element->mFlags & (VIEW_TRANSPARENT | VIEW_TRANSLUCENT))) {
                    mOpaqueRgn->Union(element->mBounds.x, element->mBounds.y, element->mBounds.width, element->mBounds.height);
                }
            }
		}
	}

    mTmpRgn->SetTo(aDamageRect.x, aDamageRect.y, aDamageRect.width, aDamageRect.height);
    mTmpRgn->Subtract(*mOpaqueRgn);
    mTmpRgn->GetBoundingBox(&aFinalTransparentRect.x, &aFinalTransparentRect.y,
      &aFinalTransparentRect.width, &aFinalTransparentRect.height);
	
	return NS_OK;
}

// Remove redundant PUSH/POP_CLIP pairs. These could be expensive.
// We also count the translucent views and compute the translucency area in
// this pass.
void nsViewManager::OptimizeDisplayListClipping(PRBool aHaveClip, nsRect& aClipRect, PRInt32& aIndex,
                                                 PRBool& aAnyRendered)
{   
  aAnyRendered = PR_FALSE;

  while (aIndex < mDisplayListCount) {
    DisplayListElement2* element = NS_STATIC_CAST(DisplayListElement2*, mDisplayList.ElementAt(aIndex));
    PRInt32 curIndex = aIndex;
    aIndex++;

    if (element->mFlags & VIEW_RENDERED) {
      // count number of translucent views, and
      // accumulate a rectangle of all translucent
      // views. this will be used to determine which
      // views need to be rendered into the blending
      // buffers.
      if (element->mFlags & VIEW_TRANSLUCENT) {
        if (mTranslucentViewCount++ == 0) {
          mTranslucentArea = element->mBounds;
        } else {
          mTranslucentArea.UnionRect(mTranslucentArea, element->mBounds);
        }
      }
	
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
      OptimizeDisplayListClipping(PR_TRUE, newClip, aIndex, anyRenderedViews);
      DisplayListElement2* popElement = NS_STATIC_CAST(DisplayListElement2*, mDisplayList.ElementAt(aIndex - 1));
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

void nsViewManager::ShowDisplayList(PRInt32 flatlen)
{
	char     nest[400];
	PRInt32  newnestcnt, nestcnt = 0, cnt;

	for (cnt = 0; cnt < 400; cnt++)
		nest[cnt] = ' ';

	float t2p;
	mContext->GetAppUnitsToDevUnits(t2p);

	printf("### display list length=%d ###\n", flatlen);

	for (cnt = 0; cnt < flatlen; cnt++) {
		nsIView   *view, *parent;
		nsRect    rect;
		PRUint32  flags;
		PRInt32   zindex;

		DisplayListElement2* element = (DisplayListElement2*) mDisplayList.ElementAt(cnt);
		view = element->mView;
		rect = element->mBounds;
		flags = element->mFlags;

		nest[nestcnt << 1] = 0;

		view->GetParent(parent);
		view->GetZIndex(zindex);
		rect *= t2p;
		printf("%snsIView@%p [z=%d, x=%d, y=%d, w=%d, h=%d, p=%p]\n",
			   nest, view, zindex,
			   rect.x, rect.y, rect.width, rect.height, parent);

		newnestcnt = nestcnt;

		if (flags)
			{
				printf("%s", nest);

				if (flags & POP_CLIP) {
					printf("POP_CLIP ");
					newnestcnt--;
				}

				if (flags & PUSH_CLIP) {
					printf("PUSH_CLIP ");
					newnestcnt++;
				}

				if (flags & VIEW_RENDERED)
					printf("VIEW_RENDERED ");

				printf("\n");
			}

		nest[nestcnt << 1] = ' ';

		nestcnt = newnestcnt;
	}
}

void nsViewManager::ComputeViewOffset(nsIView *aView, nsPoint *aOrigin)
{
    if (aOrigin) {
        while (aView != nsnull) {
            // compute the view's global position in the view hierarchy.
            nsRect bounds;
            aView->GetBounds(bounds);
            aOrigin->x += bounds.x;
            aOrigin->y += bounds.y;
        
            nsIView *parent;
            aView->GetParent(parent);
            aView = parent;
        }
    }
}

PRBool nsViewManager::DoesViewHaveNativeWidget(nsIView* aView)
{
	nsCOMPtr<nsIWidget> widget;
	aView->GetWidget(*getter_AddRefs(widget));
	if (nsnull != widget)
		return (nsnull != widget->GetNativeData(NS_NATIVE_WIDGET));
	return PR_FALSE;
}

PRBool nsViewManager::IsClipView(nsIView* aView)
{
	nsIClipView *clipView = nsnull;
	nsresult rv = aView->QueryInterface(NS_GET_IID(nsIClipView), (void **)&clipView);
	return (rv == NS_OK && clipView != nsnull);
}


nsIView* nsViewManager::GetWidgetView(nsIView *aView) const
{
	while (aView != nsnull) {
		PRBool hasWidget;
		aView->HasWidget(&hasWidget);
		if (hasWidget)
			return aView;
		aView->GetParent(aView);
	}
	return nsnull;
}

void nsViewManager::ViewToWidget(nsIView *aView, nsIView* aWidgetView, nsRect &aRect) const
{
	while (aView != aWidgetView) {
		nscoord x, y;
		aView->GetPosition(&x, &y);
		aRect.MoveBy(x, y);
		aView->GetParent(aView);
	}
	
	// intersect aRect with bounds of aWidgetView, to prevent generating any illegal rectangles.
	nsRect bounds;
	aWidgetView->GetBounds(bounds);
	bounds.x = bounds.y = 0;
	aRect.IntersectRect(aRect, bounds);
	
	// finally, convert to device coordinates.
	float t2p;
	mContext->GetAppUnitsToDevUnits(t2p);
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
    const nsIView*  clipView;

    scrollingView->GetScrollPosition(aVisibleRect.x, aVisibleRect.y);
    scrollingView->GetClipView(&clipView);
    clipView->GetDimensions(&aVisibleRect.width, &aVisibleRect.height);
  } else {
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}

nsresult nsViewManager::GetAbsoluteRect(nsIView *aView, const nsRect &aRect, 
                                         nsRect& aAbsRect)
{
  nsIScrollableView* scrollingView = nsnull;
  nsIView* scrolledView = nsnull;
  GetRootScrollableView(&scrollingView);
  if (nsnull == scrollingView) { 
    return NS_ERROR_FAILURE;
  }

  scrollingView->GetScrolledView(scrolledView);

   // Calculate the absolute coordinates of the aRect passed in.
   // aRects values are relative to aView
  aAbsRect = aRect;
  nsIView *parentView = aView;
  while ((parentView != nsnull) && (parentView != scrolledView)) {
    nscoord x, y;
    parentView->GetPosition(&x, &y);
    aAbsRect.MoveBy(x, y);
    parentView->GetParent(parentView);
  }

  if (parentView != scrolledView) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


NS_IMETHODIMP nsViewManager::IsRectVisible(nsIView *aView, const nsRect &aRect, PRBool aMustBeEntirelyVisible, PRBool *aIsVisible)
{
  // The parameter PRBool aMustBeEntirelyVisible determines if rectangle that is partially on the screen
  // and partially off the screen should be counted as visible

  *aIsVisible = PR_FALSE;
  if (aRect.width == 0 || aRect.height == 0) {
    return NS_OK;
  }

	// is this view even visible?
	nsViewVisibility  visibility;
	aView->GetVisibility(visibility);
  if (visibility == nsViewVisibility_kHide) {
		return NS_OK; 
  }

   // Calculate the absolute coordinates for the visible rectangle   
  nsRect visibleRect;
  if (GetVisibleRect(visibleRect) == NS_ERROR_FAILURE) {
    *aIsVisible = PR_TRUE;
    return NS_OK;
  }

   // Calculate the absolute coordinates of the aRect passed in.
   // aRects values are relative to aView
  nsRect absRect;
  if ((GetAbsoluteRect(aView, aRect, absRect)) == NS_ERROR_FAILURE) {
    *aIsVisible = PR_TRUE;
    return NS_OK;
  }
 
    // Compare the visible rect against the rect passed in.
  if (aMustBeEntirelyVisible)
    *aIsVisible = visibleRect.Contains(absRect);
  else
    *aIsVisible = absRect.IntersectRect(absRect, visibleRect);

#if 0
  // Debugging code
  static int toggle = 0;
  for (int i = 0; i < toggle; i++) {
    printf(" ");
  }
  if (toggle == 10) {
    toggle = 0;
  } else {
   toggle++;
  }
  printf("***overlaps %d\n", *aIsVisible);
#endif

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

nsresult
nsViewManager::ProcessInvalidateEvent() {
  FlushPendingInvalidates();
  mPendingInvalidateEvent = PR_FALSE;
  return NS_OK;
}

nsresult
nsViewManager::ProcessWidgetChanges(nsIView* aView)
{
  //printf("---------Begin Sync----------\n");
  nsresult rv = aView->SynchWidgetSizePosition();
  if (NS_FAILED(rv))
      return rv;

	nsIView *child;
	aView->GetChild(0, child);
	while (nsnull != child) {
		rv = ProcessWidgetChanges(child);
    if (NS_FAILED(rv))
      return rv;

		child->GetNextSibling(child);
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



