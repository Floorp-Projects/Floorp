/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! # Support for literal values.
//!
//! This module provides support for interpreting literal values from the UDL,
//! which appear in places such as default arguments.

use anyhow::{bail, Result};

use super::types::Type;

// Represents a literal value.
// Used for e.g. default argument values.
#[derive(Debug, Clone, Hash)]
pub enum Literal {
    Boolean(bool),
    String(String),
    // Integers are represented as the widest representation we can.
    // Number formatting vary with language and radix, so we avoid a lot of parsing and
    // formatting duplication by using only signed and unsigned variants.
    UInt(u64, Radix, Type),
    Int(i64, Radix, Type),
    // Pass the string representation through as typed in the UDL.
    // This avoids a lot of uncertainty around precision and accuracy,
    // though bindings for languages less sophisticated number parsing than WebIDL
    // will have to do extra work.
    Float(String, Type),
    Enum(String, Type),
    EmptySequence,
    EmptyMap,
    Null,
}

// Represent the radix of integer literal values.
// We preserve the radix into the generated bindings for readability reasons.
#[derive(Debug, Clone, Copy, Hash)]
pub enum Radix {
    Decimal = 10,
    Octal = 8,
    Hexadecimal = 16,
}

pub(super) fn convert_default_value(
    default_value: &weedle::literal::DefaultValue<'_>,
    type_: &Type,
) -> Result<Literal> {
    fn convert_integer(literal: &weedle::literal::IntegerLit<'_>, type_: &Type) -> Result<Literal> {
        let (string, radix) = match literal {
            weedle::literal::IntegerLit::Dec(v) => (v.0, Radix::Decimal),
            weedle::literal::IntegerLit::Hex(v) => (v.0, Radix::Hexadecimal),
            weedle::literal::IntegerLit::Oct(v) => (v.0, Radix::Octal),
        };
        // This is the radix of the parsed number, passed to `from_str_radix`.
        let src_radix = radix as u32;
        // This radix tells the backends how to represent the number in the output languages.
        let dest_radix = if string == "0" || string.starts_with('-') {
            // 1. weedle parses "0" as an octal literal, but we most likely want to treat this as a decimal.
            // 2. Explicitly negatively signed hex numbers won't convert via i64 very well if they're not 64 bit.
            //    For ease of implementation, output will use decimal.
            Radix::Decimal
        } else {
            radix
        };

        // Clippy seems to think we should be using `strip_prefix` here, but
        // it seems confused as to what this is actually doing.
        #[allow(clippy::manual_strip)]
        let string = if string.starts_with('-') {
            ("-".to_string() + string[1..].trim_start_matches("0x")).to_lowercase()
        } else {
            string.trim_start_matches("0x").to_lowercase()
        };

        Ok(match type_ {
            Type::Int8 | Type::Int16 | Type::Int32 | Type::Int64 => Literal::Int(
                i64::from_str_radix(&string, src_radix)?,
                dest_radix,
                type_.clone(),
            ),
            Type::UInt8 | Type::UInt16 | Type::UInt32 | Type::UInt64 => Literal::UInt(
                u64::from_str_radix(&string, src_radix)?,
                dest_radix,
                type_.clone(),
            ),

            _ => bail!("Cannot coerce literal {} into a non-integer type", string),
        })
    }

    fn convert_float(literal: &weedle::literal::FloatLit<'_>, type_: &Type) -> Result<Literal> {
        let string = match literal {
            weedle::literal::FloatLit::Value(v) => v.0,

            _ => bail!("Infinity and NaN is not currently supported"),
        };

        Ok(match type_ {
            Type::Float32 | Type::Float64 => Literal::Float(string.to_string(), type_.clone()),
            _ => bail!("Cannot coerce literal {} into a non-float type", string),
        })
    }

    Ok(match (default_value, type_) {
        (weedle::literal::DefaultValue::Boolean(b), Type::Boolean) => Literal::Boolean(b.0),
        (weedle::literal::DefaultValue::String(s), Type::String) => {
            // Note that weedle doesn't parse escaped double quotes.
            // Keeping backends using double quotes (possible for all to date)
            // means we don't need to escape single quotes. But we haven't spent a lot of time
            // trying to break default values with weird escapes and quotes.
            Literal::String(s.0.to_string())
        }
        (weedle::literal::DefaultValue::EmptyArray(_), Type::Sequence(_)) => Literal::EmptySequence,
        (weedle::literal::DefaultValue::String(s), Type::Enum(_)) => {
            Literal::Enum(s.0.to_string(), type_.clone())
        }
        (weedle::literal::DefaultValue::Null(_), Type::Optional(_)) => Literal::Null,
        (_, Type::Optional(inner_type)) => convert_default_value(default_value, inner_type)?,

        // We'll ensure the type safety in the convert_* number methods.
        (weedle::literal::DefaultValue::Integer(i), _) => convert_integer(i, type_)?,
        (weedle::literal::DefaultValue::Float(i), _) => convert_float(i, type_)?,

        _ => bail!("No support for {:?} literal yet", default_value),
    })
}

#[cfg(test)]
mod test {
    use super::*;
    use weedle::Parse;

    fn parse_and_convert(expr: &str, t: Type) -> Result<Literal> {
        let (_, node) = weedle::literal::DefaultValue::parse(expr).unwrap();
        convert_default_value(&node, &t)
    }

    #[test]
    fn test_default_value_conversion() -> Result<()> {
        assert!(matches!(
            parse_and_convert("0", Type::UInt8)?,
            Literal::UInt(0, Radix::Decimal, Type::UInt8)
        ));
        assert!(matches!(
            parse_and_convert("-12", Type::Int32)?,
            Literal::Int(-12, Radix::Decimal, Type::Int32)
        ));
        assert!(
            matches!(parse_and_convert("3.14", Type::Float32)?, Literal::Float(v, Type::Float32) if v == "3.14")
        );
        assert!(matches!(
            parse_and_convert("false", Type::Boolean)?,
            Literal::Boolean(false)
        ));
        assert!(
            matches!(parse_and_convert("\"TEST\"", Type::String)?, Literal::String(v) if v == "TEST")
        );
        assert!(
            matches!(parse_and_convert("\"one\"", Type::Enum("E".into()))?, Literal::Enum(v, Type::Enum(e)) if v == "one" && e == "E")
        );
        assert!(matches!(
            parse_and_convert("[]", Type::Sequence(Box::new(Type::String)))?,
            Literal::EmptySequence
        ));
        assert!(matches!(
            parse_and_convert("null", Type::Optional(Box::new(Type::String)))?,
            Literal::Null
        ));
        Ok(())
    }
    #[test]
    fn test_error_on_type_mismatch() {
        assert_eq!(
            parse_and_convert("0", Type::Boolean)
                .unwrap_err()
                .to_string(),
            "Cannot coerce literal 0 into a non-integer type"
        );
        assert!(parse_and_convert("{}", Type::Boolean)
            .unwrap_err()
            .to_string()
            .starts_with("No support for"));
    }
}
