// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use ffi;

#[derive(Debug)]
pub struct Operation(*mut ffi::pa_operation);

impl Operation {
    pub unsafe fn from_raw_ptr(raw: *mut ffi::pa_operation) -> Operation {
        Operation(raw)
    }

    pub fn cancel(&mut self) {
        unsafe {
            ffi::pa_operation_cancel(self.0);
        }
    }

    pub fn get_state(&self) -> ffi::pa_operation_state_t {
        unsafe { ffi::pa_operation_get_state(self.0) }
    }
}

impl Clone for Operation {
    fn clone(&self) -> Self {
        Operation(unsafe { ffi::pa_operation_ref(self.0) })
    }
}

impl Drop for Operation {
    fn drop(&mut self) {
        unsafe {
            ffi::pa_operation_unref(self.0);
        }
    }
}

pub unsafe fn from_raw_ptr(raw: *mut ffi::pa_operation) -> Operation {
    Operation::from_raw_ptr(raw)
}
