// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

mod context;
mod cork_state;
mod stream;

pub type Result<T> = ::std::result::Result<T, i32>;

pub use self::context::Context;
pub use self::stream::Device;
pub use self::stream::Stream;
