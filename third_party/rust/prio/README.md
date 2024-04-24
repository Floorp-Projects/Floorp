# libprio-rs
[![Latest Version]][crates.io] [![Docs badge]][docs.rs]

[Latest Version]: https://img.shields.io/crates/v/prio.svg
[crates.io]: https://crates.io/crates/prio
[Docs badge]: https://img.shields.io/badge/docs.rs-rustdoc-green
[docs.rs]: https://docs.rs/prio/

Pure Rust implementation of [Prio](https://crypto.stanford.edu/prio/), a system for Private, Robust,
and Scalable Computation of Aggregate Statistics.

## Exposure Notifications Private Analytics

This crate was used in the [Exposure Notifications Private Analytics][enpa]
system. This is referred to in various places as Prio v2. See
[`prio-server`][prio-server] or the [ENPA whitepaper][enpa-whitepaper] for more
details.

## Verifiable Distributed Aggregation Function

This crate also implements a [Verifiable Distributed Aggregation Function
(VDAF)][vdaf] called "Prio3", implemented in the `vdaf` module, allowing Prio to
be used in the [Distributed Aggregation Protocol][dap] protocol being developed
in the PPM working group at the IETF. This support is still evolving along with
the DAP and VDAF specifications.

### Draft versions and release branches

The `main` branch is under continuous development and will usually be partway between VDAF drafts.
libprio uses stable release branches to maintain implementations of different VDAF draft versions.
Crate `prio` version `x.y.z` is released from a corresponding `release/x.y` branch. We try to
maintain [Rust SemVer][semver] compatibility, meaning that API breaks only happen on minor version
increases (e.g., 0.10 to 0.11).

| Crate version | Git branch | VDAF draft version | DAP draft version | Conforms to specification? | Status |
| ----- | ---------- | ------------- | ------------- | --------------------- | ------ |
| 0.8 | `release/0.8` | [`draft-irtf-cfrg-vdaf-01`][vdaf-01] | [`draft-ietf-ppm-dap-01`][dap-01] | Yes | Unmaintained as of March 28, 2023 |
| 0.9 | `release/0.9` | [`draft-irtf-cfrg-vdaf-03`][vdaf-03] | [`draft-ietf-ppm-dap-02`][dap-02] and [`draft-ietf-ppm-dap-03`][dap-03] | Yes | Unmaintained as of September 22, 2022 |
| 0.10 | `release/0.10` | [`draft-irtf-cfrg-vdaf-03`][vdaf-03] | [`draft-ietf-ppm-dap-02`][dap-02] and [`draft-ietf-ppm-dap-03`][dap-03] | Yes | Unmaintained as of November 14, 2023 |
| 0.11 | `release/0.11` | [`draft-irtf-cfrg-vdaf-04`][vdaf-04] | N/A | Yes | Unmaintained |
| 0.12 | `release/0.12` | [`draft-irtf-cfrg-vdaf-05`][vdaf-05] | [`draft-ietf-ppm-dap-04`][dap-04] | Yes | Supported |
| 0.13 | `release/0.13` | [`draft-irtf-cfrg-vdaf-06`][vdaf-06] | [`draft-ietf-ppm-dap-05`][dap-05] | Yes | Unmaintained |
| 0.14 | `release/0.14` | [`draft-irtf-cfrg-vdaf-06`][vdaf-06] | [`draft-ietf-ppm-dap-05`][dap-05] | Yes | Unmaintained |
| 0.15 | `release/0.15` | [`draft-irtf-cfrg-vdaf-07`][vdaf-07] | [`draft-ietf-ppm-dap-07`][dap-07] | Yes | Supported |
| 0.16 | `main` | [`draft-irtf-cfrg-vdaf-08`][vdaf-08] | [`draft-ietf-ppm-dap-09`][dap-09] | Yes | Supported |

[vdaf-01]: https://datatracker.ietf.org/doc/draft-irtf-cfrg-vdaf/01/
[vdaf-03]: https://datatracker.ietf.org/doc/draft-irtf-cfrg-vdaf/03/
[vdaf-04]: https://datatracker.ietf.org/doc/draft-irtf-cfrg-vdaf/04/
[vdaf-05]: https://datatracker.ietf.org/doc/draft-irtf-cfrg-vdaf/05/
[vdaf-06]: https://datatracker.ietf.org/doc/draft-irtf-cfrg-vdaf/06/
[vdaf-07]: https://datatracker.ietf.org/doc/draft-irtf-cfrg-vdaf/07/
[vdaf-08]: https://datatracker.ietf.org/doc/draft-irtf-cfrg-vdaf/08/
[dap-01]: https://datatracker.ietf.org/doc/draft-ietf-ppm-dap/01/
[dap-02]: https://datatracker.ietf.org/doc/draft-ietf-ppm-dap/02/
[dap-03]: https://datatracker.ietf.org/doc/draft-ietf-ppm-dap/03/
[dap-04]: https://datatracker.ietf.org/doc/draft-ietf-ppm-dap/04/
[dap-05]: https://datatracker.ietf.org/doc/draft-ietf-ppm-dap/05/
[dap-07]: https://datatracker.ietf.org/doc/draft-ietf-ppm-dap/07/
[dap-09]: https://datatracker.ietf.org/doc/draft-ietf-ppm-dap/09/
[enpa]: https://www.abetterinternet.org/post/prio-services-for-covid-en/
[enpa-whitepaper]: https://covid19-static.cdn-apple.com/applications/covid19/current/static/contact-tracing/pdf/ENPA_White_Paper.pdf
[prio-server]: https://github.com/divviup/prio-server
[vdaf]: https://datatracker.ietf.org/doc/draft-irtf-cfrg-vdaf/
[dap]: https://datatracker.ietf.org/doc/draft-ietf-ppm-dap/
[semver]: https://doc.rust-lang.org/cargo/reference/semver.html

## Cargo Features

This crate defines the following feature flags:

|Name|Default feature?|Description|Semver stable?|
|---|---|---|---|
|`crypto-dependencies`|Yes|Enables dependencies on various RustCrypto crates, and uses them to implement `XofTurboShake128` to support VDAFs.|✅|
|`experimental`|No|Certain experimental APIs are guarded by this feature.|❌|
|`multithreaded`|No|Enables certain Prio3 VDAF implementations that use `rayon` for parallelization of gadget evaluations.|✅|
|`test-util`|No|Enables test utilities for VDAF users and VDAF implementers.|❌|

Features that are not marked as "Semver stable" may undergo breaking changes in future patch releases, as an exception to semantic versioning.
