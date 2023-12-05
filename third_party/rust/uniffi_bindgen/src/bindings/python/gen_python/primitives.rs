/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::backend::{CodeType, Literal};
use crate::interface::Radix;
use paste::paste;

fn render_literal(literal: &Literal) -> String {
    match literal {
        Literal::Boolean(v) => {
            if *v {
                "True".into()
            } else {
                "False".into()
            }
        }
        Literal::String(s) => format!("\"{s}\""),
        // https://docs.python.org/3/reference/lexical_analysis.html#integer-literals
        Literal::Int(i, radix, _) => match radix {
            Radix::Octal => format!("int(0o{i:o})"),
            Radix::Decimal => format!("{i}"),
            Radix::Hexadecimal => format!("{i:#x}"),
        },
        Literal::UInt(i, radix, _) => match radix {
            Radix::Octal => format!("0o{i:o}"),
            Radix::Decimal => format!("{i}"),
            Radix::Hexadecimal => format!("{i:#x}"),
        },
        Literal::Float(string, _type_) => string.clone(),

        _ => unreachable!("Literal"),
    }
}

macro_rules! impl_code_type_for_primitive {
    ($T:ty, $python_name:literal, $canonical_name:literal) => {
        paste! {
            #[derive(Debug)]
            pub struct $T;
            impl CodeType for $T  {
                fn type_label(&self) -> String {
                    $python_name.into()
                }

                fn canonical_name(&self) -> String {
                    $canonical_name.into()
                }

                fn literal(&self, literal: &Literal) -> String {
                    render_literal(&literal)
                }
            }
        }
    };
}

impl_code_type_for_primitive!(BooleanCodeType, "bool", "Bool");
impl_code_type_for_primitive!(StringCodeType, "str", "String");
impl_code_type_for_primitive!(BytesCodeType, "bytes", "Bytes");
impl_code_type_for_primitive!(Int8CodeType, "int", "Int8");
impl_code_type_for_primitive!(Int16CodeType, "int", "Int16");
impl_code_type_for_primitive!(Int32CodeType, "int", "Int32");
impl_code_type_for_primitive!(Int64CodeType, "int", "Int64");
impl_code_type_for_primitive!(UInt8CodeType, "int", "UInt8");
impl_code_type_for_primitive!(UInt16CodeType, "int", "UInt16");
impl_code_type_for_primitive!(UInt32CodeType, "int", "UInt32");
impl_code_type_for_primitive!(UInt64CodeType, "int", "UInt64");
impl_code_type_for_primitive!(Float32CodeType, "float", "Float");
impl_code_type_for_primitive!(Float64CodeType, "float", "Double");
