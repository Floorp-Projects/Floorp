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

#ifdef XP_WIN
#include <windows.h>
#include "nsWindowsRegKey.h"
#ifdef DEBUG
#include "pure.h"
#endif
#endif
#include "nsXULAppAPI.h"
#include "nsXPCOMGlue.h"
#include "nsVoidArray.h"
#include "nsTArray.h"
#include "nsIAtom.h"
#include "nsFixedSizeAllocator.h"
#include "nsRecyclingAllocator.h"
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
#include "nsArrayEnumerator.h"
#include "nsProxyRelease.h"
#include "xpt_xdr.h"
#include "xptcall.h"
#include "nsILocalFile.h"
#include "nsIPipe.h"
#include "nsStreamUtils.h"
#include "nsWeakReference.h"
#include "nsTextFormatter.h"
#include "nsIStorageStream.h"
#include "nsStringStream.h"
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
#include "nsHashKeys.h"
#include "nsTHashtable.h"
#include "pldhash.h"
#include "nsVariant.h"
#include "nsEscape.h"
#include "nsStreamUtils.h"
#include "nsNativeCharsetUtils.h"
#include "nsInterfaceRequestorAgg.h"
#include "nsHashPropertyBag.h"
#include "nsXPCOMStrings.h"
#include "nsStringBuffer.h"
#include "nsCategoryCache.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCycleCollector.h"
#include "nsThreadUtils.h"
#include "nsTObserverArray.h"
#include "nsWildCard.h"
#include "mozilla/Mutex.h"
#include "mozilla/Monitor.h"
#include "mozilla/CondVar.h"
#include "mozilla/TimeStamp.h"

using namespace mozilla;

class nsCStringContainer : private nsStringContainer_base { };
class nsStringContainer : private nsStringContainer_base { };

void XXXNeverCalled()
{
    GRE_GetGREPathWithProperties(nsnull, 0, nsnull, 0, nsnull, 0);
    nsTextFormatter::snprintf(nsnull,0,nsnull);
    nsTextFormatter::smprintf(nsnull, nsnull);
    nsTextFormatter::smprintf_free(nsnull);
    nsVoidArray();
    nsSmallVoidArray();
    {
      nsTArray<PRBool> array1(1), array2(1);
      PRBool a, b, c;
      a = b = c = PR_FALSE;
      array1.AppendElement(a);
      array2.InsertElementAt(b, 0);
      array2.InsertElementAt(c, 0);
      array1.AppendElements(array2);
    }
    {
      nsTObserverArray<PRBool> dummyObserverArray;
      PRBool a = PR_FALSE;
      dummyObserverArray.AppendElement(a);
      dummyObserverArray.RemoveElement(a);
      dummyObserverArray.Clear();
    }
    nsStringHashSet();
    nsCStringHashSet();
    nsInt32HashSet();
    nsVoidHashSet();
    nsCheapStringSet();
    nsCheapInt32Set();
    nsSupportsArray();
    NS_GetNumberOfAtoms();
    NS_NewPipe(nsnull, nsnull, 0, 0, PR_FALSE, PR_FALSE, nsnull);
    NS_NewPipe2(nsnull, nsnull, PR_FALSE, PR_FALSE, 0, 0, nsnull);
    NS_NewInputStreamReadyEvent(nsnull, nsnull, nsnull);
    NS_NewOutputStreamReadyEvent(nsnull, nsnull, nsnull);
    NS_AsyncCopy(nsnull, nsnull, nsnull, NS_ASYNCCOPY_VIA_READSEGMENTS, 0, nsnull, nsnull);
    {
      nsCString temp;
      NS_ConsumeStream(nsnull, 0, temp);
    }
    NS_InputStreamIsBuffered(nsnull);
    NS_OutputStreamIsBuffered(nsnull);
    NS_CopySegmentToStream(nsnull, nsnull, nsnull, 0, 0, nsnull);
    NS_CopySegmentToBuffer(nsnull, nsnull, nsnull, 0, 0, nsnull);
    NS_DiscardSegment(nsnull, nsnull, nsnull, 0, 0, nsnull);
    NS_WriteSegmentThunk(nsnull, nsnull, nsnull, 0, 0, 0);
    NS_NewByteInputStream(nsnull, nsnull, 0, NS_ASSIGNMENT_COPY);
    NS_NewCStringInputStream(nsnull, nsCString());
    NS_NewStringInputStream(nsnull, nsString());
    PL_DHashStubEnumRemove(nsnull, nsnull, nsnull, nsnull);
    nsIDHashKey::HashKey(nsnull);
    nsFixedSizeAllocator a;
    nsRecyclingAllocator recyclingAllocator(2);
    a.Init(0, 0, 0, 0, 0);
    a.Alloc(0);
    a.Free(0, 0);
    nsDeque d(nsnull);
    nsDequeIterator di(d);
    nsTraceRefcnt::LogAddCOMPtr(nsnull, nsnull);
    nsTraceRefcntImpl::DumpStatistics();
    NS_NewEmptyEnumerator(nsnull);
    NS_QuickSort(nsnull, 0, 0, nsnull, nsnull);
    nsString();
    NS_ProxyRelease(nsnull, nsnull, PR_FALSE);
    XPT_DoString(nsnull, nsnull, nsnull);
    XPT_DoHeader(nsnull, nsnull, nsnull);
    NS_InvokeByIndex(nsnull, 0, 0, nsnull);
    NS_GetWeakReference(nsnull);
    nsCOMPtr<nsISupports> dummyFoo(do_GetInterface(nsnull));
    NS_NewStorageStream(0,0, nsnull);
    nsString foo;
    nsPrintfCString bar("");
    nsLinebreakConverter::ConvertStringLineBreaks(foo, 
    nsLinebreakConverter::eLinebreakAny, nsLinebreakConverter::eLinebreakContent);
    NS_NewLocalFile(EmptyString(), PR_FALSE, nsnull);
    NS_NewNativeLocalFile(EmptyCString(), PR_FALSE, nsnull);
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
    nsCOMArray<nsISupports> dummyArray;
    NS_NewArrayEnumerator(nsnull, dummyArray);
    new nsVariant();
    nsUnescape(nsnull);
    nsEscape(nsnull, url_XAlphas);
    nsTArray<nsString> array;
    NS_NewStringEnumerator(nsnull, &array);
    NS_NewAdoptingStringEnumerator(nsnull, &array);
    nsTArray<nsCString> carray;
    NS_NewUTF8StringEnumerator(nsnull, &carray);
    NS_NewAdoptingUTF8StringEnumerator(nsnull, &carray);
    nsVoidableString str3;
    {
      nsAdoptingCString foo, bar;
      foo = bar;
    }
    {
      nsAdoptingString foo, bar;
      foo = bar;
    }
    NS_CopyNativeToUnicode(str2, str1);
    NS_CopyUnicodeToNative(str1, str2);
    {
      nsID id;
      CallCreateInstance(id, nsnull, id, nsnull);
      CallCreateInstance("", nsnull, id, nsnull);
      CallGetClassObject(id, id, nsnull);
      CallGetClassObject("", id, nsnull);
    }
    NS_NewInterfaceRequestorAggregation(nsnull, nsnull, nsnull);
    NS_NewHashPropertyBag(nsnull);
    nsDependentString depstring;
    depstring.Rebind(nsnull, PRUint32(0));
    nsDependentCString depcstring;
    depcstring.Rebind(nsnull, PRUint32(0));
    // nsStringAPI
    nsCStringContainer sc1;
    NS_CStringContainerInit(sc1);
    NS_CStringContainerInit2(sc1, nsnull, 0, 0);
    NS_CStringContainerFinish(sc1);
    NS_CStringGetData(str2, nsnull, nsnull);
    NS_CStringGetMutableData(str2, 0, nsnull);
    NS_CStringSetData(str2, nsnull, 0);
    NS_CStringSetDataRange(str2, 0, 0, nsnull, 0);
    NS_CStringCopy(str2, str2);
    NS_CStringCloneData(str2);
    nsStringContainer sc2;
    NS_StringContainerInit(sc2);
    NS_StringContainerInit2(sc2, nsnull, 0, 0);
    NS_StringContainerFinish(sc2);
    NS_StringGetData(str1, nsnull, nsnull);
    NS_StringGetMutableData(str1, 0, nsnull);
    NS_StringSetData(str1, nsnull, 0);
    NS_StringSetDataRange(str1, 0, 0, nsnull, 0);
    NS_StringCopy(str1, str1);
    NS_StringCloneData(str1);
    NS_UTF16ToCString(str1, NS_CSTRING_ENCODING_ASCII, str2);
    NS_CStringToUTF16(str2, NS_CSTRING_ENCODING_ASCII, str1);

    nsCategoryObserver catobs(nsnull, nsnull);
    nsCategoryCache<nsILocalFile> catcache(nsnull);

    // nsStringBuffer.h
    {
      nsString x;
      nsCString y;
      nsStringBuffer b;
      b.AddRef();
      b.Release();
      nsStringBuffer::Alloc(0);
      nsStringBuffer::Realloc(nsnull, 0);
      nsStringBuffer::FromString(x);
      nsStringBuffer::FromString(y);
      b.ToString(0, x);
      b.ToString(0, y);
    }

    nsXPCOMCycleCollectionParticipant();
    nsCycleCollector_collect(nsnull);
#ifdef XP_WIN
    sXPCOMHasLoadedNewDLLs = !sXPCOMHasLoadedNewDLLs;
    NS_SetHasLoadedNewDLLs();
    NS_NewWindowsRegKey(nsnull);
#if defined (DEBUG) && !defined (WINCE)
    PurePrintf(0);
#endif
#endif

    NS_NewThread(nsnull, nsnull);
    NS_GetCurrentThread(nsnull);
    NS_GetCurrentThread();
    NS_GetMainThread(nsnull);
    NS_DispatchToCurrentThread(nsnull);
    NS_DispatchToMainThread(nsnull, 0);
    NS_ProcessPendingEvents(nsnull, 0);
    NS_HasPendingEvents(nsnull);
    NS_ProcessNextEvent(nsnull, PR_FALSE);
    Mutex theMutex("dummy");
    Monitor theMonitor("dummy2");
    CondVar theCondVar(theMutex, "dummy3");
    TimeStamp theTimeStamp = TimeStamp::Now();
    TimeDuration theTimeDuration = TimeDuration::FromMilliseconds(0);

    NS_WildCardValid((const char *)nsnull);
    NS_WildCardValid((const PRUnichar *)nsnull);
    NS_WildCardMatch((const char *)nsnull, (const char *)nsnull, PR_FALSE);
    NS_WildCardMatch((const PRUnichar *)nsnull, (const PRUnichar *)nsnull, PR_FALSE);
    XRE_AddStaticComponent(NULL);
    XRE_AddManifestLocation(NS_COMPONENT_LOCATION, NULL);
}
