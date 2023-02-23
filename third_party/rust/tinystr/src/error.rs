// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use displaydoc::Display;

#[cfg(feature = "std")]
impl std::error::Error for TinyStrError {}

#[derive(Display, Debug, PartialEq, Eq)]
#[non_exhaustive]
pub enum TinyStrError {
    #[displaydoc("found string of larger length {len} when constructing string of length {max}")]
    TooLarge { max: usize, len: usize },
    #[displaydoc("tinystr types do not support strings with null bytes")]
    ContainsNull,
    #[displaydoc("attempted to construct TinyStrAuto from a non-ascii string")]
    NonAscii,
}
