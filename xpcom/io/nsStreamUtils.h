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

class nsIAsyncInputStream;
class nsIAsyncOutputStream;
class nsIInputStreamNotify;
class nsIOutputStreamNotify;
class nsIEventQueue;
class nsIMemory;

/**
 * A "one-shot" proxy of the OnInputStreamReady callback.  The resulting
 * proxy object's OnInputStreamReady function may only be called once!  The
 * proxy object ensures that the real notify object will be free'd on the
 * thread owning the given event queue regardless of what thread the proxy
 * object is destroyed on.
 *
 * This function is designed to be used to implement AsyncWait when the
 * aEventQ parameter is non-null.
 */
extern NS_COM nsresult
NS_NewInputStreamReadyEvent(nsIInputStreamNotify **aEvent,
                            nsIInputStreamNotify *aNotify,
                            nsIEventQueue *aEventQ);

/**
 * A "one-shot" proxy of the OnOutputStreamReady callback.  The resulting
 * proxy object's OnOutputStreamReady function may only be called once!  The
 * proxy object ensures that the real notify object will be free'd on the
 * thread owning the given event queue regardless of what thread the proxy
 * object is destroyed on.
 *
 * This function is designed to be used to implement AsyncWait when the
 * aEventQ parameter is non-null.
 */
extern NS_COM nsresult
NS_NewOutputStreamReadyEvent(nsIOutputStreamNotify **aEvent,
                             nsIOutputStreamNotify *aNotify,
                             nsIEventQueue *aEventQ);

/**
 * Asynchronously copy data from an input stream to an output stream.
 *
 * @param aSource
 *        the source input stream containing the data to copy.
 * @param aSink
 *        the data is copied to this output stream.
 * @param aChunkSize
 *        read/write at most this many bytes at one time.
 * @param aBufferedSource
 *        true if source implements ReadSegments.
 * @param aBufferedSink
 *        true if sink implements WriteSegments.
 *
 * NOTE: the implementation does not rely on any event queues.
 */
extern NS_COM nsresult
NS_AsyncCopy(nsIAsyncInputStream *aSource,
             nsIAsyncOutputStream *aSink,
             PRBool aBufferedSource = PR_TRUE,
             PRBool aBufferedSink = PR_TRUE,
             PRUint32 aSegmentSize = 4096,
             PRUint32 aSegmentCount = 1,
             nsIMemory *aSegmentAlloc = nsnull);

#endif // !nsStreamUtils_h__
