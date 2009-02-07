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

#include <olectl.h>

#include "nscore.h"
#include "nsTextStore.h"
#include "nsWindow.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "prlog.h"

/******************************************************************/
/* nsTextStore                                                    */
/******************************************************************/

ITfThreadMgr* nsTextStore::sTsfThreadMgr = NULL;
DWORD         nsTextStore::sTsfClientId  = 0;
nsTextStore*  nsTextStore::sTsfTextStore = NULL;

UINT nsTextStore::sFlushTIPInputMessage  = 0;

#ifdef PR_LOGGING
PRLogModuleInfo* sTextStoreLog = nsnull;
#endif

nsTextStore::nsTextStore()
{
  mRefCnt = 1;
  mEditCookie = 0;
  mSinkMask = 0;
  mWindow = nsnull;
  mLock = 0;
  mLockQueued = 0;
  mTextChange.acpStart = PR_INT32_MAX;
  mTextChange.acpOldEnd = mTextChange.acpNewEnd = 0;
}

nsTextStore::~nsTextStore()
{
}

PRBool
nsTextStore::Create(nsWindow* aWindow,
                    PRUint32 aIMEState)
{
  if (!mDocumentMgr) {
    // Create document manager
    HRESULT hr = sTsfThreadMgr->CreateDocumentMgr(
                                    getter_AddRefs(mDocumentMgr));
    NS_ENSURE_TRUE(SUCCEEDED(hr), PR_FALSE);
    mWindow = aWindow;
    // Create context and add it to document manager
    hr = mDocumentMgr->CreateContext(sTsfClientId, 0,
                                     static_cast<ITextStoreACP*>(this),
                                     getter_AddRefs(mContext), &mEditCookie);
    if (SUCCEEDED(hr)) {
      SetIMEEnabledInternal(aIMEState);
      hr = mDocumentMgr->Push(mContext);
    }
    if (SUCCEEDED(hr)) {
      PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
             ("TSF: Created, window=%08x\n", aWindow));
      return PR_TRUE;
    }
    mContext = NULL;
    mDocumentMgr = NULL;
  }
  return PR_FALSE;
}

PRBool
nsTextStore::Destroy(void)
{
  Blur();
  if (mWindow) {
    // When blurred, Tablet Input Panel posts "blur" messages
    // and try to insert text when the message is retrieved later.
    // But by that time the text store is already destroyed,
    // so try to get the message early
    MSG msg;
    if (::PeekMessageW(&msg, mWindow->GetWindowHandle(),
                       sFlushTIPInputMessage, sFlushTIPInputMessage,
                       PM_REMOVE)) {
      ::DispatchMessageW(&msg);
    }
  }
  mContext = NULL;
  if (mDocumentMgr) {
    mDocumentMgr->Pop(TF_POPF_ALL);
    mDocumentMgr = NULL;
  }
  mSink = NULL;
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: Destroyed, window=%08x\n", mWindow));
  mWindow = NULL;
  return PR_TRUE;
}

PRBool
nsTextStore::Focus(void)
{
  HRESULT hr = sTsfThreadMgr->SetFocus(mDocumentMgr);
  NS_ENSURE_TRUE(SUCCEEDED(hr), PR_FALSE);
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: Focused\n"));
  return PR_TRUE;
}

PRBool
nsTextStore::Blur(void)
{
  sTsfThreadMgr->SetFocus(NULL);
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: Blurred\n"));
  return PR_TRUE;
}

STDMETHODIMP
nsTextStore::QueryInterface(REFIID riid,
                            void** ppv)
{
  *ppv=NULL;
  if ( (IID_IUnknown == riid) || (IID_ITextStoreACP == riid) ) {
    *ppv = static_cast<ITextStoreACP*>(this);
  } else if (IID_ITfContextOwnerCompositionSink == riid) {
    *ppv = static_cast<ITfContextOwnerCompositionSink*>(this);
  }
  if (*ppv) {
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) nsTextStore::AddRef()
{
  return ++mRefCnt;
}

STDMETHODIMP_(ULONG) nsTextStore::Release()
{
  --mRefCnt;
  if (0 != mRefCnt)
    return mRefCnt;
  delete this;
  return 0;
}

STDMETHODIMP
nsTextStore::AdviseSink(REFIID riid,
                        IUnknown *punk,
                        DWORD dwMask)
{
  NS_ENSURE_TRUE(punk && IID_ITextStoreACPSink == riid, E_INVALIDARG);
  if (!mSink) {
    // Install sink
    punk->QueryInterface(IID_ITextStoreACPSink, getter_AddRefs(mSink));
    NS_ENSURE_TRUE(mSink, E_UNEXPECTED);
  } else {
    // If sink is already installed we check to see if they are the same
    // Get IUnknown from both sides for comparison
    nsRefPtr<IUnknown> comparison1, comparison2;
    punk->QueryInterface(IID_IUnknown, getter_AddRefs(comparison1));
    mSink->QueryInterface(IID_IUnknown, getter_AddRefs(comparison2));
    if (comparison1 != comparison2)
      return CONNECT_E_ADVISELIMIT;
  }
  // Update mask either for a new sink or an existing sink
  mSinkMask = dwMask;
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: Sink installed, punk=%08x\n", punk));
  return S_OK;
}

STDMETHODIMP
nsTextStore::UnadviseSink(IUnknown *punk)
{
  NS_ENSURE_TRUE(punk, E_INVALIDARG);
  NS_ENSURE_TRUE(mSink, CONNECT_E_NOCONNECTION);
  // Get IUnknown from both sides for comparison
  nsRefPtr<IUnknown> comparison1, comparison2;
  punk->QueryInterface(IID_IUnknown, getter_AddRefs(comparison1));
  mSink->QueryInterface(IID_IUnknown, getter_AddRefs(comparison2));
  // Unadvise only if sinks are the same
  NS_ENSURE_TRUE(comparison1 == comparison2, CONNECT_E_NOCONNECTION);
  mSink = NULL;
  mSinkMask = 0;
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: Sink removed, punk=%08x\n", punk));
  return S_OK;
}

STDMETHODIMP
nsTextStore::RequestLock(DWORD dwLockFlags,
                         HRESULT *phrSession)
{
  NS_ENSURE_TRUE(mSink, E_FAIL);
  NS_ENSURE_TRUE(phrSession, E_INVALIDARG);
  if (mLock) {
    // only time when reentrant lock is allowed is when caller holds a
    // read-only lock and is requesting an async write lock
    if (TS_LF_READ == (mLock & TS_LF_READWRITE) &&
        TS_LF_READWRITE == (dwLockFlags & TS_LF_READWRITE) &&
        !(dwLockFlags & TS_LF_SYNC)) {
      *phrSession = TS_S_ASYNC;
      mLockQueued = dwLockFlags & (~TS_LF_SYNC);
    } else {
      // no more locks allowed
      *phrSession = TS_E_SYNCHRONOUS;
      return E_FAIL;
    }
  } else {
    // put on lock
    mLock = dwLockFlags & (~TS_LF_SYNC);
    *phrSession = mSink->OnLockGranted(mLock);
    while (mLockQueued) {
      mLock = mLockQueued;
      mLockQueued = 0;
      mSink->OnLockGranted(mLock);
    }
    mLock = 0;
  }
  return S_OK;
}

STDMETHODIMP
nsTextStore::GetStatus(TS_STATUS *pdcs)
{
  NS_ENSURE_TRUE(pdcs, E_INVALIDARG);
  pdcs->dwDynamicFlags = 0;
  // we use a "flat" text model for TSF support so no hidden text
  pdcs->dwStaticFlags = TS_SS_NOHIDDENTEXT;
  return S_OK;
}

STDMETHODIMP
nsTextStore::QueryInsert(LONG acpTestStart,
                         LONG acpTestEnd,
                         ULONG cch,
                         LONG *pacpResultStart,
                         LONG *pacpResultEnd)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: QueryInsert, start=%ld end=%ld cch=%lu\n",
          acpTestStart, acpTestEnd, cch));
  // We don't test to see if these positions are
  // after the end of document for performance reasons
  NS_ENSURE_TRUE(0 <= acpTestStart && acpTestStart <= acpTestEnd &&
                 pacpResultStart && pacpResultEnd, E_INVALIDARG);

  // XXX need to adjust to cluster boundary
  // Assume we are given good offsets for now
  *pacpResultStart = acpTestStart;
  *pacpResultEnd = acpTestStart + cch;

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: QueryInsert SUCCEEDED\n"));
  return S_OK;
}

STDMETHODIMP
nsTextStore::GetSelection(ULONG ulIndex,
                          ULONG ulCount,
                          TS_SELECTION_ACP *pSelection,
                          ULONG *pcFetched)
{
  NS_ENSURE_TRUE(TS_LF_READ == (mLock & TS_LF_READ), TS_E_NOLOCK);
  NS_ENSURE_TRUE(ulCount && pSelection && pcFetched, E_INVALIDARG);

  *pcFetched = 0;
  NS_ENSURE_TRUE(TS_DEFAULT_SELECTION == ulIndex || 0 == ulIndex,
                 TS_E_NOSELECTION);
  if (mCompositionView) {
    // Emulate selection during compositions
    *pSelection = mCompositionSelection;
  } else {
    // Construct and initialize an event to get selection info
    nsQueryContentEvent event(PR_TRUE, NS_QUERY_SELECTED_TEXT, mWindow);
    mWindow->InitEvent(event);
    mWindow->DispatchWindowEvent(&event);
    NS_ENSURE_TRUE(event.mSucceeded, E_FAIL);
    // Usually the selection anchor (beginning) position corresponds to the
    // TSF start and the selection focus (ending) position corresponds to
    // the TSF end, but if selection is reversed the focus now corresponds
    // to the TSF start and the anchor now corresponds to the TSF end
    pSelection->acpStart = event.mReply.mOffset;
    pSelection->acpEnd = pSelection->acpStart + event.mReply.mString.Length();
    pSelection->style.ase = event.mReply.mString.Length() &&
        event.mReply.mReversed ? TS_AE_START : TS_AE_END;
    // No support for interim character
    pSelection->style.fInterimChar = 0;
  }
  *pcFetched = 1;
  return S_OK;
}

HRESULT
nsTextStore::SetSelectionInternal(const TS_SELECTION_ACP* pSelection)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: SetSelection, sel=%ld-%ld\n",
          pSelection->acpStart, pSelection->acpEnd));
  if (mCompositionView) {
    // Emulate selection during compositions
    NS_ENSURE_TRUE(pSelection->acpStart >= mCompositionStart &&
                   pSelection->acpEnd <= mCompositionStart +
                       LONG(mCompositionString.Length()), TS_E_INVALIDPOS);
    mCompositionSelection = *pSelection;
  } else {
    nsSelectionEvent event(PR_TRUE, NS_SELECTION_SET, mWindow);
    event.mOffset = pSelection->acpStart;
    event.mLength = PRUint32(pSelection->acpEnd - pSelection->acpStart);
    event.mReversed = pSelection->style.ase == TS_AE_START;
    mWindow->InitEvent(event);
    mWindow->DispatchWindowEvent(&event);
    NS_ENSURE_TRUE(event.mSucceeded, E_FAIL);
  }
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: SetSelection SUCCEEDED\n"));
  return S_OK;
}

STDMETHODIMP
nsTextStore::SetSelection(ULONG ulCount,
                          const TS_SELECTION_ACP *pSelection)
{
  NS_ENSURE_TRUE(TS_LF_READWRITE == (mLock & TS_LF_READWRITE), TS_E_NOLOCK);
  NS_ENSURE_TRUE(1 == ulCount && pSelection, E_INVALIDARG);

  return SetSelectionInternal(pSelection);
}

STDMETHODIMP
nsTextStore::GetText(LONG acpStart,
                     LONG acpEnd,
                     WCHAR *pchPlain,
                     ULONG cchPlainReq,
                     ULONG *pcchPlainOut,
                     TS_RUNINFO *prgRunInfo,
                     ULONG ulRunInfoReq,
                     ULONG *pulRunInfoOut,
                     LONG *pacpNext)
{
  NS_ENSURE_TRUE(TS_LF_READ == (mLock & TS_LF_READ), TS_E_NOLOCK);
  NS_ENSURE_TRUE(pcchPlainOut && (pchPlain || prgRunInfo) &&
                 (!cchPlainReq == !pchPlain) &&
                 (!ulRunInfoReq == !prgRunInfo), E_INVALIDARG);
  NS_ENSURE_TRUE(0 <= acpStart && -1 <= acpEnd &&
                 (-1 == acpEnd || acpStart <= acpEnd), TS_E_INVALIDPOS);

  // Making sure to NULL-terminate string just to be on the safe side
  *pcchPlainOut = 0;
  if (pchPlain && cchPlainReq) *pchPlain = NULL;
  if (pulRunInfoOut) *pulRunInfoOut = 0;
  if (pacpNext) *pacpNext = acpStart;
  if (prgRunInfo && ulRunInfoReq) {
    prgRunInfo->uCount = 0;
    prgRunInfo->type = TS_RT_PLAIN;
  }
  PRUint32 length = -1 == acpEnd ? PR_UINT32_MAX : PRUint32(acpEnd - acpStart);
  if (cchPlainReq && cchPlainReq - 1 < length) {
    length = cchPlainReq - 1;
  }
  if (length) {
    LONG compNewStart = 0, compOldEnd = 0, compNewEnd = 0;
    if (mCompositionView) {
      // Sometimes GetText gets called between InsertTextAtSelection and
      // OnUpdateComposition. In this case the returned text would
      // be out of sync because we haven't sent NS_TEXT_TEXT in
      // OnUpdateComposition yet. Manually resync here.
      compOldEnd = PR_MIN(LONG(length) + acpStart,
                       mCompositionLength + mCompositionStart);
      compNewEnd = PR_MIN(LONG(length) + acpStart,
                       LONG(mCompositionString.Length()) + mCompositionStart);
      compNewStart = PR_MAX(acpStart, mCompositionStart);
      // Check if the range is affected
      if (compOldEnd > compNewStart || compNewEnd > compNewStart) {
        NS_ASSERTION(compOldEnd >= mCompositionStart &&
            compNewEnd >= mCompositionStart, "Range end is less than start\n");
        length = PRUint32(LONG(length) + compOldEnd - compNewEnd);
      }
    }
    // Send NS_QUERY_TEXT_CONTENT to get text content
    nsQueryContentEvent event(PR_TRUE, NS_QUERY_TEXT_CONTENT, mWindow);
    mWindow->InitEvent(event);
    event.InitForQueryTextContent(PRUint32(acpStart), length);
    mWindow->DispatchWindowEvent(&event);
    NS_ENSURE_TRUE(event.mSucceeded, E_FAIL);

    if (compOldEnd > compNewStart || compNewEnd > compNewStart) {
      // Resync composition string
      const PRUnichar* compStrStart = mCompositionString.BeginReading() +
          PR_MAX(compNewStart - mCompositionStart, 0);
      event.mReply.mString.Replace(compNewStart - acpStart,
          compOldEnd - mCompositionStart, compStrStart,
          compNewEnd - mCompositionStart);
      length = PRUint32(LONG(length) - compOldEnd + compNewEnd);
    }
    NS_ENSURE_TRUE(-1 == acpEnd || event.mReply.mString.Length() == length,
                   TS_E_INVALIDPOS);
    length = PR_MIN(length, event.mReply.mString.Length());

    if (pchPlain && cchPlainReq) {
      memcpy(pchPlain, event.mReply.mString.BeginReading(),
             length * sizeof(*pchPlain));
      pchPlain[length] = NULL;
      *pcchPlainOut = length;
    }
    if (prgRunInfo && ulRunInfoReq) {
      prgRunInfo->uCount = length;
      prgRunInfo->type = TS_RT_PLAIN;
      if (pulRunInfoOut) *pulRunInfoOut = 1;
    }
    if (pacpNext) *pacpNext = acpStart + length;
  }
  return S_OK;
}

STDMETHODIMP
nsTextStore::SetText(DWORD dwFlags,
                     LONG acpStart,
                     LONG acpEnd,
                     const WCHAR *pchText,
                     ULONG cch,
                     TS_TEXTCHANGE *pChange)
{
  // Per SDK documentation, and since we don't have better
  // ways to do this, this method acts as a helper to
  // call SetSelection followed by InsertTextAtSelection
  NS_ENSURE_TRUE(TS_LF_READWRITE == (mLock & TS_LF_READWRITE), TS_E_NOLOCK);
  TS_SELECTION_ACP selection;
  selection.acpStart = acpStart;
  selection.acpEnd = acpEnd;
  selection.style.ase = TS_AE_END;
  selection.style.fInterimChar = 0;
  // Set selection to desired range
  NS_ENSURE_TRUE(SUCCEEDED(SetSelectionInternal(&selection)), E_FAIL);
  // Replace just selected text
  return InsertTextAtSelection(TS_IAS_NOQUERY, pchText, cch,
                               NULL, NULL, pChange);
}

STDMETHODIMP
nsTextStore::GetFormattedText(LONG acpStart,
                              LONG acpEnd,
                              IDataObject **ppDataObject)
{
  // no support for formatted text
  return E_NOTIMPL;
}

STDMETHODIMP
nsTextStore::GetEmbedded(LONG acpPos,
                         REFGUID rguidService,
                         REFIID riid,
                         IUnknown **ppunk)
{
  // embedded objects are not supported
  return E_NOTIMPL;
}

STDMETHODIMP
nsTextStore::QueryInsertEmbedded(const GUID *pguidService,
                                 const FORMATETC *pFormatEtc,
                                 BOOL *pfInsertable)
{
  // embedded objects are not supported
  *pfInsertable = FALSE;
  return S_OK;
}

STDMETHODIMP
nsTextStore::InsertEmbedded(DWORD dwFlags,
                            LONG acpStart,
                            LONG acpEnd,
                            IDataObject *pDataObject,
                            TS_TEXTCHANGE *pChange)
{
  // embedded objects are not supported
  return E_NOTIMPL;
}

STDMETHODIMP
nsTextStore::RequestSupportedAttrs(DWORD dwFlags,
                                   ULONG cFilterAttrs,
                                   const TS_ATTRID *paFilterAttrs)
{
  // no attributes defined
  return S_OK;
}

STDMETHODIMP
nsTextStore::RequestAttrsAtPosition(LONG acpPos,
                                    ULONG cFilterAttrs,
                                    const TS_ATTRID *paFilterAttrs,
                                    DWORD dwFlags)
{
  // no per character attributes defined
  return S_OK;
}

STDMETHODIMP
nsTextStore::RequestAttrsTransitioningAtPosition(LONG acpPos,
                                                 ULONG cFilterAttrs,
                                                 const TS_ATTRID *paFilterAttr,
                                                 DWORD dwFlags)
{
  // no per character attributes defined
  return S_OK;
}

STDMETHODIMP
nsTextStore::FindNextAttrTransition(LONG acpStart,
                                    LONG acpHalt,
                                    ULONG cFilterAttrs,
                                    const TS_ATTRID *paFilterAttrs,
                                    DWORD dwFlags,
                                    LONG *pacpNext,
                                    BOOL *pfFound,
                                    LONG *plFoundOffset)
{
  NS_ENSURE_TRUE(pacpNext && pfFound && plFoundOffset, E_INVALIDARG);
  // no per character attributes defined
  *pacpNext = *plFoundOffset = acpHalt;
  *pfFound = FALSE;
  return S_OK;
}

STDMETHODIMP
nsTextStore::RetrieveRequestedAttrs(ULONG ulCount,
                                    TS_ATTRVAL *paAttrVals,
                                    ULONG *pcFetched)
{
  NS_ENSURE_TRUE(pcFetched && ulCount && paAttrVals, E_INVALIDARG);
  // no attributes defined
  *pcFetched = 0;
  return S_OK;
}

STDMETHODIMP
nsTextStore::GetEndACP(LONG *pacp)
{
  NS_ENSURE_TRUE(TS_LF_READ == (mLock & TS_LF_READ), TS_E_NOLOCK);
  NS_ENSURE_TRUE(pacp, E_INVALIDARG);
  // Flattened text is retrieved and its length returned
  nsQueryContentEvent event(PR_TRUE, NS_QUERY_TEXT_CONTENT, mWindow);
  mWindow->InitEvent(event);
  // Return entire text
  event.InitForQueryTextContent(0, PR_INT32_MAX);
  mWindow->DispatchWindowEvent(&event);
  NS_ENSURE_TRUE(event.mSucceeded, E_FAIL);
  *pacp = LONG(event.mReply.mString.Length());
  return S_OK;
}

#define TEXTSTORE_DEFAULT_VIEW    (1)

STDMETHODIMP
nsTextStore::GetActiveView(TsViewCookie *pvcView)
{
  NS_ENSURE_TRUE(pvcView, E_INVALIDARG);
  *pvcView = TEXTSTORE_DEFAULT_VIEW;
  return S_OK;
}

STDMETHODIMP
nsTextStore::GetACPFromPoint(TsViewCookie vcView,
                             const POINT *pt,
                             DWORD dwFlags,
                             LONG *pacp)
{
  NS_ENSURE_TRUE(TS_LF_READ == (mLock & TS_LF_READ), TS_E_NOLOCK);
  NS_ENSURE_TRUE(TEXTSTORE_DEFAULT_VIEW == vcView, E_INVALIDARG);
  // not supported for now
  return E_NOTIMPL;
}

STDMETHODIMP
nsTextStore::GetTextExt(TsViewCookie vcView,
                        LONG acpStart,
                        LONG acpEnd,
                        RECT *prc,
                        BOOL *pfClipped)
{
  NS_ENSURE_TRUE(TS_LF_READ == (mLock & TS_LF_READ), TS_E_NOLOCK);
  NS_ENSURE_TRUE(TEXTSTORE_DEFAULT_VIEW == vcView && prc && pfClipped,
                 E_INVALIDARG);
  NS_ENSURE_TRUE(acpStart >= 0 && acpEnd >= acpStart, TS_E_INVALIDPOS);

  // use NS_QUERY_TEXT_RECT to get rect in system, screen coordinates
  nsQueryContentEvent event(PR_TRUE, NS_QUERY_TEXT_RECT, mWindow);
  mWindow->InitEvent(event);
  event.InitForQueryTextRect(acpStart, acpEnd - acpStart);
  mWindow->DispatchWindowEvent(&event);
  NS_ENSURE_TRUE(event.mSucceeded, TS_E_INVALIDPOS);
  // IMEs don't like empty rects, fix here
  if (event.mReply.mRect.width <= 0)
    event.mReply.mRect.width = 1;
  if (event.mReply.mRect.height <= 0)
    event.mReply.mRect.height = 1;

  // convert to unclipped screen rect
  nsWindow* refWindow = static_cast<nsWindow*>(
      event.mReply.mFocusedWidget ? event.mReply.mFocusedWidget : mWindow);
  // Result rect is in top level widget coordinates
  refWindow = refWindow->GetTopLevelWindow(PR_FALSE);
  NS_ENSURE_TRUE(refWindow, E_FAIL);

  nsresult rv = refWindow->WidgetToScreen(event.mReply.mRect,
                                          event.mReply.mRect);
  NS_ENSURE_SUCCESS(rv, E_FAIL);

  // get bounding screen rect to test for clipping
  HRESULT hr = GetScreenExt(vcView, prc);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // clip text rect to bounding rect
  RECT textRect;
  ::SetRect(&textRect, event.mReply.mRect.x, event.mReply.mRect.y,
            event.mReply.mRect.XMost(), event.mReply.mRect.YMost());
  if (!::IntersectRect(prc, prc, &textRect))
    // Text is not visible
    ::SetRectEmpty(prc);

  // not equal if text rect was clipped
  *pfClipped = !::EqualRect(prc, &textRect);
  return S_OK;
}

STDMETHODIMP
nsTextStore::GetScreenExt(TsViewCookie vcView,
                          RECT *prc)
{
  NS_ENSURE_TRUE(TEXTSTORE_DEFAULT_VIEW == vcView && prc, E_INVALIDARG);
  // use NS_QUERY_EDITOR_RECT to get rect in system, screen coordinates
  nsQueryContentEvent event(PR_TRUE, NS_QUERY_EDITOR_RECT, mWindow);
  mWindow->InitEvent(event);
  mWindow->DispatchWindowEvent(&event);
  NS_ENSURE_TRUE(event.mSucceeded, E_FAIL);

  nsWindow* refWindow = static_cast<nsWindow*>(
      event.mReply.mFocusedWidget ? event.mReply.mFocusedWidget : mWindow);
  // Result rect is in top level widget coordinates
  refWindow = refWindow->GetTopLevelWindow(PR_FALSE);
  NS_ENSURE_TRUE(refWindow, E_FAIL);

  nsIntRect boundRect;
  nsresult rv = refWindow->GetClientBounds(boundRect);
  NS_ENSURE_SUCCESS(rv, E_FAIL);

  // Clip frame rect to window rect
  boundRect.IntersectRect(event.mReply.mRect, boundRect);
  rv = refWindow->WidgetToScreen(boundRect, boundRect);
  NS_ENSURE_SUCCESS(rv, E_FAIL);

  if (!boundRect.IsEmpty()) {
    ::SetRect(prc, boundRect.x, boundRect.y,
              boundRect.XMost(), boundRect.YMost());
  } else {
    ::SetRectEmpty(prc);
  }
  return S_OK;
}

STDMETHODIMP
nsTextStore::GetWnd(TsViewCookie vcView,
                    HWND *phwnd)
{
  NS_ENSURE_TRUE(TEXTSTORE_DEFAULT_VIEW == vcView && phwnd, E_INVALIDARG);
  *phwnd = mWindow->GetWindowHandle();
  return S_OK;
}

STDMETHODIMP
nsTextStore::InsertTextAtSelection(DWORD dwFlags,
                                   const WCHAR *pchText,
                                   ULONG cch,
                                   LONG *pacpStart,
                                   LONG *pacpEnd,
                                   TS_TEXTCHANGE *pChange)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: InsertTextAtSelection, cch=%lu\n", cch));
  NS_ENSURE_TRUE(TS_LF_READWRITE == (mLock & TS_LF_READWRITE), TS_E_NOLOCK);
  NS_ENSURE_TRUE(!cch || pchText, E_INVALIDARG);

  // Get selection first
  TS_SELECTION_ACP sel;
  ULONG selFetched;
  NS_ENSURE_TRUE(SUCCEEDED(GetSelection(
      TS_DEFAULT_SELECTION, 1, &sel, &selFetched)) && selFetched, E_FAIL);
  if (TS_IAS_QUERYONLY == dwFlags) {
    NS_ENSURE_TRUE(pacpStart && pacpEnd, E_INVALIDARG);
    // Simulate text insertion
    *pacpStart = sel.acpStart;
    *pacpEnd = sel.acpEnd;
    if (pChange) {
      pChange->acpStart = sel.acpStart;
      pChange->acpOldEnd = sel.acpEnd;
      pChange->acpNewEnd = sel.acpStart + cch;
    }
  } else {
    NS_ENSURE_TRUE(pChange, E_INVALIDARG);
    NS_ENSURE_TRUE(TS_IAS_NOQUERY == dwFlags || (pacpStart && pacpEnd),
                   E_INVALIDARG);
    if (mCompositionView) {
      // Emulate text insertion during compositions, because during a
      // composition, editor expects the whole composition string to
      // be sent in NS_TEXT_TEXT, not just the inserted part.
      // The actual NS_TEXT_TEXT is sent in OnUpdateComposition, which
      // should get called by TSF after this returns
      mCompositionString.Replace(PRUint32(sel.acpStart - mCompositionStart),
                                 sel.acpEnd - sel.acpStart, pchText, cch);

      mCompositionSelection.acpStart += cch;
      mCompositionSelection.acpEnd = mCompositionSelection.acpStart;
      mCompositionSelection.style.ase = TS_AE_END;
      // OnUpdateComposition is not called here because it will
      // result in fun visual artifacts
      PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
             ("TSF: InsertTextAtSelection, replaced=%lu-%lu\n",
              sel.acpStart - mCompositionStart,
              sel.acpEnd - mCompositionStart));
    } else {
      // Use a temporary composition to contain the text
      nsCompositionEvent compEvent(PR_TRUE, NS_COMPOSITION_START, mWindow);
      mWindow->InitEvent(compEvent);
      mWindow->DispatchWindowEvent(&compEvent);
      nsTextEvent event(PR_TRUE, NS_TEXT_TEXT, mWindow);
      mWindow->InitEvent(event);
      if (!cch) {
        // XXX See OnEndComposition comment on inserting empty strings
        event.theText = NS_LITERAL_STRING(" ");
        mWindow->DispatchWindowEvent(&event);
      }
      event.theText.Assign(pchText, cch);
      event.theText.ReplaceSubstring(NS_LITERAL_STRING("\r\n"),
                                     NS_LITERAL_STRING("\n"));
      mWindow->DispatchWindowEvent(&event);
      compEvent.message = NS_COMPOSITION_END;
      mWindow->DispatchWindowEvent(&compEvent);
    }
    pChange->acpStart = sel.acpStart;
    pChange->acpOldEnd = sel.acpEnd;
    // Get new selection
    NS_ENSURE_TRUE(SUCCEEDED(GetSelection(
        TS_DEFAULT_SELECTION, 1, &sel, &selFetched)) && selFetched, E_FAIL);
    pChange->acpNewEnd = sel.acpEnd;
    if (TS_IAS_NOQUERY != dwFlags) {
      *pacpStart = pChange->acpStart;
      *pacpEnd = pChange->acpNewEnd;
    }
  }
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: InsertTextAtSelection SUCCEEDED\n"));
  return S_OK;
}

STDMETHODIMP
nsTextStore::InsertEmbeddedAtSelection(DWORD dwFlags,
                                       IDataObject *pDataObject,
                                       LONG *pacpStart,
                                       LONG *pacpEnd,
                                       TS_TEXTCHANGE *pChange)
{
  // embedded objects are not supported
  return E_NOTIMPL;
}

static HRESULT
GetRangeExtent(ITfRange* aRange, LONG* aStart, LONG* aLength)
{
  nsRefPtr<ITfRangeACP> rangeACP;
  aRange->QueryInterface(IID_ITfRangeACP, getter_AddRefs(rangeACP));
  NS_ENSURE_TRUE(rangeACP, E_FAIL);
  return rangeACP->GetExtent(aStart, aLength);
}

HRESULT
nsTextStore::OnStartCompositionInternal(ITfCompositionView* pComposition,
                                        ITfRange* aRange,
                                        PRBool aPreserveSelection)
{
  mCompositionView = pComposition;
  HRESULT hr = GetRangeExtent(aRange, &mCompositionStart, &mCompositionLength);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: OnStartComposition, range=%ld-%ld\n", mCompositionStart,
          mCompositionStart + mCompositionLength));

  // Select composition range so the new composition replaces the range
  nsSelectionEvent selEvent(PR_TRUE, NS_SELECTION_SET, mWindow);
  mWindow->InitEvent(selEvent);
  selEvent.mOffset = PRUint32(mCompositionStart);
  selEvent.mLength = PRUint32(mCompositionLength);
  selEvent.mReversed = PR_FALSE;
  mWindow->DispatchWindowEvent(&selEvent);
  NS_ENSURE_TRUE(selEvent.mSucceeded, E_FAIL);

  // Set up composition
  nsQueryContentEvent queryEvent(PR_TRUE, NS_QUERY_SELECTED_TEXT, mWindow);
  mWindow->InitEvent(queryEvent);
  mWindow->DispatchWindowEvent(&queryEvent);
  NS_ENSURE_TRUE(queryEvent.mSucceeded, E_FAIL);
  mCompositionString = queryEvent.mReply.mString;
  if (!aPreserveSelection) {
    mCompositionSelection.acpStart = mCompositionStart;
    mCompositionSelection.acpEnd = mCompositionStart + mCompositionLength;
    mCompositionSelection.style.ase = TS_AE_END;
    mCompositionSelection.style.fInterimChar = FALSE;
  }
  nsCompositionEvent event(PR_TRUE, NS_COMPOSITION_START, mWindow);
  mWindow->InitEvent(event);
  mWindow->DispatchWindowEvent(&event);
  return S_OK;
}

STDMETHODIMP
nsTextStore::OnStartComposition(ITfCompositionView* pComposition,
                                BOOL* pfOk)
{
  *pfOk = FALSE;

  // Only one composition at a time
  if (mCompositionView)
    return S_OK;

  nsRefPtr<ITfRange> range;
  HRESULT hr = pComposition->GetRange(getter_AddRefs(range));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  hr = OnStartCompositionInternal(pComposition, range, PR_FALSE);
  if (SUCCEEDED(hr))
    *pfOk = TRUE;
  return hr;
}

STDMETHODIMP
nsTextStore::OnUpdateComposition(ITfCompositionView* pComposition,
                                 ITfRange* pRangeNew)
{
  NS_ENSURE_TRUE(mCompositionView &&
                 mCompositionView == pComposition &&
                 mDocumentMgr && mContext, E_UNEXPECTED);

  // Getting display attributes is *really* complicated!
  // We first get the context and the property objects to query for
  // attributes, but since a big range can have a variety of values for
  // the attribute, we have to find out all the ranges that have distinct
  // attribute values. Then we query for what the value represents through
  // the display attribute manager and translate that to nsTextRange to be
  // sent in NS_TEXT_TEXT
  if (!pRangeNew) // pRangeNew is null when the update is not complete
    return S_OK;

  // Get starting offset of the composition
  LONG compStart = 0, compLength = 0;
  HRESULT hr = GetRangeExtent(pRangeNew, &compStart, &compLength);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  if (mCompositionStart != compStart ||
      mCompositionString.Length() != compLength) {
    // If the queried composition length is different from the length
    // of our composition string, OnUpdateComposition is being called
    // because a part of the original composition was committed.
    // Reflect that by committing existing composition and starting
    // a new one. OnEndComposition followed by OnStartComposition
    // will accomplish this automagically.
    OnEndComposition(pComposition);
    OnStartCompositionInternal(pComposition, pRangeNew, PR_TRUE);
    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
           ("TSF: OnUpdateComposition, (reset) range=%ld-%ld\n",
            compStart, compStart + compLength));
  } else {
    mCompositionLength = compLength;
    PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
           ("TSF: OnUpdateComposition, range=%ld-%ld\n",
            compStart, compStart + compLength));
  }

  nsRefPtr<ITfProperty> prop;
  hr = mContext->GetProperty(GUID_PROP_ATTRIBUTE, getter_AddRefs(prop));
  NS_ENSURE_TRUE(SUCCEEDED(hr) && prop, hr);
  hr = LoadManagers();
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Use NS_TEXT_TEXT to set composition string
  nsTextEvent event(PR_TRUE, NS_TEXT_TEXT, mWindow);
  mWindow->InitEvent(event);

  VARIANT propValue;
  ::VariantInit(&propValue);
  nsRefPtr<ITfRange> range;
  nsRefPtr<IEnumTfRanges> enumRanges;
  hr = prop->EnumRanges(TfEditCookie(mEditCookie),
                        getter_AddRefs(enumRanges), pRangeNew);
  NS_ENSURE_TRUE(SUCCEEDED(hr) && enumRanges, hr);

  nsAutoTArray<nsTextRange, 4> textRanges;
  nsTextRange newRange;
  newRange.mStartOffset = PRUint32(mCompositionSelection.acpStart - compStart);
  newRange.mEndOffset = PRUint32(mCompositionSelection.acpEnd - compStart);
  newRange.mRangeType = NS_TEXTRANGE_CARETPOSITION;
  textRanges.AppendElement(newRange);
  // No matter if we have display attribute info or not,
  // we always pass in at least one range to NS_TEXT_TEXT
  newRange.mStartOffset = 0;
  newRange.mEndOffset = mCompositionString.Length();
  newRange.mRangeType = NS_TEXTRANGE_RAWINPUT;
  textRanges.AppendElement(newRange);

  while (S_OK == enumRanges->Next(1, getter_AddRefs(range), NULL) && range) {

    LONG start = 0, length = 0;
    if (FAILED(GetRangeExtent(range, &start, &length))) continue;

    newRange.mStartOffset = PRUint32(start - compStart);
    // The end of the last range in the array is
    // always kept at the end of composition
    newRange.mEndOffset = mCompositionString.Length();

    // Who came up with this convoluted way that we have to follow?
    ::VariantClear(&propValue);
    hr = prop->GetValue(TfEditCookie(mEditCookie), range, &propValue);
    if (FAILED(hr) || VT_I4 != propValue.vt) continue;

    GUID guid;
    hr = mCatMgr->GetGUID(DWORD(propValue.lVal), &guid);
    if (FAILED(hr)) continue;

    nsRefPtr<ITfDisplayAttributeInfo> info;
    hr = mDAMgr->GetDisplayAttributeInfo(
                     guid, getter_AddRefs(info), NULL);
    if (FAILED(hr) || !info) continue;

    TF_DISPLAYATTRIBUTE attr;
    hr = info->GetAttributeInfo(&attr);
    if (FAILED(hr)) continue;

    switch (attr.bAttr) {
    case TF_ATTR_TARGET_CONVERTED:
      newRange.mRangeType = NS_TEXTRANGE_SELECTEDCONVERTEDTEXT;
      break;
    case TF_ATTR_CONVERTED:
      newRange.mRangeType = NS_TEXTRANGE_CONVERTEDTEXT;
      break;
    case TF_ATTR_TARGET_NOTCONVERTED:
      newRange.mRangeType = NS_TEXTRANGE_SELECTEDRAWTEXT;
      break;
    default:
      newRange.mRangeType = NS_TEXTRANGE_RAWINPUT;
      break;
    }

    nsTextRange& lastRange = textRanges[textRanges.Length() - 1];
    if (lastRange.mStartOffset == newRange.mStartOffset) {
      // Replace range if last range is the same as this one
      // So that ranges don't overlap and confuse the editor
      lastRange = newRange;
    } else {
      lastRange.mEndOffset = newRange.mStartOffset;
      textRanges.AppendElement(newRange);
    }
  }

  event.theText = mCompositionString;
  event.rangeArray = textRanges.Elements();
  event.rangeCount = textRanges.Length();
  mWindow->DispatchWindowEvent(&event);
  ::VariantClear(&propValue);
  return S_OK;
}

STDMETHODIMP
nsTextStore::OnEndComposition(ITfCompositionView* pComposition)
{
  NS_ENSURE_TRUE(mCompositionView &&
                 mCompositionView == pComposition, E_UNEXPECTED);
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: OnEndComposition\n"));

  // Use NS_TEXT_TEXT to commit composition string
  nsTextEvent textEvent(PR_TRUE, NS_TEXT_TEXT, mWindow);
  mWindow->InitEvent(textEvent);
  if (!mCompositionString.Length()) {
    // XXX HACK! HACK! NS_TEXT_TEXT handler specifically rejects
    // first-time empty strings as workaround for another IME bug
    // and our request will be rejected if this is the first time
    // we are sending NS_TEXT_TEXT. The workaround is to send it a
    // non-empty dummy string first.
    textEvent.theText = NS_LITERAL_STRING(" ");
    mWindow->DispatchWindowEvent(&textEvent);
  }
  textEvent.theText = mCompositionString;
  textEvent.theText.ReplaceSubstring(NS_LITERAL_STRING("\r\n"),
                                     NS_LITERAL_STRING("\n"));
  mWindow->DispatchWindowEvent(&textEvent);

  nsCompositionEvent event(PR_TRUE, NS_COMPOSITION_END, mWindow);
  mWindow->InitEvent(event);
  mWindow->DispatchWindowEvent(&event);

  mCompositionView = NULL;
  mCompositionString.Truncate(0);
  // Maintain selection
  SetSelectionInternal(&mCompositionSelection);
  return S_OK;
}

nsresult
nsTextStore::OnFocusChange(PRBool aFocus,
                           nsWindow* aWindow,
                           PRUint32 aIMEEnabled)
{
  // no change notifications if TSF is disabled
  if (!sTsfThreadMgr || !sTsfTextStore)
    return NS_ERROR_NOT_AVAILABLE;

  if (aFocus) {
    if (sTsfTextStore->Create(aWindow, aIMEEnabled))
      sTsfTextStore->Focus();
  } else {
    sTsfTextStore->Destroy();
  }
  return NS_OK;
}

nsresult
nsTextStore::OnTextChangeInternal(PRUint32 aStart,
                                  PRUint32 aOldEnd,
                                  PRUint32 aNewEnd)
{
  if (!mLock && mSink && 0 != (mSinkMask & TS_AS_TEXT_CHANGE)) {
    mTextChange.acpStart = PR_MIN(mTextChange.acpStart, LONG(aStart));
    mTextChange.acpOldEnd = PR_MAX(mTextChange.acpOldEnd, LONG(aOldEnd));
    mTextChange.acpNewEnd = PR_MAX(mTextChange.acpNewEnd, LONG(aNewEnd));
    ::PostMessageW(mWindow->GetWindowHandle(),
                   WM_USER_TSF_TEXTCHANGE, 0, NULL);
  }
  return NS_OK;
}

void
nsTextStore::OnTextChangeMsgInternal(void)
{
  if (!mLock && mSink && 0 != (mSinkMask & TS_AS_TEXT_CHANGE) &&
      PR_INT32_MAX > mTextChange.acpStart) {
    mSink->OnTextChange(0, &mTextChange);
    mTextChange.acpStart = PR_INT32_MAX;
    mTextChange.acpOldEnd = mTextChange.acpNewEnd = 0;
  }
}

nsresult
nsTextStore::OnSelectionChangeInternal(void)
{
  if (!mLock && mSink && 0 != (mSinkMask & TS_AS_SEL_CHANGE)) {
    mSink->OnSelectionChange();
  }
  return NS_OK;
}

void
nsTextStore::CommitCompositionInternal(PRBool aDiscard)
{
  if (mCompositionView && aDiscard) {
    mCompositionString.Truncate(0);
    if (mSink && !mLock) {
      TS_TEXTCHANGE textChange;
      textChange.acpStart = mCompositionStart;
      textChange.acpOldEnd = mCompositionStart + mCompositionLength;
      textChange.acpNewEnd = mCompositionStart;
      mSink->OnTextChange(0, &textChange);
    }
  }
  // Terminate two contexts, the base context (mContext) and the top
  // if the top context is not the same as the base context
  nsRefPtr<ITfContext> context = mContext;
  do {
    if (context) {
      nsRefPtr<ITfContextOwnerCompositionServices> services;
      context->QueryInterface(IID_ITfContextOwnerCompositionServices,
                              getter_AddRefs(services));
      if (services)
        services->TerminateComposition(NULL);
    }
    if (context != mContext)
      break;
    if (mDocumentMgr)
      mDocumentMgr->GetTop(getter_AddRefs(context));
  } while (context != mContext);
}

static
PRBool
GetCompartment(IUnknown* pUnk,
               const GUID& aID,
               ITfCompartment** aCompartment)
{
  if (!pUnk) return PR_FALSE;

  nsRefPtr<ITfCompartmentMgr> compMgr;
  pUnk->QueryInterface(IID_ITfCompartmentMgr, getter_AddRefs(compMgr));
  if (!compMgr) return PR_FALSE;

  return SUCCEEDED(compMgr->GetCompartment(aID, aCompartment)) &&
         (*aCompartment) != NULL;
}

void
nsTextStore::SetIMEOpenState(PRBool aState)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: SetIMEOpenState, state=%lu\n", aState));

  nsRefPtr<ITfCompartment> comp;
  if (!GetCompartment(sTsfThreadMgr,
                      GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,
                      getter_AddRefs(comp)))
    return;

  VARIANT variant;
  variant.vt = VT_I4;
  variant.lVal = aState;
  comp->SetValue(sTsfClientId, &variant);
}

PRBool
nsTextStore::GetIMEOpenState(void)
{
  nsRefPtr<ITfCompartment> comp;
  if (!GetCompartment(sTsfThreadMgr,
                      GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,
                      getter_AddRefs(comp)))
    return PR_FALSE;

  VARIANT variant;
  ::VariantInit(&variant);
  if (SUCCEEDED(comp->GetValue(&variant)) && variant.vt == VT_I4)
    return variant.lVal != 0;

  ::VariantClear(&variant); // clear up in case variant.vt != VT_I4
  return PR_FALSE;
}

void
nsTextStore::SetIMEEnabledInternal(PRUint32 aState)
{
  PR_LOG(sTextStoreLog, PR_LOG_ALWAYS,
         ("TSF: SetIMEEnabled, state=%lu\n", aState));

  VARIANT variant;
  variant.vt = VT_I4;
  variant.lVal = aState != nsIWidget::IME_STATUS_ENABLED;

  // Set two contexts, the base context (mContext) and the top
  // if the top context is not the same as the base context
  nsRefPtr<ITfContext> context = mContext;
  nsRefPtr<ITfCompartment> comp;
  do {
    if (GetCompartment(context, GUID_COMPARTMENT_KEYBOARD_DISABLED,
                       getter_AddRefs(comp)))
      comp->SetValue(sTsfClientId, &variant);

    if (context != mContext)
      break;
    if (mDocumentMgr)
      mDocumentMgr->GetTop(getter_AddRefs(context));
  } while (context != mContext);
}

HRESULT
nsTextStore::LoadManagers(void)
{
  HRESULT hr;
  if (!mDAMgr) {
    hr = ::CoCreateInstance(CLSID_TF_DisplayAttributeMgr, NULL,
                            CLSCTX_INPROC_SERVER, IID_ITfDisplayAttributeMgr,
                            getter_AddRefs(mDAMgr));
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  }
  if (!mCatMgr) {
    hr = ::CoCreateInstance(CLSID_TF_CategoryMgr, NULL, CLSCTX_INPROC_SERVER,
                            IID_ITfCategoryMgr, getter_AddRefs(mCatMgr));
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  }
  return S_OK;
}

void
nsTextStore::Initialize(void)
{
#ifdef PR_LOGGING
  if (!sTextStoreLog)
    sTextStoreLog = PR_NewLogModule("nsTextStoreWidgets");
#endif
  if (!sTsfThreadMgr) {
    PRBool enableTsf = PR_TRUE;
    nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefs) {
      nsCOMPtr<nsIPrefBranch> prefBranch;
      prefs->GetBranch(nsnull, getter_AddRefs(prefBranch));
      if (prefBranch && NS_FAILED(prefBranch->GetBoolPref(
            "config.windows.use_tsf", &enableTsf)))
        enableTsf = PR_TRUE;
    }
    if (enableTsf) {
      if (SUCCEEDED(CoCreateInstance(CLSID_TF_ThreadMgr, NULL,
            CLSCTX_INPROC_SERVER, IID_ITfThreadMgr,
            reinterpret_cast<void**>(&sTsfThreadMgr)))) {
        if (FAILED(sTsfThreadMgr->Activate(&sTsfClientId))) {
          NS_RELEASE(sTsfThreadMgr);
          NS_WARNING("failed to activate TSF\n");
        }
      } else
        // TSF not installed?
        NS_WARNING("failed to create TSF manager\n");
    }
  }
  if (sTsfThreadMgr && !sTsfTextStore) {
    sTsfTextStore = new nsTextStore();
    if (!sTsfTextStore)
      NS_ERROR("failed to create text store\n");
  }
  if (sTsfThreadMgr && !sFlushTIPInputMessage) {
    sFlushTIPInputMessage = ::RegisterWindowMessageW(
        NS_LITERAL_STRING("Flush TIP Input Message").get());
  }
}

void
nsTextStore::Terminate(void)
{
  NS_IF_RELEASE(sTsfTextStore);
  if (sTsfThreadMgr) {
    sTsfThreadMgr->Deactivate();
    NS_RELEASE(sTsfThreadMgr);
  }
}
