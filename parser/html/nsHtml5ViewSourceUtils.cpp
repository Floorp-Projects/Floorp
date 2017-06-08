/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsHtml5ViewSourceUtils.h"
#include "nsHtml5AttributeName.h"
#include "mozilla/Preferences.h"
#include "nsHtml5String.h"

// static
nsHtml5HtmlAttributes*
nsHtml5ViewSourceUtils::NewBodyAttributes()
{
  nsHtml5HtmlAttributes* bodyAttrs = new nsHtml5HtmlAttributes(0);
  nsHtml5String id = nsHtml5Portability::newStringFromLiteral("viewsource");
  bodyAttrs->addAttribute(nsHtml5AttributeName::ATTR_ID, id, -1);

  nsString klass;
  if (mozilla::Preferences::GetBool("view_source.wrap_long_lines", true)) {
    klass.Append(NS_LITERAL_STRING("wrap "));
  }
  if (mozilla::Preferences::GetBool("view_source.syntax_highlight", true)) {
    klass.Append(NS_LITERAL_STRING("highlight"));
  }
  if (!klass.IsEmpty()) {
    bodyAttrs->addAttribute(
      nsHtml5AttributeName::ATTR_CLASS, nsHtml5String::FromString(klass), -1);
  }

  int32_t tabSize = mozilla::Preferences::GetInt("view_source.tab_size", 4);
  if (tabSize > 0) {
    nsString style;
    style.AssignASCII("-moz-tab-size: ");
    style.AppendInt(tabSize);
    bodyAttrs->addAttribute(
      nsHtml5AttributeName::ATTR_STYLE, nsHtml5String::FromString(style), -1);
  }

  return bodyAttrs;
}

// static
nsHtml5HtmlAttributes*
nsHtml5ViewSourceUtils::NewLinkAttributes()
{
  nsHtml5HtmlAttributes* linkAttrs = new nsHtml5HtmlAttributes(0);
  nsHtml5String rel = nsHtml5Portability::newStringFromLiteral("stylesheet");
  linkAttrs->addAttribute(nsHtml5AttributeName::ATTR_REL, rel, -1);
  nsHtml5String type = nsHtml5Portability::newStringFromLiteral("text/css");
  linkAttrs->addAttribute(nsHtml5AttributeName::ATTR_TYPE, type, -1);
  nsHtml5String href = nsHtml5Portability::newStringFromLiteral(
    "resource://content-accessible/viewsource.css");
  linkAttrs->addAttribute(nsHtml5AttributeName::ATTR_HREF, href, -1);
  return linkAttrs;
}
