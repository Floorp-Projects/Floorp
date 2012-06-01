/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsHtml5ViewSourceUtils.h"
#include "nsHtml5AttributeName.h"
#include "mozilla/Preferences.h"

// static
nsHtml5HtmlAttributes*
nsHtml5ViewSourceUtils::NewBodyAttributes()
{
  nsHtml5HtmlAttributes* bodyAttrs = new nsHtml5HtmlAttributes(0);
  nsString* id = new nsString(NS_LITERAL_STRING("viewsource"));
  bodyAttrs->addAttribute(nsHtml5AttributeName::ATTR_ID, id);

  if (mozilla::Preferences::GetBool("view_source.wrap_long_lines", true)) {
    nsString* klass = new nsString(NS_LITERAL_STRING("wrap"));
    bodyAttrs->addAttribute(nsHtml5AttributeName::ATTR_CLASS, klass);
  }

  PRInt32 tabSize = mozilla::Preferences::GetInt("view_source.tab_size", 4);
  if (tabSize > 0) {
    nsString* style = new nsString(NS_LITERAL_STRING("-moz-tab-size: "));
    style->AppendInt(tabSize);
    bodyAttrs->addAttribute(nsHtml5AttributeName::ATTR_STYLE, style);
  }

  return bodyAttrs;
}

// static
nsHtml5HtmlAttributes*
nsHtml5ViewSourceUtils::NewLinkAttributes()
{
  nsHtml5HtmlAttributes* linkAttrs = new nsHtml5HtmlAttributes(0);
  nsString* rel = new nsString(NS_LITERAL_STRING("stylesheet"));
  linkAttrs->addAttribute(nsHtml5AttributeName::ATTR_REL, rel);
  nsString* type = new nsString(NS_LITERAL_STRING("text/css"));
  linkAttrs->addAttribute(nsHtml5AttributeName::ATTR_TYPE, type);
  nsString* href = new nsString(
      NS_LITERAL_STRING("resource://gre-resources/viewsource.css"));
  linkAttrs->addAttribute(nsHtml5AttributeName::ATTR_HREF, href);
  return linkAttrs;
}
