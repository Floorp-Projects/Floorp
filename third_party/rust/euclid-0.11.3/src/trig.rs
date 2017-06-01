// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.


/// Trait for basic trigonometry functions, so they can be used on generic numeric types
pub trait Trig {
    fn sin(self) -> Self;
    fn cos(self) -> Self;
    fn tan(self) -> Self;
}

impl Trig for f32 {
    #[inline]
    fn sin(self) -> f32 {
        self.sin()
    }

    #[inline]
    fn cos(self) -> f32 {
       self.cos()
    }

    #[inline]
    fn tan(self) -> f32 {
       self.tan()
    }
}

impl Trig for f64 {
    #[inline]
    fn sin(self) -> f64 {
        self.sin()
    }

    #[inline]
    fn cos(self) -> f64 {
       self.cos()
    }

    #[inline]
    fn tan(self) -> f64 {
       self.tan()
    }
}
