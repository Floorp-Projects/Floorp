/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5ContentCreatorFunction_h
#define nsHtml5ContentCreatorFunction_h

#include "nsGenericHTMLElement.h"
#include "mozilla/dom/SVGElementFactory.h"

union nsHtml5ContentCreatorFunction
{
  mozilla::dom::HTMLContentCreatorFunction html;
  mozilla::dom::SVGContentCreatorFunction svg;
};

#endif // nsHtml5ContentCreatorFunction_h
