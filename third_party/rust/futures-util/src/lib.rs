//! Combinators and utilities for working with `Future`s, `Stream`s, `Sink`s,
//! and the `AsyncRead` and `AsyncWrite` traits.

#![cfg_attr(feature = "cfg-target-has-atomic", feature(cfg_target_has_atomic))]
#![cfg_attr(feature = "read-initializer", feature(read_initializer))]

#![cfg_attr(not(feature = "std"), no_std)]
#![warn(missing_docs, missing_debug_implementations, rust_2018_idioms, unreachable_pub)]
// It cannot be included in the published code because this lints have false positives in the minimum required version.
#![cfg_attr(test, warn(single_use_lifetimes))]
#![warn(clippy::all)]

// The solution for this lint is not available on 1.39 which is the current minimum supported version.
// Can be removed as of minimum supported 1.40 or if https://github.com/rust-lang/rust-clippy/issues/3941
// get's implemented.
#![allow(clippy::mem_replace_with_default)]

#![doc(test(attr(deny(warnings), allow(dead_code, unused_assignments, unused_variables))))]

#![doc(html_root_url = "https://docs.rs/futures-util/0.3.0")]

#[cfg(all(feature = "cfg-target-has-atomic", not(feature = "unstable")))]
compile_error!("The `cfg-target-has-atomic` feature requires the `unstable` feature as an explicit opt-in to unstable features");

#[cfg(all(feature = "bilock", not(feature = "unstable")))]
compile_error!("The `bilock` feature requires the `unstable` feature as an explicit opt-in to unstable features");

#[cfg(all(feature = "read-initializer", not(feature = "unstable")))]
compile_error!("The `read-initializer` feature requires the `unstable` feature as an explicit opt-in to unstable features");

#[cfg(feature = "alloc")]
extern crate alloc;

#[macro_use(ready)]
extern crate futures_core;

// Macro re-exports
pub use futures_core::ready;
pub use pin_utils::pin_mut;

// Not public API.
#[cfg(feature = "async-await")]
#[macro_use]
#[doc(hidden)]
pub mod async_await;
#[cfg(feature = "async-await")]
#[doc(hidden)]
pub use self::async_await::*;

// Not public API.
#[doc(hidden)]
pub use futures_core::core_reexport;

macro_rules! cfg_target_has_atomic {
    ($($item:item)*) => {$(
        #[cfg_attr(feature = "cfg-target-has-atomic", cfg(target_has_atomic = "ptr"))]
        $item
    )*};
}

#[cfg(feature = "sink")]
macro_rules! delegate_sink {
    ($field:ident, $item:ty) => {
        fn poll_ready(
            self: Pin<&mut Self>,
            cx: &mut $crate::core_reexport::task::Context<'_>,
        ) -> $crate::core_reexport::task::Poll<Result<(), Self::Error>> {
            self.$field().poll_ready(cx)
        }

        fn start_send(
            self: Pin<&mut Self>,
            item: $item,
        ) -> Result<(), Self::Error> {
            self.$field().start_send(item)
        }

        fn poll_flush(
            self: Pin<&mut Self>,
            cx: &mut $crate::core_reexport::task::Context<'_>,
        ) -> $crate::core_reexport::task::Poll<Result<(), Self::Error>> {
            self.$field().poll_flush(cx)
        }

        fn poll_close(
            self: Pin<&mut Self>,
            cx: &mut $crate::core_reexport::task::Context<'_>,
        ) -> $crate::core_reexport::task::Poll<Result<(), Self::Error>> {
            self.$field().poll_close(cx)
        }
    }
}

pub mod future;
#[doc(hidden)] pub use crate::future::{FutureExt, TryFutureExt};

pub mod stream;
#[doc(hidden)] pub use crate::stream::{StreamExt, TryStreamExt};

#[cfg(feature = "sink")]
pub mod sink;
#[cfg(feature = "sink")]
#[doc(hidden)] pub use crate::sink::SinkExt;

pub mod task;

pub mod never;

#[cfg(feature = "compat")]
pub mod compat;

#[cfg(feature = "io")]
#[cfg(feature = "std")]
pub mod io;
#[cfg(feature = "io")]
#[cfg(feature = "std")]
#[doc(hidden)] pub use crate::io::{AsyncReadExt, AsyncWriteExt, AsyncSeekExt, AsyncBufReadExt};

cfg_target_has_atomic! {
    #[cfg(feature = "alloc")]
    pub mod lock;
}
