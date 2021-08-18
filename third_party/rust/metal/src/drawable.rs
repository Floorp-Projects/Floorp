// Copyright 2016 GFX developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.

use cocoa_foundation::foundation::NSUInteger;

pub enum MTLDrawable {}

foreign_obj_type! {
    type CType = MTLDrawable;
    pub struct Drawable;
    pub struct DrawableRef;
}

impl DrawableRef {
    pub fn present(&self) {
        unsafe { msg_send![self, present] }
    }

    pub fn drawable_id(&self) -> NSUInteger {
        unsafe { msg_send![self, drawableID] }
    }
}
