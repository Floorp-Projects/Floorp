#![allow(non_camel_case_types)]

pub use self::lucet_result::*;
pub use self::lucet_val::*;

use crate::alloc::Limits;
use crate::error::Error;
use crate::instance::signals::SignalBehavior;
use libc::{c_int, c_void};
use num_derive::FromPrimitive;

#[macro_export]
macro_rules! assert_nonnull {
    ( $name:ident ) => {
        if $name.is_null() {
            return lucet_error::InvalidArgument;
        }
    };
}

/// Wrap up the management of `Arc`s that go across the FFI boundary.
///
/// Trait objects must be wrapped in two `Arc`s in order to yield a thin pointer.
#[macro_export]
macro_rules! with_ffi_arcs {
    ( [ $name:ident : dyn $ty:ident ], $body:block ) => {{
        assert_nonnull!($name);
        let $name = Arc::from_raw($name as *const Arc<dyn $ty>);
        let res = $body;
        Arc::into_raw($name);
        res
    }};
    ( [ $name:ident : $ty:ty ], $body:block ) => {{
        assert_nonnull!($name);
        let $name = Arc::from_raw($name as *const $ty);
        let res = $body;
        Arc::into_raw($name);
        res
    }};
    ( [ $name:ident : dyn $ty:ident, $($tail:tt)* ], $body:block ) => {{
        assert_nonnull!($name);
        let $name = Arc::from_raw($name as *const Arc<dyn $ty>);
        let rec = with_ffi_arcs!([$($tail)*], $body);
        Arc::into_raw($name);
        rec
    }};
    ( [ $name:ident : $ty:ty, $($tail:tt)* ], $body:block ) => {{
        assert_nonnull!($name);
        let $name = Arc::from_raw($name as *const $ty);
        let rec = with_ffi_arcs!([$($tail)*], $body);
        Arc::into_raw($name);
        rec
    }};
}

/// Marker type for the `vmctx` pointer argument.
///
/// This type should only be used with [`Vmctx::from_raw()`](struct.Vmctx.html#method.from_raw) or
/// the C API.
#[repr(C)]
pub struct lucet_vmctx {
    _unused: [u8; 0],
}

#[repr(C)]
#[derive(Clone, Copy, Debug, FromPrimitive)]
pub enum lucet_error {
    Ok,
    InvalidArgument,
    RegionFull,
    Module,
    LimitsExceeded,
    NoLinearMemory,
    SymbolNotFound,
    FuncNotFound,
    RuntimeFault,
    RuntimeTerminated,
    Dl,
    InstanceNotReturned,
    InstanceNotYielded,
    StartYielded,
    Internal,
    Unsupported,
}

impl From<Error> for lucet_error {
    fn from(e: Error) -> lucet_error {
        lucet_error::from(&e)
    }
}

impl From<&Error> for lucet_error {
    fn from(e: &Error) -> lucet_error {
        match e {
            Error::InvalidArgument(_) => lucet_error::InvalidArgument,
            Error::RegionFull(_) => lucet_error::RegionFull,
            Error::ModuleError(_) => lucet_error::Module,
            Error::LimitsExceeded(_) => lucet_error::LimitsExceeded,
            Error::NoLinearMemory(_) => lucet_error::NoLinearMemory,
            Error::SymbolNotFound(_) => lucet_error::SymbolNotFound,
            Error::FuncNotFound(_, _) => lucet_error::FuncNotFound,
            Error::RuntimeFault(_) => lucet_error::RuntimeFault,
            Error::RuntimeTerminated(_) => lucet_error::RuntimeTerminated,
            Error::DlError(_) => lucet_error::Dl,
            Error::InstanceNotReturned => lucet_error::InstanceNotReturned,
            Error::InstanceNotYielded => lucet_error::InstanceNotYielded,
            Error::StartYielded => lucet_error::StartYielded,
            Error::InternalError(_) => lucet_error::Internal,
            Error::Unsupported(_) => lucet_error::Unsupported,
        }
    }
}

#[repr(C)]
pub struct lucet_instance {
    _unused: [u8; 0],
}

#[repr(C)]
pub struct lucet_region {
    _unused: [u8; 0],
}

#[repr(C)]
pub struct lucet_dl_module {
    _unused: [u8; 0],
}

/// Runtime limits for the various memories that back a Lucet instance.
///
/// Each value is specified in bytes, and must be evenly divisible by the host page size (4K).
#[derive(Clone, Debug)]
#[repr(C)]
pub struct lucet_alloc_limits {
    /// Max size of the heap, which can be backed by real memory. (default 1M)
    pub heap_memory_size: u64,
    /// Size of total virtual memory. (default 8G)
    pub heap_address_space_size: u64,
    /// Size of the guest stack. (default 128K)
    pub stack_size: u64,
    /// Size of the globals region in bytes; each global uses 8 bytes. (default 4K)
    pub globals_size: u64,
    /// Size of the signal stack in bytes. (default SIGSTKSZ for Rust release builds, 12K for Rust
    /// debug builds)
    ///
    /// This difference is to account for the greatly increased stack size usage in the signal
    /// handler when running without optimizations.
    ///
    /// Note that debug vs. release mode is determined by `cfg(debug_assertions)`, so if you are
    /// specifically enabling Rust debug assertions in your Cargo release builds, the default signal
    /// stack will be larger.
    pub signal_stack_size: u64,
}

impl From<Limits> for lucet_alloc_limits {
    fn from(limits: Limits) -> lucet_alloc_limits {
        (&limits).into()
    }
}

impl From<&Limits> for lucet_alloc_limits {
    fn from(limits: &Limits) -> lucet_alloc_limits {
        lucet_alloc_limits {
            heap_memory_size: limits.heap_memory_size as u64,
            heap_address_space_size: limits.heap_address_space_size as u64,
            stack_size: limits.stack_size as u64,
            globals_size: limits.globals_size as u64,
            signal_stack_size: limits.signal_stack_size as u64,
        }
    }
}

impl From<lucet_alloc_limits> for Limits {
    fn from(limits: lucet_alloc_limits) -> Limits {
        (&limits).into()
    }
}

impl From<&lucet_alloc_limits> for Limits {
    fn from(limits: &lucet_alloc_limits) -> Limits {
        Limits {
            heap_memory_size: limits.heap_memory_size as usize,
            heap_address_space_size: limits.heap_address_space_size as usize,
            stack_size: limits.stack_size as usize,
            globals_size: limits.globals_size as usize,
            signal_stack_size: limits.signal_stack_size as usize,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub enum lucet_signal_behavior {
    Default,
    Continue,
    Terminate,
}

impl From<lucet_signal_behavior> for SignalBehavior {
    fn from(sb: lucet_signal_behavior) -> SignalBehavior {
        sb.into()
    }
}

impl From<&lucet_signal_behavior> for SignalBehavior {
    fn from(sb: &lucet_signal_behavior) -> SignalBehavior {
        match sb {
            lucet_signal_behavior::Default => SignalBehavior::Default,
            lucet_signal_behavior::Continue => SignalBehavior::Continue,
            lucet_signal_behavior::Terminate => SignalBehavior::Terminate,
        }
    }
}

pub type lucet_signal_handler = unsafe extern "C" fn(
    inst: *mut lucet_instance,
    trap: lucet_result::lucet_trapcode,
    signum: c_int,
    siginfo: *const libc::siginfo_t,
    context: *const c_void,
) -> lucet_signal_behavior;

pub type lucet_fatal_handler = unsafe extern "C" fn(inst: *mut lucet_instance);

pub struct CTerminationDetails {
    pub details: *mut c_void,
}

unsafe impl Send for CTerminationDetails {}
unsafe impl Sync for CTerminationDetails {}

pub struct CYieldedVal {
    pub val: *mut c_void,
}

unsafe impl Send for CYieldedVal {}
unsafe impl Sync for CYieldedVal {}

pub mod lucet_result {
    use super::lucet_error;
    use crate::c_api::{lucet_val, CTerminationDetails, CYieldedVal};
    use crate::error::Error;
    use crate::instance::{RunResult, TerminationDetails};
    use crate::module::{AddrDetails, TrapCode};
    use libc::{c_uchar, c_void};
    use num_derive::FromPrimitive;
    use std::ffi::CString;
    use std::ptr;

    impl From<Result<RunResult, Error>> for lucet_result {
        fn from(res: Result<RunResult, Error>) -> lucet_result {
            match res {
                Ok(RunResult::Returned(retval)) => lucet_result {
                    tag: lucet_result_tag::Returned,
                    val: lucet_result_val {
                        returned: retval.into(),
                    },
                },
                Ok(RunResult::Yielded(val)) => lucet_result {
                    tag: lucet_result_tag::Yielded,
                    val: lucet_result_val {
                        yielded: lucet_yielded {
                            val: val
                                .downcast_ref()
                                .map(|CYieldedVal { val }| *val)
                                .unwrap_or(ptr::null_mut()),
                        },
                    },
                },
                // TODO: test this path; currently our C API tests don't include any faulting tests
                Err(Error::RuntimeFault(details)) => lucet_result {
                    tag: lucet_result_tag::Faulted,
                    val: lucet_result_val {
                        fault: lucet_runtime_faulted {
                            fatal: details.fatal,
                            trapcode: details.trapcode.into(),
                            rip_addr: details.rip_addr,
                            rip_addr_details: details.rip_addr_details.into(),
                        },
                    },
                },
                // TODO: test this path; currently our C API tests don't include any terminating tests
                Err(Error::RuntimeTerminated(details)) => lucet_result {
                    tag: lucet_result_tag::Terminated,
                    val: lucet_result_val {
                        terminated: match details {
                            TerminationDetails::Signal => lucet_terminated {
                                reason: lucet_terminated_reason::Signal,
                                provided: ptr::null_mut(),
                            },
                            TerminationDetails::CtxNotFound => lucet_terminated {
                                reason: lucet_terminated_reason::CtxNotFound,
                                provided: ptr::null_mut(),
                            },
                            TerminationDetails::YieldTypeMismatch => lucet_terminated {
                                reason: lucet_terminated_reason::YieldTypeMismatch,
                                provided: ptr::null_mut(),
                            },
                            TerminationDetails::BorrowError(_) => lucet_terminated {
                                reason: lucet_terminated_reason::BorrowError,
                                provided: ptr::null_mut(),
                            },
                            TerminationDetails::Provided(p) => lucet_terminated {
                                reason: lucet_terminated_reason::Provided,
                                provided: p
                                    .downcast_ref()
                                    .map(|CTerminationDetails { details }| *details)
                                    .unwrap_or(ptr::null_mut()),
                            },
                        },
                    },
                },
                Err(e) => lucet_result {
                    tag: lucet_result_tag::Errored,
                    val: lucet_result_val { errored: e.into() },
                },
            }
        }
    }

    #[repr(C)]
    #[derive(Clone, Copy)]
    pub struct lucet_result {
        pub tag: lucet_result_tag,
        pub val: lucet_result_val,
    }

    #[repr(C)]
    #[derive(Clone, Copy, Debug, FromPrimitive)]
    pub enum lucet_result_tag {
        Returned,
        Yielded,
        Faulted,
        Terminated,
        Errored,
    }

    #[repr(C)]
    #[derive(Clone, Copy)]
    pub union lucet_result_val {
        pub returned: lucet_val::lucet_untyped_retval,
        pub yielded: lucet_yielded,
        pub fault: lucet_runtime_faulted,
        pub terminated: lucet_terminated,
        pub errored: lucet_error,
    }

    #[repr(C)]
    #[derive(Clone, Copy)]
    pub struct lucet_terminated {
        pub reason: lucet_terminated_reason,
        pub provided: *mut c_void,
    }

    #[repr(C)]
    #[derive(Clone, Copy)]
    pub enum lucet_terminated_reason {
        Signal,
        CtxNotFound,
        YieldTypeMismatch,
        BorrowError,
        Provided,
    }

    #[repr(C)]
    #[derive(Clone, Copy)]
    pub struct lucet_yielded {
        pub val: *mut c_void,
    }

    #[repr(C)]
    #[derive(Clone, Copy)]
    pub struct lucet_runtime_faulted {
        pub fatal: bool,
        pub trapcode: lucet_trapcode,
        pub rip_addr: libc::uintptr_t,
        pub rip_addr_details: lucet_module_addr_details,
    }

    #[repr(C)]
    #[derive(Clone, Copy, Debug)]
    pub enum lucet_trapcode {
        StackOverflow,
        HeapOutOfBounds,
        OutOfBounds,
        IndirectCallToNull,
        BadSignature,
        IntegerOverflow,
        IntegerDivByZero,
        BadConversionToInteger,
        Interrupt,
        TableOutOfBounds,
        Unreachable,
        Unknown,
    }

    impl From<Option<TrapCode>> for lucet_trapcode {
        fn from(ty: Option<TrapCode>) -> lucet_trapcode {
            (&ty).into()
        }
    }

    impl From<&Option<TrapCode>> for lucet_trapcode {
        fn from(ty: &Option<TrapCode>) -> lucet_trapcode {
            if let Some(ty) = ty {
                match ty {
                    TrapCode::StackOverflow => lucet_trapcode::StackOverflow,
                    TrapCode::HeapOutOfBounds => lucet_trapcode::HeapOutOfBounds,
                    TrapCode::OutOfBounds => lucet_trapcode::OutOfBounds,
                    TrapCode::IndirectCallToNull => lucet_trapcode::IndirectCallToNull,
                    TrapCode::BadSignature => lucet_trapcode::BadSignature,
                    TrapCode::IntegerOverflow => lucet_trapcode::IntegerOverflow,
                    TrapCode::IntegerDivByZero => lucet_trapcode::IntegerDivByZero,
                    TrapCode::BadConversionToInteger => lucet_trapcode::BadConversionToInteger,
                    TrapCode::Interrupt => lucet_trapcode::Interrupt,
                    TrapCode::TableOutOfBounds => lucet_trapcode::TableOutOfBounds,
                    TrapCode::Unreachable => lucet_trapcode::Unreachable,
                }
            } else {
                lucet_trapcode::Unknown
            }
        }
    }

    const ADDR_DETAILS_NAME_LEN: usize = 256;

    /// Half a kilobyte is too substantial for `Copy`, but we must have it because [unions with
    /// non-`Copy` fields are unstable](https://github.com/rust-lang/rust/issues/32836).
    #[repr(C)]
    #[derive(Clone, Copy)]
    pub struct lucet_module_addr_details {
        pub module_code_resolvable: bool,
        pub in_module_code: bool,
        pub file_name: [c_uchar; ADDR_DETAILS_NAME_LEN],
        pub sym_name: [c_uchar; ADDR_DETAILS_NAME_LEN],
    }

    impl Default for lucet_module_addr_details {
        fn default() -> Self {
            lucet_module_addr_details {
                module_code_resolvable: false,
                in_module_code: false,
                file_name: [0; ADDR_DETAILS_NAME_LEN],
                sym_name: [0; ADDR_DETAILS_NAME_LEN],
            }
        }
    }

    impl From<Option<AddrDetails>> for lucet_module_addr_details {
        fn from(details: Option<AddrDetails>) -> Self {
            /// Convert a string into C-compatible bytes, truncate it to length
            /// `ADDR_DETAILS_NAME_LEN`, and make sure it has a trailing nul.
            fn trunc_c_str_bytes(s: &str) -> Vec<u8> {
                let s = CString::new(s);
                let mut bytes = s.ok().map(|s| s.into_bytes_with_nul()).unwrap_or(vec![0]);
                bytes.truncate(ADDR_DETAILS_NAME_LEN);
                // we always have at least the 0, so this `last` can be unwrapped
                *bytes.last_mut().unwrap() = 0;
                bytes
            }

            let mut ret = details
                .as_ref()
                .map(|details| lucet_module_addr_details {
                    module_code_resolvable: true,
                    in_module_code: details.in_module_code,
                    file_name: [0; ADDR_DETAILS_NAME_LEN],
                    sym_name: [0; ADDR_DETAILS_NAME_LEN],
                })
                .unwrap_or_default();

            // get truncated C-compatible bytes for each string, or "\0" if they're not present
            let file_name_bytes = details
                .as_ref()
                .and_then(|details| details.file_name.as_ref().map(|s| trunc_c_str_bytes(s)))
                .unwrap_or_else(|| vec![0]);
            let sym_name_bytes = details
                .and_then(|details| details.sym_name.as_ref().map(|s| trunc_c_str_bytes(s)))
                .unwrap_or_else(|| vec![0]);

            // copy the bytes into the array, making sure to copy only as many as are in the string
            ret.file_name[0..file_name_bytes.len()].copy_from_slice(file_name_bytes.as_slice());
            ret.sym_name[0..sym_name_bytes.len()].copy_from_slice(sym_name_bytes.as_slice());

            ret
        }
    }
}

pub mod lucet_val {
    use crate::val::{UntypedRetVal, UntypedRetValInternal, Val};
    use libc::{c_char, c_void};

    // Note on the value associated with each type: the most significant bits represent the "class"
    // of the type (1: a C pointer, 2: something unsigned that fits in 64 bits, 3: something signed
    // that fits in 64 bits, 4: f32, 5: f64). The remain bits can be anything as long as it is
    // unique.
    #[repr(C)]
    #[derive(Clone, Copy, Debug)]
    pub enum lucet_val_type {
        C_Ptr,    // = (1 << 16) | 0x0100,
        GuestPtr, // = (2 << 16) | 0x0101,
        U8,       // = (2 << 16) | 0x0201,
        U16,      // = (2 << 16) | 0x0202,
        U32,      // = (2 << 16) | 0x0203,
        U64,      // = (2 << 16) | 0x0204,
        I8,       // = (3 << 16) | 0x0300,
        I16,      // = (3 << 16) | 0x0301,
        I32,      // = (3 << 16) | 0x0302,
        I64,      // = (3 << 16) | 0x0303,
        USize,    // = (2 << 16) | 0x0400,
        ISize,    // = (3 << 16) | 0x0401,
        Bool,     // = (2 << 16) | 0x0700,
        F32,      // = (4 << 16) | 0x0800,
        F64,      // = (5 << 16) | 0x0801,
    }

    #[repr(C)]
    #[derive(Clone, Copy)]
    pub union lucet_val_inner_val {
        as_c_ptr: *mut c_void, // (1 << 16)
        as_u64: u64,           // (2 << 16)
        as_i64: i64,           // (3 << 16)
        as_f32: f32,           // (4 << 16)
        as_f64: f64,           // (5 << 16)
    }

    #[repr(C)]
    #[derive(Clone, Copy)]
    pub struct lucet_val {
        ty: lucet_val_type,
        inner_val: lucet_val_inner_val,
    }

    impl From<lucet_val> for Val {
        fn from(val: lucet_val) -> Val {
            (&val).into()
        }
    }

    impl From<&lucet_val> for Val {
        fn from(val: &lucet_val) -> Val {
            match val.ty {
                lucet_val_type::C_Ptr => Val::CPtr(unsafe { val.inner_val.as_u64 } as _),
                lucet_val_type::GuestPtr => Val::GuestPtr(unsafe { val.inner_val.as_u64 } as _),
                lucet_val_type::U8 => Val::U8(unsafe { val.inner_val.as_u64 } as _),
                lucet_val_type::U16 => Val::U16(unsafe { val.inner_val.as_u64 } as _),
                lucet_val_type::U32 => Val::U32(unsafe { val.inner_val.as_u64 } as _),
                lucet_val_type::U64 => Val::U64(unsafe { val.inner_val.as_u64 } as _),
                lucet_val_type::I8 => Val::I16(unsafe { val.inner_val.as_i64 } as _),
                lucet_val_type::I16 => Val::I32(unsafe { val.inner_val.as_i64 } as _),
                lucet_val_type::I32 => Val::I32(unsafe { val.inner_val.as_i64 } as _),
                lucet_val_type::I64 => Val::I64(unsafe { val.inner_val.as_i64 } as _),
                lucet_val_type::USize => Val::USize(unsafe { val.inner_val.as_u64 } as _),
                lucet_val_type::ISize => Val::ISize(unsafe { val.inner_val.as_i64 } as _),
                lucet_val_type::Bool => Val::Bool(unsafe { val.inner_val.as_u64 } != 0),
                lucet_val_type::F32 => Val::F32(unsafe { val.inner_val.as_f32 } as _),
                lucet_val_type::F64 => Val::F64(unsafe { val.inner_val.as_f64 } as _),
            }
        }
    }

    impl From<Val> for lucet_val {
        fn from(val: Val) -> Self {
            (&val).into()
        }
    }

    impl From<&Val> for lucet_val {
        fn from(val: &Val) -> Self {
            match val {
                Val::CPtr(a) => lucet_val {
                    ty: lucet_val_type::C_Ptr,
                    inner_val: lucet_val_inner_val { as_u64: *a as _ },
                },
                Val::GuestPtr(a) => lucet_val {
                    ty: lucet_val_type::GuestPtr,
                    inner_val: lucet_val_inner_val { as_u64: *a as _ },
                },
                Val::U8(a) => lucet_val {
                    ty: lucet_val_type::U8,
                    inner_val: lucet_val_inner_val { as_u64: *a as _ },
                },
                Val::U16(a) => lucet_val {
                    ty: lucet_val_type::U16,
                    inner_val: lucet_val_inner_val { as_u64: *a as _ },
                },
                Val::U32(a) => lucet_val {
                    ty: lucet_val_type::U32,
                    inner_val: lucet_val_inner_val { as_u64: *a as _ },
                },
                Val::U64(a) => lucet_val {
                    ty: lucet_val_type::U64,
                    inner_val: lucet_val_inner_val { as_u64: *a as _ },
                },
                Val::I8(a) => lucet_val {
                    ty: lucet_val_type::I8,
                    inner_val: lucet_val_inner_val { as_i64: *a as _ },
                },
                Val::I16(a) => lucet_val {
                    ty: lucet_val_type::I16,
                    inner_val: lucet_val_inner_val { as_i64: *a as _ },
                },
                Val::I32(a) => lucet_val {
                    ty: lucet_val_type::I32,
                    inner_val: lucet_val_inner_val { as_i64: *a as _ },
                },
                Val::I64(a) => lucet_val {
                    ty: lucet_val_type::I64,
                    inner_val: lucet_val_inner_val { as_i64: *a as _ },
                },
                Val::USize(a) => lucet_val {
                    ty: lucet_val_type::USize,
                    inner_val: lucet_val_inner_val { as_u64: *a as _ },
                },
                Val::ISize(a) => lucet_val {
                    ty: lucet_val_type::ISize,
                    inner_val: lucet_val_inner_val { as_i64: *a as _ },
                },
                Val::Bool(a) => lucet_val {
                    ty: lucet_val_type::Bool,
                    inner_val: lucet_val_inner_val { as_u64: *a as _ },
                },
                Val::F32(a) => lucet_val {
                    ty: lucet_val_type::F32,
                    inner_val: lucet_val_inner_val { as_f32: *a as _ },
                },
                Val::F64(a) => lucet_val {
                    ty: lucet_val_type::F64,
                    inner_val: lucet_val_inner_val { as_f64: *a as _ },
                },
            }
        }
    }

    #[repr(C)]
    #[derive(Clone, Copy, Debug)]
    pub struct lucet_untyped_retval {
        pub fp: [c_char; 16],
        pub gp: [c_char; 8],
    }

    #[repr(C)]
    #[derive(Clone, Copy)]
    pub union lucet_retval_gp {
        pub as_untyped: [c_char; 8],
        pub as_c_ptr: *mut c_void,
        pub as_u64: u64,
        pub as_i64: i64,
    }

    impl From<UntypedRetVal> for lucet_untyped_retval {
        fn from(retval: UntypedRetVal) -> lucet_untyped_retval {
            let mut v = lucet_untyped_retval {
                fp: [0; 16],
                gp: [0; 8],
            };
            unsafe {
                core::arch::x86_64::_mm_storeu_ps(
                    v.fp.as_mut().as_mut_ptr() as *mut f32,
                    retval.fp(),
                );
                *(v.gp.as_mut().as_mut_ptr() as *mut u64) = retval.gp();
            }
            v
        }
    }
}
