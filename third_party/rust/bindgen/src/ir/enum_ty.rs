//! Intermediate representation for C/C++ enumerations.

use super::super::codegen::EnumVariation;
use super::context::{BindgenContext, TypeId};
use super::item::Item;
use super::ty::{Type, TypeKind};
use crate::clang;
use crate::ir::annotations::Annotations;
use crate::parse::{ClangItemParser, ParseError};
use crate::regex_set::RegexSet;

/// An enum representing custom handling that can be given to a variant.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum EnumVariantCustomBehavior {
    /// This variant will be a module containing constants.
    ModuleConstify,
    /// This variant will be constified, that is, forced to generate a constant.
    Constify,
    /// This variant will be hidden entirely from the resulting enum.
    Hide,
}

/// A C/C++ enumeration.
#[derive(Debug)]
pub struct Enum {
    /// The representation used for this enum; it should be an `IntKind` type or
    /// an alias to one.
    ///
    /// It's `None` if the enum is a forward declaration and isn't defined
    /// anywhere else, see `tests/headers/func_ptr_in_struct.h`.
    repr: Option<TypeId>,

    /// The different variants, with explicit values.
    variants: Vec<EnumVariant>,
}

impl Enum {
    /// Construct a new `Enum` with the given representation and variants.
    pub fn new(repr: Option<TypeId>, variants: Vec<EnumVariant>) -> Self {
        Enum { repr, variants }
    }

    /// Get this enumeration's representation.
    pub fn repr(&self) -> Option<TypeId> {
        self.repr
    }

    /// Get this enumeration's variants.
    pub fn variants(&self) -> &[EnumVariant] {
        &self.variants
    }

    /// Construct an enumeration from the given Clang type.
    pub fn from_ty(
        ty: &clang::Type,
        ctx: &mut BindgenContext,
    ) -> Result<Self, ParseError> {
        use clang_sys::*;
        debug!("Enum::from_ty {:?}", ty);

        if ty.kind() != CXType_Enum {
            return Err(ParseError::Continue);
        }

        let declaration = ty.declaration().canonical();
        let repr = declaration
            .enum_type()
            .and_then(|et| Item::from_ty(&et, declaration, None, ctx).ok());
        let mut variants = vec![];

        let variant_ty =
            repr.and_then(|r| ctx.resolve_type(r).safe_canonical_type(ctx));
        let is_bool = variant_ty.map_or(false, Type::is_bool);

        // Assume signedness since the default type by the C standard is an int.
        let is_signed = variant_ty.map_or(true, |ty| match *ty.kind() {
            TypeKind::Int(ref int_kind) => int_kind.is_signed(),
            ref other => {
                panic!("Since when enums can be non-integers? {:?}", other)
            }
        });

        let type_name = ty.spelling();
        let type_name = if type_name.is_empty() {
            None
        } else {
            Some(type_name)
        };
        let type_name = type_name.as_ref().map(String::as_str);

        let definition = declaration.definition().unwrap_or(declaration);
        definition.visit(|cursor| {
            if cursor.kind() == CXCursor_EnumConstantDecl {
                let value = if is_bool {
                    cursor.enum_val_boolean().map(EnumVariantValue::Boolean)
                } else if is_signed {
                    cursor.enum_val_signed().map(EnumVariantValue::Signed)
                } else {
                    cursor.enum_val_unsigned().map(EnumVariantValue::Unsigned)
                };
                if let Some(val) = value {
                    let name = cursor.spelling();
                    let annotations = Annotations::new(&cursor);
                    let custom_behavior = ctx
                        .parse_callbacks()
                        .and_then(|callbacks| {
                            callbacks
                                .enum_variant_behavior(type_name, &name, val)
                        })
                        .or_else(|| {
                            let annotations = annotations.as_ref()?;
                            if annotations.hide() {
                                Some(EnumVariantCustomBehavior::Hide)
                            } else if annotations.constify_enum_variant() {
                                Some(EnumVariantCustomBehavior::Constify)
                            } else {
                                None
                            }
                        });

                    let name = ctx
                        .parse_callbacks()
                        .and_then(|callbacks| {
                            callbacks.enum_variant_name(type_name, &name, val)
                        })
                        .or_else(|| {
                            annotations
                                .as_ref()?
                                .use_instead_of()?
                                .last()
                                .cloned()
                        })
                        .unwrap_or(name);

                    let comment = cursor.raw_comment();
                    variants.push(EnumVariant::new(
                        name,
                        comment,
                        val,
                        custom_behavior,
                    ));
                }
            }
            CXChildVisit_Continue
        });
        Ok(Enum::new(repr, variants))
    }

    fn is_matching_enum(
        &self,
        ctx: &BindgenContext,
        enums: &RegexSet,
        item: &Item,
    ) -> bool {
        let path = item.path_for_whitelisting(ctx);
        let enum_ty = item.expect_type();

        if enums.matches(&path[1..].join("::")) {
            return true;
        }

        // Test the variants if the enum is anonymous.
        if enum_ty.name().is_some() {
            return false;
        }

        self.variants().iter().any(|v| enums.matches(&v.name()))
    }

    /// Returns the final representation of the enum.
    pub fn computed_enum_variation(
        &self,
        ctx: &BindgenContext,
        item: &Item,
    ) -> EnumVariation {
        // ModuleConsts has higher precedence before Rust in order to avoid
        // problems with overlapping match patterns.
        if self.is_matching_enum(
            ctx,
            &ctx.options().constified_enum_modules,
            item,
        ) {
            EnumVariation::ModuleConsts
        } else if self.is_matching_enum(
            ctx,
            &ctx.options().bitfield_enums,
            item,
        ) {
            EnumVariation::NewType { is_bitfield: true }
        } else if self.is_matching_enum(ctx, &ctx.options().newtype_enums, item)
        {
            EnumVariation::NewType { is_bitfield: false }
        } else if self.is_matching_enum(
            ctx,
            &ctx.options().rustified_enums,
            item,
        ) {
            EnumVariation::Rust {
                non_exhaustive: false,
            }
        } else if self.is_matching_enum(
            ctx,
            &ctx.options().rustified_non_exhaustive_enums,
            item,
        ) {
            EnumVariation::Rust {
                non_exhaustive: true,
            }
        } else if self.is_matching_enum(
            ctx,
            &ctx.options().constified_enums,
            item,
        ) {
            EnumVariation::Consts
        } else {
            ctx.options().default_enum_style
        }
    }
}

/// A single enum variant, to be contained only in an enum.
#[derive(Debug)]
pub struct EnumVariant {
    /// The name of the variant.
    name: String,

    /// An optional doc comment.
    comment: Option<String>,

    /// The integer value of the variant.
    val: EnumVariantValue,

    /// The custom behavior this variant may have, if any.
    custom_behavior: Option<EnumVariantCustomBehavior>,
}

/// A constant value assigned to an enumeration variant.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum EnumVariantValue {
    /// A boolean constant.
    Boolean(bool),

    /// A signed constant.
    Signed(i64),

    /// An unsigned constant.
    Unsigned(u64),
}

impl EnumVariant {
    /// Construct a new enumeration variant from the given parts.
    pub fn new(
        name: String,
        comment: Option<String>,
        val: EnumVariantValue,
        custom_behavior: Option<EnumVariantCustomBehavior>,
    ) -> Self {
        EnumVariant {
            name,
            comment,
            val,
            custom_behavior,
        }
    }

    /// Get this variant's name.
    pub fn name(&self) -> &str {
        &self.name
    }

    /// Get this variant's value.
    pub fn val(&self) -> EnumVariantValue {
        self.val
    }

    /// Get this variant's documentation.
    pub fn comment(&self) -> Option<&str> {
        self.comment.as_ref().map(|s| &**s)
    }

    /// Returns whether this variant should be enforced to be a constant by code
    /// generation.
    pub fn force_constification(&self) -> bool {
        self.custom_behavior
            .map_or(false, |b| b == EnumVariantCustomBehavior::Constify)
    }

    /// Returns whether the current variant should be hidden completely from the
    /// resulting rust enum.
    pub fn hidden(&self) -> bool {
        self.custom_behavior
            .map_or(false, |b| b == EnumVariantCustomBehavior::Hide)
    }
}
