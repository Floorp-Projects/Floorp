/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __RUST_URL_CAPI
#define __RUST_URL_CAPI
#include <stdlib.h>
#include "nsString.h"

extern "C" {

// NOTE: Preconditions
// * All nsACString* pointers are unchecked, and must be non-null
// * The int32_t* and bool* outparameter pointer is unchecked, and must
//   be non-null.
// * All rusturl* pointers must refer to pointers which are returned
//   by rusturl_new, and must be freed with rusturl_free.

// The `rusturl` opaque type is equivalent to the rust type `::url::Url`
struct rusturl;

rusturl* rusturl_new(const nsACString* spec);
/* unsafe */ void rusturl_free(rusturl* url);

nsresult rusturl_get_spec(const rusturl* url, nsACString* cont);
nsresult rusturl_get_scheme(const rusturl* url, nsACString* cont);
nsresult rusturl_get_username(const rusturl* url, nsACString* cont);
nsresult rusturl_get_password(const rusturl* url, nsACString* cont);
nsresult rusturl_get_host(const rusturl* url, nsACString* cont);
nsresult rusturl_get_port(const rusturl* url, int32_t* port);
nsresult rusturl_get_path(const rusturl* url, nsACString* cont);
nsresult rusturl_get_query(const rusturl* url, nsACString* cont);
nsresult rusturl_get_fragment(const rusturl* url, nsACString* cont);
nsresult rusturl_has_fragment(const rusturl* url, bool* has_fragment);

nsresult rusturl_set_scheme(rusturl* url, const nsACString* scheme);
nsresult rusturl_set_username(rusturl* url, const nsACString* user);
nsresult rusturl_set_password(rusturl* url, const nsACString* password);
nsresult rusturl_set_host_port(rusturl* url, const nsACString* hostport);
nsresult rusturl_set_host_and_port(rusturl* url, const nsACString* hostport);
nsresult rusturl_set_host(rusturl* url, const nsACString* host);
nsresult rusturl_set_port(rusturl* url, const nsACString* port);
nsresult rusturl_set_port_no(rusturl* url, const int32_t port);
nsresult rusturl_set_path(rusturl* url, const nsACString* path);
nsresult rusturl_set_query(rusturl* url, const nsACString* query);
nsresult rusturl_set_fragment(rusturl* url, const nsACString* fragment);

nsresult rusturl_resolve(const rusturl* url, const nsACString* relative, nsACString* cont);
nsresult rusturl_common_base_spec(const rusturl* url1, const rusturl* url2, nsACString* cont);
nsresult rusturl_relative_spec(const rusturl* url1, const rusturl* url2, nsACString* cont);

nsresult rusturl_parse_ipv6addr(const nsACString* input, nsACString* cont);

size_t sizeof_rusturl();

}

#endif // __RUST_URL_CAPI
