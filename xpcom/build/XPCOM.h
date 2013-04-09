/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_XPCOM_h
#define mozilla_XPCOM_h

// NOTE: the following headers are sorted topologically, not alphabetically.
// Do not reorder them without review from bsmedberg.

// system headers required by XPCOM headers

#include <string.h>

// core headers required by pretty much everything else

#include "nscore.h"

#include "nsXPCOMCID.h"
#include "nsXPCOM.h"

#include "nsError.h"
#include "nsDebug.h"
#include "nsMemory.h"
#include "nsTraceRefcnt.h"

#include "nsID.h"

#include "nsISupports.h"

#include "nsTArray.h"
#include "nsTWeakRef.h"

#include "nsCOMPtr.h"
#include "nsCOMArray.h"

#ifndef MOZILLA_INTERNAL_API
#include "nsStringAPI.h"
#else
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsNativeCharsetUtils.h"
#endif

#include "nsISupportsUtils.h"
#include "nsISupportsImpl.h"

// core data structures

#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsBaseHashtable.h"
#include "nsDataHashtable.h"
#include "nsInterfaceHashtable.h"
#include "nsClassHashtable.h"
#include "nsRefPtrHashtable.h"
#include "mozilla/TimeStamp.h"

// interfaces that inherit directly from nsISupports

#include "nsIArray.h"
#include "nsIAtom.h"
#include "nsIAtomService.h"
#include "nsICategoryManager.h"
#include "nsIClassInfo.h"
#include "nsICollection.h"
#include "nsIComponentManager.h"
#include "nsIConsoleListener.h"
#include "nsIConsoleMessage.h"
#include "nsIConsoleService.h"
#include "nsIDebug.h"
#include "nsIDirectoryEnumerator.h"
#include "nsIEnvironment.h"
#include "nsIErrorService.h"
#include "nsIEventTarget.h"
#include "nsIException.h"
#include "nsIExceptionService.h"
#include "nsIFactory.h"
#include "nsIFile.h"
#include "nsIHashable.h"
#include "nsIINIParser.h"
#include "nsIInputStream.h"
#include "nsIInterfaceRequestor.h"
#include "nsILineInputStream.h"
#include "nsIMemory.h"
#include "nsIMemoryReporter.h"
#include "nsIMutable.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIOutputStream.h"
#include "nsIProcess.h"
#include "nsIProgrammingLanguage.h"
#include "nsIProperties.h"
#include "nsIPropertyBag2.h"
#include "nsIRunnable.h"
#include "nsISeekableStream.h"
#include "nsISerializable.h"
#include "nsIServiceManager.h"
#include "nsIScriptableInputStream.h"
#include "nsISimpleEnumerator.h"
#include "nsISimpleUnicharStreamFactory.h"
#include "nsIStreamBufferAccess.h"
#include "nsIStringEnumerator.h"
#include "nsIStorageStream.h"
#include "nsISupportsIterators.h"
#include "nsISupportsPrimitives.h"
#include "nsISupportsPriority.h"
#include "nsIThreadManager.h"
#include "nsITimer.h"
#include "nsITraceRefcnt.h"
#include "nsIUUIDGenerator.h"
#include "nsIUnicharInputStream.h"
#include "nsIUnicharOutputStream.h"
#include "nsIUnicharLineInputStream.h"
#include "nsIVariant.h"
#include "nsIVersionComparator.h"
#include "nsIWritablePropertyBag2.h"

// interfaces that include something above

#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIBinaryInputStream.h"
#include "nsIBinaryOutputStream.h"
#include "nsIConverterInputStream.h"
#include "nsIConverterOutputStream.h"
#include "nsIDebug2.h"
#include "nsIInputStreamTee.h"
#include "nsIMultiplexInputStream.h"
#include "nsIMutableArray.h"
#include "nsIPersistentProperties2.h"
#include "nsIStringStream.h"
#include "nsIThread.h"
#include "nsIThreadPool.h"

// interfaces that include something above

#include "nsILocalFileWin.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIPipe.h"

#ifdef MOZ_WIDGET_COCOA
#include "nsILocalFileMac.h"
#include "nsIMacUtils.h"
#endif

#ifdef XP_OS2
#include "nsILocalFileOS2.h"
#endif

// xpcom/glue utility headers

#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"

#include "nsIWeakReferenceUtils.h"
#include "nsWeakReference.h"

#include "nsArrayEnumerator.h"
#include "nsArrayUtils.h"
#include "nsCRTGlue.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDeque.h"
#include "nsEnumeratorUtils.h"
#include "nsIClassInfoImpl.h"
#include "mozilla/ModuleUtils.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsINIParser.h"
#include "nsProxyRelease.h"
#include "nsTObserverArray.h"
#include "nsTextFormatter.h"
#include "nsThreadUtils.h"
#include "nsVersionComparator.h"
#include "nsXPTCUtils.h"

// xpcom/base utility headers

#include "nsAgg.h"
#include "nsAutoRef.h"
#include "nsInterfaceRequestorAgg.h"

// xpcom/io utility headers

#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"

#endif // mozilla_XPCOM_h
