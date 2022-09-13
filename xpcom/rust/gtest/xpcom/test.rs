/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![allow(non_snake_case)]

#[macro_use]
extern crate xpcom;

extern crate nserror;

use nserror::{nsresult, NS_OK};
use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::ptr;
use xpcom::{interfaces, RefPtr};

#[no_mangle]
pub unsafe extern "C" fn Rust_ObserveFromRust() -> *const interfaces::nsIObserverService {
    let obssvc: RefPtr<interfaces::nsIObserverService> =
        xpcom::components::Observer::service().unwrap();

    // Define an observer
    #[xpcom(implement(nsIObserver), nonatomic)]
    struct Observer {
        run: *mut bool,
    }
    impl Observer {
        unsafe fn Observe(
            &self,
            _subject: *const interfaces::nsISupports,
            topic: *const c_char,
            _data: *const u16,
        ) -> nsresult {
            *self.run = true;
            assert!(CStr::from_ptr(topic).to_str() == Ok("test-rust-observe"));
            NS_OK
        }
    }

    let topic = CString::new("test-rust-observe").unwrap();

    let mut run = false;
    let observer = Observer::allocate(InitObserver { run: &mut run });
    let rv = obssvc.AddObserver(
        observer.coerce::<interfaces::nsIObserver>(),
        topic.as_ptr(),
        false,
    );
    assert!(rv.succeeded());

    let rv = obssvc.NotifyObservers(ptr::null(), topic.as_ptr(), ptr::null());
    assert!(rv.succeeded());
    assert!(run, "The observer should have been run!");

    let rv = obssvc.RemoveObserver(observer.coerce::<interfaces::nsIObserver>(), topic.as_ptr());
    assert!(rv.succeeded());

    assert!(
        observer.coerce::<interfaces::nsISupports>() as *const _
            == &*observer
                .query_interface::<interfaces::nsISupports>()
                .unwrap() as *const _
    );

    &*obssvc
}

#[no_mangle]
pub unsafe extern "C" fn Rust_ImplementRunnableInRust(
    it_worked: *mut bool,
    runnable: *mut *const interfaces::nsIRunnable,
) {
    // Define a type which implements nsIRunnable in rust.
    #[xpcom(implement(nsIRunnable), atomic)]
    struct RunnableFn<F: Fn() + 'static> {
        run: F,
    }

    impl<F: Fn() + 'static> RunnableFn<F> {
        unsafe fn Run(&self) -> nsresult {
            (self.run)();
            NS_OK
        }
    }

    let my_runnable = RunnableFn::allocate(InitRunnableFn {
        run: move || {
            *it_worked = true;
        },
    });
    my_runnable
        .query_interface::<interfaces::nsIRunnable>()
        .unwrap()
        .forget(&mut *runnable);
}

#[no_mangle]
pub unsafe extern "C" fn Rust_GetMultipleInterfaces(
    runnable: *mut *const interfaces::nsIRunnable,
    observer: *mut *const interfaces::nsIObserver,
) {
    // Define a type which implements nsIRunnable and nsIObserver in rust, and
    // hand both references back to c++
    #[xpcom(implement(nsIRunnable, nsIObserver), atomic)]
    struct MultipleInterfaces {}

    impl MultipleInterfaces {
        unsafe fn Run(&self) -> nsresult {
            NS_OK
        }
        unsafe fn Observe(
            &self,
            _subject: *const interfaces::nsISupports,
            _topic: *const c_char,
            _data: *const u16,
        ) -> nsresult {
            NS_OK
        }
    }

    let instance = MultipleInterfaces::allocate(InitMultipleInterfaces {});
    instance
        .query_interface::<interfaces::nsIRunnable>()
        .unwrap()
        .forget(&mut *runnable);
    instance
        .query_interface::<interfaces::nsIObserver>()
        .unwrap()
        .forget(&mut *observer);
}
