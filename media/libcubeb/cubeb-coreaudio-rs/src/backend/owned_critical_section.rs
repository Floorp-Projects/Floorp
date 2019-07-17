// Copyright © 2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

extern crate libc;

use self::libc::*;
use std::{fmt, mem};

pub struct OwnedCriticalSection {
    mutex: pthread_mutex_t,
}

// Notice that `OwnedCriticalSection` only works after `init` is called.
//
// Since `pthread_mutex_t` cannot be copied, we don't initialize the `mutex` in
// `new()`. The `let x = OwnedCriticalSection::new()` will temporarily creat a
// `OwnedCriticalSection` struct inside `new()` and copy the temporary struct
// to `x`, and then destroy the temporary struct. We should initialize the
// `x.mutex` instead of the `mutex` created temporarily inside `new()`.
//
// Example:
// let mut mutex = OwnedCriticalSection::new();
// mutex.init();
impl OwnedCriticalSection {
    pub fn new() -> Self {
        OwnedCriticalSection {
            mutex: PTHREAD_MUTEX_INITIALIZER,
        }
    }

    pub fn init(&mut self) {
        unsafe {
            let mut attr: pthread_mutexattr_t = mem::zeroed();
            let r = pthread_mutexattr_init(&mut attr);
            assert_eq!(r, 0);
            let r = pthread_mutexattr_settype(&mut attr, PTHREAD_MUTEX_ERRORCHECK);
            assert_eq!(r, 0);
            let r = pthread_mutex_init(&mut self.mutex, &attr);
            assert_eq!(r, 0);
            let _ = pthread_mutexattr_destroy(&mut attr);
        }
    }

    fn destroy(&mut self) {
        unsafe {
            let r = pthread_mutex_destroy(&mut self.mutex);
            assert_eq!(r, 0);
        }
    }

    fn lock(&mut self) {
        unsafe {
            let r = pthread_mutex_lock(&mut self.mutex);
            assert_eq!(r, 0, "Deadlock");
        }
    }

    fn unlock(&mut self) {
        unsafe {
            let r = pthread_mutex_unlock(&mut self.mutex);
            assert_eq!(r, 0, "Unlocking unlocked mutex");
        }
    }

    pub fn assert_current_thread_owns(&mut self) {
        unsafe {
            let r = pthread_mutex_lock(&mut self.mutex);
            assert_eq!(r, EDEADLK);
        }
    }
}

impl Drop for OwnedCriticalSection {
    fn drop(&mut self) {
        self.destroy();
    }
}

impl fmt::Debug for OwnedCriticalSection {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "OwnedCriticalSection {{ mutex @ {:p} }}", &self.mutex)
    }
}

pub struct AutoLock<'a> {
    mutex: &'a mut OwnedCriticalSection,
}

impl<'a> AutoLock<'a> {
    pub fn new(mutex: &'a mut OwnedCriticalSection) -> Self {
        mutex.lock();
        AutoLock { mutex }
    }
}

impl<'a> Drop for AutoLock<'a> {
    fn drop(&mut self) {
        self.mutex.unlock();
    }
}

#[test]
fn test_create_critical_section() {
    let mut section = OwnedCriticalSection::new();
    section.init();
    section.lock();
    section.assert_current_thread_owns();
    section.unlock();
}

#[test]
#[should_panic]
fn test_critical_section_destroy_without_unlocking_locked() {
    let mut section = OwnedCriticalSection::new();
    section.init();
    section.lock();
    section.assert_current_thread_owns();
    // Get EBUSY(16) since we destroy the object
    // referenced by mutex while it is locked.
}

#[test]
#[should_panic]
fn test_critical_section_unlock_without_locking() {
    let mut section = OwnedCriticalSection::new();
    section.init();
    section.unlock();
    // Get EPERM(1) since it has no privilege to
    // perform the operation.
}

// #[test]
// #[should_panic]
// fn test_critical_section_assert_without_locking() {
//     let mut section = OwnedCriticalSection::new();
//     section.init();
//     section.assert_current_thread_owns();
//     // pthread_mutex_lock() in assert_current_thread_owns() returns 0
//     // since lock() wasn't called, so the `assert_eq` in
//     // assert_current_thread_owns() fails.

//     // When we finish this test since the above assertion fails,
//     // `destroy()` will be called within `drop()`. However,
//     // `destroy()` also will fail on its assertion since
//     // we didn't unlock the section.
// }

#[test]
fn test_critical_section_multithread() {
    use std::thread;
    use std::time::Duration;

    struct Resource {
        value: u32,
        mutex: OwnedCriticalSection,
    }

    let mut resource = Resource {
        value: 0,
        mutex: OwnedCriticalSection::new(),
    };
    resource.mutex.init();

    // Make a vector to hold the children which are spawned.
    let mut children = vec![];

    // Rust compilter doesn't allow a pointer to be passed across threads.
    // A hacky way to do that is to cast the pointer into a value, then
    // the value, which is actually an address, can be copied into threads.
    let resource_ptr = &mut resource as *mut Resource as usize;

    for i in 0..10 {
        // Spin up another thread
        children.push(thread::spawn(move || {
            let res = unsafe {
                let ptr = resource_ptr as *mut Resource;
                &mut (*ptr)
            };
            assert_eq!(res as *mut Resource as usize, resource_ptr);

            // Test fails after commenting `AutoLock` and since the order
            // to run the threads is random.
            // The scope of `_guard` is a critical section.
            let _guard = AutoLock::new(&mut res.mutex); // -------------+
                                                        //              |
            res.value = i; //                                           | critical
            thread::sleep(Duration::from_millis(1)); //                 | section
                                                     //                 |
            (i, res.value) //                                           |
        })); // <-------------------------------------------------------+
    }

    for child in children {
        // Wait for the thread to finish.
        let (num, value) = child.join().unwrap();
        assert_eq!(num, value)
    }
}

#[test]
fn test_dummy_mutex_multithread() {
    use std::sync::Mutex;
    use std::thread;
    use std::time::Duration;

    struct Resource {
        value: u32,
        mutex: Mutex<()>,
    }

    let mut resource = Resource {
        value: 0,
        mutex: Mutex::new(()),
    };

    // Make a vector to hold the children which are spawned.
    let mut children = vec![];

    // Rust compilter doesn't allow a pointer to be passed across threads.
    // A hacky way to do that is to cast the pointer into a value, then
    // the value, which is actually an address, can be copied into threads.
    let resource_ptr = &mut resource as *mut Resource as usize;

    for i in 0..10 {
        // Spin up another thread
        children.push(thread::spawn(move || {
            let res = unsafe {
                let ptr = resource_ptr as *mut Resource;
                &mut (*ptr)
            };
            assert_eq!(res as *mut Resource as usize, resource_ptr);

            // Test fails after commenting res.mutex.lock() since the order
            // to run the threads is random.
            // The scope of `_guard` is a critical section.
            let _guard = res.mutex.lock().unwrap(); // -----------------+
                                                    //                  |
            res.value = i; //                                           | critical
            thread::sleep(Duration::from_millis(1)); //                 | section
                                                     //                 |
            (i, res.value) //                                           |
        })); // <-------------------------------------------------------+
    }

    for child in children {
        // Wait for the thread to finish.
        let (num, value) = child.join().unwrap();
        assert_eq!(num, value)
    }
}
