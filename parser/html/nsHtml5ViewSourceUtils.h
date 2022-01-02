/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5ViewSourceUtils_h
#define nsHtml5ViewSourceUtils_h

#include "nsHtml5HtmlAttributes.h"

class nsHtml5ViewSourceUtils {
 public:
  static nsHtml5HtmlAttributes* NewBodyAttributes();
  static nsHtml5HtmlAttributes* NewLinkAttributes();
  static nsHtml5HtmlAttributes* NewMetaViewportAttributes();
};

#endif  // nsHtml5ViewSourceUtils_h
