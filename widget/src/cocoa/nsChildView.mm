/* -*- Mode: objc; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
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
 *   Josh Aas <josh@mozilla.com>
 *   Mark Mentovai <mark@moxienet.com>
 *   Håkan Waara <hwaara@gmail.com>
 *   Stuart Morgan <stuart.morgan@alumni.case.edu>
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *   Thomas K. Dyas <tdyas@zecador.org>
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

#include <unistd.h>
 
#include "nsChildView.h"
#include "nsCocoaWindow.h"

#include "nsObjCExceptions.h"
#include "nsCOMPtr.h"
#include "nsToolkit.h"
#include "nsCRT.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

#include "nsIFontMetrics.h"
#include "nsIDeviceContext.h"
#include "nsIRegion.h"
#include "nsIRollupListener.h"
#include "nsIScrollableView.h"
#include "nsIViewManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIServiceManager.h"
#include "nsILocalFile.h"
#include "nsILocalFileMac.h"
#include "nsGfxCIID.h"
#include "nsIMenuRollup.h"
#include "nsIDOMSimpleGestureEvent.h"
#include "nsIPluginInstance.h"

#include "nsDragService.h"
#include "nsClipboard.h"
#include "nsCursorManager.h"
#include "nsWindowMap.h"
#include "nsCocoaUtils.h"
#include "nsMenuUtilsX.h"
#include "nsMenuBarX.h"

#include "gfxContext.h"
#include "gfxQuartzSurface.h"
#include "nsRegion.h"

#include <dlfcn.h>

#include <ApplicationServices/ApplicationServices.h>

#undef DEBUG_IME
#undef DEBUG_UPDATE
#undef INVALIDATE_DEBUGGING  // flash areas as they are invalidated

// Don't put more than this many rects in the dirty region, just fluff
// out to the bounding-box if there are more
#define MAX_RECTS_IN_REGION 100

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif
#include "prlog.h"

#ifdef PR_LOGGING
PRLogModuleInfo* sCocoaLog = nsnull;
#endif

extern "C" {
  CG_EXTERN void CGContextResetCTM(CGContextRef);
  CG_EXTERN void CGContextSetCTM(CGContextRef, CGAffineTransform);
  CG_EXTERN void CGContextResetClip(CGContextRef);
}

#ifndef NS_LEOPARD_AND_LATER

#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4
struct __TISInputSource;
typedef __TISInputSource* TISInputSourceRef;
#endif
TISInputSourceRef (*Leopard_TISCopyCurrentKeyboardInputSource)() = NULL;
TISInputSourceRef (*Leopard_TISCopyCurrentKeyboardLayoutInputSource)() = NULL;
void* (*Leopard_TISGetInputSourceProperty)(TISInputSourceRef inputSource, CFStringRef propertyKey) = NULL;
CFArrayRef (*Leopard_TISCreateInputSourceList)(CFDictionaryRef properties, Boolean includeAllInstalled) = NULL;
CFStringRef kOurTISPropertyUnicodeKeyLayoutData = NULL;
CFStringRef kOurTISPropertyInputSourceID = NULL;
CFStringRef kOurTISPropertyInputSourceLanguages = NULL;

PRBool nsTSMManager::sIsIMEEnabled = PR_TRUE;
PRBool nsTSMManager::sIsRomanKeyboardsOnly = PR_FALSE;
PRBool nsTSMManager::sIgnoreCommit = PR_FALSE;
NSView<mozView>* nsTSMManager::sComposingView = nsnull;
TSMDocumentID nsTSMManager::sDocumentID = nsnull;
NSString* nsTSMManager::sComposingString = nsnull;

#endif // NS_LEOPARD_AND_LATER

// these are defined in nsCocoaWindow.mm
extern PRBool gConsumeRollupEvent;

PRBool gChildViewMethodsSwizzled = PR_FALSE;

extern nsISupportsArray *gDraggedTransferables;

ChildView* ChildViewMouseTracker::sLastMouseEventView = nil;

static NS_DEFINE_CID(kRegionCID, NS_REGION_CID);
#ifdef INVALIDATE_DEBUGGING
static void blinkRect(Rect* r);
static void blinkRgn(RgnHandle rgn);
#endif

nsIRollupListener * gRollupListener = nsnull;
nsIWidget         * gRollupWidget   = nsnull;

PRUint32 gLastModifierState = 0;

PRBool gUserCancelledDrag = PR_FALSE;

PRUint32 nsChildView::sLastInputEventCount = 0;

@interface ChildView(Private)

// sets up our view, attaching it to its owning gecko view
- (id)initWithFrame:(NSRect)inFrame geckoChild:(nsChildView*)inChild;

// sends gecko an ime composition event
- (nsIntRect) sendCompositionEvent:(PRInt32)aEventType;

// sends gecko an ime text event
- (void) sendTextEvent:(PRUnichar*) aBuffer 
                       attributedString:(NSAttributedString*) aString
                       selectedRange:(NSRange)selRange
                       markedRange:(NSRange)markRange
                       doCommit:(BOOL)doCommit;

// do generic gecko event setup with a generic cocoa event. accepts nil inEvent.
- (void) convertGenericCocoaEvent:(NSEvent*)inEvent toGeckoEvent:(nsInputEvent*)outGeckoEvent;

// set up a gecko mouse event based on a cocoa mouse event
- (void) convertCocoaMouseEvent:(NSEvent*)aMouseEvent toGeckoEvent:(nsInputEvent*)outGeckoEvent;

// set up a gecko key event based on a cocoa key event
- (void) convertCocoaKeyEvent:(NSEvent*)aKeyEvent toGeckoEvent:(nsKeyEvent*)outGeckoEvent;

- (NSMenu*)contextMenu;

- (void)setIsPluginView:(BOOL)aIsPlugin;
- (BOOL)isPluginView;
- (void)setPluginEventModel:(NPEventModel)eventModel;
- (NPEventModel)pluginEventModel;

- (BOOL)isRectObscuredBySubview:(NSRect)inRect;

- (void)processPendingRedraws;

- (PRBool)processKeyDownEvent:(NSEvent*)theEvent keyEquiv:(BOOL)isKeyEquiv;

- (void)maybeInitContextMenuTracking;

+ (NSEvent*)makeNewCocoaEventWithType:(NSEventType)type fromEvent:(NSEvent*)theEvent;

- (void)maybeInvalidateShadow;
- (void)invalidateShadow;

#if USE_CLICK_HOLD_CONTEXTMENU
 // called on a timer two seconds after a mouse down to see if we should display
 // a context menu (click-hold)
- (void)clickHoldCallback:(id)inEvent;
#endif

#ifdef ACCESSIBILITY
- (id<mozAccessible>)accessible;
#endif

- (BOOL)isFirstResponder;

- (void)fireKeyEventForFlagsChanged:(NSEvent*)theEvent keyDown:(BOOL)isKeyDown;

- (void)initTSMDocument;
@end

#pragma mark -

// Key code constants
enum
{
  kEscapeKeyCode      = 0x35,
  kRCommandKeyCode    = 0x36, // right command key
  kCommandKeyCode     = 0x37,
  kShiftKeyCode       = 0x38,
  kCapsLockKeyCode    = 0x39,
  kOptionkeyCode      = 0x3A,
  kControlKeyCode     = 0x3B,
  kRShiftKeyCode      = 0x3C, // right shift key
  kROptionKeyCode     = 0x3D, // right option key
  kRControlKeyCode    = 0x3E, // right control key
  kClearKeyCode       = 0x47,

  // function keys
  kF1KeyCode          = 0x7A,
  kF2KeyCode          = 0x78,
  kF3KeyCode          = 0x63,
  kF4KeyCode          = 0x76,
  kF5KeyCode          = 0x60,
  kF6KeyCode          = 0x61,
  kF7KeyCode          = 0x62,
  kF8KeyCode          = 0x64,
  kF9KeyCode          = 0x65,
  kF10KeyCode         = 0x6D,
  kF11KeyCode         = 0x67,
  kF12KeyCode         = 0x6F,
  kF13KeyCode         = 0x69,
  kF14KeyCode         = 0x6B,
  kF15KeyCode         = 0x71,
  
  kPrintScreenKeyCode = kF13KeyCode,
  kScrollLockKeyCode  = kF14KeyCode,
  kPauseKeyCode       = kF15KeyCode,
  
  // keypad
  kKeypad0KeyCode     = 0x52,
  kKeypad1KeyCode     = 0x53,
  kKeypad2KeyCode     = 0x54,
  kKeypad3KeyCode     = 0x55,
  kKeypad4KeyCode     = 0x56,
  kKeypad5KeyCode     = 0x57,
  kKeypad6KeyCode     = 0x58,
  kKeypad7KeyCode     = 0x59,
  kKeypad8KeyCode     = 0x5B,
  kKeypad9KeyCode     = 0x5C,

// The following key codes are not defined until Mac OS X 10.5
#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4
  kVK_ANSI_1          = 0x12,
  kVK_ANSI_2          = 0x13,
  kVK_ANSI_3          = 0x14,
  kVK_ANSI_4          = 0x15,
  kVK_ANSI_5          = 0x17,
  kVK_ANSI_6          = 0x16,
  kVK_ANSI_7          = 0x1A,
  kVK_ANSI_8          = 0x1C,
  kVK_ANSI_9          = 0x19,
  kVK_ANSI_0          = 0x1D,
#endif

  kKeypadMultiplyKeyCode  = 0x43,
  kKeypadAddKeyCode       = 0x45,
  kKeypadSubtractKeyCode  = 0x4E,
  kKeypadDecimalKeyCode   = 0x41,
  kKeypadDivideKeyCode    = 0x4B,
  kKeypadEqualsKeyCode    = 0x51, // no correpsonding gecko key code
  kEnterKeyCode           = 0x4C,
  kReturnKeyCode          = 0x24,
  kPowerbookEnterKeyCode  = 0x34, // Enter on Powerbook's keyboard is different
  
  kInsertKeyCode          = 0x72, // also help key
  kDeleteKeyCode          = 0x75, // also forward delete key
  kTabKeyCode             = 0x30,
  kTildeKeyCode           = 0x32,
  kBackspaceKeyCode       = 0x33,
  kHomeKeyCode            = 0x73, 
  kEndKeyCode             = 0x77,
  kPageUpKeyCode          = 0x74,
  kPageDownKeyCode        = 0x79,
  kLeftArrowKeyCode       = 0x7B,
  kRightArrowKeyCode      = 0x7C,
  kUpArrowKeyCode         = 0x7E,
  kDownArrowKeyCode       = 0x7D
};

/* Convenience routines to go from a gecko rect to cocoa NSRects and back
 *
 * Gecko rects (nsRect) contain an origin (x,y) in a coordinate
 * system with (0,0) in the top-left of the screen. Cocoa rects
 * (NSRect) contain an origin (x,y) in a coordinate system with
 * (0,0) in the bottom-left of the screen. Both nsRect and NSRect
 * contain width/height info, with no difference in their use.
 * If a Cocoa rect is from a flipped view, there is no need to
 * convert coordinate systems.
 */

static inline void
GeckoRectToNSRect(const nsIntRect & inGeckoRect, NSRect & outCocoaRect)
{
  outCocoaRect.origin.x = inGeckoRect.x;
  outCocoaRect.origin.y = inGeckoRect.y;
  outCocoaRect.size.width = inGeckoRect.width;
  outCocoaRect.size.height = inGeckoRect.height;
}

static inline void
NSRectToGeckoRect(const NSRect & inCocoaRect, nsIntRect & outGeckoRect)
{
  outGeckoRect.x = NSToIntRound(inCocoaRect.origin.x);
  outGeckoRect.y = NSToIntRound(inCocoaRect.origin.y);
  outGeckoRect.width = NSToIntRound(inCocoaRect.origin.x + inCocoaRect.size.width) - outGeckoRect.x;
  outGeckoRect.height = NSToIntRound(inCocoaRect.origin.y + inCocoaRect.size.height) - outGeckoRect.y;
}

static inline void 
ConvertGeckoRectToMacRect(const nsIntRect& aRect, Rect& outMacRect)
{
  outMacRect.left = aRect.x;
  outMacRect.top = aRect.y;
  outMacRect.right = aRect.x + aRect.width;
  outMacRect.bottom = aRect.y + aRect.height;
}

// Flips a screen coordinate from a point in the cocoa coordinate system (bottom-left rect) to a point
// that is a "flipped" cocoa coordinate system (starts in the top-left).
static inline void
FlipCocoaScreenCoordinate(NSPoint &inPoint)
{  
  inPoint.y = nsCocoaUtils::FlippedScreenY(inPoint.y);
}

static void
InitNPCocoaEvent(NPCocoaEvent* event)
{
  memset(event, 0, sizeof(NPCocoaEvent));
}

static PRUint32
UnderlineAttributeToTextRangeType(PRUint32 aUnderlineStyle, NSRange selRange)
{
#ifdef DEBUG_IME
  NSLog(@"****in underlineAttributeToTextRangeType = %d", aUnderlineStyle);
#endif

  // For more info on the underline attribute, please see: 
  // http://developer.apple.com/techpubs/macosx/Cocoa/TasksAndConcepts/ProgrammingTopics/AttributedStrings/Tasks/AccessingAttrs.html
  // We are not clear where the define for value 2 is right now. 
  // To see this value in japanese ime, type 'aaaaaaaaa' and hit space to make the
  // ime send you some part of text in 1 (NSSingleUnderlineStyle) and some part in 2. 
  // ftang will ask apple for more details
  //
  // It probably means show 1-pixel thickness underline vs 2-pixel thickness.
  
  PRUint32 attr;
  if (selRange.length == 0) {
    switch (aUnderlineStyle) {
      case 1:
        attr = NS_TEXTRANGE_RAWINPUT;
        break;
      case 2:
      default:
        attr = NS_TEXTRANGE_SELECTEDRAWTEXT;
        break;
    }
  }
  else {
    switch (aUnderlineStyle) {
      case 1:
        attr = NS_TEXTRANGE_CONVERTEDTEXT;
        break;
      case 2:
      default:
        attr = NS_TEXTRANGE_SELECTEDCONVERTEDTEXT;
        break;
    }
  }
  return attr;
}

static PRUint32
CountRanges(NSAttributedString *aString)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  // Iterate through aString for the NSUnderlineStyleAttributeName and count the 
  // different segments adjusting limitRange as we go.
  PRUint32 count = 0;
  NSRange effectiveRange;
  NSRange limitRange = NSMakeRange(0, [aString length]);
  while (limitRange.length > 0) {
    [aString attribute:NSUnderlineStyleAttributeName 
               atIndex:limitRange.location 
 longestEffectiveRange:&effectiveRange
               inRange:limitRange];
    limitRange = NSMakeRange(NSMaxRange(effectiveRange), 
                             NSMaxRange(limitRange) - NSMaxRange(effectiveRange));
    count++;
  }
  return count;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(0);
}

static void
ConvertAttributeToGeckoRange(NSAttributedString *aString, NSRange markRange, NSRange selRange, PRUint32 inCount, nsTextRange* aRanges)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Convert the Cocoa range into the nsTextRange Array used in Gecko.
  // Iterate through the attributed string and map the underline attribute to Gecko IME textrange attributes.
  // We may need to change the code here if we change the implementation of validAttributesForMarkedText.
  PRUint32 i = 0;
  NSRange effectiveRange;
  NSRange limitRange = NSMakeRange(0, [aString length]);
  while ((limitRange.length > 0) && (i < inCount)) {
    id attributeValue = [aString attribute:NSUnderlineStyleAttributeName 
                              atIndex:limitRange.location 
                              longestEffectiveRange:&effectiveRange
                              inRange:limitRange];
    aRanges[i].mStartOffset = effectiveRange.location;                         
    aRanges[i].mEndOffset = NSMaxRange(effectiveRange);                         
    aRanges[i].mRangeType = UnderlineAttributeToTextRangeType([attributeValue intValue], selRange); 
    limitRange = NSMakeRange(NSMaxRange(effectiveRange), 
                             NSMaxRange(limitRange) - NSMaxRange(effectiveRange));
    i++;
  }
  // Get current caret position.
  // Caret is indicator of insertion point, so mEndOffset = 0.
  aRanges[i].mStartOffset = selRange.location + selRange.length;                         
  aRanges[i].mEndOffset = 0;                         
  aRanges[i].mRangeType = NS_TEXTRANGE_CARETPOSITION;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

static void
FillTextRangeInTextEvent(nsTextEvent *aTextEvent, NSAttributedString* aString, NSRange markRange, NSRange selRange)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Count the number of segments in the attributed string and add one more count for sending current caret position to Gecko.
  // Allocate the right size of nsTextRange and draw caret at right position.
  // Convert the attributed string into an array of nsTextRange and get current caret position by calling above functions.
  PRUint32 count = CountRanges(aString) + 1;
  aTextEvent->rangeArray = new nsTextRange[count];
  if (aTextEvent->rangeArray) {
    aTextEvent->rangeCount = count;
    ConvertAttributeToGeckoRange(aString, markRange, selRange, aTextEvent->rangeCount,  aTextEvent->rangeArray);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

#if defined(DEBUG) && defined(PR_LOGGING)

static void DebugPrintAllKeyboardLayouts()
{
#ifdef NS_LEOPARD_AND_LATER
  nsCocoaTextInputHandler::DebugPrintAllKeyboardLayouts(sCocoaLog);
  nsCocoaIMEHandler::DebugPrintAllIMEModes(sCocoaLog);
#else
  CFIndex idx;
  KLGetKeyboardLayoutCount(&idx);
  PR_LOG(sCocoaLog, PR_LOG_ALWAYS, ("Keyboard layout configuration:"));
  for (CFIndex i = 0; i < idx; ++i) {
    KeyboardLayoutRef curKL;
    if (KLGetKeyboardLayoutAtIndex(i, &curKL) == noErr) {
      CFStringRef name;
      if (KLGetKeyboardLayoutProperty(curKL, kKLName, (const void**)&name) == noErr) {
        int idn;
        KLGetKeyboardLayoutProperty(curKL, kKLIdentifier, (const void**)&idn);
        int kind;
        KLGetKeyboardLayoutProperty(curKL, kKLKind, (const void**)&kind);
        char buf[256];
        CFStringGetCString(name, buf, 256, kCFStringEncodingASCII);
        PR_LOG(sCocoaLog, PR_LOG_ALWAYS, ("  %d,%s,%d\n", idn, buf, kind));
      }
    }
  }
#endif // NS_LEOPARD_AND_LATER
}

#endif // defined(DEBUG) && defined(PR_LOGGING)

#pragma mark -

nsChildView::nsChildView() : nsBaseWidget()
, mView(nsnull)
, mParentView(nsnull)
, mParentWidget(nsnull)
, mVisible(PR_FALSE)
, mDrawing(PR_FALSE)
, mPluginDrawing(PR_FALSE)
, mPluginIsCG(PR_FALSE)
{
#ifdef PR_LOGGING
  if (!sCocoaLog) {
    sCocoaLog = PR_NewLogModule("nsCocoaWidgets");
#ifdef DEBUG
    DebugPrintAllKeyboardLayouts();
#endif // DEBUG
  }
#endif // PR_LOGGING

  SetBackgroundColor(NS_RGB(255, 255, 255));
  SetForegroundColor(NS_RGB(0, 0, 0));

#ifndef NS_LEOPARD_AND_LATER
  if (nsToolkit::OnLeopardOrLater() && !Leopard_TISCopyCurrentKeyboardLayoutInputSource) {
    // This libary would already be open for LMGetKbdType (and probably other
    // symbols), so merely using RTLD_DEFAULT in dlsym would be sufficient,
    // but man dlsym says: "all mach-o images in the process (except ...) are
    // searched in the order they were loaded.  This can be a costly search
    // and should be avoided."
    void* hitoolboxHandle = dlopen("/System/Library/Frameworks/Carbon.framework/Frameworks/HIToolbox.framework/Versions/A/HIToolbox", RTLD_LAZY);
    if (hitoolboxHandle) {
      *(void **)(&Leopard_TISCopyCurrentKeyboardInputSource) = dlsym(hitoolboxHandle, "TISCopyCurrentKeyboardInputSource");
      *(void **)(&Leopard_TISCopyCurrentKeyboardLayoutInputSource) = dlsym(hitoolboxHandle, "TISCopyCurrentKeyboardLayoutInputSource");
      *(void **)(&Leopard_TISGetInputSourceProperty) = dlsym(hitoolboxHandle, "TISGetInputSourceProperty");
      *(void **)(&Leopard_TISCreateInputSourceList) = dlsym(hitoolboxHandle, "TISCreateInputSourceList");
      kOurTISPropertyUnicodeKeyLayoutData = *static_cast<CFStringRef*>(dlsym(hitoolboxHandle, "kTISPropertyUnicodeKeyLayoutData"));
      kOurTISPropertyInputSourceID = *static_cast<CFStringRef*>(dlsym(hitoolboxHandle, "kTISPropertyInputSourceID"));
      kOurTISPropertyInputSourceLanguages = *static_cast<CFStringRef*>(dlsym(hitoolboxHandle, "kTISPropertyInputSourceLanguages"));
    }
  }
#endif // NS_LEOPARD_AND_LATER
}

nsChildView::~nsChildView()
{
  // Notify the children that we're gone.  childView->ResetParent() can change
  // our list of children while it's being iterated, so the way we iterate the
  // list must allow for this.
  for (nsIWidget* kid = mLastChild; kid;) {
    nsChildView* childView = static_cast<nsChildView*>(kid);
    kid = kid->GetPrevSibling();
    childView->ResetParent();
  }

  NS_WARN_IF_FALSE(mOnDestroyCalled, "nsChildView object destroyed without calling Destroy()");

  // An nsChildView object that was in use can be destroyed without Destroy()
  // ever being called on it.  So we also need to do a quick, safe cleanup
  // here (it's too late to just call Destroy(), which can cause crashes).
  // It's particularly important to make sure widgetDestroyed is called on our
  // mView -- this method NULLs mView's mGeckoChild, and NULL checks on
  // mGeckoChild are used throughout the ChildView class to tell if it's safe
  // to use a ChildView object.
  [mView widgetDestroyed]; // Safe if mView is nil.
  mParentWidget = nil;
  TearDownView(); // Safe if called twice.
}

NS_IMPL_ISUPPORTS_INHERITED1(nsChildView, nsBaseWidget, nsIPluginWidget)

nsresult nsChildView::Create(nsIWidget *aParent,
                             nsNativeWidget aNativeParent,
                             const nsIntRect &aRect,
                             EVENT_CALLBACK aHandleEventFunction,
                             nsIDeviceContext *aContext,
                             nsIAppShell *aAppShell,
                             nsIToolkit *aToolkit,
                             nsWidgetInitData *aInitData)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // See NSView (MethodSwizzling) below.
  if (nsToolkit::OnLeopardOrLater() && !gChildViewMethodsSwizzled) {
    nsToolkit::SwizzleMethods([NSView class], @selector(mouseDownCanMoveWindow),
                              @selector(nsChildView_NSView_mouseDownCanMoveWindow));
    gChildViewMethodsSwizzled = PR_TRUE;
  }

  mBounds = aRect;

  BaseCreate(aParent, aRect, aHandleEventFunction, 
             aContext, aAppShell, aToolkit, aInitData);

  // inherit things from the parent view and create our parallel 
  // NSView in the Cocoa display system
  mParentView = nil;
  if (aParent) {
    // This is the case when we're the popup content view of a popup window.
    SetBackgroundColor(aParent->GetBackgroundColor());
    SetForegroundColor(aParent->GetForegroundColor());

    // inherit the top-level window. NS_NATIVE_WIDGET is always a NSView
    // regardless of if we're asking a window or a view (for compatibility
    // with windows).
    mParentView = (NSView*)aParent->GetNativeData(NS_NATIVE_WIDGET); 
    mParentWidget = aParent;   
  } else {
    // This is the normal case. When we're the root widget of the view hiararchy,
    // aNativeParent will be the contentView of our window, since that's what
    // nsCocoaWindow returns when asked for an NS_NATIVE_VIEW.
    mParentView = reinterpret_cast<NSView*>(aNativeParent);
  }
  
  // create our parallel NSView and hook it up to our parent. Recall
  // that NS_NATIVE_WIDGET is the NSView.
  NSRect r;
  GeckoRectToNSRect(mBounds, r);
  mView = [CreateCocoaView(r) retain];
  if (!mView) return NS_ERROR_FAILURE;

  // If this view was created in a Gecko view hierarchy, the initial state
  // is hidden.  If the view is attached only to a native NSView but has
  // no Gecko parent (as in embedding), the initial state is visible.
  if (mParentWidget)
    [mView setHidden:YES];
  else
    mVisible = PR_TRUE;

  // Hook it up in the NSView hierarchy.
  if (mParentView) {
    [mParentView addSubview:mView];
  }

  // if this is a ChildView, make sure that our per-window data
  // is set up
  if ([mView isKindOfClass:[ChildView class]])
    [[WindowDataMap sharedWindowDataMap] ensureDataForWindow:[mView window]];

#ifdef NS_LEOPARD_AND_LATER
  mTextInputHandler.Init(this);
#endif // NS_LEOPARD_AND_LATER

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

// Creates the appropriate child view. Override to create something other than
// our |ChildView| object. Autoreleases, so caller must retain.
NSView*
nsChildView::CreateCocoaView(NSRect inFrame)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  return [[[ChildView alloc] initWithFrame:inFrame geckoChild:this] autorelease];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

void nsChildView::TearDownView()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mView)
    return;

  NSWindow* win = [mView window];
  NSResponder* responder = [win firstResponder];

  // We're being unhooked from the view hierarchy, don't leave our view
  // or a child view as the window first responder.
  if (responder && [responder isKindOfClass:[NSView class]] &&
      [(NSView*)responder isDescendantOf:mView]) {
    [win makeFirstResponder:[mView superview]];
  }

  // If mView is win's contentView, win (mView's NSWindow) "owns" mView --
  // win has retained mView, and will detach it from the view hierarchy and
  // release it when necessary (when win is itself destroyed (in a call to
  // [win dealloc])).  So all we need to do here is call [mView release] (to
  // match the call to [mView retain] in nsChildView::StandardCreate()).
  // Also calling [mView removeFromSuperviewWithoutNeedingDisplay] causes
  // mView to be released again and dealloced, while remaining win's
  // contentView.  So if we do that here, win will (for a short while) have
  // an invalid contentView (for the consequences see bmo bugs 381087 and
  // 374260).
  if ([mView isEqual:[win contentView]]) {
    [mView release];
  } else {
    // Stop NSView hierarchy being changed during [ChildView drawRect:]
    [mView performSelectorOnMainThread:@selector(delayedTearDown) withObject:nil waitUntilDone:false];
  }
  mView = nil;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsCocoaWindow*
nsChildView::GetXULWindowWidget()
{
  id windowDelegate = [[mView window] delegate];
  if (windowDelegate && [windowDelegate isKindOfClass:[WindowDelegate class]]) {
    return [(WindowDelegate *)windowDelegate geckoWidget];
  }
  return nsnull;
}

NS_IMETHODIMP nsChildView::Destroy()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (mOnDestroyCalled)
    return NS_OK;
  mOnDestroyCalled = PR_TRUE;

  [mView widgetDestroyed];

  nsBaseWidget::Destroy();

  ReportDestroyEvent(); 
  mParentWidget = nil;

  TearDownView();

  nsBaseWidget::OnDestroy();

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

#pragma mark -

#if 0
static void PrintViewHierarchy(NSView *view)
{
  while (view) {
    NSLog(@"  view is %x, frame %@", view, NSStringFromRect([view frame]));
    view = [view superview];
  }
}
#endif

// Return native data according to aDataType
void* nsChildView::GetNativeData(PRUint32 aDataType)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSNULL;

  void* retVal = nsnull;

  switch (aDataType) 
  {
    case NS_NATIVE_WIDGET:
    case NS_NATIVE_DISPLAY:
      retVal = (void*)mView;
      break;

    case NS_NATIVE_WINDOW:
      retVal = [mView window];
      break;

    case NS_NATIVE_GRAPHIC:
      NS_ERROR("Requesting NS_NATIVE_GRAPHIC on a Mac OS X child view!");
      retVal = nsnull;
      break;

    case NS_NATIVE_OFFSETX:
      retVal = 0;
      break;

    case NS_NATIVE_OFFSETY:
      retVal = 0;
      break;

    case NS_NATIVE_PLUGIN_PORT:
    case NS_NATIVE_PLUGIN_PORT_QD:
    case NS_NATIVE_PLUGIN_PORT_CG:
    {
#ifdef NP_NO_QUICKDRAW
      aDataType = NS_NATIVE_PLUGIN_PORT_CG;
#endif
      mPluginIsCG = (aDataType == NS_NATIVE_PLUGIN_PORT_CG);
      if ([mView isKindOfClass:[ChildView class]])
        [(ChildView*)mView setIsPluginView:YES];

      UpdatePluginPort();
      if (mPluginIsCG)
        retVal = (void*)&mPluginCGContext;
      else
        retVal = (void*)&mPluginQDPort;
      break;
    }
  }

  return retVal;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSNULL;
}

#pragma mark -

nsTransparencyMode nsChildView::GetTransparencyMode()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  return [mView isOpaque] ? eTransparencyOpaque : eTransparencyTransparent;

  NS_OBJC_END_TRY_ABORT_BLOCK;
  return eTransparencyOpaque;
}

// This is called by nsContainerFrame on the root widget for all window types
// except popup windows (when nsCocoaWindow::SetTransparencyMode is used instead).
void nsChildView::SetTransparencyMode(nsTransparencyMode aMode)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  BOOL isTransparent = aMode == eTransparencyTransparent;
  BOOL currentTransparency = ![[mView window] isOpaque];
  if (isTransparent != currentTransparency) {
    nsCocoaWindow *widget = GetXULWindowWidget();
    if (widget) {
      widget->MakeBackgroundTransparent(aMode);
      [(ChildView*)mView setTransparent:isTransparent];
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

NS_IMETHODIMP nsChildView::IsVisible(PRBool& outState)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!mVisible) {
    outState = mVisible;
  }
  else {
    // mVisible does not accurately reflect the state of a hidden tabbed view
    // so verify that the view has a window as well
    outState = ([mView window] != nil);
    // now check native widget hierarchy visibility
    if (outState && NSIsEmptyRect([mView visibleRect])) {
      outState = PR_FALSE;
    }
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

void nsChildView::HidePlugin()
{
  NS_ASSERTION(mWindowType == eWindowType_plugin,
               "HidePlugin called on non-plugin view");

  if (mPluginInstanceOwner && !mPluginIsCG) {
    NPWindow* window;
    mPluginInstanceOwner->GetWindow(window);
    nsCOMPtr<nsIPluginInstance> instance;
    mPluginInstanceOwner->GetInstance(*getter_AddRefs(instance));
    if (window && instance) {
       window->clipRect.top = 0;
       window->clipRect.left = 0;
       window->clipRect.bottom = 0;
       window->clipRect.right = 0;
       instance->SetWindow(window);
    }
  }
}

void nsChildView::UpdatePluginPort()
{
  NS_ASSERTION(mWindowType == eWindowType_plugin,
               "UpdatePluginPort called on non-plugin view");

  NSWindow* cocoaWindow = [mView window];
#if !defined(NP_NO_CARBON) || !defined(NP_NO_QUICKDRAW)
  WindowRef carbonWindow = cocoaWindow ? (WindowRef)[cocoaWindow windowRef] : NULL;
#endif

  if (mPluginIsCG) {
    // [NSGraphicsContext currentContext] is supposed to "return the
    // current graphics context of the current thread."  But sometimes
    // (when called while mView isn't focused for drawing) it returns a
    // graphics context for the wrong window.  [window graphicsContext]
    // (which "provides the graphics context associated with the window
    // for the current thread") seems always to return the "right"
    // graphics context.  See bug 500130.
    mPluginCGContext.context = NULL;
    mPluginCGContext.window = NULL;
#ifndef NP_NO_CARBON
    if ([(ChildView*)mView pluginEventModel] == NPEventModelCarbon) {
      if (carbonWindow) {
        mPluginCGContext.context = (CGContextRef)[[cocoaWindow graphicsContext] graphicsPort];
        mPluginCGContext.window = carbonWindow;
      }
    }
#endif
  }
#ifndef NP_NO_QUICKDRAW
  else {
    if (carbonWindow) {
      mPluginQDPort.port = ::GetWindowPort(carbonWindow);

      NSPoint viewOrigin = [mView convertPoint:NSZeroPoint toView:nil];
      NSRect frame = [[cocoaWindow contentView] frame];
      viewOrigin.y = frame.size.height - viewOrigin.y;

      // need to convert view's origin to window coordinates.
      // then, encode as "SetOrigin" ready values.
      mPluginQDPort.portx = (PRInt32)-viewOrigin.x;
      mPluginQDPort.porty = (PRInt32)-viewOrigin.y;
    } else {
      mPluginQDPort.port = NULL;
    }
  }
#endif
}

static void HideChildPluginViews(NSView* aView)
{
  NSArray* subviews = [aView subviews];

  for (unsigned int i = 0; i < [subviews count]; ++i) {
    NSView* view = [subviews objectAtIndex: i];

    if (![view isKindOfClass:[ChildView class]])
      continue;

    ChildView* childview = static_cast<ChildView*>(view);
    if ([childview isPluginView]) {
      nsChildView* widget = static_cast<nsChildView*>([childview widget]);
      if (widget) {
        widget->HidePlugin();
      }
    } else {
      HideChildPluginViews(view);
    }
  }
}

// Hide or show this component
NS_IMETHODIMP nsChildView::Show(PRBool aState)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (aState != mVisible) {
    [mView setHidden:!aState];
    mVisible = aState;
    if (!mVisible)
      HideChildPluginViews(mView);
  }
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

// Change the parent of this widget
NS_IMETHODIMP
nsChildView::SetParent(nsIWidget* aNewParent)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NS_ENSURE_ARG(aNewParent);
  NSView<mozView>* newParentView =
   (NSView*)aNewParent->GetNativeData(NS_NATIVE_WIDGET); 
  NS_ENSURE_TRUE(newParentView, NS_ERROR_FAILURE);

  if (mOnDestroyCalled)
    return NS_OK;

  // make sure we stay alive
  nsCOMPtr<nsIWidget> kungFuDeathGrip(this);
  
  // remove us from our existing parent
  if (mParentWidget)
    mParentWidget->RemoveChild(this);
  // we hold a ref to mView, so this is safe
  [mView removeFromSuperview];
  
  // add us to the new parent
  aNewParent->AddChild(this);
  mParentWidget = aNewParent;
  mParentView   = newParentView;
  [mParentView addSubview:mView];
  return NS_OK;
  
  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

void nsChildView::ResetParent()
{
  if (!mOnDestroyCalled) {
    if (mParentWidget)
      mParentWidget->RemoveChild(this);
    if (mView)
      [mView removeFromSuperview];
  }
  mParentWidget = nsnull;
}

nsIWidget*
nsChildView::GetParent(void)
{
  return mParentWidget;
}

NS_IMETHODIMP nsChildView::Enable(PRBool aState)
{
  return NS_OK;
}

NS_IMETHODIMP nsChildView::IsEnabled(PRBool *aState)
{
  // unimplemented
  if (aState)
   *aState = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsChildView::SetFocus(PRBool aRaise)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // Don't so anything if we're invisible (if Show(PR_FALSE) has been
  // called on us, or if Show(PR_TRUE) hasn't yet been called).  This
  // resolves bug 504450.
  if (mView && ![mView isHidden]) {
    NSWindow* window = [mView window];
    if (window)
      [window makeFirstResponder:mView];
  }
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

// Override to set the cursor on the mac
NS_IMETHODIMP nsChildView::SetCursor(nsCursor aCursor)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsBaseWidget::SetCursor(aCursor);
  [[nsCursorManager sharedInstance] setCursor: aCursor];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

// implement to fix "hidden virtual function" warning
NS_IMETHODIMP nsChildView::SetCursor(imgIContainer* aCursor,
                                      PRUint32 aHotspotX, PRUint32 aHotspotY)
{
  return nsBaseWidget::SetCursor(aCursor, aHotspotX, aHotspotY);
}

#pragma mark -

// Get this component dimension
NS_IMETHODIMP nsChildView::GetBounds(nsIntRect &aRect)
{
  aRect = mBounds;
  return NS_OK;
}

NS_IMETHODIMP nsChildView::ConstrainPosition(PRBool aAllowSlop,
                                             PRInt32 *aX, PRInt32 *aY)
{
  return NS_OK;
}

// Move this component, aX and aY are in the parent widget coordinate system
NS_IMETHODIMP nsChildView::Move(PRInt32 aX, PRInt32 aY)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!mView || (mBounds.x == aX && mBounds.y == aY))
    return NS_OK;

  mBounds.x = aX;
  mBounds.y = aY;

  NSRect r;
  GeckoRectToNSRect(mBounds, r);
  [mView setFrame:r];

  if (mVisible)
    [mView setNeedsDisplay:YES];

  ReportMoveEvent();

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsChildView::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!mView || (mBounds.width == aWidth && mBounds.height == aHeight))
    return NS_OK;

  mBounds.width  = aWidth;
  mBounds.height = aHeight;

  NSRect r;
  GeckoRectToNSRect(mBounds, r);
  [mView setFrame:r];

  if (mVisible && aRepaint)
    [mView setNeedsDisplay:YES];

  ReportSizeEvent();

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsChildView::Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  BOOL isMoving = (mBounds.x != aX || mBounds.y != aY);
  BOOL isResizing = (mBounds.width != aWidth || mBounds.height != aHeight);
  if (!mView || (!isMoving && !isResizing))
    return NS_OK;

  if (isMoving) {
    mBounds.x = aX;
    mBounds.y = aY;
  }
  if (isResizing) {
    mBounds.width  = aWidth;
    mBounds.height = aHeight;
  }

  NSRect r;
  GeckoRectToNSRect(mBounds, r);
  [mView setFrame:r];

  if (mVisible && aRepaint)
    [mView setNeedsDisplay:YES];

  if (isMoving) {
    ReportMoveEvent();
    if (mOnDestroyCalled)
      return NS_OK;
  }
  if (isResizing)
    ReportSizeEvent();

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

static const PRInt32 resizeIndicatorWidth = 15;
static const PRInt32 resizeIndicatorHeight = 15;
PRBool nsChildView::ShowsResizeIndicator(nsIntRect* aResizerRect)
{
  NSView *topLevelView = mView, *superView = nil;
  while ((superView = [topLevelView superview]))
    topLevelView = superView;

  if (![[topLevelView window] showsResizeIndicator] ||
      !([[topLevelView window] styleMask] & NSResizableWindowMask))
    return PR_FALSE;

  if (aResizerRect) {
    NSSize bounds = [topLevelView bounds].size;
    NSPoint corner = NSMakePoint(bounds.width, [topLevelView isFlipped] ? bounds.height : 0);
    corner = [topLevelView convertPoint:corner toView:mView];
    aResizerRect->SetRect(NSToIntRound(corner.x) - resizeIndicatorWidth,
                          NSToIntRound(corner.y) - resizeIndicatorHeight,
                          resizeIndicatorWidth, resizeIndicatorHeight);
  }
  return PR_TRUE;
}

// In QuickDraw mode the coordinate system used here should be that of the
// browser window's content region (defined as everything but the 22-pixel
// high titlebar).  But in CoreGraphics mode the coordinate system should be
// that of the browser window as a whole (including its titlebar).  Both
// coordinate systems have a top-left origin.  See bmo bug 474491.
//
// There's a bug in this method's code -- it currently uses the QuickDraw
// coordinate system for both the QuickDraw and CoreGraphics drawing modes.
// This bug is fixed by the patch for bug 474491.  But the Flash plugin (both
// version 10.0.12.36 from Adobe and version 9.0 r151 from Apple) has Mozilla-
// specific code to work around this bug, which breaks when we fix it (see bmo
// bug 477077).  So we'll need to coordinate releasing a fix for this bug with
// Adobe and other major plugin vendors that support the CoreGraphics mode.
NS_IMETHODIMP nsChildView::GetPluginClipRect(nsIntRect& outClipRect, nsIntPoint& outOrigin, PRBool& outWidgetVisible)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NS_ASSERTION(mWindowType == eWindowType_plugin,
               "GetPluginClipRect must only be called on a plugin widget");
  if (mWindowType != eWindowType_plugin) return NS_ERROR_FAILURE;
  
  NSWindow* window = [mView window];
  if (!window) return NS_ERROR_FAILURE;
  
  NSPoint viewOrigin = [mView convertPoint:NSZeroPoint toView:nil];
  NSRect frame = [[window contentView] frame];
  viewOrigin.y = frame.size.height - viewOrigin.y;
  
  // set up the clipping region for plugins.
  NSRect visibleBounds = [mView visibleRect];
  NSPoint clipOrigin   = [mView convertPoint:visibleBounds.origin toView:nil];
  
  // Convert from cocoa to QuickDraw coordinates
  clipOrigin.y = frame.size.height - clipOrigin.y;
  
  outClipRect.x = NSToIntRound(clipOrigin.x);
  outClipRect.y = NSToIntRound(clipOrigin.y);

  // need to convert view's origin to window coordinates.
  // then, encode as "SetOrigin" ready values.
  outOrigin.x = -NSToIntRound(viewOrigin.x);
  outOrigin.y = -NSToIntRound(viewOrigin.y);

  PRBool isVisible;
  IsVisible(isVisible);
  if (isVisible && [mView window] != nil) {
    outClipRect.width  = NSToIntRound(visibleBounds.origin.x + visibleBounds.size.width) - NSToIntRound(visibleBounds.origin.x);
    outClipRect.height = NSToIntRound(visibleBounds.origin.y + visibleBounds.size.height) - NSToIntRound(visibleBounds.origin.y);

    if (mClipRects) {
      nsIntRect clipBounds;
      for (PRInt32 i = 0; i < mClipRectCount; ++i) {
        clipBounds.UnionRect(clipBounds, mClipRects[i]);
      }
      outClipRect.IntersectRect(outClipRect, clipBounds - outOrigin);
    }

    // XXXroc should this be !outClipRect.IsEmpty()?
    outWidgetVisible = PR_TRUE;
  }
  else {
    outClipRect.width = 0;
    outClipRect.height = 0;
    outWidgetVisible = PR_FALSE;
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsChildView::StartDrawPlugin()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NS_ASSERTION(mWindowType == eWindowType_plugin,
               "StartDrawPlugin must only be called on a plugin widget");
  if (mWindowType != eWindowType_plugin) return NS_ERROR_FAILURE;

  // This code is necessary for both Quickdraw and CoreGraphics in 32-bit builds.
  // See comments below about why. In 64-bit CoreGraphics mode we will not keep
  // this region up to date, plugins should not depend on it.
#ifndef __LP64__
  NSWindow* window = [mView window];
  if (!window)
    return NS_ERROR_FAILURE;

  // In QuickDraw drawing mode, prevent reentrant handling of any plugin event
  // (this emulates behavior on the 1.8 branch, where only QuickDraw mode is
  // supported).  But in CoreGraphics drawing mode only do this if the current
  // plugin event isn't an update/paint event.  This allows popupcontextmenu()
  // to work properly from a plugin that supports the Cocoa event model,
  // without regressing bug 409615.  See bug 435041.  (StartDrawPlugin() and
  // EndDrawPlugin() wrap every call to nsIPluginInstance::HandleEvent() --
  // not just calls that "draw" or paint.)
  if (!mPluginIsCG || (mView != [NSView focusView])) {
    if (mPluginDrawing)
      return NS_ERROR_FAILURE;
  }

  // It appears that the WindowRef from which we get the plugin port undergoes the
  // traditional BeginUpdate/EndUpdate cycle, which, if you recall, sets the visible
  // region to the intersection of the visible region and the update region. Since
  // we don't know here if we're being drawn inside a BeginUpdate/EndUpdate pair
  // (which seem to occur in [NSWindow display]), and we don't want to have the burden
  // of correctly doing Carbon invalidates of the plugin rect, we manually set the
  // visible region to be the entire port every time. It is necessary to set up our
  // window's port even for CoreGraphics plugins, because they may still use Carbon
  // internally (see bug #420527 for details).
  CGrafPtr port = ::GetWindowPort(WindowRef([window windowRef]));
  if (!mPluginIsCG)
    port = mPluginQDPort.port;

  RgnHandle pluginRegion = ::NewRgn();
  if (pluginRegion) {
    PRBool portChanged = (port != CGrafPtr(GetQDGlobalsThePort()));
    CGrafPtr oldPort;
    GDHandle oldDevice;

    if (portChanged) {
      ::GetGWorld(&oldPort, &oldDevice);
      ::SetGWorld(port, ::IsPortOffscreen(port) ? nsnull : ::GetMainDevice());
    }

    ::SetOrigin(0, 0);
    
    nsIntRect clipRect; // this is in native window coordinates
    nsIntPoint origin;
    PRBool visible;
    GetPluginClipRect(clipRect, origin, visible);
    
    // XXX if we're not visible, set an empty clip region?
    Rect pluginRect;
    ConvertGeckoRectToMacRect(clipRect, pluginRect);
    
    ::RectRgn(pluginRegion, &pluginRect);
    ::SetPortVisibleRegion(port, pluginRegion);
    ::SetPortClipRegion(port, pluginRegion);
    
    // now set up the origin for the plugin
    ::SetOrigin(origin.x, origin.y);
    
    ::DisposeRgn(pluginRegion);

    if (portChanged)
      ::SetGWorld(oldPort, oldDevice);
  }
#endif

  mPluginDrawing = PR_TRUE;
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsChildView::EndDrawPlugin()
{
  NS_ASSERTION(mWindowType == eWindowType_plugin,
               "EndDrawPlugin must only be called on a plugin widget");
  if (mWindowType != eWindowType_plugin) return NS_ERROR_FAILURE;

  mPluginDrawing = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsChildView::SetPluginInstanceOwner(nsIPluginInstanceOwner* aInstanceOwner)
{
  mPluginInstanceOwner = aInstanceOwner;

  return NS_OK;
}

NS_IMETHODIMP nsChildView::SetPluginEventModel(int inEventModel)
{
  [(ChildView*)mView setPluginEventModel:(NPEventModel)inEventModel];
  return NS_OK;
}

NS_IMETHODIMP nsChildView::GetPluginEventModel(int* outEventModel)
{
  *outEventModel = [(ChildView*)mView pluginEventModel];
  return NS_OK;
}

static NSString* ToNSString(const nsAString& aString)
{
  return [NSString stringWithCharacters:aString.BeginReading()
                                 length:aString.Length()];
}

struct KeyboardLayoutOverride {
  PRInt32 mKeyboardLayout;
  PRBool mOverrideEnabled;
};

static KeyboardLayoutOverride gOverrideKeyboardLayout;

static const PRUint32 sModifierFlagMap[][2] = {
  { nsIWidget::CAPS_LOCK, NSAlphaShiftKeyMask },
  { nsIWidget::SHIFT_L, NSShiftKeyMask },
  { nsIWidget::SHIFT_R, NSShiftKeyMask },
  { nsIWidget::CTRL_L, NSControlKeyMask },
  { nsIWidget::CTRL_R, NSControlKeyMask },
  { nsIWidget::ALT_L, NSAlternateKeyMask },
  { nsIWidget::ALT_R, NSAlternateKeyMask },
  { nsIWidget::COMMAND_L, NSCommandKeyMask },
  { nsIWidget::COMMAND_R, NSCommandKeyMask },
  { nsIWidget::NUMERIC_KEY_PAD, NSNumericPadKeyMask },
  { nsIWidget::HELP, NSHelpKeyMask },
  { nsIWidget::FUNCTION, NSFunctionKeyMask }
};
nsresult nsChildView::SynthesizeNativeKeyEvent(PRInt32 aNativeKeyboardLayout,
                                               PRInt32 aNativeKeyCode,
                                               PRUint32 aModifierFlags,
                                               const nsAString& aCharacters,
                                               const nsAString& aUnmodifiedCharacters)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;
  
  PRUint32 modifierFlags = 0;
  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(sModifierFlagMap); ++i) {
    if (aModifierFlags & sModifierFlagMap[i][0]) {
      modifierFlags |= sModifierFlagMap[i][1];
    }
  }
  int windowNumber = [[mView window] windowNumber];
  BOOL sendFlagsChangedEvent = NO;
  switch (aNativeKeyCode) {
    case kCapsLockKeyCode:
    case kRCommandKeyCode:
    case kCommandKeyCode:
    case kShiftKeyCode:
    case kOptionkeyCode:
    case kControlKeyCode:
    case kRShiftKeyCode:
    case kROptionKeyCode:
    case kRControlKeyCode:
      sendFlagsChangedEvent = YES;
  }
  NSEventType eventType = sendFlagsChangedEvent ? NSFlagsChanged : NSKeyDown;
  NSEvent* downEvent = [NSEvent keyEventWithType:eventType
                                        location:NSMakePoint(0,0)
                                   modifierFlags:modifierFlags
                                       timestamp:0
                                    windowNumber:windowNumber
                                         context:[NSGraphicsContext currentContext]
                                      characters:ToNSString(aCharacters)
                     charactersIgnoringModifiers:ToNSString(aUnmodifiedCharacters)
                                       isARepeat:NO
                                         keyCode:aNativeKeyCode];

  NSEvent* upEvent = sendFlagsChangedEvent ? nil :
                       [ChildView makeNewCocoaEventWithType:NSKeyUp
                                                  fromEvent:downEvent];

  if (downEvent && (sendFlagsChangedEvent || upEvent)) {
    KeyboardLayoutOverride currentLayout = gOverrideKeyboardLayout;
    gOverrideKeyboardLayout.mKeyboardLayout = aNativeKeyboardLayout;
    gOverrideKeyboardLayout.mOverrideEnabled = PR_TRUE;
    [NSApp sendEvent:downEvent];
    if (upEvent)
      [NSApp sendEvent:upEvent];
    // processKeyDownEvent and keyUp block exceptions so we're sure to
    // reach here to restore gOverrideKeyboardLayout
    gOverrideKeyboardLayout = currentLayout;
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult nsChildView::SynthesizeNativeMouseEvent(nsIntPoint aPoint,
                                                 PRUint32 aNativeMessage,
                                                 PRUint32 aModifierFlags)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // Move the mouse cursor to the requested position and reconnect it to the mouse.
  CGWarpMouseCursorPosition(CGPointMake(aPoint.x, aPoint.y));
  CGAssociateMouseAndMouseCursorPosition(true);

  // aPoint is given with the origin on the top left, but convertScreenToBase
  // expects a point in a coordinate system that has its origin on the bottom left.
  NSPoint screenPoint = NSMakePoint(aPoint.x, [[NSScreen mainScreen] frame].size.height - aPoint.y);
  NSPoint windowPoint = [[mView window] convertScreenToBase:screenPoint];

  NSEvent* event = [NSEvent mouseEventWithType:aNativeMessage
                                      location:windowPoint
                                 modifierFlags:aModifierFlags
                                     timestamp:[NSDate timeIntervalSinceReferenceDate]
                                  windowNumber:[[mView window] windowNumber]
                                       context:nil
                                   eventNumber:0
                                    clickCount:1
                                      pressure:0.0];

  if (!event)
    return NS_ERROR_FAILURE;

  [NSApp sendEvent:event];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

// First argument has to be an NSMenu representing the application's top-level
// menu bar. The returned item is *not* retained.
static NSMenuItem* NativeMenuItemWithLocation(NSMenu* menubar, NSString* locationString)
{
  NSArray* indexes = [locationString componentsSeparatedByString:@"|"];
  unsigned int indexCount = [indexes count];
  if (indexCount == 0)
    return nil;

  NSMenu* currentSubmenu = [NSApp mainMenu];
  for (unsigned int i = 0; i < indexCount; i++) {
    int targetIndex;
    // We remove the application menu from consideration for the top-level menu
    if (i == 0)
      targetIndex = [[indexes objectAtIndex:i] intValue] + 1;
    else
      targetIndex = [[indexes objectAtIndex:i] intValue];
    int itemCount = [currentSubmenu numberOfItems];
    if (targetIndex < itemCount) {
      NSMenuItem* menuItem = [currentSubmenu itemAtIndex:targetIndex];
      // if this is the last index just return the menu item
      if (i == (indexCount - 1))
        return menuItem;
      // if this is not the last index find the submenu and keep going
      if ([menuItem hasSubmenu])
        currentSubmenu = [menuItem submenu];
      else
        return nil;
    }
  }

  return nil;
}

// Used for testing native menu system structure and event handling.
NS_IMETHODIMP nsChildView::ActivateNativeMenuItemAt(const nsAString& indexString)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSString* locationString = [NSString stringWithCharacters:indexString.BeginReading() length:indexString.Length()];
  NSMenuItem* item = NativeMenuItemWithLocation([NSApp mainMenu], locationString);
  // We can't perform an action on an item with a submenu, that will raise
  // an obj-c exception.
  if (item && ![item hasSubmenu]) {
    NSMenu* parent = [item menu];
    if (parent) {
      // NSLog(@"Performing action for native menu item titled: %@\n",
      //       [[currentSubmenu itemAtIndex:targetIndex] title]);
      [parent performActionForItemAtIndex:[parent indexOfItem:item]];
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

// Used for testing native menu system structure and event handling.
NS_IMETHODIMP nsChildView::ForceUpdateNativeMenuAt(const nsAString& indexString)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsCocoaWindow *widget = GetXULWindowWidget();
  if (widget) {
    nsMenuBarX* mb = widget->GetMenuBar();
    if (mb) {
      if (indexString.IsEmpty())
        mb->ForceNativeMenuReload();
      else
        mb->ForceUpdateNativeMenuAt(indexString);
    }
  }
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

#pragma mark -

#ifdef INVALIDATE_DEBUGGING

static Boolean KeyDown(const UInt8 theKey)
{
  KeyMap map;
  GetKeys(map);
  return ((*((UInt8 *)map + (theKey >> 3)) >> (theKey & 7)) & 1) != 0;
}

static Boolean caps_lock()
{
  return KeyDown(0x39);
}

static void blinkRect(Rect* r)
{
  StRegionFromPool oldClip;
  if (oldClip != NULL)
    ::GetClip(oldClip);

  ::ClipRect(r);
  ::InvertRect(r);
  UInt32 end = ::TickCount() + 5;
  while (::TickCount() < end) ;
  ::InvertRect(r);

  if (oldClip != NULL)
    ::SetClip(oldClip);
}

static void blinkRgn(RgnHandle rgn)
{
  StRegionFromPool oldClip;
  if (oldClip != NULL)
    ::GetClip(oldClip);

  ::SetClip(rgn);
  ::InvertRgn(rgn);
  UInt32 end = ::TickCount() + 5;
  while (::TickCount() < end) ;
  ::InvertRgn(rgn);

  if (oldClip != NULL)
    ::SetClip(oldClip);
}

#endif

// Invalidate this component's visible area
NS_IMETHODIMP nsChildView::Invalidate(const nsIntRect &aRect, PRBool aIsSynchronous)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!mView || !mVisible)
    return NS_OK;

  NSRect r;
  GeckoRectToNSRect(aRect, r);
  
  if (aIsSynchronous) {
    [mView displayRect:r];
  }
  else if ([NSView focusView]) {
    // if a view is focussed (i.e. being drawn), then postpone the invalidate so that we
    // don't lose it.
    [mView setNeedsPendingDisplayInRect:r];
  }
  else {
    [mView setNeedsDisplayInRect:r];
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

inline PRUint16 COLOR8TOCOLOR16(PRUint8 color8)
{
  // return (color8 == 0xFF ? 0xFFFF : (color8 << 8));
  return (color8 << 8) | color8;  /* (color8 * 257) == (color8 * 0x0101) */
}

// The OS manages repaints well enough on its own, so we don't have to
// flush them out here.  In other words, the OS will automatically call
// displayIfNeeded at the appropriate times, so we don't need to do it
// ourselves.  See bmo bug 459319.
NS_IMETHODIMP nsChildView::Update()
{
  return NS_OK;
}

#pragma mark -

void nsChildView::ApplyConfiguration(nsIWidget* aExpectedParent,
                                     const nsIWidget::Configuration& aConfiguration,
                                     PRBool aRepaint)
{
#ifdef DEBUG
  nsWindowType kidType;
  aConfiguration.mChild->GetWindowType(kidType);
#endif
  NS_ASSERTION(kidType == eWindowType_plugin || kidType == eWindowType_child,
               "Configured widget is not a child or plugin type");
  NS_ASSERTION(aConfiguration.mChild->GetParent() == aExpectedParent,
               "Configured widget is not a child of the right widget");
  aConfiguration.mChild->Resize(
      aConfiguration.mBounds.x, aConfiguration.mBounds.y,
      aConfiguration.mBounds.width, aConfiguration.mBounds.height,
      aRepaint);
  // On Mac we don't use the clip region here, we just store it
  // in case GetPluginClipRect needs it.
  static_cast<nsChildView*>(aConfiguration.mChild)->
    StoreWindowClipRegion(aConfiguration.mClipRegion);
}

nsresult nsChildView::ConfigureChildren(const nsTArray<Configuration>& aConfigurations)
{
  for (PRUint32 i = 0; i < aConfigurations.Length(); ++i) {
    nsChildView::ApplyConfiguration(this, aConfigurations[i], PR_TRUE);
  }
  return NS_OK;
}  

static PRInt32
PickValueForSign(PRInt32 aSign, PRInt32 aLessThanZero, PRInt32 aZero,
                 PRInt32 aGreaterThanZero)
{
  if (aSign < 0) {
    return aLessThanZero;
  }
  if (aSign > 0) {
    return aGreaterThanZero;
  }
  return aZero;
}

void nsChildView::Scroll(const nsIntPoint& aDelta,
                         const nsTArray<nsIntRect>& aDestRects,
                         const nsTArray<Configuration>& aConfigurations)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_ASSERTION(mParentView, "Attempting to scroll a view that does not have a parent");
  if (!mParentView)
    return;

  BOOL viewWasDirty = mVisible && [mView needsDisplay];
  if (mVisible && !aDestRects.IsEmpty()) {
    // Union of all source and destination rects
    nsIntRegion destRegion;
    NSSize scrollVector = {aDelta.x, aDelta.y};
    for (PRUint32 i = 0; i < aDestRects.Length(); ++i) {
      NSRect rect;
      GeckoRectToNSRect(aDestRects[i] - aDelta, rect);
      [mView scrollRect:rect by:scrollVector];
      destRegion.Or(destRegion, aDestRects[i]);
    }
#ifdef NS_LEOPARD_AND_LATER
    if (viewWasDirty) {
      nsIntRect allRects = destRegion.GetBounds();
      allRects.UnionRect(allRects, allRects - aDelta);
      NSRect all;
      GeckoRectToNSRect(allRects, all);
      [mView translateRectsNeedingDisplayInRect:all by:scrollVector];

      // Areas that could be affected by the
      // translateRectsNeedingDisplayInRect but aren't in any destination
      // may have had their invalidation moved incorrectly. So just
      // invalidate them now. Unfortunately Apple hasn't given us an API
      // to do exactly what we need here.
      nsIntRegion needsInvalidation;
      needsInvalidation.Sub(allRects, destRegion);
      nsIntRegionRectIterator iter(needsInvalidation);
      const nsIntRect* invalidate;
      for (nsIntRegionRectIterator iter(needsInvalidation);
           (invalidate = iter.Next()) != nsnull;) {
        NSRect rect;
        GeckoRectToNSRect(*invalidate, rect);
        [mView setNeedsDisplayInRect:rect];
      }
    }
#endif // NS_LEOPARD_AND_LATER

    // Leopard, at least, has a nasty bug where calling scrollRect:by: doesn't
    // actually trigger a window update. A window update is only triggered
    // if you actually paint something. In some cases Gecko might optimize
    // scrolling in such a way that nothing actually gets repainted.
    // So let's invalidate one pixel. We'll pick a pixel on the trailing edge
    // of the last destination rectangle, since in most situations that's going
    // to be invalidated anyway.
    nsIntRect lastRect = aDestRects[aDestRects.Length() - 1] + aDelta;
    nsIntPoint pointToInvalidate(
      PickValueForSign(aDelta.x, lastRect.XMost(), lastRect.x, lastRect.x - 1),
      PickValueForSign(aDelta.y, lastRect.YMost(), lastRect.y, lastRect.y - 1));
    if (!nsIntRect(0,0,mBounds.width,mBounds.height).Contains(pointToInvalidate)) {
      pointToInvalidate = nsIntPoint(0, 0);
    }
    Invalidate(nsIntRect(pointToInvalidate, nsIntSize(1,1)), PR_FALSE);
  }

  // Don't force invalidation of the child if it's moving by the scroll
  // amount and not changing size
  for (PRUint32 i = 0; i < aConfigurations.Length(); ++i) {
    const Configuration& configuration = aConfigurations[i];
    nsIntRect oldBounds;
    configuration.mChild->GetBounds(oldBounds);
    ApplyConfiguration(this, aConfigurations[i],
                       oldBounds + aDelta != configuration.mBounds);
  }

  if (mOnDestroyCalled)
    return;

#ifndef NS_LEOPARD_AND_LATER
  if (viewWasDirty) {
    [mView setNeedsDisplay:YES];
  }
#endif // NS_LEOPARD_AND_LATER

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Invokes callback and ProcessEvent methods on Event Listener object
NS_IMETHODIMP nsChildView::DispatchEvent(nsGUIEvent* event, nsEventStatus& aStatus)
{
#ifdef DEBUG
  debug_DumpEvent(stdout, event->widget, event, nsCAutoString("something"), 0);
#endif

  NS_ASSERTION(!(IME_IsComposing() && NS_IS_KEY_EVENT(event)),
    "Any key events should not be fired during IME composing");

  aStatus = nsEventStatus_eIgnore;

  nsCOMPtr<nsIWidget> kungFuDeathGrip = do_QueryInterface(mParentWidget ? mParentWidget : this);
  if (mParentWidget) {
    nsWindowType type;
    mParentWidget->GetWindowType(type);
    if (type == eWindowType_popup) {
      // use the parent popup's widget if there is no view
      void* clientData = nsnull;
      if (event->widget)
        event->widget->GetClientData(clientData);
      if (!clientData)
        event->widget = mParentWidget;
    }
  }

  if (mEventCallback)
    aStatus = (*mEventCallback)(event);

  return NS_OK;
}

PRBool nsChildView::DispatchWindowEvent(nsGUIEvent &event)
{
  nsEventStatus status;
  DispatchEvent(&event, status);
  return ConvertStatus(status);
}

#pragma mark -

PRBool nsChildView::ReportDestroyEvent()
{
  nsGUIEvent event(PR_TRUE, NS_DESTROY, this);
  event.time = PR_IntervalNow();
  return DispatchWindowEvent(event);
}

PRBool nsChildView::ReportMoveEvent()
{
  nsGUIEvent moveEvent(PR_TRUE, NS_MOVE, this);
  moveEvent.refPoint.x = mBounds.x;
  moveEvent.refPoint.y = mBounds.y;
  moveEvent.time       = PR_IntervalNow();
  return DispatchWindowEvent(moveEvent);
}

PRBool nsChildView::ReportSizeEvent()
{
  nsSizeEvent sizeEvent(PR_TRUE, NS_SIZE, this);
  sizeEvent.time        = PR_IntervalNow();
  sizeEvent.windowSize  = &mBounds;
  sizeEvent.mWinWidth   = mBounds.width;
  sizeEvent.mWinHeight  = mBounds.height;
  return DispatchWindowEvent(sizeEvent);
}

#pragma mark -

//    Return the offset between this child view and the screen.
//    @return       -- widget origin in screen coordinates
nsIntPoint nsChildView::WidgetToScreenOffset()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NSPoint temp;
  temp.x = 0;
  temp.y = 0;
  
  // 1. First translate this point into window coords. The returned point is always in
  //    bottom-left coordinates.
  temp = [mView convertPoint:temp toView:nil];  
  
  // 2. We turn the window-coord rect's origin into screen (still bottom-left) coords.
  temp = [[mView window] convertBaseToScreen:temp];
  
  // 3. Since we're dealing in bottom-left coords, we need to make it top-left coords
  //    before we pass it back to Gecko.
  FlipCocoaScreenCoordinate(temp);
  
  return nsIntPoint(NSToIntRound(temp.x), NSToIntRound(temp.y));

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(nsIntPoint(0,0));
}

NS_IMETHODIMP nsChildView::CaptureRollupEvents(nsIRollupListener * aListener, 
                                               PRBool aDoCapture, 
                                               PRBool aConsumeRollupEvent)
{
  // this never gets called, only top-level windows can be rollup widgets
  return NS_OK;
}

NS_IMETHODIMP nsChildView::SetTitle(const nsAString& title)
{
  // child views don't have titles
  return NS_OK;
}

NS_IMETHODIMP nsChildView::GetAttention(PRInt32 aCycleCount)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  [NSApp requestUserAttention:NSInformationalRequest];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

/* static */
PRBool nsChildView::DoHasPendingInputEvent()
{
  return sLastInputEventCount != GetCurrentInputEventCount(); 
}

/* static */
PRUint32 nsChildView::GetCurrentInputEventCount()
{
  // Can't use kCGAnyInputEventType because that updates too rarely for us (and
  // always in increments of 30+!) and because apparently it's sort of broken
  // on Tiger.  So just go ahead and query the counters we care about.
  static const CGEventType eventTypes[] = {
    kCGEventLeftMouseDown,
    kCGEventLeftMouseUp,
    kCGEventRightMouseDown,
    kCGEventRightMouseUp,
    kCGEventMouseMoved,
    kCGEventLeftMouseDragged,
    kCGEventRightMouseDragged,
    kCGEventKeyDown,
    kCGEventKeyUp,
    kCGEventScrollWheel,
    kCGEventTabletPointer,
    kCGEventOtherMouseDown,
    kCGEventOtherMouseUp,
    kCGEventOtherMouseDragged
  };

  PRUint32 eventCount = 0;
  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(eventTypes); ++i) {
    eventCount +=
      CGEventSourceCounterForEventType(kCGEventSourceStateCombinedSessionState,
                                       eventTypes[i]);
  }
  return eventCount;
}

/* static */
void nsChildView::UpdateCurrentInputEventCount()
{
  sLastInputEventCount = GetCurrentInputEventCount();
}

PRBool nsChildView::HasPendingInputEvent()
{
  return DoHasPendingInputEvent();
}

#pragma mark -

// Force Input Method Editor to commit the uncommitted input
// Note that this and other IME methods don't necessarily
// get called on the same ChildView that input is going through.
NS_IMETHODIMP nsChildView::ResetInputState()
{
#ifdef DEBUG_IME
  NSLog(@"**** ResetInputState");
#endif

  IME_CommitComposition();
  return NS_OK;
}

// 'open' means that it can take non-ASCII chars
NS_IMETHODIMP nsChildView::SetIMEOpenState(PRBool aState)
{
#ifdef DEBUG_IME
  NSLog(@"**** SetIMEOpenState aState = %d", aState);
#endif

  IME_SetOpenState(aState);
  return NS_OK;
}

// 'open' means that it can take non-ASCII chars
NS_IMETHODIMP nsChildView::GetIMEOpenState(PRBool* aState)
{
#ifdef DEBUG_IME
  NSLog(@"**** GetIMEOpenState");
#endif

  *aState = IME_IsOpened();
  return NS_OK;
}

NS_IMETHODIMP nsChildView::SetIMEEnabled(PRUint32 aState)
{
#ifdef DEBUG_IME
  NSLog(@"**** SetIMEEnabled aState = %d", aState);
#endif

  switch (aState) {
    case nsIWidget::IME_STATUS_ENABLED:
    case nsIWidget::IME_STATUS_PLUGIN:
      IME_SetASCIICapableOnly(PR_FALSE);
      IME_Enable(PR_TRUE);
      break;
    case nsIWidget::IME_STATUS_DISABLED:
      IME_SetASCIICapableOnly(PR_FALSE);
      IME_Enable(PR_FALSE);
      break;
    case nsIWidget::IME_STATUS_PASSWORD:
      IME_SetASCIICapableOnly(PR_TRUE);
      IME_Enable(PR_FALSE);
      break;
    default:
      NS_ERROR("not implemented!");
  }
  return NS_OK;
}

NS_IMETHODIMP nsChildView::GetIMEEnabled(PRUint32* aState)
{
#ifdef DEBUG_IME
  NSLog(@"**** GetIMEEnabled");
#endif

  if (IME_IsEnabled())
    *aState = nsIWidget::IME_STATUS_ENABLED;
  else if (IME_IsASCIICapableOnly())
    *aState = nsIWidget::IME_STATUS_PASSWORD;
  else
    *aState = nsIWidget::IME_STATUS_DISABLED;
  return NS_OK;
}

// Destruct and don't commit the IME composition string.
NS_IMETHODIMP nsChildView::CancelIMEComposition()
{
#ifdef DEBUG_IME
  NSLog(@"**** CancelIMEComposition");
#endif

  IME_CancelComposition();
  return NS_OK;
}

NS_IMETHODIMP nsChildView::GetToggledKeyState(PRUint32 aKeyCode,
                                              PRBool* aLEDState)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

#ifdef DEBUG_IME
  NSLog(@"**** GetToggledKeyState");
#endif
  NS_ENSURE_ARG_POINTER(aLEDState);
  PRUint32 key;
  switch (aKeyCode) {
    case NS_VK_CAPS_LOCK:
      key = alphaLock;
      break;
    case NS_VK_NUM_LOCK:
      key = kEventKeyModifierNumLockMask;
      break;
    // Mac doesn't support SCROLL_LOCK state.
    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }
  PRUint32 modifierFlags = ::GetCurrentKeyModifiers();
  *aLEDState = (modifierFlags & key) != 0;
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsChildView::OnIMEFocusChange(PRBool aFocus)
{
  mTextInputHandler.OnFocusChangeInGecko(aFocus);
  // XXX Return NS_ERROR_NOT_IMPLEMENTED, see bug 496360.
  return NS_ERROR_NOT_IMPLEMENTED;
}

#ifdef NS_LEOPARD_AND_LATER

NSView<mozView>* nsChildView::GetEditorView()
{
  NSView<mozView>* editorView = mView;
  // We need to get editor's view. E.g., when the focus is in the bookmark
  // dialog, the view is <panel> element of the dialog.  At this time, the key
  // events are processed the parent window's view that has native focus.
  nsQueryContentEvent textContent(PR_TRUE, NS_QUERY_TEXT_CONTENT, this);
  textContent.InitForQueryTextContent(0, 0);
  DispatchWindowEvent(textContent);
  if (textContent.mSucceeded && textContent.mReply.mFocusedWidget) {
    NSView<mozView>* view = static_cast<NSView<mozView>*>(
      textContent.mReply.mFocusedWidget->GetNativeData(NS_NATIVE_WIDGET));
    if (view)
      editorView = view;
  }
  return editorView;
}

#endif // NS_LEOPARD_AND_LATER

#pragma mark -

gfxASurface*
nsChildView::GetThebesSurface()
{
  if (!mTempThebesSurface) {
    mTempThebesSurface = new gfxQuartzSurface(gfxSize(1, 1), gfxASurface::ImageFormatARGB32);
  }

  return mTempThebesSurface;
}

NS_IMETHODIMP
nsChildView::BeginSecureKeyboardInput()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsresult rv = nsBaseWidget::BeginSecureKeyboardInput();
  if (NS_SUCCEEDED(rv))
    ::EnableSecureEventInput();
  return rv;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsChildView::EndSecureKeyboardInput()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsresult rv = nsBaseWidget::EndSecureKeyboardInput();
  if (NS_SUCCEEDED(rv))
    ::DisableSecureEventInput();
  return rv;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

#ifdef ACCESSIBILITY
void
nsChildView::GetDocumentAccessible(nsIAccessible** aAccessible)
{
  *aAccessible = nsnull;
  
  nsCOMPtr<nsIAccessible> accessible = do_QueryReferent(mAccessible);
  if (!mAccessible) {
    // need to fetch the accessible anew, because it has gone away.
    nsEventStatus status;
    nsAccessibleEvent event(PR_TRUE, NS_GETACCESSIBLE, this);
    DispatchEvent(&event, status);
  
    // cache the accessible in our weak ptr
    mAccessible = do_GetWeakReference(event.accessible);
    
    // now try again
    accessible = do_QueryReferent(mAccessible);
  }
  
  NS_IF_ADDREF(*aAccessible = accessible.get());

  return;
}
#endif

#pragma mark -

@implementation ChildView

// globalDragPboard is non-null during native drag sessions that did not originate
// in our native NSView (it is set in |draggingEntered:|). It is unset when the
// drag session ends for this view, either with the mouse exiting or when a drop
// occurs in this view.
NSPasteboard* globalDragPboard = nil;

// gLastDragView and gLastDragMouseDownEvent are used to communicate information
// to the drag service during drag invocation (starting a drag in from the view).
// gLastDragView is only non-null while mouseDragged is on the call stack.
NSView* gLastDragView = nil;
NSEvent* gLastDragMouseDownEvent = nil;

+ (void)initialize
{
  static BOOL initialized = NO;

  if (!initialized) {
    // Inform the OS about the types of services (from the "Services" menu)
    // that we can handle.

    NSArray *sendTypes = [[NSArray alloc] initWithObjects:NSStringPboardType,NSHTMLPboardType,nil];
    NSArray *returnTypes = [[NSArray alloc] init];

    [NSApp registerServicesMenuSendTypes:sendTypes returnTypes:returnTypes];

    [sendTypes release];
    [returnTypes release];

    initialized = YES;
  }
}

// initWithFrame:geckoChild:
- (id)initWithFrame:(NSRect)inFrame geckoChild:(nsChildView*)inChild
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ((self = [super initWithFrame:inFrame])) {
    mGeckoChild = inChild;
    mIsPluginView = NO;
#ifndef NP_NO_CARBON
    mPluginEventModel = NPEventModelCarbon;
#else
    mPluginEventModel = NPEventModelCocoa;
#endif
    mCurKeyEvent = nil;
    mKeyDownHandled = PR_FALSE;
    mKeyPressHandled = NO;
    mKeyPressSent = NO;

    // initialization for NSTextInput
    mMarkedRange.location = NSNotFound;
    mMarkedRange.length = 0;

    mLastMouseDownEvent = nil;
    mDragService = nsnull;

    mPluginTSMDoc = nil;

    mGestureState = eGestureState_None;
    mCumulativeMagnification = 0.0;
    mCumulativeRotation = 0.0;

    [self setFocusRingType:NSFocusRingTypeNone];
  }
  
  // register for things we'll take from other applications
  PR_LOG(sCocoaLog, PR_LOG_ALWAYS, ("ChildView initWithFrame: registering drag types\n"));
  [self registerForDraggedTypes:[NSArray arrayWithObjects:NSFilenamesPboardType,
                                                          NSStringPboardType,
                                                          NSHTMLPboardType,
                                                          NSURLPboardType,
                                                          NSFilesPromisePboardType,
                                                          kWildcardPboardType,
                                                          kCorePboardType_url,
                                                          kCorePboardType_urld,
                                                          kCorePboardType_urln,
                                                          nil]];
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(systemMetricsChanged)
                                               name:NSControlTintDidChangeNotification
                                             object:nil];
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(systemMetricsChanged)
                                               name:NSSystemColorsDidChangeNotification
                                             object:nil];
  [[NSDistributedNotificationCenter defaultCenter] addObserver:self
                                                      selector:@selector(systemMetricsChanged)
                                                          name:@"AppleAquaScrollBarVariantChanged"
                                                        object:nil
                                            suspensionBehavior:NSNotificationSuspensionBehaviorDeliverImmediately]; 

  return self;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void)dealloc
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mPendingDirtyRects release];
  [mLastMouseDownEvent release];
  ChildViewMouseTracker::OnDestroyView(self);
#ifndef NP_NO_CARBON
  if (mPluginTSMDoc)
    ::DeleteTSMDocument(mPluginTSMDoc);
#endif

  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [[NSDistributedNotificationCenter defaultCenter] removeObserver:self];

  [super dealloc];    

#ifndef NP_NO_QUICKDRAW
  // This sets the current port to _savePort.
  // todo: Only do if a Quickdraw plugin is present in the hierarchy!
  ::SetPort(NULL);
#endif

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)widgetDestroyed
{
  mGeckoChild->IME_OnDestroyView(self);
  mGeckoChild = nsnull;

  // Just in case we're destroyed abruptly and missed the draggingExited
  // or performDragOperation message.
  NS_IF_RELEASE(mDragService);
}

// mozView method, return our gecko child view widget. Note this does not AddRef.
- (nsIWidget*) widget
{
  return static_cast<nsIWidget*>(mGeckoChild);
}

- (void)systemMetricsChanged
{
  if (!mGeckoChild)
    return;

  nsGUIEvent guiEvent(PR_TRUE, NS_THEMECHANGED, mGeckoChild);
  mGeckoChild->DispatchWindowEvent(guiEvent);
}

- (void)setNeedsPendingDisplay
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  mPendingFullDisplay = YES;
  [self performSelector:@selector(processPendingRedraws) withObject:nil afterDelay:0];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)setNeedsPendingDisplayInRect:(NSRect)invalidRect
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mPendingDirtyRects)
    mPendingDirtyRects = [[NSMutableArray alloc] initWithCapacity:1];
  [mPendingDirtyRects addObject:[NSValue valueWithRect:invalidRect]];
  [self performSelector:@selector(processPendingRedraws) withObject:nil afterDelay:0];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Clears the queue of any pending invalides
- (void)processPendingRedraws
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mPendingFullDisplay) {
    [self setNeedsDisplay:YES];
  }
  else {
    unsigned int count = [mPendingDirtyRects count];
    for (unsigned int i = 0; i < count; ++i) {
      [self setNeedsDisplayInRect:[[mPendingDirtyRects objectAtIndex:i] rectValue]];
    }
  }
  mPendingFullDisplay = NO;
  [mPendingDirtyRects release];
  mPendingDirtyRects = nil;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)setNeedsDisplayInRect:(NSRect)aRect
{
  [super setNeedsDisplayInRect:aRect];

  if ([[self window] isKindOfClass:[ToolbarWindow class]]) {
    ToolbarWindow* window = (ToolbarWindow*)[self window];
    if ([window drawsContentsIntoWindowFrame]) {
      // Tell it to mark the rect in the titlebar as dirty.
      NSView* borderView = [[window contentView] superview];
      [window setTitlebarNeedsDisplayInRect:[self convertRect:aRect toView:borderView]];
    }
  }
}

- (NSString*)description
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  return [NSString stringWithFormat:@"ChildView %p, gecko child %p, frame %@", self, mGeckoChild, NSStringFromRect([self frame])];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

// Make the origin of this view the topLeft corner (gecko origin) rather
// than the bottomLeft corner (standard cocoa origin).
- (BOOL)isFlipped
{
  return YES;
}

- (void)setTransparent:(BOOL)transparent
{
  mIsTransparent = transparent;
}

- (BOOL)isOpaque
{
  return !mIsTransparent;
}

-(void)setIsPluginView:(BOOL)aIsPlugin
{
  mIsPluginView = aIsPlugin;
}


-(BOOL)isPluginView
{
  return mIsPluginView;
}

- (void)setPluginEventModel:(NPEventModel)eventModel
{
  mPluginEventModel = eventModel;
}

- (NPEventModel)pluginEventModel;
{
  return mPluginEventModel;
}

- (void)sendFocusEvent:(PRUint32)eventType
{
  if (!mGeckoChild)
    return;

  nsEventStatus status = nsEventStatus_eIgnore;
  nsGUIEvent focusGuiEvent(PR_TRUE, eventType, mGeckoChild);
  focusGuiEvent.time = PR_IntervalNow();
  mGeckoChild->DispatchEvent(&focusGuiEvent, status);
}

// We accept key and mouse events, so don't keep passing them up the chain. Allow
// this to be a 'focussed' widget for event dispatch
- (BOOL)acceptsFirstResponder
{
  return YES;
}

- (void)viewWillMoveToWindow:(NSWindow *)newWindow
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!newWindow)
    HideChildPluginViews(self);

  [super viewWillMoveToWindow:newWindow];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)viewDidMoveToWindow
{
  if ([self window] && [self isPluginView] && mGeckoChild) {
    mGeckoChild->UpdatePluginPort();
  }

  [super viewDidMoveToWindow];
}

- (void)scrollRect:(NSRect)aRect by:(NSSize)offset
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Update any pending dirty rects to reflect the new scroll position
  if (mPendingDirtyRects) {
    unsigned int count = [mPendingDirtyRects count];
    for (unsigned int i = 0; i < count; ++i) {
      NSRect oldRect = [[mPendingDirtyRects objectAtIndex:i] rectValue];
      NSRect newRect = NSOffsetRect(oldRect, offset.width, offset.height);
      [mPendingDirtyRects replaceObjectAtIndex:i
                                    withObject:[NSValue valueWithRect:newRect]];
    }
  }
  [super scrollRect:aRect by:offset];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (BOOL)mouseDownCanMoveWindow
{
  return NO;
}

- (void)lockFocus
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

#ifndef NP_NO_QUICKDRAW
  // Set the current GrafPort to a "safe" port before calling [NSQuickDrawView lockFocus],
  // so that the NSQuickDrawView stashes a pointer to this known-good port internally.
  // It will set the port back to this port on destruction.
  ::SetPort(NULL);  // todo: only do if a Quickdraw plugin is present in the hierarchy!
#endif

  [super lockFocus];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Limit shadow invalidation to 10 times per second.
static const PRInt32 sShadowInvalidationInterval = 100;
- (void)maybeInvalidateShadow
{
  NSWindow* window = [self window];
  if (!window || [window isOpaque] || ![window hasShadow])
    return;

  PRIntervalTime now = PR_IntervalNow();
  PRInt32 elapsed = PR_IntervalToMilliseconds(now - mLastShadowInvalidation);
  if (!mLastShadowInvalidation ||
      elapsed >= sShadowInvalidationInterval) {
    [window invalidateShadow];
    mLastShadowInvalidation = now;
    mNeedsShadowInvalidation = NO;
  } else if (!mNeedsShadowInvalidation) {
    mNeedsShadowInvalidation = YES;
    [self performSelector:@selector(invalidateShadow)
               withObject:nil
               afterDelay:(sShadowInvalidationInterval - elapsed) / 1000.0];
  }
}

- (void)invalidateShadow
{
  if (![self window] || !mNeedsShadowInvalidation)
    return;
  [[self window] invalidateShadow];
  mNeedsShadowInvalidation = NO;
}

// The display system has told us that a portion of our view is dirty. Tell
// gecko to paint it
- (void)drawRect:(NSRect)aRect
{
  CGContextRef cgContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
  [self drawRect:aRect inContext:cgContext];

  // If we're a transparent window, and our contents have changed, we need
  // to make sure the shadow is updated to the new contents.
  [self maybeInvalidateShadow];
}

- (void)drawRect:(NSRect)aRect inContext:(CGContextRef)aContext
{
  PRBool isVisible;
  if (!mGeckoChild || NS_FAILED(mGeckoChild->IsVisible(isVisible)) ||
      !isVisible)
    return;

#ifdef DEBUG_UPDATE
  nsIntRect geckoBounds;
  mGeckoChild->GetBounds(geckoBounds);

  fprintf (stderr, "---- Update[%p][%p] [%f %f %f %f] cgc: %p\n  gecko bounds: [%d %d %d %d]\n",
           self, mGeckoChild,
           aRect.origin.x, aRect.origin.y, aRect.size.width, aRect.size.height, aContext,
           geckoBounds.x, geckoBounds.y, geckoBounds.width, geckoBounds.height);

  CGAffineTransform xform = CGContextGetCTM(aContext);
  fprintf (stderr, "  xform in: [%f %f %f %f %f %f]\n", xform.a, xform.b, xform.c, xform.d, xform.tx, xform.ty);
#endif

  // Create Cairo objects.
  NSSize bufferSize = [self bounds].size;
  nsRefPtr<gfxQuartzSurface> targetSurface =
    new gfxQuartzSurface(aContext, gfxSize(bufferSize.width, bufferSize.height));

  nsRefPtr<gfxContext> targetContext = new gfxContext(targetSurface);

  nsCOMPtr<nsIRenderingContext> rc;
  mGeckoChild->GetDeviceContext()->CreateRenderingContextInstance(*getter_AddRefs(rc));
  rc->Init(mGeckoChild->GetDeviceContext(), targetContext);

  // Build a region.
  nsCOMPtr<nsIRegion> rgn(do_CreateInstance(kRegionCID));
  if (!rgn)
    return;
  rgn->Init();

  const NSRect *rects;
  NSInteger count, i;
  [[NSView focusView] getRectsBeingDrawn:&rects count:&count];
  if (count < MAX_RECTS_IN_REGION) {
    for (i = 0; i < count; ++i) {
      // Add the rect to the region.
      const NSRect& r = [self convertRect:rects[i] fromView:[NSView focusView]];
      rgn->Union((PRInt32)r.origin.x, (PRInt32)r.origin.y, (PRInt32)r.size.width, (PRInt32)r.size.height);
    }
  } else {
    rgn->Union(aRect.origin.x, aRect.origin.y, aRect.size.width, aRect.size.height);
  }

  // Subtract child view rectangles from the region
  NSArray* subviews = [self subviews];
  for (int i = 0; i < int([subviews count]); ++i) {
    NSView* view = [subviews objectAtIndex:i];
    if (![view isKindOfClass:[ChildView class]] || [view isHidden])
      continue;
    NSRect frame = [view frame];
    rgn->Subtract(frame.origin.x, frame.origin.y, frame.size.width, frame.size.height);
  }

  // Set up the clip region.
  nsRegionRectSet* rgnRects = nsnull;
  rgn->GetRects(&rgnRects);
  if (!rgnRects)
    return;

  for (PRUint32 i = 0; i < rgnRects->mNumRects; ++i) {
    const nsRegionRect& rect = rgnRects->mRects[i];
    targetContext->Rectangle(gfxRect(rect.x, rect.y, rect.width, rect.height));
  }
  rgn->FreeRects(rgnRects);
  targetContext->Clip();

  // bounding box of the dirty area
  nsIntRect fullRect;
  NSRectToGeckoRect(aRect, fullRect);

  // Create the event and dispatch it.
  nsPaintEvent paintEvent(PR_TRUE, NS_PAINT, mGeckoChild);
  paintEvent.renderingContext = rc;
  paintEvent.rect = &fullRect;
  paintEvent.region = rgn;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  PRBool painted = mGeckoChild->DispatchWindowEvent(paintEvent);
  if (!painted && [self isOpaque]) {
    // Gecko refused to draw, but we've claimed to be opaque, so we have to
    // draw something--fill with white.
    CGContextSetRGBFillColor(aContext, 1, 1, 1, 1);
    CGContextFillRect(aContext, CGRectMake(aRect.origin.x, aRect.origin.y,
                                           aRect.size.width, aRect.size.height));
  }
  if (!mGeckoChild)
    return;

  paintEvent.renderingContext = nsnull;
  paintEvent.region = nsnull;

  targetContext = nsnull;
  targetSurface = nsnull;

  // note that the cairo surface *MUST* be destroyed at this point,
  // or bad things will happen (since we can't keep the cgContext around
  // beyond this drawRect message handler)

#ifdef DEBUG_UPDATE
  fprintf (stderr, "  window coords: [%d %d %d %d]\n", fullRect.x, fullRect.y, fullRect.width, fullRect.height);
  fprintf (stderr, "---- update done ----\n");

#if 0
  CGContextSetRGBStrokeColor (aContext,
                            ((((unsigned long)self) & 0xff)) / 255.0,
                            ((((unsigned long)self) & 0xff00) >> 8) / 255.0,
                            ((((unsigned long)self) & 0xff0000) >> 16) / 255.0,
                            0.5);
#endif 
  CGContextSetRGBStrokeColor(aContext, 1, 0, 0, 0.8);
  CGContextSetLineWidth(aContext, 4.0);
  CGContextStrokeRect(aContext,
                      CGRectMake(aRect.origin.x, aRect.origin.y, aRect.size.width, aRect.size.height));
#endif
}

// Allows us to turn off setting up the clip region
// before each drawRect. We already clip within gecko.
- (BOOL)wantsDefaultClipping
{
  return NO;
}

#if USE_CLICK_HOLD_CONTEXTMENU
//
// -clickHoldCallback:
//
// called from a timer two seconds after a mouse down to see if we should display
// a context menu (click-hold). |anEvent| is the original mouseDown event. If we're
// still in that mouseDown by this time, put up the context menu, otherwise just
// fuhgeddaboutit. |anEvent| has been retained by the OS until after this callback
// fires so we're ok there.
//
// This code currently messes in a bunch of edge cases (bugs 234751, 232964, 232314)
// so removing it until we get it straightened out.
//
- (void)clickHoldCallback:(id)theEvent;
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if( theEvent == [NSApp currentEvent] ) {
    // we're still in the middle of the same mousedown event here, activate
    // click-hold context menu by triggering the right mouseDown action.
    NSEvent* clickHoldEvent = [NSEvent mouseEventWithType:NSRightMouseDown
                                                  location:[theEvent locationInWindow]
                                             modifierFlags:nsCocoaUtils::GetCocoaEventModifierFlags(theEvent)
                                                 timestamp:[theEvent timestamp]
                                              windowNumber:[theEvent windowNumber]
                                                   context:[theEvent context]
                                               eventNumber:[theEvent eventNumber]
                                                clickCount:[theEvent clickCount]
                                                  pressure:[theEvent pressure]];
    [self rightMouseDown:clickHoldEvent];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}
#endif

// If we've just created a non-native context menu, we need to mark it as
// such and let the OS (and other programs) know when it opens and closes
// (this is how the OS knows to close other programs' context menus when
// ours open).  We send the initial notification here, but others are sent
// in nsCocoaWindow::Show().
- (void)maybeInitContextMenuTracking
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!gRollupWidget)
    return;

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    PRBool useNativeContextMenus;
    nsresult rv = prefs->GetBoolPref("ui.use_native_popup_windows", &useNativeContextMenus);
    if (NS_SUCCEEDED(rv) && useNativeContextMenus)
      return;
  }

  NSWindow *popupWindow = (NSWindow*)gRollupWidget->GetNativeData(NS_NATIVE_WINDOW);
  if (!popupWindow || ![popupWindow isKindOfClass:[PopupWindow class]])
    return;

  [[NSDistributedNotificationCenter defaultCenter]
    postNotificationName:@"com.apple.HIToolbox.beginMenuTrackingNotification"
                  object:@"org.mozilla.gecko.PopupWindow"];
  [(PopupWindow*)popupWindow setIsContextMenu:YES];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Returns true if the event should no longer be processed, false otherwise.
// This does not return whether or not anything was rolled up.
- (BOOL)maybeRollup:(NSEvent*)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  BOOL consumeEvent = NO;

  if (gRollupWidget && gRollupListener) {
    NSWindow* currentPopup = static_cast<NSWindow*>(gRollupWidget->GetNativeData(NS_NATIVE_WINDOW));
    if (!nsCocoaUtils::IsEventOverWindow(theEvent, currentPopup)) {
      // event is not over the rollup window, default is to roll up
      PRBool shouldRollup = PR_TRUE;

      // check to see if scroll events should roll up the popup
      if ([theEvent type] == NSScrollWheel) {
        gRollupListener->ShouldRollupOnMouseWheelEvent(&shouldRollup);
        // always consume scroll events that aren't over the popup
        consumeEvent = YES;
      }

      // if we're dealing with menus, we probably have submenus and
      // we don't want to rollup if the click is in a parent menu of
      // the current submenu
      PRUint32 popupsToRollup = PR_UINT32_MAX;
      nsCOMPtr<nsIMenuRollup> menuRollup;
      menuRollup = (do_QueryInterface(gRollupListener));
      if (menuRollup) {
        nsAutoTArray<nsIWidget*, 5> widgetChain;
        menuRollup->GetSubmenuWidgetChain(&widgetChain);
        PRUint32 sameTypeCount = menuRollup->GetSubmenuWidgetChain(&widgetChain);
        for (PRUint32 i = 0; i < widgetChain.Length(); i++) {
          nsIWidget* widget = widgetChain[i];
          NSWindow* currWindow = (NSWindow*)widget->GetNativeData(NS_NATIVE_WINDOW);
          if (nsCocoaUtils::IsEventOverWindow(theEvent, currWindow)) {
            // don't roll up if the mouse event occured within a menu of the
            // same type. If the mouse event occured in a menu higher than
            // that, roll up, but pass the number of popups to Rollup so
            // that only those of the same type close up.
            if (i < sameTypeCount) {
              shouldRollup = PR_FALSE;
            }
            else {
              popupsToRollup = sameTypeCount;
            }
            break;
          }
        }
      }

      if (shouldRollup) {
        gRollupListener->Rollup(popupsToRollup, nsnull);
        consumeEvent = (BOOL)gConsumeRollupEvent;
      }
    }
  }

  return consumeEvent;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

/*
 * XXX - The swipeWithEvent, beginGestureWithEvent, magnifyWithEvent,
 * rotateWithEvent, and endGestureWithEvent methods are part of a
 * PRIVATE interface exported by nsResponder and reverse-engineering
 * was necessary to obtain the methods' prototypes. Thus, Apple may
 * change the interface in the future without notice.
 *
 * The prototypes were obtained from the following link:
 * http://cocoadex.com/2008/02/nsevent-modifications-swipe-ro.html
 */

- (void)swipeWithEvent:(NSEvent *)anEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!anEvent || !mGeckoChild)
    return;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  float deltaX = [anEvent deltaX];  // left=1.0, right=-1.0
  float deltaY = [anEvent deltaY];  // up=1.0, down=-1.0

  // Setup the "swipe" event.
  nsSimpleGestureEvent geckoEvent(PR_TRUE, NS_SIMPLE_GESTURE_SWIPE, mGeckoChild, 0, 0.0);
  [self convertCocoaMouseEvent:anEvent toGeckoEvent:&geckoEvent];

  // Record the left/right direction.
  if (deltaX > 0.0)
    geckoEvent.direction |= nsIDOMSimpleGestureEvent::DIRECTION_LEFT;
  else if (deltaX < 0.0)
    geckoEvent.direction |= nsIDOMSimpleGestureEvent::DIRECTION_RIGHT;

  // Record the up/down direction.
  if (deltaY > 0.0)
    geckoEvent.direction |= nsIDOMSimpleGestureEvent::DIRECTION_UP;
  else if (deltaY < 0.0)
    geckoEvent.direction |= nsIDOMSimpleGestureEvent::DIRECTION_DOWN;

  // Send the event.
  mGeckoChild->DispatchWindowEvent(geckoEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)beginGestureWithEvent:(NSEvent *)anEvent
{
  NS_ASSERTION(mGestureState == eGestureState_None, "mGestureState should be eGestureState_None");

  if (!anEvent)
    return;

  mGestureState = eGestureState_StartGesture;
  mCumulativeMagnification = 0;
  mCumulativeRotation = 0.0;
}

- (void)magnifyWithEvent:(NSEvent *)anEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!anEvent || !mGeckoChild)
    return;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  float deltaZ = [anEvent deltaZ];

  PRUint32 msg;
  switch (mGestureState) {
  case eGestureState_StartGesture:
    msg = NS_SIMPLE_GESTURE_MAGNIFY_START;
    mGestureState = eGestureState_MagnifyGesture;
    break;

  case eGestureState_MagnifyGesture:
    msg = NS_SIMPLE_GESTURE_MAGNIFY_UPDATE;
    break;

  case eGestureState_None:
  case eGestureState_RotateGesture:
  default:
    return;
  }

  // Setup the event.
  nsSimpleGestureEvent geckoEvent(PR_TRUE, msg, mGeckoChild, 0, deltaZ);
  [self convertCocoaMouseEvent:anEvent toGeckoEvent:&geckoEvent];

  // Send the event.
  mGeckoChild->DispatchWindowEvent(geckoEvent);

  // Keep track of the cumulative magnification for the final "magnify" event.
  mCumulativeMagnification += deltaZ;
  
  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)rotateWithEvent:(NSEvent *)anEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!anEvent || !mGeckoChild)
    return;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  float rotation = [anEvent rotation];

  PRUint32 msg;
  switch (mGestureState) {
  case eGestureState_StartGesture:
    msg = NS_SIMPLE_GESTURE_ROTATE_START;
    mGestureState = eGestureState_RotateGesture;
    break;

  case eGestureState_RotateGesture:
    msg = NS_SIMPLE_GESTURE_ROTATE_UPDATE;
    break;

  case eGestureState_None:
  case eGestureState_MagnifyGesture:
  default:
    return;
  }

  // Setup the event.
  nsSimpleGestureEvent geckoEvent(PR_TRUE, msg, mGeckoChild, 0, 0.0);
  [self convertCocoaMouseEvent:anEvent toGeckoEvent:&geckoEvent];
  geckoEvent.delta = -rotation;
  if (rotation > 0.0) {
    geckoEvent.direction = nsIDOMSimpleGestureEvent::ROTATION_COUNTERCLOCKWISE;
  } else {
    geckoEvent.direction = nsIDOMSimpleGestureEvent::ROTATION_CLOCKWISE;
  }

  // Send the event.
  mGeckoChild->DispatchWindowEvent(geckoEvent);

  // Keep track of the cumulative rotation for the final "rotate" event.
  mCumulativeRotation += rotation;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)endGestureWithEvent:(NSEvent *)anEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!anEvent || !mGeckoChild) {
    // Clear the gestures state if we cannot send an event.
    mGestureState = eGestureState_None;
    mCumulativeMagnification = 0.0;
    mCumulativeRotation = 0.0;
    return;
  }

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  switch (mGestureState) {
  case eGestureState_MagnifyGesture:
    {
      // Setup the "magnify" event.
      nsSimpleGestureEvent geckoEvent(PR_TRUE, NS_SIMPLE_GESTURE_MAGNIFY,
                                      mGeckoChild, 0, mCumulativeMagnification);
      [self convertCocoaMouseEvent:anEvent toGeckoEvent:&geckoEvent];

      // Send the event.
      mGeckoChild->DispatchWindowEvent(geckoEvent);
    }
    break;

  case eGestureState_RotateGesture:
    {
      // Setup the "rotate" event.
      nsSimpleGestureEvent geckoEvent(PR_TRUE, NS_SIMPLE_GESTURE_ROTATE, mGeckoChild, 0, 0.0);
      [self convertCocoaMouseEvent:anEvent toGeckoEvent:&geckoEvent];
      geckoEvent.delta = -mCumulativeRotation;
      if (mCumulativeRotation > 0.0) {
        geckoEvent.direction = nsIDOMSimpleGestureEvent::ROTATION_COUNTERCLOCKWISE;
      } else {
        geckoEvent.direction = nsIDOMSimpleGestureEvent::ROTATION_CLOCKWISE;
      }

      // Send the event.
      mGeckoChild->DispatchWindowEvent(geckoEvent);
    }
    break;

  case eGestureState_None:
  case eGestureState_StartGesture:
  default:
    break;
  }

  // Clear the gestures state.
  mGestureState = eGestureState_None;
  mCumulativeMagnification = 0.0;
  mCumulativeRotation = 0.0;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)mouseDown:(NSEvent*)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // If we've already seen this event due to direct dispatch from menuForEvent:
  // just bail; if not, remember it.
  if (mLastMouseDownEvent == theEvent) {
    [mLastMouseDownEvent release];
    mLastMouseDownEvent = nil;
    return;
  }
  else {
    [mLastMouseDownEvent release];
    mLastMouseDownEvent = [theEvent retain];
  }

  [gLastDragMouseDownEvent release];
  gLastDragMouseDownEvent = [theEvent retain];

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  if ([self maybeRollup:theEvent] ||
      !ChildViewMouseTracker::WindowAcceptsEvent([self window], theEvent))
    return;

#if USE_CLICK_HOLD_CONTEXTMENU
  // fire off timer to check for click-hold after two seconds. retains |theEvent|
  [self performSelector:@selector(clickHoldCallback:) withObject:theEvent afterDelay:2.0];
#endif

  // in order to send gecko events we'll need a gecko widget
  if (!mGeckoChild)
    return;

  NSUInteger modifierFlags = nsCocoaUtils::GetCocoaEventModifierFlags(theEvent);

  nsMouseEvent geckoEvent(PR_TRUE, NS_MOUSE_BUTTON_DOWN, nsnull, nsMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.clickCount = [theEvent clickCount];
  if (modifierFlags & NSControlKeyMask)
    geckoEvent.button = nsMouseEvent::eRightButton;
  else
    geckoEvent.button = nsMouseEvent::eLeftButton;

  // Create event for use by plugins.
  // This is going to our child view so we don't need to look up the destination
  // event type.
#ifndef NP_NO_CARBON
  EventRecord carbonEvent;
  if (mPluginEventModel == NPEventModelCarbon) {
    carbonEvent.what = mouseDown;
    carbonEvent.message = 0;
    carbonEvent.when = ::TickCount();
    ::GetGlobalMouse(&carbonEvent.where);
    carbonEvent.modifiers = ::GetCurrentKeyModifiers();
    geckoEvent.pluginEvent = &carbonEvent;
  }
#endif
  NPCocoaEvent cocoaEvent;
  if (mPluginEventModel == NPEventModelCocoa) {
    InitNPCocoaEvent(&cocoaEvent);
    NSPoint point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    cocoaEvent.type = NPCocoaEventMouseDown;
    cocoaEvent.data.mouse.modifierFlags = modifierFlags;
    cocoaEvent.data.mouse.pluginX = point.x;
    cocoaEvent.data.mouse.pluginY = point.y;
    cocoaEvent.data.mouse.buttonNumber = [theEvent buttonNumber];
    cocoaEvent.data.mouse.clickCount = [theEvent clickCount];
    cocoaEvent.data.mouse.deltaX = [theEvent deltaX];
    cocoaEvent.data.mouse.deltaY = [theEvent deltaY];
    cocoaEvent.data.mouse.deltaZ = [theEvent deltaZ];
    geckoEvent.pluginEvent = &cocoaEvent;
  }

  mGeckoChild->DispatchWindowEvent(geckoEvent);

  // XXX maybe call markedTextSelectionChanged:client: here?

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)mouseUp:(NSEvent *)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;


  if (!mGeckoChild)
    return;

  nsMouseEvent geckoEvent(PR_TRUE, NS_MOUSE_BUTTON_UP, nsnull, nsMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  if (nsCocoaUtils::GetCocoaEventModifierFlags(theEvent) & NSControlKeyMask)
    geckoEvent.button = nsMouseEvent::eRightButton;
  else
    geckoEvent.button = nsMouseEvent::eLeftButton;

  // Create event for use by plugins.
  // This is going to our child view so we don't need to look up the destination
  // event type.
#ifndef NP_NO_CARBON
  EventRecord carbonEvent;
  if (mPluginEventModel == NPEventModelCarbon) {
    carbonEvent.what = mouseUp;
    carbonEvent.message = 0;
    carbonEvent.when = ::TickCount();
    ::GetGlobalMouse(&carbonEvent.where);
    carbonEvent.modifiers = ::GetCurrentKeyModifiers();
    geckoEvent.pluginEvent = &carbonEvent;
  }
#endif
  NPCocoaEvent cocoaEvent;
  if (mPluginEventModel == NPEventModelCocoa) {
    InitNPCocoaEvent(&cocoaEvent);
    NSPoint point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    cocoaEvent.type = NPCocoaEventMouseUp;
    cocoaEvent.data.mouse.modifierFlags = [theEvent modifierFlags];
    cocoaEvent.data.mouse.pluginX = point.x;
    cocoaEvent.data.mouse.pluginY = point.y;
    cocoaEvent.data.mouse.buttonNumber = [theEvent buttonNumber];
    cocoaEvent.data.mouse.clickCount = [theEvent clickCount];
    cocoaEvent.data.mouse.deltaX = [theEvent deltaX];
    cocoaEvent.data.mouse.deltaY = [theEvent deltaY];
    cocoaEvent.data.mouse.deltaZ = [theEvent deltaZ];
    geckoEvent.pluginEvent = &cocoaEvent;
  }

  mGeckoChild->DispatchWindowEvent(geckoEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)sendMouseEnterOrExitEvent:(NSEvent*)aEvent
                            enter:(BOOL)aEnter
                             type:(nsMouseEvent::exitType)aType
{
  if (!mGeckoChild)
    return;

  NSPoint windowEventLocation = nsCocoaUtils::EventLocationForWindow(aEvent, [self window]);
  NSPoint localEventLocation = [self convertPoint:windowEventLocation fromView:nil];

  PRUint32 msg = aEnter ? NS_MOUSE_ENTER : NS_MOUSE_EXIT;
  nsMouseEvent event(PR_TRUE, msg, mGeckoChild, nsMouseEvent::eReal);
  event.refPoint.x = nscoord((PRInt32)localEventLocation.x);
  event.refPoint.y = nscoord((PRInt32)localEventLocation.y);

  // Create event for use by plugins.
  // This is going to our child view so we don't need to look up the destination
  // event type.
  EventRecord carbonEvent;
  NPCocoaEvent cocoaEvent;
  if (mIsPluginView) {
#ifndef NP_NO_CARBON  
    if (mPluginEventModel == NPEventModelCarbon) {
      carbonEvent.what = NPEventType_AdjustCursorEvent;
      carbonEvent.message = 0;
      carbonEvent.when = ::TickCount();
      ::GetGlobalMouse(&carbonEvent.where);
      carbonEvent.modifiers = ::GetCurrentKeyModifiers();
      event.pluginEvent = &carbonEvent;
    }
#endif
    if (mPluginEventModel == NPEventModelCocoa) {
      InitNPCocoaEvent(&cocoaEvent);
      cocoaEvent.type = ((msg == NS_MOUSE_ENTER) ? NPCocoaEventMouseEntered : NPCocoaEventMouseExited);
      cocoaEvent.data.mouse.modifierFlags = [aEvent modifierFlags];
      cocoaEvent.data.mouse.pluginX = 5;
      cocoaEvent.data.mouse.pluginY = 5;
      cocoaEvent.data.mouse.buttonNumber = [aEvent buttonNumber];
      cocoaEvent.data.mouse.deltaX = [aEvent deltaX];
      cocoaEvent.data.mouse.deltaY = [aEvent deltaY];
      cocoaEvent.data.mouse.deltaZ = [aEvent deltaZ];
      event.pluginEvent = &cocoaEvent;
    }    
  }

  event.exit = aType;

  nsEventStatus status; // ignored
  mGeckoChild->DispatchEvent(&event, status);
}

- (void)mouseMoved:(NSEvent*)aEvent
{
  ChildViewMouseTracker::MouseMoved(aEvent);
}

- (void)handleMouseMoved:(NSEvent*)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mGeckoChild)
    return;

  nsMouseEvent geckoEvent(PR_TRUE, NS_MOUSE_MOVE, nsnull, nsMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];

  // Create event for use by plugins.
  // This is going to our child view so we don't need to look up the destination
  // event type.
  EventRecord carbonEvent;
  NPCocoaEvent cocoaEvent;
  if (mIsPluginView) {
#ifndef NP_NO_CARBON
    if (mPluginEventModel == NPEventModelCarbon) {
      carbonEvent.what = NPEventType_AdjustCursorEvent;
      carbonEvent.message = 0;
      carbonEvent.when = ::TickCount();
      ::GetGlobalMouse(&carbonEvent.where);
      carbonEvent.modifiers = ::GetCurrentKeyModifiers();
      geckoEvent.pluginEvent = &carbonEvent;
    }
#endif
    if (mPluginEventModel == NPEventModelCocoa) {
      InitNPCocoaEvent(&cocoaEvent);
      NSPoint point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
      cocoaEvent.type = NPCocoaEventMouseMoved;
      cocoaEvent.data.mouse.modifierFlags = [theEvent modifierFlags];
      cocoaEvent.data.mouse.pluginX = point.x;
      cocoaEvent.data.mouse.pluginY = point.y;
      cocoaEvent.data.mouse.buttonNumber = [theEvent buttonNumber];
      cocoaEvent.data.mouse.clickCount = [theEvent clickCount];
      cocoaEvent.data.mouse.deltaX = [theEvent deltaX];
      cocoaEvent.data.mouse.deltaY = [theEvent deltaY];
      cocoaEvent.data.mouse.deltaZ = [theEvent deltaZ];
      geckoEvent.pluginEvent = &cocoaEvent;
    }
  }
  mGeckoChild->DispatchWindowEvent(geckoEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)mouseDragged:(NSEvent*)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mGeckoChild)
    return;

  gLastDragView = self;

  nsMouseEvent geckoEvent(PR_TRUE, NS_MOUSE_MOVE, nsnull, nsMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];

  // create event for use by plugins
#ifndef NP_NO_CARBON
  EventRecord carbonEvent;
  if (mPluginEventModel == NPEventModelCarbon) {
    carbonEvent.what = NPEventType_AdjustCursorEvent;
    carbonEvent.message = 0;
    carbonEvent.when = ::TickCount();
    ::GetGlobalMouse(&carbonEvent.where);
    carbonEvent.modifiers = btnState | ::GetCurrentKeyModifiers();
    geckoEvent.pluginEvent = &carbonEvent;
  }
#endif
  NPCocoaEvent cocoaEvent;
  if (mPluginEventModel == NPEventModelCocoa) {
    InitNPCocoaEvent(&cocoaEvent);
    NSPoint point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    cocoaEvent.type = NPCocoaEventMouseDragged;
    cocoaEvent.data.mouse.modifierFlags = [theEvent modifierFlags];
    cocoaEvent.data.mouse.pluginX = point.x;
    cocoaEvent.data.mouse.pluginY = point.y;
    cocoaEvent.data.mouse.buttonNumber = [theEvent buttonNumber];
    cocoaEvent.data.mouse.clickCount = [theEvent clickCount];
    cocoaEvent.data.mouse.deltaX = [theEvent deltaX];
    cocoaEvent.data.mouse.deltaY = [theEvent deltaY];
    cocoaEvent.data.mouse.deltaZ = [theEvent deltaZ];
    geckoEvent.pluginEvent = &cocoaEvent;
  }

  mGeckoChild->DispatchWindowEvent(geckoEvent);

  // Note, sending the above event might have destroyed our widget since we didn't retain.
  // Fine so long as we don't access any local variables from here on.
  gLastDragView = nil;

  // XXX maybe call markedTextSelectionChanged:client: here?

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)rightMouseDown:(NSEvent *)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  [self maybeRollup:theEvent];
  if (!mGeckoChild)
    return;

  // The right mouse went down, fire off a right mouse down event to gecko
  nsMouseEvent geckoEvent(PR_TRUE, NS_MOUSE_BUTTON_DOWN, nsnull, nsMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.button = nsMouseEvent::eRightButton;
  geckoEvent.clickCount = [theEvent clickCount];

  // create event for use by plugins
#ifndef NP_NO_CARBON
  EventRecord carbonEvent;
  if (mPluginEventModel == NPEventModelCarbon) {
    carbonEvent.what = mouseDown;
    carbonEvent.message = 0;
    carbonEvent.when = ::TickCount();
    ::GetGlobalMouse(&carbonEvent.where);
    carbonEvent.modifiers = controlKey;  // fake a context menu click
    geckoEvent.pluginEvent = &carbonEvent;    
  }
#endif
  NPCocoaEvent cocoaEvent;
  if (mPluginEventModel == NPEventModelCocoa) {
    InitNPCocoaEvent(&cocoaEvent);
    NSPoint point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    cocoaEvent.type = NPCocoaEventMouseDown;
    cocoaEvent.data.mouse.modifierFlags = [theEvent modifierFlags];
    cocoaEvent.data.mouse.pluginX = point.x;
    cocoaEvent.data.mouse.pluginY = point.y;
    cocoaEvent.data.mouse.buttonNumber = [theEvent buttonNumber];
    cocoaEvent.data.mouse.clickCount = [theEvent clickCount];
    cocoaEvent.data.mouse.deltaX = [theEvent deltaX];
    cocoaEvent.data.mouse.deltaY = [theEvent deltaY];
    cocoaEvent.data.mouse.deltaZ = [theEvent deltaZ];
    geckoEvent.pluginEvent = &cocoaEvent;
  }

  PRBool handled = mGeckoChild->DispatchWindowEvent(geckoEvent);
  if (!mGeckoChild)
    return;

  if (!handled)
    [super rightMouseDown:theEvent]; // let the superview do context menu stuff

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)rightMouseUp:(NSEvent *)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mGeckoChild)
    return;

  nsMouseEvent geckoEvent(PR_TRUE, NS_MOUSE_BUTTON_UP, nsnull, nsMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.button = nsMouseEvent::eRightButton;
  geckoEvent.clickCount = [theEvent clickCount];

  // create event for use by plugins
#ifndef NP_NO_CARBON
  EventRecord carbonEvent;
  if (mPluginEventModel == NPEventModelCarbon) {
    carbonEvent.what = mouseUp;
    carbonEvent.message = 0;
    carbonEvent.when = ::TickCount();
    ::GetGlobalMouse(&carbonEvent.where);
    carbonEvent.modifiers = controlKey;  // fake a context menu click
    geckoEvent.pluginEvent = &carbonEvent;
  }
#endif
  NPCocoaEvent cocoaEvent;
  if (mPluginEventModel == NPEventModelCocoa) {
    InitNPCocoaEvent(&cocoaEvent);
    NSPoint point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    cocoaEvent.type = NPCocoaEventMouseUp;
    cocoaEvent.data.mouse.modifierFlags = [theEvent modifierFlags];
    cocoaEvent.data.mouse.pluginX = point.x;
    cocoaEvent.data.mouse.pluginY = point.y;
    cocoaEvent.data.mouse.buttonNumber = [theEvent buttonNumber];
    cocoaEvent.data.mouse.clickCount = [theEvent clickCount];
    cocoaEvent.data.mouse.deltaX = [theEvent deltaX];
    cocoaEvent.data.mouse.deltaY = [theEvent deltaY];
    cocoaEvent.data.mouse.deltaZ = [theEvent deltaZ];
    geckoEvent.pluginEvent = &cocoaEvent;
  }

  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  mGeckoChild->DispatchWindowEvent(geckoEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)rightMouseDragged:(NSEvent*)theEvent
{
  if (!mGeckoChild)
    return;

  nsMouseEvent geckoEvent(PR_TRUE, NS_MOUSE_MOVE, nsnull, nsMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.button = nsMouseEvent::eRightButton;

  // send event into Gecko by going directly to the
  // the widget.
  mGeckoChild->DispatchWindowEvent(geckoEvent);
}

- (void)otherMouseDown:(NSEvent *)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  if ([self maybeRollup:theEvent] ||
      !ChildViewMouseTracker::WindowAcceptsEvent([self window], theEvent))
    return;

  if (!mGeckoChild)
    return;

  nsMouseEvent geckoEvent(PR_TRUE, NS_MOUSE_BUTTON_DOWN, nsnull, nsMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.button = nsMouseEvent::eMiddleButton;
  geckoEvent.clickCount = [theEvent clickCount];

  mGeckoChild->DispatchWindowEvent(geckoEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)otherMouseUp:(NSEvent *)theEvent
{
  if (!mGeckoChild)
    return;

  nsMouseEvent geckoEvent(PR_TRUE, NS_MOUSE_BUTTON_UP, nsnull, nsMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.button = nsMouseEvent::eMiddleButton;

  mGeckoChild->DispatchWindowEvent(geckoEvent);
}

- (void)otherMouseDragged:(NSEvent*)theEvent
{
  if (!mGeckoChild)
    return;

  nsMouseEvent geckoEvent(PR_TRUE, NS_MOUSE_MOVE, nsnull, nsMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.button = nsMouseEvent::eMiddleButton;

  // send event into Gecko by going directly to the
  // the widget.
  mGeckoChild->DispatchWindowEvent(geckoEvent);
}

// Handle an NSScrollWheel event for a single axis only.
-(void)scrollWheel:(NSEvent*)theEvent forAxis:(enum nsMouseScrollEvent::nsMouseScrollFlags)inAxis
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mGeckoChild)
    return;

  float scrollDelta = 0;
  float scrollDeltaPixels = 0;
  PRBool checkPixels = PR_TRUE;

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs)
    prefs->GetBoolPref("mousewheel.enable_pixel_scrolling", &checkPixels);

  // Calling deviceDeltaX or deviceDeltaY on theEvent will trigger a Cocoa
  // assertion and an Objective-C NSInternalInconsistencyException if the
  // underlying "Carbon" event doesn't contain pixel scrolling information.
  // For these events, carbonEventKind is kEventMouseWheelMoved instead of
  // kEventMouseScroll.
  if (checkPixels) {
    EventRef theCarbonEvent = [theEvent _eventRef];
    UInt32 carbonEventKind = theCarbonEvent ? ::GetEventKind(theCarbonEvent) : 0;
    if (carbonEventKind != mozkEventMouseScroll)
      checkPixels = PR_FALSE;
  }

  // Some scrolling devices supports pixel scrolling, e.g. a Macbook
  // touchpad or a Mighty Mouse. On those devices, [event deviceDeltaX/Y]
  // contains the amount of pixels to scroll. 
  if (inAxis & nsMouseScrollEvent::kIsVertical) {
    scrollDelta       = -[theEvent deltaY];
    if (checkPixels && (scrollDelta == 0 || scrollDelta != floor(scrollDelta))) {
      scrollDeltaPixels = -[theEvent deviceDeltaY];
    }
  } else if (inAxis & nsMouseScrollEvent::kIsHorizontal) {
    scrollDelta       = -[theEvent deltaX];
    if (checkPixels && (scrollDelta == 0 || scrollDelta != floor(scrollDelta))) {
      scrollDeltaPixels = -[theEvent deviceDeltaX];
    }
  } else {
    return; // caller screwed up
  }

  BOOL hasPixels = (scrollDeltaPixels != 0);

  if (!hasPixels && scrollDelta == 0)
    // No sense in firing off a Gecko event.
     return;

  if (scrollDelta != 0) {
    // Send the line scroll event.
    nsMouseScrollEvent geckoEvent(PR_TRUE, NS_MOUSE_SCROLL, nsnull);
    [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
    geckoEvent.scrollFlags |= inAxis;

    if (hasPixels)
      geckoEvent.scrollFlags |= nsMouseScrollEvent::kHasPixels;

    // Gecko only understands how to scroll by an integer value. Using floor
    // and ceil is better than truncating the fraction, especially when
    // |delta| < 1.
    if (scrollDelta < 0)
      geckoEvent.delta = (PRInt32)floorf(scrollDelta);
    else
      geckoEvent.delta = (PRInt32)ceilf(scrollDelta);

    NPCocoaEvent cocoaEvent;
    if (mPluginEventModel == NPEventModelCocoa) {
      InitNPCocoaEvent(&cocoaEvent);
      NSPoint point = [self convertPoint:[theEvent locationInWindow] fromView:nil];
      cocoaEvent.type = NPCocoaEventScrollWheel;
      cocoaEvent.data.mouse.modifierFlags = [theEvent modifierFlags];
      cocoaEvent.data.mouse.pluginX = point.x;
      cocoaEvent.data.mouse.pluginY = point.y;
      cocoaEvent.data.mouse.buttonNumber = [theEvent buttonNumber];
      cocoaEvent.data.mouse.clickCount = 0;
      if (inAxis & nsMouseScrollEvent::kIsHorizontal)
        cocoaEvent.data.mouse.deltaX = [theEvent deltaX];
      else
        cocoaEvent.data.mouse.deltaX = 0.0;
      if (inAxis & nsMouseScrollEvent::kIsVertical)
        cocoaEvent.data.mouse.deltaY = [theEvent deltaY];
      else
        cocoaEvent.data.mouse.deltaY = 0.0;
      cocoaEvent.data.mouse.deltaZ = 0.0;
      geckoEvent.pluginEvent = &cocoaEvent;
    }

    nsAutoRetainCocoaObject kungFuDeathGrip(self);
    mGeckoChild->DispatchWindowEvent(geckoEvent);
    if (!mGeckoChild)
      return;

#ifndef NP_NO_CARBON
    // dispatch scroll wheel carbon event for plugins
    if (mPluginEventModel == NPEventModelCarbon) {
      EventRef theEvent;
      OSStatus err = ::CreateEvent(NULL,
                                   kEventClassMouse,
                                   kEventMouseWheelMoved,
                                   TicksToEventTime(TickCount()),
                                   kEventAttributeUserEvent,
                                   &theEvent);
      if (err == noErr) {
        EventMouseWheelAxis axis;
        if (inAxis & nsMouseScrollEvent::kIsVertical)
          axis = kEventMouseWheelAxisY;
        else if (inAxis & nsMouseScrollEvent::kIsHorizontal)
          axis = kEventMouseWheelAxisX;
        
        SetEventParameter(theEvent,
                          kEventParamMouseWheelAxis,
                          typeMouseWheelAxis,
                          sizeof(EventMouseWheelAxis),
                          &axis);
        
        SInt32 delta = (SInt32)-geckoEvent.delta;
        SetEventParameter(theEvent,
                          kEventParamMouseWheelDelta,
                          typeLongInteger,
                          sizeof(SInt32),
                          &delta);
        
        Point mouseLoc;
        ::GetGlobalMouse(&mouseLoc);
        SetEventParameter(theEvent,
                          kEventParamMouseLocation,
                          typeQDPoint,
                          sizeof(Point),
                          &mouseLoc);
        
        ::SendEventToEventTarget(theEvent, GetWindowEventTarget((WindowRef)[[self window] windowRef]));
        ReleaseEvent(theEvent);
      }
    }
#endif
  }

  if (hasPixels) {
    // Send the pixel scroll event.
    nsMouseScrollEvent geckoEvent(PR_TRUE, NS_MOUSE_PIXEL_SCROLL, nsnull);
    [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
    geckoEvent.scrollFlags |= inAxis;
    geckoEvent.delta = NSToIntRound(scrollDeltaPixels);
    nsAutoRetainCocoaObject kungFuDeathGrip(self);
    mGeckoChild->DispatchWindowEvent(geckoEvent);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

-(void)scrollWheel:(NSEvent*)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  if ([self maybeRollup:theEvent])
    return;

  if (!mGeckoChild)
    return;

  // It's possible for a single NSScrollWheel event to carry both useful
  // deltaX and deltaY, for example, when the "wheel" is a trackpad.
  // NSMouseScrollEvent can only carry one axis at a time, so the system
  // event will be split into two Gecko events if necessary.
  [self scrollWheel:theEvent forAxis:nsMouseScrollEvent::kIsVertical];
  if (!mGeckoChild)
    return;
  [self scrollWheel:theEvent forAxis:nsMouseScrollEvent::kIsHorizontal];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

-(NSMenu*)menuForEvent:(NSEvent*)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if (!mGeckoChild || [self isPluginView])
    return nil;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  [self maybeRollup:theEvent];
  if (!mGeckoChild)
    return nil;

  // Cocoa doesn't always dispatch a mouseDown: for a control-click event,
  // depends on what we return from menuForEvent:. Gecko always expects one
  // and expects the mouse down event before the context menu event, so
  // get that event sent first if this is a left mouse click.
  if ([theEvent type] == NSLeftMouseDown) {
    [self mouseDown:theEvent];
    if (!mGeckoChild)
      return nil;
  }

  nsMouseEvent geckoEvent(PR_TRUE, NS_CONTEXTMENU, nsnull, nsMouseEvent::eReal);
  [self convertCocoaMouseEvent:theEvent toGeckoEvent:&geckoEvent];
  geckoEvent.button = nsMouseEvent::eRightButton;
  mGeckoChild->DispatchWindowEvent(geckoEvent);
  if (!mGeckoChild)
    return nil;

  [self maybeInitContextMenuTracking];

  // Go up our view chain to fetch the correct menu to return.
  return [self contextMenu];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSMenu*)contextMenu
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  NSView* superView = [self superview];
  if ([superView respondsToSelector:@selector(contextMenu)])
    return [(NSView<mozView>*)superView contextMenu];

  return nil;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

#ifndef NP_NO_CARBON
static PRBool ConvertUnicodeToCharCode(PRUnichar inUniChar, unsigned char* outChar)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  UnicodeToTextInfo converterInfo;
  TextEncoding      systemEncoding;
  Str255            convertedString;
  OSStatus          err;
  
  *outChar = 0;
  
  err = ::UpgradeScriptInfoToTextEncoding(smSystemScript, kTextLanguageDontCare, kTextRegionDontCare, NULL, &systemEncoding);
  if (err != noErr)
    return PR_FALSE;
  
  err = ::CreateUnicodeToTextInfoByEncoding(systemEncoding, &converterInfo);
  if (err != noErr)
    return PR_FALSE;
  
  err = ::ConvertFromUnicodeToPString(converterInfo, sizeof(PRUnichar), &inUniChar, convertedString);
  if (err != noErr)
    return PR_FALSE;

  *outChar = convertedString[1];
  ::DisposeUnicodeToTextInfo(&converterInfo);
  return PR_TRUE;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(PR_FALSE);
}
#endif // NP_NO_CARBON

static void ConvertCocoaKeyEventToNPCocoaEvent(NSEvent* cocoaEvent, NPCocoaEvent& pluginEvent, PRUint32 keyType = 0)
{
  InitNPCocoaEvent(&pluginEvent);
  NSEventType nativeType = [cocoaEvent type];
  switch (nativeType) {
    case NSKeyDown:
      pluginEvent.type = NPCocoaEventKeyDown;
      break;
    case NSKeyUp:
      pluginEvent.type = NPCocoaEventKeyUp;
      break;
    case NSFlagsChanged:
      pluginEvent.type = NPCocoaEventFlagsChanged;
      break;
    default:
      printf("Asked to convert key event of unknown type to Cocoa plugin event!");
  }
  pluginEvent.data.key.modifierFlags = [cocoaEvent modifierFlags];
  // don't try to access character data for flags changed events, it will raise an exception
  if (nativeType != NSFlagsChanged) {
    pluginEvent.data.key.characters = (NPNSString*)[cocoaEvent characters];
    pluginEvent.data.key.charactersIgnoringModifiers = (NPNSString*)[cocoaEvent charactersIgnoringModifiers];
    pluginEvent.data.key.isARepeat = [cocoaEvent isARepeat];
    pluginEvent.data.key.keyCode = [cocoaEvent keyCode];
  }
}

#ifndef NP_NO_CARBON
static void ConvertCocoaKeyEventToCarbonEvent(NSEvent* cocoaEvent, EventRecord& pluginEvent, PRUint32 keyType = 0)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

    UInt32 charCode = 0;
    if ([cocoaEvent type] == NSFlagsChanged) {
      pluginEvent.what = keyType == NS_KEY_DOWN ? keyDown : keyUp;
    } else {
      if ([[cocoaEvent characters] length] > 0)
        charCode = [[cocoaEvent characters] characterAtIndex:0];
      if ([cocoaEvent type] == NSKeyDown)
        pluginEvent.what = [cocoaEvent isARepeat] ? autoKey : keyDown;
      else
        pluginEvent.what = keyUp;
    }

    if (charCode >= 0x0080) {
        switch (charCode) {
        case NSUpArrowFunctionKey:
            charCode = kUpArrowCharCode;
            break;
        case NSDownArrowFunctionKey:
            charCode = kDownArrowCharCode;
            break;
        case NSLeftArrowFunctionKey:
            charCode = kLeftArrowCharCode;
            break;
        case NSRightArrowFunctionKey:
            charCode = kRightArrowCharCode;
            break;
        default:
            unsigned char convertedCharCode;
            if (ConvertUnicodeToCharCode(charCode, &convertedCharCode))
              charCode = convertedCharCode;
            //NSLog(@"charcode is %d, converted to %c, char is %@", charCode, convertedCharCode, [cocoaEvent characters]);
            break;
        }
    }
    pluginEvent.message = (charCode & 0x00FF) | (nsCocoaUtils::GetCocoaEventKeyCode(cocoaEvent) << 8);
    pluginEvent.when = ::TickCount();
    ::GetGlobalMouse(&pluginEvent.where);
    pluginEvent.modifiers = ::GetCurrentKeyModifiers();

  NS_OBJC_END_TRY_ABORT_BLOCK;
}
#endif // NP_NO_CARBON

static PRBool IsPrintableChar(PRUnichar aChar)
{
  return (aChar >= 0x20 && aChar <= 0x7E) || aChar >= 0xA0;
}

static PRUint32 GetGeckoKeyCodeFromChar(PRUnichar aChar)
{
  // We don't support the key code for non-ASCII characters
  if (aChar > 0x7E)
    return 0;

  if (aChar >= 'a' && aChar <= 'z') // lowercase
    return PRUint32(toupper(aChar));
  else if (aChar >= 'A' && aChar <= 'Z') // uppercase
    return PRUint32(aChar);
  else if (aChar >= '0' && aChar <= '9')
    return PRUint32(aChar - '0' + NS_VK_0);

  switch (aChar)
  {
    case kReturnCharCode:
    case kEnterCharCode:
    case '\n':
      return NS_VK_RETURN;
    case '{':
    case '[':
      return NS_VK_OPEN_BRACKET;
    case '}':
    case ']':
      return NS_VK_CLOSE_BRACKET;
    case '\'':
    case '"':
      return NS_VK_QUOTE;

    case '\\':                  return NS_VK_BACK_SLASH;
    case ' ':                   return NS_VK_SPACE;
    case ';':                   return NS_VK_SEMICOLON;
    case '=':                   return NS_VK_EQUALS;
    case ',':                   return NS_VK_COMMA;
    case '.':                   return NS_VK_PERIOD;
    case '/':                   return NS_VK_SLASH;
    case '`':                   return NS_VK_BACK_QUOTE;
    case '\t':                  return NS_VK_TAB;
    case '-':                   return NS_VK_SUBTRACT;
    case '+':                   return NS_VK_ADD;

    default:
      if (!IsPrintableChar(aChar))
        NS_WARNING("GetGeckoKeyCodeFromChar has failed.");
      return 0;
    }
}

static PRUint32 ConvertMacToGeckoKeyCode(UInt32 keyCode, nsKeyEvent* aKeyEvent, NSString* characters)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  PRUint32 geckoKeyCode = 0;

  switch (keyCode)
  {
    // modifiers. We don't get separate events for these
    case kEscapeKeyCode:        geckoKeyCode = NS_VK_ESCAPE;         break;
    case kRCommandKeyCode:
    case kCommandKeyCode:       geckoKeyCode = NS_VK_META;           break;
    case kRShiftKeyCode:
    case kShiftKeyCode:         geckoKeyCode = NS_VK_SHIFT;          break;
    case kCapsLockKeyCode:      geckoKeyCode = NS_VK_CAPS_LOCK;      break;
    case kRControlKeyCode:
    case kControlKeyCode:       geckoKeyCode = NS_VK_CONTROL;        break;
    case kROptionKeyCode:
    case kOptionkeyCode:        geckoKeyCode = NS_VK_ALT;            break;
    case kClearKeyCode:         geckoKeyCode = NS_VK_CLEAR;          break;

    // function keys
    case kF1KeyCode:            geckoKeyCode = NS_VK_F1;             break;
    case kF2KeyCode:            geckoKeyCode = NS_VK_F2;             break;
    case kF3KeyCode:            geckoKeyCode = NS_VK_F3;             break;
    case kF4KeyCode:            geckoKeyCode = NS_VK_F4;             break;
    case kF5KeyCode:            geckoKeyCode = NS_VK_F5;             break;
    case kF6KeyCode:            geckoKeyCode = NS_VK_F6;             break;
    case kF7KeyCode:            geckoKeyCode = NS_VK_F7;             break;
    case kF8KeyCode:            geckoKeyCode = NS_VK_F8;             break;
    case kF9KeyCode:            geckoKeyCode = NS_VK_F9;             break;
    case kF10KeyCode:           geckoKeyCode = NS_VK_F10;            break;
    case kF11KeyCode:           geckoKeyCode = NS_VK_F11;            break;
    case kF12KeyCode:           geckoKeyCode = NS_VK_F12;            break;
    // case kF13KeyCode:           geckoKeyCode = NS_VK_F13;            break;    // clash with the 3 below
    // case kF14KeyCode:           geckoKeyCode = NS_VK_F14;            break;
    // case kF15KeyCode:           geckoKeyCode = NS_VK_F15;            break;
    case kPauseKeyCode:         geckoKeyCode = NS_VK_PAUSE;          break;
    case kScrollLockKeyCode:    geckoKeyCode = NS_VK_SCROLL_LOCK;    break;
    case kPrintScreenKeyCode:   geckoKeyCode = NS_VK_PRINTSCREEN;    break;

    // keypad
    case kKeypad0KeyCode:       geckoKeyCode = NS_VK_NUMPAD0;        break;
    case kKeypad1KeyCode:       geckoKeyCode = NS_VK_NUMPAD1;        break;
    case kKeypad2KeyCode:       geckoKeyCode = NS_VK_NUMPAD2;        break;
    case kKeypad3KeyCode:       geckoKeyCode = NS_VK_NUMPAD3;        break;
    case kKeypad4KeyCode:       geckoKeyCode = NS_VK_NUMPAD4;        break;
    case kKeypad5KeyCode:       geckoKeyCode = NS_VK_NUMPAD5;        break;
    case kKeypad6KeyCode:       geckoKeyCode = NS_VK_NUMPAD6;        break;
    case kKeypad7KeyCode:       geckoKeyCode = NS_VK_NUMPAD7;        break;
    case kKeypad8KeyCode:       geckoKeyCode = NS_VK_NUMPAD8;        break;
    case kKeypad9KeyCode:       geckoKeyCode = NS_VK_NUMPAD9;        break;

    case kKeypadMultiplyKeyCode:  geckoKeyCode = NS_VK_MULTIPLY;     break;
    case kKeypadAddKeyCode:       geckoKeyCode = NS_VK_ADD;          break;
    case kKeypadSubtractKeyCode:  geckoKeyCode = NS_VK_SUBTRACT;     break;
    case kKeypadDecimalKeyCode:   geckoKeyCode = NS_VK_DECIMAL;      break;
    case kKeypadDivideKeyCode:    geckoKeyCode = NS_VK_DIVIDE;       break;

    // these may clash with forward delete and help
    case kInsertKeyCode:        geckoKeyCode = NS_VK_INSERT;         break;
    case kDeleteKeyCode:        geckoKeyCode = NS_VK_DELETE;         break;

    case kBackspaceKeyCode:     geckoKeyCode = NS_VK_BACK;           break;
    case kTabKeyCode:           geckoKeyCode = NS_VK_TAB;            break;
    case kHomeKeyCode:          geckoKeyCode = NS_VK_HOME;           break;
    case kEndKeyCode:           geckoKeyCode = NS_VK_END;            break;
    case kPageUpKeyCode:        geckoKeyCode = NS_VK_PAGE_UP;        break;
    case kPageDownKeyCode:      geckoKeyCode = NS_VK_PAGE_DOWN;      break;
    case kLeftArrowKeyCode:     geckoKeyCode = NS_VK_LEFT;           break;
    case kRightArrowKeyCode:    geckoKeyCode = NS_VK_RIGHT;          break;
    case kUpArrowKeyCode:       geckoKeyCode = NS_VK_UP;             break;
    case kDownArrowKeyCode:     geckoKeyCode = NS_VK_DOWN;           break;
    case kVK_ANSI_1:            geckoKeyCode = NS_VK_1;              break;
    case kVK_ANSI_2:            geckoKeyCode = NS_VK_2;              break;
    case kVK_ANSI_3:            geckoKeyCode = NS_VK_3;              break;
    case kVK_ANSI_4:            geckoKeyCode = NS_VK_4;              break;
    case kVK_ANSI_5:            geckoKeyCode = NS_VK_5;              break;
    case kVK_ANSI_6:            geckoKeyCode = NS_VK_6;              break;
    case kVK_ANSI_7:            geckoKeyCode = NS_VK_7;              break;
    case kVK_ANSI_8:            geckoKeyCode = NS_VK_8;              break;
    case kVK_ANSI_9:            geckoKeyCode = NS_VK_9;              break;
    case kVK_ANSI_0:            geckoKeyCode = NS_VK_0;              break;

    default:
      // if we haven't gotten the key code already, look at the char code
      if ([characters length])
        geckoKeyCode = GetGeckoKeyCodeFromChar([characters characterAtIndex:0]);
  }

  return geckoKeyCode;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(0);
}

static PRBool IsSpecialGeckoKey(UInt32 macKeyCode)
{
  PRBool  isSpecial;
  
  // this table is used to determine which keys are special and should not generate a charCode
  switch (macKeyCode)
  {
    // modifiers - we don't get separate events for these yet
    case kEscapeKeyCode:
    case kShiftKeyCode:
    case kRShiftKeyCode:
    case kCommandKeyCode:
    case kRCommandKeyCode:
    case kCapsLockKeyCode:
    case kControlKeyCode:
    case kRControlKeyCode:
    case kOptionkeyCode:
    case kROptionKeyCode:
    case kClearKeyCode:
      
      // function keys
    case kF1KeyCode:
    case kF2KeyCode:
    case kF3KeyCode:
    case kF4KeyCode:
    case kF5KeyCode:
    case kF6KeyCode:
    case kF7KeyCode:
    case kF8KeyCode:
    case kF9KeyCode:
    case kF10KeyCode:
    case kF11KeyCode:
    case kF12KeyCode:
    case kPauseKeyCode:
    case kScrollLockKeyCode:
    case kPrintScreenKeyCode:
      
    case kInsertKeyCode:
    case kDeleteKeyCode:
    case kTabKeyCode:
    case kBackspaceKeyCode:
      
    case kHomeKeyCode:
    case kEndKeyCode:
    case kPageUpKeyCode:
    case kPageDownKeyCode:
    case kLeftArrowKeyCode:
    case kRightArrowKeyCode:
    case kUpArrowKeyCode:
    case kDownArrowKeyCode:
    case kReturnKeyCode:
    case kEnterKeyCode:
    case kPowerbookEnterKeyCode:
      isSpecial = PR_TRUE;
      break;
      
    default:
      isSpecial = PR_FALSE;
      break;
  }
  
  return isSpecial;
}

static PRBool IsNormalCharInputtingEvent(const nsKeyEvent& aEvent)
{
  // this is not character inputting event, simply.
  if (!aEvent.isChar || !aEvent.charCode)
    return PR_FALSE;
  // if this is unicode char inputting event, we don't need to check
  // ctrl/alt/command keys
  if (aEvent.charCode > 0x7F)
    return PR_TRUE;
  // ASCII chars should be inputted without ctrl/alt/command keys
  return !aEvent.isControl && !aEvent.isAlt && !aEvent.isMeta;
}

// Basic conversion for cocoa to gecko events, common to all conversions.
// Note that it is OK for inEvent to be nil.
- (void) convertGenericCocoaEvent:(NSEvent*)inEvent toGeckoEvent:(nsInputEvent*)outGeckoEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_ASSERTION(outGeckoEvent, "convertGenericCocoaEvent:toGeckoEvent: requires non-null outGeckoEvent");
  if (!outGeckoEvent)
    return;

  outGeckoEvent->widget = [self widget];
  outGeckoEvent->time = PR_IntervalNow();

  if (inEvent) {
    unsigned int modifiers = nsCocoaUtils::GetCocoaEventModifierFlags(inEvent);
    outGeckoEvent->isShift   = ((modifiers & NSShiftKeyMask) != 0);
    outGeckoEvent->isControl = ((modifiers & NSControlKeyMask) != 0);
    outGeckoEvent->isAlt     = ((modifiers & NSAlternateKeyMask) != 0);
    outGeckoEvent->isMeta    = ((modifiers & NSCommandKeyMask) != 0);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void) convertCocoaMouseEvent:(NSEvent*)aMouseEvent toGeckoEvent:(nsInputEvent*)outGeckoEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_ASSERTION(aMouseEvent && outGeckoEvent, "convertCocoaMouseEvent:toGeckoEvent: requires non-null arguments");
  if (!aMouseEvent || !outGeckoEvent)
    return;

  [self convertGenericCocoaEvent:aMouseEvent toGeckoEvent:outGeckoEvent];

  // convert point to view coordinate system
  NSPoint locationInWindow = nsCocoaUtils::EventLocationForWindow(aMouseEvent, [self window]);
  NSPoint localPoint = [self convertPoint:locationInWindow fromView:nil];
  outGeckoEvent->refPoint.x = static_cast<nscoord>(localPoint.x);
  outGeckoEvent->refPoint.y = static_cast<nscoord>(localPoint.y);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// #define DEBUG_KB 1

#ifndef NS_LEOPARD_AND_LATER

#define CHARCODE_MASK_1 0x00FF0000
#define CHARCODE_MASK_2 0x000000FF
#define CHARCODE_MASK   0x00FF00FF

static PRUint32
KeyTranslateToUnicode(Handle aHandle, UInt32 aKeyCode, UInt32 aModifiers,
                      TextEncoding aEncoding)
{
#ifdef DEBUG_KB
  NSLog(@"****  KeyTranslateToUnicode: aHandle: %p, aKeyCode: %X, aModifiers: %X, aEncoding: %X",
        aHandle, aKeyCode, aModifiers, aEncoding);
  PRBool isShift = aModifiers & shiftKey;
  PRBool isCtrl = aModifiers & controlKey;
  PRBool isOpt = aModifiers & optionKey;
  PRBool isCmd = aModifiers & cmdKey;
  PRBool isCL = aModifiers & alphaLock;
  PRBool isNL = aModifiers & kEventKeyModifierNumLockMask;
  NSLog(@"        Shift: %s, Ctrl: %s, Opt: %s, Cmd: %s, CapsLock: %s, NumLock: %s",
        isShift ? "ON" : "off", isCtrl ? "ON" : "off", isOpt ? "ON" : "off",
        isCmd ? "ON" : "off", isCL ? "ON" : "off", isNL ? "ON" : "off");
#endif
  UInt32 state = 0;
  UInt32 val =
    ::KeyTranslate(aHandle, aKeyCode | aModifiers, &state) & CHARCODE_MASK;
  // If state is not zero, it is in dead key state. Then, we need to recall
  // KeyTranslate for getting the actual character.
  if (state) {
    val =
      ::KeyTranslate(aHandle, aKeyCode | aModifiers, &state) & CHARCODE_MASK;
  }
  PRUint32 ch = 0;
  UInt8 buf[2];
  CFIndex len = 0;
  if (val & CHARCODE_MASK_1)
    buf[len++] = (val & CHARCODE_MASK_1) >> 16;
  buf[len++] = val & CHARCODE_MASK_2;

  CFStringRef str =
    ::CFStringCreateWithBytes(kCFAllocatorDefault, buf, len,
                              (CFStringEncoding)aEncoding, false);
  ch = ::CFStringGetLength(str) == 1 ?
         ::CFStringGetCharacterAtIndex(str, 0) : 0;
  ::CFRelease(str);
#ifdef DEBUG_KB
  NSLog(@"       result: %X(%C)", ch, ch > ' ' ? ch : ' ');
#endif
  return ch;
}

#endif // NS_LEOPARD_AND_LATER

static PRUint32
UCKeyTranslateToUnicode(const UCKeyboardLayout* aHandle, UInt32 aKeyCode, UInt32 aModifiers,
                        UInt32 aKbType)
{
#ifdef DEBUG_KB
  NSLog(@"**** UCKeyTranslateToUnicode: aHandle: %p, aKeyCode: %X, aModifiers: %X, aKbType: %X",
        aHandle, aKeyCode, aModifiers, aKbType);
  PRBool isShift = aModifiers & shiftKey;
  PRBool isCtrl = aModifiers & controlKey;
  PRBool isOpt = aModifiers & optionKey;
  PRBool isCmd = aModifiers & cmdKey;
  PRBool isCL = aModifiers & alphaLock;
  PRBool isNL = aModifiers & kEventKeyModifierNumLockMask;
  NSLog(@"        Shift: %s, Ctrl: %s, Opt: %s, Cmd: %s, CapsLock: %s, NumLock: %s",
        isShift ? "ON" : "off", isCtrl ? "ON" : "off", isOpt ? "ON" : "off",
        isCmd ? "ON" : "off", isCL ? "ON" : "off", isNL ? "ON" : "off");
#endif
  UInt32 deadKeyState = 0;
  UniCharCount len;
  UniChar chars[5];
  OSStatus err = ::UCKeyTranslate(aHandle, aKeyCode,
                                  kUCKeyActionDown, aModifiers >> 8,
                                  aKbType, kUCKeyTranslateNoDeadKeysMask,
                                  &deadKeyState, 5, &len, chars);
  PRUint32 ch = (err == noErr && len == 1) ? PRUint32(chars[0]) : 0;
#ifdef DEBUG_KB
  NSLog(@"       result: %X(%C)", ch, ch > ' ' ? ch : ' ');
#endif
  return ch;
}

struct KeyTranslateData {
  KeyTranslateData() {
    mUchr.mLayout = nsnull;
    mUchr.mKbType = 0;
#ifndef NS_LEOPARD_AND_LATER
    mKchr.mHandle = nsnull;
    mKchr.mEncoding = nsnull;
#endif // NS_LEOPARD_AND_LATER
  }

#ifndef NS_LEOPARD_AND_LATER
  // The script of the layout determines the encoding of characters obtained
  // from kchr resources.
  SInt16 mScript;
  // The keyboard layout identifier
  SInt32 mLayoutID;
#endif // NS_LEOPARD_AND_LATER

  struct {
    const UCKeyboardLayout* mLayout;
    UInt32 mKbType;
  } mUchr;
#ifndef NS_LEOPARD_AND_LATER
  struct {
    Handle mHandle;
    TextEncoding mEncoding;
  } mKchr;
#endif // NS_LEOPARD_AND_LATER
};

static PRUint32
GetUniCharFromKeyTranslate(KeyTranslateData& aData,
                           UInt32 aKeyCode, UInt32 aModifiers)
{
  if (aData.mUchr.mLayout) {
    return UCKeyTranslateToUnicode(aData.mUchr.mLayout, aKeyCode, aModifiers,
                                   aData.mUchr.mKbType);
  }
#ifndef NS_LEOPARD_AND_LATER
  if (aData.mKchr.mHandle) {
    return KeyTranslateToUnicode(aData.mKchr.mHandle, aKeyCode, aModifiers,
                                 aData.mKchr.mEncoding);
  }
#endif // NS_LEOPARD_AND_LATER
  return 0;
}

#ifndef NS_LEOPARD_AND_LATER

static SInt32
GetScriptFromKeyboardLayout(SInt32 aLayoutID)
{
  switch (aLayoutID) {
    case 0:                      // US
    case 3:                      // German
    case 224:                    // Swedish
    case -2:     return smRoman; // US-Extended
    case -18944: return smGreek; // Greek
    default: NS_NOTREACHED("unknown keyboard layout");
  }
  return smRoman;
}

static CFStringRef
GetInputSourceIDFromKeyboardLayout(SInt32 aLayoutID)
{
  NS_ASSERTION(nsToolkit::OnLeopardOrLater() &&
               Leopard_TISCopyCurrentKeyboardLayoutInputSource &&
               Leopard_TISGetInputSourceProperty &&
               Leopard_TISCreateInputSourceList &&
               kOurTISPropertyUnicodeKeyLayoutData &&
               kOurTISPropertyInputSourceID,
               "GetInputSourceIDFromKeyboardLayout should only be used on Leopard or later.");

  KeyboardLayoutRef keylayout;
  if (KLGetKeyboardLayoutWithIdentifier(aLayoutID, &keylayout) != noErr)
    return nsnull;

  const void* uchrFromID;
  if (KLGetKeyboardLayoutProperty(keylayout, kKLuchrData, &uchrFromID) != noErr)
    return nsnull;

  CFDictionaryRef dict = CFDictionaryCreate(kCFAllocatorDefault, NULL, NULL, 0, NULL, NULL);
  CFArrayRef inputSources = Leopard_TISCreateInputSourceList(dict, true);
  CFRelease(dict);

  CFStringRef sourceID = nsnull;
  for (CFIndex i = 0; i < CFArrayGetCount(inputSources); ++i) {
    TISInputSourceRef tis = static_cast<TISInputSourceRef>(const_cast<void *>(CFArrayGetValueAtIndex(inputSources, i)));
    CFDataRef data = static_cast<CFDataRef>(Leopard_TISGetInputSourceProperty(tis, kOurTISPropertyUnicodeKeyLayoutData));
    if (!data)
      continue;

    const UCKeyboardLayout* uchr = reinterpret_cast<const UCKeyboardLayout*>(CFDataGetBytePtr(data));
    if (uchr == uchrFromID) {
      sourceID = static_cast<CFStringRef>(Leopard_TISGetInputSourceProperty(tis, kOurTISPropertyInputSourceID));
      break;
    }
  }

  CFRelease(inputSources);

  return sourceID;
}

static void
GetKCHRData(KeyTranslateData &aKT)
{
  KeyboardLayoutRef kbRef;
  OSStatus err =
    ::KLGetKeyboardLayoutWithIdentifier(aKT.mLayoutID, &kbRef);
  if (err != noErr)
    return;

  err = ::KLGetKeyboardLayoutProperty(kbRef, kKLKCHRData,
                                      (const void**)&aKT.mKchr.mHandle);
  if (err != noErr || !aKT.mKchr.mHandle)
    return;

  err = ::GetTextEncodingFromScriptInfo(aKT.mScript, kTextLanguageDontCare,
                                        kTextRegionDontCare,
                                        &aKT.mKchr.mEncoding);
  if (err != noErr)
    aKT.mKchr.mHandle = nsnull;
}

#endif // NS_LEOPARD_AND_LATER

static PRUint32
GetUSLayoutCharFromKeyTranslate(UInt32 aKeyCode, UInt32 aModifiers)
{
#ifdef NS_LEOPARD_AND_LATER
  static const UCKeyboardLayout* sUSLayout = nsnull;
  if (!sUSLayout) {
    nsTISInputSource tis("com.apple.keylayout.US");
    sUSLayout = tis.GetUCKeyboardLayout();
    NS_ENSURE_TRUE(sUSLayout, 0);
  }

  UInt32 kbType = 40; // ANSI, don't use actual layout
  return UCKeyTranslateToUnicode(sUSLayout, aKeyCode, aModifiers, kbType);
#else
  KeyboardLayoutRef kbRef = nsnull;
  OSStatus err = ::KLGetKeyboardLayoutWithIdentifier(kKLUSKeyboard, &kbRef);
  NS_ENSURE_TRUE(err == noErr && kbRef, 0);
  const UCKeyboardLayout* layout = nsnull;
  err = ::KLGetKeyboardLayoutProperty(kbRef, kKLuchrData,
                                      (const void**)&layout);
  NS_ENSURE_TRUE(err == noErr && layout, 0);
  UInt32 kbType = 40; // ANSI, don't use actual layout
  return UCKeyTranslateToUnicode(layout, aKeyCode,
                                 aModifiers, kbType);
#endif // NS_LEOPARD_AND_LATER
}

- (void) convertCocoaKeyEvent:(NSEvent*)aKeyEvent toGeckoEvent:(nsKeyEvent*)outGeckoEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_ASSERTION(aKeyEvent && outGeckoEvent, "convertCocoaKeyEvent:toGeckoEvent: requires non-null arguments");
  if (!aKeyEvent || !outGeckoEvent)
    return;

  [self convertGenericCocoaEvent:aKeyEvent toGeckoEvent:outGeckoEvent];

  // coords for key events are always 0,0
  outGeckoEvent->refPoint.x = outGeckoEvent->refPoint.y = 0;

  // Initialize whether or not we are using charCodes to false.
  outGeckoEvent->isChar = PR_FALSE;

  // Check to see if the message is a key press that does not involve
  // one of our special key codes.
  if (outGeckoEvent->message == NS_KEY_PRESS &&
      !IsSpecialGeckoKey(nsCocoaUtils::GetCocoaEventKeyCode(aKeyEvent))) {
    outGeckoEvent->isChar = PR_TRUE; // this is not a special key
    
    outGeckoEvent->charCode = 0;
    outGeckoEvent->keyCode  = 0; // not set for key press events
    
    NSString* chars = [aKeyEvent characters];
    if ([chars length] > 0)
      outGeckoEvent->charCode = [chars characterAtIndex:0];
    
    // convert control-modified charCode to raw charCode (with appropriate case)
    if (outGeckoEvent->isControl && outGeckoEvent->charCode <= 26)
      outGeckoEvent->charCode += (outGeckoEvent->isShift) ? ('A' - 1) : ('a' - 1);

    // Accel and access key handling needs to know the characters that this
    // key produces with Shift up or down.  So, provide this information
    // when Ctrl or Command or Alt is pressed.
    if (outGeckoEvent->isControl || outGeckoEvent->isMeta ||
        outGeckoEvent->isAlt) {
      KeyTranslateData kt;

      PRBool isRomanKeyboardLayout;
#ifdef NS_LEOPARD_AND_LATER
      nsTISInputSource tis;
      if (gOverrideKeyboardLayout.mOverrideEnabled) {
        tis.InitByLayoutID(gOverrideKeyboardLayout.mKeyboardLayout);
      } else {
        tis.InitByCurrentKeyboardLayout();
      }
      kt.mUchr.mLayout = tis.GetUCKeyboardLayout();
      isRomanKeyboardLayout = tis.IsASCIICapable();
#else // NS_LEOPARD_AND_LATER
      if (gOverrideKeyboardLayout.mOverrideEnabled) {
        kt.mLayoutID = gOverrideKeyboardLayout.mKeyboardLayout;
        kt.mScript = GetScriptFromKeyboardLayout(kt.mLayoutID);
      } else {
        // GetScriptManagerVariable and GetScriptVariable are both deprecated.
        // KLGetCurrentKeyboardLayout is newer but also deprecated in OS X
        // 10.5.  It's not clear from the documentation but it seems that
        // KLGetKeyboardLayoutProperty with kKLGroupIdentifier may provide the
        // script identifier for a KeyboardLayoutRef (bug 432388 comment 6).
        // The "Text Input Source Services" API is not available prior to OS X
        // 10.5.
        kt.mScript = ::GetScriptManagerVariable(smKeyScript);
        kt.mLayoutID = ::GetScriptVariable(kt.mScript, smScriptKeys);
      }
      isRomanKeyboardLayout = (kt.mScript == smRoman);

      PRBool isUchrKeyboardLayout = PR_FALSE;
      // GetResource('uchr', kt.mLayoutID) fails on OS X 10.5
      if (nsToolkit::OnLeopardOrLater() &&
          Leopard_TISCopyCurrentKeyboardLayoutInputSource &&
          Leopard_TISGetInputSourceProperty &&
          Leopard_TISCreateInputSourceList &&
          kOurTISPropertyUnicodeKeyLayoutData &&
          kOurTISPropertyInputSourceID) {
        CFDataRef uchr = NULL;
        if (gOverrideKeyboardLayout.mOverrideEnabled) {
          CFStringRef sourceID = GetInputSourceIDFromKeyboardLayout(kt.mLayoutID);
          NS_ASSERTION(sourceID, "unable to map keyboard layout ID to input source ID");
          const void* keys[] = { kOurTISPropertyInputSourceID };
          const void* vals[] = { sourceID };
          CFDictionaryRef dict = CFDictionaryCreate(kCFAllocatorDefault, keys, vals, 1, NULL, NULL);
          CFArrayRef inputSources = Leopard_TISCreateInputSourceList(dict, true);
          CFRelease(dict);
          if (CFArrayGetCount(inputSources) == 1) {
            TISInputSourceRef tis = static_cast<TISInputSourceRef>(const_cast<void *>(CFArrayGetValueAtIndex(inputSources, 0)));
            uchr = static_cast<CFDataRef>(Leopard_TISGetInputSourceProperty(tis, kOurTISPropertyUnicodeKeyLayoutData));
          }
          CFRelease(inputSources);
        } else {
          TISInputSourceRef tis = Leopard_TISCopyCurrentKeyboardLayoutInputSource();
          uchr = static_cast<CFDataRef>(Leopard_TISGetInputSourceProperty(tis, kOurTISPropertyUnicodeKeyLayoutData));
        }
        if (uchr) {
          // We should be here on OS X 10.5 for any Apple provided layout, as
          // they are all uchr.  It may be possible to still use kchr resources
          // from elsewhere, so they may be picked by GetKCHRData below
          kt.mUchr.mLayout = reinterpret_cast<const UCKeyboardLayout*>
            (CFDataGetBytePtr(uchr));
          isUchrKeyboardLayout = PR_TRUE;
        }
      } else {
        // 10.4
        KeyboardLayoutRef kbRef = nsnull;
        OSStatus err = ::KLGetKeyboardLayoutWithIdentifier(kt.mLayoutID,
                                                           &kbRef);
        if (err == noErr && kbRef) {
          SInt32 kind;
          err = ::KLGetKeyboardLayoutProperty(kbRef, kKLKind,
                                              (const void **)&kind);
          if (err == noErr && kind != kKLKCHRKind) {
            err = ::KLGetKeyboardLayoutProperty(kbRef, kKLuchrData,
                      (const void**)&kt.mUchr.mLayout);
            if (err != noErr) {
              kt.mUchr.mLayout = nsnull;
              // if the kind is kKLKCHRuchrKind, we can retry by KCHR.
              isUchrKeyboardLayout = kind != kKLKCHRuchrKind;
            } else {
              isUchrKeyboardLayout = PR_TRUE;
            }
          }
        }
      }

      if (!isUchrKeyboardLayout) {
        GetKCHRData(kt);
      }
#endif // NS_LEOPARD_AND_LATER

      // If a keyboard layout override is set, we also need to force the
      // keyboard type to something ANSI to avoid test failures on machines
      // with JIS keyboards (since the pair of keyboard layout and physical
      // keyboard type form the actual key layout).  This assumes that the
      // test setting the override was written assuming an ANSI keyboard.
      if (kt.mUchr.mLayout)
        kt.mUchr.mKbType = gOverrideKeyboardLayout.mOverrideEnabled ? 40 : ::LMGetKbdType();

      UInt32 key = nsCocoaUtils::GetCocoaEventKeyCode(aKeyEvent);

      // Caps lock and num lock modifier state:
      UInt32 lockState = 0;
      if (nsCocoaUtils::GetCocoaEventModifierFlags(aKeyEvent) & NSAlphaShiftKeyMask)
        lockState |= alphaLock;
      if (nsCocoaUtils::GetCocoaEventModifierFlags(aKeyEvent) & NSNumericPadKeyMask)
        lockState |= kEventKeyModifierNumLockMask;

      // normal chars
      PRUint32 unshiftedChar = GetUniCharFromKeyTranslate(kt, key, lockState);
      UInt32 shiftLockMod = shiftKey | lockState;
      PRUint32 shiftedChar = GetUniCharFromKeyTranslate(kt, key, shiftLockMod);

      // characters generated with Cmd key
      // XXX we should remove CapsLock state, which changes characters from
      //     Latin to Cyrillic with Russian layout on 10.4 only when Cmd key
      //     is pressed.
      UInt32 numState = (lockState & ~alphaLock); // only num lock state
      PRUint32 uncmdedChar = GetUniCharFromKeyTranslate(kt, key, numState);
      UInt32 shiftNumMod = numState | shiftKey;
      PRUint32 uncmdedShiftChar =
                 GetUniCharFromKeyTranslate(kt, key, shiftNumMod);
      PRUint32 uncmdedUSChar = GetUSLayoutCharFromKeyTranslate(key, numState);
      UInt32 cmdNumMod = cmdKey | numState;
      PRUint32 cmdedChar = GetUniCharFromKeyTranslate(kt, key, cmdNumMod);
      UInt32 cmdShiftNumMod = shiftKey | cmdNumMod;
      PRUint32 cmdedShiftChar =
        GetUniCharFromKeyTranslate(kt, key, cmdShiftNumMod);

      // Is the keyboard layout changed by Cmd key?
      // E.g., Arabic, Russian, Hebrew, Greek and Dvorak-QWERTY.
      PRBool isCmdSwitchLayout = uncmdedChar != cmdedChar;
      // Is the keyboard layout for Latin, but Cmd key switches the layout?
      // I.e., Dvorak-QWERTY
      PRBool isDvorakQWERTY = isCmdSwitchLayout && isRomanKeyboardLayout;

      // If the current keyboard is not Dvorak-QWERTY or Cmd is not pressed,
      // we should append unshiftedChar and shiftedChar for handling the
      // normal characters.  These are the characters that the user is most
      // likely to associate with this key.
      if ((unshiftedChar || shiftedChar) &&
          (!outGeckoEvent->isMeta || !isDvorakQWERTY)) {
        nsAlternativeCharCode altCharCodes(unshiftedChar, shiftedChar);
        outGeckoEvent->alternativeCharCodes.AppendElement(altCharCodes);
      }

      // Most keyboard layouts provide the same characters in the NSEvents
      // with Command+Shift as with Command.  However, with Command+Shift we
      // want the character on the second level.  e.g. With a US QWERTY
      // layout, we want "?" when the "/","?" key is pressed with
      // Command+Shift.

      // On a German layout, the OS gives us '/' with Cmd+Shift+SS(eszett)
      // even though Cmd+SS is 'SS' and Shift+'SS' is '?'.  This '/' seems
      // like a hack to make the Cmd+"?" event look the same as the Cmd+"?"
      // event on a US keyboard.  The user thinks they are typing Cmd+"?", so
      // we'll prefer the "?" character, replacing charCode with shiftedChar
      // when Shift is pressed.  However, in case there is a layout where the
      // character unique to Cmd+Shift is the character that the user expects,
      // we'll send it as an alternative char.
      PRBool hasCmdShiftOnlyChar =
        cmdedChar != cmdedShiftChar && uncmdedShiftChar != cmdedShiftChar;
      PRUint32 originalCmdedShiftChar = cmdedShiftChar;

      // If we can make a good guess at the characters that the user would
      // expect this key combination to produce (with and without Shift) then
      // use those characters.  This also corrects for CapsLock, which was
      // ignored above.
      if (!isCmdSwitchLayout) {
        // The characters produced with Command seem similar to those without
        // Command.
        if (unshiftedChar)
          cmdedChar = unshiftedChar;
        if (shiftedChar)
          cmdedShiftChar = shiftedChar;
      } else if (uncmdedUSChar == cmdedChar) {
        // It looks like characters from a US layout are provided when Command
        // is down.
        PRUint32 ch = GetUSLayoutCharFromKeyTranslate(key, lockState);
        if (ch)
          cmdedChar = ch;
        ch = GetUSLayoutCharFromKeyTranslate(key, shiftLockMod);
        if (ch)
          cmdedShiftChar = ch;
      }

      // Only charCode (not alternativeCharCodes) is available to javascript,
      // so attempt to set this to the most likely intended (or most useful)
      // character.  Note that cmdedChar and cmdedShiftChar are usually
      // Latin/ASCII characters and that is what is wanted here as accel
      // keys are expected to be Latin characters.
      //
      // XXX We should do something similar when Control is down (bug 429510).
      if (outGeckoEvent->isMeta &&
           !(outGeckoEvent->isControl || outGeckoEvent->isAlt)) {

        // The character to use for charCode.
        PRUint32 preferredCharCode = 0;
        preferredCharCode = outGeckoEvent->isShift ? cmdedShiftChar : cmdedChar;

        if (preferredCharCode) {
#ifdef DEBUG_KB
          if (outGeckoEvent->charCode != preferredCharCode) {
            NSLog(@"      charCode replaced: %X(%C) to %X(%C)",
                  outGeckoEvent->charCode,
                  outGeckoEvent->charCode > ' ' ? outGeckoEvent->charCode : ' ',
                  preferredCharCode,
                  preferredCharCode > ' ' ? preferredCharCode : ' ');
          }
#endif
          outGeckoEvent->charCode = preferredCharCode;
        }
      }

      // If the current keyboard layout is switched by the Cmd key,
      // we should append cmdedChar and shiftedCmdChar that are
      // Latin char for the key. But don't append at Dvorak-QWERTY.
      if ((cmdedChar || cmdedShiftChar) &&
          isCmdSwitchLayout && !isDvorakQWERTY) {
        nsAlternativeCharCode altCharCodes(cmdedChar, cmdedShiftChar);
        outGeckoEvent->alternativeCharCodes.AppendElement(altCharCodes);
      }
      // Special case for 'SS' key of German layout. See the comment of
      // hasCmdShiftOnlyChar definition for the detail.
      if (hasCmdShiftOnlyChar && originalCmdedShiftChar) {
        nsAlternativeCharCode altCharCodes(0, originalCmdedShiftChar);
        outGeckoEvent->alternativeCharCodes.AppendElement(altCharCodes);
      }
    }
  }
  else {
    NSString* characters = nil;
    if ([aKeyEvent type] != NSFlagsChanged)
      characters = [aKeyEvent charactersIgnoringModifiers];
    
    outGeckoEvent->keyCode =
      ConvertMacToGeckoKeyCode(nsCocoaUtils::GetCocoaEventKeyCode(aKeyEvent), outGeckoEvent, characters);
    outGeckoEvent->charCode = 0;
  } 

  if (outGeckoEvent->message == NS_KEY_PRESS && !outGeckoEvent->isMeta)
    [NSCursor setHiddenUntilMouseMoves:YES];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

#ifndef NP_NO_CARBON
// Called from PluginKeyEventsHandler() (a handler for Carbon TSM events) to
// process a Carbon key event for the currently focused plugin.  Both Unicode
// characters and "Mac encoding characters" (in the MBCS or "multibyte
// character system") are (or should be) available from aKeyEvent, but here we
// use the MCBS characters.  This is how the WebKit does things, and seems to
// be what plugins expect.
- (void) processPluginKeyEvent:(EventRef)aKeyEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mGeckoChild)
    return;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  UInt32 numCharCodes;
  OSStatus status = ::GetEventParameter(aKeyEvent, kEventParamKeyMacCharCodes,
                                        typeChar, NULL, 0, &numCharCodes, NULL);
  if (status != noErr)
    return;

  nsAutoTArray<unsigned char, 3> charCodes;
  charCodes.SetLength(numCharCodes);
  status = ::GetEventParameter(aKeyEvent, kEventParamKeyMacCharCodes,
                               typeChar, NULL, numCharCodes, NULL, charCodes.Elements());
  if (status != noErr)
    return;

  UInt32 modifiers;
  status = ::GetEventParameter(aKeyEvent, kEventParamKeyModifiers,
                               typeUInt32, NULL, sizeof(modifiers), NULL, &modifiers);
  if (status != noErr)
    return;

  UInt32 macKeyCode;
  status = ::GetEventParameter(aKeyEvent, kEventParamKeyCode,
                               typeUInt32, NULL, sizeof(macKeyCode), NULL, &macKeyCode);
  if (status != noErr)
    return;

  EventRef cloneEvent = ::CopyEvent(aKeyEvent);
  for (unsigned int i = 0; i < numCharCodes; ++i) {
    status = ::SetEventParameter(cloneEvent, kEventParamKeyMacCharCodes,
                                 typeChar, 1, charCodes.Elements() + i);
    if (status != noErr)
      break;

    EventRecord eventRec;
    if (::ConvertEventRefToEventRecord(cloneEvent, &eventRec)) {
      nsKeyEvent keyDownEvent(PR_TRUE, NS_KEY_DOWN, mGeckoChild);

      PRUint32 keyCode(ConvertMacToGeckoKeyCode(macKeyCode, &keyDownEvent, @""));
      PRUint32 charCode(charCodes.ElementAt(i));

      keyDownEvent.time       = PR_IntervalNow();
      keyDownEvent.pluginEvent  = &eventRec;
      if (IsSpecialGeckoKey(macKeyCode)) {
        keyDownEvent.keyCode  = keyCode;
      } else {
        keyDownEvent.charCode = charCode;
        keyDownEvent.isChar   = PR_TRUE;
      }
      keyDownEvent.isShift   = ((modifiers & shiftKey) != 0);
      keyDownEvent.isControl = ((modifiers & controlKey) != 0);
      keyDownEvent.isAlt     = ((modifiers & optionKey) != 0);
      keyDownEvent.isMeta    = ((modifiers & cmdKey) != 0); // Should never happen
      mGeckoChild->DispatchWindowEvent(keyDownEvent);
      if (!mGeckoChild)
        break;
    }
  }

  ::ReleaseEvent(cloneEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}
#endif // NP_NO_CARBON

- (nsIntRect)sendCompositionEvent:(PRInt32) aEventType
{
#ifdef DEBUG_IME
  NSLog(@"****in sendCompositionEvent; type = %d", aEventType);
#endif

  if (!mGeckoChild)
    return nsIntRect(0, 0, 0, 0);

  if (aEventType == NS_COMPOSITION_START)
    [self initTSMDocument];

  // static void init_composition_event( *aEvent, int aType)
  nsCompositionEvent event(PR_TRUE, aEventType, mGeckoChild);
  event.time = PR_IntervalNow();
  mGeckoChild->DispatchWindowEvent(event);
  return event.theReply.mCursorPosition;
}

- (void)sendTextEvent:(PRUnichar*) aBuffer 
                      attributedString:(NSAttributedString*) aString  
                      selectedRange:(NSRange) selRange 
                      markedRange:(NSRange) markRange
                      doCommit:(BOOL) doCommit
{
#ifdef DEBUG_IME
  NSLog(@"****in sendTextEvent; string = '%@'", aString);
  NSLog(@" markRange = %d, %d;  selRange = %d, %d", markRange.location, markRange.length, selRange.location, selRange.length);
#endif

  if (!mGeckoChild)
    return;

  nsTextEvent textEvent(PR_TRUE, NS_TEXT_TEXT, mGeckoChild);
  textEvent.time = PR_IntervalNow();
  textEvent.theText = aBuffer;
  if (!doCommit)
    FillTextRangeInTextEvent(&textEvent, aString, markRange, selRange);

  mGeckoChild->DispatchWindowEvent(textEvent);
  if (textEvent.rangeArray)
    delete [] textEvent.rangeArray;
}

#pragma mark -
// NSTextInput implementation

#define MAX_BUFFER_SIZE 32

- (void)insertText:(id)insertString
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

#if DEBUG_IME
  NSLog(@"****in insertText: '%@'", insertString);
  NSLog(@" markRange = %d, %d", mMarkedRange.location, mMarkedRange.length);
#endif
  if (!mGeckoChild)
    return;

#ifdef NS_LEOPARD_AND_LATER
  if (mGeckoChild->TextInputHandler()->IgnoreIMEComposition())
    return;
#endif // NS_LEOPARD_AND_LATER

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  if (![insertString isKindOfClass:[NSAttributedString class]])
    insertString = [[[NSAttributedString alloc] initWithString:insertString] autorelease];

  NSString *tmpStr = [insertString string];
  unsigned int len = [tmpStr length];
  if (!mGeckoChild->IME_IsComposing() && len == 0)
    return; // nothing to do
  PRUnichar buffer[MAX_BUFFER_SIZE];
  PRUnichar *bufPtr = (len >= MAX_BUFFER_SIZE) ? new PRUnichar[len + 1] : buffer;
  [tmpStr getCharacters:bufPtr];
  bufPtr[len] = PRUnichar('\0');

  if (len == 1 && !mGeckoChild->IME_IsComposing()) {
    // don't let the same event be fired twice when hitting
    // enter/return! (Bug 420502)
    if (mKeyPressSent)
      return;

    // dispatch keypress event with char instead of textEvent
    nsKeyEvent geckoEvent(PR_TRUE, NS_KEY_PRESS, mGeckoChild);
    geckoEvent.time      = PR_IntervalNow();
    geckoEvent.charCode  = bufPtr[0]; // gecko expects OS-translated unicode
    geckoEvent.keyCode   = 0;
    geckoEvent.isChar    = PR_TRUE;
    if (mKeyDownHandled)
      geckoEvent.flags |= NS_EVENT_FLAG_NO_DEFAULT;
    // don't set other modifiers from the current event, because here in
    // -insertText: they've already been taken into account in creating
    // the input string.
        
    // create event for use by plugins
#ifndef NP_NO_CARBON
    EventRecord carbonEvent;
#endif
    if (mCurKeyEvent) {
      // XXX The ASCII characters inputting mode of egbridge (Japanese IME)
      // might send the keyDown event with wrong keyboard layout if other
      // keyboard layouts are already loaded. In that case, the native event
      // doesn't match to this gecko event...
#ifndef NP_NO_CARBON
      if (mPluginEventModel == NPEventModelCarbon) {
        ConvertCocoaKeyEventToCarbonEvent(mCurKeyEvent, carbonEvent);
        geckoEvent.pluginEvent = &carbonEvent;
      } else
#endif
      {
        geckoEvent.pluginEvent = NULL;
      }
      geckoEvent.isShift   = (nsCocoaUtils::GetCocoaEventModifierFlags(mCurKeyEvent) & NSShiftKeyMask) != 0;
      if (!IsPrintableChar(geckoEvent.charCode)) {
        geckoEvent.keyCode = 
          ConvertMacToGeckoKeyCode(nsCocoaUtils::GetCocoaEventKeyCode(mCurKeyEvent), &geckoEvent,
                                   [mCurKeyEvent charactersIgnoringModifiers]);
        geckoEvent.charCode = 0;
      }
    } else {
      // Note that insertText is not called only at key pressing.
      if (!IsPrintableChar(geckoEvent.charCode)) {
        geckoEvent.keyCode = GetGeckoKeyCodeFromChar(geckoEvent.charCode);
        geckoEvent.charCode = 0;
      }
    }

    PRBool keyPressHandled = mGeckoChild->DispatchWindowEvent(geckoEvent);
    // Only record the results of dispatching geckoEvent if we're currently
    // processing a keyDown event.
    if (mCurKeyEvent) {
      mKeyPressHandled = keyPressHandled;
      mKeyPressSent = YES;
    }
  }
  else {
    if (!mGeckoChild->IME_IsComposing()) {
      [self sendCompositionEvent:NS_COMPOSITION_START];
      // Note: mGeckoChild might have become null here. Don't count on it from here on.
      mGeckoChild->IME_OnStartComposition(self);
      // Note: mGeckoChild might have become null here. Don't count on it from here on.
    }

    if (mGeckoChild->IME_IgnoreCommit()) {
      tmpStr = [tmpStr init];
      len = 0;
      bufPtr[0] = PRUnichar('\0');
      insertString =
        [[[NSAttributedString alloc] initWithString:tmpStr] autorelease];
    }
    [self sendTextEvent:bufPtr attributedString:insertString
                               selectedRange:NSMakeRange(0, len)
                               markedRange:mMarkedRange
                               doCommit:YES];
    // Note: mGeckoChild might have become null here. Don't count on it from here on.

    [self sendCompositionEvent:NS_COMPOSITION_END];
    // Note: mGeckoChild might have become null here. Don't count on it from here on.
    mGeckoChild->IME_OnEndComposition();
    mMarkedRange = NSMakeRange(NSNotFound, 0);
  }

  if (bufPtr != buffer)
    delete[] bufPtr;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)insertNewline:(id)sender
{
  [self insertText:@"\n"];
}

- (void) doCommandBySelector:(SEL)aSelector
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

#if DEBUG_IME 
  NSLog(@"**** in doCommandBySelector %s (ignore %d)", aSelector, mKeyPressHandled);
#endif

  if (!mKeyPressHandled)
    [super doCommandBySelector:aSelector];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void) setMarkedText:(id)aString selectedRange:(NSRange)selRange
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

#if DEBUG_IME 
  NSLog(@"****in setMarkedText location: %d, length: %d", selRange.location, selRange.length);
  NSLog(@" markRange = %d, %d", mMarkedRange.location, mMarkedRange.length);
  NSLog(@" aString = '%@'", aString);
#endif

#ifdef NS_LEOPARD_AND_LATER
  if (mGeckoChild->TextInputHandler()->IgnoreIMEComposition())
    return;
#endif // NS_LEOPARD_AND_LATER

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  if (![aString isKindOfClass:[NSAttributedString class]])
    aString = [[[NSAttributedString alloc] initWithString:aString] autorelease];

  NSMutableAttributedString *mutableAttribStr = aString;
  NSString *tmpStr = [mutableAttribStr string];
  unsigned int len = [tmpStr length];
  PRUnichar buffer[MAX_BUFFER_SIZE];
  PRUnichar *bufPtr = (len >= MAX_BUFFER_SIZE) ? new PRUnichar[len + 1] : buffer;
  [tmpStr getCharacters:bufPtr];
  bufPtr[len] = PRUnichar('\0');

#if DEBUG_IME 
  printf("****in setMarkedText, len = %d, text = ", len);
  PRUint32 n = 0;
  PRUint32 maxlen = len > 12 ? 12 : len;
  for (PRUnichar *a = bufPtr; (*a != PRUnichar('\0')) && n<maxlen; a++, n++)
    printf((*a&0xff80) ? "\\u%4X" : "%c", *a); 
  printf("\n");
#endif

  mMarkedRange.length = len;

  if (!mGeckoChild->IME_IsComposing() && len > 0) {
    nsQueryContentEvent selection(PR_TRUE, NS_QUERY_SELECTED_TEXT, mGeckoChild);
    mGeckoChild->DispatchWindowEvent(selection);
    mMarkedRange.location = selection.mSucceeded ? selection.mReply.mOffset : 0;
    [self sendCompositionEvent:NS_COMPOSITION_START];
    // Note: mGeckoChild might have become null here. Don't count on it from here on.
    mGeckoChild->IME_OnStartComposition(self);
    // Note: mGeckoChild might have become null here. Don't count on it from here on.
  }

  if (mGeckoChild->IME_IsComposing()) {
    mGeckoChild->IME_OnUpdateComposition(tmpStr);

    BOOL commit = len == 0;
    [self sendTextEvent:bufPtr attributedString:aString
                                  selectedRange:selRange
                                    markedRange:mMarkedRange
                                       doCommit:commit];
    // Note: mGeckoChild might have become null here. Don't count on it from here on.

    if (commit) {
      [self sendCompositionEvent:NS_COMPOSITION_END];
      mGeckoChild->IME_OnEndComposition();
    }
  }

  if (bufPtr != buffer)
    delete[] bufPtr;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void) unmarkText
{
#if DEBUG_IME
  NSLog(@"****in unmarkText");
  NSLog(@" markedRange   = %d, %d", mMarkedRange.location, mMarkedRange.length);
#endif
  mGeckoChild->IME_CommitComposition();
}

- (BOOL) hasMarkedText
{
#if DEBUG_IME
  NSLog(@"****in hasMarkText");
  NSLog(@" markedRange   = %d, %d", mMarkedRange.location, mMarkedRange.length);
#endif
  return (mMarkedRange.location != NSNotFound) && (mMarkedRange.length != 0);
}

- (NSInteger) conversationIdentifier
{
#if DEBUG_IME
  NSLog(@"****in conversationIdentifier");
#endif
  if (!mGeckoChild)
    return (long)self;
  nsQueryContentEvent textContent(PR_TRUE, NS_QUERY_TEXT_CONTENT, mGeckoChild);
  textContent.InitForQueryTextContent(0, 0);
  mGeckoChild->DispatchWindowEvent(textContent);
  if (!textContent.mSucceeded)
    return (long)self;
#if DEBUG_IME
  NSLog(@" the ID = %ld", (long)textContent.mReply.mContentsRoot);
#endif
  return (long)textContent.mReply.mContentsRoot;
}

- (NSAttributedString *) attributedSubstringFromRange:(NSRange)theRange
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

#if DEBUG_IME
  NSLog(@"****in attributedSubstringFromRange");
  NSLog(@" theRange      = %d, %d", theRange.location, theRange.length);
  NSLog(@" markedRange   = %d, %d", mMarkedRange.location, mMarkedRange.length);
#endif
  if (!mGeckoChild || theRange.length == 0)
    return nil;

  nsAutoString str;
  nsQueryContentEvent textContent(PR_TRUE, NS_QUERY_TEXT_CONTENT, mGeckoChild);
  textContent.InitForQueryTextContent(theRange.location, theRange.length);
  mGeckoChild->DispatchWindowEvent(textContent);

  if (!textContent.mSucceeded || textContent.mReply.mString.IsEmpty())
    return nil;

  NSString* nsstr = ToNSString(textContent.mReply.mString);
  NSAttributedString* result =
    [[[NSAttributedString alloc] initWithString:nsstr
                                     attributes:nil] autorelease];
  return result;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSRange) markedRange
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

#if DEBUG_IME
  NSLog(@"****in markedRange");
  NSLog(@" markedRange   = %d, %d", mMarkedRange.location, mMarkedRange.length);
#endif

  if (![self hasMarkedText]) {
    return NSMakeRange(NSNotFound, 0);
  }

  return mMarkedRange;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSMakeRange(0, 0));
}

- (NSRange) selectedRange
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

#if DEBUG_IME
  NSLog(@"****in selectedRange");
  NSLog(@" markedRange   = %d, %d", mMarkedRange.location, mMarkedRange.length);
#endif
  if (!mGeckoChild)
    return NSMakeRange(NSNotFound, 0);
  nsQueryContentEvent selection(PR_TRUE, NS_QUERY_SELECTED_TEXT, mGeckoChild);
  mGeckoChild->DispatchWindowEvent(selection);
  if (!selection.mSucceeded)
    return NSMakeRange(NSNotFound, 0);

#if DEBUG_IME
  NSLog(@" result of selectedRange = %d, %d",
        selection.mReply.mOffset, selection.mReply.mString.Length());
#endif
  return NSMakeRange(selection.mReply.mOffset,
                     selection.mReply.mString.Length());

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSMakeRange(0, 0));
}

- (NSRect) firstRectForCharacterRange:(NSRange)theRange
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

#if DEBUG_IME
  NSLog(@"****in firstRectForCharacterRange");
  NSLog(@" theRange      = %d, %d", theRange.location, theRange.length);
  NSLog(@" markedRange   = %d, %d", mMarkedRange.location, mMarkedRange.length);
#endif
  // XXX this returns first character rect or caret rect, it is limitation of
  // now. We need more work for returns first line rect. But current
  // implementation is enough for IMEs.

  NSRect rect;
  if (!mGeckoChild || theRange.location == NSNotFound)
    return rect;

  nsIntRect r;
  PRBool useCaretRect = theRange.length == 0;
  if (!useCaretRect) {
    nsQueryContentEvent charRect(PR_TRUE, NS_QUERY_TEXT_RECT, mGeckoChild);
    charRect.InitForQueryTextRect(theRange.location, 1);
    mGeckoChild->DispatchWindowEvent(charRect);
    if (charRect.mSucceeded)
      r = charRect.mReply.mRect;
    else
      useCaretRect = PR_TRUE;
  }

  if (useCaretRect) {
    nsQueryContentEvent caretRect(PR_TRUE, NS_QUERY_CARET_RECT, mGeckoChild);
    caretRect.InitForQueryCaretRect(theRange.location);
    mGeckoChild->DispatchWindowEvent(caretRect);
    if (!caretRect.mSucceeded)
      return rect;
    r = caretRect.mReply.mRect;
    r.width = 0;
  }

  nsIWidget* rootWidget = mGeckoChild->GetTopLevelWidget();
  NSWindow* rootWindow =
    static_cast<NSWindow*>(rootWidget->GetNativeData(NS_NATIVE_WINDOW));
  NSView* rootView =
    static_cast<NSView*>(rootWidget->GetNativeData(NS_NATIVE_WIDGET));
  if (!rootWindow || !rootView)
    return rect;
  GeckoRectToNSRect(r, rect);
  rect = [rootView convertRect:rect toView:nil];
  rect.origin = [rootWindow convertBaseToScreen:rect.origin];
#if DEBUG_IME
  NSLog(@" result rect (x,y,w,h) = %f, %f, %f, %f",
        rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
#endif
  return rect;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSMakeRect(0.0, 0.0, 0.0, 0.0));
}

- (NSUInteger)characterIndexForPoint:(NSPoint)thePoint
{
#if DEBUG_IME
  NSLog(@"****in characterIndexForPoint");
  NSLog(@" markRange = %d, %d", mMarkedRange.location, mMarkedRange.length);
#endif

  // To implement this, we'd have to grovel in text frames looking at text offsets.
  return 0;
}

- (NSArray*) validAttributesForMarkedText
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

#if DEBUG_IME
  NSLog(@"****in validAttributesForMarkedText");
  NSLog(@" markRange = %d, %d", mMarkedRange.location, mMarkedRange.length);
#endif

  //return [NSArray arrayWithObjects:NSUnderlineStyleAttributeName, NSMarkedClauseSegmentAttributeName, NSTextInputReplacementRangeAttributeName, nil];
  return [NSArray array]; // empty array; we don't support any attributes right now

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

#pragma mark -

+ (NSEvent*)makeNewCocoaEventWithType:(NSEventType)type fromEvent:(NSEvent*)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  NSEvent* newEvent = [NSEvent keyEventWithType:type
                                       location:[theEvent locationInWindow] 
                                  modifierFlags:nsCocoaUtils::GetCocoaEventModifierFlags(theEvent)
                                      timestamp:[theEvent timestamp]
                                   windowNumber:[theEvent windowNumber]
                                        context:[theEvent context]
                                     characters:[theEvent characters]
                    charactersIgnoringModifiers:[theEvent charactersIgnoringModifiers]
                                      isARepeat:[theEvent isARepeat]
                                        keyCode:nsCocoaUtils::GetCocoaEventKeyCode(theEvent)];
  return newEvent;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

#ifdef PR_LOGGING
static const char* ToEscapedString(NSString* aString, nsCAutoString& aBuf)
{
  for (PRUint32 i = 0; i < [aString length]; ++i) {
    unichar ch = [aString characterAtIndex:i];
    if (ch >= 32 && ch < 128) {
      aBuf.Append(char(ch));
    } else {
      aBuf += nsPrintfCString("\\u%04x", ch);
    }
  }
  return aBuf.get();
}
#endif

// Returns PR_TRUE if Gecko claims to have handled the event, PR_FALSE otherwise.
- (PRBool)processKeyDownEvent:(NSEvent*)theEvent keyEquiv:(BOOL)isKeyEquiv
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if (!mGeckoChild)
    return NO;

#ifdef PR_LOGGING
  nsCAutoString str1;
  nsCAutoString str2;
#endif
  PR_LOG(sCocoaLog, PR_LOG_ALWAYS,
         ("ChildView processKeyDownEvent: keycode=%d,modifiers=%x,chars=%s,charsIgnoringModifiers=%s\n",
          nsCocoaUtils::GetCocoaEventKeyCode(theEvent),
          nsCocoaUtils::GetCocoaEventModifierFlags(theEvent),
          ToEscapedString([theEvent characters], str1),
          ToEscapedString([theEvent charactersIgnoringModifiers], str2)));

  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  mCurKeyEvent = theEvent;

  BOOL nonDeadKeyPress = [[theEvent characters] length] > 0;
  if (nonDeadKeyPress && !mGeckoChild->IME_IsComposing()) {
    if (![theEvent isARepeat]) {
      NSResponder* firstResponder = [[self window] firstResponder];

      nsKeyEvent geckoEvent(PR_TRUE, NS_KEY_DOWN, nsnull);
      [self convertCocoaKeyEvent:theEvent toGeckoEvent:&geckoEvent];

      // create event for use by plugins
#ifndef NP_NO_CARBON
      EventRecord carbonEvent;
      if (mPluginEventModel == NPEventModelCarbon) {
        ConvertCocoaKeyEventToCarbonEvent(theEvent, carbonEvent);
        geckoEvent.pluginEvent = &carbonEvent;
      }
#endif
      NPCocoaEvent cocoaEvent;
      if (mPluginEventModel == NPEventModelCocoa) {
        ConvertCocoaKeyEventToNPCocoaEvent(theEvent, cocoaEvent);
        geckoEvent.pluginEvent = &cocoaEvent;
      }

      mKeyDownHandled = mGeckoChild->DispatchWindowEvent(geckoEvent);
      if (!mGeckoChild)
        return mKeyDownHandled;

      // The key down event may have shifted the focus, in which
      // case we should not fire the key press.
      if (firstResponder != [[self window] firstResponder]) {
        PRBool handled = mKeyDownHandled;
        mCurKeyEvent = nil;
        mKeyDownHandled = PR_FALSE;
        return handled;
      }
    }

    // If this is the context menu key command, send a context menu key event.
    unsigned int modifierFlags =
      nsCocoaUtils::GetCocoaEventModifierFlags(theEvent) & NSDeviceIndependentModifierFlagsMask;
    if (modifierFlags == NSControlKeyMask && [[theEvent charactersIgnoringModifiers] isEqualToString:@" "]) {
      nsMouseEvent contextMenuEvent(PR_TRUE, NS_CONTEXTMENU, [self widget], nsMouseEvent::eReal, nsMouseEvent::eContextMenuKey);
      contextMenuEvent.isShift = contextMenuEvent.isControl = contextMenuEvent.isAlt = contextMenuEvent.isMeta = PR_FALSE;
      PRBool cmEventHandled = mGeckoChild->DispatchWindowEvent(contextMenuEvent);
      [self maybeInitContextMenuTracking];
      // Bail, there is nothing else to do here.
      PRBool handled = (cmEventHandled || mKeyDownHandled);
      mCurKeyEvent = nil;
      mKeyDownHandled = PR_FALSE;
      return handled;
    }

    nsKeyEvent geckoEvent(PR_TRUE, NS_KEY_PRESS, nsnull);
    [self convertCocoaKeyEvent:theEvent toGeckoEvent:&geckoEvent];

    // if this is a non-letter keypress, or the control key is down,
    // dispatch the keydown to gecko, so that we trap delete,
    // control-letter combinations etc before Cocoa tries to use
    // them for keybindings.
    if ((!geckoEvent.isChar || geckoEvent.isControl) &&
        !mGeckoChild->IME_IsComposing()) {
      if (mKeyDownHandled)
        geckoEvent.flags |= NS_EVENT_FLAG_NO_DEFAULT;

      // create event for use by plugins
#ifndef NP_NO_CARBON
      EventRecord carbonEvent;
      if (mPluginEventModel == NPEventModelCarbon) {
        ConvertCocoaKeyEventToCarbonEvent(theEvent, carbonEvent);
        geckoEvent.pluginEvent = &carbonEvent;
      } else
#endif
      {
        geckoEvent.pluginEvent = NULL;
      }

      mKeyPressHandled = mGeckoChild->DispatchWindowEvent(geckoEvent);
      mKeyPressSent = YES;
      if (!mGeckoChild)
        return (mKeyDownHandled || mKeyPressHandled);
    }
  }

  // Let Cocoa interpret the key events, caching IsComposing first.
  // We don't do it if this came from performKeyEquivalent because
  // interpretKeyEvents isn't set up to handle those key combinations.
  PRBool wasComposing = mGeckoChild->IME_IsComposing();
  PRBool interpretKeyEventsCalled = PR_FALSE;
  if (!isKeyEquiv &&
      (mGeckoChild->IME_IsEnabled() || mGeckoChild->IME_IsASCIICapableOnly())) {
    [super interpretKeyEvents:[NSArray arrayWithObject:theEvent]];
    interpretKeyEventsCalled = PR_TRUE;
  }

  if (!mGeckoChild)
    return (mKeyDownHandled || mKeyPressHandled);;

  if (!mKeyPressSent && nonDeadKeyPress && !wasComposing &&
      !mGeckoChild->IME_IsComposing()) {
    nsKeyEvent geckoEvent(PR_TRUE, NS_KEY_PRESS, nsnull);
    [self convertCocoaKeyEvent:theEvent toGeckoEvent:&geckoEvent];

    // If we called interpretKeyEvents and this isn't normal character input
    // then IME probably ate the event for some reason. We do not want to
    // send a key press event in that case.
    if (!(interpretKeyEventsCalled && IsNormalCharInputtingEvent(geckoEvent))) {
      if (mKeyDownHandled)
        geckoEvent.flags |= NS_EVENT_FLAG_NO_DEFAULT;

      // create event for use by plugins
#ifndef NP_NO_CARBON
      EventRecord carbonEvent;
      if (mPluginEventModel == NPEventModelCarbon) {
        ConvertCocoaKeyEventToCarbonEvent(theEvent, carbonEvent);
        geckoEvent.pluginEvent = &carbonEvent;
      } else
#endif
      {
        geckoEvent.pluginEvent = NULL;
      }

      mKeyPressHandled = mGeckoChild->DispatchWindowEvent(geckoEvent);
    }
  }

  // Note: mGeckoChild might have become null here. Don't count on it from here on.

  PRBool handled = (mKeyDownHandled || mKeyPressHandled);

  // See note about nested event loops where these variables are declared in header.
  mKeyPressHandled = NO;
  mKeyPressSent = NO;
  mCurKeyEvent = nil;
  mKeyDownHandled = PR_FALSE;

  return handled;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

- (void)initTSMDocument
{
  if (!mGeckoChild)
    return;

#ifndef NS_LEOPARD_AND_LATER
  // We need to get actual focused view. E.g., the view is in bookmark dialog
  // that is <panel> element. Then, the key events are processed the parent
  // window's view that has native focus.
  nsQueryContentEvent textContent(PR_TRUE, NS_QUERY_TEXT_CONTENT,
                                  mGeckoChild);
  textContent.InitForQueryTextContent(0, 0);
  mGeckoChild->DispatchWindowEvent(textContent);
  NSView<mozView>* focusedView = self;
  if (textContent.mSucceeded && textContent.mReply.mFocusedWidget) {
    NSView<mozView>* view =
      static_cast<NSView<mozView>*>(textContent.mReply.mFocusedWidget->
                                    GetNativeData(NS_NATIVE_WIDGET));
    if (view)
      focusedView = view;
  }
  nsTSMManager::InitTSMDocument(focusedView);
#endif // NS_LEOPARD_AND_LATER
}

#ifndef NP_NO_CARBON
// Create a TSM document for use with plugins, so that we can support IME in
// them.  Once it's created, if need be (re)activate it.  Some plugins (e.g.
// the Flash plugin running in Camino) don't create their own TSM document --
// without which IME can't work.  Others (e.g. the Flash plugin running in
// Firefox) create a TSM document that (somehow) makes the input window behave
// badly when it contains more than one kind of input (say Hiragana and
// Romaji).  (We can't just use the per-NSView TSM documents that Cocoa
// provices (those created and managed by the NSTSMInputContext class) -- for
// some reason TSMProcessRawKeyEvent() doesn't work with them.)
- (void)activatePluginTSMDoc
{
  if (!mPluginTSMDoc) {
    // Create a TSM document that supports both non-Unicode and Unicode input.
    // Though [ChildView processPluginKeyEvent:] only sends Mac char codes to
    // the plugin, this makes the input window behave better when it contains
    // more than one kind of input (say Hiragana and Romaji).  This is what
    // the OS does when it creates a TSM document for use by an
    // NSTSMInputContext class.
    InterfaceTypeList supportedServices;
    supportedServices[0] = kTextServiceDocumentInterfaceType;
    supportedServices[1] = kUnicodeDocumentInterfaceType;
    ::NewTSMDocument(2, supportedServices, &mPluginTSMDoc, 0);
    // We'll need to use the "input window".
    ::UseInputWindow(mPluginTSMDoc, YES);
    ::ActivateTSMDocument(mPluginTSMDoc);
  } else if (::TSMGetActiveDocument() != mPluginTSMDoc) {
    ::ActivateTSMDocument(mPluginTSMDoc);
  }
}
#endif // NP_NO_CARBON

- (void)keyDown:(NSEvent*)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

#ifndef NP_NO_CARBON
  // If a plugin has the focus, we need to use an alternate method for
  // handling NSKeyDown and NSKeyUp events (otherwise Carbon-based IME won't
  // work in plugins like the Flash plugin).  The same strategy is used by the
  // WebKit.  See PluginKeyEventsHandler() and [ChildView processPluginKeyEvent:]
  // for more info.
  if (mGeckoChild && mIsPluginView &&
      (mPluginEventModel == NPEventModelCarbon)) {
    [self activatePluginTSMDoc];
    // We use the active TSM document to pass a pointer to ourselves (the
    // currently focused ChildView) to PluginKeyEventsHandler().  Because this
    // pointer is weak, we should retain and release ourselves around the call
    // to TSMProcessRawKeyEvent().
    nsAutoRetainCocoaObject kungFuDeathGrip(self);
    ::TSMSetDocumentProperty(mPluginTSMDoc, kFocusedChildViewTSMDocPropertyTag,
                             sizeof(ChildView *), &self);
    ::TSMProcessRawKeyEvent([theEvent _eventRef]);
    ::TSMRemoveDocumentProperty(mPluginTSMDoc, kFocusedChildViewTSMDocPropertyTag);
    return;
  }
#endif

  [self processKeyDownEvent:theEvent keyEquiv:NO];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

static BOOL keyUpAlreadySentKeyDown = NO;

- (void)keyUp:(NSEvent*)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

#ifdef PR_LOGGING
  nsCAutoString str1;
  nsCAutoString str2;
#endif
  PR_LOG(sCocoaLog, PR_LOG_ALWAYS,
         ("ChildView keyUp: keycode=%d,modifiers=%x,chars=%s,charsIgnoringModifiers=%s\n",
          nsCocoaUtils::GetCocoaEventKeyCode(theEvent),
          nsCocoaUtils::GetCocoaEventModifierFlags(theEvent),
          ToEscapedString([theEvent characters], str1),
          ToEscapedString([theEvent charactersIgnoringModifiers], str2)));

  if (!mGeckoChild)
    return;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  if (mIsPluginView) {
    if (mPluginEventModel == NPEventModelCocoa) {
      nsKeyEvent keyUpEvent(PR_TRUE, NS_KEY_UP, nsnull);
      [self convertCocoaKeyEvent:theEvent toGeckoEvent:&keyUpEvent];
      NPCocoaEvent pluginEvent;
      ConvertCocoaKeyEventToNPCocoaEvent(theEvent, pluginEvent);
      keyUpEvent.pluginEvent = &pluginEvent;
      mGeckoChild->DispatchWindowEvent(keyUpEvent);
    }
#ifndef NP_NO_CARBON
    if (mPluginEventModel == NPEventModelCarbon) {
      // I'm not sure the call to TSMProcessRawKeyEvent() is needed here (though
      // WebKit makes one).
      ::TSMProcessRawKeyEvent([theEvent _eventRef]);
      
      // Don't send a keyUp event if the corresponding keyDown event(s) is/are
      // still being processed (idea borrowed from WebKit).
      ChildView *keyDownTarget = nil;
      OSStatus status = ::TSMGetDocumentProperty(mPluginTSMDoc, kFocusedChildViewTSMDocPropertyTag,
                                                 sizeof(ChildView *), nil, &keyDownTarget);
      if (status != noErr)
        keyDownTarget = nil;
      if (keyDownTarget == self)
        return;
      
      // PluginKeyEventsHandler() never sends keyUp events to [ChildView
      // processPluginKeyEvent:], so we need to send them to Gecko here.  (This
      // means that when commiting text from IME, several keyDown events may be
      // sent to Gecko (in processPluginKeyEvent) for one keyUp event here.
      // But this is how the WebKit does it, and games expect a keyUp event to
      // be sent when it actually happens (they need to be able to detect how
      // long a key has been held down) -- which wouldn't be possible if we sent
      // them from processPluginKeyEvent.)
      nsKeyEvent keyUpEvent(PR_TRUE, NS_KEY_UP, nsnull);
      [self convertCocoaKeyEvent:theEvent toGeckoEvent:&keyUpEvent];
      EventRecord macKeyUpEvent;
      ConvertCocoaKeyEventToCarbonEvent(theEvent, macKeyUpEvent);
      keyUpEvent.pluginEvent = &macKeyUpEvent;
      mGeckoChild->DispatchWindowEvent(keyUpEvent);      
    }
#endif
    return;
  }

  // if we don't have any characters we can't generate a keyUp event
  if ([[theEvent characters] length] == 0 || mGeckoChild->IME_IsComposing())
    return;

  // Cocoa doesn't send an NSKeyDown event for control-tab on 10.4, so if this
  // is an NSKeyUp event for control-tab, send a down event to gecko first.
  if (!nsToolkit::OnLeopardOrLater() && !keyUpAlreadySentKeyDown &&
      nsCocoaUtils::GetCocoaEventModifierFlags(theEvent) & NSControlKeyMask &&
      nsCocoaUtils::GetCocoaEventKeyCode(theEvent) == kTabKeyCode) {
    // We'll need an NSKeyDown copy of our native event so we convert to a gecko event correctly.
    NSEvent* nativeKeyDownEvent = [ChildView makeNewCocoaEventWithType:NSKeyDown fromEvent:theEvent];

    // send a key down event if we should
    PRBool keyDownHandled = PR_FALSE;
    if (![nativeKeyDownEvent isARepeat]) {
      nsKeyEvent geckoEvent(PR_TRUE, NS_KEY_DOWN, nsnull);
      [self convertCocoaKeyEvent:nativeKeyDownEvent toGeckoEvent:&geckoEvent];

      // plugin case returned out early, we don't need a native event here
      geckoEvent.pluginEvent = NULL;

      keyDownHandled = mGeckoChild->DispatchWindowEvent(geckoEvent);
      if (!mGeckoChild)
        return;
    }

    // Check to see if we are still the first responder.
    // The key down event may have shifted the focus, in which
    // case we should not fire the key press.
    NSResponder* resp = [[self window] firstResponder];
    if (resp != (NSResponder*)self) {
      keyUpAlreadySentKeyDown = YES;
      [resp keyUp:theEvent];      
      keyUpAlreadySentKeyDown = NO;
      return;
    }

    // now send a key press event if we should
    nsKeyEvent geckoEvent(PR_TRUE, NS_KEY_PRESS, nsnull);
    [self convertCocoaKeyEvent:nativeKeyDownEvent toGeckoEvent:&geckoEvent];

    if (keyDownHandled)
      geckoEvent.flags |= NS_EVENT_FLAG_NO_DEFAULT;

    // plugin case returned out early, we don't need a native event here
    geckoEvent.pluginEvent = NULL;

    mGeckoChild->DispatchWindowEvent(geckoEvent);
    if (!mGeckoChild)
      return;
  }

  nsKeyEvent geckoEvent(PR_TRUE, NS_KEY_UP, nsnull);
  [self convertCocoaKeyEvent:theEvent toGeckoEvent:&geckoEvent];

  // plugin case returned out early, we don't need a native event here
  geckoEvent.pluginEvent = NULL;

  mGeckoChild->DispatchWindowEvent(geckoEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (BOOL)performKeyEquivalent:(NSEvent*)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  // don't do anything if we don't have a gecko widget
  if (!mGeckoChild)
    return NO;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  // If we're not the first responder and the first responder is an NSView
  // object, pass the event on.  Otherwise (if, for example, the first
  // responder is an NSWindow object) we should trust the OS to have called
  // us correctly.
  id firstResponder = [[self window] firstResponder];
  if (firstResponder != self) {
    // Special handling if the other first responder is a ChildView.
    if ([firstResponder isKindOfClass:[ChildView class]])
      return [(ChildView *)firstResponder performKeyEquivalent:theEvent];
    if ([firstResponder isKindOfClass:[NSView class]])
      return [super performKeyEquivalent:theEvent];
  }

  // don't process if we're composing, but don't consume the event
  if (mGeckoChild->IME_IsComposing())
    return NO;

  UInt32 modifierFlags =
    nsCocoaUtils::GetCocoaEventModifierFlags(theEvent) & NSDeviceIndependentModifierFlagsMask;

  // Set to true if embedding menus handled the event when a plugin has focus.
  // We give menus a crack at handling commands before Gecko in the plugin case.
  BOOL handledByEmbedding = NO;

  // Perform native menu UI feedback even if we stop the event from propagating to it normally.
  // Recall that the menu system won't actually execute any commands for keyboard command invocations.
  //
  // If this is a plugin, we do actually perform the action on keyboard commands. See bug 428047.
  // If the action on plugins here changes the first responder, don't continue.
  NSMenu* mainMenu = [NSApp mainMenu];
  if (mIsPluginView) {
    if ([mainMenu isKindOfClass:[GeckoNSMenu class]]) {
      // Maintain a list of cmd+key combinations that we never act on (in the
      // browser) when the keyboard focus is in a plugin.  What a particular
      // cmd+key combo means here (to the browser) is governed by browser.dtd,
      // which "contains the browser main menu items".
      PRBool dontActOnKeyEquivalent = PR_FALSE;
      if (modifierFlags == NSCommandKeyMask) {
        NSString *unmodchars = [theEvent charactersIgnoringModifiers];
        if ([unmodchars length] == 1) {
          if ([unmodchars characterAtIndex:0] ==
              nsMenuBarX::GetLocalizedAccelKey("key_selectAll"))
            dontActOnKeyEquivalent = PR_TRUE;
        }
      }
      if (dontActOnKeyEquivalent) {
        [(GeckoNSMenu*)mainMenu performMenuUserInterfaceEffectsForEvent:theEvent];
      } else {
        [(GeckoNSMenu*)mainMenu actOnKeyEquivalent:theEvent];
      }
    }
    else {
      // This is probably an embedding situation. If the native menu handle the event
      // then return YES from pKE no matter what Gecko or the plugin does.
      handledByEmbedding = [mainMenu performKeyEquivalent:theEvent];
    }
    if ([[self window] firstResponder] != self)
      return YES;
  }
  else {
    if ([mainMenu isKindOfClass:[GeckoNSMenu class]])
      [(GeckoNSMenu*)mainMenu performMenuUserInterfaceEffectsForEvent:theEvent];
  }

  // With Cmd key or Ctrl+Tab or Ctrl+Esc, keyDown will be never called.
  // Therefore, we need to call processKeyDownEvent from performKeyEquivalent.
  UInt32 keyCode = nsCocoaUtils::GetCocoaEventKeyCode(theEvent);
  PRBool keyDownNeverFiredEvent = (modifierFlags & NSCommandKeyMask) ||
           ((modifierFlags & NSControlKeyMask) &&
            (keyCode == kEscapeKeyCode || keyCode == kTabKeyCode));

  // don't handle this if certain modifiers are down - those should
  // be sent as normal key up/down events and cocoa will do so automatically
  // if we reject here
  if (!keyDownNeverFiredEvent &&
      (modifierFlags & (NSFunctionKeyMask| NSNumericPadKeyMask)))
    return handledByEmbedding;

  // Control and option modifiers are used when changing input sources in the
  // input menu. We need to send such key events via "keyDown:", which will
  // happen if we return NO here. This only applies to Mac OS X 10.5 and higher,
  // previous OS versions just call "keyDown:" and not "performKeyEquivalent:"
  // for such events.
  if (!keyDownNeverFiredEvent &&
      (modifierFlags & (NSControlKeyMask | NSAlternateKeyMask)))
    return handledByEmbedding;

  if ([theEvent type] == NSKeyDown) {
    // We trust the Gecko handled status for cmd key events. See bug 417466 for more info.
    if (modifierFlags & NSCommandKeyMask)
      return ([self processKeyDownEvent:theEvent keyEquiv:YES] || handledByEmbedding);
    else
      [self processKeyDownEvent:theEvent keyEquiv:YES];
  }

  return YES;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

- (void)flagsChanged:(NSEvent*)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mGeckoChild)
    return;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  // CapsLock state and other modifier states are different:
  // CapsLock state does not revert when the CapsLock key goes up, as the
  // modifier state does for other modifier keys on key up.
  if ([theEvent keyCode] == kCapsLockKeyCode) {
    // Fire key down event for caps lock.
    [self fireKeyEventForFlagsChanged:theEvent keyDown:YES];
    if (!mGeckoChild)
      return;
    // XXX should we fire keyup event too? The keyup event for CapsLock key
    // is never sent to gecko.
  } else if ([theEvent type] == NSFlagsChanged) {
    // Fire key up/down events for the modifier keys (shift, alt, ctrl, command).
    unsigned int modifiers =
      nsCocoaUtils::GetCocoaEventModifierFlags(theEvent) & NSDeviceIndependentModifierFlagsMask;
    const PRUint32 kModifierMaskTable[] =
      { NSShiftKeyMask, NSControlKeyMask, NSAlternateKeyMask, NSCommandKeyMask };
    const PRUint32 kModifierCount = sizeof(kModifierMaskTable) /
                                    sizeof(kModifierMaskTable[0]);

    for (PRUint32 i = 0; i < kModifierCount; i++) {
      PRUint32 modifierBit = kModifierMaskTable[i];
      if ((modifiers & modifierBit) != (gLastModifierState & modifierBit)) {
        BOOL isKeyDown = (modifiers & modifierBit) != 0 ? YES : NO;

        [self fireKeyEventForFlagsChanged:theEvent keyDown:isKeyDown];

        if (!mGeckoChild)
          return;

        // Stop if focus has changed.
        // Check to see if we are still the first responder.
        if (![self isFirstResponder])
          break;
      }
    }

    gLastModifierState = modifiers;
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (BOOL) isFirstResponder
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NSResponder* resp = [[self window] firstResponder];
  return (resp == (NSResponder*)self);

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

- (void)fireKeyEventForFlagsChanged:(NSEvent*)theEvent keyDown:(BOOL)isKeyDown
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mGeckoChild || [theEvent type] != NSFlagsChanged ||
      mGeckoChild->IME_IsComposing())
    return;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  PRUint32 message = isKeyDown ? NS_KEY_DOWN : NS_KEY_UP;

  // Fire a key event.
  nsKeyEvent geckoEvent(PR_TRUE, message, nsnull);
  [self convertCocoaKeyEvent:theEvent toGeckoEvent:&geckoEvent];

  // create event for use by plugins
#ifndef NP_NO_CARBON
  EventRecord carbonEvent;
  if (mPluginEventModel == NPEventModelCarbon) {
    ConvertCocoaKeyEventToCarbonEvent(theEvent, carbonEvent, message);
    geckoEvent.pluginEvent = &carbonEvent;
  }
#endif
  NPCocoaEvent cocoaEvent;
  if (mPluginEventModel == NPEventModelCocoa) {
    ConvertCocoaKeyEventToNPCocoaEvent(theEvent, cocoaEvent, message);
    geckoEvent.pluginEvent = &cocoaEvent;
  }

  mGeckoChild->DispatchWindowEvent(geckoEvent);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// This method is called when are are about to lose focus.
// We must always call through to our superclass, even when mGeckoChild is
// nil -- otherwise the keyboard focus can end up in the wrong NSView.
- (BOOL)resignFirstResponder
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  return [super resignFirstResponder];

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

- (void)viewsWindowDidBecomeKey
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mGeckoChild)
    return;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  // check to see if the window implements the mozWindow protocol. This
  // allows embedders to avoid re-entrant calls to -makeKeyAndOrderFront,
  // which can happen because these activate calls propagate out
  // to the embedder via nsIEmbeddingSiteWindow::SetFocus().
  BOOL isMozWindow = [[self window] respondsToSelector:@selector(setSuppressMakeKeyFront:)];
  if (isMozWindow)
    [[self window] setSuppressMakeKeyFront:YES];

  [self sendFocusEvent:NS_ACTIVATE];

  if (isMozWindow)
    [[self window] setSuppressMakeKeyFront:NO];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)viewsWindowDidResignKey
{
  if (!mGeckoChild)
    return;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  [self sendFocusEvent:NS_DEACTIVATE];
}

// If the call to removeFromSuperview isn't delayed from nsChildView::
// TearDownView(), the NSView hierarchy might get changed during calls to
// [ChildView drawRect:], which leads to "beyond bounds" exceptions in
// NSCFArray.  For more info see bmo bug 373122.  Apple's docs claim that
// removeFromSuperviewWithoutNeedingDisplay "can be safely invoked during
// display" (whatever "display" means).  But it's _not_ true that it can be
// safely invoked during calls to [NSView drawRect:].  We use
// removeFromSuperview here because there's no longer any danger of being
// "invoked during display", and because doing do clears up bmo bug 384343.
- (void)delayedTearDown
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [self removeFromSuperview];
  [self release];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

#pragma mark -

// drag'n'drop stuff
#define kDragServiceContractID "@mozilla.org/widget/dragservice;1"

// This is a utility function used by NSView drag event methods
// to send events. It contains all of the logic needed for Gecko
// dragging to work. Returns YES if the event was handled, NO
// if it wasn't.
- (BOOL)doDragAction:(PRUint32)aMessage sender:(id)aSender
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if (!mGeckoChild)
    return NO;

  PR_LOG(sCocoaLog, PR_LOG_ALWAYS, ("ChildView doDragAction: entered\n"));

  if (!mDragService) {
    CallGetService(kDragServiceContractID, &mDragService);
    NS_ASSERTION(mDragService, "Couldn't get a drag service - big problem!");
    if (!mDragService)
      return NO;
  }

  if (aMessage == NS_DRAGDROP_ENTER)
    mDragService->StartDragSession();

  nsCOMPtr<nsIDragSession> dragSession;
  mDragService->GetCurrentSession(getter_AddRefs(dragSession));
  if (dragSession) {
    if (aMessage == NS_DRAGDROP_OVER) {
      // fire the drag event at the source. Just ignore whether it was
      // cancelled or not as there isn't actually a means to stop the drag
      mDragService->FireDragEventAtSource(NS_DRAGDROP_DRAG);
      dragSession->SetCanDrop(PR_FALSE);
    }
    else if (aMessage == NS_DRAGDROP_DROP) {
      // We make the assumption that the dragOver handlers have correctly set
      // the |canDrop| property of the Drag Session.
      PRBool canDrop = PR_FALSE;
      if (!NS_SUCCEEDED(dragSession->GetCanDrop(&canDrop)) || !canDrop) {
        nsCOMPtr<nsIDOMNode> sourceNode;
        dragSession->GetSourceNode(getter_AddRefs(sourceNode));
        if (!sourceNode) {
          mDragService->EndDragSession(PR_FALSE);
        }
        return NO;
      }
    }
    
    unsigned int modifierFlags =
      nsCocoaUtils::GetCocoaEventModifierFlags([NSApp currentEvent]);
    PRUint32 action = nsIDragService::DRAGDROP_ACTION_MOVE;
    // force copy = option, alias = cmd-option, default is move
    if (modifierFlags & NSAlternateKeyMask) {
      if (modifierFlags & NSCommandKeyMask)
        action = nsIDragService::DRAGDROP_ACTION_LINK;
      else
        action = nsIDragService::DRAGDROP_ACTION_COPY;
    }
    dragSession->SetDragAction(action);
  }

  // set up gecko event
  nsDragEvent geckoEvent(PR_TRUE, aMessage, nsnull);
  [self convertGenericCocoaEvent:nil toGeckoEvent:&geckoEvent];

  // Use our own coordinates in the gecko event.
  // Convert event from gecko global coords to gecko view coords.
  NSPoint localPoint = [self convertPoint:[aSender draggingLocation] fromView:nil];
  geckoEvent.refPoint.x = static_cast<nscoord>(localPoint.x);
  geckoEvent.refPoint.y = static_cast<nscoord>(localPoint.y);

  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  mGeckoChild->DispatchWindowEvent(geckoEvent);
  if (!mGeckoChild)
    return YES;

  if ((aMessage == NS_DRAGDROP_EXIT || aMessage == NS_DRAGDROP_DROP) &&
      dragSession) {
    nsCOMPtr<nsIDOMNode> sourceNode;
    dragSession->GetSourceNode(getter_AddRefs(sourceNode));
    if (!sourceNode) {
      // We're leaving a window while doing a drag that was
      // initiated in a different app. End the drag session,
      // since we're done with it for now (until the user
      // drags back into mozilla).
      mDragService->EndDragSession(PR_FALSE);
    }
  }

  return YES;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  PR_LOG(sCocoaLog, PR_LOG_ALWAYS, ("ChildView draggingEntered: entered\n"));
  
  // there should never be a globalDragPboard when "draggingEntered:" is
  // called, but just in case we'll take care of it here.
  [globalDragPboard release];

  // Set the global drag pasteboard that will be used for this drag session.
  // This will be set back to nil when the drag session ends (mouse exits
  // the view or a drop happens within the view).
  globalDragPboard = [[sender draggingPasteboard] retain];

  BOOL handled = [self doDragAction:NS_DRAGDROP_ENTER sender:sender];

  return handled ? NSDragOperationGeneric : NSDragOperationNone;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NSDragOperationNone);
}

- (NSDragOperation)draggingUpdated:(id <NSDraggingInfo>)sender
{
  PR_LOG(sCocoaLog, PR_LOG_ALWAYS, ("ChildView draggingUpdated: entered\n"));

  BOOL handled = [self doDragAction:NS_DRAGDROP_OVER sender:sender];
  return handled ? NSDragOperationGeneric : NSDragOperationNone;
}

- (void)draggingExited:(id <NSDraggingInfo>)sender
{
  PR_LOG(sCocoaLog, PR_LOG_ALWAYS, ("ChildView draggingExited: entered\n"));

  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  [self doDragAction:NS_DRAGDROP_EXIT sender:sender];
  NS_IF_RELEASE(mDragService);
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  BOOL handled = [self doDragAction:NS_DRAGDROP_DROP sender:sender];
  NS_IF_RELEASE(mDragService);
  return handled;
}

// NSDraggingSource
- (void)draggedImage:(NSImage *)anImage endedAt:(NSPoint)aPoint operation:(NSDragOperation)operation
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  gDraggedTransferables = nsnull;

  NSEvent *currentEvent = [NSApp currentEvent];
  gUserCancelledDrag = ([currentEvent type] == NSKeyDown &&
                        [currentEvent keyCode] == kEscapeKeyCode);

  if (!mDragService) {
    CallGetService(kDragServiceContractID, &mDragService);
    NS_ASSERTION(mDragService, "Couldn't get a drag service - big problem!");
  }

  if (mDragService) {
    // set the dragend point from the current mouse location
    nsDragService* dragService = static_cast<nsDragService *>(mDragService);
    NSPoint pnt = [NSEvent mouseLocation];
    FlipCocoaScreenCoordinate(pnt);
    dragService->SetDragEndPoint(nsIntPoint(NSToIntRound(pnt.x), NSToIntRound(pnt.y)));

    // XXX: dropEffect should be updated per |operation|. 
    // As things stand though, |operation| isn't well handled within "our"
    // events, that is, when the drop happens within the window: it is set
    // either to NSDragOperationGeneric or to NSDragOperationNone.
    // For that reason, it's not yet possible to override dropEffect per the
    // given OS value, and it's also unclear what's the correct dropEffect 
    // value for NSDragOperationGeneric that is passed by other applications.
    // All that said, NSDragOperationNone is still reliable.
    if (operation == NSDragOperationNone) {
      nsCOMPtr<nsIDOMDataTransfer> dataTransfer;
      dragService->GetDataTransfer(getter_AddRefs(dataTransfer));
      nsCOMPtr<nsIDOMNSDataTransfer> dataTransferNS =
        do_QueryInterface(dataTransfer);

      if (dataTransferNS)
        dataTransferNS->SetDropEffectInt(nsIDragService::DRAGDROP_ACTION_NONE);
    }

    mDragService->EndDragSession(PR_TRUE);
    NS_RELEASE(mDragService);
  }

  [globalDragPboard release];
  globalDragPboard = nil;
  [gLastDragMouseDownEvent release];
  gLastDragMouseDownEvent = nil;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// NSDraggingSource
// this is just implemented so we comply with the NSDraggingSource informal protocol
- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal
{
  return UINT_MAX;
}

// This method is a callback typically invoked in response to a drag ending on the desktop
// or a Findow folder window; the argument passed is a path to the drop location, to be used
// in constructing a complete pathname for the file(s) we want to create as a result of
// the drag.
- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL*)dropDestination
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  nsresult rv;

  PR_LOG(sCocoaLog, PR_LOG_ALWAYS, ("ChildView namesOfPromisedFilesDroppedAtDestination: entering callback for promised files\n"));

  nsCOMPtr<nsILocalFile> targFile;
  NS_NewLocalFile(EmptyString(), PR_TRUE, getter_AddRefs(targFile));
  nsCOMPtr<nsILocalFileMac> macLocalFile = do_QueryInterface(targFile);
  if (!macLocalFile) {
    NS_ERROR("No Mac local file");
    return nil;
  }

  if (!NS_SUCCEEDED(macLocalFile->InitWithCFURL((CFURLRef)dropDestination))) {
    NS_ERROR("failed InitWithCFURL");
    return nil;
  }

  if (!gDraggedTransferables)
    return nil;

  PRUint32 transferableCount;
  rv = gDraggedTransferables->Count(&transferableCount);
  if (NS_FAILED(rv))
    return nil;

  for (PRUint32 i = 0; i < transferableCount; i++) {
    nsCOMPtr<nsISupports> genericItem;
    gDraggedTransferables->GetElementAt(i, getter_AddRefs(genericItem));
    nsCOMPtr<nsITransferable> item(do_QueryInterface(genericItem));
    if (!item) {
      NS_ERROR("no transferable");
      return nil;
    }

    item->SetTransferData(kFilePromiseDirectoryMime, macLocalFile, sizeof(nsILocalFile*));
    
    // now request the kFilePromiseMime data, which will invoke the data provider
    // If successful, the returned data is a reference to the resulting file.
    nsCOMPtr<nsISupports> fileDataPrimitive;
    PRUint32 dataSize = 0;
    item->GetTransferData(kFilePromiseMime, getter_AddRefs(fileDataPrimitive), &dataSize);
  }
  
  NSPasteboard* generalPboard = [NSPasteboard pasteboardWithName:NSDragPboard];
  NSData* data = [generalPboard dataForType:@"application/x-moz-file-promise-dest-filename"];
  NSString* name = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
  NSArray* rslt = [NSArray arrayWithObject:name];

  [name release];

  return rslt;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

#pragma mark -

// Support for the "Services" menu. We currently only support sending strings
// and HTML to system services.

- (id)validRequestorForSendType:(NSString *)sendType
                     returnType:(NSString *)returnType
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  // sendType contains the type of data that the service would like this
  // application to send to it.  sendType is nil if the service is not
  // requesting any data.
  //
  // returnType contains the type of data the the service would like to
  // return to this application (e.g., to overwrite the selection).
  // returnType is nil if the service will not return any data.
  //
  // The following condition thus triggers when the service expects a string
  // or HTML from us or no data at all AND when the service will not send back
  // any data to us.

  if ((!sendType || [sendType isEqual:NSStringPboardType] ||
       [sendType isEqual:NSHTMLPboardType]) && !returnType) {
    // Query the Gecko window to determine if there is a current selection.
    if (mGeckoChild) {
      nsAutoRetainCocoaObject kungFuDeathGrip(self);

      nsQueryContentEvent event(PR_TRUE, NS_QUERY_CONTENT_STATE, mGeckoChild);
      mGeckoChild->DispatchWindowEvent(event);

      // Return this object if it can handle the request.
      if ((!sendType || (event.mSucceeded && event.mReply.mHasSelection)) &&
          !returnType)
        return self;
    }
  }

  return [super validRequestorForSendType:sendType returnType:returnType];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)writeSelectionToPasteboard:(NSPasteboard *)pboard
                             types:(NSArray *)types
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);

  // Make sure that the service will accept strings or HTML.
  if ([types containsObject:NSStringPboardType] == NO &&
      [types containsObject:NSHTMLPboardType] == NO)
    return NO;

  // Bail out if there is no Gecko object.
  if (!mGeckoChild)
    return NO;

  // Obtain the current selection.
  nsQueryContentEvent event(PR_TRUE,
                            NS_QUERY_SELECTION_AS_TRANSFERABLE,
                            mGeckoChild);
  mGeckoChild->DispatchWindowEvent(event);
  if (!event.mSucceeded || !event.mReply.mTransferable)
    return NO;

  // Transform the transferable to an NSDictionary.
  NSDictionary* pasteboardOutputDict = nsClipboard::PasteboardDictFromTransferable(event.mReply.mTransferable);
  if (!pasteboardOutputDict)
    return NO;

  // Declare the pasteboard types.
  unsigned int typeCount = [pasteboardOutputDict count];
  NSMutableArray * types = [NSMutableArray arrayWithCapacity:typeCount];
  [types addObjectsFromArray:[pasteboardOutputDict allKeys]];
  [pboard declareTypes:types owner:nil];

  // Write the data to the pasteboard.
  for (unsigned int i = 0; i < typeCount; i++) {
    NSString* currentKey = [types objectAtIndex:i];
    id currentValue = [pasteboardOutputDict valueForKey:currentKey];

    if (currentKey == NSStringPboardType ||
        currentKey == kCorePboardType_url ||
        currentKey == kCorePboardType_urld ||
        currentKey == kCorePboardType_urln) {
      [pboard setString:currentValue forType:currentKey];
    } else if (currentKey == NSHTMLPboardType) {
      [pboard setString:(nsClipboard::WrapHtmlForSystemPasteboard(currentValue)) forType:currentKey];
    } else if (currentKey == NSTIFFPboardType) {
      [pboard setData:currentValue forType:currentKey];
    } else if (currentKey == NSFilesPromisePboardType) {
      [pboard setPropertyList:currentValue forType:currentKey];        
    }
  }

  return YES;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

// Called if the service wants us to replace the current selection. We do
// not currently support replacing the current selection so just return NO.
- (BOOL)readSelectionFromPasteboard:(NSPasteboard *)pboard
{
  return NO;
}

#pragma mark -

#ifdef ACCESSIBILITY

/* Every ChildView has a corresponding mozDocAccessible object that is doing all
   the heavy lifting. The topmost ChildView corresponds to a mozRootAccessible
   object.

   All ChildView needs to do is to route all accessibility calls (from the NSAccessibility APIs)
   down to its object, pretending that they are the same.
*/
- (id<mozAccessible>)accessible
{
  if (!mGeckoChild)
    return nil;

  id<mozAccessible> nativeAccessible = nil;

  nsAutoRetainCocoaObject kungFuDeathGrip(self);
  nsCOMPtr<nsIWidget> kungFuDeathGrip2(mGeckoChild);
  nsCOMPtr<nsIAccessible> accessible;
  mGeckoChild->GetDocumentAccessible(getter_AddRefs(accessible));
  if (!mGeckoChild)
    return nil;

  if (accessible)
    accessible->GetNativeInterface((void**)&nativeAccessible);

#ifdef DEBUG_hakan
  NSAssert(![nativeAccessible isExpired], @"native acc is expired!!!");
#endif
  
  return nativeAccessible;
}

/* Implementation of formal mozAccessible formal protocol (enabling mozViews
   to talk to mozAccessible objects in the accessibility module). */

- (BOOL)hasRepresentedView
{
  return YES;
}

- (id)representedView
{
  return self;
}

- (BOOL)isRoot
{
  return [[self accessible] isRoot];
}

#ifdef DEBUG
- (void)printHierarchy
{
  [[self accessible] printHierarchy];
}
#endif

#pragma mark -

// general

- (BOOL)accessibilityIsIgnored
{
  return [[self accessible] accessibilityIsIgnored];
}

- (id)accessibilityHitTest:(NSPoint)point
{
  return [[self accessible] accessibilityHitTest:point];
}

- (id)accessibilityFocusedUIElement
{
  return [[self accessible] accessibilityFocusedUIElement];
}

// actions

- (NSArray*)accessibilityActionNames
{
  return [[self accessible] accessibilityActionNames];
}

- (NSString*)accessibilityActionDescription:(NSString*)action
{
  return [[self accessible] accessibilityActionDescription:action];
}

- (void)accessibilityPerformAction:(NSString*)action
{
  return [[self accessible] accessibilityPerformAction:action];
}

// attributes

- (NSArray*)accessibilityAttributeNames
{
  return [[self accessible] accessibilityAttributeNames];
}

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute
{
  return [[self accessible] accessibilityIsAttributeSettable:attribute];
}

- (id)accessibilityAttributeValue:(NSString*)attribute
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  id<mozAccessible> accessible = [self accessible];
  
  // if we're the root (topmost) accessible, we need to return our native AXParent as we
  // traverse outside to the hierarchy of whoever embeds us. thus, fall back on NSView's
  // default implementation for this attribute.
  if ([attribute isEqualToString:NSAccessibilityParentAttribute] && [accessible isRoot]) {
    id parentAccessible = [super accessibilityAttributeValue:attribute];
    return parentAccessible;
  }

  return [accessible accessibilityAttributeValue:attribute];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

#endif /* ACCESSIBILITY */

@end

#ifndef NS_LEOPARD_AND_LATER

#pragma mark -

void
nsTSMManager::OnDestroyView(NSView<mozView>* aDestroyingView)
{
  if (aDestroyingView != sComposingView)
    return;
  if (IsComposing()) {
    CancelIME(); // XXX Might CancelIME() fail because sComposingView is being destroyed?
    EndComposing();
  }
}

PRBool
nsTSMManager::GetIMEOpenState()
{
  return GetScriptManagerVariable(smKeyScript) != smRoman ? PR_TRUE : PR_FALSE;
}

static const NSString* GetCurrentIMELanguage()
{
  if (!nsToolkit::OnLeopardOrLater()) {
    // XXX [[NSInputManager currentInputManager] language] doesn't work fine.
    switch (::GetScriptManagerVariable(smKeyScript)) {
      case smJapanese:
        return @"ja";
      default:
        return nil;
    }
  }

  NS_PRECONDITION(Leopard_TISCopyCurrentKeyboardInputSource,
    "Leopard_TISCopyCurrentKeyboardInputSource is not initialized");
  TISInputSourceRef inputSource = Leopard_TISCopyCurrentKeyboardInputSource();
  if (!inputSource) {
    NS_ERROR("Leopard_TISCopyCurrentKeyboardInputSource failed");
    return nil;
  }

  NS_PRECONDITION(Leopard_TISGetInputSourceProperty,
    "Leopard_TISGetInputSourceProperty is not initialized");
  CFArrayRef langs = static_cast<CFArrayRef>(
    Leopard_TISGetInputSourceProperty(inputSource,
                                      kOurTISPropertyInputSourceLanguages));
  return static_cast<const NSString*>(CFArrayGetValueAtIndex(langs, 0));
}

void
nsTSMManager::InitTSMDocument(NSView<mozView>* aViewForCaret)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  sDocumentID = ::TSMGetActiveDocument();
  if (!sDocumentID)
    return;

  // We need to set the focused window level to TSMDocument. Then, the popup
  // windows of IME (E.g., a candidate list window) will be over the focused
  // view. See http://developer.apple.com/technotes/tn2005/tn2128.html#TNTAG1
  NSInteger TSMLevel, windowLevel;
  UInt32 size = sizeof(TSMLevel);

  OSStatus err =
    ::TSMGetDocumentProperty(sDocumentID, kTSMDocumentWindowLevelPropertyTag,
                             size, &size, &TSMLevel);
  windowLevel = [[aViewForCaret window] level];

  // Chinese IMEs on 10.5 don't work fine if the level is NSNormalWindowLevel,
  // then, we need to increment the value.
  if (windowLevel == NSNormalWindowLevel)
    windowLevel++;

  if (err == noErr && TSMLevel >= windowLevel)
    return;
  ::TSMSetDocumentProperty(sDocumentID, kTSMDocumentWindowLevelPropertyTag,
                           sizeof(windowLevel), &windowLevel);

  // ATOK (Japanese IME) updates the window level at activating,
  // we need to notify the change with this hack.
  const NSString* lang = ::GetCurrentIMELanguage();
  if (lang && [lang isEqualToString:@"ja"]) {
    ::DeactivateTSMDocument(sDocumentID);
    ::ActivateTSMDocument(sDocumentID);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
nsTSMManager::StartComposing(NSView<mozView>* aComposingView)
{
  if (sComposingView && sComposingView != sComposingView)
    CommitIME();
  sComposingView = aComposingView;
  NS_ASSERTION(::TSMGetActiveDocument() == sDocumentID,
               "We didn't initialize the TSMDocument");
}

void
nsTSMManager::UpdateComposing(NSString* aComposingString)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (sComposingString)
    [sComposingString release];
  sComposingString = [aComposingString retain];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
nsTSMManager::EndComposing()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  sComposingView = nsnull;
  if (sComposingString) {
    [sComposingString release];
    sComposingString = nsnull;
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
nsTSMManager::EnableIME(PRBool aEnable)
{
  if (aEnable == sIsIMEEnabled)
    return;
  CommitIME();
  sIsIMEEnabled = aEnable;
}

void
nsTSMManager::SetIMEOpenState(PRBool aOpen)
{
  if (aOpen == GetIMEOpenState())
    return;
  CommitIME();
  KeyScript(aOpen ? smKeySwapScript : smKeyRoman);
}

#define ENABLE_ROMAN_KYBDS_ONLY -23
void
nsTSMManager::SetRomanKeyboardsOnly(PRBool aRomanOnly)
{
  if (aRomanOnly == sIsRomanKeyboardsOnly)
    return;
  CommitIME();
  KeyScript(aRomanOnly ? ENABLE_ROMAN_KYBDS_ONLY : smKeyEnableKybds);
  sIsRomanKeyboardsOnly = aRomanOnly;
}

void
nsTSMManager::KillComposing()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Force commit the current composition
  // XXX Don't use NSInputManager. Because it cannot control the non-forcused
  // input manager, therefore, on deactivating a window, it does not work fine.
  NS_ASSERTION(sDocumentID, "The TSMDocumentID is null");
  ::FixTSMDocument(sDocumentID);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
nsTSMManager::CommitIME()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!IsComposing())
    return;
  KillComposing();
  if (!IsComposing())
    return;
  // If the composing transaction is still there, KillComposing only kills the
  // composing in TSM. We also need to kill the our composing transaction too.
  NSAttributedString* str =
    [[NSAttributedString alloc] initWithString:sComposingString];
  [sComposingView insertText:str];
  [str release];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
nsTSMManager::CancelIME()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!IsComposing())
    return;
  // For canceling the current composing, we need to ignore the param of
  // insertText. But this code is ugly...
  sIgnoreCommit = PR_TRUE;
  KillComposing();
  sIgnoreCommit = PR_FALSE;
  if (!IsComposing())
    return;
  // If the composing transaction is still there, KillComposing only kills the
  // composing in TSM. We also need to kill the our composing transaction too.
  NSAttributedString* str = [[NSAttributedString alloc] initWithString:@""];
  [sComposingView insertText:str];
  [str release];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

#endif // NS_LEOPARD_AND_LATER

#pragma mark -

void
ChildViewMouseTracker::OnDestroyView(ChildView* aView)
{
  if (sLastMouseEventView == aView)
    sLastMouseEventView = nil;
}

void
ChildViewMouseTracker::MouseMoved(NSEvent* aEvent)
{
  ChildView* oldView = sLastMouseEventView;
  ChildView* newView = ViewForEvent(aEvent);
  sLastMouseEventView = newView;
  if (newView != oldView) {
    // Send enter and / or exit events.
    nsMouseEvent::exitType type = [newView window] == [oldView window] ?
                                    nsMouseEvent::eChild : nsMouseEvent::eTopLevel;
    [oldView sendMouseEnterOrExitEvent:aEvent enter:NO type:type];
    // After the cursor exits the window set it to a visible regular arrow cursor.
    if (type == nsMouseEvent::eTopLevel) {
      [[nsCursorManager sharedInstance] setCursor:eCursor_standard];
    }
    // Sending the exit event to the old view might have destroyed our new view;
    // if that has happened, sLastMouseEventView has been set to nil.
    newView = sLastMouseEventView;
    [newView sendMouseEnterOrExitEvent:aEvent enter:YES type:type];
  }
  [newView handleMouseMoved:aEvent];
}

ChildView*
ChildViewMouseTracker::ViewForEvent(NSEvent* aEvent)
{
  NSWindow* window = WindowForEvent(aEvent);
  if (!window || !WindowAcceptsEvent(window, aEvent))
    return nil;

  NSPoint windowEventLocation = nsCocoaUtils::EventLocationForWindow(aEvent, window);
  NSView* view = [[[window contentView] superview] hitTest:windowEventLocation];
  NS_ASSERTION(view, "How can the mouse be over a window but not over a view in that window?");
  return [view isKindOfClass:[ChildView class]] ? (ChildView*)view : nil;
}

// Find the active window under the mouse. Returns nil if the mouse isn't over
// any active window.
NSWindow*
ChildViewMouseTracker::WindowForEvent(NSEvent* anEvent)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  NSInteger windowCount;
  NSCountWindows(&windowCount);
  NSInteger* windowList = (NSInteger*)malloc(sizeof(NSInteger) * windowCount);
  if (!windowList)
    return nil;
  // The list we get back here is in order from front to back.
  NSWindowList(windowCount, windowList);

  NSPoint screenPoint = nsCocoaUtils::ScreenLocationForEvent(anEvent);

  for (NSInteger i = 0; i < windowCount; i++) {
    NSWindow* currentWindow = [NSApp windowWithWindowNumber:windowList[i]];
    if (currentWindow && NSMouseInRect(screenPoint, [currentWindow frame], NO)) {
      free(windowList);
      return currentWindow;
    }
  }

  free(windowList);
  return nil;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

BOOL
ChildViewMouseTracker::WindowAcceptsEvent(NSWindow* aWindow, NSEvent* anEvent)
{
  // Right mouse down events may get through to all windows, even to a top level
  // window with an open sheet.
  if (!aWindow || [anEvent type] == NSRightMouseDown)
    return YES;

  id delegate = [aWindow delegate];
  if (!delegate || ![delegate isKindOfClass:[WindowDelegate class]])
    return YES;

  nsIWidget *windowWidget = [(WindowDelegate *)delegate geckoWidget];
  if (!windowWidget)
    return YES;

  nsWindowType windowType;
  windowWidget->GetWindowType(windowType);

  switch (windowType) {
    case eWindowType_popup:
      // If this is a context menu, it won't have a parent. So we'll always
      // accept mouse move events on context menus even when none of our windows
      // is active, which is the right thing to do.
      // For panels, the parent window is the XUL window that owns the panel.
      return WindowAcceptsEvent([aWindow parentWindow], anEvent);

    case eWindowType_toplevel:
    case eWindowType_dialog:
      // Block all mouse events other than RightMouseDown on background windows
      // and on windows behind sheets.
      return [aWindow isMainWindow] && ![aWindow attachedSheet];

    case eWindowType_sheet: {
      nsIWidget* parentWidget = windowWidget->GetSheetWindowParent();
      if (!parentWidget)
        return YES;

      // Only accept mouse events on a sheet whose containing window is active.
      NSWindow* parentWindow = (NSWindow*)parentWidget->GetNativeData(NS_NATIVE_WINDOW);
      return [parentWindow isMainWindow];
    }

    default:
      return YES;
  }
}

#pragma mark -

#ifndef NP_NO_CARBON

// Target for text services events sent as the result of calls made to
// TSMProcessRawKeyEvent() in [ChildView keyDown:] (above) when a plugin has
// the focus.  The calls to TSMProcessRawKeyEvent() short-circuit Cocoa-based
// IME (which would otherwise interfere with our efforts) and allow Carbon-
// based IME to work in plugins (via the NPAPI).  This strategy doesn't cause
// trouble for plugins that (like the Java Embedding Plugin) bypass the NPAPI
// to get their keyboard events and do their own Cocoa-based IME.
OSStatus PluginKeyEventsHandler(EventHandlerCallRef inHandlerRef,
                                EventRef inEvent, void *userData)
{
  id arp = [[NSAutoreleasePool alloc] init];

  TSMDocumentID activeDoc = ::TSMGetActiveDocument();
  if (!activeDoc) {
    [arp release];
    return eventNotHandledErr;
  }

  ChildView *target = nil;
  OSStatus status = ::TSMGetDocumentProperty(activeDoc, kFocusedChildViewTSMDocPropertyTag,
                                             sizeof(ChildView *), nil, &target);
  if (status != noErr)
    target = nil;
  if (!target) {
    [arp release];
    return eventNotHandledErr;
  }

  EventRef keyEvent = NULL;
  status = ::GetEventParameter(inEvent, kEventParamTextInputSendKeyboardEvent,
                               typeEventRef, NULL, sizeof(EventRef), NULL, &keyEvent);
  if ((status != noErr) || !keyEvent) {
    [arp release];
    return eventNotHandledErr;
  }

  [target processPluginKeyEvent:keyEvent];

  [arp release];
  return noErr;
}

static EventHandlerRef gPluginKeyEventsHandler = NULL;

// Called from nsAppShell::Init()
void NS_InstallPluginKeyEventsHandler()
{
  if (gPluginKeyEventsHandler)
    return;
  static const EventTypeSpec sTSMEvents[] =
    { { kEventClassTextInput, kEventTextInputUnicodeForKeyEvent } };
  ::InstallEventHandler(::GetEventDispatcherTarget(),
                        ::NewEventHandlerUPP(PluginKeyEventsHandler),
                        GetEventTypeCount(sTSMEvents),
                        sTSMEvents,
                        NULL,
                        &gPluginKeyEventsHandler);
}

// Called from nsAppShell::Exit()
void NS_RemovePluginKeyEventsHandler()
{
  if (!gPluginKeyEventsHandler)
    return;
  ::RemoveEventHandler(gPluginKeyEventsHandler);
  gPluginKeyEventsHandler = NULL;
}

#endif // NP_NO_CARBON

@interface NSView (MethodSwizzling)
- (BOOL)nsChildView_NSView_mouseDownCanMoveWindow;
@end

@implementation NSView (MethodSwizzling)

// All top-level browser windows belong to the ToolbarWindow class and have
// NSTexturedBackgroundWindowMask turned on in their "style" (see particularly
// [ToolbarWindow initWithContentRect:...] in nsCocoaWindow.mm).  This style
// normally means the window "may be moved by clicking and dragging anywhere
// in the window background", but we've suppressed this by giving the
// ChildView class a mouseDownCanMoveWindow method that always returns NO.
// Normally a ToolbarWindow's contentView (not a ChildView) returns YES when
// NSTexturedBackgroundWindowMask is turned on.  But normally this makes no
// difference.  However, under some (probably very unusual) circumstances
// (and only on Leopard) it *does* make a difference -- for example it
// triggers bmo bugs 431902 and 476393.  So here we make sure that a
// ToolbarWindow's contentView always returns NO from the
// mouseDownCanMoveWindow method.
- (BOOL)nsChildView_NSView_mouseDownCanMoveWindow
{
  NSWindow *ourWindow = [self window];
  NSView *contentView = [ourWindow contentView];
  if ([ourWindow isKindOfClass:[ToolbarWindow class]] && (self == contentView))
    return NO;
  return [self nsChildView_NSView_mouseDownCanMoveWindow];
}

@end
