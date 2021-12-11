# Contributing to `packed_simd`

Welcome! If you are reading this document, it means you are interested in contributing
to the `packed_simd` crate.

## Reporting issues

All issues with this crate are tracked using GitHub's [Issue Tracker].

You can use issues to bring bugs to the attention of the maintainers, to discuss
certain problems encountered with the crate, or to request new features (although
feature requests should be limited to things mentioned in the [RFC]).

One thing to keep in mind is to always use the **latest** nightly toolchain when
working on this crate. Due to the nature of this project, we use a lot of unstable
features, meaning breakage happens often.

[Issue Tracker]: https://github.com/rust-lang-nursery/packed_simd/issues
[RFC]: https://github.com/rust-lang/rfcs/pull/2366

### LLVM issues

The Rust compiler relies on [LLVM](https://llvm.org/) for machine code generation,
and quite a few LLVM bugs have been discovered during the development of this project.

If you encounter issues with incorrect/suboptimal codegen, which you do not encounter
when using the [SIMD vendor intrinsics](https://doc.rust-lang.org/nightly/std/arch/),
it is likely the issue is with LLVM, or this crate's interaction with it.

You should first open an issue **in this repo** to help us track the problem, and we
will help determine what is the exact cause of the problem.
If LLVM is indeed the cause, the issue will be reported upstream to the
[LLVM bugtracker](https://bugs.llvm.org/).

## Submitting Pull Requests

New code is submitted to the crate using GitHub's [pull request] mechanism.
You should first fork this repository, make your changes (preferrably in a new
branch), then use GitHub's web UI to create a new PR.

[pull request]: https://help.github.com/articles/about-pull-requests/

### Examples

The `examples` directory contains code showcasing SIMD code written with this crate,
usually in comparison to scalar or ISPC code. If you have a project / idea which
uses SIMD, we'd love to add it to the examples list.

Every example should include a small `README`, describing the example code's purpose.
If your example could potentially work as a benchmark, then add a `benchmark.sh`
script to allow running the example benchmark code in CI. See an existing example's
[`benchmark.sh`](examples/aobench/benchmark.sh) for a sample.

Don't forget to update the crate's top-level `README` with a link to your example.

### Perf guide

The objective of the [performance guide][perf-guide] is to be a comprehensive
resource detailing the process of optimizing Rust code with SIMD support.

If you believe a certain section could be reworded, or if you have any tips & tricks
related to SIMD which you'd like to share, please open a PR.

[mdBook] is used to manage the formatting of the guide as a book.

[perf-guide]: https://rust-lang-nursery.github.io/packed_simd/perf-guide/
[mdBook]: https://github.com/rust-lang-nursery/mdBook
