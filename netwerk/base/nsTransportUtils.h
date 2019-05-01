/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

 * Coalescing events can help prevent a backlog of unprocessed transport
 * events in the case that the target thread is overworked.
 */
nsresult net_NewTransportEventSinkProxy(nsITransportEventSink** aResult,
                                        nsITransportEventSink* aSink,
                                        nsIEventTarget* aTarget);

#endif  // nsTransportUtils_h__
