/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Force references to all of the symbols that we want exported from
// the dll that are located in the .lib files we link with

#include "nsVoidArray.h"
#include "nsVoidBTree.h"
#include "nsIAtom.h"
#include "nsFileSpec.h"
//#include "nsIBuffer.h"
//#include "nsIByteBufferInputStream.h"
#include "nsFileStream.h"
#include "nsFileSpecStreaming.h"
#include "nsFixedSizeAllocator.h"
#include "nsSpecialSystemDirectory.h"
#include "nsIThread.h"
#include "nsDeque.h"
#include "nsObserver.h"
#include "nsTraceRefcnt.h"
#include "nsXPIDLString.h"
#include "nsIEnumerator.h"
#include "nsEnumeratorUtils.h"
#include "nsQuickSort.h"
#include "nsString.h"
#include "nsPrintfCString.h"
#include "nsSupportsArray.h"
#include "nsProxyEventPrivate.h"
#include "xpt_xdr.h"
#include "xptcall.h"
#include "nsIFileSpec.h"
#include "nsILocalFile.h"
#include "nsIGenericFactory.h"
#include "nsAVLTree.h"
#include "nsHashtableEnumerator.h"
#include "nsIPipe.h"
#include "nsCWeakReference.h"
#include "nsWeakReference.h"
#include "nsISizeOfHandler.h"
#include "nsTextFormatter.h"
#include "nsStatistics.h"
#include "nsStorageStream.h"
#include "nsLinebreakConverter.h"
#include "nsIBinaryInputStream.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIByteArrayInputStream.h"
#include "nsReadableUtils.h"
#include "nsStaticNameTable.h"
#include "nsProcess.h"
#include "nsSlidingString.h"
#include "nsIInputStreamTee.h"
#ifdef DEBUG
#include "pure.h"
#endif
#include "pldhash.h"
#ifdef NS_TRACE_MALLOC
#include "nsTraceMalloc.h"
#endif

class dummyComparitor: public nsAVLNodeComparitor {
public:
  virtual PRInt32 operator()(void* anItem1,void* anItem2)
  {
    return 0;
  }
}; 

#ifdef DEBUG
extern NS_COM void
TestSegmentedBuffer();
#endif

void XXXNeverCalled()
{
    nsTextFormatter::snprintf(nsnull,0,nsnull);
    nsTextFormatter::smprintf(nsnull, nsnull);
    nsTextFormatter::smprintf_free(nsnull);
    dummyComparitor dummy;
    nsVoidArray();
    nsVoidBTree();
    nsAVLTree(dummy, nsnull);
    nsSupportsArray();
    NS_GetNumberOfAtoms();
    nsFileURL(NULL);
    NS_NewPipe(NULL, NULL, 0, 0, PR_FALSE, PR_FALSE, NULL);
    nsFileSpec s;
    nsFixedSizeAllocator a;
    a.Init(0, 0, 0, 0, 0);
    a.Alloc(0);
    a.Free(0, 0);
    NS_NewIOFileStream(NULL, s, 0, 0);
    nsInputFileStream(s, 0, 0);
    nsPersistentFileDescriptor d;
    ReadDescriptor(NULL, d);
    new nsSpecialSystemDirectory(nsSpecialSystemDirectory::OS_DriveDirectory);
    nsIThread::GetCurrent(NULL);
    nsDeque(NULL);
    NS_NewObserver(NULL, NULL);
    nsTraceRefcnt::DumpStatistics();
    NS_NewEmptyEnumerator(NULL);
    nsArrayEnumerator(NULL);
    NS_NewIntersectionEnumerator(NULL, NULL, NULL);
    NS_QuickSort(NULL, 0, 0, NULL, NULL);
    nsStatistics("dummy");
    nsString();
    nsProxyObject(NULL, 0, NULL);
    XPT_DoString(NULL, NULL, NULL);
    XPT_DoHeader(NULL, NULL, NULL);
#ifdef DEBUG
    PurePrintf(0);
#endif
    XPTC_InvokeByIndex(NULL, 0, 0, NULL);
    NS_NewFileSpec(NULL);
    xptc_dummy();
    xptc_dummy2();
    XPTI_GetInterfaceInfoManager();
    NS_NewGenericFactory(NULL, NULL);
    NS_NewGenericModule(NULL, 0, NULL, NULL, NULL);
    NS_NewGenericModule2(NULL, NULL);
    NS_NewHashtableEnumerator(NULL, NULL, NULL, NULL);
    nsCWeakProxy(0, 0);
    nsCWeakReferent(0);
    NS_GetWeakReference(NULL);
    nsCOMPtr<nsISupports> dummyFoo(do_GetInterface(nsnull));
#ifdef DEBUG
    TestSegmentedBuffer();
#endif
    NS_NewSizeOfHandler(0);
    nsStorageStream();
    NS_NewBinaryInputStream(0, 0);
    nsString foo;
    nsPrintfCString bar("");
    nsLinebreakConverter::ConvertStringLineBreaks(foo, 
    nsLinebreakConverter::eLinebreakAny, nsLinebreakConverter::eLinebreakContent);
    NS_NewLocalFile(NULL, PR_FALSE, NULL);
    nsProcess();
    NS_NewByteArrayInputStream (NULL, NULL, 0);
    nsStaticCaseInsensitiveNameTable();
    nsAutoString str1;
    nsCAutoString str2;
    ToNewUnicode(str1);
    ToNewUnicode(str2);
    ToNewCString(str1);
    ToNewCString(str2);
    PL_DHashTableFinish(NULL);
    nsSlidingString sliding(NULL, NULL, NULL);
    NS_NewInputStreamTee(NULL, NULL, NULL);
#ifdef NS_TRACE_MALLOC
    NS_TraceMallocStartup(0);
    NS_TraceMallocStartupArgs(0, NULL);
    NS_TraceMallocShutdown();
    NS_TraceMallocDisable();
    NS_TraceMallocEnable();
    NS_TraceMallocChangeLogFD(0);
    NS_TraceMallocCloseLogFD(0);
    NS_TraceMallocLogTimestamp(NULL);
    NS_TraceMallocDumpAllocations(NULL);
    NS_TraceMallocFlushLogfiles();
#endif
}
