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
 * Portions created by IBM Corporation are Copyright (C) 2004
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
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
