/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! # Function definitions for a `ComponentInterface`.
//!
//! This module converts function definitions from UDL into structures that
//! can be added to a `ComponentInterface`. A declaration in the UDL like this:
//!
//! ```
//! # let ci = uniffi_bindgen::interface::ComponentInterface::from_webidl(r##"
//! namespace example {
//!     string hello();
//! };
//! # "##)?;
//! # Ok::<(), anyhow::Error>(())
//! ```
//!
//! Will result in a [`Function`] member being added to the resulting [`ComponentInterface`]:
//!
//! ```
//! # use uniffi_bindgen::interface::Type;
//! # let ci = uniffi_bindgen::interface::ComponentInterface::from_webidl(r##"
//! # namespace example {
//! #     string hello();
//! # };
//! # "##)?;
//! let func = ci.get_function_definition("hello").unwrap();
//! assert_eq!(func.name(), "hello");
//! assert!(matches!(func.return_type(), Some(Type::String)));
//! assert_eq!(func.arguments().len(), 0);
//! # Ok::<(), anyhow::Error>(())
//! ```
use std::convert::TryFrom;

use anyhow::{bail, Result};
use uniffi_meta::Checksum;

use super::attributes::{ArgumentAttributes, FunctionAttributes};
use super::ffi::{FfiArgument, FfiFunction, FfiType};
use super::literal::{convert_default_value, Literal};
use super::types::{ObjectImpl, Type, TypeIterator};
use super::{APIConverter, AsType, ComponentInterface};

/// Represents a standalone function.
///
/// Each `Function` corresponds to a standalone function in the rust module,
/// and has a corresponding standalone function in the foreign language bindings.
///
/// In the FFI, this will be a standalone function with appropriately lowered types.
#[derive(Debug, Clone, Checksum)]
pub struct Function {
    pub(super) name: String,
    pub(super) is_async: bool,
    pub(super) arguments: Vec<Argument>,
    pub(super) return_type: Option<Type>,
    // We don't include the FFIFunc in the hash calculation, because:
    //  - it is entirely determined by the other fields,
    //    so excluding it is safe.
    //  - its `name` property includes a checksum derived from the very
    //    hash value we're trying to calculate here, so excluding it
    //    avoids a weird circular dependency in the calculation.
    #[checksum_ignore]
    pub(super) ffi_func: FfiFunction,
    pub(super) throws: Option<Type>,
    pub(super) checksum_fn_name: String,
    // Force a checksum value.  This is used for functions from the proc-macro code, which uses a
    // different checksum method.
    #[checksum_ignore]
    pub(super) checksum_override: Option<u16>,
}

impl Function {
    pub fn name(&self) -> &str {
        &self.name
    }

    pub fn is_async(&self) -> bool {
        self.is_async
    }

    pub fn arguments(&self) -> Vec<&Argument> {
        self.arguments.iter().collect()
    }

    pub fn full_arguments(&self) -> Vec<Argument> {
        self.arguments.to_vec()
    }

    pub fn return_type(&self) -> Option<&Type> {
        self.return_type.as_ref()
    }

    pub fn ffi_func(&self) -> &FfiFunction {
        &self.ffi_func
    }

    pub fn checksum_fn_name(&self) -> &str {
        &self.checksum_fn_name
    }

    pub fn checksum(&self) -> u16 {
        self.checksum_override
            .unwrap_or_else(|| uniffi_meta::checksum(self))
    }

    pub fn throws(&self) -> bool {
        self.throws.is_some()
    }

    pub fn throws_name(&self) -> Option<&str> {
        super::throws_name(&self.throws)
    }

    pub fn throws_type(&self) -> Option<&Type> {
        self.throws.as_ref()
    }

    pub fn derive_ffi_func(&mut self, ci_namespace: &str) -> Result<()> {
        // The name is already set if the function is defined through a proc-macro invocation
        // rather than in UDL. Don't overwrite it in that case.
        if self.ffi_func.name.is_empty() {
            self.ffi_func.name = uniffi_meta::fn_symbol_name(ci_namespace, &self.name);
        }
        self.ffi_func.init(
            self.return_type.as_ref().map(Into::into),
            self.arguments.iter().map(Into::into),
        );
        Ok(())
    }
}

impl From<uniffi_meta::FnParamMetadata> for Argument {
    fn from(meta: uniffi_meta::FnParamMetadata) -> Self {
        Argument {
            name: meta.name,
            type_: meta.ty.into(),
            by_ref: false,
            optional: false,
            default: None,
        }
    }
}

impl From<uniffi_meta::FnMetadata> for Function {
    fn from(meta: uniffi_meta::FnMetadata) -> Self {
        let ffi_name = meta.ffi_symbol_name();
        let checksum_fn_name = meta.checksum_symbol_name();
        let is_async = meta.is_async;
        let return_type = meta.return_type.map(Into::into);
        let arguments = meta.inputs.into_iter().map(Into::into).collect();

        let ffi_func = FfiFunction {
            name: ffi_name,
            is_async,
            ..FfiFunction::default()
        };

        let throws = match meta.throws {
            None => None,
            Some(uniffi_meta::Type::Enum { name, .. }) => Some(Type::Enum(name)),
            _ => panic!("unsupported error type {:?}", meta.throws),
        };

        Self {
            name: meta.name,
            is_async,
            arguments,
            return_type,
            ffi_func,
            throws,
            checksum_fn_name,
            checksum_override: Some(meta.checksum),
        }
    }
}

impl APIConverter<Function> for weedle::namespace::NamespaceMember<'_> {
    fn convert(&self, ci: &mut ComponentInterface) -> Result<Function> {
        match self {
            weedle::namespace::NamespaceMember::Operation(f) => f.convert(ci),
            _ => bail!("no support for namespace member type {:?} yet", self),
        }
    }
}

impl APIConverter<Function> for weedle::namespace::OperationNamespaceMember<'_> {
    fn convert(&self, ci: &mut ComponentInterface) -> Result<Function> {
        let return_type = ci.resolve_return_type_expression(&self.return_type)?;
        let name = match self.identifier {
            None => bail!("anonymous functions are not supported {:?}", self),
            Some(id) => id.0.to_string(),
        };
        let checksum_fn_name = uniffi_meta::fn_checksum_symbol_name(ci.namespace(), &name);
        let attrs = FunctionAttributes::try_from(self.attributes.as_ref())?;
        let throws = match attrs.get_throws_err() {
            None => None,
            Some(name) => match ci.get_type(name) {
                Some(t) => {
                    ci.note_name_used_as_error(name);
                    Some(t)
                }
                None => bail!("unknown type for error: {name}"),
            },
        };
        Ok(Function {
            name,
            is_async: false,
            return_type,
            arguments: self.args.body.list.convert(ci)?,
            ffi_func: Default::default(),
            throws,
            checksum_fn_name,
            checksum_override: None,
        })
    }
}

/// Represents an argument to a function/constructor/method call.
///
/// Each argument has a name and a type, along with some optional metadata.
#[derive(Debug, Clone, Checksum)]
pub struct Argument {
    pub(super) name: String,
    pub(super) type_: Type,
    pub(super) by_ref: bool,
    pub(super) optional: bool,
    pub(super) default: Option<Literal>,
}

impl Argument {
    pub fn name(&self) -> &str {
        &self.name
    }

    pub fn by_ref(&self) -> bool {
        self.by_ref
    }

    pub fn is_trait_ref(&self) -> bool {
        matches!(&self.type_, Type::Object { imp, .. } if *imp == ObjectImpl::Trait)
    }

    pub fn default_value(&self) -> Option<&Literal> {
        self.default.as_ref()
    }

    pub fn iter_types(&self) -> TypeIterator<'_> {
        self.type_.iter_types()
    }
}

impl AsType for Argument {
    fn as_type(&self) -> Type {
        self.type_.clone()
    }
}

impl From<&Argument> for FfiArgument {
    fn from(a: &Argument) -> FfiArgument {
        FfiArgument {
            name: a.name.clone(),
            type_: (&a.type_).into(),
        }
    }
}

impl APIConverter<Argument> for weedle::argument::Argument<'_> {
    fn convert(&self, ci: &mut ComponentInterface) -> Result<Argument> {
        match self {
            weedle::argument::Argument::Single(t) => t.convert(ci),
            weedle::argument::Argument::Variadic(_) => bail!("variadic arguments not supported"),
        }
    }
}

impl APIConverter<Argument> for weedle::argument::SingleArgument<'_> {
    fn convert(&self, ci: &mut ComponentInterface) -> Result<Argument> {
        let type_ = ci.resolve_type_expression(&self.type_)?;
        let default = match self.default {
            None => None,
            Some(v) => Some(convert_default_value(&v.value, &type_)?),
        };
        let by_ref = ArgumentAttributes::try_from(self.attributes.as_ref())?.by_ref();
        Ok(Argument {
            name: self.identifier.0.to_string(),
            type_,
            by_ref,
            optional: self.optional.is_some(),
            default,
        })
    }
}

/// Combines the return and throws type of a function/method
#[derive(Debug, PartialOrd, Ord, PartialEq, Eq)]
pub struct ResultType {
    pub return_type: Option<Type>,
    pub throws_type: Option<Type>,
}

impl ResultType {
    /// Get the `T` parameters for the `FutureCallback<T>` for this ResultType
    pub fn future_callback_param(&self) -> FfiType {
        match &self.return_type {
            Some(t) => t.ffi_type(),
            None => FfiType::UInt8,
        }
    }
}

/// Implemented by function-like types (Function, Method, Constructor)
pub trait Callable {
    fn arguments(&self) -> Vec<&Argument>;
    fn return_type(&self) -> Option<Type>;
    fn throws_type(&self) -> Option<Type>;
    fn result_type(&self) -> ResultType {
        ResultType {
            return_type: self.return_type(),
            throws_type: self.throws_type(),
        }
    }
}

impl Callable for Function {
    fn arguments(&self) -> Vec<&Argument> {
        self.arguments()
    }

    fn return_type(&self) -> Option<Type> {
        self.return_type().cloned()
    }

    fn throws_type(&self) -> Option<Type> {
        self.throws_type().cloned()
    }
}

// Needed because Askama likes to add extra refs to variables
impl<T: Callable> Callable for &T {
    fn arguments(&self) -> Vec<&Argument> {
        (*self).arguments()
    }

    fn return_type(&self) -> Option<Type> {
        (*self).return_type()
    }

    fn throws_type(&self) -> Option<Type> {
        (*self).throws_type()
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_minimal_and_rich_function() -> Result<()> {
        let ci = ComponentInterface::from_webidl(
            r##"
            namespace test {
                void minimal();
                [Throws=TestError]
                sequence<string?> rich(u32 arg1, TestDict arg2);
            };
            [Error]
            enum TestError { "err" };
            dictionary TestDict {
                u32 field;
            };
        "##,
        )?;

        let func1 = ci.get_function_definition("minimal").unwrap();
        assert_eq!(func1.name(), "minimal");
        assert!(func1.return_type().is_none());
        assert!(func1.throws_type().is_none());
        assert_eq!(func1.arguments().len(), 0);

        let func2 = ci.get_function_definition("rich").unwrap();
        assert_eq!(func2.name(), "rich");
        assert_eq!(
            func2.return_type().unwrap().canonical_name(),
            "SequenceOptionalstring"
        );
        assert!(
            matches!(func2.throws_type(), Some(Type::Enum(name)) if name == "TestError" && ci.is_name_used_as_error(name))
        );
        assert_eq!(func2.arguments().len(), 2);
        assert_eq!(func2.arguments()[0].name(), "arg1");
        assert_eq!(func2.arguments()[0].as_type().canonical_name(), "u32");
        assert_eq!(func2.arguments()[1].name(), "arg2");
        assert_eq!(
            func2.arguments()[1].as_type().canonical_name(),
            "TypeTestDict"
        );
        Ok(())
    }
}
