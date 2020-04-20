//! Streams
//!
//! This module contains a number of functions for working with `Stream`s,
//! including the [`StreamExt`] trait and the [`TryStreamExt`] trait which add
//! methods to `Stream` types

#[cfg(feature = "alloc")]
pub use futures_core::stream::{BoxStream, LocalBoxStream};
pub use futures_core::stream::{FusedStream, Stream, TryStream};

// Extension traits and combinators

#[allow(clippy::module_inception)]
mod stream;
pub use self::stream::{
    Chain, Collect, Concat, Enumerate, Filter, FilterMap, Flatten, Fold, ForEach, Fuse, Inspect,
    Map, Next, Peek, Peekable, Scan, SelectNextSome, Skip, SkipWhile, StreamExt, StreamFuture, Take,
    TakeWhile, Then, Zip,
};

#[cfg(feature = "std")]
pub use self::stream::CatchUnwind;

#[cfg(feature = "alloc")]
pub use self::stream::Chunks;

#[cfg(feature = "sink")]
pub use self::stream::Forward;

#[cfg_attr(feature = "cfg-target-has-atomic", cfg(target_has_atomic = "ptr"))]
#[cfg(feature = "alloc")]
pub use self::stream::{BufferUnordered, Buffered, ForEachConcurrent};

#[cfg_attr(feature = "cfg-target-has-atomic", cfg(target_has_atomic = "ptr"))]
#[cfg(feature = "sink")]
#[cfg(feature = "alloc")]
pub use self::stream::{ReuniteError, SplitSink, SplitStream};

mod try_stream;
pub use self::try_stream::{
    try_unfold, AndThen, ErrInto, InspectErr, InspectOk, IntoStream, MapErr, MapOk, OrElse,
    TryCollect, TryConcat, TryFilter, TryFilterMap, TryFlatten, TryFold, TryForEach, TryNext,
    TrySkipWhile, TryStreamExt, TryUnfold,
};

#[cfg(feature = "io")]
#[cfg(feature = "std")]
pub use self::try_stream::IntoAsyncRead;

#[cfg_attr(feature = "cfg-target-has-atomic", cfg(target_has_atomic = "ptr"))]
#[cfg(feature = "alloc")]
pub use self::try_stream::{TryBufferUnordered, TryForEachConcurrent};

// Primitive streams

mod iter;
pub use self::iter::{iter, Iter};

mod repeat;
pub use self::repeat::{repeat, Repeat};

mod empty;
pub use self::empty::{empty, Empty};

mod once;
pub use self::once::{once, Once};

mod pending;
pub use self::pending::{pending, Pending};

mod poll_fn;
pub use self::poll_fn::{poll_fn, PollFn};

mod select;
pub use self::select::{select, Select};

mod unfold;
pub use self::unfold::{unfold, Unfold};

cfg_target_has_atomic! {
    #[cfg(feature = "alloc")]
    mod futures_ordered;
    #[cfg(feature = "alloc")]
    pub use self::futures_ordered::FuturesOrdered;

    #[cfg(feature = "alloc")]
    pub mod futures_unordered;
    #[cfg(feature = "alloc")]
    #[doc(inline)]
    pub use self::futures_unordered::FuturesUnordered;

    #[cfg(feature = "alloc")]
    mod select_all;
    #[cfg(feature = "alloc")]
    pub use self::select_all::{select_all, SelectAll};
}
