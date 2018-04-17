/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![allow(non_snake_case)]

#[macro_use]
extern crate xpcom;

extern crate nserror;

use std::os::raw::c_char;
use std::ptr;
use std::ffi::{CStr, CString};
use xpcom::interfaces;
use nserror::{NsresultExt, nsresult, NS_OK};

#[no_mangle]
pub unsafe extern fn Rust_ObserveFromRust() -> *const interfaces::nsIObserverService {
    let obssvc = xpcom::services::get_ObserverService().unwrap();

    // Define an observer
    #[derive(xpcom)]
    #[xpimplements(nsIObserver)]
    #[refcnt = "nonatomic"]
    struct InitObserver {
        run: *mut bool
    }
    impl Observer {
        unsafe fn Observe(
            &self,
            _subject: *const interfaces::nsISupports,
            topic: *const c_char,
            _data: *const i16
        ) -> nsresult {
            *self.run = true;
            assert!(CStr::from_ptr(topic).to_str() == Ok("test-rust-observe"));
            NS_OK
        }
    }

    let topic = CString::new("test-rust-observe").unwrap();

    let mut run = false;
    let observer = Observer::allocate(InitObserver{ run: &mut run });
    let rv = obssvc.AddObserver(observer.coerce::<interfaces::nsIObserver>(), topic.as_ptr(), false);
    assert!(rv.succeeded());

    let rv = obssvc.NotifyObservers(ptr::null(), topic.as_ptr(), ptr::null());
    assert!(rv.succeeded());
    assert!(run, "The observer should have been run!");

    let rv = obssvc.RemoveObserver(observer.coerce::<interfaces::nsIObserver>(), topic.as_ptr());
    assert!(rv.succeeded());

    assert!(observer.coerce::<interfaces::nsISupports>() as *const _==
            &*observer.query_interface::<interfaces::nsISupports>().unwrap() as *const _);

    &*obssvc
}

#[no_mangle]
pub unsafe extern fn Rust_ImplementRunnableInRust(it_worked: *mut bool,
                                                  runnable: *mut *const interfaces::nsIRunnable) {
    // Define a type which implements nsIRunnable in rust.
    #[derive(xpcom)]
    #[xpimplements(nsIRunnable)]
    #[refcnt = "atomic"]
    struct InitMyRunnable {
        it_worked: *mut bool,
    }

    impl MyRunnable {
        unsafe fn Run(&self) -> nsresult {
            *self.it_worked = true;
            NS_OK
        }
    }

    // Create my runnable type, and forget it into the outparameter!
    let my_runnable = MyRunnable::allocate(InitMyRunnable {
        it_worked: it_worked
    });
    my_runnable.query_interface::<interfaces::nsIRunnable>().unwrap().forget(&mut *runnable);
}
