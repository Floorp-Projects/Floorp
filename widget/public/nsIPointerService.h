/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created Christopher Blizzard are Copyright (C) Christopher
 * Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 */

#ifndef nsIPointerService_h_
#define nsIPointerService_h_

#include <nsIWidget.h>

#define NS_IPOINTERSERVICE_IID \
{ 0xabde6104, 0x1dd1, 0x11b2, { 0x8e, 0x8f, 0xb2, 0xe4, 0x76, 0x8b, 0xbb, 0xfb } };

#define NS_IPOINTERSERVICE_CONTRACTID \
"@mozilla.org/widget/pointer-service;1"

class nsIPointerService : public nsISupports {
 public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPOINTERSERVICE_IID);

  // This method will return the widget that is currently under the
  // pointer.  If there is no widget under the pointer it will return
  // 0.

  // This call is EXTRAORDINARILY EXPENSIVE UNDER X.  Please don't
  // use it unless you have to.

  NS_IMETHOD WidgetUnderPointer(nsIWidget **_retval,
				PRUint32 *aXOffset, PRUint32 *aYOffset) = 0;

};

#endif /* nsIPointerService_h_ */
