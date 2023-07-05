# gpu-alloc

[![crates](https://img.shields.io/crates/v/gpu-alloc.svg?style=for-the-badge&label=gpu-alloc)](https://crates.io/crates/gpu-alloc)
[![docs](https://img.shields.io/badge/docs.rs-gpu--alloc-66c2a5?style=for-the-badge&labelColor=555555&logoColor=white)](https://docs.rs/gpu-alloc)
[![actions](https://img.shields.io/github/workflow/status/zakarumych/gpu-alloc/Rust/main?style=for-the-badge)](https://github.com/zakarumych/gpu-alloc/actions?query=workflow%3ARust)
[![MIT/Apache](https://img.shields.io/badge/license-MIT%2FApache-blue.svg?style=for-the-badge)](COPYING)
![loc](https://img.shields.io/tokei/lines/github/zakarumych/gpu-alloc?style=for-the-badge)


Implementation agnostic memory allocator for Vulkan like APIs.

This crate is intended to be used as part of safe API implementations.\
Use with caution. There are unsafe functions all over the place.

## Usage

Start with fetching `DeviceProperties` from `gpu-alloc-<backend>` crate for the backend of choice.\
Then create `GpuAllocator` instance and use it for all device memory allocations.\
`GpuAllocator` will take care for all necessary bookkeeping like memory object count limit,
heap budget and memory mapping.

#### Backends implementations

Backend supporting crates should not depend on this crate.\
Instead they should depend on `gpu-alloc-types` which is much more stable,
allowing to upgrade `gpu-alloc` version without `gpu-alloc-<backend>` upgrade.


Supported Rust Versions

The minimum supported version is 1.40.
The current version is not guaranteed to build on Rust versions earlier than the minimum supported version.

`gpu-alloc-erupt` crate requires version 1.48 or higher due to dependency on `erupt` crate.

## License

Licensed under either of

* Apache License, Version 2.0, ([license/APACHE](license/APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
* MIT license ([license/MIT](license/MIT) or http://opensource.org/licenses/MIT)

at your option.

## Contributions

Unless you explicitly state otherwise, any contribution intentionally submitted for inclusion in the work by you, as defined in the Apache-2.0 license, shall be dual licensed as above, without any additional terms or conditions.

## Donate

[![Become a patron](https://c5.patreon.com/external/logo/become_a_patron_button.png)](https://www.patreon.com/zakarum)
