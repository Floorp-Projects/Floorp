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
// * All rusturl_ptr pointers must refer to pointers which are returned
//   by rusturl_new, and must be freed with rusturl_free.

struct rusturl;
typedef struct rusturl* rusturl_ptr;

rusturl_ptr rusturl_new(const nsACString* spec);
void rusturl_free(rusturl_ptr url);

int32_t rusturl_get_spec(const rusturl_ptr url, nsACString* cont);
int32_t rusturl_get_scheme(const rusturl_ptr url, nsACString* cont);
int32_t rusturl_get_username(const rusturl_ptr url, nsACString* cont);
int32_t rusturl_get_password(const rusturl_ptr url, nsACString* cont);
int32_t rusturl_get_host(const rusturl_ptr url, nsACString* cont);
int32_t rusturl_get_port(const rusturl_ptr url); // returns port or -1
int32_t rusturl_get_path(const rusturl_ptr url, nsACString* cont);
int32_t rusturl_get_query(const rusturl_ptr url, nsACString* cont);
int32_t rusturl_get_fragment(const rusturl_ptr url, nsACString* cont);
int32_t rusturl_has_fragment(const rusturl_ptr url); // 1 true, 0 false, < 0 error

int32_t rusturl_set_scheme(rusturl_ptr url, const nsACString* scheme);
int32_t rusturl_set_username(rusturl_ptr url, const nsACString* user);
int32_t rusturl_set_password(rusturl_ptr url, const nsACString* password);
int32_t rusturl_set_host_port(rusturl_ptr url, const nsACString* hostport);
int32_t rusturl_set_host_and_port(rusturl_ptr url, const nsACString* hostport);
int32_t rusturl_set_host(rusturl_ptr url, const nsACString* host);
int32_t rusturl_set_port(rusturl_ptr url, const nsACString* port);
int32_t rusturl_set_port_no(rusturl_ptr url, const int32_t port);
int32_t rusturl_set_path(rusturl_ptr url, const nsACString* path);
int32_t rusturl_set_query(rusturl_ptr url, const nsACString* query);
int32_t rusturl_set_fragment(rusturl_ptr url, const nsACString* fragment);

int32_t rusturl_resolve(const rusturl_ptr url, const nsACString* relative, nsACString* cont);
int32_t rusturl_common_base_spec(const rusturl_ptr url1, const rusturl_ptr url2, nsACString* cont);
int32_t rusturl_relative_spec(const rusturl_ptr url1, const rusturl_ptr url2, nsACString* cont);

size_t sizeof_rusturl();

}

#endif // __RUST_URL_CAPI
