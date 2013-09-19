/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsInterfaceRequestorAgg_h__
#define nsInterfaceRequestorAgg_h__

#include "nsError.h"

class nsIEventTarget;
class nsIInterfaceRequestor;

/**
 * This function returns an instance of nsIInterfaceRequestor that aggregates
 * two nsIInterfaceRequestor instances.  Its GetInterface method queries
 * aFirst for the requested interface and will query aSecond only if aFirst
 * failed to supply the requested interface.  Both aFirst and aSecond may
 * be null, and will be released on the main thread when the aggregator is
 * destroyed.
 */
extern nsresult
NS_NewInterfaceRequestorAggregation(nsIInterfaceRequestor  *aFirst,
                                    nsIInterfaceRequestor  *aSecond,
                                    nsIInterfaceRequestor **aResult);

/**
 * Like the previous method, but aFirst and aSecond will be released on the
 * provided target thread.
 */
extern nsresult
NS_NewInterfaceRequestorAggregation(nsIInterfaceRequestor  *aFirst,
                                    nsIInterfaceRequestor  *aSecond,
                                    nsIEventTarget         *aTarget,
                                    nsIInterfaceRequestor **aResult);

#endif // !defined( nsInterfaceRequestorAgg_h__ )
