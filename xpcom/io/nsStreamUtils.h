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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
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

#ifndef nsStreamUtils_h__
#define nsStreamUtils_h__

#include "nscore.h"

class nsIInputStream;
class nsIOutputStream;
class nsIInputStreamCallback;
class nsIOutputStreamCallback;
class nsIEventTarget;

/**
 * A "one-shot" proxy of the OnInputStreamReady callback.  The resulting
 * proxy object's OnInputStreamReady function may only be called once!  The
 * proxy object ensures that the real notify object will be free'd on the
 * thread corresponding to the given event target regardless of what thread
 * the proxy object is destroyed on.
 *
 * This function is designed to be used to implement AsyncWait when the
 * aEventTarget parameter is non-null.
 */
extern NS_COM nsresult
NS_NewInputStreamReadyEvent(nsIInputStreamCallback **aEvent,
                            nsIInputStreamCallback  *aNotify,
                            nsIEventTarget          *aEventTarget);

/**
 * A "one-shot" proxy of the OnOutputStreamReady callback.  The resulting
 * proxy object's OnOutputStreamReady function may only be called once!  The
 * proxy object ensures that the real notify object will be free'd on the
 * thread corresponding to the given event target regardless of what thread 
 * the proxy object is destroyed on.
 *
 * This function is designed to be used to implement AsyncWait when the
 * aEventTarget parameter is non-null.
 */
extern NS_COM nsresult
NS_NewOutputStreamReadyEvent(nsIOutputStreamCallback **aEvent,
                             nsIOutputStreamCallback  *aNotify,
                             nsIEventTarget           *aEventTarget);

/* ------------------------------------------------------------------------- */

enum nsAsyncCopyMode {
    NS_ASYNCCOPY_VIA_READSEGMENTS,
    NS_ASYNCCOPY_VIA_WRITESEGMENTS
};

/**
 * This function is called when the async copy process completes.  The reported
 * status is NS_OK on success and some error code on failure.
 */
typedef void (* nsAsyncCopyCallbackFun)(void *closure, nsresult status);

/**
 * This function asynchronously copies data from the source to the sink. All
 * data transfer occurs on the thread corresponding to the given event target.
 * A null event target is not permitted.
 *
 * The copier handles blocking or non-blocking streams transparently.  If a
 * stream operation returns NS_BASE_STREAM_WOULD_BLOCK, then the stream will
 * be QI'd to nsIAsync{In,Out}putStream and its AsyncWait method will be used
 * to determine when to resume copying.
 */
extern NS_COM nsresult
NS_AsyncCopy(nsIInputStream         *aSource,
             nsIOutputStream        *aSink,
             nsIEventTarget         *aEventTarget,
             nsAsyncCopyMode         aMode = NS_ASYNCCOPY_VIA_READSEGMENTS,
             PRUint32                aChunkSize = 4096,
             nsAsyncCopyCallbackFun  aCallbackFun = nsnull,
             void                   *aCallbackClosure = nsnull);

#endif // !nsStreamUtils_h__
