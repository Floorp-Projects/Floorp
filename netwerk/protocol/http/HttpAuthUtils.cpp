/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/HttpAuthUtils.h"
#include "mozilla/Tokenizer.h"
#include "nsIPrefService.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsUnicharUtils.h"

namespace mozilla {
namespace net {
namespace auth {

namespace detail {

bool
MatchesBaseURI(const nsACString& matchScheme,
               const nsACString& matchHost,
               int32_t             matchPort,
               nsDependentCSubstring const& url)
{
  // check if scheme://host:port matches baseURI

  // parse the base URI
  mozilla::Tokenizer t(url);
  mozilla::Tokenizer::Token token;

  t.SkipWhites();

  // We don't know if the url to check against starts with scheme
  // or a host name.  Start recording here.
  t.Record();

  mozilla::Unused << t.Next(token);

  // The ipv6 literals MUST be enclosed with [] in the preference.
  bool ipv6 = false;
  if (token.Equals(mozilla::Tokenizer::Token::Char('['))) {
    nsDependentCSubstring ipv6BareLiteral;
    if (!t.ReadUntil(mozilla::Tokenizer::Token::Char(']'), ipv6BareLiteral)) {
      // Broken ipv6 literal
      return false;
    }

    nsDependentCSubstring ipv6Literal;
    t.Claim(ipv6Literal, mozilla::Tokenizer::INCLUDE_LAST);
    if (!matchHost.Equals(ipv6Literal, nsCaseInsensitiveUTF8StringComparator()) &&
        !matchHost.Equals(ipv6BareLiteral, nsCaseInsensitiveUTF8StringComparator())) {
      return false;
    }

    ipv6 = true;
  } else if (t.CheckChar(':') && t.CheckChar('/') && t.CheckChar('/')) {
    if (!matchScheme.Equals(token.Fragment())) {
      return false;
    }
    // Re-start recording the hostname from the point after scheme://.
    t.Record();
  }

  while (t.Next(token)) {
    bool eof = token.Equals(mozilla::Tokenizer::Token::EndOfFile());
    bool port = token.Equals(mozilla::Tokenizer::Token::Char(':'));

    if (eof || port) {
      if (!ipv6) { // Match already performed above.
        nsDependentCSubstring hostName;
        t.Claim(hostName);

        // An empty hostname means to accept everything for the schema
        if (!hostName.IsEmpty()) {
          /*
           host:      bar.com   foo.bar.com   foobar.com  foo.bar.com    bar.com
           pref:      bar.com       bar.com      bar.com     .bar.com   .bar.com
           result:     accept        accept       reject       accept     reject
          */
          if (!StringEndsWith(matchHost, hostName, nsCaseInsensitiveUTF8StringComparator())) {
            return false;
          }
          if (matchHost.Length() > hostName.Length() &&
              matchHost[matchHost.Length() - hostName.Length() - 1] != '.' &&
              hostName[0] != '.') {
            return false;
          }
        }
      }

      if (port) {
        uint16_t portNumber;
        if (!t.ReadInteger(&portNumber)) {
          // Missing port number
          return false;
        }
        if (matchPort != portNumber) {
          return false;
        }
        if (!t.CheckEOF()) {
          return false;
        }
      }
    } else if (ipv6) {
      // After an ipv6 literal there can only be EOF or :port.  Everything else
      // must be treated as non-match/broken input.
      return false;
    }
  }

  // All negative checks has passed positively.
  return true;
}

} // namespace detail


bool
URIMatchesPrefPattern(nsIURI *uri, const char *pref)
{
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (!prefs) {
    return false;
  }

  nsAutoCString scheme, host;
  int32_t port;

  if (NS_FAILED(uri->GetScheme(scheme))) {
    return false;
  }
  if (NS_FAILED(uri->GetAsciiHost(host))) {
    return false;
  }

  port = NS_GetRealPort(uri);
  if (port == -1) {
    return false;
  }

  char *hostList;
  if (NS_FAILED(prefs->GetCharPref(pref, &hostList)) || !hostList) {
    return false;
  }

  struct FreePolicy { void operator()(void* p) { free(p); } };
  mozilla::UniquePtr<char[], FreePolicy> hostListScope;
  hostListScope.reset(hostList);

  // pseudo-BNF
  // ----------
  //
  // url-list       base-url ( base-url "," LWS )*
  // base-url       ( scheme-part | host-part | scheme-part host-part )
  // scheme-part    scheme "://"
  // host-part      host [":" port]
  //
  // for example:
  //   "https://, http://office.foo.com"
  //

  mozilla::Tokenizer t(hostList);
  while (!t.CheckEOF()) {
    t.SkipWhites();
    nsDependentCSubstring url;
    mozilla::Unused << t.ReadUntil(mozilla::Tokenizer::Token::Char(','), url);
    if (url.IsEmpty()) {
      continue;
    }
    if (detail::MatchesBaseURI(scheme, host, port, url)) {
      return true;
    }
  }

  return false;
}

} // namespace auth
} // namespace net
} // namespace mozilla
