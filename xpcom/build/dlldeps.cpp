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

// Force references to all of the symbols that we want exported from
// the dll that are located in the .lib files we link with

#include "nsVoidArray.h"
#include "nsValueArray.h"
#include "nsIAtom.h"
//#include "nsIBuffer.h"
#include "nsIByteArrayInputStream.h"
#include "nsFixedSizeAllocator.h"
#include "nsRecyclingAllocator.h"
#include "nsIThread.h"
#include "nsDeque.h"
#include "nsTraceRefcnt.h"
#include "nsTraceRefcntImpl.h"
#include "nsXPIDLString.h"
#include "nsIEnumerator.h"
#include "nsEnumeratorUtils.h"
#include "nsQuickSort.h"
#include "nsString.h"
#include "nsPrintfCString.h"
#include "nsSupportsArray.h"
#include "nsArray.h"
#include "nsArrayEnumerator.h"
#include "nsProxyEventPrivate.h"
#include "nsProxyRelease.h"
#include "xpt_xdr.h"
#include "xptcall.h"
#include "nsILocalFile.h"
#include "nsIGenericFactory.h"
#include "nsIPipe.h"
#include "nsStreamUtils.h"
#include "nsWeakReference.h"
#include "nsTextFormatter.h"
#include "nsIStorageStream.h"
#include "nsLinebreakConverter.h"
#include "nsIBinaryInputStream.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsReadableUtils.h"
#include "nsStaticNameTable.h"
#include "nsProcess.h"
#include "nsStringEnumerator.h"
#include "nsIInputStreamTee.h"
#include "nsCheapSets.h"
#ifdef DEBUG
#include "pure.h"
#endif
#include "nsHashKeys.h"
#include "nsTHashtable.h"
#include "pldhash.h"
#include "nsVariant.h"
#include "nsEscape.h"
#include "nsStreamUtils.h"

#define NS_STRINGAPI_IMPL
#include "nsStringAPI.h"

void XXXNeverCalled()
{
    nsTextFormatter::snprintf(nsnull,0,nsnull);
    nsTextFormatter::smprintf(nsnull, nsnull);
    nsTextFormatter::smprintf_free(nsnull);
    nsVoidArray();
    nsSmallVoidArray();
    nsStringHashSet();
    nsCStringHashSet();
    nsInt32HashSet();
    nsVoidHashSet();
    nsCheapStringSet();
    nsCheapInt32Set();
    nsValueArray(0);
    nsSupportsArray();
    NS_GetNumberOfAtoms();
    NS_NewPipe2(nsnull, nsnull, PR_FALSE, PR_FALSE, 0, 0, nsnull);
    NS_NewInputStreamReadyEvent(nsnull, nsnull, nsnull);
    NS_NewOutputStreamReadyEvent(nsnull, nsnull, nsnull);
    NS_AsyncCopy(nsnull, nsnull, nsnull, NS_ASYNCCOPY_VIA_READSEGMENTS, 0, nsnull, nsnull);
    PL_DHashStubEnumRemove(nsnull, nsnull, nsnull, nsnull);
    nsIDHashKey::HashKey(nsnull);
    nsFixedSizeAllocator a;
    nsRecyclingAllocator recyclingAllocator(2);
    a.Init(0, 0, 0, 0, 0);
    a.Alloc(0);
    a.Free(0, 0);
    nsIThread::GetCurrent(nsnull);
    nsDeque(nsnull);
    nsTraceRefcnt::LogAddCOMPtr(nsnull, nsnull);
    nsTraceRefcntImpl::DumpStatistics();
    NS_NewEmptyEnumerator(nsnull);
    new nsArrayEnumerator(nsnull);
    NS_QuickSort(nsnull, 0, 0, nsnull, nsnull);
    nsString();
    nsProxyObject(nsnull, 0, nsnull);
    NS_ProxyRelease(nsnull, nsnull, PR_FALSE);
    XPT_DoString(nsnull, nsnull, nsnull);
    XPT_DoHeader(nsnull, nsnull, nsnull);
#ifdef DEBUG
    PurePrintf(0);
#endif
    XPTC_InvokeByIndex(nsnull, 0, 0, nsnull);
    xptc_dummy();
    xptc_dummy2();
    XPTI_GetInterfaceInfoManager();
    NS_NewGenericFactory(nsnull, nsnull);
    NS_NewGenericModule2(nsnull, nsnull);
    NS_GetWeakReference(nsnull);
    nsCOMPtr<nsISupports> dummyFoo(do_GetInterface(nsnull));
    NS_NewByteArrayInputStream(nsnull, nsnull, 0);
    NS_NewStorageStream(0,0, nsnull);
    nsString foo;
    nsPrintfCString bar("");
    nsLinebreakConverter::ConvertStringLineBreaks(foo, 
    nsLinebreakConverter::eLinebreakAny, nsLinebreakConverter::eLinebreakContent);
    NS_NewLocalFile(nsString(), PR_FALSE, nsnull);
    NS_NewNativeLocalFile(nsCString(), PR_FALSE, nsnull);
    new nsProcess();
    nsStaticCaseInsensitiveNameTable();
    nsAutoString str1;
    str1.AssignWithConversion(nsnull, 0);
    nsCAutoString str2;
    ToNewUnicode(str1);
    ToNewUnicode(str2);
    ToNewCString(str1);
    ToNewCString(str2);
    PL_DHashTableFinish(nsnull);
    NS_NewInputStreamTee(nsnull, nsnull, nsnull);
    NS_NewArray(nsnull);
    nsCOMArray<nsISupports> dummyArray;
    NS_NewArray(nsnull, dummyArray);
    NS_NewArrayEnumerator(nsnull, dummyArray);
    new nsVariant();
    nsUnescape(nsnull);
    nsEscape(nsnull, url_XAlphas);
    nsStringArray array;
    NS_NewStringEnumerator(nsnull, &array);
    NS_NewAdoptingStringEnumerator(nsnull, &array);
    nsCStringArray carray;
    NS_NewUTF8StringEnumerator(nsnull, &carray);
    NS_NewAdoptingUTF8StringEnumerator(nsnull, &carray);
    nsVoidableString str3;
    nsCStringContainer sc1;
    NS_CStringContainerInit(sc1);
    NS_CStringContainerFinish(sc1);
    NS_CStringGetData(str2, nsnull, nsnull);
    NS_CStringSetData(str2, nsnull, 0);
    NS_CStringSetDataRange(str2, 0, 0, nsnull, 0);
    NS_CStringCopy(str2, str2);
    nsStringContainer sc2;
    NS_StringContainerInit(sc2);
    NS_StringContainerFinish(sc2);
    NS_StringGetData(str1, nsnull, nsnull);
    NS_StringSetData(str1, nsnull, 0);
    NS_StringSetDataRange(str1, 0, 0, nsnull, 0);
    NS_StringCopy(str1, str1);
    {
      nsAdoptingCString foo, bar;
      foo = bar;
    }
    {
      nsAdoptingString foo, bar;
      foo = bar;
    }
    NS_UTF16ToCString(str1, NS_CSTRING_ENCODING_ASCII, str2);
    NS_CStringToUTF16(str2, NS_CSTRING_ENCODING_ASCII, str1);
}
