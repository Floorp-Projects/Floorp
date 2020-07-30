use serde::{Deserialize, Serialize};
use std::fmt::{Display, Formatter};

#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub enum ValueType {
    I32,
    I64,
    F32,
    F64,
}

impl Display for ValueType {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        match self {
            ValueType::I32 => write!(f, "I32"),
            ValueType::I64 => write!(f, "I64"),
            ValueType::F32 => write!(f, "F32"),
            ValueType::F64 => write!(f, "F64"),
        }
    }
}

/// A signature for a function in a wasm module.
///
/// Note that this does not explicitly name VMContext as a parameter! It is assumed that all wasm
/// functions take VMContext as their first parameter.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct Signature {
    pub params: Vec<ValueType>,
    // In the future, wasm may permit this to be a Vec of ValueType
    pub ret_ty: Option<ValueType>,
}

impl Display for Signature {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        write!(f, "(")?;
        for (i, p) in self.params.iter().enumerate() {
            if i == 0 {
                write!(f, "{}", p)?;
            } else {
                write!(f, ", {}", p)?;
            }
        }
        write!(f, ") -> ")?;
        match self.ret_ty {
            Some(ty) => write!(f, "{}", ty),
            None => write!(f, "()"),
        }
    }
}

#[macro_export]
macro_rules! lucet_signature {
    ((() -> ())) => {
        $crate::Signature {
            params: vec![],
            ret_ty: None
        }
    };
    (($($arg_ty:ident),*) -> ()) => {
        $crate::Signature {
            params: vec![$($crate::ValueType::$arg_ty),*],
            ret_ty: None,
        }
    };
    (($($arg_ty:ident),*) -> $ret_ty:ident) => {
        $crate::Signature {
            params: vec![$($crate::ValueType::$arg_ty),*],
            ret_ty: Some($crate::ValueType::$ret_ty),
        }
    };
}
