// Copyright 2013-2014 The Rust Project Developers.
// Copyright 2018 The Uuid Project Developers.
//
// See the COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use core::fmt;
use prelude::*;

impl fmt::Display for super::Hyphenated {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::LowerHex::fmt(self, f)
    }
}

impl<'a> fmt::Display for super::HyphenatedRef<'a> {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::LowerHex::fmt(self, f)
    }
}

impl fmt::Display for super::Simple {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::LowerHex::fmt(self, f)
    }
}

impl<'a> fmt::Display for super::SimpleRef<'a> {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::LowerHex::fmt(self, f)
    }
}

impl fmt::Display for super::Urn {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::LowerHex::fmt(self, f)
    }
}

impl<'a> fmt::Display for super::UrnRef<'a> {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::LowerHex::fmt(self, f)
    }
}

impl fmt::LowerHex for super::Hyphenated {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str(self.encode_lower(&mut [0; Self::LENGTH]))
    }
}

impl<'a> fmt::LowerHex for super::HyphenatedRef<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        // TODO: Self doesn't work https://github.com/rust-lang/rust/issues/52808
        f.write_str(self.encode_lower(&mut [0; super::HyphenatedRef::LENGTH]))
    }
}

impl fmt::LowerHex for super::Simple {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str(self.encode_lower(&mut [0; Self::LENGTH]))
    }
}

impl<'a> fmt::LowerHex for super::SimpleRef<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        // TODO: Self doesn't work https://github.com/rust-lang/rust/issues/52808
        f.write_str(self.encode_lower(&mut [0; super::SimpleRef::LENGTH]))
    }
}

impl fmt::LowerHex for super::Urn {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str(self.encode_lower(&mut [0; Self::LENGTH]))
    }
}

impl<'a> fmt::LowerHex for super::UrnRef<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        // TODO: Self doesn't work https://github.com/rust-lang/rust/issues/52808
        f.write_str(self.encode_lower(&mut [0; super::UrnRef::LENGTH]))
    }
}

impl fmt::UpperHex for super::Hyphenated {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str(self.encode_upper(&mut [0; Self::LENGTH]))
    }
}

impl<'a> fmt::UpperHex for super::HyphenatedRef<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        // TODO: Self doesn't work https://github.com/rust-lang/rust/issues/52808
        f.write_str(self.encode_upper(&mut [0; super::HyphenatedRef::LENGTH]))
    }
}

impl fmt::UpperHex for super::Simple {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str(self.encode_upper(&mut [0; Self::LENGTH]))
    }
}

impl<'a> fmt::UpperHex for super::SimpleRef<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        // TODO: Self doesn't work https://github.com/rust-lang/rust/issues/52808
        f.write_str(self.encode_upper(&mut [0; super::SimpleRef::LENGTH]))
    }
}

impl fmt::UpperHex for super::Urn {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str(self.encode_upper(&mut [0; Self::LENGTH]))
    }
}

impl<'a> fmt::UpperHex for super::UrnRef<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        // TODO: Self doesn't work https://github.com/rust-lang/rust/issues/52808
        f.write_str(self.encode_upper(&mut [0; super::UrnRef::LENGTH]))
    }
}

impl From<Uuid> for super::Hyphenated {
    #[inline]
    fn from(f: Uuid) -> Self {
        super::Hyphenated::from_uuid(f)
    }
}

impl<'a> From<&'a Uuid> for super::HyphenatedRef<'a> {
    #[inline]
    fn from(f: &'a Uuid) -> Self {
        super::HyphenatedRef::from_uuid_ref(f)
    }
}

impl From<Uuid> for super::Simple {
    #[inline]
    fn from(f: Uuid) -> Self {
        super::Simple::from_uuid(f)
    }
}

impl<'a> From<&'a Uuid> for super::SimpleRef<'a> {
    #[inline]
    fn from(f: &'a Uuid) -> Self {
        super::SimpleRef::from_uuid_ref(f)
    }
}

impl From<Uuid> for super::Urn {
    #[inline]
    fn from(f: Uuid) -> Self {
        super::Urn::from_uuid(f)
    }
}

impl<'a> From<&'a Uuid> for super::UrnRef<'a> {
    #[inline]
    fn from(f: &'a Uuid) -> Self {
        super::UrnRef::from_uuid_ref(f)
    }
}
