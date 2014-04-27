/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIMEPicker.h"
#include "AndroidBridge.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS(nsIMEPicker, nsIIMEPicker)

nsIMEPicker::nsIMEPicker()
{
  /* member initializers and constructor code */
}

nsIMEPicker::~nsIMEPicker()
{
  /* destructor code */
}

/* void show (); */
NS_IMETHODIMP nsIMEPicker::Show()
{
    mozilla::widget::android::GeckoAppShell::ShowInputMethodPicker();
    return NS_OK;
}
