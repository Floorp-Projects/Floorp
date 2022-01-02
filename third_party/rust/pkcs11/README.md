<!--
Copyright 2017 Marcus Heese

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->

# Rust PKCS#11 Library

[![Build Status](https://travis-ci.org/mheese/rust-pkcs11.svg?branch=master)](https://travis-ci.org/mheese/rust-pkcs11)

This is a library which brings support for PKCS#11 to Rust. It is aiming at having both a very low-level API to map the PKCS#11 functionality to Rust as well as having a higher-level API for more easy usage as well as bringing more safety for programming against PKCS#11.

## Testing

Testing is currently done with [SoftHSM2](https://github.com/opendnssec/SoftHSMv2 "SoftHSM2 Repo"). A trillion thanks to the people at OpenDNSSEC for writing SoftHSM. This makes it possible to develop applications that need to support PKCS#11. I would have no idea what to do without it. (Suggestions are always welcome.)

### Status

Here is a list of the implementation status and plans on what to do next:

- [x] Dynamic loading of PKCS#11 module (thanks to [libloading](https://github.com/nagisa/rust_libloading "libloading Repo"))
- [x] Initializing and Dropping PKCS#11 context
- [x] Implementing Token and PIN Management functions
- [x] Implementing Session Management functions
- [x] Implementing Object Management functions
- [x] Implementing Key Management functions
- [x] Implementing Encryption/Decryption functions (TODO: tests still missing)
- [x] Implementing Message Digest functions (TODO: tests still missing)
- [x] Implementing Signing and MACing (TODO: tests still missing)
- [x] Implementing Verifying of signatures and MACs (TODO: tests still missing)
- [x] Implementing Dual-function cryptographic operations (TODO: tests still missing)
- [x] Implementing Legacy PKCS#11 functions
- [x] Reorganize code of low-level API (too bloated, which we all know is what PKCS#11 is like)
- [x] Import the rest of the C header `pkcs11t.h` types into rust
- [x] Import the rest of the C header `pkcs11f.h` functions into rust
- [ ] C type constants to string converter functions, and the reverse (maybe part of the high-level API?)
- [ ] Design and implement high-level API
- [x] Publish on crates.io (wow, that was easy)
- [ ] Write and Generate Documentation for Rust docs
- [ ] Better Testing (lots of repetitive code + we need a testing framework and different SoftHSM versions for different platforms)
