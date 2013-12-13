/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsHtml5PlainTextUtils.h"
#include "nsHtml5AttributeName.h"
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"
#include "mozilla/Preferences.h"

// static
nsHtml5HtmlAttributes*
nsHtml5PlainTextUtils::NewLinkAttributes()
{
  nsHtml5HtmlAttributes* linkAttrs = new nsHtml5HtmlAttributes(0);
  nsString* rel = new nsString(NS_LITERAL_STRING("alternate stylesheet"));
  linkAttrs->addAttribute(nsHtml5AttributeName::ATTR_REL, rel);
  nsString* type = new nsString(NS_LITERAL_STRING("text/css"));
  linkAttrs->addAttribute(nsHtml5AttributeName::ATTR_TYPE, type);
  nsString* href = new nsString(
      NS_LITERAL_STRING("resource://gre-resources/plaintext.css"));
  linkAttrs->addAttribute(nsHtml5AttributeName::ATTR_HREF, href);

  nsresult rv;
  nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  NS_ASSERTION(NS_SUCCEEDED(rv) && bundleService, "The bundle service could not be loaded");
  nsCOMPtr<nsIStringBundle> bundle;
  rv = bundleService->CreateBundle("chrome://global/locale/browser.properties",
                                   getter_AddRefs(bundle));
  NS_ASSERTION(NS_SUCCEEDED(rv) && bundle, "chrome://global/locale/browser.properties could not be loaded");
  nsXPIDLString title;
  if (bundle) {
    bundle->GetStringFromName(MOZ_UTF16("plainText.wordWrap"), getter_Copies(title));
  }

  nsString* titleCopy = new nsString(title);
  linkAttrs->addAttribute(nsHtml5AttributeName::ATTR_TITLE, titleCopy);
  return linkAttrs;
}
