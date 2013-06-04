/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsIMEPicker
#define __nsIMEPicker

#include "nsIIMEPicker.h"

class nsIMEPicker MOZ_FINAL : public nsIIMEPicker
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIIMEPICKER

  nsIMEPicker();

private:
  ~nsIMEPicker();
};

#endif
