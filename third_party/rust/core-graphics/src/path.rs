// Copyright 2017 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use core_foundation::base::{CFRelease, CFRetain, CFTypeID};
use foreign_types::ForeignType;
use geometry::CGPoint;
use std::os::raw::c_void;
use std::fmt::{self, Debug, Formatter};
use std::marker::PhantomData;
use std::ops::Deref;
use std::slice;

foreign_type! {
    #[doc(hidden)]
    type CType = ::sys::CGPath;
    fn drop = |p| CFRelease(p as *mut _);
    fn clone = |p| CFRetain(p as *const _) as *mut _;
    pub struct CGPath;
    pub struct CGPathRef;
}

impl CGPath {
    pub fn type_id() -> CFTypeID {
        unsafe {
            CGPathGetTypeID()
        }
    }

    pub fn apply<'a, F>(&'a self, mut closure: &'a F) where F: FnMut(CGPathElementRef<'a>) {
        unsafe {
            CGPathApply(self.as_ptr(), &mut closure as *mut _ as *mut c_void, do_apply::<F>);
        }

        unsafe extern "C" fn do_apply<'a, F>(info: *mut c_void, element: *const CGPathElement)
                                             where F: FnMut(CGPathElementRef<'a>) {
            let closure = info as *mut *mut F;
            (**closure)(CGPathElementRef::new(element))
        }
    }
}

#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum CGPathElementType {
    MoveToPoint = 0,
    AddLineToPoint = 1,
    AddQuadCurveToPoint = 2,
    AddCurveToPoint = 3,
    CloseSubpath = 4,
}

pub struct CGPathElementRef<'a> {
    element: *const CGPathElement,
    phantom: PhantomData<&'a CGPathElement>,
}

impl<'a> CGPathElementRef<'a> {
    fn new<'b>(element: *const CGPathElement) -> CGPathElementRef<'b> {
        CGPathElementRef {
            element: element,
            phantom: PhantomData,
        }
    }
}

impl<'a> Deref for CGPathElementRef<'a> {
    type Target = CGPathElement;
    fn deref(&self) -> &CGPathElement {
        unsafe {
            &*self.element
        }
    }
}

#[repr(C)]
pub struct CGPathElement {
    pub element_type: CGPathElementType,
    points: *mut CGPoint,
}

impl Debug for CGPathElement {
    fn fmt(&self, formatter: &mut Formatter) -> Result<(), fmt::Error> {
        write!(formatter, "{:?}: {:?}", self.element_type, self.points())
    }
}

impl CGPathElement {
    pub fn points(&self) -> &[CGPoint] {
        unsafe {
            match self.element_type {
                CGPathElementType::CloseSubpath => &[],
                CGPathElementType::MoveToPoint | CGPathElementType::AddLineToPoint => {
                    slice::from_raw_parts(self.points, 1)
                }
                CGPathElementType::AddQuadCurveToPoint => slice::from_raw_parts(self.points, 2),
                CGPathElementType::AddCurveToPoint => slice::from_raw_parts(self.points, 3),
            }
        }
    }
}

type CGPathApplierFunction = unsafe extern "C" fn(info: *mut c_void,
                                                  element: *const CGPathElement);

#[link(name = "CoreGraphics", kind = "framework")]
extern {
    fn CGPathApply(path: ::sys::CGPathRef, info: *mut c_void, function: CGPathApplierFunction);
    fn CGPathGetTypeID() -> CFTypeID;
}
