/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "mozilla/layers/CompositorChild.h"
#include "mozilla/layers/CompositorParent.h"
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
#include "base/thread.h"
#include "prenv.h"
#include "mozilla/Attributes.h"

#ifdef DEBUG
#include "nsIObserver.h"

static void debug_RegisterPrefCallbacks();

static bool debug_InSecureKeyboardInputMode = false;
#endif

#ifdef NOISY_WIDGET_LEAKS
static PRInt32 gNumWidgets;
#endif

static void InitOnlyOnce();
static bool sUseOffMainThreadCompositing = false;

using namespace mozilla::layers;
using namespace mozilla;
using base::Thread;
using mozilla::ipc::AsyncChannel;

nsIContent* nsBaseWidget::mLastRollup = nsnull;
// Global user preference for disabling native theme. Used
// in NativeWindowTheme.
bool            gDisableNativeTheme               = false;

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
, mCompositorThread(nsnull)
, mCursor(eCursor_standard)
, mWindowType(eWindowType_child)
, mBorderStyle(eBorderStyle_none)
, mOnDestroyCalled(false)
, mUseAcceleratedRendering(false)
, mForceLayersAcceleration(false)
, mTemporarilyUseBasicLayerManager(false)
, mBounds(0,0,0,0)
, mOriginalBounds(nsnull)
, mClipRectCount(0)
, mZIndex(0)
, mSizeMode(nsSizeMode_Normal)
, mPopupLevel(ePopupLevelTop)
, mPopupType(ePopupTypeAny)
{
#ifdef NOISY_WIDGET_LEAKS
  gNumWidgets++;
  printf("WIDGETS+ = %d\n", gNumWidgets);
#endif

#ifdef DEBUG
  debug_RegisterPrefCallbacks();
#endif
  InitOnlyOnce();
}


static void DeferredDestroyCompositor(CompositorParent* aCompositorParent,
                              CompositorChild* aCompositorChild,
                              Thread* aCompositorThread)
{
    aCompositorChild->Destroy();
    delete aCompositorThread;
    aCompositorParent->Release();
    aCompositorChild->Release();
}

void nsBaseWidget::DestroyCompositor() 
{
  if (mCompositorChild) {
    mCompositorChild->SendWillStop();

    // The call just made to SendWillStop can result in IPC from the
    // CompositorParent to the CompositorChild (e.g. caused by the destruction
    // of shared memory). We need to ensure this gets processed by the
    // CompositorChild before it gets destroyed. It suffices to ensure that
    // events already in the MessageLoop get processed before the
    // CompositorChild is destroyed, so we add a task to the MessageLoop to
    // handle compositor desctruction.
    MessageLoop::current()->PostTask(FROM_HERE,
               NewRunnableFunction(DeferredDestroyCompositor, mCompositorParent,
                                   mCompositorChild, mCompositorThread));
    // The DestroyCompositor task we just added to the MessageLoop will handle
    // releasing mCompositorParent and mCompositorChild.
    mCompositorParent.forget();
    mCompositorChild.forget();
  }
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
    mLayerManager = nsnull;
  }

  DestroyCompositor();

#ifdef NOISY_WIDGET_LEAKS
  gNumWidgets--;
  printf("WIDGETS- = %d\n", gNumWidgets);
#endif

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
                              nsWidgetInitData *aInitData)
{
  static bool gDisableNativeThemeCached = false;
  if (!gDisableNativeThemeCached) {
    mozilla::Preferences::AddBoolVarCache(&gDisableNativeTheme,
                                          "mozilla.widget.disable-native-theme",
                                          gDisableNativeTheme);
    gDisableNativeThemeCached = true;
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
    mPopupType = aInitData->mPopupHint;
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
                                  aContext, aInitData))) {
    return widget.forget();
  }

  return nsnull;
}

NS_IMETHODIMP
nsBaseWidget::SetEventCallback(EVENT_CALLBACK aEventFunction,
                               nsDeviceContext *aContext)
{
  NS_ASSERTION(aEventFunction, "Must have valid event callback!");

  mEventCallback = aEventFunction;

  if (aContext) {
    NS_IF_RELEASE(mContext);
    mContext = aContext;
    NS_ADDREF(mContext);
  }

  return NS_OK;
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
          PlaceBehind(eZPlacementBelow, sib, false);
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
    return false;

  mClipRectCount = aRects.Length();
  mClipRects = new nsIntRect[mClipRectCount];
  if (mClipRects) {
    memcpy(mClipRects, aRects.Elements(), sizeof(nsIntRect)*mClipRectCount);
  }
  return true;
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
          Resize(left, top, width, height, true);
        }
      }
    }

  } else if (mOriginalBounds) {
    Resize(mOriginalBounds->x, mOriginalBounds->y, mOriginalBounds->width,
           mOriginalBounds->height, true);
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
  mWidget->mTemporarilyUseBasicLayerManager = true;
}

nsBaseWidget::AutoUseBasicLayerManager::~AutoUseBasicLayerManager()
{
  mWidget->mTemporarilyUseBasicLayerManager = false;
}

bool
nsBaseWidget::GetShouldAccelerate()
{
#if defined(XP_WIN) || defined(ANDROID) || (MOZ_PLATFORM_MAEMO > 5) || defined(MOZ_GL_PROVIDER)
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
        accelerateByDefault = false;
      }
    }
  }

# else
  bool accelerateByDefault = false;
# endif

#else
  bool accelerateByDefault = false;
#endif

  // We don't want to accelerate small popup windows like menu, but we still 
  // want to accelerate xul panels that may contain arbitrarily complex content.
  bool isSmallPopup = ((mWindowType == eWindowType_popup) && 
                      (mPopupType != ePopupTypePanel));
  // we should use AddBoolPrefVarCache
  bool disableAcceleration = isSmallPopup || 
    Preferences::GetBool("layers.acceleration.disabled", false);
  mForceLayersAcceleration =
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
    return false;

  if (mForceLayersAcceleration)
    return true;
  
  if (!whitelisted) {
    NS_WARNING("OpenGL-accelerated layers are not supported on this system.");
    return false;
  }

  if (accelerateByDefault)
    return true;

  /* use the window acceleration flag */
  return mUseAcceleratedRendering;
}

void nsBaseWidget::CreateCompositor()
{
  mCompositorThread = new Thread("CompositorThread");
  if (mCompositorThread->Start()) {
    bool renderToEGLSurface = false;
#ifdef MOZ_JAVA_COMPOSITOR
    renderToEGLSurface = true;
#endif
    nsIntRect rect;
    GetBounds(rect);
    mCompositorParent =
      new CompositorParent(this, mCompositorThread->message_loop(), mCompositorThread->thread_id(),
                           renderToEGLSurface, rect.width, rect.height);
    LayerManager* lm = CreateBasicLayerManager();
    MessageLoop *childMessageLoop = mCompositorThread->message_loop();
    mCompositorChild = new CompositorChild(lm);
    AsyncChannel *parentChannel = mCompositorParent->GetIPCChannel();
    AsyncChannel::Side childSide = mozilla::ipc::AsyncChannel::Child;
    mCompositorChild->Open(parentChannel, childMessageLoop, childSide);
    PRInt32 maxTextureSize;
    PLayersChild* shadowManager;
    if (mUseAcceleratedRendering) {
      shadowManager = mCompositorChild->SendPLayersConstructor(LayerManager::LAYERS_OPENGL, &maxTextureSize);
    } else {
      shadowManager = mCompositorChild->SendPLayersConstructor(LayerManager::LAYERS_BASIC, &maxTextureSize);
    }

    if (shadowManager) {
      ShadowLayerForwarder* lf = lm->AsShadowForwarder();
      if (!lf) {
        delete lm;
        mCompositorChild = nsnull;
        return;
      }
      lf->SetShadowManager(shadowManager);
      if (mUseAcceleratedRendering)
        lf->SetParentBackendType(LayerManager::LAYERS_OPENGL);
      else
        lf->SetParentBackendType(LayerManager::LAYERS_BASIC);
      lf->SetMaxTextureSize(maxTextureSize);

      mLayerManager = lm;
    } else {
      // We don't currently want to support not having a LayersChild
      NS_RUNTIMEABORT("failed to construct LayersChild");
      delete lm;
      mCompositorChild = nsnull;
    }
  }
}

bool nsBaseWidget::UseOffMainThreadCompositing()
{
  return sUseOffMainThreadCompositing;
}

LayerManager* nsBaseWidget::GetLayerManager(PLayersChild* aShadowManager,
                                            LayersBackend aBackendHint,
                                            LayerManagerPersistence aPersistence,
                                            bool* aAllowRetaining)
{
  if (!mLayerManager) {

    mUseAcceleratedRendering = GetShouldAccelerate();

    // Try to use an async compositor first, if possible
    if (UseOffMainThreadCompositing()) {
      // e10s uses the parameter to pass in the shadow manager from the TabChild
      // so we don't expect to see it there since this doesn't support e10s.
      NS_ASSERTION(aShadowManager == nsnull, "Async Compositor not supported with e10s");
      CreateCompositor();
    }

    if (mUseAcceleratedRendering) {
      if (!mLayerManager) {
        nsRefPtr<LayerManagerOGL> layerManager = new LayerManagerOGL(this);
        /**
         * XXX - On several OSes initialization is expected to fail for now.
         * If we'd get a non-basic layer manager they'd crash. This is ok though
         * since on those platforms it will fail. Anyone implementing new
         * platforms on LayerManagerOGL should ensure their widget is able to
         * deal with it though!
         */

        if (layerManager->Initialize(mForceLayersAcceleration)) {
          mLayerManager = layerManager;
        }
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
  // release references to device context and app shell
  NS_IF_RELEASE(mContext);
}

NS_METHOD nsBaseWidget::SetWindowClass(const nsAString& xulWinType)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsBaseWidget::MoveClient(PRInt32 aX, PRInt32 aY)
{
  nsIntPoint clientOffset(GetClientOffset());
  aX -= clientOffset.x;
  aY -= clientOffset.y;
  return Move(aX, aY);
}

NS_METHOD nsBaseWidget::ResizeClient(PRInt32 aWidth,
                                     PRInt32 aHeight,
                                     bool aRepaint)
{
  NS_ASSERTION((aWidth >=0) , "Negative width passed to ResizeClient");
  NS_ASSERTION((aHeight >=0), "Negative height passed to ResizeClient");

  nsIntRect clientBounds;
  GetClientBounds(clientBounds);
  aWidth = mBounds.width + (aWidth - clientBounds.width);
  aHeight = mBounds.height + (aHeight - clientBounds.height);

  return Resize(aWidth, aHeight, aRepaint);
}

NS_METHOD nsBaseWidget::ResizeClient(PRInt32 aX,
                                     PRInt32 aY,
                                     PRInt32 aWidth,
                                     PRInt32 aHeight,
                                     bool aRepaint)
{
  NS_ASSERTION((aWidth >=0) , "Negative width passed to ResizeClient");
  NS_ASSERTION((aHeight >=0), "Negative height passed to ResizeClient");

  nsIntRect clientBounds;
  GetClientBounds(clientBounds);
  aWidth = mBounds.width + (aWidth - clientBounds.width);
  aHeight = mBounds.height + (aHeight - clientBounds.height);

  nsIntPoint clientOffset(GetClientOffset());
  aX -= clientOffset.x;
  aY -= clientOffset.y;

  return Resize(aX, aY, aWidth, aHeight, aRepaint);
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
  return false;
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
  debug_InSecureKeyboardInputMode = true;
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsBaseWidget::EndSecureKeyboardInput()
{
#ifdef DEBUG
  NS_ASSERTION(debug_InSecureKeyboardInputMode, "Calling EndSecureKeyboardInput when it hasn't been enabled!");
  debug_InSecureKeyboardInputMode = false;
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
  return false;
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
ResolveIconNameHelper(nsIFile *aFile,
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
                              nsIFile **aResult)
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
      nsCOMPtr<nsIFile> file = do_QueryInterface(element);
      if (!file)
        continue;
      if (ResolveIconNameHelper(file, aIconName, aIconSuffix)) {
        NS_ADDREF(*aResult = file);
        return;
      }
    }
  }

  // then check the main app chrome directory

  nsCOMPtr<nsIFile> file;
  dirSvc->Get(NS_APP_CHROME_DIR, NS_GET_IID(nsIFile),
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

PRUint32
nsBaseWidget::GetGLFrameBufferFormat()
{
  if (mLayerManager &&
      mLayerManager->GetBackendType() == LayerManager::LAYERS_OPENGL) {
    // Assume that the default framebuffer has RGBA format.  Specific
    // backends that know differently will override this method.
    return LOCAL_GL_RGBA;
  }
  return LOCAL_GL_NONE;
}

static void InitOnlyOnce()
{
  static bool once = true;
  if (!once) {
    return;
  }
  once = false;

#ifdef MOZ_X11
  // On X11 platforms only use OMTC if firefox was initalized with thread-safe 
  // X11 (else it would crash).
  sUseOffMainThreadCompositing = (PR_GetEnv("MOZ_USE_OMTC") != NULL);
#else
  sUseOffMainThreadCompositing = Preferences::GetBool(
        "layers.offmainthreadcomposition.enabled", 
        false);
  // Until https://bugzilla.mozilla.org/show_bug.cgi?id=745148 lands,
  // we use either omtc or content processes, but not both.  Prefer
  // OOP content to omtc.  (Currently, this only affects b2g.)
  //
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=761962 .
  if (!Preferences::GetBool("dom.ipc.tabs.disabled", true)) {
    // Disable omtc if OOP content isn't force-disabled.
    sUseOffMainThreadCompositing = false;
  }
#endif
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
case _value: eventName.AssignLiteral(_name) ; break

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
      
      CopyASCIItoUTF16(buf, eventName);
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
  { "nglayout.debug.crossing_event_dumping", false },
  { "nglayout.debug.event_dumping", false },
  { "nglayout.debug.invalidate_dumping", false },
  { "nglayout.debug.motion_event_dumping", false },
  { "nglayout.debug.paint_dumping", false },
  { "nglayout.debug.paint_flashing", false }
};

//////////////////////////////////////////////////////////////
bool
nsBaseWidget::debug_GetCachedBoolPref(const char * aPrefName)
{
  NS_ASSERTION(nsnull != aPrefName,"cmon, pref name is null.");

  for (PRUint32 i = 0; i < ArrayLength(debug_PrefValues); i++)
  {
    if (strcmp(debug_PrefValues[i].name, aPrefName) == 0)
    {
      return debug_PrefValues[i].value;
    }
  }

  return false;
}
//////////////////////////////////////////////////////////////
static void debug_SetCachedBoolPref(const char * aPrefName,bool aValue)
{
  NS_ASSERTION(nsnull != aPrefName,"cmon, pref name is null.");

  for (PRUint32 i = 0; i < ArrayLength(debug_PrefValues); i++)
  {
    if (strcmp(debug_PrefValues[i].name, aPrefName) == 0)
    {
      debug_PrefValues[i].value = aValue;

      return;
    }
  }

  NS_ASSERTION(false, "cmon, this code is not reached dude.");
}

//////////////////////////////////////////////////////////////
class Debug_PrefObserver MOZ_FINAL : public nsIObserver {
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

  once = false;

  nsCOMPtr<nsIObserver> obs(new Debug_PrefObserver());
  for (PRUint32 i = 0; i < ArrayLength(debug_PrefValues); i++) {
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

  NS_LossyConvertUTF16toASCII tempString(debug_GuiEventToString(aGuiEvent).get());
  
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
  
  fprintf(aFileOut,"\n");
}
//////////////////////////////////////////////////////////////

#endif // DEBUG

