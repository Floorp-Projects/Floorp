/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
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
 * Mozilla Japan.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Masayuki Nakano <masayuki@d-toybox.com>
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

#ifndef TextInputHandler_h_
#define TextInputHandler_h_

#include "nsCocoaUtils.h"

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#include "mozView.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsITimer.h"
#include "npapi.h"
#include "nsTArray.h"
#include "nsEvent.h"

struct PRLogModuleInfo;
class nsChildView;
struct nsTextRange;

namespace mozilla {
namespace widget {

/**
 * TISInputSourceWrapper is a wrapper for the TISInputSourceRef.  If we get the
 * TISInputSourceRef from InputSourceID, we need to release the CFArray instance
 * which is returned by TISCreateInputSourceList.  However, when we release the
 * list, we cannot access the TISInputSourceRef.  So, it's not usable, and it
 * may cause the memory leak bugs.  nsTISInputSource automatically releases the
 * list when the instance is destroyed.
 */
class TISInputSourceWrapper
{
public:
  static TISInputSourceWrapper& CurrentKeyboardLayout();

  TISInputSourceWrapper()
  {
    mInputSourceList = nsnull;
    Clear();
  }

  TISInputSourceWrapper(const char* aID)
  {
    mInputSourceList = nsnull;
    InitByInputSourceID(aID);
  }

  TISInputSourceWrapper(SInt32 aLayoutID)
  {
    mInputSourceList = nsnull;
    InitByLayoutID(aLayoutID);
  }

  TISInputSourceWrapper(TISInputSourceRef aInputSource)
  {
    mInputSourceList = nsnull;
    InitByTISInputSourceRef(aInputSource);
  }

  ~TISInputSourceWrapper() { Clear(); }

  void InitByInputSourceID(const char* aID);
  void InitByInputSourceID(const nsAFlatString &aID);
  void InitByInputSourceID(const CFStringRef aID);
  void InitByLayoutID(SInt32 aLayoutID);
  void InitByCurrentInputSource();
  void InitByCurrentKeyboardLayout();
  void InitByCurrentASCIICapableInputSource();
  void InitByCurrentASCIICapableKeyboardLayout();
  void InitByTISInputSourceRef(TISInputSourceRef aInputSource);
  void InitByLanguage(CFStringRef aLanguage);

  const UCKeyboardLayout* GetUCKeyboardLayout();

  PRBool IsOpenedIMEMode();
  PRBool IsIMEMode();

  PRBool IsASCIICapable()
  {
    NS_ENSURE_TRUE(mInputSource, PR_FALSE);
    return GetBoolProperty(kTISPropertyInputSourceIsASCIICapable);
  }

  PRBool IsEnabled()
  {
    NS_ENSURE_TRUE(mInputSource, PR_FALSE);
    return GetBoolProperty(kTISPropertyInputSourceIsEnabled);
  }

  PRBool GetLanguageList(CFArrayRef &aLanguageList);
  PRBool GetPrimaryLanguage(CFStringRef &aPrimaryLanguage);
  PRBool GetPrimaryLanguage(nsAString &aPrimaryLanguage);

  PRBool GetLocalizedName(CFStringRef &aName)
  {
    NS_ENSURE_TRUE(mInputSource, PR_FALSE);
    return GetStringProperty(kTISPropertyLocalizedName, aName);
  }

  PRBool GetLocalizedName(nsAString &aName)
  {
    NS_ENSURE_TRUE(mInputSource, PR_FALSE);
    return GetStringProperty(kTISPropertyLocalizedName, aName);
  }

  PRBool GetInputSourceID(CFStringRef &aID)
  {
    NS_ENSURE_TRUE(mInputSource, PR_FALSE);
    return GetStringProperty(kTISPropertyInputSourceID, aID);
  }

  PRBool GetInputSourceID(nsAString &aID)
  {
    NS_ENSURE_TRUE(mInputSource, PR_FALSE);
    return GetStringProperty(kTISPropertyInputSourceID, aID);
  }

  PRBool GetBundleID(CFStringRef &aBundleID)
  {
    NS_ENSURE_TRUE(mInputSource, PR_FALSE);
    return GetStringProperty(kTISPropertyBundleID, aBundleID);
  }

  PRBool GetBundleID(nsAString &aBundleID)
  {
    NS_ENSURE_TRUE(mInputSource, PR_FALSE);
    return GetStringProperty(kTISPropertyBundleID, aBundleID);
  }

  PRBool GetInputSourceType(CFStringRef &aType)
  {
    NS_ENSURE_TRUE(mInputSource, PR_FALSE);
    return GetStringProperty(kTISPropertyInputSourceType, aType);
  }

  PRBool GetInputSourceType(nsAString &aType)
  {
    NS_ENSURE_TRUE(mInputSource, PR_FALSE);
    return GetStringProperty(kTISPropertyInputSourceType, aType);
  }

  PRBool IsForRTLLanguage();
  PRBool IsInitializedByCurrentKeyboardLayout();

  enum {
    // 40 is an actual result of the ::LMGetKbdType() when we connect an
    // unknown keyboard and set the keyboard type to ANSI manually on the
    // set up dialog.
    eKbdType_ANSI = 40
  };

  PRBool TranslateToString(UInt32 aKeyCode, UInt32 aModifiers, UInt32 aKbdType,
                           nsAString &aStr);

  void Select();
  void Clear();

protected:
  static PRBool UCKeyTranslateToString(const UCKeyboardLayout* aHandle,
                                       UInt32 aKeyCode, UInt32 aModifiers,
                                       UInt32 aKbType, nsAString &aStr);

  PRBool GetBoolProperty(const CFStringRef aKey);
  PRBool GetStringProperty(const CFStringRef aKey, CFStringRef &aStr);
  PRBool GetStringProperty(const CFStringRef aKey, nsAString &aStr);

  TISInputSourceRef mInputSource;
  CFArrayRef mInputSourceList;
  const UCKeyboardLayout* mUCKeyboardLayout;
  PRInt8 mIsRTL;
};

/**
 * TextInputHandlerBase is a base class of PluginTextInputHandler,
 * IMEInputHandler and TextInputHandler.  Utility methods should be implemented
 * this level.
 */

class TextInputHandlerBase
{
public:
  nsrefcnt AddRef()
  {
    NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "mRefCnt is negative");
    ++mRefCnt;
    NS_LOG_ADDREF(this, mRefCnt, "TextInputHandlerBase", sizeof(*this));
    return mRefCnt;
  }
  nsrefcnt Release()
  {
    NS_PRECONDITION(mRefCnt != 0, "mRefCnt is alrady zero");
    --mRefCnt;
    NS_LOG_RELEASE(this, mRefCnt, "TextInputHandlerBase");
    if (mRefCnt == 0) {
        mRefCnt = 1; /* stabilize */
        delete this;
        return 0;
    }
    return mRefCnt;
  }

protected:
  nsAutoRefCnt mRefCnt;

public:
   /**
   * mWidget must not be destroyed without OnDestroyWidget being called.
   *
   * @param aDestroyingWidget     Destroying widget.  This might not be mWidget.
   * @return                      This result doesn't have any meaning for
   *                              callers.  When aDstroyingWidget isn't the same
   *                              as mWidget, FALSE.  Then, inherited methods in
   *                              sub classes should return from this method
   *                              without cleaning up.
   */
  virtual PRBool OnDestroyWidget(nsChildView* aDestroyingWidget);

protected:
  // The creater of this instance and client.
  // This must not be null after initialized until OnDestroyWidget() is called.
  nsChildView* mWidget; // [WEAK]

  // The native view for mWidget.
  // This view handles the actual text inputting.
  NSView<mozView>* mView; // [STRONG]

  TextInputHandlerBase(nsChildView* aWidget, NSView<mozView> *aNativeView);
  virtual ~TextInputHandlerBase();

  PRBool Destroyed() { return !mWidget; }
};

/**
 * PluginTextInputHandler handles text input events for plugins.
 */

class PluginTextInputHandler : public TextInputHandlerBase
{
protected:
  PluginTextInputHandler(nsChildView* aWidget, NSView<mozView> *aNativeView);
  ~PluginTextInputHandler();
};

/**
 * IMEInputHandler manages:
 *   1. The IME/keyboard layout statement of nsChildView.
 *   2. The IME composition statement of nsChildView.
 * And also provides the methods which controls the current IME transaction of
 * the instance.
 *
 * Note that an nsChildView handles one or more NSView's events.  E.g., even if
 * a text editor on XUL panel element, the input events handled on the parent
 * (or its ancestor) widget handles it (the native focus is set to it).  The
 * actual focused view is notified by OnFocusChangeInGecko.
 */

class IMEInputHandler : public PluginTextInputHandler
{
public:
  virtual PRBool OnDestroyWidget(nsChildView* aDestroyingWidget);

  virtual void OnFocusChangeInGecko(PRBool aFocus);

  /**
   * DispatchTextEvent() dispatches a text event on mWidget.
   *
   * @param aText                 User text input.
   * @param aAttrString           An NSAttributedString instance which indicates
   *                              current composition string.
   * @param aSelectedRange        Current selected range (or caret position).
   * @param aDoCommit             TRUE if the composition string should be
   *                              committed.  Otherwise, FALSE.
   */
  PRBool DispatchTextEvent(const nsString& aText,
                           NSAttributedString* aAttrString,
                           NSRange& aSelectedRange,
                           PRBool aDoCommit);

  /**
   * SetMarkedText() is a handler of setMarkedText of NSTextInput.
   *
   * @param aAttrString           This mut be an instance of NSAttributedString.
   *                              If the aString parameter to
   *                              [ChildView setMarkedText:setSelectedRange:]
   *                              isn't an instance of NSAttributedString,
   *                              create an NSAttributedString from it and pass
   *                              that instead.
   * @param aSelectedRange        Current selected range (or caret position).
   */
  void SetMarkedText(NSAttributedString* aAttrString,
                     NSRange& aSelectedRange);

  /**
   * InsertTextAsCommittingComposition() commits current composition.  If there
   * is no composition, this starts a composition and commits it immediately.
   *
   * @param aAttrString           A string which is committed.
   */
  void InsertTextAsCommittingComposition(NSAttributedString* aAttrString);

  /**
   * ConversationIdentifier() returns an ID for the current editor.  The ID is
   * guaranteed to be unique among currently existing editors.  But it might be
   * the same as the ID of an editor that has already been destroyed.
   *
   * @return                      An identifier of current focused editor.
   */
  NSInteger ConversationIdentifier();

  /**
   * GetAttributedSubstringFromRange() returns an NSAttributedString instance
   * which is allocated as autorelease for aRange.
   *
   * @param aRange                The range of string which you want.
   * @return                      The string in aRange.  If the string is empty,
   *                              this returns nil.  If succeeded, this returns
   *                              an instance which is allocated as autorelease.
   *                              If this has some troubles, returns nil.
   */
  NSAttributedString* GetAttributedSubstringFromRange(NSRange& aRange);

  /**
   * SelectedRange() returns current selected range.
   *
   * @return                      If an editor has focus, this returns selection
   *                              range in the editor.  Otherwise, this returns
   *                              selection range  in the focused document.
   */
  NSRange SelectedRange();

  /**
   * FirstRectForCharacterRange() returns first *character* rect in the range.
   * Cocoa needs the first line rect in the range, but we cannot compute it
   * on current implementation.
   *
   * @param aRange                A range of text to examine.  Its position is
   *                              an offset from the beginning of the focused
   *                              editor or document.
   * @return                      An NSRect containing the first character in
   *                              aRange, in screen coordinates.
   *                              If the length of aRange is 0, the width will
   *                              be 0.
   */
  NSRect FirstRectForCharacterRange(NSRange& aRange);

  /**
   * CharacterIndexForPoint() returns an offset of a character at aPoint.
   * XXX This isn't implemented, always returns 0.
   *
   * @param                       The point in screen coordinates.
   * @return                      The offset of the character at aPoint from
   *                              the beginning of the focused editor or
   *                              document.
   */
  NSUInteger CharacterIndexForPoint(NSPoint& aPoint);

  /**
   * GetValidAttributesForMarkedText() returns attributes which we support.
   *
   * @return                      Always empty array for now.
   */
  NSArray* GetValidAttributesForMarkedText();

  PRBool HasMarkedText()
  {
    return (mMarkedRange.location != NSNotFound) && (mMarkedRange.length != 0);
  }

  NSRange MarkedRange()
  {
    if (!HasMarkedText()) {
      return NSMakeRange(NSNotFound, 0);
    }
    return mMarkedRange;
  }

  PRBool IsIMEComposing() { return mIsIMEComposing; }
  PRBool IsIMEOpened();
  PRBool IsIMEEnabled() { return mIsIMEEnabled; }
  PRBool IsASCIICapableOnly() { return mIsASCIICapableOnly; }
  PRBool IgnoreIMECommit() { return mIgnoreIMECommit; }

  PRBool IgnoreIMEComposition()
  {
    // Ignore the IME composition events when we're pending to discard the
    // composition and we are not to handle the IME composition now.
    return (mPendingMethods & kDiscardIMEComposition) &&
           (mIsInFocusProcessing || !IsFocused());
  }

  void CommitIMEComposition();
  void CancelIMEComposition();

  void EnableIME(PRBool aEnableIME);
  void SetIMEOpenState(PRBool aOpen);
  void SetASCIICapableOnly(PRBool aASCIICapableOnly);

  static CFArrayRef CreateAllIMEModeList();
  static void DebugPrintAllIMEModes(PRLogModuleInfo* aLogModuleInfo);

  // Don't use ::TSMGetActiveDocument() API directly, the document may not
  // be what you want.
  static TSMDocumentID GetCurrentTSMDocumentID();

protected:
  // We cannot do some jobs in the given stack by some reasons.
  // Following flags and the timer provide the execution pending mechanism,
  // See the comment in nsCocoaTextInputHandler.mm.
  nsCOMPtr<nsITimer> mTimer;
  enum {
    kResetIMEWindowLevel     = 1,
    kDiscardIMEComposition   = 2,
    kSyncASCIICapableOnly    = 4
  };
  PRUint32 mPendingMethods;

  IMEInputHandler(nsChildView* aWidget, NSView<mozView> *aNativeView);
  virtual ~IMEInputHandler();

  PRBool IsFocused();
  void ResetTimer();

  virtual void ExecutePendingMethods();

private:
  // If mIsIMEComposing is true, the composition string is stored here.
  NSString* mIMECompositionString;

  NSRange mMarkedRange;

  PRPackedBool mIsIMEComposing;
  PRPackedBool mIsIMEEnabled;
  PRPackedBool mIsASCIICapableOnly;
  PRPackedBool mIgnoreIMECommit;
  // This flag is enabled by OnFocusChangeInGecko, and will be cleared by
  // ExecutePendingMethods.  When this is true, IsFocus() returns TRUE.  At
  // that time, the focus processing in Gecko might not be finished yet.  So,
  // you cannot use nsQueryContentEvent or something.
  PRPackedBool mIsInFocusProcessing;

  void KillIMEComposition();
  void SendCommittedText(NSString *aString);
  void OpenSystemPreferredLanguageIME();

  // Pending methods
  void ResetIMEWindowLevel();
  void DiscardIMEComposition();
  void SyncASCIICapableOnly();

  static PRBool sStaticMembersInitialized;
  static CFStringRef sLatestIMEOpenedModeInputSourceID;
  static void InitStaticMembers();
  static void OnCurrentTextInputSourceChange(CFNotificationCenterRef aCenter,
                                             void* aObserver,
                                             CFStringRef aName,
                                             const void* aObject,
                                             CFDictionaryRef aUserInfo);

  static void FlushPendingMethods(nsITimer* aTimer, void* aClosure);

  /**
   * ConvertToTextRangeStyle converts the given native underline style to
   * our defined text range type.
   *
   * @param aUnderlineStyle       NSUnderlineStyleSingle or
   *                              NSUnderlineStyleThick.
   * @param aSelectedRange        Current selected range (or caret position).
   * @return                      NS_TEXTRANGE_*.
   */
  PRUint32 ConvertToTextRangeType(PRUint32 aUnderlineStyle,
                                  NSRange& aSelectedRange);

  /**
   * GetRangeCount() computes the range count of aAttrString.
   *
   * @param aAttrString           An NSAttributedString instance whose number of
   *                              NSUnderlineStyleAttributeName ranges you with
   *                              to know.
   * @return                      The count of NSUnderlineStyleAttributeName
   *                              ranges in aAttrString.
   */
  PRUint32 GetRangeCount(NSAttributedString *aString);

  /**
   * SetTextRangeList() appends text ranges to aTextRangeList.
   *
   * @param aTextRangeList        When SetTextRangeList() returns, this will
   *                              be set to the NSUnderlineStyleAttributeName
   *                              ranges in aAttrString.  Note that if you pass
   *                              in a large enough auto-range instance for most
   *                              cases (e.g., nsAutoTArray<nsTextRange, 4>),
   *                              it prevents memory fragmentation.
   * @param aAttrString           An NSAttributedString instance which indicates
   *                              current composition string.
   * @param aSelectedRange        Current selected range (or caret position).
   */
  void SetTextRangeList(nsTArray<nsTextRange>& aTextRangeList,
                        NSAttributedString *aAttrString,
                        NSRange& aSelectedRange);

  /**
   * InitCompositionEvent() initializes aCompositionEvent.
   *
   * @param aCompositionEvent     A composition event which you want to
   *                              initialize.
   */
  void InitCompositionEvent(nsCompositionEvent& aCompositionEvent);

  /**
   * When a composition starts, OnStartIMEComposition() is called.
   */
  void OnStartIMEComposition();

  /**
   * When a composition is updated, OnUpdateIMEComposition() is called.
   */
  void OnUpdateIMEComposition(NSString* aIMECompositionString);

  /**
   * When a composition is finished, OnEndIMEComposition() is called.
   */
  void OnEndIMEComposition();

  // The focused IME handler.  Please note that the handler might lost the
  // actual focus by deactivating the application.  If we are active, this
  // must have the actual focused handle.
  // We cannot access to the NSInputManager during we aren't active, so, the
  // focused handler can have an IME transaction even if we are deactive.
  static IMEInputHandler* sFocusedIMEHandler;
};

/**
 * TextInputHandler implements the NSTextInput protocol.
 */
class TextInputHandler : public IMEInputHandler
{
public:
  static CFArrayRef CreateAllKeyboardLayoutList();
  static void DebugPrintAllKeyboardLayouts(PRLogModuleInfo* aLogModuleInfo);

  TextInputHandler(nsChildView* aWidget, NSView<mozView> *aNativeView);
  virtual ~TextInputHandler();
};

} // namespace widget
} // namespace mozilla

#endif // TextInputHandler_h_
