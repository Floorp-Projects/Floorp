/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GTKENUMS_WRAPPER_H
#define GTKENUMS_WRAPPER_H

#include_next <gtk/gtkenums.h>

#include <gtk/gtkversion.h>

#if !GTK_CHECK_VERSION(3, 6, 0)
enum GtkInputPurpose {
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

enum GtkInputHints {
  GTK_INPUT_HINT_NONE = 0,
  GTK_INPUT_HINT_SPELLCHECK = 1 << 0,
  GTK_INPUT_HINT_NO_SPELLCHECK = 1 << 1,
  GTK_INPUT_HINT_WORD_COMPLETION = 1 << 2,
  GTK_INPUT_HINT_LOWERCASE = 1 << 3,
  GTK_INPUT_HINT_UPPERCASE_CHARS = 1 << 4,
  GTK_INPUT_HINT_UPPERCASE_WORDS = 1 << 5,
  GTK_INPUT_HINT_UPPERCASE_SENTENCES = 1 << 6,
  GTK_INPUT_HINT_INHIBIT_OSK = 1 << 7
};
#endif  // 3.6.0

#endif /* GTKENUMS_WRAPPER_H */
