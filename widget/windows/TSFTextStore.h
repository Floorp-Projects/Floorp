/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TSFTextStore_h_
#define TSFTextStore_h_

#include "nsCOMPtr.h"
#include "nsIWidget.h"
#include "nsString.h"
#include "nsWindowBase.h"

#include "WinUtils.h"
#include "WritingModes.h"

#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TextEventDispatcher.h"
#include "mozilla/TextRange.h"
#include "mozilla/WindowsVersion.h"

#include <msctf.h>
#include <textstor.h>

// GUID_PROP_INPUTSCOPE is declared in inputscope.h using INIT_GUID.
// With initguid.h, we get its instance instead of extern declaration.
#ifdef INPUTSCOPE_INIT_GUID
#include <initguid.h>
#endif
#ifdef TEXTATTRS_INIT_GUID
#include <tsattrs.h>
#endif
#include <inputscope.h>

// TSF InputScope, for earlier SDK 8
#define IS_SEARCH static_cast<InputScope>(50)

struct ITfThreadMgr;
struct ITfDocumentMgr;
struct ITfDisplayAttributeMgr;
struct ITfCategoryMgr;
class nsWindow;

namespace mozilla {
namespace widget {

class TSFStaticSink;
struct MSGResult;

/*
 * Text Services Framework text store
 */

class TSFTextStore final : public ITextStoreACP
                         , public ITfContextOwnerCompositionSink
                         , public ITfMouseTrackerACP
{
  friend class TSFStaticSink;
private:
  typedef IMENotification::SelectionChangeDataBase SelectionChangeDataBase;
  typedef IMENotification::SelectionChangeData SelectionChangeData;
  typedef IMENotification::TextChangeDataBase TextChangeDataBase;
  typedef IMENotification::TextChangeData TextChangeData;

public: /*IUnknown*/
  STDMETHODIMP          QueryInterface(REFIID, void**);

  NS_INLINE_DECL_IUNKNOWN_REFCOUNTING(TSFTextStore)

public: /*ITextStoreACP*/
  STDMETHODIMP AdviseSink(REFIID, IUnknown*, DWORD);
  STDMETHODIMP UnadviseSink(IUnknown*);
  STDMETHODIMP RequestLock(DWORD, HRESULT*);
  STDMETHODIMP GetStatus(TS_STATUS*);
  STDMETHODIMP QueryInsert(LONG, LONG, ULONG, LONG*, LONG*);
  STDMETHODIMP GetSelection(ULONG, ULONG, TS_SELECTION_ACP*, ULONG*);
  STDMETHODIMP SetSelection(ULONG, const TS_SELECTION_ACP*);
  STDMETHODIMP GetText(LONG, LONG, WCHAR*, ULONG, ULONG*, TS_RUNINFO*, ULONG,
                       ULONG*, LONG*);
  STDMETHODIMP SetText(DWORD, LONG, LONG, const WCHAR*, ULONG, TS_TEXTCHANGE*);
  STDMETHODIMP GetFormattedText(LONG, LONG, IDataObject**);
  STDMETHODIMP GetEmbedded(LONG, REFGUID, REFIID, IUnknown**);
  STDMETHODIMP QueryInsertEmbedded(const GUID*, const FORMATETC*, BOOL*);
  STDMETHODIMP InsertEmbedded(DWORD, LONG, LONG, IDataObject*, TS_TEXTCHANGE*);
  STDMETHODIMP RequestSupportedAttrs(DWORD, ULONG, const TS_ATTRID*);
  STDMETHODIMP RequestAttrsAtPosition(LONG, ULONG, const TS_ATTRID*, DWORD);
  STDMETHODIMP RequestAttrsTransitioningAtPosition(LONG, ULONG,
                                                   const TS_ATTRID*, DWORD);
  STDMETHODIMP FindNextAttrTransition(LONG, LONG, ULONG, const TS_ATTRID*,
                                      DWORD, LONG*, BOOL*, LONG*);
  STDMETHODIMP RetrieveRequestedAttrs(ULONG, TS_ATTRVAL*, ULONG*);
  STDMETHODIMP GetEndACP(LONG*);
  STDMETHODIMP GetActiveView(TsViewCookie*);
  STDMETHODIMP GetACPFromPoint(TsViewCookie, const POINT*, DWORD, LONG*);
  STDMETHODIMP GetTextExt(TsViewCookie, LONG, LONG, RECT*, BOOL*);
  STDMETHODIMP GetScreenExt(TsViewCookie, RECT*);
  STDMETHODIMP GetWnd(TsViewCookie, HWND*);
  STDMETHODIMP InsertTextAtSelection(DWORD, const WCHAR*, ULONG, LONG*, LONG*,
                                     TS_TEXTCHANGE*);
  STDMETHODIMP InsertEmbeddedAtSelection(DWORD, IDataObject*, LONG*, LONG*,
                                         TS_TEXTCHANGE*);

public: /*ITfContextOwnerCompositionSink*/
  STDMETHODIMP OnStartComposition(ITfCompositionView*, BOOL*);
  STDMETHODIMP OnUpdateComposition(ITfCompositionView*, ITfRange*);
  STDMETHODIMP OnEndComposition(ITfCompositionView*);

public: /*ITfMouseTrackerACP*/
  STDMETHODIMP AdviseMouseSink(ITfRangeACP*, ITfMouseSink*, DWORD*);
  STDMETHODIMP UnadviseMouseSink(DWORD);

public:
  static void     Initialize(void);
  static void     Terminate(void);

  static bool     ProcessRawKeyMessage(const MSG& aMsg);
  static void     ProcessMessage(nsWindowBase* aWindow, UINT aMessage,
                                 WPARAM& aWParam, LPARAM& aLParam,
                                 MSGResult& aResult);

  static void     SetIMEOpenState(bool);
  static bool     GetIMEOpenState(void);

  static void     CommitComposition(bool aDiscard)
  {
    NS_ASSERTION(IsInTSFMode(), "Not in TSF mode, shouldn't be called");
    if (!sEnabledTextStore) {
      return;
    }
    RefPtr<TSFTextStore> textStore(sEnabledTextStore);
    textStore->CommitCompositionInternal(aDiscard);
  }

  static void SetInputContext(nsWindowBase* aWidget,
                              const InputContext& aContext,
                              const InputContextAction& aAction);

  static nsresult OnFocusChange(bool aGotFocus,
                                nsWindowBase* aFocusedWidget,
                                const InputContext& aContext);
  static nsresult OnTextChange(const IMENotification& aIMENotification)
  {
    NS_ASSERTION(IsInTSFMode(), "Not in TSF mode, shouldn't be called");
    if (!sEnabledTextStore) {
      return NS_OK;
    }
    RefPtr<TSFTextStore> textStore(sEnabledTextStore);
    return textStore->OnTextChangeInternal(aIMENotification);
  }

  static nsresult OnSelectionChange(const IMENotification& aIMENotification)
  {
    NS_ASSERTION(IsInTSFMode(), "Not in TSF mode, shouldn't be called");
    if (!sEnabledTextStore) {
      return NS_OK;
    }
    RefPtr<TSFTextStore> textStore(sEnabledTextStore);
    return textStore->OnSelectionChangeInternal(aIMENotification);
  }

  static nsresult OnLayoutChange()
  {
    NS_ASSERTION(IsInTSFMode(), "Not in TSF mode, shouldn't be called");
    if (!sEnabledTextStore) {
      return NS_OK;
    }
    RefPtr<TSFTextStore> textStore(sEnabledTextStore);
    return textStore->OnLayoutChangeInternal();
  }

  static nsresult OnUpdateComposition()
  {
    NS_ASSERTION(IsInTSFMode(), "Not in TSF mode, shouldn't be called");
    if (!sEnabledTextStore) {
      return NS_OK;
    }
    RefPtr<TSFTextStore> textStore(sEnabledTextStore);
    return textStore->OnUpdateCompositionInternal();
  }

  static nsresult OnMouseButtonEvent(const IMENotification& aIMENotification)
  {
    NS_ASSERTION(IsInTSFMode(), "Not in TSF mode, shouldn't be called");
    if (!sEnabledTextStore) {
      return NS_OK;
    }
    RefPtr<TSFTextStore> textStore(sEnabledTextStore);
    return textStore->OnMouseButtonEventInternal(aIMENotification);
  }

  static IMENotificationRequests GetIMENotificationRequests();

  // Returns the address of the pointer so that the TSF automatic test can
  // replace the system object with a custom implementation for testing.
  // XXX TSF doesn't work now.  Should we remove it?
  static void* GetNativeData(uint32_t aDataType)
  {
    switch (aDataType) {
      case NS_NATIVE_TSF_THREAD_MGR:
        Initialize(); // Apply any previous changes
        return static_cast<void*>(&sThreadMgr);
      case NS_NATIVE_TSF_CATEGORY_MGR:
        return static_cast<void*>(&sCategoryMgr);
      case NS_NATIVE_TSF_DISPLAY_ATTR_MGR:
        return static_cast<void*>(&sDisplayAttrMgr);
      default:
        return nullptr;
    }
  }

  static void* GetThreadManager()
  {
    return static_cast<void*>(sThreadMgr);
  }

  static bool ThinksHavingFocus()
  {
    return (sEnabledTextStore && sEnabledTextStore->mContext);
  }

  static bool IsInTSFMode()
  {
    return sThreadMgr != nullptr;
  }

  static bool IsComposing()
  {
    return (sEnabledTextStore && sEnabledTextStore->mComposition.IsComposing());
  }

  static bool IsComposingOn(nsWindowBase* aWidget)
  {
    return (IsComposing() && sEnabledTextStore->mWidget == aWidget);
  }

  static nsWindowBase* GetEnabledWindowBase()
  {
    return sEnabledTextStore ? sEnabledTextStore->mWidget.get() : nullptr;
  }

  /**
   * Returns true if active keyboard layout is a legacy IMM-IME.
   */
  static bool IsIMM_IMEActive();

  /**
   * Returns true if active TIP is MS-IME for Japanese.
   */
  static bool IsMSJapaneseIMEActive();

  /**
   * Returns true if active TIP is Google Japanese Input.
   * Note that if Google Japanese Input is installed as an IMM-IME,
   * this return false even if Google Japanese Input is active.
   * So, you may need to check IMMHandler::IsGoogleJapaneseInputActive() too.
   */
  static bool IsGoogleJapaneseInputActive();

  /**
   * Returns true if active TIP or IME is a black listed one and we should
   * set input scope of URL bar to IS_DEFAULT rather than IS_URL.
   */
  static bool ShouldSetInputScopeOfURLBarToDefault();

  /**
   * Returns true if TSF may crash if GetSelection() returns E_FAIL.
   */
  static bool DoNotReturnErrorFromGetSelection();

#ifdef DEBUG
  // Returns true when keyboard layout has IME (TIP).
  static bool     CurrentKeyboardLayoutHasIME();
#endif // #ifdef DEBUG

protected:
  TSFTextStore();
  ~TSFTextStore();

  static bool CreateAndSetFocus(nsWindowBase* aFocusedWidget,
                                const InputContext& aContext);
  static void EnsureToDestroyAndReleaseEnabledTextStoreIf(
                RefPtr<TSFTextStore>& aTextStore);
  static void MarkContextAsKeyboardDisabled(ITfContext* aContext);
  static void MarkContextAsEmpty(ITfContext* aContext);

  bool     Init(nsWindowBase* aWidget, const InputContext& aContext);
  void     Destroy();
  void     ReleaseTSFObjects();

  bool     IsReadLock(DWORD aLock) const
  {
    return (TS_LF_READ == (aLock & TS_LF_READ));
  }
  bool     IsReadWriteLock(DWORD aLock) const
  {
    return (TS_LF_READWRITE == (aLock & TS_LF_READWRITE));
  }
  bool     IsReadLocked() const { return IsReadLock(mLock); }
  bool     IsReadWriteLocked() const { return IsReadWriteLock(mLock); }

  // This is called immediately after a call of OnLockGranted() of mSink.
  // Note that mLock isn't cleared yet when this is called.
  void     DidLockGranted();

  bool     GetScreenExtInternal(RECT& aScreenExt);
  // If aDispatchCompositionChangeEvent is true, this method will dispatch
  // compositionchange event if this is called during IME composing.
  // aDispatchCompositionChangeEvent should be true only when this is called
  // from SetSelection.  Because otherwise, the compositionchange event should
  // not be sent from here.
  HRESULT  SetSelectionInternal(const TS_SELECTION_ACP*,
                                bool aDispatchCompositionChangeEvent = false);
  bool     InsertTextAtSelectionInternal(const nsAString& aInsertStr,
                                         TS_TEXTCHANGE* aTextChange);
  void     CommitCompositionInternal(bool);
  HRESULT  GetDisplayAttribute(ITfProperty* aProperty,
                               ITfRange* aRange,
                               TF_DISPLAYATTRIBUTE* aResult);
  HRESULT  RestartCompositionIfNecessary(ITfRange* pRangeNew = nullptr);
  HRESULT  RestartComposition(ITfCompositionView* aCompositionView,
                              ITfRange* aNewRange);

  // Following methods record composing action(s) to mPendingActions.
  // They will be flushed FlushPendingActions().
  HRESULT  RecordCompositionStartAction(ITfCompositionView* aCompositionView,
                                        ITfRange* aRange,
                                        bool aPreserveSelection);
  HRESULT  RecordCompositionStartAction(ITfCompositionView* aComposition,
                                        LONG aStart,
                                        LONG aLength,
                                        bool aPreserveSelection);
  HRESULT  RecordCompositionUpdateAction();
  HRESULT  RecordCompositionEndAction();

  // DispatchEvent() dispatches the event and if it may not be handled
  // synchronously, this makes the instance not notify TSF of pending
  // notifications until next notification from content.
  void     DispatchEvent(WidgetGUIEvent& aEvent);
  void     OnLayoutInformationAvaliable();

  // FlushPendingActions() performs pending actions recorded in mPendingActions
  // and clear it.
  void     FlushPendingActions();
  // MaybeFlushPendingNotifications() performs pending notifications to TSF.
  void     MaybeFlushPendingNotifications();

  nsresult OnTextChangeInternal(const IMENotification& aIMENotification);
  nsresult OnSelectionChangeInternal(const IMENotification& aIMENotification);
  nsresult OnMouseButtonEventInternal(const IMENotification& aIMENotification);
  nsresult OnLayoutChangeInternal();
  nsresult OnUpdateCompositionInternal();

  // mPendingSelectionChangeData stores selection change data until notifying
  // TSF of selection change.  If two or more selection changes occur, this
  // stores the latest selection change data because only it is necessary.
  SelectionChangeData mPendingSelectionChangeData;

  // mPendingTextChangeData stores one or more text change data until notifying
  // TSF of text change.  If two or more text changes occur, this merges
  // every text change data.
  TextChangeData mPendingTextChangeData;

  void     NotifyTSFOfTextChange();
  void     NotifyTSFOfSelectionChange();
  bool     NotifyTSFOfLayoutChange();
  void     NotifyTSFOfLayoutChangeAgain();

  HRESULT  HandleRequestAttrs(DWORD aFlags,
                              ULONG aFilterCount,
                              const TS_ATTRID* aFilterAttrs);
  void     SetInputScope(const nsString& aHTMLInputType,
                         const nsString& aHTMLInputInputmode);

  // Creates native caret over our caret.  This method only works on desktop
  // application.  Otherwise, this does nothing.
  void     CreateNativeCaret();
  // Destroys native caret if there is.
  void     MaybeDestroyNativeCaret();

  // Holds the pointer to our current win32 widget
  RefPtr<nsWindowBase>       mWidget;
  // mDispatcher is a helper class to dispatch composition events.
  RefPtr<TextEventDispatcher> mDispatcher;
  // Document manager for the currently focused editor
  RefPtr<ITfDocumentMgr>     mDocumentMgr;
  // Edit cookie associated with the current editing context
  DWORD                        mEditCookie;
  // Editing context at the bottom of mDocumentMgr's context stack
  RefPtr<ITfContext>         mContext;
  // Currently installed notification sink
  RefPtr<ITextStoreACPSink>  mSink;
  // TS_AS_* mask of what events to notify
  DWORD                        mSinkMask;
  // 0 if not locked, otherwise TS_LF_* indicating the current lock
  DWORD                        mLock;
  // 0 if no lock is queued, otherwise TS_LF_* indicating the queue lock
  DWORD                        mLockQueued;

  uint32_t mHandlingKeyMessage;
  void OnStartToHandleKeyMessage() { ++mHandlingKeyMessage; }
  void OnEndHandlingKeyMessage()
  {
    MOZ_ASSERT(mHandlingKeyMessage);
    if (--mHandlingKeyMessage) {
      return;
    }
    // If TSFTextStore instance is destroyed during handling key message(s),
    // release all TSF objects when all nested key messages have been handled.
    if (mDestroyed) {
      ReleaseTSFObjects();
    }
  }

  class Composition final
  {
  public:
    // nullptr if no composition is active, otherwise the current composition
    RefPtr<ITfCompositionView> mView;

    // Current copy of the active composition string. Only mString is
    // changed during a InsertTextAtSelection call if we have a composition.
    // mString acts as a buffer until OnUpdateComposition is called
    // and mString is flushed to editor through eCompositionChange.
    // This way all changes are updated in batches to avoid
    // inconsistencies/artifacts.
    nsString mString;

    // The start of the current active composition, in ACP offsets
    LONG mStart;

    bool IsComposing() const
    {
      return (mView != nullptr);
    }

    LONG EndOffset() const
    {
      return mStart + static_cast<LONG>(mString.Length());
    }

    // Start() and End() updates the members for emulating the latest state.
    // Unless flush the pending actions, this data never matches the actual
    // content.
    void Start(ITfCompositionView* aCompositionView,
               LONG aCompositionStartOffset,
               const nsAString& aCompositionString);
    void End();
  };
  // While the document is locked, we cannot dispatch any events which cause
  // DOM events since the DOM events' handlers may modify the locked document.
  // However, even while the document is locked, TSF may queries us.
  // For that, TSFTextStore modifies mComposition even while the document is
  // locked.  With mComposition, query methods can returns the text content
  // information.
  Composition mComposition;

  /**
   * IsHandlingComposition() returns true if there is a composition in the
   * focused editor.
   */
  bool IsHandlingComposition() const
  {
    return mDispatcher && mDispatcher->IsHandlingComposition();
  }

  class Selection
  {
  public:
    Selection() : mDirty(true) {}

    bool IsDirty() const { return mDirty; };
    void MarkDirty() { mDirty = true; }

    TS_SELECTION_ACP& ACP()
    {
      MOZ_ASSERT(!mDirty);
      return mACP;
    }

    void SetSelection(const TS_SELECTION_ACP& aSelection)
    {
      mDirty = false;
      mACP = aSelection;
      // Selection end must be active in our editor.
      if (mACP.style.ase != TS_AE_START) {
        mACP.style.ase = TS_AE_END;
      }
      // We're not support interim char selection for now.
      // XXX Probably, this is necessary for supporting South Asian languages.
      mACP.style.fInterimChar = FALSE;
    }

    bool SetSelection(uint32_t aStart,
                      uint32_t aLength,
                      bool aReversed,
                      WritingMode aWritingMode)
    {
      bool changed = mDirty ||
                     mACP.acpStart != static_cast<LONG>(aStart) ||
                     mACP.acpEnd != static_cast<LONG>(aStart + aLength);

      mDirty = false;
      mACP.acpStart = static_cast<LONG>(aStart);
      mACP.acpEnd = static_cast<LONG>(aStart + aLength);
      mACP.style.ase = aReversed ? TS_AE_START : TS_AE_END;
      mACP.style.fInterimChar = FALSE;
      mWritingMode = aWritingMode;

      return changed;
    }

    bool IsCollapsed() const
    {
      MOZ_ASSERT(!mDirty);
      return (mACP.acpStart == mACP.acpEnd);
    }

    void CollapseAt(uint32_t aOffset)
    {
      // XXX This does not update the selection's mWritingMode.
      // If it is ever used to "collapse" to an entirely new location,
      // we may need to fix that.
      mDirty = false;
      mACP.acpStart = mACP.acpEnd = static_cast<LONG>(aOffset);
      mACP.style.ase = TS_AE_END;
      mACP.style.fInterimChar = FALSE;
    }

    LONG MinOffset() const
    {
      MOZ_ASSERT(!mDirty);
      LONG min = std::min(mACP.acpStart, mACP.acpEnd);
      MOZ_ASSERT(min >= 0);
      return min;
    }

    LONG MaxOffset() const
    {
      MOZ_ASSERT(!mDirty);
      LONG max = std::max(mACP.acpStart, mACP.acpEnd);
      MOZ_ASSERT(max >= 0);
      return max;
    }

    LONG StartOffset() const
    {
      MOZ_ASSERT(!mDirty);
      MOZ_ASSERT(mACP.acpStart >= 0);
      return mACP.acpStart;
    }

    LONG EndOffset() const
    {
      MOZ_ASSERT(!mDirty);
      MOZ_ASSERT(mACP.acpEnd >= 0);
      return mACP.acpEnd;
    }

    LONG Length() const
    {
      MOZ_ASSERT(!mDirty);
      MOZ_ASSERT(mACP.acpEnd >= mACP.acpStart);
      return std::abs(mACP.acpEnd - mACP.acpStart);
    }

    bool IsReversed() const
    {
      MOZ_ASSERT(!mDirty);
      return (mACP.style.ase == TS_AE_START);
    }

    TsActiveSelEnd ActiveSelEnd() const
    {
      MOZ_ASSERT(!mDirty);
      return mACP.style.ase;
    }

    bool IsInterimChar() const
    {
      MOZ_ASSERT(!mDirty);
      return (mACP.style.fInterimChar != FALSE);
    }

    WritingMode GetWritingMode() const
    {
      MOZ_ASSERT(!mDirty);
      return mWritingMode;
    }

    bool EqualsExceptDirection(const TS_SELECTION_ACP& aACP) const
    {
      if (mACP.style.ase == aACP.style.ase) {
        return mACP.acpStart == aACP.acpStart &&
               mACP.acpEnd == aACP.acpEnd;
      }
      return mACP.acpStart == aACP.acpEnd &&
             mACP.acpEnd == aACP.acpStart;
    }

    bool EqualsExceptDirection(
           const SelectionChangeDataBase& aChangedSelection) const
    {
      MOZ_ASSERT(!mDirty);
      MOZ_ASSERT(aChangedSelection.IsValid());
      return aChangedSelection.Length() == static_cast<uint32_t>(Length()) &&
             aChangedSelection.mOffset == static_cast<uint32_t>(StartOffset());
    }

  private:
    TS_SELECTION_ACP mACP;
    WritingMode mWritingMode;
    bool mDirty;
  };
  // Don't access mSelection directly except at calling MarkDirty().
  // Use SelectionForTSFRef() instead.  This is modified immediately when
  // TSF requests to set selection and not updated by selection change in
  // content until mContentForTSF is cleared.
  Selection mSelectionForTSF;

  /**
   * Get the selection expected by TSF.  If mSelectionForTSF is already valid,
   * this just return the reference to it.  Otherwise, this initializes it
   * with eQuerySelectedText.  Please check if the result is valid before
   * actually using it.
   * Note that this is also called by ContentForTSFRef().
   */
  Selection& SelectionForTSFRef();

  class MOZ_STACK_CLASS AutoSetTemporarySelection final
  {
  public:
    explicit AutoSetTemporarySelection(Selection& aSelection)
      : mSelection(aSelection)
    {
      mDirty = mSelection.IsDirty();
      if (mDirty) {
        mSelection.CollapseAt(0);
      }
    }

    ~AutoSetTemporarySelection()
    {
      if (mDirty) {
        mSelection.MarkDirty();
      }
    }

 private:
    Selection& mSelection;
    bool mDirty;
  };

  struct PendingAction final
  {
    enum ActionType : uint8_t
    {
      COMPOSITION_START,
      COMPOSITION_UPDATE,
      COMPOSITION_END,
      SET_SELECTION
    };
    ActionType mType;
    // For compositionstart and selectionset
    LONG mSelectionStart;
    LONG mSelectionLength;
    // For compositionstart, compositionupdate and compositionend
    nsString mData;
    // For compositionupdate
    RefPtr<TextRangeArray> mRanges;
    // For selectionset
    bool mSelectionReversed;
    // For compositionupdate
    bool mIncomplete;
    // For compositionstart
    bool mAdjustSelection;
  };
  // Items of mPendingActions are appended when TSF tells us to need to dispatch
  // DOM composition events.  However, we cannot dispatch while the document is
  // locked because it can cause modifying the locked document.  So, the pending
  // actions should be performed when document lock is unlocked.
  nsTArray<PendingAction> mPendingActions;

  PendingAction* LastOrNewPendingCompositionUpdate()
  {
    if (!mPendingActions.IsEmpty()) {
      PendingAction& lastAction = mPendingActions.LastElement();
      if (lastAction.mType == PendingAction::COMPOSITION_UPDATE) {
        return &lastAction;
      }
    }
    PendingAction* newAction = mPendingActions.AppendElement();
    newAction->mType = PendingAction::COMPOSITION_UPDATE;
    newAction->mRanges = new TextRangeArray();
    newAction->mIncomplete = true;
    return newAction;
  }

  /**
   * WasTextInsertedWithoutCompositionAt() checks if text was inserted without
   * composition immediately before (e.g., see InsertTextAtSelectionInternal()).
   *
   * @param aStart              The inserted offset you expected.
   * @param aLength             The inserted text length you expected.
   * @return                    true if the last pending actions are
   *                            COMPOSITION_START and COMPOSITION_END and
   *                            aStart and aLength match their information.
   */
  bool WasTextInsertedWithoutCompositionAt(LONG aStart, LONG aLength) const
  {
    if (mPendingActions.Length() < 2) {
      return false;
    }
    const PendingAction& pendingLastAction = mPendingActions.LastElement();
    if (pendingLastAction.mType != PendingAction::COMPOSITION_END ||
        pendingLastAction.mData.Length() != ULONG(aLength)) {
      return false;
    }
    const PendingAction& pendingPreLastAction =
      mPendingActions[mPendingActions.Length() - 2];
    return pendingPreLastAction.mType == PendingAction::COMPOSITION_START &&
           pendingPreLastAction.mSelectionStart == aStart;
  }

  bool IsPendingCompositionUpdateIncomplete() const
  {
    if (mPendingActions.IsEmpty()) {
      return false;
    }
    const PendingAction& lastAction = mPendingActions.LastElement();
    return lastAction.mType == PendingAction::COMPOSITION_UPDATE &&
           lastAction.mIncomplete;
  }

  void CompleteLastActionIfStillIncomplete()
  {
    if (!IsPendingCompositionUpdateIncomplete()) {
      return;
    }
    RecordCompositionUpdateAction();
  }

  // When On*Composition() is called without document lock, we need to flush
  // the recorded actions at quitting the method.
  // AutoPendingActionAndContentFlusher class is usedful for it.
  class MOZ_STACK_CLASS AutoPendingActionAndContentFlusher final
  {
  public:
    explicit AutoPendingActionAndContentFlusher(TSFTextStore* aTextStore)
      : mTextStore(aTextStore)
    {
      MOZ_ASSERT(!mTextStore->mIsRecordingActionsWithoutLock);
      if (!mTextStore->IsReadWriteLocked()) {
        mTextStore->mIsRecordingActionsWithoutLock = true;
      }
    }

    ~AutoPendingActionAndContentFlusher()
    {
      if (!mTextStore->mIsRecordingActionsWithoutLock) {
        return;
      }
      mTextStore->FlushPendingActions();
      mTextStore->mIsRecordingActionsWithoutLock = false;
    }

  private:
    AutoPendingActionAndContentFlusher() {}

    RefPtr<TSFTextStore> mTextStore;
  };

  class Content final
  {
  public:
    Content(TSFTextStore::Composition& aComposition,
            TSFTextStore::Selection& aSelection) :
      mComposition(aComposition), mSelection(aSelection)
    {
      Clear();
    }

    void Clear()
    {
      mText.Truncate();
      mLastCompositionString.Truncate();
      mInitialized = false;
    }

    bool IsInitialized() const { return mInitialized; }

    void Init(const nsAString& aText)
    {
      mText = aText;
      if (mComposition.IsComposing()) {
        mLastCompositionString = mComposition.mString;
      } else {
        mLastCompositionString.Truncate();
      }
      mMinTextModifiedOffset = NOT_MODIFIED;
      mInitialized = true;
    }

    void OnLayoutChanged()
    {
      mMinTextModifiedOffset = NOT_MODIFIED;
    }

    const nsDependentSubstring GetSelectedText() const;
    const nsDependentSubstring GetSubstring(uint32_t aStart,
                                            uint32_t aLength) const;
    void ReplaceSelectedTextWith(const nsAString& aString);
    void ReplaceTextWith(LONG aStart, LONG aLength, const nsAString& aString);

    void StartComposition(ITfCompositionView* aCompositionView,
                          const PendingAction& aCompStart,
                          bool aPreserveSelection);
    /**
     * RestoreCommittedComposition() restores the committed string as
     * composing string.  If InsertTextAtSelection() or something is called
     * before a call of OnStartComposition(), there is a pending
     * compositionstart and a pending compositionend.  In this case, we
     * need to cancel the pending compositionend and continue the composition.
     *
     * @param aCompositionView          The composition view.
     * @param aPendingCompositionStart  The pending compositionstart which
     *                                  started the committed composition.
     * @param aCanceledCompositionEnd   The pending compositionend which is
     *                                  canceled for restarting the composition.
     */
    void RestoreCommittedComposition(
                         ITfCompositionView* aCompositionView,
                         const PendingAction& aPendingCompositionStart,
                         const PendingAction& aCanceledCompositionEnd);
    void EndComposition(const PendingAction& aCompEnd);

    const nsString& Text() const
    {
      MOZ_ASSERT(mInitialized);
      return mText;
    }
    const nsString& LastCompositionString() const
    {
      MOZ_ASSERT(mInitialized);
      return mLastCompositionString;
    }
    uint32_t MinTextModifiedOffset() const
    {
      MOZ_ASSERT(mInitialized);
      return mMinTextModifiedOffset;
    }

    // Returns true if layout of the character at the aOffset has not been
    // calculated.
    bool IsLayoutChangedAt(uint32_t aOffset) const
    {
      return IsLayoutChanged() && (mMinTextModifiedOffset <= aOffset);
    }
    // Returns true if layout of the content has been changed, i.e., the new
    // layout has not been calculated.
    bool IsLayoutChanged() const
    {
      return mInitialized && (mMinTextModifiedOffset != NOT_MODIFIED);
    }
    // Returns minimum offset of modified text range.
    uint32_t MinOffsetOfLayoutChanged() const
    {
      return mInitialized ? mMinTextModifiedOffset : NOT_MODIFIED;
    }

    TSFTextStore::Composition& Composition() { return mComposition; }
    TSFTextStore::Selection& Selection() { return mSelection; }

  private:
    nsString mText;
    // mLastCompositionString stores the composition string when the document
    // is locked. This is necessary to compute mMinTextModifiedOffset.
    nsString mLastCompositionString;
    TSFTextStore::Composition& mComposition;
    TSFTextStore::Selection& mSelection;

    // The minimum offset of modified part of the text.
    enum : uint32_t
    {
      NOT_MODIFIED = UINT32_MAX
    };
    uint32_t mMinTextModifiedOffset;

    bool mInitialized;
  };
  // mContentForTSF is cache of content.  The information is expected by TSF
  // and TIP.  Therefore, this is useful for answering the query from TSF or
  // TIP.
  // This is initialized by ContentForTSFRef() automatically (therefore, don't
  // access this member directly except at calling Clear(), IsInitialized(),
  // IsLayoutChangeAfter() or IsLayoutChanged()).
  // This is cleared when:
  //  - When there is no composition, the document is unlocked.
  //  - When there is a composition, all dispatched events are handled by
  //    the focused editor which may be in a remote process.
  // So, if two compositions are created very quickly, this cache may not be
  // cleared between eCompositionCommit(AsIs) and eCompositionStart.
  Content mContentForTSF;

  Content& ContentForTSFRef();

  // CanAccessActualContentDirectly() returns true when TSF/TIP can access
  // actual content directly.  In other words, mContentForTSF and/or
  // mSelectionForTSF doesn't cache content or they matches with actual
  // contents due to no pending text/selection change notifications.
  bool CanAccessActualContentDirectly() const;

  // While mContentForTSF is valid, this returns the text stored by it.
  // Otherwise, return the current text content retrieved by eQueryTextContent.
  bool GetCurrentText(nsAString& aTextContent);

  class MouseTracker final
  {
  public:
    static const DWORD kInvalidCookie = static_cast<DWORD>(-1);

    MouseTracker();

    HRESULT Init(TSFTextStore* aTextStore);
    HRESULT AdviseSink(TSFTextStore* aTextStore,
                       ITfRangeACP* aTextRange, ITfMouseSink* aMouseSink);
    void UnadviseSink();

    bool IsUsing() const { return mSink != nullptr; }
    bool InRange(uint32_t aOffset) const
    {
      if (NS_WARN_IF(mStart < 0) ||
          NS_WARN_IF(mLength <= 0)) {
        return false;
      }
      return aOffset >= static_cast<uint32_t>(mStart) &&
             aOffset < static_cast<uint32_t>(mStart + mLength);
    }
    DWORD Cookie() const { return mCookie; }
    bool OnMouseButtonEvent(ULONG aEdge, ULONG aQuadrant, DWORD aButtonStatus);
    LONG RangeStart() const { return mStart; }

  private:
    RefPtr<ITfMouseSink> mSink;
    LONG mStart;
    LONG mLength;
    DWORD mCookie;
  };
  // mMouseTrackers is an array to store each information of installed
  // ITfMouseSink instance.
  nsTArray<MouseTracker> mMouseTrackers;

  // The input scopes for this context, defaults to IS_DEFAULT.
  nsTArray<InputScope>         mInputScopes;

  // Support retrieving attributes.
  // TODO: We should support RightToLeft, perhaps.
  enum
  {
    // Used for result of GetRequestedAttrIndex()
    eNotSupported = -1,

    // Supported attributes
    eInputScope = 0,
    eTextVerticalWriting,
    eTextOrientation,

    // Count of the supported attributes
    NUM_OF_SUPPORTED_ATTRS
  };
  bool mRequestedAttrs[NUM_OF_SUPPORTED_ATTRS];

  int32_t GetRequestedAttrIndex(const TS_ATTRID& aAttrID);
  TS_ATTRID GetAttrID(int32_t aIndex);

  bool mRequestedAttrValues;

  // If edit actions are being recorded without document lock, this is true.
  // Otherwise, false.
  bool                         mIsRecordingActionsWithoutLock;
  // If GetTextExt() or GetACPFromPoint() is called and the layout hasn't been
  // calculated yet, these methods return TS_E_NOLAYOUT.  At that time,
  // mHasReturnedNoLayoutError is set to true.
  bool                         mHasReturnedNoLayoutError;
  // Before calling ITextStoreACPSink::OnLayoutChange() and
  // ITfContextOwnerServices::OnLayoutChange(), mWaitingQueryLayout is set to
  // true.  This is set to  false when GetTextExt() or GetACPFromPoint() is
  // called.
  bool                         mWaitingQueryLayout;
  // During the documet is locked, we shouldn't destroy the instance.
  // If this is true, the instance will be destroyed after unlocked.
  bool                         mPendingDestroy;
  // If this is false, MaybeFlushPendingNotifications() will clear the
  // mContentForTSF.
  bool                         mDeferClearingContentForTSF;
  // While there is native caret, this is true.  Otherwise, false.
  bool                         mNativeCaretIsCreated;
  // While the instance is dispatching events, the event may not be handled
  // synchronously in e10s mode.  So, in such case, in strictly speaking,
  // we shouldn't query layout information.  However, TS_E_NOLAYOUT bugs of
  // ITextStoreAPC::GetTextExt() blocks us to behave ideally.
  // For preventing it to be called, we should put off notifying TSF of
  // anything until layout information becomes available.
  bool                         mDeferNotifyingTSF;
  // While the document is locked, committing composition always fails since
  // TSF needs another document lock for modifying the composition, selection
  // and etc.  So, committing composition should be performed after the
  // document is unlocked.
  bool                         mDeferCommittingComposition;
  bool                         mDeferCancellingComposition;
  // Immediately after a call of Destroy(), mDestroyed becomes true.  If this
  // is true, the instance shouldn't grant any requests from the TIP anymore.
  bool                         mDestroyed;
  // While the instance is being destroyed, this is set to true for avoiding
  // recursive Destroy() calls.
  bool                         mBeingDestroyed;


  // TSF thread manager object for the current application
  static StaticRefPtr<ITfThreadMgr> sThreadMgr;
  static already_AddRefed<ITfThreadMgr> GetThreadMgr();
  // sMessagePump is QI'ed from sThreadMgr
  static StaticRefPtr<ITfMessagePump> sMessagePump;
public:
  // Expose GetMessagePump() for WinUtils.
  static already_AddRefed<ITfMessagePump> GetMessagePump();
private:
  // sKeystrokeMgr is QI'ed from sThreadMgr
  static StaticRefPtr<ITfKeystrokeMgr> sKeystrokeMgr;
  // TSF display attribute manager
  static StaticRefPtr<ITfDisplayAttributeMgr> sDisplayAttrMgr;
  static already_AddRefed<ITfDisplayAttributeMgr> GetDisplayAttributeMgr();
  // TSF category manager
  static StaticRefPtr<ITfCategoryMgr> sCategoryMgr;
  static already_AddRefed<ITfCategoryMgr> GetCategoryMgr();
  // Compartment for (Get|Set)IMEOpenState()
  static StaticRefPtr<ITfCompartment> sCompartmentForOpenClose;
  static already_AddRefed<ITfCompartment> GetCompartmentForOpenClose();

  // Current text store which is managing a keyboard enabled editor (i.e.,
  // editable editor).  Currently only ONE TSFTextStore instance is ever used,
  // although Create is called when an editor is focused and Destroy called
  // when the focused editor is blurred.
  static StaticRefPtr<TSFTextStore> sEnabledTextStore;

  // For IME (keyboard) disabled state:
  static StaticRefPtr<ITfDocumentMgr> sDisabledDocumentMgr;
  static StaticRefPtr<ITfContext> sDisabledContext;

  static StaticRefPtr<ITfInputProcessorProfiles> sInputProcessorProfiles;
  static already_AddRefed<ITfInputProcessorProfiles>
           GetInputProcessorProfiles();

  // TSF client ID for the current application
  static DWORD sClientId;
};

} // namespace widget
} // namespace mozilla

#endif // #ifndef TSFTextStore_h_
