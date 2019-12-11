/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_MozURL_ffi_h
#define mozilla_net_MozURL_ffi_h

// The MozURL type is implemented in Rust code, and uses extern "C" FFI calls to
// operate on the internal data structure. This file contains C declarations of
// these files.
//
// WARNING: DO NOT CALL ANY OF THESE FUNCTIONS. USE |MozURL| INSTEAD!

#include "nsString.h"
#include "nsError.h"

namespace mozilla {
namespace net {
class MozURL;
}  // namespace net
}  // namespace mozilla

extern "C" {

// FFI-compatible string slice struct used internally by MozURL.
// Coerces to nsDependentCSubstring.
struct MozURLSpecSlice {
  char* mData;
  uint32_t mLen;

  operator nsDependentCSubstring() {
    return nsDependentCSubstring(mData, mLen);
  }
};

nsrefcnt mozurl_addref(const mozilla::net::MozURL*);
nsrefcnt mozurl_release(const mozilla::net::MozURL*);

nsresult mozurl_new(mozilla::net::MozURL** aResult, const nsACString* aSpec,
                    /* optional */ const mozilla::net::MozURL* aBase);

void mozurl_clone(const mozilla::net::MozURL* aThis,
                  mozilla::net::MozURL** aResult);

// Spec segment getters
MozURLSpecSlice mozurl_spec(const mozilla::net::MozURL*);
MozURLSpecSlice mozurl_scheme(const mozilla::net::MozURL*);
MozURLSpecSlice mozurl_username(const mozilla::net::MozURL*);
MozURLSpecSlice mozurl_password(const mozilla::net::MozURL*);
MozURLSpecSlice mozurl_host(const mozilla::net::MozURL*);
int32_t mozurl_port(const mozilla::net::MozURL*);
int32_t mozurl_real_port(const mozilla::net::MozURL*);
MozURLSpecSlice mozurl_host_port(const mozilla::net::MozURL*);
MozURLSpecSlice mozurl_filepath(const mozilla::net::MozURL*);
MozURLSpecSlice mozurl_path(const mozilla::net::MozURL*);
MozURLSpecSlice mozurl_query(const mozilla::net::MozURL*);
MozURLSpecSlice mozurl_fragment(const mozilla::net::MozURL*);
MozURLSpecSlice mozurl_spec_no_ref(const mozilla::net::MozURL*);

bool mozurl_has_fragment(const mozilla::net::MozURL*);
MozURLSpecSlice mozurl_directory(const mozilla::net::MozURL*);
MozURLSpecSlice mozurl_prepath(const mozilla::net::MozURL*);
void mozurl_origin(const mozilla::net::MozURL*, nsACString* aResult);
nsresult mozurl_base_domain(const mozilla::net::MozURL*, nsACString* aResult);

nsresult mozurl_common_base(const mozilla::net::MozURL* aUrl1,
                            const mozilla::net::MozURL* aUrl2,
                            mozilla::net::MozURL** aResult);
nsresult mozurl_relative(const mozilla::net::MozURL* aUrl1,
                         const mozilla::net::MozURL* aUrl2,
                         nsACString* aResult);

// Mutators. These must only be called when a MozURL is uniquely owned.
// This is debug_assert-ed in the rust code.
nsresult mozurl_set_scheme(mozilla::net::MozURL* aUrl,
                           const nsACString* aScheme);
nsresult mozurl_set_username(mozilla::net::MozURL* aUrl,
                             const nsACString* aUsername);
nsresult mozurl_set_password(mozilla::net::MozURL* aUrl,
                             const nsACString* aPassword);
nsresult mozurl_set_host_port(mozilla::net::MozURL* aUrl,
                              const nsACString* aHostPort);
nsresult mozurl_set_hostname(mozilla::net::MozURL* aUrl,
                             const nsACString* aHostname);
nsresult mozurl_set_port_no(mozilla::net::MozURL* aUrl, int32_t port);
nsresult mozurl_set_pathname(mozilla::net::MozURL* aUrl,
                             const nsACString* aPath);
nsresult mozurl_set_query(mozilla::net::MozURL* aUrl, const nsACString* aQuery);
nsresult mozurl_set_fragment(mozilla::net::MozURL* aUrl,
                             const nsACString* aFragment);

size_t mozurl_sizeof(const mozilla::net::MozURL*);

// Utility function for parsing IPv6 addresses, used by nsStandardURL.h
//
// This function does not follow the mozurl_ naming convention, as it does not
// work on MozURL objects.
nsresult rusturl_parse_ipv6addr(const nsACString* aHost, nsACString* aAddr);

}  // extern "C"

#endif  // !defined(mozilla_net_MozURL_ffi_h)
