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
#include "mozilla/TextRange.h"
#include "mozilla/WindowsVersion.h"

#include <msctf.h>
#include <textstor.h>

// GUID_PROP_INPUTSCOPE is declared in inputscope.h using INIT_GUID.
// With initguid.h, we get its instance instead of extern declaration.
#ifdef INPUTSCOPE_INIT_GUID
#include <initguid.h>
#endif
#include <inputscope.h>

// TSF InputScope, for earlier SDK 8
#define IS_SEARCH static_cast<InputScope>(50)

struct ITfThreadMgr;
struct ITfDocumentMgr;
struct ITfDisplayAttributeMgr;
struct ITfCategoryMgr;
class nsWindow;
#ifdef MOZ_METRO
class MetroWidget;
#endif

namespace mozilla {
namespace widget {
struct MSGResult;
} // namespace widget
} // namespace mozilla

// It doesn't work well when we notify TSF of text change
// during a mutation observer call because things get broken.
// So we post a message and notify TSF when we get it later.
#define WM_USER_TSF_TEXTCHANGE  (WM_USER + 0x100)

/*
 * Text Services Framework text store
 */

class nsTextStore MOZ_FINAL : public ITextStoreACP,
                              public ITfContextOwnerCompositionSink,
                              public ITfInputProcessorProfileActivationSink
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

public: /*ITfInputProcessorProfileActivationSink*/
  STDMETHODIMP OnActivated(DWORD, LANGID, REFCLSID, REFGUID, REFGUID,
                           HKL, DWORD);

protected:
  typedef mozilla::widget::IMEState IMEState;
  typedef mozilla::widget::InputContext InputContext;
  typedef mozilla::widget::InputContextAction InputContextAction;

public:
  static void     Initialize(void);
  static void     Terminate(void);

  static bool     ProcessRawKeyMessage(const MSG& aMsg);
  static void     ProcessMessage(nsWindowBase* aWindow, UINT aMessage,
                                 WPARAM& aWParam, LPARAM& aLParam,
                                 mozilla::widget::MSGResult& aResult);


  static void     SetIMEOpenState(bool);
  static bool     GetIMEOpenState(void);

  static void     CommitComposition(bool aDiscard)
  {
    NS_ENSURE_TRUE_VOID(sTsfTextStore);
    sTsfTextStore->CommitCompositionInternal(aDiscard);
  }

  static void SetInputContext(nsWindowBase* aWidget,
                              const InputContext& aContext,
                              const InputContextAction& aAction);

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

  static nsresult OnSelectionChange(void)
  {
    NS_ENSURE_TRUE(sTsfTextStore, NS_ERROR_NOT_AVAILABLE);
    return sTsfTextStore->OnSelectionChangeInternal();
  }

  static nsIMEUpdatePreference GetIMEUpdatePreference();

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

  static ITfMessagePump* GetMessagePump()
  {
    return sMessagePump;
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

  static bool     IsIMM_IME()
  {
    return sTsfTextStore ? sTsfTextStore->mIsIMM_IME :
                           IsIMM_IME(::GetKeyboardLayout(0));
  }

  static bool     IsIMM_IME(HKL aHKL)
  {
     return (::ImmGetIMEFileNameW(aHKL, nullptr, 0) > 0);
  }

#ifdef DEBUG
  // Returns true when keyboard layout has IME (TIP).
  static bool     CurrentKeyboardLayoutHasIME();
#endif // #ifdef DEBUG

protected:
  nsTextStore();
  ~nsTextStore();

  static void MarkContextAsKeyboardDisabled(ITfContext* aContext);
  static void MarkContextAsEmpty(ITfContext* aContext);

  bool     Create(nsWindowBase* aWidget);
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
  // If aDispatchTextEvent is true, this method will dispatch text event if
  // this is called during IME composing.  aDispatchTextEvent should be true
  // only when this is called from SetSelection.  Because otherwise, the text
  // event should not be sent from here.
  HRESULT  SetSelectionInternal(const TS_SELECTION_ACP*,
                                bool aDispatchTextEvent = false);
  bool     InsertTextAtSelectionInternal(const nsAString &aInsertStr,
                                         TS_TEXTCHANGE* aTextChange);
  void     CommitCompositionInternal(bool);
  nsresult OnTextChangeInternal(uint32_t, uint32_t, uint32_t);
  void     OnTextChangeMsg();
  nsresult OnSelectionChangeInternal(void);
  HRESULT  GetDisplayAttribute(ITfProperty* aProperty,
                               ITfRange* aRange,
                               TF_DISPLAYATTRIBUTE* aResult);
  HRESULT  RestartCompositionIfNecessary(ITfRange* pRangeNew = nullptr);

  // Following methods record composing action(s) to mPendingActions.
  // They will be flushed FlushPendingActions().
  HRESULT  RecordCompositionStartAction(ITfCompositionView* aCompositionView,
                                        ITfRange* aRange,
                                        bool aPreserveSelection);
  HRESULT  RecordCompositionUpdateAction();
  HRESULT  RecordCompositionEndAction();

  // FlushPendingActions() performs pending actions recorded in mPendingActions
  // and clear it.
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
  // Cookie of installing ITfInputProcessorProfileActivationSink
  DWORD                        mIPProfileCookie;
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
    // nullptr if no composition is active, otherwise the current composition
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

    void SetSelection(uint32_t aStart, uint32_t aLength, bool aReversed)
    {
      mDirty = false;
      mACP.acpStart = static_cast<LONG>(aStart);
      mACP.acpEnd = static_cast<LONG>(aStart + aLength);
      mACP.style.ase = aReversed ? TS_AE_START : TS_AE_END;
      mACP.style.fInterimChar = FALSE;
    }

    bool IsCollapsed() const
    {
      MOZ_ASSERT(!mDirty);
      return (mACP.acpStart == mACP.acpEnd);
    }

    void CollapseAt(uint32_t aOffset)
    {
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

  private:
    TS_SELECTION_ACP mACP;
    bool mDirty;
  };
  // Don't access mSelection directly except at calling MarkDirty().
  // Use CurrentSelection() instead.  This is marked as dirty when the
  // selection or content is changed without document lock.
  Selection mSelection;

  // Get "current selection" while the document is locked.  The selection is
  // NOT modified immediately during document lock.  The pending changes will
  // be flushed at unlocking the document.  The "current selection" is the
  // modified selection during document lock.  This is also called
  // CurrentContent() too.
  Selection& CurrentSelection();

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
    nsTArray<mozilla::TextRange> mRanges;
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
  // AutoPendingActionAndContentFlusher class is usedful for it.  
  class MOZ_STACK_CLASS AutoPendingActionAndContentFlusher MOZ_FINAL
  {
  public:
    AutoPendingActionAndContentFlusher(nsTextStore* aTextStore)
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

    nsRefPtr<nsTextStore> mTextStore;
  };

  class Content MOZ_FINAL
  {
  public:
    Content(nsTextStore::Composition& aComposition,
            nsTextStore::Selection& aSelection) :
      mComposition(aComposition), mSelection(aSelection)
    {
      Clear();
    }

    void Clear()
    {
      mText.Truncate();
      mInitialized = false;
    }

    bool IsInitialized() const { return mInitialized; }

    void Init(const nsAString& aText)
    {
      mText = aText;
      mMinTextModifiedOffset = NOT_MODIFIED;
      mInitialized = true;
      mNotifyTSFOfLayoutChange = false;
    }

    const nsDependentSubstring GetSelectedText() const;
    const nsDependentSubstring GetSubstring(uint32_t aStart,
                                            uint32_t aLength) const;
    void ReplaceSelectedTextWith(const nsAString& aString);
    void ReplaceTextWith(LONG aStart, LONG aLength, const nsAString& aString);

    void StartComposition(ITfCompositionView* aCompositionView,
                          const PendingAction& aCompStart,
                          bool aPreserveSelection);
    void EndComposition(const PendingAction& aCompEnd);

    const nsString& Text() const
    {
      MOZ_ASSERT(mInitialized);
      return mText;
    }

    // Returns true if layout of the character at the aOffset has not been
    // calculated.
    bool IsLayoutChangedAfter(uint32_t aOffset) const
    {
      return mInitialized && (mMinTextModifiedOffset < aOffset);
    }
    // Returns true if layout of the content has been changed, i.e., the new
    // layout has not been calculated.
    bool IsLayoutChanged() const
    {
      return mInitialized && (mMinTextModifiedOffset != NOT_MODIFIED);
    }

    void NeedsToNotifyTSFOfLayoutChange()
    {
      mNotifyTSFOfLayoutChange = true;
    }

    bool NeedToNotifyTSFOfLayoutChange() const
    {
      return mInitialized && mNotifyTSFOfLayoutChange;
    }

    nsTextStore::Composition& Composition() { return mComposition; }
    nsTextStore::Selection& Selection() { return mSelection; }

  private:
    nsString mText;
    nsTextStore::Composition& mComposition;
    nsTextStore::Selection& mSelection;

    // The minimum offset of modified part of the text.
    enum MOZ_ENUM_TYPE(uint32_t)
    {
      NOT_MODIFIED = UINT32_MAX
    };
    uint32_t mMinTextModifiedOffset;

    bool mInitialized;
    bool mNotifyTSFOfLayoutChange;
  };
  // mContent caches "current content" of the document ONLY while the document
  // is locked.  I.e., the content is cleared at unlocking the document since
  // we need to reduce the memory usage.  This is initialized by
  // CurrentContent() automatically, so, don't access this member directly
  // except at calling Clear(), IsInitialized(), IsLayoutChangedAfter() or
  // IsLayoutChanged().
  Content mContent;

  Content& CurrentContent();

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

  // True if current IME is implemented with IMM.
  bool mIsIMM_IME;

  // TSF thread manager object for the current application
  static ITfThreadMgr*  sTsfThreadMgr;
  // sMessagePump is QI'ed from sTsfThreadMgr
  static ITfMessagePump* sMessagePump;
  // sKeystrokeMgr is QI'ed from sTsfThreadMgr
  static ITfKeystrokeMgr* sKeystrokeMgr;
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

  // For IME (keyboard) disabled state:
  static ITfDocumentMgr* sTsfDisabledDocumentMgr;
  static ITfContext* sTsfDisabledContext;

  static ITfInputProcessorProfiles* sInputProcessorProfiles;

  // Message the Tablet Input Panel uses to flush text during blurring.
  // See comments in Destroy
  static UINT           sFlushTIPInputMessage;

private:
  ULONG                       mRefCnt;
};

#endif /*NSTEXTSTORE_H_*/
