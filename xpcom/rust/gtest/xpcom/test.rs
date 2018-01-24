/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![allow(non_snake_case)]

#[macro_use]
extern crate xpcom;

extern crate nserror;

extern crate nsstring;

use std::ptr;
use xpcom::{XpCom, getter_addrefs, interfaces};
use nserror::{NsresultExt, nsresult, NS_OK};
use nsstring::{nsCStr, nsCString};

#[no_mangle]
pub unsafe extern fn Rust_CallIURIFromRust() -> *const interfaces::nsIIOService {
    let iosvc = xpcom::services::get_IOService().unwrap();

    let uri = getter_addrefs(|p| iosvc.NewURI(&nsCStr::from("https://google.com"), ptr::null(), ptr::null(), p)).unwrap();

    let mut host = nsCString::new();
    let rv = uri.GetHost(&mut host);
    assert!(rv.succeeded());

    assert_eq!(&*host, "google.com");

    assert!(iosvc.query_interface::<interfaces::nsISupports>().is_some());
    assert!(iosvc.query_interface::<interfaces::nsIURI>().is_none());
    &*iosvc
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
