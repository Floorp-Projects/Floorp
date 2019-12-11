// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use ffi;
use std::mem;
use std::os::raw::c_void;


#[allow(non_camel_case_types)]
type pa_once_cb_t = Option<unsafe extern "C" fn(m: *mut ffi::pa_mainloop_api,
                                                userdata: *mut c_void)>;
fn wrap_once_cb<F>(_: F) -> pa_once_cb_t
    where F: Fn(&MainloopApi, *mut c_void)
{
    assert!(mem::size_of::<F>() == 0);

    unsafe extern "C" fn wrapped<F>(m: *mut ffi::pa_mainloop_api, userdata: *mut c_void)
        where F: Fn(&MainloopApi, *mut c_void)
    {
        let api = from_raw_ptr(m);
        let result = mem::transmute::<_, &F>(&())(&api, userdata);
        mem::forget(api);
        result
    }

    Some(wrapped::<F>)
}

pub struct MainloopApi(*mut ffi::pa_mainloop_api);

impl MainloopApi {
    pub fn raw_mut(&self) -> &mut ffi::pa_mainloop_api {
        unsafe { &mut *self.0 }
    }

    pub fn once<CB>(&self, cb: CB, userdata: *mut c_void)
        where CB: Fn(&MainloopApi, *mut c_void)
    {
        let wrapped = wrap_once_cb(cb);
        unsafe {
            ffi::pa_mainloop_api_once(self.raw_mut(), wrapped, userdata);
        }
    }

    pub fn time_free(&self, e: *mut ffi::pa_time_event) {
        unsafe {
            if let Some(f) = self.raw_mut().time_free {
                f(e);
            }
        }
    }
}

pub unsafe fn from_raw_ptr(raw: *mut ffi::pa_mainloop_api) -> MainloopApi {
    MainloopApi(raw)
}
