//! A port of the tests from `lib/lucet-runtime-c/test/context_suite.c`

use crate::context::{Context, ContextHandle};
use crate::val::{Val, __m128_as_f32, __m128_as_f64};
use lazy_static::lazy_static;
use std::cell::RefCell;
use std::fmt::Write;
use std::os::raw::{c_int, c_void};
use std::sync::Mutex;

lazy_static! {
    static ref TEST_GLOBAL_LOCK: Mutex<()> = Mutex::new(());
    static ref OUTPUT_STRING: Mutex<String> = Mutex::new(String::new());
}

macro_rules! assert_output_eq {
    ( $s:expr ) => {
        assert_eq!($s, &*OUTPUT_STRING.lock().unwrap())
    };
}

fn reset_output() {
    *OUTPUT_STRING.lock().unwrap() = String::with_capacity(1024);
}

static mut PARENT: Option<ContextHandle> = None;
static mut CHILD: Option<ContextHandle> = None;

fn new_stack() -> Box<[u64]> {
    vec![0u64; 1024].into_boxed_slice()
}

macro_rules! test_body {
    ( $stack:ident, $body:block ) => {
        let _lock = TEST_GLOBAL_LOCK.lock().unwrap();
        reset_output();

        let mut $stack = new_stack();
        unsafe {
            PARENT = Some(ContextHandle::new());
        }

        $body
    };
}

macro_rules! init_and_swap {
    ( $stack:ident, $fn:ident, [ $( $args:expr ),* ] ) => {
        unsafe {
            let child = ContextHandle::create_and_init(
                &mut *$stack,
                PARENT.as_mut().unwrap(),
                $fn as usize,
                &[$( $args ),*],
            ).unwrap();
            CHILD = Some(child);

            Context::swap(PARENT.as_mut().unwrap(), CHILD.as_ref().unwrap());
        }
    }
}

extern "C" fn arg_printing_child(arg0: *mut c_void, arg1: *mut c_void) {
    let arg0_val = unsafe { *(arg0 as *mut c_int) };
    let arg1_val = unsafe { *(arg1 as *mut c_int) };

    write!(
        OUTPUT_STRING.lock().unwrap(),
        "hello from the child! my args were {} and {}\n",
        arg0_val,
        arg1_val
    )
    .unwrap();

    unsafe { Context::swap(CHILD.as_mut().unwrap(), PARENT.as_ref().unwrap()) };

    // Read the arguments again
    let arg0_val = unsafe { *(arg0 as *mut c_int) };
    let arg1_val = unsafe { *(arg1 as *mut c_int) };

    write!(
        OUTPUT_STRING.lock().unwrap(),
        "now they are {} and {}\n",
        arg0_val,
        arg1_val
    )
    .unwrap();

    unsafe { Context::swap(CHILD.as_mut().unwrap(), PARENT.as_ref().unwrap()) };
}

#[test]
fn call_child() {
    test_body!(stack, {
        let mut arg0_val: c_int = 123;
        let mut arg1_val: c_int = 456;
        let arg0 = Val::CPtr(&mut arg0_val as *mut c_int as *mut c_void);
        let arg1 = Val::CPtr(&mut arg1_val as *mut c_int as *mut c_void);

        init_and_swap!(stack, arg_printing_child, [arg0, arg1]);

        assert_output_eq!("hello from the child! my args were 123 and 456\n");
    });
}

#[test]
#[allow(unused_assignments)]
fn call_child_twice() {
    test_body!(stack, {
        let mut arg0_val: c_int = 123;
        let mut arg1_val: c_int = 456;
        let arg0 = Val::CPtr(&mut arg0_val as *mut c_int as *mut c_void);
        let arg1 = Val::CPtr(&mut arg1_val as *mut c_int as *mut c_void);

        init_and_swap!(stack, arg_printing_child, [arg0, arg1]);

        assert_output_eq!("hello from the child! my args were 123 and 456\n");

        arg0_val = 9;
        arg1_val = 10;

        unsafe {
            Context::swap(PARENT.as_mut().unwrap(), CHILD.as_ref().unwrap());
        }

        assert_output_eq!(
            "hello from the child! my args were 123 and 456\n\
             now they are 9 and 10\n"
        );
    });
}

extern "C" fn context_set_child() {
    write!(
        OUTPUT_STRING.lock().unwrap(),
        "hello from the child! setting context to parent...\n",
    )
    .unwrap();
    unsafe {
        Context::set(PARENT.as_ref().unwrap());
    }
}

#[test]
fn call_child_setcontext() {
    test_body!(stack, {
        init_and_swap!(stack, context_set_child, []);

        assert_output_eq!("hello from the child! setting context to parent...\n");
    });
}

#[test]
fn call_child_setcontext_twice() {
    test_body!(stack, {
        init_and_swap!(stack, context_set_child, []);

        assert_output_eq!("hello from the child! setting context to parent...\n");

        init_and_swap!(stack, context_set_child, []);
        assert_output_eq!(
            "hello from the child! setting context to parent...\n\
             hello from the child! setting context to parent...\n"
        );
    });
}

extern "C" fn returning_child() {
    write!(
        OUTPUT_STRING.lock().unwrap(),
        "hello from the child! returning...\n",
    )
    .unwrap();
}

#[test]
fn call_returning_child() {
    test_body!(stack, {
        init_and_swap!(stack, returning_child, []);

        assert_output_eq!("hello from the child! returning...\n");
    });
}

#[test]
fn returning_add_u32() {
    extern "C" fn add(x: u32, y: u32) -> u32 {
        x + y
    }

    test_body!(stack, {
        init_and_swap!(stack, add, [Val::U32(100), Val::U32(20)]);

        unsafe {
            if let Some(ref child) = CHILD {
                assert_eq!(child.get_retval_gp(0), 120);
            } else {
                panic!("no child context present after returning");
            }
        }
    });
}

#[test]
fn returning_add_u64() {
    extern "C" fn add(x: u64, y: u64) -> u64 {
        x + y
    }

    test_body!(stack, {
        init_and_swap!(stack, add, [Val::U64(100), Val::U64(20)]);

        unsafe {
            assert_eq!(CHILD.as_ref().unwrap().get_retval_gp(0), 120);
        }
    });
}

#[test]
fn returning_add_f32() {
    extern "C" fn add(x: f32, y: f32) -> f32 {
        x + y
    }

    test_body!(stack, {
        init_and_swap!(stack, add, [Val::F32(100.0), Val::F32(20.0)]);

        unsafe {
            let reg = CHILD.as_ref().unwrap().get_retval_fp();
            assert_eq!(__m128_as_f32(reg), 120.0);
        }
    });
}

#[test]
fn returning_add_f64() {
    extern "C" fn add(x: f64, y: f64) -> f64 {
        x + y
    }

    test_body!(stack, {
        init_and_swap!(stack, add, [Val::F64(100.0), Val::F64(20.0)]);

        unsafe {
            let reg = CHILD.as_ref().unwrap().get_retval_fp();
            assert_eq!(__m128_as_f64(reg), 120.0);
        }
    });
}

macro_rules! child_n_args {
    ( $name: ident, $prefix:expr, { $( $arg:ident : $val:expr ),* } ) => {
        #[test]
        fn $name() {
            extern "C" fn child_n_args_gen( $( $arg: u64 ),*) {
                let mut out = OUTPUT_STRING.lock().unwrap();
                write!(out, $prefix).unwrap();
                $(
                    write!(out, " {}", $arg).unwrap();
                );*
                write!(out, "\n").unwrap();
            }

            test_body!(stack, {
                init_and_swap!(stack, child_n_args_gen, [ $( Val::U64($val) ),* ]);

                assert_output_eq!(
                    concat!($prefix, $( " ", $val ),* , "\n")
                );
            });
        }
    }
}

child_n_args!(child_3_args, "the good three args boy", {
    arg1: 10,
    arg2: 11,
    arg3: 12
});

child_n_args!(child_4_args, "the large four args boy", {
    arg1: 20,
    arg2: 21,
    arg3: 22,
    arg4: 23
});

child_n_args!(child_5_args, "the big five args son", {
    arg1: 30,
    arg2: 31,
    arg3: 32,
    arg4: 33,
    arg5: 34
});

child_n_args!(child_6_args, "6 args, hahaha long boy", {
    arg1: 40,
    arg2: 41,
    arg3: 42,
    arg4: 43,
    arg5: 44,
    arg6: 45
});

child_n_args!(child_7_args, "7 args, hahaha long boy", {
    arg1: 50,
    arg2: 51,
    arg3: 52,
    arg4: 53,
    arg5: 54,
    arg6: 55,
    arg7: 56
});

child_n_args!(child_8_args, "8 args, hahaha long boy", {
    arg1: 60,
    arg2: 61,
    arg3: 62,
    arg4: 63,
    arg5: 64,
    arg6: 65,
    arg7: 66,
    arg8: 67
});

child_n_args!(child_9_args, "9 args, hahaha long boy", {
    arg1: 70,
    arg2: 71,
    arg3: 72,
    arg4: 73,
    arg5: 74,
    arg6: 75,
    arg7: 76,
    arg8: 77,
    arg9: 78
});

child_n_args!(child_10_args, "10 args, hahaha very long boy", {
    arg1: 80,
    arg2: 81,
    arg3: 82,
    arg4: 83,
    arg5: 84,
    arg6: 85,
    arg7: 86,
    arg8: 87,
    arg9: 88,
    arg10: 89
});

macro_rules! child_n_fp_args {
    ( $name: ident, $prefix:expr, { $( $arg:ident : $val:expr ),* } ) => {
        #[test]
        fn $name() {
            extern "C" fn child_n_fp_args_gen( $( $arg: f64 ),*) {
                let mut out = OUTPUT_STRING.lock().unwrap();
                write!(out, $prefix).unwrap();
                $(
                    write!(out, " {:.1}", $arg).unwrap();
                );*
                write!(out, "\n").unwrap();
            }

            test_body!(stack, {
                init_and_swap!(stack, child_n_fp_args_gen, [ $( Val::F64($val) ),* ]);

                assert_output_eq!(
                    concat!($prefix, $( " ", $val ),* , "\n")
                );
            });
        }
    }
}

child_n_fp_args!(child_6_fp_args, "6 args, hahaha long boy", {
    arg1: 40.0,
    arg2: 41.0,
    arg3: 42.0,
    arg4: 43.0,
    arg5: 44.0,
    arg6: 45.0
});

child_n_fp_args!(child_7_fp_args, "7 args, hahaha long boy", {
    arg1: 50.0,
    arg2: 51.0,
    arg3: 52.0,
    arg4: 53.0,
    arg5: 54.0,
    arg6: 55.0,
    arg7: 56.0
});

child_n_fp_args!(child_8_fp_args, "8 args, hahaha long boy", {
    arg1: 60.0,
    arg2: 61.0,
    arg3: 62.0,
    arg4: 63.0,
    arg5: 64.0,
    arg6: 65.0,
    arg7: 66.0,
    arg8: 67.0
});

child_n_fp_args!(child_9_fp_args, "9 args, hahaha long boy", {
    arg1: 70.0,
    arg2: 71.0,
    arg3: 72.0,
    arg4: 73.0,
    arg5: 74.0,
    arg6: 75.0,
    arg7: 76.0,
    arg8: 77.0,
    arg9: 78.0
});

child_n_fp_args!(child_10_fp_args, "10 args, hahaha very long boy", {
    arg1: 80.0,
    arg2: 81.0,
    arg3: 82.0,
    arg4: 83.0,
    arg5: 84.0,
    arg6: 85.0,
    arg7: 86.0,
    arg8: 87.0,
    arg9: 88.0,
    arg10: 89.0
});

#[test]
fn guest_realloc_string() {
    extern "C" fn guest_fn(
        _arg1: u64,
        _arg2: u64,
        _arg3: u64,
        _arg4: u64,
        _arg5: u64,
        _arg6: u64,
        _arg7: u64,
    ) {
        let mut s = String::with_capacity(0);
        s.push_str("hello, I did some allocation");
        write!(OUTPUT_STRING.lock().unwrap(), "{}", s).unwrap();
    }

    test_body!(stack, {
        init_and_swap!(
            stack,
            guest_fn,
            [
                Val::U64(1),
                Val::U64(2),
                Val::U64(3),
                Val::U64(4),
                Val::U64(5),
                Val::U64(6),
                Val::U64(7)
            ]
        );
        assert_output_eq!("hello, I did some allocation");
    });
}

#[test]
fn guest_access_tls() {
    thread_local! {
        static TLS: RefCell<u64> = RefCell::new(0);
    }
    extern "C" fn guest_fn() {
        TLS.with(|tls| {
            write!(OUTPUT_STRING.lock().unwrap(), "tls = {}", tls.borrow()).unwrap();
        });
    }

    TLS.with(|tls| *tls.borrow_mut() = 42);
    test_body!(stack, {
        init_and_swap!(stack, guest_fn, []);
        assert_output_eq!("tls = 42");
    });
}
