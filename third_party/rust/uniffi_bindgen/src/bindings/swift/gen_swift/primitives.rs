/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::backend::{CodeOracle, CodeType, Literal};
use crate::interface::{types::Type, Radix};
use askama::Template;
use paste::paste;

#[allow(unused_imports)]
use super::filters;

fn render_literal(oracle: &dyn CodeOracle, literal: &Literal) -> String {
    fn typed_number(oracle: &dyn CodeOracle, type_: &Type, num_str: String) -> String {
        match type_ {
            // special case Int32.
            Type::Int32 => num_str,
            // otherwise use constructor e.g. UInt8(x)
            Type::Int8
            | Type::UInt8
            | Type::Int16
            | Type::UInt16
            | Type::UInt32
            | Type::Int64
            | Type::UInt64
            | Type::Float32
            | Type::Float64 =>
            // XXX we should pass in the codetype itself.
            {
                format!("{}({})", oracle.find(type_).type_label(oracle), num_str)
            }
            _ => panic!("Unexpected literal: {} is not a number", num_str),
        }
    }

    match literal {
        Literal::Boolean(v) => format!("{}", v),
        Literal::String(s) => format!("\"{}\"", s),
        Literal::Int(i, radix, type_) => typed_number(
            oracle,
            type_,
            match radix {
                Radix::Octal => format!("0o{:o}", i),
                Radix::Decimal => format!("{}", i),
                Radix::Hexadecimal => format!("{:#x}", i),
            },
        ),
        Literal::UInt(i, radix, type_) => typed_number(
            oracle,
            type_,
            match radix {
                Radix::Octal => format!("0o{:o}", i),
                Radix::Decimal => format!("{}", i),
                Radix::Hexadecimal => format!("{:#x}", i),
            },
        ),
        Literal::Float(string, type_) => typed_number(oracle, type_, string.clone()),
        _ => unreachable!("Literal"),
    }
}

macro_rules! impl_code_type_for_primitive {
    ($T:ty, $class_name:literal, $template_file:literal) => {
        paste! {
            #[derive(Template)]
            #[template(syntax = "swift", escape = "none", path = $template_file)]
            pub struct $T;

            impl CodeType for $T  {
                fn type_label(&self, _oracle: &dyn CodeOracle) -> String {
                    $class_name.into()
                }

                fn literal(&self, oracle: &dyn CodeOracle, literal: &Literal) -> String {
                    render_literal(oracle, &literal)
                }

                fn helper_code(&self, _oracle: &dyn CodeOracle) -> Option<String> {
                    Some(self.render().unwrap())
                }
            }
        }
    };
}

impl_code_type_for_primitive!(BooleanCodeType, "Bool", "BooleanHelper.swift");
impl_code_type_for_primitive!(StringCodeType, "String", "StringHelper.swift");
impl_code_type_for_primitive!(Int8CodeType, "Int8", "Int8Helper.swift");
impl_code_type_for_primitive!(Int16CodeType, "Int16", "Int16Helper.swift");
impl_code_type_for_primitive!(Int32CodeType, "Int32", "Int32Helper.swift");
impl_code_type_for_primitive!(Int64CodeType, "Int64", "Int64Helper.swift");
impl_code_type_for_primitive!(UInt8CodeType, "UInt8", "UInt8Helper.swift");
impl_code_type_for_primitive!(UInt16CodeType, "UInt16", "UInt16Helper.swift");
impl_code_type_for_primitive!(UInt32CodeType, "UInt32", "UInt32Helper.swift");
impl_code_type_for_primitive!(UInt64CodeType, "UInt64", "UInt64Helper.swift");
impl_code_type_for_primitive!(Float32CodeType, "Float", "Float32Helper.swift");
impl_code_type_for_primitive!(Float64CodeType, "Double", "Float64Helper.swift");
