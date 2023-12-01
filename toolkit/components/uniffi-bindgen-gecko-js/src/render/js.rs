/* This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::shared::*;
use crate::{CallbackIds, Config, FunctionIds, ObjectIds};
use askama::Template;
use extend::ext;
use heck::{ToLowerCamelCase, ToShoutySnakeCase, ToUpperCamelCase};
use uniffi_bindgen::interface::{
    Argument, AsType, CallbackInterface, ComponentInterface, Constructor, Enum, Field, Function,
    Literal, Method, Object, Radix, Record, Type,
};

fn arg_names(args: &[&Argument]) -> String {
    args.iter()
        .map(|arg| {
            if let Some(default_value) = arg.default_value() {
                format!("{} = {}", arg.nm(), default_value.render())
            } else {
                arg.nm()
            }
        })
        .collect::<Vec<String>>()
        .join(",")
}

fn render_enum_literal(typ: &Type, variant_name: &str) -> String {
    if let Type::Enum { name, .. } = typ {
        // TODO: This does not support complex enum literals yet.
        return format!(
            "{}.{}",
            name.to_upper_camel_case(),
            variant_name.to_shouty_snake_case()
        );
    } else {
        panic!("Rendering an enum literal on a type that is not an enum")
    }
}

#[derive(Template)]
#[template(path = "js/wrapper.sys.mjs", escape = "none")]
pub struct JSBindingsTemplate<'a> {
    pub ci: &'a ComponentInterface,
    pub config: &'a Config,
    pub function_ids: &'a FunctionIds<'a>,
    pub object_ids: &'a ObjectIds<'a>,
    pub callback_ids: &'a CallbackIds<'a>,
}

impl<'a> JSBindingsTemplate<'a> {
    pub fn js_module_name(&self) -> String {
        self.js_module_name_for_ci_namespace(self.ci.namespace())
    }

    fn external_type_module(&self, crate_name: &str) -> String {
        format!(
            "resource://gre/modules/{}",
            self.js_module_name_for_crate_name(crate_name),
        )
    }

    // TODO: Once https://phabricator.services.mozilla.com/D156116 is merged maybe the next two
    // functions should use a map from the config file

    fn js_module_name_for_ci_namespace(&self, namespace: &str) -> String {
        // The plain namespace name is a bit too generic as a module name for m-c, so we
        // prefix it with "Rust". Later we'll probably allow this to be customized.
        format!("Rust{}.sys.mjs", namespace.to_upper_camel_case())
    }

    fn js_module_name_for_crate_name(&self, crate_name: &str) -> String {
        let namespace = match crate_name {
            "uniffi_geometry" => "geometry",
            s => s,
        };
        self.js_module_name_for_ci_namespace(namespace)
    }
}

// Define extension traits with methods used in our template code

#[ext(name=LiteralJSExt)]
pub impl Literal {
    fn render(&self) -> String {
        match self {
            Literal::Boolean(inner) => inner.to_string(),
            Literal::String(inner) => format!("\"{}\"", inner),
            Literal::UInt(num, radix, _) => format!("{}", radix.render_num(num)),
            Literal::Int(num, radix, _) => format!("{}", radix.render_num(num)),
            Literal::Float(num, _) => num.clone(),
            Literal::Enum(name, typ) => render_enum_literal(typ, name),
            Literal::EmptyMap => "{}".to_string(),
            Literal::EmptySequence => "[]".to_string(),
            Literal::Null => "null".to_string(),
        }
    }
}

#[ext(name=RadixJSExt)]
pub impl Radix {
    fn render_num(
        &self,
        num: impl std::fmt::Display + std::fmt::LowerHex + std::fmt::Octal,
    ) -> String {
        match self {
            Radix::Decimal => format!("{}", num),
            Radix::Hexadecimal => format!("{:#x}", num),
            Radix::Octal => format!("{:#o}", num),
        }
    }
}

#[ext(name=RecordJSExt)]
pub impl Record {
    fn nm(&self) -> String {
        self.name().to_upper_camel_case()
    }

    fn constructor_field_list(&self) -> String {
        self.fields()
            .iter()
            .map(|field| {
                if let Some(default_value) = field.default_value() {
                    format!("{} = {}", field.nm(), default_value.render())
                } else {
                    field.nm()
                }
            })
            .collect::<Vec<String>>()
            .join(",")
    }
}

#[ext(name=CallbackInterfaceJSExt)]
pub impl CallbackInterface {
    fn nm(&self) -> String {
        self.name().to_upper_camel_case()
    }

    fn handler(&self) -> String {
        format!("callbackHandler{}", self.nm())
    }
}

#[ext(name=FieldJSExt)]
pub impl Field {
    fn nm(&self) -> String {
        self.name().to_lower_camel_case()
    }

    fn lower_fn(&self) -> String {
        self.as_type().lower_fn()
    }

    fn lift_fn(&self) -> String {
        self.as_type().lift_fn()
    }

    fn write_datastream_fn(&self) -> String {
        self.as_type().write_datastream_fn()
    }

    fn read_datastream_fn(&self) -> String {
        self.as_type().read_datastream_fn()
    }

    fn compute_size_fn(&self) -> String {
        self.as_type().compute_size_fn()
    }

    fn ffi_converter(&self) -> String {
        self.as_type().ffi_converter()
    }
}

#[ext(name=ArgumentJSExt)]
pub impl Argument {
    fn nm(&self) -> String {
        self.name().to_lower_camel_case()
    }

    fn lower_fn(&self) -> String {
        self.as_type().lower_fn()
    }

    fn lift_fn(&self) -> String {
        self.as_type().lift_fn()
    }

    fn write_datastream_fn(&self) -> String {
        self.as_type().write_datastream_fn()
    }

    fn read_datastream_fn(&self) -> String {
        self.as_type().read_datastream_fn()
    }

    fn compute_size_fn(&self) -> String {
        self.as_type().compute_size_fn()
    }

    fn ffi_converter(&self) -> String {
        self.as_type().ffi_converter()
    }
}

#[ext(name=TypeJSExt)]
pub impl Type {
    // Render an expression to check if two instances of this type are equal
    fn equals(&self, first: &str, second: &str) -> String {
        match self {
            Type::Record { .. } => format!("{}.equals({})", first, second),
            _ => format!("{} == {}", first, second),
        }
    }

    fn lower_fn(&self) -> String {
        format!("{}.lower", self.ffi_converter())
    }

    fn lift_fn(&self) -> String {
        format!("{}.lift", self.ffi_converter())
    }

    fn write_datastream_fn(&self) -> String {
        format!("{}.write", self.ffi_converter())
    }

    fn read_datastream_fn(&self) -> String {
        format!("{}.read", self.ffi_converter())
    }

    fn compute_size_fn(&self) -> String {
        format!("{}.computeSize", self.ffi_converter())
    }

    fn canonical_name(&self) -> String {
        match self {
            Type::Int8 => "i8".into(),
            Type::UInt8 => "u8".into(),
            Type::Int16 => "i16".into(),
            Type::UInt16 => "u16".into(),
            Type::Int32 => "i32".into(),
            Type::UInt32 => "u32".into(),
            Type::Int64 => "i64".into(),
            Type::UInt64 => "u64".into(),
            Type::Float32 => "f32".into(),
            Type::Float64 => "f64".into(),
            Type::String => "string".into(),
            Type::Bytes => "bytes".into(),
            Type::Boolean => "bool".into(),
            Type::Object { name, .. }
            | Type::Enum { name, .. }
            | Type::Record { name, .. }
            | Type::CallbackInterface { name, .. } => format!("Type{name}"),
            Type::Timestamp => "Timestamp".into(),
            Type::Duration => "Duration".into(),
            Type::ForeignExecutor => "ForeignExecutor".into(),
            Type::Optional { inner_type } => format!("Optional{}", inner_type.canonical_name()),
            Type::Sequence { inner_type } => format!("Sequence{}", inner_type.canonical_name()),
            Type::Map {
                key_type,
                value_type,
            } => format!(
                "Map{}{}",
                key_type.canonical_name().to_upper_camel_case(),
                value_type.canonical_name().to_upper_camel_case()
            ),
            Type::External { name, .. } | Type::Custom { name, .. } => format!("Type{name}"),
        }
    }

    fn ffi_converter(&self) -> String {
        format!(
            "FfiConverter{}",
            self.canonical_name().to_upper_camel_case()
        )
    }
}

#[ext(name=EnumJSExt)]
pub impl Enum {
    fn nm(&self) -> String {
        self.name().to_upper_camel_case()
    }
}

#[ext(name=FunctionJSExt)]
pub impl Function {
    fn arg_names(&self) -> String {
        arg_names(self.arguments().as_slice())
    }

    fn nm(&self) -> String {
        self.name().to_lower_camel_case()
    }
}

#[ext(name=ObjectJSExt)]
pub impl Object {
    fn nm(&self) -> String {
        self.name().to_upper_camel_case()
    }
}

#[ext(name=ConstructorJSExt)]
pub impl Constructor {
    fn nm(&self) -> String {
        if self.is_primary_constructor() {
            "init".to_string()
        } else {
            self.name().to_lower_camel_case()
        }
    }

    fn arg_names(&self) -> String {
        arg_names(&self.arguments().as_slice())
    }
}

#[ext(name=MethodJSExt)]
pub impl Method {
    fn arg_names(&self) -> String {
        arg_names(self.arguments().as_slice())
    }

    fn nm(&self) -> String {
        self.name().to_lower_camel_case()
    }
}
