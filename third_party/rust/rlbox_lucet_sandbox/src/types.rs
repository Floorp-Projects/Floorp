use lucet_module::{Signature, ValueType};
use lucet_runtime::{InstanceHandle, MmapRegion};
use lucet_runtime_internals::val::Val;

use std::convert::TryFrom;
use std::ffi::c_void;
use std::sync::Arc;

pub struct LucetSandboxInstance {
    pub region: Arc<MmapRegion>,
    pub instance_handle: InstanceHandle,
    pub signatures: Vec<Signature>,
}

#[derive(Debug)]
pub enum Error {
    GoblinError(goblin::error::Error),
    WasiError(failure::Error),
    LucetRuntimeError(lucet_runtime_internals::error::Error),
    LucetModuleError(lucet_module::Error),
}

impl From<goblin::error::Error> for Error {
    fn from(e: goblin::error::Error) -> Self {
        Error::GoblinError(e)
    }
}

impl From<failure::Error> for Error {
    fn from(e: failure::Error) -> Self {
        Error::WasiError(e)
    }
}

impl From<lucet_runtime_internals::error::Error> for Error {
    fn from(e: lucet_runtime_internals::error::Error) -> Self {
        Error::LucetRuntimeError(e)
    }
}

impl From<lucet_module::Error> for Error {
    fn from(e: lucet_module::Error) -> Self {
        Error::LucetModuleError(e)
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub enum LucetValueType {
    I32,
    I64,
    F32,
    F64,
    Void,
}

impl From<Option<ValueType>> for LucetValueType {
    fn from(value_type: Option<ValueType>) -> Self {
        match value_type {
            Some(value_type_val) => match value_type_val {
                ValueType::I32 => LucetValueType::I32,
                ValueType::I64 => LucetValueType::I64,
                ValueType::F32 => LucetValueType::F32,
                ValueType::F64 => LucetValueType::F64,
            },
            _ => LucetValueType::Void,
        }
    }
}

impl From<ValueType> for LucetValueType {
    fn from(value_type: ValueType) -> Self {
        match value_type {
            ValueType::I32 => LucetValueType::I32,
            ValueType::I64 => LucetValueType::I64,
            ValueType::F32 => LucetValueType::F32,
            ValueType::F64 => LucetValueType::F64,
        }
    }
}

impl Into<Option<ValueType>> for LucetValueType {
    fn into(self) -> Option<ValueType> {
        match self {
            LucetValueType::I32 => Some(ValueType::I32),
            LucetValueType::I64 => Some(ValueType::I64),
            LucetValueType::F32 => Some(ValueType::F32),
            LucetValueType::F64 => Some(ValueType::F64),
            _ => None,
        }
    }
}

impl Into<ValueType> for LucetValueType {
    fn into(self) -> ValueType {
        match self {
            LucetValueType::I32 => ValueType::I32,
            LucetValueType::I64 => ValueType::I64,
            LucetValueType::F32 => ValueType::F32,
            LucetValueType::F64 => ValueType::F64,
            _ => panic!("Unexpected!"),
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub union LucetValueUnion {
    as_u32: u32,
    as_u64: u64,
    as_f32: f32,
    as_f64: f64,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct LucetValue {
    val_type: LucetValueType,
    val : LucetValueUnion,
}

impl From<&LucetValue> for Val {
    fn from(val: &LucetValue) -> Val {
        match val.val_type {
            LucetValueType::I32 => Val::U32(unsafe { val.val.as_u32 }),
            LucetValueType::I64 => Val::U64(unsafe { val.val.as_u64 }),
            LucetValueType::F32 => Val::F32(unsafe { val.val.as_f32 }),
            LucetValueType::F64 => Val::F64(unsafe { val.val.as_f64 }),
            _ => panic!("Unexpected!"),
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct LucetFunctionSignature {
    ret: LucetValueType,
    parameter_cnt: u32,
    parameters: *mut LucetValueType,
}

impl Into<Signature> for LucetFunctionSignature {
    fn into(self) -> Signature {
        let len = usize::try_from(self.parameter_cnt).unwrap();
        let vec = unsafe { Vec::from_raw_parts(self.parameters, len, len) };
        let p: Vec<ValueType> = vec.iter().map(|x| x.clone().into()).collect();
        std::mem::forget(vec);
        Signature {
            params: p,
            ret_ty: self.ret.into(),
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct SizedBuffer {
    pub data: *mut c_void,
    pub length: usize,
}
