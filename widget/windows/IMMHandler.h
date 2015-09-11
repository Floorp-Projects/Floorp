/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IMMHandler_h_
#define IMMHandler_h_

#include "nscore.h"
#include <windows.h>
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsIWidget.h"
#include "mozilla/EventForwards.h"
#include "nsRect.h"
#include "WritingModes.h"

class nsWindow;

namespace mozilla {
namespace widget {

struct MSGResult;

class IMEContext final
{
public:
  explicit IMEContext(HWND aWnd);
  explicit IMEContext(nsWindow* aWindow);

  ~IMEContext()
  {
    if (mIMC) {
      ::ImmReleaseContext(mWnd, mIMC);
      mIMC = nullptr;
    }
  }

  HIMC get() const
  {
    return mIMC;
  }

  bool IsValid() const
  {
    return !!mIMC;
  }

  void SetOpenState(bool aOpen) const
  {
    if (!mIMC) {
      return;
    }
    ::ImmSetOpenStatus(mIMC, aOpen);
  }

  bool GetOpenState() const
  {
    if (!mIMC) {
      return false;
    }
    return (::ImmGetOpenStatus(mIMC) != FALSE);
  }

  bool AssociateDefaultContext()
  {
    // We assume that there is only default IMC, no new IMC has been created.
    if (mIMC) {
      return false;
    }
    if (!::ImmAssociateContextEx(mWnd, nullptr, IACE_DEFAULT)) {
      return false;
    }
    mIMC = ::ImmGetContext(mWnd);
    return (mIMC != nullptr);
  }

  bool Disassociate()
  {
    if (!mIMC) {
      return false;
    }
    if (!::ImmAssociateContextEx(mWnd, nullptr, 0)) {
      return false;
    }
    ::ImmReleaseContext(mWnd, mIMC);
    mIMC = nullptr;
    return true;
  }

protected:
  IMEContext()
  {
    MOZ_CRASH("Don't create IMEContext without window handle");
  }

  IMEContext(const IMEContext& aOther)
  {
    MOZ_CRASH("Don't copy IMEContext");
  }

  HWND mWnd;
  HIMC mIMC;
};

class IMMHandler final
{
public:
  static void Initialize();
  static void Terminate();

  // If Process*() returns true, the caller shouldn't do anything anymore.
  static bool ProcessMessage(nsWindow* aWindow, UINT msg,
                             WPARAM& wParam, LPARAM& lParam,
                             MSGResult& aResult);
  static bool IsComposing()
  {
    return IsComposingOnOurEditor() || IsComposingOnPlugin();
  }
  static bool IsComposingOn(nsWindow* aWindow)
  {
    return IsComposing() && IsComposingWindow(aWindow);
  }

#ifdef DEBUG
  /**
   * IsIMEAvailable() returns TRUE when current keyboard layout has IME.
   * Otherwise, FALSE.
   */
  static bool IsIMEAvailable() { return !!::ImmIsIME(::GetKeyboardLayout(0)); }
#endif

  // If aForce is TRUE, these methods doesn't check whether we have composition
  // or not.  If you don't set it to TRUE, these method doesn't commit/cancel
  // the composition on uexpected window.
  static void CommitComposition(nsWindow* aWindow, bool aForce = false);
  static void CancelComposition(nsWindow* aWindow, bool aForce = false);
  static void OnFocusChange(bool aFocus, nsWindow* aWindow);
  static void OnUpdateComposition(nsWindow* aWindow);
  static void OnSelectionChange(nsWindow* aWindow,
                                const IMENotification& aIMENotification,
                                bool aIsIMMActive);

  static nsIMEUpdatePreference GetIMEUpdatePreference();

  // Returns NS_SUCCESS_EVENT_CONSUMED if the mouse button event is consumed by
  // IME.  Otherwise, NS_OK.
  static nsresult OnMouseButtonEvent(nsWindow* aWindow,
                                     const IMENotification& aIMENotification);

protected:
  static void EnsureHandlerInstance();

  static bool IsComposingOnOurEditor();
  static bool IsComposingOnPlugin();
  static bool IsComposingWindow(nsWindow* aWindow);

  static bool IsJapanist2003Active();
  static bool IsGoogleJapaneseInputActive();

  static bool ShouldDrawCompositionStringOurselves();
  static bool IsVerticalWritingSupported();
  // aWindow can be nullptr if it's called without receiving WM_INPUTLANGCHANGE.
  static void InitKeyboardLayout(nsWindow* aWindow, HKL aKeyboardLayout);
  static UINT GetKeyboardCodePage();

  /**
   * Checks whether the window is top level window of the composing window.
   * In this method, the top level window means in all windows, not only in all
   * OUR windows.  I.e., if the aWindow is embedded, this always returns FALSE.
   */
  static bool IsTopLevelWindowOfComposition(nsWindow* aWindow);

  static bool ProcessInputLangChangeMessage(nsWindow* aWindow,
                                              WPARAM wParam,
                                              LPARAM lParam,
                                              MSGResult& aResult);
  static bool ProcessMessageForPlugin(nsWindow* aWindow, UINT msg,
                                        WPARAM &wParam, LPARAM &lParam,
                                        MSGResult& aResult);

  IMMHandler();
  ~IMMHandler();

  // On*() methods return true if the caller of message handler shouldn't do
  // anything anymore.  Otherwise, false.
  static bool OnKeyDownEvent(nsWindow* aWindow, WPARAM wParam, LPARAM lParam,
                             MSGResult& aResult);

  bool OnIMEStartComposition(nsWindow* aWindow, MSGResult& aResult);
  bool OnIMEStartCompositionOnPlugin(nsWindow* aWindow,
                                     WPARAM wParam, LPARAM lParam,
                                     MSGResult& aResult);
  bool OnIMEComposition(nsWindow* aWindow, WPARAM wParam, LPARAM lParam,
                        MSGResult& aResult);
  bool OnIMECompositionOnPlugin(nsWindow* aWindow, WPARAM wParam, LPARAM lParam,
                                MSGResult& aResult);
  bool OnIMEEndComposition(nsWindow* aWindow, MSGResult& aResult);
  bool OnIMEEndCompositionOnPlugin(nsWindow* aWindow, WPARAM wParam,
                                   LPARAM lParam, MSGResult& aResult);
  bool OnIMERequest(nsWindow* aWindow, WPARAM wParam, LPARAM lParam,
                    MSGResult& aResult);
  bool OnIMECharOnPlugin(nsWindow* aWindow, WPARAM wParam, LPARAM lParam,
                         MSGResult& aResult);
  bool OnChar(nsWindow* aWindow, WPARAM wParam, LPARAM lParam,
              MSGResult& aResult);
  bool OnCharOnPlugin(nsWindow* aWindow, WPARAM wParam, LPARAM lParam,
                      MSGResult& aResult);
  void OnInputLangChange(nsWindow* aWindow, WPARAM wParam, LPARAM lParam,
                         MSGResult& aResult);

  // These message handlers don't use instance members, we should not create
  // the instance by the messages.  So, they should be static.
  static bool OnIMEChar(nsWindow* aWindow, WPARAM wParam, LPARAM lParam,
                        MSGResult& aResult);
  static bool OnIMESetContext(nsWindow* aWindow, WPARAM wParam, LPARAM lParam,
                              MSGResult& aResult);
  static bool OnIMESetContextOnPlugin(nsWindow* aWindow,
                                      WPARAM wParam, LPARAM lParam,
                                      MSGResult& aResult);
  static bool OnIMECompositionFull(nsWindow* aWindow, MSGResult& aResult);
  static bool OnIMENotify(nsWindow* aWindow, WPARAM wParam, LPARAM lParam,
                          MSGResult& aResult);
  static bool OnIMESelect(nsWindow* aWindow, WPARAM wParam, LPARAM lParam,
                          MSGResult& aResult);

  // The result of Handle* method mean "Processed" when it's TRUE.
  void HandleStartComposition(nsWindow* aWindow,
                              const IMEContext& aContext);
  bool HandleComposition(nsWindow* aWindow,
                         const IMEContext& aContext,
                         LPARAM lParam);
  // If aCommitString is null, this commits composition with the latest
  // dispatched data.  Otherwise, commits composition with the value.
  void HandleEndComposition(nsWindow* aWindow,
                            const nsAString* aCommitString = nullptr);
  bool HandleReconvert(nsWindow* aWindow, LPARAM lParam, LRESULT *oResult);
  bool HandleQueryCharPosition(nsWindow* aWindow, LPARAM lParam,
                                 LRESULT *oResult);
  bool HandleDocumentFeed(nsWindow* aWindow, LPARAM lParam, LRESULT *oResult);

  /**
   *  When a window's IME context is activating but we have composition on
   *  another window, we should commit our composition because IME context is
   *  shared by all our windows (including plug-ins).
   *  @param aWindow is a new activated window.
   *  If aWindow is our composing window, this method does nothing.
   *  Otherwise, this commits the composition on the previous window.
   *  If this method did commit a composition, this returns TRUE.
   */
  bool CommitCompositionOnPreviousWindow(nsWindow* aWindow);

  /**
   *  ResolveIMECaretPos
   *  Convert the caret rect of a composition event to another widget's
   *  coordinate system.
   *
   *  @param aReferenceWidget The origin widget of aCursorRect.
   *                          Typically, this is mReferenceWidget of the
   *                          composing events. If the aCursorRect is in screen
   *                          coordinates, set nullptr.
   *  @param aCursorRect      The cursor rect.
   *  @param aNewOriginWidget aOutRect will be in this widget's coordinates. If
   *                          this is nullptr, aOutRect will be in screen
   *                          coordinates.
   *  @param aOutRect         The converted cursor rect.
   */
  void ResolveIMECaretPos(nsIWidget* aReferenceWidget,
                          nsIntRect& aCursorRect,
                          nsIWidget* aNewOriginWidget,
                          nsIntRect& aOutRect);

  bool ConvertToANSIString(const nsAFlatString& aStr,
                             UINT aCodePage,
                             nsACString& aANSIStr);

  bool SetIMERelatedWindowsPos(nsWindow* aWindow,
                               const IMEContext& aContext);
  void SetIMERelatedWindowsPosOnPlugin(nsWindow* aWindow,
                                       const IMEContext& aContext);
  /**
   * GetCharacterRectOfSelectedTextAt() returns character rect of the offset
   * from the selection start or the start of composition string if there is
   * a composition.
   *
   * @param aWindow         The window which has focus.
   * @param aOffset         Offset from the selection start or the start of
   *                        composition string when there is a composition.
   *                        This must be in the selection range or
   *                        the composition string.
   * @param aCharRect       The result.
   * @param aWritingMode    The writing mode of current selection.  When this
   *                        is nullptr, this assumes that the selection is in
   *                        horizontal writing mode.
   * @return                true if this succeeded to retrieve the rect.
   *                        Otherwise, false.
   */
  bool GetCharacterRectOfSelectedTextAt(
         nsWindow* aWindow,
         uint32_t aOffset,
         nsIntRect& aCharRect,
         mozilla::WritingMode* aWritingMode = nullptr);
  /**
   * GetCaretRect() returns caret rect at current selection start.
   *
   * @param aWindow         The window which has focus.
   * @param aCaretRect      The result.
   * @param aWritingMode    The writing mode of current selection.  When this
   *                        is nullptr, this assumes that the selection is in
   *                        horizontal writing mode.
   * @return                true if this succeeded to retrieve the rect.
   *                        Otherwise, false.
   */
  bool GetCaretRect(nsWindow* aWindow,
                    nsIntRect& aCaretRect,
                    mozilla::WritingMode* aWritingMode = nullptr);
  void GetCompositionString(const IMEContext& aContext,
                            DWORD aIndex,
                            nsAString& aCompositionString) const;

  /**
   * AdjustCompositionFont() makes IME vertical writing mode if it's supported.
   * If aForceUpdate is true, it will update composition font even if writing
   * mode isn't being changed.
   */
  void AdjustCompositionFont(const IMEContext& aContext,
                             const mozilla::WritingMode& aWritingMode,
                             bool aForceUpdate = false);

  /**
   * MaybeAdjustCompositionFont() calls AdjustCompositionFont() when the
   * locale of active IME is CJK.  Note that this creates an instance even
   * when there is no composition but the locale is CJK.
   */
  static void MaybeAdjustCompositionFont(
                nsWindow* aWindow,
                const mozilla::WritingMode& aWritingMode,
                bool aForceUpdate = false);

  /**
   *  Get the current target clause of composition string.
   *  If there are one or more characters whose attribute is ATTR_TARGET_*,
   *  this returns the first character's offset and its length.
   *  Otherwise, e.g., the all characters are ATTR_INPUT, this returns
   *  the composition string range because the all is the current target.
   *
   *  aLength can be null (default), but aOffset must not be null.
   *
   *  The aOffset value is offset in the contents.  So, when you need offset
   *  in the composition string, you need to subtract mCompositionStart from it.
   */
  bool GetTargetClauseRange(uint32_t *aOffset, uint32_t *aLength = nullptr);

  /**
   * DispatchCompositionChangeEvent() dispatches eCompositionChange event
   * with clause information (it'll be retrieved by CreateTextRangeArray()).
   * I.e., this should be called only during composing.  If a composition is
   * being committed, only HandleCompositionEnd() should be called.
   *
   * @param aWindow     The window which has the composition.
   * @param aContext    Native IME context which has the composition.
   */
  void DispatchCompositionChangeEvent(nsWindow* aWindow,
                                      const IMEContext& aContext);
  already_AddRefed<mozilla::TextRangeArray> CreateTextRangeArray();

  nsresult EnsureClauseArray(int32_t aCount);
  nsresult EnsureAttributeArray(int32_t aCount);

  /**
   * When WM_IME_CHAR is received and passed to DefWindowProc, we need to
   * record the messages.  In other words, we should record the messages
   * when we receive WM_IME_CHAR on windowless plug-in (if we have focus,
   * we always eat them).  When focus is moved from a windowless plug-in to
   * our window during composition, WM_IME_CHAR messages were received when
   * the plug-in has focus.  However, WM_CHAR messages are received after the
   * plug-in lost focus.  So, we need to ignore the WM_CHAR messages because
   * they make unexpected text input events on us.
   */
  nsTArray<MSG> mPassedIMEChar;

  bool IsIMECharRecordsEmpty()
  {
    return mPassedIMEChar.IsEmpty();
  }
  void ResetIMECharRecords()
  {
    mPassedIMEChar.Clear();
  }
  void DequeueIMECharRecords(WPARAM &wParam, LPARAM &lParam)
  {
    MSG msg = mPassedIMEChar.ElementAt(0);
    wParam = msg.wParam;
    lParam = msg.lParam;
    mPassedIMEChar.RemoveElementAt(0);
  }
  void EnqueueIMECharRecords(WPARAM wParam, LPARAM lParam)
  {
    MSG msg;
    msg.wParam = wParam;
    msg.lParam = lParam;
    mPassedIMEChar.AppendElement(msg);
  }

  nsWindow* mComposingWindow;
  nsString  mCompositionString;
  InfallibleTArray<uint32_t> mClauseArray;
  InfallibleTArray<uint8_t> mAttributeArray;

  int32_t mCursorPosition;
  uint32_t mCompositionStart;

  struct Selection
  {
    nsString mString;
    uint32_t mOffset;
    mozilla::WritingMode mWritingMode;
    bool mIsValid;

    Selection()
      : mOffset(UINT32_MAX)
      , mIsValid(false)
    {
    }

    void Clear()
    {
      mOffset = UINT32_MAX;
      mIsValid = false;
    }
    uint32_t Length() const { return mString.Length(); }
    bool Collapsed() const { return !Length(); }

    bool IsValid() const;
    bool Update(const IMENotification& aIMENotification);
    bool Init(nsWindow* aWindow);
    bool EnsureValidSelection(nsWindow* aWindow);
  private:
    Selection(const Selection& aOther) = delete;
    void operator =(const Selection& aOther) = delete;
  };
  // mSelection stores the latest selection data only when sHasFocus is true.
  // Don't access mSelection directly.  You should use GetSelection() for
  // getting proper state.
  Selection mSelection;

  Selection& GetSelection()
  {
    // When IME has focus, mSelection is automatically updated by
    // NOTIFY_IME_OF_SELECTION_CHANGE.
    if (sHasFocus) {
      return mSelection;
    }
    // Otherwise, i.e., While IME doesn't have focus, we cannot observe
    // selection changes.  So, in such case, we need to query selection
    // when it's necessary.
    static Selection sTempSelection;
    sTempSelection.Clear();
    return sTempSelection;
  }

  bool mIsComposing;
  bool mIsComposingOnPlugin;
  bool mNativeCaretIsCreated;

  static mozilla::WritingMode sWritingModeOfCompositionFont;
  static nsString sIMEName;
  static UINT sCodePage;
  static DWORD sIMEProperty;
  static DWORD sIMEUIProperty;
  static bool sAssumeVerticalWritingModeNotSupported;
  static bool sHasFocus;
};

} // namespace widget
} // namespace mozilla

#endif // IMMHandler_h_
