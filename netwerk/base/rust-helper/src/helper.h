#ifndef RUST_NS_NET_HELPER
#define RUST_NS_NET_HELPER

#include "nsError.h"
#include "nsString.h"

extern "C" {

nsresult rust_prepare_accept_languages(const nsACString* i_accept_languages,
                                       nsACString* o_accept_languages);

bool rust_net_is_valid_ipv4_addr(const nsACString& aAddr);

bool rust_net_is_valid_ipv6_addr(const nsACString& aAddr);
}

#endif  // RUST_NS_NET_HELPER
