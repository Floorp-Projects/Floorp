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
 *   Dean Tessman <dean_tessman@hotmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsBaseWidget.h"
#include "nsDeviceContext.h"
#include "nsCOMPtr.h"
#include "nsGfxCIID.h"
#include "nsWidgetsCID.h"
#include "nsServiceManagerUtils.h"
#include "nsIScreenManager.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsISimpleEnumerator.h"
#include "nsIContent.h"
#include "nsIServiceManager.h"
#include "mozilla/Preferences.h"
#include "BasicLayers.h"
#include "LayerManagerOGL.h"
#include "nsIXULRuntime.h"
#include "nsIGfxInfo.h"
#include "npapi.h"

#ifdef DEBUG
#include "nsIObserver.h"

static void debug_RegisterPrefCallbacks();

static bool debug_InSecureKeyboardInputMode = false;
#endif

#ifdef NOISY_WIDGET_LEAKS
static PRInt32 gNumWidgets;
#endif

using namespace mozilla::layers;
using namespace mozilla;

nsIContent* nsBaseWidget::mLastRollup = nsnull;

// nsBaseWidget
NS_IMPL_ISUPPORTS1(nsBaseWidget, nsIWidget)


nsAutoRollup::nsAutoRollup()
{
  // remember if mLastRollup was null, and only clear it upon destruction
  // if so. This prevents recursive usage of nsAutoRollup from clearing
  // mLastRollup when it shouldn't.
  wasClear = !nsBaseWidget::mLastRollup;
}

nsAutoRollup::~nsAutoRollup()
{
  if (nsBaseWidget::mLastRollup && wasClear) {
    NS_RELEASE(nsBaseWidget::mLastRollup);
  }
}

//-------------------------------------------------------------------------
//
// nsBaseWidget constructor
//
//-------------------------------------------------------------------------

nsBaseWidget::nsBaseWidget()
: mClientData(nsnull)
, mViewWrapperPtr(nsnull)
, mEventCallback(nsnull)
, mViewCallback(nsnull)
, mContext(nsnull)
, mToolkit(nsnull)
, mCursor(eCursor_standard)
, mWindowType(eWindowType_child)
, mBorderStyle(eBorderStyle_none)
, mOnDestroyCalled(PR_FALSE)
, mUseAcceleratedRendering(PR_FALSE)
, mTemporarilyUseBasicLayerManager(PR_FALSE)
, mBounds(0,0,0,0)
, mOriginalBounds(nsnull)
, mClipRectCount(0)
, mZIndex(0)
, mSizeMode(nsSizeMode_Normal)
, mPopupLevel(ePopupLevelTop)
{
#ifdef NOISY_WIDGET_LEAKS
  gNumWidgets++;
  printf("WIDGETS+ = %d\n", gNumWidgets);
#endif

#ifdef DEBUG
    debug_RegisterPrefCallbacks();
#endif
}


//-------------------------------------------------------------------------
//
// nsBaseWidget destructor
//
//-------------------------------------------------------------------------
nsBaseWidget::~nsBaseWidget()
{
  if (mLayerManager &&
      mLayerManager->GetBackendType() == LayerManager::LAYERS_BASIC) {
    static_cast<BasicLayerManager*>(mLayerManager.get())->ClearRetainerWidget();
  }

  if (mLayerManager) {
    mLayerManager->Destroy();
  }

#ifdef NOISY_WIDGET_LEAKS
  gNumWidgets--;
  printf("WIDGETS- = %d\n", gNumWidgets);
#endif

  NS_IF_RELEASE(mToolkit);
  NS_IF_RELEASE(mContext);
  delete mOriginalBounds;
}


//-------------------------------------------------------------------------
//
// Basic create.
//
//-------------------------------------------------------------------------
void nsBaseWidget::BaseCreate(nsIWidget *aParent,
                              const nsIntRect &aRect,
                              EVENT_CALLBACK aHandleEventFunction,
                              nsDeviceContext *aContext,
                              nsIAppShell *aAppShell,
                              nsIToolkit *aToolkit,
                              nsWidgetInitData *aInitData)
{
  if (nsnull == mToolkit) {
    if (nsnull != aToolkit) {
      mToolkit = (nsIToolkit*)aToolkit;
      NS_ADDREF(mToolkit);
    }
    else {
      if (nsnull != aParent) {
        mToolkit = aParent->GetToolkit();
        NS_IF_ADDREF(mToolkit);
      }
      // it's some top level window with no toolkit passed in.
      // Create a default toolkit with the current thread
#if !defined(USE_TLS_FOR_TOOLKIT)
      else {
        static NS_DEFINE_CID(kToolkitCID, NS_TOOLKIT_CID);
        
        nsresult res;
        res = CallCreateInstance(kToolkitCID, &mToolkit);
        NS_ASSERTION(NS_SUCCEEDED(res), "Can not create a toolkit in nsBaseWidget::Create");
        if (mToolkit)
          mToolkit->Init(PR_GetCurrentThread());
      }
#else /* USE_TLS_FOR_TOOLKIT */
      else {
        nsresult rv;

        rv = NS_GetCurrentToolkit(&mToolkit);
      }
#endif /* USE_TLS_FOR_TOOLKIT */
    }
    
  }
  
  // save the event callback function
  mEventCallback = aHandleEventFunction;
  
  // keep a reference to the device context
  if (aContext) {
    mContext = aContext;
    NS_ADDREF(mContext);
  }
  else {
    mContext = new nsDeviceContext();
    NS_ADDREF(mContext);
    mContext->Init(nsnull);
  }

  if (nsnull != aInitData) {
    mWindowType = aInitData->mWindowType;
    mBorderStyle = aInitData->mBorderStyle;
    mPopupLevel = aInitData->mPopupLevel;
  }

  if (aParent) {
    aParent->AddChild(this);
  }
}

NS_IMETHODIMP nsBaseWidget::CaptureMouse(bool aCapture)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Accessor functions to get/set the client data
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsBaseWidget::GetClientData(void*& aClientData)
{
  aClientData = mClientData;
  return NS_OK;
}

NS_IMETHODIMP nsBaseWidget::SetClientData(void* aClientData)
{
  mClientData = aClientData;
  return NS_OK;
}

already_AddRefed<nsIWidget>
nsBaseWidget::CreateChild(const nsIntRect  &aRect,
                          EVENT_CALLBACK   aHandleEventFunction,
                          nsDeviceContext *aContext,
                          nsIAppShell      *aAppShell,
                          nsIToolkit       *aToolkit,
                          nsWidgetInitData *aInitData,
                          bool             aForceUseIWidgetParent)
{
  nsIWidget* parent = this;
  nsNativeWidget nativeParent = nsnull;

  if (!aForceUseIWidgetParent) {
    // Use only either parent or nativeParent, not both, to match
    // existing code.  Eventually Create() should be divested of its
    // nativeWidget parameter.
    nativeParent = parent ? parent->GetNativeData(NS_NATIVE_WIDGET) : nsnull;
    parent = nativeParent ? nsnull : parent;
    NS_ABORT_IF_FALSE(!parent || !nativeParent, "messed up logic");
  }

  nsCOMPtr<nsIWidget> widget;
  if (aInitData && aInitData->mWindowType == eWindowType_popup) {
    widget = AllocateChildPopupWidget();
  } else {
    static NS_DEFINE_IID(kCChildCID, NS_CHILD_CID);
    widget = do_CreateInstance(kCChildCID);
  }

  if (widget &&
      NS_SUCCEEDED(widget->Create(parent, nativeParent, aRect,
                                  aHandleEventFunction,
                                  aContext, aAppShell, aToolkit,
                                  aInitData))) {
    return widget.forget();
  }

  return nsnull;
}

// Attach a view to our widget which we'll send events to. 
NS_IMETHODIMP
nsBaseWidget::AttachViewToTopLevel(EVENT_CALLBACK aViewEventFunction,
                                   nsDeviceContext *aContext)
{
  NS_ASSERTION((mWindowType == eWindowType_toplevel ||
                mWindowType == eWindowType_dialog ||
                mWindowType == eWindowType_invisible ||
                mWindowType == eWindowType_child),
               "Can't attach to window of that type");

  mViewCallback = aViewEventFunction;

  if (aContext) {
    if (mContext) {
      NS_IF_RELEASE(mContext);
    }
    mContext = aContext;
    NS_ADDREF(mContext);
  }

  return NS_OK;
}

ViewWrapper* nsBaseWidget::GetAttachedViewPtr()
 {
   return mViewWrapperPtr;
 }
 
NS_IMETHODIMP nsBaseWidget::SetAttachedViewPtr(ViewWrapper* aViewWrapper)
 {
   mViewWrapperPtr = aViewWrapper;
   return NS_OK;
 }

NS_METHOD nsBaseWidget::ResizeClient(PRInt32 aX,
                                     PRInt32 aY,
                                     PRInt32 aWidth,
                                     PRInt32 aHeight,
                                     bool aRepaint)
{
  return Resize(aX, aY, aWidth, aHeight, aRepaint);
}

//-------------------------------------------------------------------------
//
// Close this nsBaseWidget
//
//-------------------------------------------------------------------------
NS_METHOD nsBaseWidget::Destroy()
{
  // Just in case our parent is the only ref to us
  nsCOMPtr<nsIWidget> kungFuDeathGrip(this);
  // disconnect from the parent
  nsIWidget *parent = GetParent();
  if (parent) {
    parent->RemoveChild(this);
  }

  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set this nsBaseWidget's parent
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseWidget::SetParent(nsIWidget* aNewParent)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


//-------------------------------------------------------------------------
//
// Get this nsBaseWidget parent
//
//-------------------------------------------------------------------------
nsIWidget* nsBaseWidget::GetParent(void)
{
  return nsnull;
}

//-------------------------------------------------------------------------
//
// Get this nsBaseWidget top level widget
//
//-------------------------------------------------------------------------
nsIWidget* nsBaseWidget::GetTopLevelWidget()
{
  nsIWidget *topLevelWidget = nsnull, *widget = this;
  while (widget) {
    topLevelWidget = widget;
    widget = widget->GetParent();
  }
  return topLevelWidget;
}

//-------------------------------------------------------------------------
//
// Get this nsBaseWidget's top (non-sheet) parent (if it's a sheet)
//
//-------------------------------------------------------------------------
nsIWidget* nsBaseWidget::GetSheetWindowParent(void)
{
  return nsnull;
}

float nsBaseWidget::GetDPI()
{
  return 96.0f;
}

double nsBaseWidget::GetDefaultScale()
{
  return 1.0;
}

//-------------------------------------------------------------------------
//
// Add a child to the list of children
//
//-------------------------------------------------------------------------
void nsBaseWidget::AddChild(nsIWidget* aChild)
{
  NS_PRECONDITION(!aChild->GetNextSibling() && !aChild->GetPrevSibling(),
                  "aChild not properly removed from its old child list");
  
  if (!mFirstChild) {
    mFirstChild = mLastChild = aChild;
  } else {
    // append to the list
    NS_ASSERTION(mLastChild, "Bogus state");
    NS_ASSERTION(!mLastChild->GetNextSibling(), "Bogus state");
    mLastChild->SetNextSibling(aChild);
    aChild->SetPrevSibling(mLastChild);
    mLastChild = aChild;
  }
}


//-------------------------------------------------------------------------
//
// Remove a child from the list of children
//
//-------------------------------------------------------------------------
void nsBaseWidget::RemoveChild(nsIWidget* aChild)
{
  NS_ASSERTION(aChild->GetParent() == this, "Not one of our kids!");
  
  if (mLastChild == aChild) {
    mLastChild = mLastChild->GetPrevSibling();
  }
  if (mFirstChild == aChild) {
    mFirstChild = mFirstChild->GetNextSibling();
  }

  // Now remove from the list.  Make sure that we pass ownership of the tail
  // of the list correctly before we have aChild let go of it.
  nsIWidget* prev = aChild->GetPrevSibling();
  nsIWidget* next = aChild->GetNextSibling();
  if (prev) {
    prev->SetNextSibling(next);
  }
  if (next) {
    next->SetPrevSibling(prev);
  }
  
  aChild->SetNextSibling(nsnull);
  aChild->SetPrevSibling(nsnull);
}


//-------------------------------------------------------------------------
//
// Sets widget's position within its parent's child list.
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseWidget::SetZIndex(PRInt32 aZIndex)
{
  // Hold a ref to ourselves just in case, since we're going to remove
  // from our parent.
  nsCOMPtr<nsIWidget> kungFuDeathGrip(this);
  
  mZIndex = aZIndex;

  // reorder this child in its parent's list.
  nsBaseWidget* parent = static_cast<nsBaseWidget*>(GetParent());
  if (parent) {
    parent->RemoveChild(this);
    // Scope sib outside the for loop so we can check it afterward
    nsIWidget* sib = parent->GetFirstChild();
    for ( ; sib; sib = sib->GetNextSibling()) {
      PRInt32 childZIndex;
      if (NS_SUCCEEDED(sib->GetZIndex(&childZIndex))) {
        if (aZIndex < childZIndex) {
          // Insert ourselves before sib
          nsIWidget* prev = sib->GetPrevSibling();
          mNextSibling = sib;
          mPrevSibling = prev;
          sib->SetPrevSibling(this);
          if (prev) {
            prev->SetNextSibling(this);
          } else {
            NS_ASSERTION(sib == parent->mFirstChild, "Broken child list");
            // We've taken ownership of sib, so it's safe to have parent let
            // go of it
            parent->mFirstChild = this;
          }
          PlaceBehind(eZPlacementBelow, sib, PR_FALSE);
          break;
        }
      }
    }
    // were we added to the list?
    if (!sib) {
      parent->AddChild(this);
    }
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Gets widget's position within its parent's child list.
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseWidget::GetZIndex(PRInt32* aZIndex)
{
  *aZIndex = mZIndex;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Places widget behind the given widget (platforms must override)
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseWidget::PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                                        nsIWidget *aWidget, bool aActivate)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Maximize, minimize or restore the window. The BaseWidget implementation
// merely stores the state.
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseWidget::SetSizeMode(PRInt32 aMode)
{
  if (aMode == nsSizeMode_Normal ||
      aMode == nsSizeMode_Minimized ||
      aMode == nsSizeMode_Maximized ||
      aMode == nsSizeMode_Fullscreen) {

    mSizeMode = (nsSizeMode) aMode;
    return NS_OK;
  }
  return NS_ERROR_ILLEGAL_VALUE;
}

//-------------------------------------------------------------------------
//
// Get the size mode (minimized, maximized, that sort of thing...)
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseWidget::GetSizeMode(PRInt32* aMode)
{
  *aMode = mSizeMode;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the foreground color
//
//-------------------------------------------------------------------------
nscolor nsBaseWidget::GetForegroundColor(void)
{
  return mForeground;
}

    
//-------------------------------------------------------------------------
//
// Set the foreground color
//
//-------------------------------------------------------------------------
NS_METHOD nsBaseWidget::SetForegroundColor(const nscolor &aColor)
{
  mForeground = aColor;
  return NS_OK;
}

    
//-------------------------------------------------------------------------
//
// Get the background color
//
//-------------------------------------------------------------------------
nscolor nsBaseWidget::GetBackgroundColor(void)
{
  return mBackground;
}

//-------------------------------------------------------------------------
//
// Set the background color
//
//-------------------------------------------------------------------------
NS_METHOD nsBaseWidget::SetBackgroundColor(const nscolor &aColor)
{
  mBackground = aColor;
  return NS_OK;
}
     
//-------------------------------------------------------------------------
//
// Get this component cursor
//
//-------------------------------------------------------------------------
nsCursor nsBaseWidget::GetCursor()
{
  return mCursor;
}

NS_METHOD nsBaseWidget::SetCursor(nsCursor aCursor)
{
  mCursor = aCursor; 
  return NS_OK;
}

NS_IMETHODIMP nsBaseWidget::SetCursor(imgIContainer* aCursor,
                                      PRUint32 aHotspotX, PRUint32 aHotspotY)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
    
//-------------------------------------------------------------------------
//
// Get the window type for this widget
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseWidget::GetWindowType(nsWindowType& aWindowType)
{
  aWindowType = mWindowType;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Window transparency methods
//
//-------------------------------------------------------------------------

void nsBaseWidget::SetTransparencyMode(nsTransparencyMode aMode) {
}

nsTransparencyMode nsBaseWidget::GetTransparencyMode() {
  return eTransparencyOpaque;
}

bool
nsBaseWidget::StoreWindowClipRegion(const nsTArray<nsIntRect>& aRects)
{
  if (mClipRects && mClipRectCount == aRects.Length() &&
      memcmp(mClipRects, aRects.Elements(), sizeof(nsIntRect)*mClipRectCount) == 0)
    return PR_FALSE;

  mClipRectCount = aRects.Length();
  mClipRects = new nsIntRect[mClipRectCount];
  if (mClipRects) {
    memcpy(mClipRects, aRects.Elements(), sizeof(nsIntRect)*mClipRectCount);
  }
  return PR_TRUE;
}

void
nsBaseWidget::GetWindowClipRegion(nsTArray<nsIntRect>* aRects)
{
  if (mClipRects) {
    aRects->AppendElements(mClipRects.get(), mClipRectCount);
  } else {
    aRects->AppendElement(nsIntRect(0, 0, mBounds.width, mBounds.height));
  }
}

//-------------------------------------------------------------------------
//
// Set window shadow style
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsBaseWidget::SetWindowShadowStyle(PRInt32 aMode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-------------------------------------------------------------------------
//
// Hide window borders/decorations for this widget
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseWidget::HideWindowChrome(bool aShouldHide)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-------------------------------------------------------------------------
//
// Put the window into full-screen mode
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsBaseWidget::MakeFullScreen(bool aFullScreen)
{
  HideWindowChrome(aFullScreen);

  if (aFullScreen) {
    if (!mOriginalBounds)
      mOriginalBounds = new nsIntRect();
    GetScreenBounds(*mOriginalBounds);

    // Move to top-left corner of screen and size to the screen dimensions
    nsCOMPtr<nsIScreenManager> screenManager;
    screenManager = do_GetService("@mozilla.org/gfx/screenmanager;1"); 
    NS_ASSERTION(screenManager, "Unable to grab screenManager.");
    if (screenManager) {
      nsCOMPtr<nsIScreen> screen;
      screenManager->ScreenForRect(mOriginalBounds->x, mOriginalBounds->y,
                                   mOriginalBounds->width, mOriginalBounds->height,
                                   getter_AddRefs(screen));
      if (screen) {
        PRInt32 left, top, width, height;
        if (NS_SUCCEEDED(screen->GetRect(&left, &top, &width, &height))) {
          Resize(left, top, width, height, PR_TRUE);
        }
      }
    }

  } else if (mOriginalBounds) {
    Resize(mOriginalBounds->x, mOriginalBounds->y, mOriginalBounds->width,
           mOriginalBounds->height, PR_TRUE);
  }

  return NS_OK;
}

nsBaseWidget::AutoLayerManagerSetup::AutoLayerManagerSetup(
    nsBaseWidget* aWidget, gfxContext* aTarget,
    BasicLayerManager::BufferMode aDoubleBuffering)
  : mWidget(aWidget)
{
  BasicLayerManager* manager =
    static_cast<BasicLayerManager*>(mWidget->GetLayerManager());
  if (manager) {
    NS_ASSERTION(manager->GetBackendType() == LayerManager::LAYERS_BASIC,
      "AutoLayerManagerSetup instantiated for non-basic layer backend!");
    manager->SetDefaultTarget(aTarget, aDoubleBuffering);
  }
}

nsBaseWidget::AutoLayerManagerSetup::~AutoLayerManagerSetup()
{
  BasicLayerManager* manager =
    static_cast<BasicLayerManager*>(mWidget->GetLayerManager());
  if (manager) {
    NS_ASSERTION(manager->GetBackendType() == LayerManager::LAYERS_BASIC,
      "AutoLayerManagerSetup instantiated for non-basic layer backend!");
    manager->SetDefaultTarget(nsnull, BasicLayerManager::BUFFER_NONE);
  }
}

nsBaseWidget::AutoUseBasicLayerManager::AutoUseBasicLayerManager(nsBaseWidget* aWidget)
  : mWidget(aWidget)
{
  mWidget->mTemporarilyUseBasicLayerManager = PR_TRUE;
}

nsBaseWidget::AutoUseBasicLayerManager::~AutoUseBasicLayerManager()
{
  mWidget->mTemporarilyUseBasicLayerManager = PR_FALSE;
}

bool
nsBaseWidget::GetShouldAccelerate()
{
#if defined(XP_WIN) || defined(ANDROID) || (MOZ_PLATFORM_MAEMO > 5)
  bool accelerateByDefault = true;
#elif defined(XP_MACOSX)
/* quickdraw plugins don't work with OpenGL so we need to avoid OpenGL when we want to support
 * them. e.g. 10.5 */
# if defined(NP_NO_QUICKDRAW)
  bool accelerateByDefault = true;

  // 10.6.2 and lower have a bug involving textures and pixel buffer objects
  // that caused bug 629016, so we don't allow OpenGL-accelerated layers on
  // those versions of the OS.
  // This will still let full-screen video be accelerated on OpenGL, because
  // that XUL widget opts in to acceleration, but that's probably OK.
  SInt32 major, minor, bugfix;
  OSErr err1 = ::Gestalt(gestaltSystemVersionMajor, &major);
  OSErr err2 = ::Gestalt(gestaltSystemVersionMinor, &minor);
  OSErr err3 = ::Gestalt(gestaltSystemVersionBugFix, &bugfix);
  if (err1 == noErr && err2 == noErr && err3 == noErr) {
    if (major == 10 && minor == 6) {
      if (bugfix <= 2) {
        accelerateByDefault = PR_FALSE;
      }
    }
  }

# else
  bool accelerateByDefault = false;
# endif

#else
  bool accelerateByDefault = false;
#endif

  // we should use AddBoolPrefVarCache
  bool disableAcceleration =
    Preferences::GetBool("layers.acceleration.disabled", false);
  bool forceAcceleration =
    Preferences::GetBool("layers.acceleration.force-enabled", false);

  const char *acceleratedEnv = PR_GetEnv("MOZ_ACCELERATED");
  accelerateByDefault = accelerateByDefault ||
                        (acceleratedEnv && (*acceleratedEnv != '0'));

  nsCOMPtr<nsIXULRuntime> xr = do_GetService("@mozilla.org/xre/runtime;1");
  bool safeMode = false;
  if (xr)
    xr->GetInSafeMode(&safeMode);

  bool whitelisted = false;

  nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
  if (gfxInfo) {
    // bug 655578: on X11 at least, we must always call GetData (even if we don't need that information)
    // as that's what causes GfxInfo initialization which kills the zombie 'glxtest' process.
    // initially we relied on the fact that GetFeatureStatus calls GetData for us, but bug 681026 showed
    // that assumption to be unsafe.
    gfxInfo->GetData();

    PRInt32 status;
    if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_OPENGL_LAYERS, &status))) {
      if (status == nsIGfxInfo::FEATURE_NO_INFO) {
        whitelisted = true;
      }
    }
  }

  if (disableAcceleration || safeMode)
    return PR_FALSE;

  if (forceAcceleration)
    return PR_TRUE;
  
  if (!whitelisted) {
    NS_WARNING("OpenGL-accelerated layers are not supported on this system.");
    return PR_FALSE;
  }

  if (accelerateByDefault)
    return PR_TRUE;

  /* use the window acceleration flag */
  return mUseAcceleratedRendering;
}

LayerManager* nsBaseWidget::GetLayerManager(PLayersChild* aShadowManager,
                                            LayersBackend aBackendHint,
                                            LayerManagerPersistence aPersistence,
                                            bool* aAllowRetaining)
{
  if (!mLayerManager) {

    mUseAcceleratedRendering = GetShouldAccelerate();

    if (mUseAcceleratedRendering) {
      nsRefPtr<LayerManagerOGL> layerManager = new LayerManagerOGL(this);
      /**
       * XXX - On several OSes initialization is expected to fail for now.
       * If we'd get a none-basic layer manager they'd crash. This is ok though
       * since on those platforms it will fail. Anyone implementing new
       * platforms on LayerManagerOGL should ensure their widget is able to
       * deal with it though!
       */
      if (layerManager->Initialize()) {
        mLayerManager = layerManager;
      }
    }
    if (!mLayerManager) {
      mBasicLayerManager = mLayerManager = CreateBasicLayerManager();
    }
  }
  if (mTemporarilyUseBasicLayerManager && !mBasicLayerManager) {
    mBasicLayerManager = CreateBasicLayerManager();
  }
  LayerManager* usedLayerManager = mTemporarilyUseBasicLayerManager ?
                                     mBasicLayerManager : mLayerManager;
  if (aAllowRetaining) {
    *aAllowRetaining = (usedLayerManager == mLayerManager);
  }
  return usedLayerManager;
}

BasicLayerManager* nsBaseWidget::CreateBasicLayerManager()
{
      return new BasicShadowLayerManager(this);
}

//-------------------------------------------------------------------------
//
// Return the toolkit this widget was created on
//
//-------------------------------------------------------------------------
nsIToolkit* nsBaseWidget::GetToolkit()
{
  return mToolkit;
}


//-------------------------------------------------------------------------
//
// Return the used device context
//
//-------------------------------------------------------------------------
nsDeviceContext* nsBaseWidget::GetDeviceContext() 
{
  return mContext; 
}

//-------------------------------------------------------------------------
//
// Get the thebes surface
//
//-------------------------------------------------------------------------
gfxASurface *nsBaseWidget::GetThebesSurface()
{
  // in theory we should get our parent's surface,
  // clone it, and set a device offset before returning
  return nsnull;
}


//-------------------------------------------------------------------------
//
// Destroy the window
//
//-------------------------------------------------------------------------
void nsBaseWidget::OnDestroy()
{
  // release references to device context, toolkit, and app shell
  NS_IF_RELEASE(mContext);
  NS_IF_RELEASE(mToolkit);
}

NS_METHOD nsBaseWidget::SetWindowClass(const nsAString& xulWinType)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-------------------------------------------------------------------------
//
// Bounds
//
//-------------------------------------------------------------------------

/**
* If the implementation of nsWindow supports borders this method MUST be overridden
*
**/
NS_METHOD nsBaseWidget::GetClientBounds(nsIntRect &aRect)
{
  return GetBounds(aRect);
}

/**
* If the implementation of nsWindow supports borders this method MUST be overridden
*
**/
NS_METHOD nsBaseWidget::GetBounds(nsIntRect &aRect)
{
  aRect = mBounds;
  return NS_OK;
}

/**
* If the implementation of nsWindow uses a local coordinate system within the window,
* this method must be overridden
*
**/
NS_METHOD nsBaseWidget::GetScreenBounds(nsIntRect &aRect)
{
  return GetBounds(aRect);
}

nsIntPoint nsBaseWidget::GetClientOffset()
{
  return nsIntPoint(0, 0);
}

NS_METHOD nsBaseWidget::SetBounds(const nsIntRect &aRect)
{
  mBounds = aRect;

  return NS_OK;
}

NS_IMETHODIMP
nsBaseWidget::GetNonClientMargins(nsIntMargin &margins)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
 
NS_IMETHODIMP
nsBaseWidget::SetNonClientMargins(nsIntMargin &margins)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsBaseWidget::EnableDragDrop(bool aEnable)
{
  return NS_OK;
}

NS_METHOD nsBaseWidget::SetModal(bool aModal)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsBaseWidget::GetAttention(PRInt32 aCycleCount) {
    return NS_OK;
}

bool
nsBaseWidget::HasPendingInputEvent()
{
  return PR_FALSE;
}

NS_IMETHODIMP
nsBaseWidget::SetIcon(const nsAString&)
{
  return NS_OK;
}

NS_IMETHODIMP
nsBaseWidget::BeginSecureKeyboardInput()
{
#ifdef DEBUG
  NS_ASSERTION(!debug_InSecureKeyboardInputMode, "Attempting to nest call to BeginSecureKeyboardInput!");
  debug_InSecureKeyboardInputMode = PR_TRUE;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsBaseWidget::EndSecureKeyboardInput()
{
#ifdef DEBUG
  NS_ASSERTION(debug_InSecureKeyboardInputMode, "Calling EndSecureKeyboardInput when it hasn't been enabled!");
  debug_InSecureKeyboardInputMode = PR_FALSE;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsBaseWidget::SetWindowTitlebarColor(nscolor aColor, bool aActive)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

bool
nsBaseWidget::ShowsResizeIndicator(nsIntRect* aResizerRect)
{
  return PR_FALSE;
}

NS_IMETHODIMP
nsBaseWidget::SetAcceleratedRendering(bool aEnabled)
{
  if (mUseAcceleratedRendering == aEnabled) {
    return NS_OK;
  }
  mUseAcceleratedRendering = aEnabled;
  if (mLayerManager) {
    mLayerManager->Destroy();
  }
  mLayerManager = NULL;
  return NS_OK;
}

bool
nsBaseWidget::GetAcceleratedRendering()
{
  return mUseAcceleratedRendering;
}

NS_METHOD nsBaseWidget::RegisterTouchWindow()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsBaseWidget::UnregisterTouchWindow()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBaseWidget::OverrideSystemMouseScrollSpeed(PRInt32 aOriginalDelta,
                                             bool aIsHorizontal,
                                             PRInt32 &aOverriddenDelta)
{
  aOverriddenDelta = aOriginalDelta;

  const char* kPrefNameOverrideEnabled =
    "mousewheel.system_scroll_override_on_root_content.enabled";
  bool isOverrideEnabled =
    Preferences::GetBool(kPrefNameOverrideEnabled, false);
  if (!isOverrideEnabled) {
    return NS_OK;
  }

  nsCAutoString factorPrefName(
    "mousewheel.system_scroll_override_on_root_content.");
  if (aIsHorizontal) {
    factorPrefName.AppendLiteral("horizontal.");
  } else {
    factorPrefName.AppendLiteral("vertical.");
  }
  factorPrefName.AppendLiteral("factor");
  PRInt32 iFactor = Preferences::GetInt(factorPrefName.get(), 0);
  // The pref value must be larger than 100, otherwise, we don't override the
  // delta value.
  if (iFactor <= 100) {
    return NS_OK;
  }
  double factor = (double)iFactor / 100;
  aOverriddenDelta = PRInt32(NS_round((double)aOriginalDelta * factor));

  return NS_OK;
}


/**
 * Modifies aFile to point at an icon file with the given name and suffix.  The
 * suffix may correspond to a file extension with leading '.' if appropriate.
 * Returns true if the icon file exists and can be read.
 */
static bool
ResolveIconNameHelper(nsILocalFile *aFile,
                      const nsAString &aIconName,
                      const nsAString &aIconSuffix)
{
  aFile->Append(NS_LITERAL_STRING("icons"));
  aFile->Append(NS_LITERAL_STRING("default"));
  aFile->Append(aIconName + aIconSuffix);

  bool readable;
  return NS_SUCCEEDED(aFile->IsReadable(&readable)) && readable;
}

/**
 * Resolve the given icon name into a local file object.  This method is
 * intended to be called by subclasses of nsBaseWidget.  aIconSuffix is a
 * platform specific icon file suffix (e.g., ".ico" under Win32).
 *
 * If no file is found matching the given parameters, then null is returned.
 */
void
nsBaseWidget::ResolveIconName(const nsAString &aIconName,
                              const nsAString &aIconSuffix,
                              nsILocalFile **aResult)
{ 
  *aResult = nsnull;

  nsCOMPtr<nsIProperties> dirSvc = do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
  if (!dirSvc)
    return;

  // first check auxilary chrome directories

  nsCOMPtr<nsISimpleEnumerator> dirs;
  dirSvc->Get(NS_APP_CHROME_DIR_LIST, NS_GET_IID(nsISimpleEnumerator),
              getter_AddRefs(dirs));
  if (dirs) {
    bool hasMore;
    while (NS_SUCCEEDED(dirs->HasMoreElements(&hasMore)) && hasMore) {
      nsCOMPtr<nsISupports> element;
      dirs->GetNext(getter_AddRefs(element));
      if (!element)
        continue;
      nsCOMPtr<nsILocalFile> file = do_QueryInterface(element);
      if (!file)
        continue;
      if (ResolveIconNameHelper(file, aIconName, aIconSuffix)) {
        NS_ADDREF(*aResult = file);
        return;
      }
    }
  }

  // then check the main app chrome directory

  nsCOMPtr<nsILocalFile> file;
  dirSvc->Get(NS_APP_CHROME_DIR, NS_GET_IID(nsILocalFile),
              getter_AddRefs(file));
  if (file && ResolveIconNameHelper(file, aIconName, aIconSuffix))
    NS_ADDREF(*aResult = file);
}

NS_IMETHODIMP 
nsBaseWidget::BeginResizeDrag(nsGUIEvent* aEvent, PRInt32 aHorizontal, PRInt32 aVertical)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBaseWidget::BeginMoveDrag(nsMouseEvent* aEvent)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// For backwards compatibility only
NS_IMETHODIMP
nsBaseWidget::SetIMEEnabled(PRUint32 aState)
{
  IMEContext context;
  context.mStatus = aState;
  return SetInputMode(context);
}
 
NS_IMETHODIMP
nsBaseWidget::GetIMEEnabled(PRUint32* aState)
{
  IMEContext context;
  nsresult rv = GetInputMode(context);
  NS_ENSURE_SUCCESS(rv, rv);

  *aState = context.mStatus;
  return NS_OK;
}
 
#ifdef DEBUG
//////////////////////////////////////////////////////////////
//
// Convert a GUI event message code to a string.
// Makes it a lot easier to debug events.
//
// See gtk/nsWidget.cpp and windows/nsWindow.cpp
// for a DebugPrintEvent() function that uses
// this.
//
//////////////////////////////////////////////////////////////
/* static */ nsAutoString
nsBaseWidget::debug_GuiEventToString(nsGUIEvent * aGuiEvent)
{
  NS_ASSERTION(nsnull != aGuiEvent,"cmon, null gui event.");

  nsAutoString eventName(NS_LITERAL_STRING("UNKNOWN"));

#define _ASSIGN_eventName(_value,_name)\
case _value: eventName.AssignWithConversion(_name) ; break

  switch(aGuiEvent->message)
  {
    _ASSIGN_eventName(NS_BLUR_CONTENT,"NS_BLUR_CONTENT");
    _ASSIGN_eventName(NS_CREATE,"NS_CREATE");
    _ASSIGN_eventName(NS_DESTROY,"NS_DESTROY");
    _ASSIGN_eventName(NS_DRAGDROP_GESTURE,"NS_DND_GESTURE");
    _ASSIGN_eventName(NS_DRAGDROP_DROP,"NS_DND_DROP");
    _ASSIGN_eventName(NS_DRAGDROP_ENTER,"NS_DND_ENTER");
    _ASSIGN_eventName(NS_DRAGDROP_EXIT,"NS_DND_EXIT");
    _ASSIGN_eventName(NS_DRAGDROP_OVER,"NS_DND_OVER");
    _ASSIGN_eventName(NS_FOCUS_CONTENT,"NS_FOCUS_CONTENT");
    _ASSIGN_eventName(NS_FORM_SELECTED,"NS_FORM_SELECTED");
    _ASSIGN_eventName(NS_FORM_CHANGE,"NS_FORM_CHANGE");
    _ASSIGN_eventName(NS_FORM_INPUT,"NS_FORM_INPUT");
    _ASSIGN_eventName(NS_FORM_RESET,"NS_FORM_RESET");
    _ASSIGN_eventName(NS_FORM_SUBMIT,"NS_FORM_SUBMIT");
    _ASSIGN_eventName(NS_IMAGE_ABORT,"NS_IMAGE_ABORT");
    _ASSIGN_eventName(NS_LOAD_ERROR,"NS_LOAD_ERROR");
    _ASSIGN_eventName(NS_KEY_DOWN,"NS_KEY_DOWN");
    _ASSIGN_eventName(NS_KEY_PRESS,"NS_KEY_PRESS");
    _ASSIGN_eventName(NS_KEY_UP,"NS_KEY_UP");
    _ASSIGN_eventName(NS_MOUSE_ENTER,"NS_MOUSE_ENTER");
    _ASSIGN_eventName(NS_MOUSE_EXIT,"NS_MOUSE_EXIT");
    _ASSIGN_eventName(NS_MOUSE_BUTTON_DOWN,"NS_MOUSE_BUTTON_DOWN");
    _ASSIGN_eventName(NS_MOUSE_BUTTON_UP,"NS_MOUSE_BUTTON_UP");
    _ASSIGN_eventName(NS_MOUSE_CLICK,"NS_MOUSE_CLICK");
    _ASSIGN_eventName(NS_MOUSE_DOUBLECLICK,"NS_MOUSE_DBLCLICK");
    _ASSIGN_eventName(NS_MOUSE_MOVE,"NS_MOUSE_MOVE");
    _ASSIGN_eventName(NS_MOVE,"NS_MOVE");
    _ASSIGN_eventName(NS_LOAD,"NS_LOAD");
    _ASSIGN_eventName(NS_POPSTATE,"NS_POPSTATE");
    _ASSIGN_eventName(NS_BEFORE_SCRIPT_EXECUTE,"NS_BEFORE_SCRIPT_EXECUTE");
    _ASSIGN_eventName(NS_AFTER_SCRIPT_EXECUTE,"NS_AFTER_SCRIPT_EXECUTE");
    _ASSIGN_eventName(NS_PAGE_UNLOAD,"NS_PAGE_UNLOAD");
    _ASSIGN_eventName(NS_HASHCHANGE,"NS_HASHCHANGE");
    _ASSIGN_eventName(NS_READYSTATECHANGE,"NS_READYSTATECHANGE");
    _ASSIGN_eventName(NS_PAINT,"NS_PAINT");
    _ASSIGN_eventName(NS_XUL_BROADCAST, "NS_XUL_BROADCAST");
    _ASSIGN_eventName(NS_XUL_COMMAND_UPDATE, "NS_XUL_COMMAND_UPDATE");
    _ASSIGN_eventName(NS_SCROLLBAR_LINE_NEXT,"NS_SB_LINE_NEXT");
    _ASSIGN_eventName(NS_SCROLLBAR_LINE_PREV,"NS_SB_LINE_PREV");
    _ASSIGN_eventName(NS_SCROLLBAR_PAGE_NEXT,"NS_SB_PAGE_NEXT");
    _ASSIGN_eventName(NS_SCROLLBAR_PAGE_PREV,"NS_SB_PAGE_PREV");
    _ASSIGN_eventName(NS_SCROLLBAR_POS,"NS_SB_POS");
    _ASSIGN_eventName(NS_SIZE,"NS_SIZE");

#undef _ASSIGN_eventName

  default: 
    {
      char buf[32];
      
      sprintf(buf,"UNKNOWN: %d",aGuiEvent->message);
      
      eventName.AssignWithConversion(buf);
    }
    break;
  }
  
  return nsAutoString(eventName);
}
//////////////////////////////////////////////////////////////
//
// Code to deal with paint and event debug prefs.
//
//////////////////////////////////////////////////////////////
struct PrefPair
{
  const char * name;
  bool value;
};

static PrefPair debug_PrefValues[] =
{
  { "nglayout.debug.crossing_event_dumping", PR_FALSE },
  { "nglayout.debug.event_dumping", PR_FALSE },
  { "nglayout.debug.invalidate_dumping", PR_FALSE },
  { "nglayout.debug.motion_event_dumping", PR_FALSE },
  { "nglayout.debug.paint_dumping", PR_FALSE },
  { "nglayout.debug.paint_flashing", PR_FALSE }
};

//////////////////////////////////////////////////////////////
bool
nsBaseWidget::debug_GetCachedBoolPref(const char * aPrefName)
{
  NS_ASSERTION(nsnull != aPrefName,"cmon, pref name is null.");

  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(debug_PrefValues); i++)
  {
    if (strcmp(debug_PrefValues[i].name, aPrefName) == 0)
    {
      return debug_PrefValues[i].value;
    }
  }

  return PR_FALSE;
}
//////////////////////////////////////////////////////////////
static void debug_SetCachedBoolPref(const char * aPrefName,bool aValue)
{
  NS_ASSERTION(nsnull != aPrefName,"cmon, pref name is null.");

  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(debug_PrefValues); i++)
  {
    if (strcmp(debug_PrefValues[i].name, aPrefName) == 0)
    {
      debug_PrefValues[i].value = aValue;

      return;
    }
  }

  NS_ASSERTION(PR_FALSE, "cmon, this code is not reached dude.");
}

//////////////////////////////////////////////////////////////
class Debug_PrefObserver : public nsIObserver {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS1(Debug_PrefObserver, nsIObserver)

NS_IMETHODIMP
Debug_PrefObserver::Observe(nsISupports* subject, const char* topic,
                            const PRUnichar* data)
{
  NS_ConvertUTF16toUTF8 prefName(data);

  bool value = Preferences::GetBool(prefName.get(), false);
  debug_SetCachedBoolPref(prefName.get(), value);
  return NS_OK;
}

//////////////////////////////////////////////////////////////
/* static */ void
debug_RegisterPrefCallbacks()
{
  static bool once = true;

  if (!once) {
    return;
  }

  once = PR_FALSE;

  nsCOMPtr<nsIObserver> obs(new Debug_PrefObserver());
  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(debug_PrefValues); i++) {
    // Initialize the pref values
    debug_PrefValues[i].value =
      Preferences::GetBool(debug_PrefValues[i].name, false);

    if (obs) {
      // Register callbacks for when these change
      Preferences::AddStrongObserver(obs, debug_PrefValues[i].name);
    }
  }
}
//////////////////////////////////////////////////////////////
static PRInt32
_GetPrintCount()
{
  static PRInt32 sCount = 0;
  
  return ++sCount;
}
//////////////////////////////////////////////////////////////
/* static */ bool
nsBaseWidget::debug_WantPaintFlashing()
{
  return debug_GetCachedBoolPref("nglayout.debug.paint_flashing");
}
//////////////////////////////////////////////////////////////
/* static */ void
nsBaseWidget::debug_DumpEvent(FILE *                aFileOut,
                              nsIWidget *           aWidget,
                              nsGUIEvent *          aGuiEvent,
                              const nsCAutoString & aWidgetName,
                              PRInt32               aWindowID)
{
  // NS_PAINT is handled by debug_DumpPaintEvent()
  if (aGuiEvent->message == NS_PAINT)
    return;

  if (aGuiEvent->message == NS_MOUSE_MOVE)
  {
    if (!debug_GetCachedBoolPref("nglayout.debug.motion_event_dumping"))
      return;
  }
  
  if (aGuiEvent->message == NS_MOUSE_ENTER || 
      aGuiEvent->message == NS_MOUSE_EXIT)
  {
    if (!debug_GetCachedBoolPref("nglayout.debug.crossing_event_dumping"))
      return;
  }

  if (!debug_GetCachedBoolPref("nglayout.debug.event_dumping"))
    return;

  nsCAutoString tempString; tempString.AssignWithConversion(debug_GuiEventToString(aGuiEvent).get());
  
  fprintf(aFileOut,
          "%4d %-26s widget=%-8p name=%-12s id=%-8p refpt=%d,%d\n",
          _GetPrintCount(),
          tempString.get(),
          (void *) aWidget,
          aWidgetName.get(),
          (void *) (aWindowID ? aWindowID : 0x0),
          aGuiEvent->refPoint.x,
          aGuiEvent->refPoint.y);
}
//////////////////////////////////////////////////////////////
/* static */ void
nsBaseWidget::debug_DumpPaintEvent(FILE *                aFileOut,
                                   nsIWidget *           aWidget,
                                   nsPaintEvent *        aPaintEvent,
                                   const nsCAutoString & aWidgetName,
                                   PRInt32               aWindowID)
{
  NS_ASSERTION(nsnull != aFileOut,"cmon, null output FILE");
  NS_ASSERTION(nsnull != aWidget,"cmon, the widget is null");
  NS_ASSERTION(nsnull != aPaintEvent,"cmon, the paint event is null");

  if (!debug_GetCachedBoolPref("nglayout.debug.paint_dumping"))
    return;
  
  nsIntRect rect = aPaintEvent->region.GetBounds();
  fprintf(aFileOut,
          "%4d PAINT      widget=%p name=%-12s id=%-8p bounds-rect=%3d,%-3d %3d,%-3d", 
          _GetPrintCount(),
          (void *) aWidget,
          aWidgetName.get(),
          (void *) aWindowID,
          rect.x, rect.y, rect.width, rect.height
    );
  
  fprintf(aFileOut,"\n");
}
//////////////////////////////////////////////////////////////
/* static */ void
nsBaseWidget::debug_DumpInvalidate(FILE *                aFileOut,
                                   nsIWidget *           aWidget,
                                   const nsIntRect *     aRect,
                                   bool                  aIsSynchronous,
                                   const nsCAutoString & aWidgetName,
                                   PRInt32               aWindowID)
{
  if (!debug_GetCachedBoolPref("nglayout.debug.invalidate_dumping"))
    return;

  NS_ASSERTION(nsnull != aFileOut,"cmon, null output FILE");
  NS_ASSERTION(nsnull != aWidget,"cmon, the widget is null");

  fprintf(aFileOut,
          "%4d Invalidate widget=%p name=%-12s id=%-8p",
          _GetPrintCount(),
          (void *) aWidget,
          aWidgetName.get(),
          (void *) aWindowID);

  if (aRect) 
  {
    fprintf(aFileOut,
            " rect=%3d,%-3d %3d,%-3d",
            aRect->x, 
            aRect->y,
            aRect->width, 
            aRect->height);
  }
  else
  {
    fprintf(aFileOut,
            " rect=%-15s",
            "none");
  }

  fprintf(aFileOut,
          " sync=%s",
          (const char *) (aIsSynchronous ? "yes" : "no "));
  
  fprintf(aFileOut,"\n");
}
//////////////////////////////////////////////////////////////

#endif // DEBUG

