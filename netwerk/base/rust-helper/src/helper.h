#ifndef RUST_NS_NET_HELPER
#define RUST_NS_NET_HELPER

#include "nsError.h"
#include "nsString.h"

extern "C" {

nsresult
rust_prepare_accept_languages(const nsACString* i_accept_languages,
                              nsACString* o_accept_languages);
}

#endif // RUST_NS_NET_HELPER
