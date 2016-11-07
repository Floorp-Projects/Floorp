/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __RUST_URL_CAPI
#define __RUST_URL_CAPI
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rusturl;
typedef struct rusturl* rusturl_ptr;

rusturl_ptr rusturl_new(const char *spec, size_t src_len);
void rusturl_free(rusturl_ptr url);

int32_t rusturl_get_spec(rusturl_ptr url, void*);
int32_t rusturl_get_scheme(rusturl_ptr url, void*);
int32_t rusturl_get_username(rusturl_ptr url, void*);
int32_t rusturl_get_password(rusturl_ptr url, void*);
int32_t rusturl_get_host(rusturl_ptr url, void*);
int32_t rusturl_get_port(rusturl_ptr url); // returns port or -1
int32_t rusturl_get_path(rusturl_ptr url, void*);
int32_t rusturl_get_query(rusturl_ptr url, void*);
int32_t rusturl_get_fragment(rusturl_ptr url, void*);
int32_t rusturl_has_fragment(rusturl_ptr url); // 1 true, 0 false, < 0 error

int32_t rusturl_set_scheme(rusturl_ptr url, const char *scheme, size_t len);
int32_t rusturl_set_username(rusturl_ptr url, const char *user, size_t len);
int32_t rusturl_set_password(rusturl_ptr url, const char *pass, size_t len);
int32_t rusturl_set_host_and_port(rusturl_ptr url, const char *hostport, size_t len);
int32_t rusturl_set_host(rusturl_ptr url, const char *host, size_t len);
int32_t rusturl_set_port(rusturl_ptr url, const char *port, size_t len);
int32_t rusturl_set_port_no(rusturl_ptr url, const int32_t port);
int32_t rusturl_set_path(rusturl_ptr url, const char *path, size_t len);
int32_t rusturl_set_query(rusturl_ptr url, const char *path, size_t len);
int32_t rusturl_set_fragment(rusturl_ptr url, const char *path, size_t len);

int32_t rusturl_resolve(rusturl_ptr url, const char *relative, size_t len, void*);
int32_t rusturl_common_base_spec(rusturl_ptr url1, rusturl_ptr url2, void*);
int32_t rusturl_relative_spec(rusturl_ptr url1, rusturl_ptr url2, void*);

size_t sizeof_rusturl();

#ifdef __cplusplus
}
#endif

#endif // __RUST_URL_CAPI
