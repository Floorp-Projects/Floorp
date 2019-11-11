// The `__m128` is not defined to be FFI-compatible, so Rust doesn't
// like that we're `extern`ing the `Context`, which contains
// them. However the context is opaque on the C side, so this is okay.
#![allow(improper_ctypes)]

//! A port of the tests from `lib/lucet-runtime-c/test/context_suite.c`

use crate::context::{Context, ContextHandle};
use crate::val::Val;
use lazy_static::lazy_static;
use std::ffi::CStr;
use std::os::raw::{c_char, c_int, c_void};
use std::sync::Mutex;

lazy_static! {
    static ref TEST_GLOBAL_LOCK: Mutex<()> = Mutex::new(());
}

extern "C" {
    static mut parent_regs: *mut ContextHandle;
    static mut child_regs: *mut ContextHandle;
}

fn new_stack() -> Box<[u64]> {
    vec![0u64; 1024].into_boxed_slice()
}

macro_rules! test_body {
    ( $stack:ident, $body:block ) => {
        let _lock = TEST_GLOBAL_LOCK.lock().unwrap();
        reset_output();

        let mut $stack = new_stack();
        let parent = Box::into_raw(Box::new(ContextHandle::new()));

        unsafe {
            parent_regs = parent;
        }

        {
            $body
        }

        unsafe {
            Box::from_raw(parent_regs);
            Box::from_raw(child_regs);
        }
    };
}

macro_rules! init_and_swap {
    ( $stack:ident, $fn:ident, [ $( $args:expr ),* ] ) => {
        unsafe {
            let child = Box::into_raw(Box::new(ContextHandle::create_and_init(
                &mut *$stack,
                parent_regs.as_mut().unwrap(),
                $fn as usize,
                &[$( $args ),*],
            ).unwrap()));

            child_regs = child;

            Context::swap(parent_regs.as_mut().unwrap(), child_regs.as_ref().unwrap());
        }
    }
}

#[test]
fn call_child() {
    test_body!(stack, {
        extern "C" {
            fn arg_printing_child();
        }

        let mut arg0_val: c_int = 123;
        let mut arg1_val: c_int = 456;
        let arg0 = Val::CPtr(&mut arg0_val as *mut c_int as *mut c_void);
        let arg1 = Val::CPtr(&mut arg1_val as *mut c_int as *mut c_void);

        init_and_swap!(stack, arg_printing_child, [arg0, arg1]);

        assert_eq!(
            "hello from the child! my args were 123 and 456\n",
            &get_output()
        );
    });
}

#[test]
#[allow(unused_assignments)]
fn call_child_twice() {
    test_body!(stack, {
        extern "C" {
            fn arg_printing_child();
        }

        let mut arg0_val: c_int = 123;
        let mut arg1_val: c_int = 456;
        let arg0 = Val::CPtr(&mut arg0_val as *mut c_int as *mut c_void);
        let arg1 = Val::CPtr(&mut arg1_val as *mut c_int as *mut c_void);

        init_and_swap!(stack, arg_printing_child, [arg0, arg1]);

        assert_eq!(
            "hello from the child! my args were 123 and 456\n",
            &get_output()
        );

        arg0_val = 9;
        arg1_val = 10;

        unsafe {
            Context::swap(parent_regs.as_mut().unwrap(), child_regs.as_ref().unwrap());
        }

        assert_eq!(
            "hello from the child! my args were 123 and 456\n\
             now they are 9 and 10\n",
            &get_output()
        );
    });
}

#[test]
fn call_child_setcontext() {
    test_body!(stack, {
        extern "C" {
            fn context_set_child();
        }

        init_and_swap!(stack, context_set_child, []);

        assert_eq!(
            "hello from the child! setting context to parent...\n",
            &get_output()
        );
    });
}

#[test]
fn call_child_setcontext_twice() {
    test_body!(stack, {
        extern "C" {
            fn context_set_child();
        }

        init_and_swap!(stack, context_set_child, []);

        assert_eq!(
            "hello from the child! setting context to parent...\n",
            &get_output()
        );

        init_and_swap!(stack, context_set_child, []);

        assert_eq!(
            "hello from the child! setting context to parent...\n\
             hello from the child! setting context to parent...\n",
            &get_output()
        );
    });
}

#[test]
fn call_returning_child() {
    test_body!(stack, {
        extern "C" {
            fn returning_child();
        }

        init_and_swap!(stack, returning_child, []);

        assert_eq!("hello from the child! returning...\n", &get_output());
    });
}

macro_rules! child_n_args {
    ( $fn:ident, $prefix:expr, $( $arg:expr ),* ) => {
        test_body!(stack, {
            extern "C" {
                fn $fn();
            }

            init_and_swap!(stack, $fn, [ $( Val::U64($arg) ),* ]);

            assert_eq!(
                concat!($prefix, $( " ", $arg ),* , "\n"),
                &get_output()
            );
        });
    }
}

#[test]
fn test_child_3_args() {
    child_n_args!(child_3_args, "the good three args boy", 10, 11, 12);
}

#[test]
fn test_child_4_args() {
    child_n_args!(child_4_args, "the large four args boy", 20, 21, 22, 23);
}

#[test]
fn test_child_5_args() {
    child_n_args!(child_5_args, "the big five args son", 30, 31, 32, 33, 34);
}

#[test]
fn test_child_6_args() {
    child_n_args!(
        child_6_args,
        "6 args, hahaha long boy",
        40,
        41,
        42,
        43,
        44,
        45
    );
}

#[test]
fn test_child_7_args() {
    child_n_args!(
        child_7_args,
        "7 args, hahaha long boy",
        50,
        51,
        52,
        53,
        54,
        55,
        56
    );
}

#[test]
fn test_child_8_args() {
    child_n_args!(
        child_8_args,
        "8 args, hahaha long boy",
        60,
        61,
        62,
        63,
        64,
        65,
        66,
        67
    );
}

#[test]
fn test_child_9_args() {
    child_n_args!(
        child_9_args,
        "9 args, hahaha long boy",
        70,
        71,
        72,
        73,
        74,
        75,
        76,
        77,
        78
    );
}

#[test]
fn test_child_10_args() {
    child_n_args!(
        child_10_args,
        "10 args, hahaha very long boy",
        80,
        81,
        82,
        83,
        84,
        85,
        86,
        87,
        88,
        89
    );
}

fn get_output() -> String {
    extern "C" {
        static output_string: c_char;
    }
    unsafe {
        CStr::from_ptr(&output_string as *const c_char)
            .to_string_lossy()
            .into_owned()
    }
}

fn reset_output() {
    extern "C" {
        fn reset_output();
    }
    unsafe {
        reset_output();
    }
}
