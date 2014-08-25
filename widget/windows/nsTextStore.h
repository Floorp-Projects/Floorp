/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSTEXTSTORE_H_
#define NSTEXTSTORE_H_

#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIWidget.h"
#include "nsWindowBase.h"
#include "WinUtils.h"
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
#ifdef MOZ_METRO
class MetroWidget;
#endif

namespace mozilla {
namespace widget {
struct MSGResult;
} // namespace widget
} // namespace mozilla

/*
 * Text Services Framework text store
 */

class nsTextStore MOZ_FINAL : public ITextStoreACP,
                              public ITfContextOwnerCompositionSink,
                              public ITfActiveLanguageProfileNotifySink,
                              public ITfInputProcessorProfileActivationSink
{
public: /*IUnknown*/
  STDMETHODIMP          QueryInterface(REFIID, void**);

  NS_INLINE_DECL_IUNKNOWN_REFCOUNTING(nsTextStore)

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

public: /*ITfActiveLanguageProfileNotifySink*/
  STDMETHODIMP OnActivated(REFCLSID clsid, REFGUID guidProfile,
                           BOOL fActivated);

public: /*ITfInputProcessorProfileActivationSink*/
  STDMETHODIMP OnActivated(DWORD, LANGID, REFCLSID, REFGUID, REFGUID,
                           HKL, DWORD);

protected:
  typedef mozilla::widget::IMENotification IMENotification;
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
                                const IMEState& aIMEState);
  static nsresult OnTextChange(const IMENotification& aIMENotification)
  {
    NS_ENSURE_TRUE(sTsfTextStore, NS_ERROR_NOT_AVAILABLE);
    return sTsfTextStore->OnTextChangeInternal(aIMENotification);
  }

  static nsresult OnSelectionChange(void)
  {
    NS_ENSURE_TRUE(sTsfTextStore, NS_ERROR_NOT_AVAILABLE);
    return sTsfTextStore->OnSelectionChangeInternal();
  }

  static nsresult OnLayoutChange()
  {
    NS_ENSURE_TRUE(sTsfTextStore, NS_ERROR_NOT_AVAILABLE);
    return sTsfTextStore->OnLayoutChangeInternal();
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
    if (!sTsfTextStore || !sTsfTextStore->EnsureInitActiveTIPKeyboard()) {
      return IsIMM_IME(::GetKeyboardLayout(0));
    }
    return sTsfTextStore->mIsIMM_IME;
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

  bool Init(ITfThreadMgr* aThreadMgr);
  void Shutdown();

  static void MarkContextAsKeyboardDisabled(ITfContext* aContext);
  static void MarkContextAsEmpty(ITfContext* aContext);

  static bool IsTIPCategoryKeyboard(REFCLSID aTextService, LANGID aLangID,
                                    REFGUID aProfile);
  static void GetTIPDescription(REFCLSID aTextService, LANGID aLangID,
                                REFGUID aProfile, nsAString& aDescription);

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

  // This is called immediately after a call of OnLockGranted() of mSink.
  // Note that mLock isn't cleared yet when this is called.
  void     DidLockGranted();

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
  nsresult OnTextChangeInternal(const IMENotification& aIMENotification);
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

  nsresult OnLayoutChangeInternal();
  HRESULT  HandleRequestAttrs(DWORD aFlags,
                              ULONG aFilterCount,
                              const TS_ATTRID* aFilterAttrs);
  void     SetInputScope(const nsString& aHTMLInputType);

  // Creates native caret over our caret.  This method only works on desktop
  // application.  Otherwise, this does nothing.
  void     CreateNativeCaret();

  bool     EnsureInitActiveTIPKeyboard();

  // Holds the pointer to our current win32 or metro widget
  nsRefPtr<nsWindowBase>       mWidget;
  // Document manager for the currently focused editor
  nsRefPtr<ITfDocumentMgr>     mDocumentMgr;
  // Edit cookie associated with the current editing context
  DWORD                        mEditCookie;
  // Cookie of installing ITfInputProcessorProfileActivationSink
  DWORD                        mIPProfileCookie;
  // Cookie of installing ITfActiveLanguageProfileNotifySink
  DWORD                        mLangProfileCookie;
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
  // Active TIP keyboard's description.  If active language profile isn't TIP,
  // i.e., IMM-IME or just a keyboard layout, this is empty.
  nsString                     mActiveTIPKeyboardDescription;

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

  // Get "current selection".  If the document is locked, this initializes
  // mSelection with the selection at the first call during a lock and returns
  // it.  However, mSelection is NOT modified immediately.  When pending
  // changes are flushed at unlocking the document, cached mSelection is
  // modified.  Note that this is also called by LockedContent().
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
    nsRefPtr<mozilla::TextRangeArray> mRanges;
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
    newAction->mRanges = new mozilla::TextRangeArray();
    newAction->mIncomplete = true;
    return newAction;
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
  };
  // mLockedContent caches content of the document ONLY while the document
  // is locked.  I.e., the content is cleared at unlocking the document since
  // we need to reduce the memory usage.  This is initialized by
  // LockedContent() automatically.  So, don't access this member directly
  // except at calling Clear(), IsInitialized(), IsLayoutChangedAfter() or
  // IsLayoutChanged().
  Content mLockedContent;

  Content& LockedContent();

  // While the documet is locked, this returns the text stored by
  // mLockedContent.  Otherwise, return the current text content.
  bool GetCurrentText(nsAString& aTextContent);

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
  // During recording actions, we shouldn't call mSink->OnSelectionChange()
  // because it may cause TSF request new lock.  This is a problem if the
  // selection change is caused by a call of On*Composition() without document
  // lock since RequestLock() tries to flush the pending actions again (which
  // are flushing).  Therefore, OnSelectionChangeInternal() sets this true
  // during recoding actions and then, RequestLock() will call
  // mSink->OnSelectionChange() after mLock becomes 0.
  bool                         mPendingOnSelectionChange;
  // If GetTextExt() or GetACPFromPoint() is called and the layout hasn't been
  // calculated yet, these methods return TS_E_NOLAYOUT.  Then, RequestLock()
  // will call mSink->OnLayoutChange() and
  // ITfContextOwnerServices::OnLayoutChange() after the layout is fixed and
  // the document is unlocked.
  bool                         mPendingOnLayoutChange;
  // While there is native caret, this is true.  Otherwise, false.
  bool                         mNativeCaretIsCreated;

  // True if current IME is implemented with IMM.
  bool                         mIsIMM_IME;
  // True if OnActivated() is already called
  bool                         mOnActivatedCalled;

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

  // Enables/Disables hack for specific TIP.
  static bool sCreateNativeCaretForATOK;
  static bool sDoNotReturnNoLayoutErrorToFreeChangJie;
  static bool sDoNotReturnNoLayoutErrorToEasyChangjei;

  // Message the Tablet Input Panel uses to flush text during blurring.
  // See comments in Destroy
  static UINT           sFlushTIPInputMessage;
};

#endif /*NSTEXTSTORE_H_*/
