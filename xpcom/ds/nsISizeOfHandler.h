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
#ifndef nsISizeOfHandler_h___
#define nsISizeOfHandler_h___

#include "nscore.h"
#include "nsISupports.h"

/* c028d1f0-fc9e-11d1-89e4-006008911b81 */
#define NS_ISIZEOF_HANDLER_IID \
{ 0xc028d1f0, 0xfc9e, 0x11d1, {0x89, 0xe4, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81}}

class nsIAtom;
class nsISizeOfHandler;

/**
 * Function used by the Report method to report data gathered during
 * a collection of data.
 */
typedef void (*nsISizeofReportFunc)(nsISizeOfHandler* aHandler,
                                    nsIAtom* aKey,
                                    PRUint32 aCount,
                                    PRUint32 aTotalSize,
                                    PRUint32 aMinSize,
                                    PRUint32 aMaxSize,
                                    void* aArg);

/**
 * An API to managing a sizeof computation of an arbitrary graph.
 * The handler is responsible for remembering which objects have been
 * seen before (using RecordObject). Note that the handler doesn't
 * hold references to nsISupport's objects; the assumption is that the
 * objects being sized are stationary and will not be modified during
 * the sizing computation and therefore do not need an extra reference
 * count.
 *
 * Users of this API are responsible for the actual graph/tree walking.
 */
class nsISizeOfHandler : public nsISupports {
public:
	NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISIZEOF_HANDLER_IID)

  /**
   * Initialize the handler for a new collection of data. This empties
   * out the object prescence table and the keyed size table.
   */
  NS_IMETHOD Init() = 0;

  /**
   * Record the sizing status of a given object. The first time
   * aObject is recorded, aResult will be PR_FALSE. Subsequent times,
   * aResult will be PR_TRUE.
   */
  NS_IMETHOD RecordObject(void* aObject, PRBool* aResult) = 0;

  /**
   * Add size information to the running size data. The atom is used
   * as a key to keep type specific running totals of size
   * information.  This increments the total count and the total size
   * as well as updates the minimum, maximum and total size for aKey's
   * type.
   */
  NS_IMETHOD AddSize(nsIAtom* aKey, PRUint32 aSize) = 0;

  /**
   * Enumerate data collected for each type and invoke the
   * reporting function with the data gathered.
   */
  NS_IMETHOD Report(nsISizeofReportFunc aFunc, void* aArg) = 0;

  /**
   * Get the current totals - the number of total objects sized (not
   * necessarily anything to do with RecordObject's tracking of
   * objects) and the total number of bytes that those object use. The
   * counters are not reset by this call (use Init to reset
   * everything).
   */
  NS_IMETHOD GetTotals(PRUint32* aTotalCountResult,
                       PRUint32* aTotalSizeResult) = 0;
};

extern NS_COM nsresult
NS_NewSizeOfHandler(nsISizeOfHandler** aInstancePtrResult);

#endif /* nsISizeofHandler_h___ */
