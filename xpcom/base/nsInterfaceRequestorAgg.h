/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsInterfaceRequestorAgg_h__
#define nsInterfaceRequestorAgg_h__

#include "nsIInterfaceRequestor.h"

/**
 * This function returns an instance of nsIInterfaceRequestor that aggregates
 * two nsIInterfaceRequestor instances.  Its GetInterface method queries
 * aFirst for the requested interface and will query aSecond only if aFirst
 * failed to supply the requested interface.  Both aFirst and aSecond may
 * be null.
 */
extern nsresult
NS_NewInterfaceRequestorAggregation(nsIInterfaceRequestor  *aFirst,
                                    nsIInterfaceRequestor  *aSecond,
                                    nsIInterfaceRequestor **aResult);

#endif // !defined( nsInterfaceRequestorAgg_h__ )
