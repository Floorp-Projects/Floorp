// Copyright 2014 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//! A one-dimensional length, tagged with its units.

use num_traits;


pub trait Zero {
    fn zero() -> Self;
}

impl<T: num_traits::Zero> Zero for T {
    fn zero() -> T { num_traits::Zero::zero() }
}

pub trait One {
    fn one() -> Self;
}

impl<T: num_traits::One> One for T {
    fn one() -> T { num_traits::One::one() }
}

pub trait Round : Copy { fn round(self) -> Self; }
pub trait Floor : Copy { fn floor(self) -> Self; }
pub trait Ceil : Copy { fn ceil(self) -> Self; }

impl Round for f32 { fn round(self) -> Self { self.round() } }
impl Round for f64 { fn round(self) -> Self { self.round() } }
impl Round for i16 { fn round(self) -> Self { self } }
impl Round for u16 { fn round(self) -> Self { self } }
impl Round for i32 { fn round(self) -> Self { self } }
impl Round for i64 { fn round(self) -> Self { self } }
impl Round for u32 { fn round(self) -> Self { self } }
impl Round for u64 { fn round(self) -> Self { self } }
impl Round for usize { fn round(self) -> Self { self } }
impl Round for isize { fn round(self) -> Self { self } }

impl Floor for f32 { fn floor(self) -> Self { self.floor() } }
impl Floor for f64 { fn floor(self) -> Self { self.floor() } }
impl Floor for i16 { fn floor(self) -> Self { self } }
impl Floor for u16 { fn floor(self) -> Self { self } }
impl Floor for i32 { fn floor(self) -> Self { self } }
impl Floor for i64 { fn floor(self) -> Self { self } }
impl Floor for u32 { fn floor(self) -> Self { self } }
impl Floor for u64 { fn floor(self) -> Self { self } }
impl Floor for usize { fn floor(self) -> Self { self } }
impl Floor for isize { fn floor(self) -> Self { self } }

impl Ceil for f32 { fn ceil(self) -> Self { self.ceil() } }
impl Ceil for f64 { fn ceil(self) -> Self { self.ceil() } }
impl Ceil for i16 { fn ceil(self) -> Self { self } }
impl Ceil for u16 { fn ceil(self) -> Self { self } }
impl Ceil for i32 { fn ceil(self) -> Self { self } }
impl Ceil for i64 { fn ceil(self) -> Self { self } }
impl Ceil for u32 { fn ceil(self) -> Self { self } }
impl Ceil for u64 { fn ceil(self) -> Self { self } }
impl Ceil for usize { fn ceil(self) -> Self { self } }
impl Ceil for isize { fn ceil(self) -> Self { self } }

