//! Typed values for passing into and returning from sandboxed
//! programs.

use libc::c_void;
use std::arch::x86_64::{
    __m128, _mm_castpd_ps, _mm_castps_pd, _mm_load_pd1, _mm_load_ps1, _mm_setzero_ps,
    _mm_storeu_pd, _mm_storeu_ps,
};

use lucet_module::ValueType;

impl Val {
    pub fn value_type(&self) -> ValueType {
        match self {
            // USize, ISize, and CPtr are all as fits for definitions on the target architecture
            // (wasm) which is all 32-bit.
            Val::USize(_) | Val::ISize(_) | Val::CPtr(_) => ValueType::I32,
            Val::GuestPtr(_) => ValueType::I32,
            Val::I8(_) | Val::U8(_) | Val::I16(_) | Val::U16(_) | Val::I32(_) | Val::U32(_) => {
                ValueType::I32
            }
            Val::I64(_) | Val::U64(_) => ValueType::I64,
            Val::Bool(_) => ValueType::I32,
            Val::F32(_) => ValueType::F32,
            Val::F64(_) => ValueType::F64,
        }
    }
}

/// Typed values used for passing arguments into guest functions.
#[derive(Clone, Copy, Debug)]
pub enum Val {
    CPtr(*const c_void),
    /// A WebAssembly linear memory address
    GuestPtr(u32),
    U8(u8),
    U16(u16),
    U32(u32),
    U64(u64),
    I8(i8),
    I16(i16),
    I32(i32),
    I64(i64),
    USize(usize),
    ISize(isize),
    Bool(bool),
    F32(f32),
    F64(f64),
}

// the pointer variant is just a wrapper; the caller will know they're still responsible for their
// safety
unsafe impl Send for Val {}
unsafe impl Sync for Val {}

impl<T> From<*const T> for Val {
    fn from(x: *const T) -> Val {
        Val::CPtr(x as *const c_void)
    }
}

impl<T> From<*mut T> for Val {
    fn from(x: *mut T) -> Val {
        Val::CPtr(x as *mut c_void)
    }
}

macro_rules! impl_from_scalars {
    ( { $( $ctor:ident : $ty:ty ),* } ) => {
        $(
            impl From<$ty> for Val {
                fn from(x: $ty) -> Val {
                    Val::$ctor(x)
                }
            }
        )*
    };
}

// Since there is overlap in these enum variants, we can't have instances for all of them, such as
// GuestPtr
impl_from_scalars!({
    U8: u8,
    U16: u16,
    U32: u32,
    U64: u64,
    I8: i8,
    I16: i16,
    I32: i32,
    I64: i64,
    USize: usize,
    ISize: isize,
    Bool: bool,
    F32: f32,
    F64: f64
});

/// Register representation of `Val`.
///
/// When mapping `Val`s to x86_64 registers, we map floating point
/// values into the SSE registers _xmmN_, and all other values into
/// general-purpose (integer) registers.
pub enum RegVal {
    GpReg(u64),
    FpReg(__m128),
}

/// Convert a `Val` to its representation when stored in an
/// argument register.
pub fn val_to_reg(val: &Val) -> RegVal {
    use self::RegVal::*;
    use self::Val::*;
    match *val {
        CPtr(v) => GpReg(v as u64),
        GuestPtr(v) => GpReg(v as u64),
        U8(v) => GpReg(v as u64),
        U16(v) => GpReg(v as u64),
        U32(v) => GpReg(v as u64),
        U64(v) => GpReg(v as u64),
        I8(v) => GpReg(v as u64),
        I16(v) => GpReg(v as u64),
        I32(v) => GpReg(v as u64),
        I64(v) => GpReg(v as u64),
        USize(v) => GpReg(v as u64),
        ISize(v) => GpReg(v as u64),
        Bool(false) => GpReg(0u64),
        Bool(true) => GpReg(1u64),
        Val::F32(v) => FpReg(unsafe { _mm_load_ps1(&v as *const f32) }),
        Val::F64(v) => FpReg(unsafe { _mm_castpd_ps(_mm_load_pd1(&v as *const f64)) }),
    }
}

/// Convert a `Val` to its representation when spilled onto the
/// stack.
pub fn val_to_stack(val: &Val) -> u64 {
    use self::Val::*;
    match *val {
        CPtr(v) => v as u64,
        GuestPtr(v) => v as u64,
        U8(v) => v as u64,
        U16(v) => v as u64,
        U32(v) => v as u64,
        U64(v) => v as u64,
        I8(v) => v as u64,
        I16(v) => v as u64,
        I32(v) => v as u64,
        I64(v) => v as u64,
        USize(v) => v as u64,
        ISize(v) => v as u64,
        Bool(false) => 0u64,
        Bool(true) => 1u64,
        F32(v) => v.to_bits() as u64,
        F64(v) => v.to_bits(),
    }
}

/// A value returned by a guest function.
///
/// Since the Rust type system cannot know the type of the returned value, the user must use the
/// appropriate `From` implementation or `as_T` method.
#[derive(Clone, Copy, Debug)]
pub struct UntypedRetVal {
    fp: __m128,
    gp: u64,
}

impl std::fmt::Display for UntypedRetVal {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "<untyped return value>")
    }
}

impl UntypedRetVal {
    pub(crate) fn new(gp: u64, fp: __m128) -> UntypedRetVal {
        UntypedRetVal { gp, fp }
    }
}

impl From<RegVal> for UntypedRetVal {
    fn from(reg: RegVal) -> UntypedRetVal {
        match reg {
            RegVal::GpReg(r) => UntypedRetVal::new(r, unsafe { _mm_setzero_ps() }),
            RegVal::FpReg(r) => UntypedRetVal::new(0, r),
        }
    }
}

impl<T: Into<Val>> From<T> for UntypedRetVal {
    fn from(v: T) -> UntypedRetVal {
        val_to_reg(&v.into()).into()
    }
}

macro_rules! impl_from_fp {
    ( $ty:ty, $f:ident, $as:ident ) => {
        impl From<UntypedRetVal> for $ty {
            fn from(retval: UntypedRetVal) -> $ty {
                $f(retval.fp)
            }
        }

        impl From<&UntypedRetVal> for $ty {
            fn from(retval: &UntypedRetVal) -> $ty {
                $f(retval.fp)
            }
        }

        impl UntypedRetVal {
            pub fn $as(&self) -> $ty {
                $f(self.fp)
            }
        }
    };
}

impl_from_fp!(f32, __m128_as_f32, as_f32);
impl_from_fp!(f64, __m128_as_f64, as_f64);

macro_rules! impl_from_gp {
    ( $ty:ty, $as:ident ) => {
        impl From<UntypedRetVal> for $ty {
            fn from(retval: UntypedRetVal) -> $ty {
                retval.gp as $ty
            }
        }

        impl From<&UntypedRetVal> for $ty {
            fn from(retval: &UntypedRetVal) -> $ty {
                retval.gp as $ty
            }
        }

        impl UntypedRetVal {
            pub fn $as(&self) -> $ty {
                self.gp as $ty
            }
        }
    };
}

impl_from_gp!(u8, as_u8);
impl_from_gp!(u16, as_u16);
impl_from_gp!(u32, as_u32);
impl_from_gp!(u64, as_u64);

impl_from_gp!(i8, as_i8);
impl_from_gp!(i16, as_i16);
impl_from_gp!(i32, as_i32);
impl_from_gp!(i64, as_i64);

impl From<UntypedRetVal> for bool {
    fn from(retval: UntypedRetVal) -> bool {
        retval.gp != 0
    }
}

impl From<&UntypedRetVal> for bool {
    fn from(retval: &UntypedRetVal) -> bool {
        retval.gp != 0
    }
}

impl UntypedRetVal {
    pub fn as_bool(&self) -> bool {
        self.gp != 0
    }

    pub fn as_ptr<T>(&self) -> *const T {
        self.gp as *const T
    }

    pub fn as_mut<T>(&self) -> *mut T {
        self.gp as *mut T
    }
}

impl Default for UntypedRetVal {
    fn default() -> UntypedRetVal {
        let fp = unsafe { _mm_setzero_ps() };
        UntypedRetVal { fp, gp: 0 }
    }
}

pub trait UntypedRetValInternal {
    fn fp(&self) -> __m128;
    fn gp(&self) -> u64;
}

impl UntypedRetValInternal for UntypedRetVal {
    fn fp(&self) -> __m128 {
        self.fp
    }

    fn gp(&self) -> u64 {
        self.gp
    }
}

// Helpers that we might want to put in a utils module someday

/// Interpret the contents of a `__m128` register as an `f32`.
pub fn __m128_as_f32(v: __m128) -> f32 {
    let mut out: [f32; 4] = [0.0; 4];
    unsafe {
        _mm_storeu_ps(&mut out[0] as *mut f32, v);
    }
    out[0]
}

/// Interpret the contents of a `__m128` register as an `f64`.
pub fn __m128_as_f64(v: __m128) -> f64 {
    let mut out: [f64; 2] = [0.0; 2];
    unsafe {
        let vd = _mm_castps_pd(v);
        _mm_storeu_pd(&mut out[0] as *mut f64, vd);
    }
    out[0]
}
