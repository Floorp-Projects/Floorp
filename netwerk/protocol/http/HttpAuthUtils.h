/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HttpAuthUtils_h__
#define HttpAuthUtils_h__

class nsIURI;

namespace mozilla {
namespace net {
namespace auth {

/* Tries to match the given URI against the value of a given pref
 *
 * The pref should be in pseudo-BNF format.
 * url-list       base-url ( base-url "," LWS )*
 * base-url       ( scheme-part | host-part | scheme-part host-part )
 * scheme-part    scheme "://"
 * host-part      host [":" port]
 *
 * for example:
 *   "https://, http://office.foo.com"
 *
 * Will return true if the URI matches any of the patterns, or false otherwise.
 */
bool URIMatchesPrefPattern(nsIURI* uri, const char* pref);

}  // namespace auth
}  // namespace net
}  // namespace mozilla

#endif  // HttpAuthUtils_h__
