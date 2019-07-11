/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHtml5PlainTextUtils.h"
#include "mozilla/StaticPrefs.h"
#include "nsHtml5AttributeName.h"
#include "nsHtml5Portability.h"
#include "nsHtml5String.h"
#include "nsGkAtoms.h"

// static
nsHtml5HtmlAttributes* nsHtml5PlainTextUtils::NewLinkAttributes() {
  nsHtml5HtmlAttributes* linkAttrs = new nsHtml5HtmlAttributes(0);
  nsHtml5String rel = nsHtml5Portability::newStringFromLiteral("stylesheet");
  linkAttrs->addAttribute(nsHtml5AttributeName::ATTR_REL, rel, -1);
  nsHtml5String href = nsHtml5Portability::newStringFromLiteral(
      "resource://content-accessible/plaintext.css");
  linkAttrs->addAttribute(nsHtml5AttributeName::ATTR_HREF, href, -1);
  return linkAttrs;
}

// static
nsHtml5HtmlAttributes* nsHtml5PlainTextUtils::NewBodyAttributes() {
  if (mozilla::StaticPrefs::plain_text_wrap_long_lines()) {
    return nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES;
  }
  nsHtml5HtmlAttributes* bodyAttrs = new nsHtml5HtmlAttributes(0);
  RefPtr<nsAtom> nowrap = nsGkAtoms::nowrap;
  bodyAttrs->addAttribute(nsHtml5AttributeName::ATTR_CLASS,
                          nsHtml5String::FromAtom(nowrap.forget()), -1);
  return bodyAttrs;
}
