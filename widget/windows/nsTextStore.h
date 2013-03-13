/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSTEXTSTORE_H_
#define NSTEXTSTORE_H_

#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsITimer.h"
#include "nsIWidget.h"
#include "nsWindowBase.h"
#include "mozilla/Attributes.h"

#include <msctf.h>
#include <textstor.h>

// GUID_PROP_INPUTSCOPE is declared in inputscope.h using INIT_GUID.
// With initguid.h, we get its instance instead of extern declaration.
#ifdef INPUTSCOPE_INIT_GUID
#include <initguid.h>
#endif
#include <inputscope.h>

struct ITfThreadMgr;
struct ITfDocumentMgr;
struct ITfDisplayAttributeMgr;
struct ITfCategoryMgr;
class nsWindow;
class nsTextEvent;
#ifdef MOZ_METRO
class MetroWidget;
#endif

// It doesn't work well when we notify TSF of text change
// during a mutation observer call because things get broken.
// So we post a message and notify TSF when we get it later.
#define WM_USER_TSF_TEXTCHANGE  (WM_USER + 0x100)

/*
 * Text Services Framework text store
 */

class nsTextStore MOZ_FINAL : public ITextStoreACP,
                              public ITfContextOwnerCompositionSink
{
public: /*IUnknown*/
  STDMETHODIMP_(ULONG)  AddRef(void);
  STDMETHODIMP          QueryInterface(REFIID, void**);
  STDMETHODIMP_(ULONG)  Release(void);

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

protected:
  typedef mozilla::widget::IMEState IMEState;
  typedef mozilla::widget::InputContext InputContext;

public:
  static void     Initialize(void);
  static void     Terminate(void);
  static void     SetIMEOpenState(bool);
  static bool     GetIMEOpenState(void);

  static void     CommitComposition(bool aDiscard)
  {
    NS_ENSURE_TRUE_VOID(sTsfTextStore);
    sTsfTextStore->CommitCompositionInternal(aDiscard);
  }

  static void     SetInputContext(const InputContext& aContext)
  {
    NS_ENSURE_TRUE_VOID(sTsfTextStore);
    sTsfTextStore->SetInputScope(aContext.mHTMLInputType);
    sTsfTextStore->SetInputContextInternal(aContext.mIMEState.mEnabled);
  }

  static nsresult OnFocusChange(bool aGotFocus,
                                nsWindowBase* aFocusedWidget,
                                IMEState::Enabled aIMEEnabled);
  static nsresult OnTextChange(uint32_t aStart,
                               uint32_t aOldEnd,
                               uint32_t aNewEnd)
  {
    NS_ENSURE_TRUE(sTsfTextStore, NS_ERROR_NOT_AVAILABLE);
    return sTsfTextStore->OnTextChangeInternal(aStart, aOldEnd, aNewEnd);
  }

  static void     OnTextChangeMsg(void)
  {
    NS_ENSURE_TRUE_VOID(sTsfTextStore);
    // Notify TSF text change
    // (see comments on WM_USER_TSF_TEXTCHANGE in nsTextStore.h)
    sTsfTextStore->OnTextChangeMsgInternal();
  }

  static nsresult OnSelectionChange(void)
  {
    NS_ENSURE_TRUE(sTsfTextStore, NS_ERROR_NOT_AVAILABLE);
    return sTsfTextStore->OnSelectionChangeInternal();
  }

  static nsIMEUpdatePreference GetIMEUpdatePreference();

  static bool CanOptimizeKeyAndIMEMessages()
  {
    // TODO: We need to implement this for ATOK.
    return true;
  }

  // Returns the address of the pointer so that the TSF automatic test can
  // replace the system object with a custom implementation for testing.
  static void* GetNativeData(uint32_t aDataType)
  {
    switch (aDataType) {
      case NS_NATIVE_TSF_THREAD_MGR:
        Initialize(); // Apply any previous changes
        return static_cast<void*>(&sTsfThreadMgr);
      case NS_NATIVE_TSF_CATEGORY_MGR:
        return static_cast<void*>(&sCategoryMgr);
      case NS_NATIVE_TSF_DISPLAY_ATTR_MGR:
        return static_cast<void*>(&sDisplayAttrMgr);
      default:
        return nullptr;
    }
  }

  static void*    GetTextStore()
  {
    return static_cast<void*>(sTsfTextStore);
  }

  static bool     ThinksHavingFocus()
  {
    return (sTsfTextStore && sTsfTextStore->mContext);
  }

  static bool     IsInTSFMode()
  {
    return sTsfThreadMgr != nullptr;
  }

  static bool     IsComposing()
  {
    return (sTsfTextStore && sTsfTextStore->mComposition.IsComposing());
  }

  static bool     IsComposingOn(nsWindowBase* aWidget)
  {
    return (IsComposing() && sTsfTextStore->mWidget == aWidget);
  }

#ifdef DEBUG
  // Returns true when keyboard layout has IME (TIP).
  static bool     CurrentKeyboardLayoutHasIME();
#endif // #ifdef DEBUG

protected:
  nsTextStore();
  ~nsTextStore();

  bool     Create(nsWindowBase* aWidget,
                  IMEState::Enabled aIMEEnabled);
  bool     Destroy(void);

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

  bool     GetScreenExtInternal(RECT &aScreenExt);
  bool     GetSelectionInternal(TS_SELECTION_ACP &aSelectionACP);
  // If aDispatchTextEvent is true, this method will dispatch text event if
  // this is called during IME composing.  aDispatchTextEvent should be true
  // only when this is called from SetSelection.  Because otherwise, the text
  // event should not be sent from here.
  HRESULT  SetSelectionInternal(const TS_SELECTION_ACP*,
                                bool aDispatchTextEvent = false);
  bool     InsertTextAtSelectionInternal(const nsAString &aInsertStr,
                                         TS_TEXTCHANGE* aTextChange);
  HRESULT  OnStartCompositionInternal(ITfCompositionView*, ITfRange*, bool);
  void     CommitCompositionInternal(bool);
  void     SetInputContextInternal(IMEState::Enabled aState);
  nsresult OnTextChangeInternal(uint32_t, uint32_t, uint32_t);
  void     OnTextChangeMsgInternal(void);
  nsresult OnSelectionChangeInternal(void);
  HRESULT  GetDisplayAttribute(ITfProperty* aProperty,
                               ITfRange* aRange,
                               TF_DISPLAYATTRIBUTE* aResult);
  HRESULT  UpdateCompositionExtent(ITfRange* pRangeNew);

  HRESULT  RecordCompositionUpdateAction();
  void     FlushPendingActions();

  nsresult OnLayoutChange();
  HRESULT  ProcessScopeRequest(DWORD dwFlags,
                               ULONG cFilterAttrs,
                               const TS_ATTRID *paFilterAttrs);
  void     SetInputScope(const nsString& aHTMLInputType);

  // Holds the pointer to our current win32 or metro widget
  nsRefPtr<nsWindowBase>       mWidget;
  // Document manager for the currently focused editor
  nsRefPtr<ITfDocumentMgr>     mDocumentMgr;
  // Edit cookie associated with the current editing context
  DWORD                        mEditCookie;
  // Editing context at the bottom of mDocumentMgr's context stack
  nsRefPtr<ITfContext>         mContext;
  // Currently installed notification sink
  nsRefPtr<ITextStoreACPSink>  mSink;
  // TS_AS_* mask of what events to notify
  DWORD                        mSinkMask;
  // 0 if not locked, otherwise TS_LF_* indicating the current lock
  DWORD                        mLock;
  // 0 if no lock is queued, otherwise TS_LF_* indicating the queue lock
  DWORD                        mLockQueued;
  // Cumulative text change offsets since the last notification
  TS_TEXTCHANGE                mTextChange;

  class Composition MOZ_FINAL
  {
  public:
    // NULL if no composition is active, otherwise the current composition
    nsRefPtr<ITfCompositionView> mView;

    // Current copy of the active composition string. Only mString is
    // changed during a InsertTextAtSelection call if we have a composition.
    // mString acts as a buffer until OnUpdateComposition is called
    // and mString is flushed to editor through NS_TEXT_TEXT. This
    // way all changes are updated in batches to avoid
    // inconsistencies/artifacts.
    nsString mString;

    // The latest composition string which was dispatched by composition update
    // event.
    nsString mLastData;

    // "Current selection" during a composition, in ACP offsets.
    // We use a fake selection during a composition because editor code doesn't
    // like us accessing the actual selection during a composition. So we leave
    // the actual selection alone and get/set mSelection instead
    // during GetSelection/SetSelection calls.
    TS_SELECTION_ACP mSelection;

    // The start and length of the current active composition, in ACP offsets
    LONG mStart;
    LONG mLength;

    bool IsComposing() const
    {
      return (mView != nullptr);
    }

    bool IsSelectionCollapsed() const
    {
      return (mSelection.acpStart == mSelection.acpEnd);
    }

    LONG MinSelectionOffset() const
    {
      return std::min(mSelection.acpStart, mSelection.acpEnd);
    }

    LONG MaxSelectionOffset() const
    {
      return std::max(mSelection.acpStart, mSelection.acpEnd);
    }

    LONG StringEndOffset() const
    {
      return mStart + static_cast<LONG>(mString.Length());
    }

    LONG EndOffset() const
    {
      return mStart + mLength;
    }

    // Start() and End() updates the members for emulating the latest state.
    // Unless flush the pending actions, this data never matches the actual
    // content.
    void Start(ITfCompositionView* aCompositionView,
               LONG aCompositionStartOffset,
               const nsAString& aCompositionString);
    void End();

    void StartLayoutChangeTimer(nsTextStore* aTextStore);
    void EnsureLayoutChangeTimerStopped();

  private:
    // Timer for calling ITextStoreACPSink::OnLayoutChange(). This is only used
    // during composing.
    nsCOMPtr<nsITimer> mLayoutChangeTimer;

    static void TimerCallback(nsITimer* aTimer, void *aClosure);
    static uint32_t GetLayoutChangeIntervalTime();
  };
  // While the document is locked, we cannot dispatch any events which cause
  // DOM events since the DOM events' handlers may modify the locked document.
  // However, even while the document is locked, TSF may queries us.
  // For that, nsTextStore modifies mComposition even while the document is
  // locked.  With mComposition, query methods can returns the text content
  // information.
  Composition mComposition;

  struct PendingAction MOZ_FINAL
  {
    enum ActionType MOZ_ENUM_TYPE(uint8_t)
    {
      COMPOSITION_START,
      COMPOSITION_UPDATE,
      COMPOSITION_END,
      SELECTION_SET
    };
    ActionType mType;
    // For compositionstart and selectionset
    LONG mSelectionStart;
    LONG mSelectionLength;
    // For compositionupdate and compositionend
    nsString mData;
    // For compositionupdate
    nsTArray<nsTextRange> mRanges;
    // For selectionset
    bool mSelectionReversed;
  };
  // Items of mPendingActions are appended when TSF tells us to need to dispatch
  // DOM composition events.  However, we cannot dispatch while the document is
  // locked because it can cause modifying the locked document.  So, the pending
  // actions should be performed when document lock is unlocked.
  nsTArray<PendingAction> mPendingActions;

  PendingAction* GetPendingCompositionUpdate()
  {
    if (!mPendingActions.IsEmpty()) {
      PendingAction& lastAction = mPendingActions.LastElement();
      if (lastAction.mType == PendingAction::COMPOSITION_UPDATE) {
        return &lastAction;
      }
    }
    PendingAction* newAction = mPendingActions.AppendElement();
    newAction->mType = PendingAction::COMPOSITION_UPDATE;
    // We think that 4 ranges (3 clauses and caret position) are enough for
    // most cases.
    newAction->mRanges.SetCapacity(4);
    return newAction;
  }

  // When On*Composition() is called without document lock, we need to flush
  // the recorded actions at quitting the method.
  // AutoPendingActionFlusher class is usedful for it.  
  class NS_STACK_CLASS AutoPendingActionFlusher MOZ_FINAL
  {
  public:
    AutoPendingActionFlusher(nsTextStore* aTextStore)
      : mTextStore(aTextStore)
    {
      MOZ_ASSERT(!mTextStore->mIsRecordingActionsWithoutLock);
      if (!mTextStore->IsReadWriteLocked()) {
        mTextStore->mIsRecordingActionsWithoutLock = true;
      }
    }

    ~AutoPendingActionFlusher()
    {
      if (!mTextStore->mIsRecordingActionsWithoutLock) {
        return;
      }
      mTextStore->FlushPendingActions();
      mTextStore->mIsRecordingActionsWithoutLock = false;
    }

  private:
    AutoPendingActionFlusher() {}

    nsRefPtr<nsTextStore> mTextStore;
  };

  // The input scopes for this context, defaults to IS_DEFAULT.
  nsTArray<InputScope>         mInputScopes;
  bool                         mInputScopeDetected;
  bool                         mInputScopeRequested;
  // If edit actions are being recorded without document lock, this is true.
  // Otherwise, false.
  bool                         mIsRecordingActionsWithoutLock;
  // During recording actions, we shouldn't call mSink->OnSelectionChange()
  // because it may cause TSF request new lock.  This is a problem if the
  // selection change is caused by a call of On*Composition() without document
  // lock since RequestLock() tries to flush the pending actions again (which
  // are flushing).  Therefore, OnSelectionChangeInternal() sets this true
  // during recoding actions and then, FlushPendingActions() will call
  // mSink->OnSelectionChange().
  bool                         mNotifySelectionChange;

  // TSF thread manager object for the current application
  static ITfThreadMgr*  sTsfThreadMgr;
  // TSF display attribute manager
  static ITfDisplayAttributeMgr* sDisplayAttrMgr;
  // TSF category manager
  static ITfCategoryMgr* sCategoryMgr;

  // TSF client ID for the current application
  static DWORD          sTsfClientId;
  // Current text store. Currently only ONE nsTextStore instance is ever used,
  // although Create is called when an editor is focused and Destroy called
  // when the focused editor is blurred.
  static nsTextStore*   sTsfTextStore;

  // Message the Tablet Input Panel uses to flush text during blurring.
  // See comments in Destroy
  static UINT           sFlushTIPInputMessage;

private:
  ULONG                       mRefCnt;
};

#endif /*NSTEXTSTORE_H_*/
