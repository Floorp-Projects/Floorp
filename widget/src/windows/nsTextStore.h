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
 *   Ningjie Chen <chenn@email.uc.edu>
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

#ifndef NSTEXTSTORE_H_
#define NSTEXTSTORE_H_

#include "nsAutoPtr.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsITimer.h"

#include <msctf.h>
#include <textstor.h>

struct ITfThreadMgr;
struct ITfDocumentMgr;
struct ITfDisplayAttributeMgr;
struct ITfCategoryMgr;
class nsWindow;
class nsTextEvent;

// It doesn't work well when we notify TSF of text change
// during a mutation observer call because things get broken.
// So we post a message and notify TSF when we get it later.
#define WM_USER_TSF_TEXTCHANGE  (WM_USER + 0x100)

/*
 * Text Services Framework text store
 */

class nsTextStore : public ITextStoreACP,
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

public:
  static void     Initialize(void);
  static void     Terminate(void);
  static void     SetIMEOpenState(PRBool);
  static PRBool   GetIMEOpenState(void);

  static void     CommitComposition(PRBool aDiscard)
  {
    if (!sTsfTextStore) return;
    sTsfTextStore->CommitCompositionInternal(aDiscard);
  }

  static void     SetIMEEnabled(PRUint32 aState)
  {
    if (!sTsfTextStore) return;
    sTsfTextStore->SetIMEEnabledInternal(aState);
  }

  static nsresult OnFocusChange(PRBool, nsWindow*, PRUint32);

  static nsresult OnTextChange(PRUint32 aStart,
                               PRUint32 aOldEnd,
                               PRUint32 aNewEnd)
  {
    if (!sTsfTextStore) return NS_OK;
    return sTsfTextStore->OnTextChangeInternal(aStart, aOldEnd, aNewEnd);
  }

  static void     OnTextChangeMsg(void)
  {
    if (!sTsfTextStore) return;
    // Notify TSF text change
    // (see comments on WM_USER_TSF_TEXTCHANGE in nsTextStore.h)
    sTsfTextStore->OnTextChangeMsgInternal();
  }

  static nsresult OnSelectionChange(void)
  {
    if (!sTsfTextStore) return NS_OK;
    return sTsfTextStore->OnSelectionChangeInternal();
  }

  static void CompositionTimerCallbackFunc(nsITimer *aTimer, void *aClosure)
  {
    nsTextStore *ts = static_cast<nsTextStore*>(aClosure);
    ts->OnCompositionTimer();
  }

  // Returns the address of the pointer so that the TSF automatic test can
  // replace the system object with a custom implementation for testing.
  static void*    GetThreadMgr(void)
  {
    Initialize(); // Apply any previous changes
    return (void*) & sTsfThreadMgr;
  }

  static void*    GetCategoryMgr(void)
  {
    return (void*) & sCategoryMgr;
  }

  static void*    GetDisplayAttrMgr(void)
  {
    return (void*) & sDisplayAttrMgr;
  }

protected:
  nsTextStore();
  ~nsTextStore();

  PRBool   Create(nsWindow*, PRUint32);
  PRBool   Destroy(void);

  // If aDispatchTextEvent is true, this method will dispatch text event if
  // this is called during IME composing.  aDispatchTextEvent should be true
  // only when this is called from SetSelection.  Because otherwise, the text
  // event should not be sent from here.
  HRESULT  SetSelectionInternal(const TS_SELECTION_ACP*,
                                PRBool aDispatchTextEvent = PR_FALSE);
  HRESULT  OnStartCompositionInternal(ITfCompositionView*, ITfRange*, PRBool);
  void     CommitCompositionInternal(PRBool);
  void     SetIMEEnabledInternal(PRUint32 aState);
  nsresult OnTextChangeInternal(PRUint32, PRUint32, PRUint32);
  void     OnTextChangeMsgInternal(void);
  nsresult OnSelectionChangeInternal(void);
  HRESULT  GetDisplayAttribute(ITfProperty* aProperty,
                               ITfRange* aRange,
                               TF_DISPLAYATTRIBUTE* aResult);
  HRESULT  UpdateCompositionExtent(ITfRange* pRangeNew);
  HRESULT  SendTextEventForCompositionString();
  HRESULT  SaveTextEvent(const nsTextEvent* aEvent);
  nsresult OnCompositionTimer();

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
  // Window containing the focused editor
  nsWindow*                    mWindow;
  // 0 if not locked, otherwise TS_LF_* indicating the current lock
  DWORD                        mLock;
  // 0 if no lock is queued, otherwise TS_LF_* indicating the queue lock
  DWORD                        mLockQueued;
  // Cumulative text change offsets since the last notification
  TS_TEXTCHANGE                mTextChange;
  // NULL if no composition is active, otherwise the current composition
  nsRefPtr<ITfCompositionView> mCompositionView;
  // Current copy of the active composition string. Only mCompositionString is
  // changed during a InsertTextAtSelection call if we have a composition.
  // mCompositionString acts as a buffer until OnUpdateComposition is called
  // and mCompositionString is flushed to editor through NS_TEXT_TEXT. This
  // way all changes are updated in batches to avoid inconsistencies/artifacts.
  nsString                     mCompositionString;
  // "Current selection" during a composition, in ACP offsets.
  // We use a fake selection during a composition because editor code doesn't
  // like us accessing the actual selection during a composition. So we leave
  // the actual selection alone and get/set mCompositionSelection instead
  // during GetSelection/SetSelection calls.
  TS_SELECTION_ACP             mCompositionSelection;
  // The start and length of the current active composition, in ACP offsets
  LONG                         mCompositionStart;
  LONG                         mCompositionLength;
  // The latest text event which was dispatched for composition string
  // of the current composing transaction.
  nsTextEvent*                 mLastDispatchedTextEvent;
  // Timer for calling ITextStoreACPSink::OnLayoutChange. This is only used
  // during composing.
  nsCOMPtr<nsITimer>           mCompositionTimer;

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
