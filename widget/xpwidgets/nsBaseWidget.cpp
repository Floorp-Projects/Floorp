/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "mozilla/layers/CompositorChild.h"
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/ImageBridgeChild.h"
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
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIServiceManager.h"
#include "mozilla/Preferences.h"
#include "BasicLayers.h"
#include "ClientLayerManager.h"
#include "mozilla/layers/Compositor.h"
#include "nsIXULRuntime.h"
#include "nsIXULWindow.h"
#include "nsIBaseWindow.h"
#include "nsXULPopupManager.h"
#include "nsIWidgetListener.h"
#include "nsIGfxInfo.h"
#include "npapi.h"
#include "base/thread.h"
#include "prdtoa.h"
#include "prenv.h"
#include "mozilla/Attributes.h"
#include "mozilla/unused.h"
#include "nsContentUtils.h"
#include "gfxPrefs.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/MouseEvents.h"
#include "GLConsts.h"
#include "LayerScope.h"
#include "mozilla/unused.h"

#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"
#endif

#ifdef DEBUG
#include "nsIObserver.h"

static void debug_RegisterPrefCallbacks();

#endif

#ifdef NOISY_WIDGET_LEAKS
static int32_t gNumWidgets;
#endif

#ifdef XP_MACOSX
#include "nsCocoaFeatures.h"
#endif

nsIRollupListener* nsBaseWidget::gRollupListener = nullptr;

using namespace mozilla::layers;
using namespace mozilla::ipc;
using namespace mozilla;
using base::Thread;

nsIContent* nsBaseWidget::mLastRollup = nullptr;
// Global user preference for disabling native theme. Used
// in NativeWindowTheme.
bool            gDisableNativeTheme               = false;

// Async pump timer during injected long touch taps
#define TOUCH_INJECT_PUMP_TIMER_MSEC 50
#define TOUCH_INJECT_LONG_TAP_DEFAULT_MSEC 1500
int32_t nsIWidget::sPointerIdCounter = 0;

// nsBaseWidget
NS_IMPL_ISUPPORTS(nsBaseWidget, nsIWidget)


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
: mWidgetListener(nullptr)
, mAttachedWidgetListener(nullptr)
, mContext(nullptr)
, mCursor(eCursor_standard)
, mUpdateCursor(true)
, mBorderStyle(eBorderStyle_none)
, mUseLayersAcceleration(false)
, mForceLayersAcceleration(false)
, mTemporarilyUseBasicLayerManager(false)
, mUseAttachedEvents(false)
, mContextInitialized(false)
, mBounds(0,0,0,0)
, mOriginalBounds(nullptr)
, mClipRectCount(0)
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

  mShutdownObserver = new WidgetShutdownObserver(this);
  nsContentUtils::RegisterShutdownObserver(mShutdownObserver);
}

NS_IMPL_ISUPPORTS(WidgetShutdownObserver, nsIObserver)

NS_IMETHODIMP
WidgetShutdownObserver::Observe(nsISupports *aSubject,
                                const char *aTopic,
                                const char16_t *aData)
{
  if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0 &&
      mWidget) {
    mWidget->Shutdown();
    nsContentUtils::UnregisterShutdownObserver(this);
  }
 return NS_OK;
}

void
nsBaseWidget::Shutdown()
{
  DestroyCompositor();
  mShutdownObserver = nullptr;
}

static void DeferredDestroyCompositor(CompositorParent* aCompositorParent,
                              CompositorChild* aCompositorChild)
{
    // Bug 848949 needs to be fixed before
    // we can close the channel properly
    //aCompositorChild->Close();
    aCompositorParent->Release();
    aCompositorChild->Release();
}

void nsBaseWidget::DestroyCompositor()
{
  LayerScope::DeInit();

  if (mCompositorChild) {
    mCompositorChild->SendWillStop();
    mCompositorChild->Destroy();

    // The call just made to SendWillStop can result in IPC from the
    // CompositorParent to the CompositorChild (e.g. caused by the destruction
    // of shared memory). We need to ensure this gets processed by the
    // CompositorChild before it gets destroyed. It suffices to ensure that
    // events already in the MessageLoop get processed before the
    // CompositorChild is destroyed, so we add a task to the MessageLoop to
    // handle compositor desctruction.
    MessageLoop::current()->PostTask(FROM_HERE,
               NewRunnableFunction(DeferredDestroyCompositor, mCompositorParent,
                                   mCompositorChild));
    // The DestroyCompositor task we just added to the MessageLoop will handle
    // releasing mCompositorParent and mCompositorChild.
    unused << mCompositorParent.forget();
    unused << mCompositorChild.forget();
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
      mLayerManager->GetBackendType() == LayersBackend::LAYERS_BASIC) {
    static_cast<BasicLayerManager*>(mLayerManager.get())->ClearRetainerWidget();
  }

  if (mLayerManager) {
    mLayerManager->Destroy();
    mLayerManager = nullptr;
  }

  if (mShutdownObserver) {
    // If the shutdown observer is currently processing observers,
    // then UnregisterShutdownObserver won't stop our Observer
    // function from being called. Make sure we don't try
    // to reference the dead widget.
    mShutdownObserver->mWidget = nullptr;
    nsContentUtils::UnregisterShutdownObserver(mShutdownObserver);
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
                              nsDeviceContext *aContext,
                              nsWidgetInitData *aInitData)
{
  static bool gDisableNativeThemeCached = false;
  if (!gDisableNativeThemeCached) {
    Preferences::AddBoolVarCache(&gDisableNativeTheme,
                                 "mozilla.widget.disable-native-theme",
                                 gDisableNativeTheme);
    gDisableNativeThemeCached = true;
  }

  // keep a reference to the device context
  if (aContext) {
    mContext = aContext;
    NS_ADDREF(mContext);
  }
  else {
    mContext = new nsDeviceContext();
    NS_ADDREF(mContext);
    mContext->Init(nullptr);
  }

  if (nullptr != aInitData) {
    mWindowType = aInitData->mWindowType;
    mBorderStyle = aInitData->mBorderStyle;
    mPopupLevel = aInitData->mPopupLevel;
    mPopupType = aInitData->mPopupHint;
    mRequireOffMainThreadCompositing = aInitData->mRequireOffMainThreadCompositing;
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

nsIWidgetListener* nsBaseWidget::GetWidgetListener()
{
  return mWidgetListener;
}

void nsBaseWidget::SetWidgetListener(nsIWidgetListener* aWidgetListener)
{
  mWidgetListener = aWidgetListener;
}

already_AddRefed<nsIWidget>
nsBaseWidget::CreateChild(const nsIntRect  &aRect,
                          nsDeviceContext *aContext,
                          nsWidgetInitData *aInitData,
                          bool             aForceUseIWidgetParent)
{
  nsIWidget* parent = this;
  nsNativeWidget nativeParent = nullptr;

  if (!aForceUseIWidgetParent) {
    // Use only either parent or nativeParent, not both, to match
    // existing code.  Eventually Create() should be divested of its
    // nativeWidget parameter.
    nativeParent = parent ? parent->GetNativeData(NS_NATIVE_WIDGET) : nullptr;
    parent = nativeParent ? nullptr : parent;
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
                                  aContext, aInitData))) {
    return widget.forget();
  }

  return nullptr;
}

// Attach a view to our widget which we'll send events to.
NS_IMETHODIMP
nsBaseWidget::AttachViewToTopLevel(bool aUseAttachedEvents,
                                   nsDeviceContext *aContext)
{
  NS_ASSERTION((mWindowType == eWindowType_toplevel ||
                mWindowType == eWindowType_dialog ||
                mWindowType == eWindowType_invisible ||
                mWindowType == eWindowType_child),
               "Can't attach to window of that type");

  mUseAttachedEvents = aUseAttachedEvents;

  if (aContext) {
    if (mContext) {
      NS_IF_RELEASE(mContext);
    }
    mContext = aContext;
    NS_ADDREF(mContext);
  }

  return NS_OK;
}

nsIWidgetListener* nsBaseWidget::GetAttachedWidgetListener()
 {
   return mAttachedWidgetListener;
 }

void nsBaseWidget::SetAttachedWidgetListener(nsIWidgetListener* aListener)
 {
   mAttachedWidgetListener = aListener;
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
  return nullptr;
}

//-------------------------------------------------------------------------
//
// Get this nsBaseWidget top level widget
//
//-------------------------------------------------------------------------
nsIWidget* nsBaseWidget::GetTopLevelWidget()
{
  nsIWidget *topLevelWidget = nullptr, *widget = this;
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
  return nullptr;
}

float nsBaseWidget::GetDPI()
{
  return 96.0f;
}

CSSToLayoutDeviceScale nsIWidget::GetDefaultScale()
{
  double devPixelsPerCSSPixel = DefaultScaleOverride();

  if (devPixelsPerCSSPixel <= 0.0) {
    devPixelsPerCSSPixel = GetDefaultScaleInternal();
  }

  return CSSToLayoutDeviceScale(devPixelsPerCSSPixel);
}

/* static */
double nsIWidget::DefaultScaleOverride()
{
  // The number of device pixels per CSS pixel. A value <= 0 means choose
  // automatically based on the DPI. A positive value is used as-is. This effectively
  // controls the size of a CSS "px".
  double devPixelsPerCSSPixel = -1.0;

  nsAdoptingCString prefString = Preferences::GetCString("layout.css.devPixelsPerPx");
  if (!prefString.IsEmpty()) {
    devPixelsPerCSSPixel = PR_strtod(prefString, nullptr);
  }

  return devPixelsPerCSSPixel;
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
#ifdef DEBUG
#ifdef XP_MACOSX
  // nsCocoaWindow doesn't implement GetParent, so in that case parent will be
  // null and we'll just have to do without this assertion.
  nsIWidget* parent = aChild->GetParent();
  NS_ASSERTION(!parent || parent == this, "Not one of our kids!");
#else
  NS_ASSERTION(aChild->GetParent() == this, "Not one of our kids!");
#endif
#endif

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

  aChild->SetNextSibling(nullptr);
  aChild->SetPrevSibling(nullptr);
}


//-------------------------------------------------------------------------
//
// Sets widget's position within its parent's child list.
//
//-------------------------------------------------------------------------
void nsBaseWidget::SetZIndex(int32_t aZIndex)
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
      int32_t childZIndex = GetZIndex();
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
    // were we added to the list?
    if (!sib) {
      parent->AddChild(this);
    }
  }
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
NS_IMETHODIMP nsBaseWidget::SetSizeMode(int32_t aMode)
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
                                      uint32_t aHotspotX, uint32_t aHotspotY)
{
  return NS_ERROR_NOT_IMPLEMENTED;
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

NS_IMETHODIMP nsBaseWidget::SetWindowShadowStyle(int32_t aMode)
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
    // convert dev pix to display pix for window manipulation
    CSSToLayoutDeviceScale scale = GetDefaultScale();
    mOriginalBounds->x = NSToIntRound(mOriginalBounds->x / scale.scale);
    mOriginalBounds->y = NSToIntRound(mOriginalBounds->y / scale.scale);
    mOriginalBounds->width = NSToIntRound(mOriginalBounds->width / scale.scale);
    mOriginalBounds->height = NSToIntRound(mOriginalBounds->height / scale.scale);

    // Move to top-left corner of screen and size to the screen dimensions
    nsCOMPtr<nsIScreenManager> screenManager;
    screenManager = do_GetService("@mozilla.org/gfx/screenmanager;1");
    NS_ASSERTION(screenManager, "Unable to grab screenManager.");
    if (screenManager) {
      nsCOMPtr<nsIScreen> screen;
      screenManager->ScreenForRect(mOriginalBounds->x,
                                   mOriginalBounds->y,
                                   mOriginalBounds->width,
                                   mOriginalBounds->height,
                                   getter_AddRefs(screen));
      if (screen) {
        int32_t left, top, width, height;
        if (NS_SUCCEEDED(screen->GetRectDisplayPix(&left, &top, &width, &height))) {
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
    BufferMode aDoubleBuffering, ScreenRotation aRotation)
  : mWidget(aWidget)
{
  mLayerManager = static_cast<BasicLayerManager*>(mWidget->GetLayerManager());
  if (mLayerManager) {
    NS_ASSERTION(mLayerManager->GetBackendType() == LayersBackend::LAYERS_BASIC,
      "AutoLayerManagerSetup instantiated for non-basic layer backend!");
    mLayerManager->SetDefaultTarget(aTarget);
    mLayerManager->SetDefaultTargetConfiguration(aDoubleBuffering, aRotation);
  }
}

nsBaseWidget::AutoLayerManagerSetup::~AutoLayerManagerSetup()
{
  if (mLayerManager) {
    NS_ASSERTION(mLayerManager->GetBackendType() == LayersBackend::LAYERS_BASIC,
      "AutoLayerManagerSetup instantiated for non-basic layer backend!");
    mLayerManager->SetDefaultTarget(nullptr);
    mLayerManager->SetDefaultTargetConfiguration(mozilla::layers::BufferMode::BUFFER_NONE, ROTATION_0);
  }
}

nsBaseWidget::AutoUseBasicLayerManager::AutoUseBasicLayerManager(nsBaseWidget* aWidget)
  : mWidget(aWidget)
{
  mPreviousTemporarilyUseBasicLayerManager =
    mWidget->mTemporarilyUseBasicLayerManager;
  mWidget->mTemporarilyUseBasicLayerManager = true;
}

nsBaseWidget::AutoUseBasicLayerManager::~AutoUseBasicLayerManager()
{
  mWidget->mTemporarilyUseBasicLayerManager =
    mPreviousTemporarilyUseBasicLayerManager;
}

bool
nsBaseWidget::ComputeShouldAccelerate(bool aDefault)
{
#if defined(XP_WIN) || defined(ANDROID) || \
    defined(MOZ_GL_PROVIDER) || defined(XP_MACOSX) || defined(MOZ_WIDGET_QT)
  bool accelerateByDefault = true;
#else
  bool accelerateByDefault = false;
#endif

#ifdef XP_MACOSX
  // 10.6.2 and lower have a bug involving textures and pixel buffer objects
  // that caused bug 629016, so we don't allow OpenGL-accelerated layers on
  // those versions of the OS.
  // This will still let full-screen video be accelerated on OpenGL, because
  // that XUL widget opts in to acceleration, but that's probably OK.
  SInt32 major = nsCocoaFeatures::OSXVersionMajor();
  SInt32 minor = nsCocoaFeatures::OSXVersionMinor();
  SInt32 bugfix = nsCocoaFeatures::OSXVersionBugFix();
  if (major == 10 && minor == 6 && bugfix <= 2) {
    accelerateByDefault = false;
  }
#endif

  // we should use AddBoolPrefVarCache
  bool disableAcceleration = gfxPrefs::LayersAccelerationDisabled();
  mForceLayersAcceleration = gfxPrefs::LayersAccelerationForceEnabled();

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

    int32_t status;
    if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_OPENGL_LAYERS, &status))) {
      if (status == nsIGfxInfo::FEATURE_STATUS_OK) {
        whitelisted = true;
      }
    }
  }

  if (disableAcceleration || safeMode)
    return false;

  if (mForceLayersAcceleration)
    return true;

  if (!whitelisted) {
    static int tell_me_once = 0;
    if (!tell_me_once) {
      NS_WARNING("OpenGL-accelerated layers are not supported on this system");
      tell_me_once = 1;
    }
#ifdef MOZ_WIDGET_ANDROID
    NS_RUNTIMEABORT("OpenGL-accelerated layers are a hard requirement on this platform. "
                    "Cannot continue without support for them");
#endif
    return false;
  }

  if (accelerateByDefault)
    return true;

  /* use the window acceleration flag */
  return aDefault;
}

CompositorParent* nsBaseWidget::NewCompositorParent(int aSurfaceWidth,
                                                    int aSurfaceHeight)
{
  return new CompositorParent(this, false, aSurfaceWidth, aSurfaceHeight);
}

void nsBaseWidget::CreateCompositor()
{
  nsIntRect rect;
  GetBounds(rect);
  CreateCompositor(rect.width, rect.height);
}

void
nsBaseWidget::GetPreferredCompositorBackends(nsTArray<LayersBackend>& aHints)
{
  if (mUseLayersAcceleration) {
    aHints.AppendElement(LayersBackend::LAYERS_OPENGL);
  }

  aHints.AppendElement(LayersBackend::LAYERS_BASIC);
}

static void
RemoveBasicBackend(nsTArray<LayersBackend>& aHints)
{
#ifndef XP_WIN
  for (size_t i = 0; i < aHints.Length(); ++i) {
    if (aHints[i] == LayersBackend::LAYERS_BASIC &&
        !Preferences::GetBool("layers.offmainthreadcomposition.force-basic", false)) {
      aHints[i] = LayersBackend::LAYERS_NONE;
    }
  }
#endif
}

void nsBaseWidget::CreateCompositor(int aWidth, int aHeight)
{
  MOZ_ASSERT(gfxPlatform::UsesOffMainThreadCompositing(),
             "This function assumes OMTC");

  // Recreating this is tricky, as we may still have an old and we need
  // to make sure it's properly destroyed by calling DestroyCompositor!

  // If we've already received a shutdown notification, don't try
  // create a new compositor.
  if (!mShutdownObserver) {
    return;
  }

  // Initialize LayerScope on the main thread.
  LayerScope::Init();

  mCompositorParent = NewCompositorParent(aWidth, aHeight);
  MessageChannel *parentChannel = mCompositorParent->GetIPCChannel();
  nsRefPtr<ClientLayerManager> lm = new ClientLayerManager(this);
  MessageLoop *childMessageLoop = CompositorParent::CompositorLoop();
  mCompositorChild = new CompositorChild(lm);
  mCompositorChild->Open(parentChannel, childMessageLoop, ipc::ChildSide);

  TextureFactoryIdentifier textureFactoryIdentifier;
  PLayerTransactionChild* shadowManager = nullptr;
  nsTArray<LayersBackend> backendHints;
  GetPreferredCompositorBackends(backendHints);

#ifndef MOZ_X11
  if (!mRequireOffMainThreadCompositing) {
    RemoveBasicBackend(backendHints);
  }
#endif

  bool success = false;
  if (!backendHints.IsEmpty()) {
    shadowManager = mCompositorChild->SendPLayerTransactionConstructor(
      backendHints, 0, &textureFactoryIdentifier, &success);
  }

  if (success) {
    ShadowLayerForwarder* lf = lm->AsShadowForwarder();
    if (!lf) {
      lm = nullptr;
      mCompositorChild = nullptr;
      return;
    }
    lf->SetShadowManager(shadowManager);
    lf->IdentifyTextureHost(textureFactoryIdentifier);
    ImageBridgeChild::IdentifyCompositorTextureHost(textureFactoryIdentifier);
    WindowUsesOMTC();

    mLayerManager = lm.forget();
    return;
  }

  NS_WARNING("Failed to create an OMT compositor.");
  DestroyCompositor();
  // Compositor child had the only reference to LayerManager and will have
  // deallocated it when being freed.
}

bool nsBaseWidget::ShouldUseOffMainThreadCompositing()
{
  return gfxPlatform::UsesOffMainThreadCompositing();
}

LayerManager* nsBaseWidget::GetLayerManager(PLayerTransactionChild* aShadowManager,
                                            LayersBackend aBackendHint,
                                            LayerManagerPersistence aPersistence,
                                            bool* aAllowRetaining)
{
  if (!mLayerManager) {

    mUseLayersAcceleration = ComputeShouldAccelerate(mUseLayersAcceleration);

    // Try to use an async compositor first, if possible
    if (ShouldUseOffMainThreadCompositing()) {
      // e10s uses the parameter to pass in the shadow manager from the TabChild
      // so we don't expect to see it there since this doesn't support e10s.
      NS_ASSERTION(aShadowManager == nullptr, "Async Compositor not supported with e10s");
      CreateCompositor();
    }

    if (!mLayerManager) {
      mLayerManager = CreateBasicLayerManager();
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

LayerManager* nsBaseWidget::CreateBasicLayerManager()
{
  return new BasicLayerManager(this);
}

CompositorChild* nsBaseWidget::GetRemoteRenderer()
{
  return mCompositorChild;
}

TemporaryRef<mozilla::gfx::DrawTarget> nsBaseWidget::StartRemoteDrawing()
{
  return nullptr;
}

//-------------------------------------------------------------------------
//
// Return the used device context
//
//-------------------------------------------------------------------------
nsDeviceContext* nsBaseWidget::GetDeviceContext()
{
  if (!mContextInitialized) {
    mContext->Init(this);
    mContextInitialized = true;
  }
  return mContext;
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

NS_METHOD nsBaseWidget::MoveClient(double aX, double aY)
{
  nsIntPoint clientOffset(GetClientOffset());

  // GetClientOffset returns device pixels; scale back to display pixels
  // if that's what this widget uses for the Move/Resize APIs
  CSSToLayoutDeviceScale scale = BoundsUseDisplayPixels()
                                    ? GetDefaultScale()
                                    : CSSToLayoutDeviceScale(1.0);
  aX -= clientOffset.x * 1.0 / scale.scale;
  aY -= clientOffset.y * 1.0 / scale.scale;

  return Move(aX, aY);
}

NS_METHOD nsBaseWidget::ResizeClient(double aWidth,
                                     double aHeight,
                                     bool aRepaint)
{
  NS_ASSERTION((aWidth >=0) , "Negative width passed to ResizeClient");
  NS_ASSERTION((aHeight >=0), "Negative height passed to ResizeClient");

  nsIntRect clientBounds;
  GetClientBounds(clientBounds);

  // GetClientBounds and mBounds are device pixels; scale back to display pixels
  // if that's what this widget uses for the Move/Resize APIs
  CSSToLayoutDeviceScale scale = BoundsUseDisplayPixels()
                                    ? GetDefaultScale()
                                    : CSSToLayoutDeviceScale(1.0);
  double invScale = 1.0 / scale.scale;
  aWidth = mBounds.width * invScale + (aWidth - clientBounds.width * invScale);
  aHeight = mBounds.height * invScale + (aHeight - clientBounds.height * invScale);

  return Resize(aWidth, aHeight, aRepaint);
}

NS_METHOD nsBaseWidget::ResizeClient(double aX,
                                     double aY,
                                     double aWidth,
                                     double aHeight,
                                     bool aRepaint)
{
  NS_ASSERTION((aWidth >=0) , "Negative width passed to ResizeClient");
  NS_ASSERTION((aHeight >=0), "Negative height passed to ResizeClient");

  nsIntRect clientBounds;
  GetClientBounds(clientBounds);

  double scale = BoundsUseDisplayPixels() ? 1.0 / GetDefaultScale().scale : 1.0;
  aWidth = mBounds.width * scale + (aWidth - clientBounds.width * scale);
  aHeight = mBounds.height * scale + (aHeight - clientBounds.height * scale);

  nsIntPoint clientOffset(GetClientOffset());
  aX -= clientOffset.x * scale;
  aY -= clientOffset.y * scale;

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

NS_METHOD nsBaseWidget::GetRestoredBounds(nsIntRect &aRect)
{
  if (SizeMode() != nsSizeMode_Normal) {
    return NS_ERROR_FAILURE;
  }
  return GetScreenBounds(aRect);
}

nsIntPoint nsBaseWidget::GetClientOffset()
{
  return nsIntPoint(0, 0);
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

uint32_t nsBaseWidget::GetMaxTouchPoints() const
{
  return 0;
}

NS_METHOD nsBaseWidget::SetModal(bool aModal)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsBaseWidget::GetAttention(int32_t aCycleCount) {
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
nsBaseWidget::SetLayersAcceleration(bool aEnabled)
{
  if (mUseLayersAcceleration == aEnabled) {
    return NS_OK;
  }

  bool usedAcceleration = mUseLayersAcceleration;

  mUseLayersAcceleration = ComputeShouldAccelerate(aEnabled);
  // ComputeShouldAccelerate may have set mUseLayersAcceleration to a value
  // different from aEnabled.
  if (usedAcceleration == mUseLayersAcceleration) {
    return NS_OK;
  }
  if (mLayerManager) {
    mLayerManager->Destroy();
  }
  mLayerManager = nullptr;
  return NS_OK;
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
nsBaseWidget::OverrideSystemMouseScrollSpeed(double aOriginalDeltaX,
                                             double aOriginalDeltaY,
                                             double& aOverriddenDeltaX,
                                             double& aOverriddenDeltaY)
{
  aOverriddenDeltaX = aOriginalDeltaX;
  aOverriddenDeltaY = aOriginalDeltaY;

  static bool sInitialized = false;
  static bool sIsOverrideEnabled = false;
  static int32_t sIntFactorX = 0;
  static int32_t sIntFactorY = 0;

  if (!sInitialized) {
    Preferences::AddBoolVarCache(&sIsOverrideEnabled,
      "mousewheel.system_scroll_override_on_root_content.enabled", false);
    Preferences::AddIntVarCache(&sIntFactorX,
      "mousewheel.system_scroll_override_on_root_content.horizontal.factor", 0);
    Preferences::AddIntVarCache(&sIntFactorY,
      "mousewheel.system_scroll_override_on_root_content.vertical.factor", 0);
    sIntFactorX = std::max(sIntFactorX, 0);
    sIntFactorY = std::max(sIntFactorY, 0);
    sInitialized = true;
  }

  if (!sIsOverrideEnabled) {
    return NS_OK;
  }

  // The pref value must be larger than 100, otherwise, we don't override the
  // delta value.
  if (sIntFactorX > 100) {
    double factor = static_cast<double>(sIntFactorX) / 100;
    aOverriddenDeltaX *= factor;
  }
  if (sIntFactorY > 100) {
    double factor = static_cast<double>(sIntFactorY) / 100;
    aOverriddenDeltaY *= factor;
  }

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
  *aResult = nullptr;

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
nsBaseWidget::BeginResizeDrag(WidgetGUIEvent* aEvent,
                              int32_t aHorizontal,
                              int32_t aVertical)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBaseWidget::BeginMoveDrag(WidgetMouseEvent* aEvent)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

uint32_t
nsBaseWidget::GetGLFrameBufferFormat()
{
  return LOCAL_GL_RGBA;
}

void nsBaseWidget::SetSizeConstraints(const SizeConstraints& aConstraints)
{
  mSizeConstraints = aConstraints;
  // We can't ensure that the size is honored at this point because we're
  // probably in the middle of a reflow.
}

const widget::SizeConstraints& nsBaseWidget::GetSizeConstraints() const
{
  return mSizeConstraints;
}

// static
nsIRollupListener*
nsBaseWidget::GetActiveRollupListener()
{
  // If set, then this is likely an <html:select> dropdown.
  if (gRollupListener)
    return gRollupListener;

  return nsXULPopupManager::GetInstance();
}

void
nsBaseWidget::NotifyWindowDestroyed()
{
  if (!mWidgetListener)
    return;

  nsCOMPtr<nsIXULWindow> window = mWidgetListener->GetXULWindow();
  nsCOMPtr<nsIBaseWindow> xulWindow(do_QueryInterface(window));
  if (xulWindow) {
    xulWindow->Destroy();
  }
}

void
nsBaseWidget::NotifySizeMoveDone()
{
  if (!mWidgetListener || mWidgetListener->GetXULWindow())
    return;

  nsIPresShell* presShell = mWidgetListener->GetPresShell();
  if (presShell) {
    presShell->WindowSizeMoveDone();
  }
}

void
nsBaseWidget::NotifyWindowMoved(int32_t aX, int32_t aY)
{
  if (mWidgetListener) {
    mWidgetListener->WindowMoved(this, aX, aY);
  }

  if (GetIMEUpdatePreference().WantPositionChanged()) {
    NotifyIME(IMENotification(IMEMessage::NOTIFY_IME_OF_POSITION_CHANGE));
  }
}

void
nsBaseWidget::NotifySysColorChanged()
{
  if (!mWidgetListener || mWidgetListener->GetXULWindow())
    return;

  nsIPresShell* presShell = mWidgetListener->GetPresShell();
  if (presShell) {
    presShell->SysColorChanged();
  }
}

void
nsBaseWidget::NotifyThemeChanged()
{
  if (!mWidgetListener || mWidgetListener->GetXULWindow())
    return;

  nsIPresShell* presShell = mWidgetListener->GetPresShell();
  if (presShell) {
    presShell->ThemeChanged();
  }
}

void
nsBaseWidget::NotifyUIStateChanged(UIStateChangeType aShowAccelerators,
                                   UIStateChangeType aShowFocusRings)
{
  if (!mWidgetListener)
    return;

  nsIPresShell* presShell = mWidgetListener->GetPresShell();
  if (!presShell)
    return;

  nsIDocument* doc = presShell->GetDocument();
  if (doc) {
    nsPIDOMWindow* win = doc->GetWindow();
    if (win) {
      win->SetKeyboardIndicators(aShowAccelerators, aShowFocusRings);
    }
  }
}

#ifdef ACCESSIBILITY

a11y::Accessible*
nsBaseWidget::GetRootAccessible()
{
  NS_ENSURE_TRUE(mWidgetListener, nullptr);

  nsIPresShell* presShell = mWidgetListener->GetPresShell();
  NS_ENSURE_TRUE(presShell, nullptr);

  // If container is null then the presshell is not active. This often happens
  // when a preshell is being held onto for fastback.
  nsPresContext* presContext = presShell->GetPresContext();
  NS_ENSURE_TRUE(presContext->GetContainerWeak(), nullptr);

  // Accessible creation might be not safe so use IsSafeToRunScript to
  // make sure it's not created at unsafe times.
  nsCOMPtr<nsIAccessibilityService> accService =
    services::GetAccessibilityService();
  if (accService) {
    return accService->GetRootDocumentAccessible(presShell, nsContentUtils::IsSafeToRunScript());
  }

  return nullptr;
}

#endif // ACCESSIBILITY

nsresult
nsIWidget::SynthesizeNativeTouchTap(nsIntPoint aPointerScreenPoint, bool aLongTap)
{
  if (sPointerIdCounter > TOUCH_INJECT_MAX_POINTS) {
    sPointerIdCounter = 0;
  }
  int pointerId = sPointerIdCounter;
  sPointerIdCounter++;
  nsresult rv = SynthesizeNativeTouchPoint(pointerId, TOUCH_CONTACT,
                                           aPointerScreenPoint, 1.0, 90);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!aLongTap) {
    nsresult rv = SynthesizeNativeTouchPoint(pointerId, TOUCH_REMOVE,
                                             aPointerScreenPoint, 0, 0);
    return rv;
  }

  // initiate a long tap
  int elapse = Preferences::GetInt("ui.click_hold_context_menus.delay",
                                   TOUCH_INJECT_LONG_TAP_DEFAULT_MSEC);
  if (!mLongTapTimer) {
    mLongTapTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      SynthesizeNativeTouchPoint(pointerId, TOUCH_CANCEL,
                                 aPointerScreenPoint, 0, 0);
      return NS_ERROR_UNEXPECTED;
    }
    // Windows requires recuring events, so we set this to a smaller window
    // than the pref value.
    int timeout = elapse;
    if (timeout > TOUCH_INJECT_PUMP_TIMER_MSEC) {
      timeout = TOUCH_INJECT_PUMP_TIMER_MSEC;
    }
    mLongTapTimer->InitWithFuncCallback(OnLongTapTimerCallback, this,
                                        timeout,
                                        nsITimer::TYPE_REPEATING_SLACK);
  }

  // If we already have a long tap pending, cancel it. We only allow one long
  // tap to be active at a time.
  if (mLongTapTouchPoint) {
    SynthesizeNativeTouchPoint(mLongTapTouchPoint->mPointerId, TOUCH_CANCEL,
                               mLongTapTouchPoint->mPosition, 0, 0);
  }

  mLongTapTouchPoint = new LongTapInfo(pointerId, aPointerScreenPoint,
                                       TimeDuration::FromMilliseconds(elapse));
  return NS_OK;
}

// static
void
nsIWidget::OnLongTapTimerCallback(nsITimer* aTimer, void* aClosure)
{
  nsIWidget *self = static_cast<nsIWidget *>(aClosure);

  if ((self->mLongTapTouchPoint->mStamp + self->mLongTapTouchPoint->mDuration) >
      TimeStamp::Now()) {
#ifdef XP_WIN
    // Windows needs us to keep pumping feedback to the digitizer, so update
    // the pointer id with the same position.
    self->SynthesizeNativeTouchPoint(self->mLongTapTouchPoint->mPointerId,
                                     TOUCH_CONTACT,
                                     self->mLongTapTouchPoint->mPosition,
                                     1.0, 90);
#endif
    return;
  }

  // finished, remove the touch point
  self->mLongTapTimer->Cancel();
  self->mLongTapTimer = nullptr;
  self->SynthesizeNativeTouchPoint(self->mLongTapTouchPoint->mPointerId,
                                   TOUCH_REMOVE,
                                   self->mLongTapTouchPoint->mPosition,
                                   0, 0);
  self->mLongTapTouchPoint = nullptr;
}

nsresult
nsIWidget::ClearNativeTouchSequence()
{
  if (!mLongTapTimer) {
    return NS_OK;
  }
  mLongTapTimer->Cancel();
  mLongTapTimer = nullptr;
  SynthesizeNativeTouchPoint(mLongTapTouchPoint->mPointerId, TOUCH_CANCEL,
                             mLongTapTouchPoint->mPosition, 0, 0);
  mLongTapTouchPoint = nullptr;
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
nsBaseWidget::debug_GuiEventToString(WidgetGUIEvent* aGuiEvent)
{
  NS_ASSERTION(nullptr != aGuiEvent,"cmon, null gui event.");

  nsAutoString eventName(NS_LITERAL_STRING("UNKNOWN"));

#define _ASSIGN_eventName(_value,_name)\
case _value: eventName.AssignLiteral(_name) ; break

  switch(aGuiEvent->message)
  {
    _ASSIGN_eventName(NS_BLUR_CONTENT,"NS_BLUR_CONTENT");
    _ASSIGN_eventName(NS_DRAGDROP_GESTURE,"NS_DND_GESTURE");
    _ASSIGN_eventName(NS_DRAGDROP_DROP,"NS_DND_DROP");
    _ASSIGN_eventName(NS_DRAGDROP_ENTER,"NS_DND_ENTER");
    _ASSIGN_eventName(NS_DRAGDROP_EXIT,"NS_DND_EXIT");
    _ASSIGN_eventName(NS_DRAGDROP_OVER,"NS_DND_OVER");
    _ASSIGN_eventName(NS_EDITOR_INPUT,"NS_EDITOR_INPUT");
    _ASSIGN_eventName(NS_FOCUS_CONTENT,"NS_FOCUS_CONTENT");
    _ASSIGN_eventName(NS_FORM_SELECTED,"NS_FORM_SELECTED");
    _ASSIGN_eventName(NS_FORM_CHANGE,"NS_FORM_CHANGE");
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
    _ASSIGN_eventName(NS_LOAD,"NS_LOAD");
    _ASSIGN_eventName(NS_POPSTATE,"NS_POPSTATE");
    _ASSIGN_eventName(NS_BEFORE_SCRIPT_EXECUTE,"NS_BEFORE_SCRIPT_EXECUTE");
    _ASSIGN_eventName(NS_AFTER_SCRIPT_EXECUTE,"NS_AFTER_SCRIPT_EXECUTE");
    _ASSIGN_eventName(NS_PAGE_UNLOAD,"NS_PAGE_UNLOAD");
    _ASSIGN_eventName(NS_HASHCHANGE,"NS_HASHCHANGE");
    _ASSIGN_eventName(NS_READYSTATECHANGE,"NS_READYSTATECHANGE");
    _ASSIGN_eventName(NS_XUL_BROADCAST, "NS_XUL_BROADCAST");
    _ASSIGN_eventName(NS_XUL_COMMAND_UPDATE, "NS_XUL_COMMAND_UPDATE");

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
  NS_ASSERTION(nullptr != aPrefName,"cmon, pref name is null.");

  for (uint32_t i = 0; i < ArrayLength(debug_PrefValues); i++)
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
  NS_ASSERTION(nullptr != aPrefName,"cmon, pref name is null.");

  for (uint32_t i = 0; i < ArrayLength(debug_PrefValues); i++)
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
    ~Debug_PrefObserver() {}

  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS(Debug_PrefObserver, nsIObserver)

NS_IMETHODIMP
Debug_PrefObserver::Observe(nsISupports* subject, const char* topic,
                            const char16_t* data)
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
  for (uint32_t i = 0; i < ArrayLength(debug_PrefValues); i++) {
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
static int32_t
_GetPrintCount()
{
  static int32_t sCount = 0;

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
                              WidgetGUIEvent*       aGuiEvent,
                              const nsAutoCString & aWidgetName,
                              int32_t               aWindowID)
{
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
          "%4d %-26s widget=%-8p name=%-12s id=0x%-6x refpt=%d,%d\n",
          _GetPrintCount(),
          tempString.get(),
          (void *) aWidget,
          aWidgetName.get(),
          aWindowID,
          aGuiEvent->refPoint.x,
          aGuiEvent->refPoint.y);
}
//////////////////////////////////////////////////////////////
/* static */ void
nsBaseWidget::debug_DumpPaintEvent(FILE *                aFileOut,
                                   nsIWidget *           aWidget,
                                   const nsIntRegion &   aRegion,
                                   const nsAutoCString & aWidgetName,
                                   int32_t               aWindowID)
{
  NS_ASSERTION(nullptr != aFileOut,"cmon, null output FILE");
  NS_ASSERTION(nullptr != aWidget,"cmon, the widget is null");

  if (!debug_GetCachedBoolPref("nglayout.debug.paint_dumping"))
    return;

  nsIntRect rect = aRegion.GetBounds();
  fprintf(aFileOut,
          "%4d PAINT      widget=%p name=%-12s id=0x%-6x bounds-rect=%3d,%-3d %3d,%-3d",
          _GetPrintCount(),
          (void *) aWidget,
          aWidgetName.get(),
          aWindowID,
          rect.x, rect.y, rect.width, rect.height
    );

  fprintf(aFileOut,"\n");
}
//////////////////////////////////////////////////////////////
/* static */ void
nsBaseWidget::debug_DumpInvalidate(FILE *                aFileOut,
                                   nsIWidget *           aWidget,
                                   const nsIntRect *     aRect,
                                   const nsAutoCString & aWidgetName,
                                   int32_t               aWindowID)
{
  if (!debug_GetCachedBoolPref("nglayout.debug.invalidate_dumping"))
    return;

  NS_ASSERTION(nullptr != aFileOut,"cmon, null output FILE");
  NS_ASSERTION(nullptr != aWidget,"cmon, the widget is null");

  fprintf(aFileOut,
          "%4d Invalidate widget=%p name=%-12s id=0x%-6x",
          _GetPrintCount(),
          (void *) aWidget,
          aWidgetName.get(),
          aWindowID);

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

