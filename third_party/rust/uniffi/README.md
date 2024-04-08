# UniFFI - a multi-language bindings generator for Rust

UniFFI is a toolkit for building cross-platform software components in Rust.

For the impatient, see [**the UniFFI user guide**](https://mozilla.github.io/uniffi-rs/)
or [**the UniFFI examples**](https://github.com/mozilla/uniffi-rs/tree/main/examples#example-uniffi-components).

By writing your core business logic in Rust and describing its interface in an "object model",
you can use UniFFI to help you:

* Compile your Rust code into a shared library for use on different target platforms.
* Generate bindings to load and use the library from different target languages.

You can describe your object model in an [interface definition file](https://mozilla.github.io/uniffi-rs/udl_file_spec.html)
or [by using proc-macros](https://mozilla.github.io/uniffi-rs/proc_macro/index.html).

UniFFI is currently used extensively by Mozilla in Firefox mobile and desktop browsers;
written once in Rust, auto-generated bindings allow that functionality to be called
from both Kotlin (for Android apps) and Swift (for iOS apps).
It also has a growing community of users shipping various cool things to many users.

UniFFI comes with support for **Kotlin**, **Swift**, **Python** and **Ruby** with 3rd party bindings available for **C#** and **Golang**.
Additional foreign language bindings can be developed externally and we welcome contributions to list them here.
See [Third-party foreign language bindings](#third-party-foreign-language-bindings).

## User Guide

You can read more about using the tool in [**the UniFFI user guide**](https://mozilla.github.io/uniffi-rs/).

We consider it ready for production use, but UniFFI is a long way from a 1.0 release with lots of internal work still going on.
We try hard to avoid breaking simple consumers, but more advanced things might break as you upgrade over time.

### Etymology and Pronunciation

ˈjuːnɪfaɪ. Pronounced to rhyme with "unify".

A portmanteau word that also puns with "unify", to signify the joining of one codebase accessed from many languages.

uni - [Latin ūni-, from ūnus, one]
FFI - [Abbreviation, Foreign Function Interface]

## Alternative tools

Other tools we know of which try and solve a similarly shaped problem are:

* [Diplomat](https://github.com/rust-diplomat/diplomat/) - see our [writeup of
  the different approach taken by that tool](docs/diplomat-and-macros.md)
* [Interoptopus](https://github.com/ralfbiedert/interoptopus/)

(Please open a PR if you think other tools should be listed!)

## Third-party foreign language bindings

* [Kotlin Multiplatform support](https://gitlab.com/trixnity/uniffi-kotlin-multiplatform-bindings). The repository contains Kotlin Multiplatform bindings generation for UniFFI, letting you target both JVM and Native.
* [Go bindings](https://github.com/NordSecurity/uniffi-bindgen-go)
* [C# bindings](https://github.com/NordSecurity/uniffi-bindgen-cs)
* [Dart bindings](https://github.com/NiallBunting/uniffi-rs-dart)

### External resources

There are a few third-party resources that make it easier to work with UniFFI:

* [Plugin support for `.udl` files](https://github.com/Lonami/uniffi-dl) for the IDEA platform ([*uniffi-dl* in the JetBrains marketplace](https://plugins.jetbrains.com/plugin/20527-uniffi-dl)). It provides syntax highlighting, code folding, code completion, reference resolution and navigation (among others features) for the [UniFFI Definition Language (UDL)](https://mozilla.github.io/uniffi-rs/).
* [cargo swift](https://github.com/antoniusnaumann/cargo-swift), a cargo plugin to build a Swift Package from Rust code. It provides an init command for setting up a UniFFI crate and a package command for building a Swift package from Rust code - without the need for additional configuration or build scripts.
* [Cargo NDK Gradle Plugin](https://github.com/willir/cargo-ndk-android-gradle) allows you to build Rust code using [`cargo-ndk`](https://github.com/bbqsrc/cargo-ndk), which generally makes Android library builds less painful.
* [`uniffi-starter`](https://github.com/ianthetechie/uniffi-starter) is a minimal project demonstrates a wide range of UniFFI in a complete project in a compact manner. It includes a full Android library build process, an XCFramework generation script, and example Swift package structure. 

(Please open a PR if you think other resources should be listed!)

## Contributing

If this tool sounds interesting to you, please help us develop it! You can:

* View the [contributor guidelines](./docs/contributing.md).
* File or work on [issues](https://github.com/mozilla/uniffi-rs/issues) here in GitHub.
* Join discussions in the [#uniffi:mozilla.org](https://matrix.to/#/#uniffi:mozilla.org)
  room on Matrix.

## Code of Conduct

This project is governed by Mozilla's [Community Participation Guidelines](./CODE_OF_CONDUCT.md).
