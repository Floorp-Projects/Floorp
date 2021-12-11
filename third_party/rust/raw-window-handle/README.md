# `raw-window-handle`: A common windowing interoperability library for Rust
[![Crates.io](https://img.shields.io/crates/v/raw-window-handle.svg?maxAge=2592000)](https://crates.io/crates/raw-window-handle)
[![Docs](https://docs.rs/raw-window-handle/badge.svg)](https://docs.rs/raw-window-handle)
[![Travis Build Status](https://travis-ci.org/rust-windowing/raw-window-handle.svg)](https://travis-ci.org/rust-windowing/raw-window-handle)
[![Appveyor Build Status](https://ci.appveyor.com/api/projects/status/iq3j85x0ruw5y205?svg=true)](https://ci.appveyor.com/project/Osspial/raw-window-handle)

This library provides standard types for accessing a window's platform-specific raw window handle. This does not provide any utilities for creating and managing windows; instead, it provides a common interface that window creation libraries (e.g. Winit, SDL) can use to easily talk with graphics libraries (e.g. gfx-hal).
