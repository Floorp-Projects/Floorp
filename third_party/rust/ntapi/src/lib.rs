//! # Features
//! **`func-types`** -- Generate [types][fn_ptr] for external functions.<br/>
//! **`impl-default`** -- Implement [`Default`] for structs and unions.<br/>
//! **`user`** *(default)* -- Link to `ntdll`.<br/>
//! **`kernel`** -- Link to `ntoskrnl` on MSVC targets.<br/>
//! **`nightly`** --  Unlock unstable features that require a nightly compiler:
//! - [`NtCurrentTeb`]
//! - [`__readfsdword`]
//! - [`__readgsqword`]
//! - [`_bittest64`]
//! - [`NtCurrentPeb`]
//! - [`NtCurrentProcessId`]
//! - [`NtCurrentThreadId`]
//! - [`RtlProcessHeap`]
//! - [`RtlCheckBit`] implementation using [`_bittest64`] on x86_64.
//!
//! [`NtCurrentTeb`]: winapi_local/um/winnt/fn.NtCurrentTeb.html
//! [`__readfsdword`]: winapi_local/um/winnt/fn.__readfsdword.html
//! [`__readgsqword`]: winapi_local/um/winnt/fn.__readgsqword.html
//! [`_bittest64`]: winapi_local/um/winnt/fn._bittest64.html
//! [`NtCurrentPeb`]: ntpsapi/fn.NtCurrentPeb.html
//! [`NtCurrentProcessId`]: ntpsapi/fn.NtCurrentProcessId.html
//! [`NtCurrentThreadId`]: ntpsapi/fn.NtCurrentThreadId.html
//! [`RtlProcessHeap`]: ntrtl/fn.RtlProcessHeap.html
//! [`RtlCheckBit`]: ntrtl/fn.RtlCheckBit.html
//! [fn_ptr]: https://doc.rust-lang.org/reference/types.html#function-pointer-types
//! [`Default`]: https://doc.rust-lang.org/std/default/trait.Default.html#tymethod.default
#![cfg(all(windows, any(target_arch = "x86", target_arch = "x86_64", target_arch = "aarch64")))]
#![no_std]
#![deny(unused, unused_qualifications)]
#![warn(unused_attributes)]
#![allow(bad_style, overflowing_literals, unused_macros, clippy::cast_lossless, clippy::cast_ptr_alignment, clippy::len_without_is_empty, clippy::trivially_copy_pass_by_ref, clippy::unreadable_literal)]
#![cfg_attr(feature = "nightly", feature(llvm_asm))]
#[doc(hidden)]
pub extern crate core as _core;
#[macro_use]
#[doc(hidden)]
pub extern crate winapi;
#[macro_use]
mod macros;
pub mod ntapi_base;
pub mod ntdbg;
pub mod ntexapi;
pub mod ntgdi;
pub mod ntioapi;
pub mod ntkeapi;
pub mod ntldr;
pub mod ntlpcapi;
pub mod ntmisc;
pub mod ntmmapi;
pub mod ntnls;
pub mod ntobapi;
pub mod ntpebteb;
pub mod ntpfapi;
pub mod ntpnpapi;
pub mod ntpoapi;
pub mod ntpsapi;
pub mod ntregapi;
pub mod ntrtl;
pub mod ntsam;
pub mod ntseapi;
pub mod ntsmss;
pub mod nttmapi;
pub mod nttp;
pub mod ntwow64;
pub mod ntxcapi;
pub mod ntzwapi;
pub mod string;
pub mod subprocesstag;
pub mod winapi_local;
pub mod winsta;
