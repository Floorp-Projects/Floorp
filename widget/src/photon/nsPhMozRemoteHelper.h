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
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Adrian Mardare <amaradre@qnx.com>
 */

#ifndef __nsPhMozRemoteHelper_h__
#define __nsPhMozRemoteHelper_h__

#include <nsIXRemoteWidgetHelper.h>

// {84f94aac-1dd2-11b2-a05f-9b338fea662c}

#define NS_PHXREMOTEWIDGETHELPER_CID \
  { 0x84f94aac, 0x1dd2, 0x11b2, \
  { 0xa0, 0x5f, 0x9b, 0x33, 0x8f, 0xea, 0x66, 0x2c } }

class nsPhXRemoteWidgetHelper : public nsIXRemoteWidgetHelper {
 public:
  nsPhXRemoteWidgetHelper();
  virtual ~nsPhXRemoteWidgetHelper();

  NS_DECL_ISUPPORTS

  NS_IMETHOD EnableXRemoteCommands(nsIWidget *aWidget);
};

#endif /* __nsPhMozRemoteHelper_h__ */
