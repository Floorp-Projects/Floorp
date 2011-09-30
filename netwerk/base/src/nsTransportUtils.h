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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2003
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   IBM Corp.
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

#ifndef nsTransportUtils_h__
#define nsTransportUtils_h__

#include "nsITransport.h"

/**
 * This function returns a proxy object for a transport event sink instance.
 * The transport event sink will be called on the thread indicated by the
 * given event target.  Like events are automatically coalesced.  This means
 * that for example if the status value is the same from event to event, and
 * the previous event has not yet been delivered, then only one event will
 * be delivered.  The progress reported will be that from the second event.
 * If aCoalesceAllEvents is true, then any undelivered event will be replaced
 * with the next event if it arrives early enough.  This option should be used
 * cautiously since it can cause states to be effectively skipped.  Coalescing
 * events can help prevent a backlog of unprocessed transport events in the
 * case that the target thread is overworked.
 */
NS_HIDDEN_(nsresult)
net_NewTransportEventSinkProxy(nsITransportEventSink **aResult,
                               nsITransportEventSink *aSink,
                               nsIEventTarget *aTarget,
                               bool aCoalesceAllEvents = false);

#endif // nsTransportUtils_h__
