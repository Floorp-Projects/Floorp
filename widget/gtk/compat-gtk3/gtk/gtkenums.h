/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GTKENUMS_WRAPPER_H
#define GTKENUMS_WRAPPER_H

#include_next <gtk/gtkenums.h>

#include <gtk/gtkversion.h>

#if !GTK_CHECK_VERSION(3, 6, 0)
enum GtkInputPurpose
{
    GTK_INPUT_PURPOSE_FREE_FORM,
    GTK_INPUT_PURPOSE_ALPHA,
    GTK_INPUT_PURPOSE_DIGITS,
    GTK_INPUT_PURPOSE_NUMBER,
    GTK_INPUT_PURPOSE_PHONE,
    GTK_INPUT_PURPOSE_URL,
    GTK_INPUT_PURPOSE_EMAIL,
    GTK_INPUT_PURPOSE_NAME,
    GTK_INPUT_PURPOSE_PASSWORD,
    GTK_INPUT_PURPOSE_PIN
};
#endif // 3.6.0

#endif /* GTKENUMS_WRAPPER_H */
