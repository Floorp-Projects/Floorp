use std::io::Write;

use crate::callbacks::IntKind;

use crate::ir::comp::CompKind;
use crate::ir::context::{BindgenContext, TypeId};
use crate::ir::function::{Function, FunctionKind};
use crate::ir::item::Item;
use crate::ir::item::ItemCanonicalName;
use crate::ir::item_kind::ItemKind;
use crate::ir::ty::{FloatKind, Type, TypeKind};

use super::CodegenError;

fn get_loc(item: &Item) -> String {
    item.location()
        .map(|x| x.to_string())
        .unwrap_or_else(|| "unknown".to_owned())
}

pub(crate) trait CSerialize<'a> {
    type Extra;

    fn serialize<W: Write>(
        &self,
        ctx: &BindgenContext,
        extra: Self::Extra,
        stack: &mut Vec<String>,
        writer: &mut W,
    ) -> Result<(), CodegenError>;
}

impl<'a> CSerialize<'a> for Item {
    type Extra = ();

    fn serialize<W: Write>(
        &self,
        ctx: &BindgenContext,
        (): Self::Extra,
        stack: &mut Vec<String>,
        writer: &mut W,
    ) -> Result<(), CodegenError> {
        match self.kind() {
            ItemKind::Function(func) => {
                func.serialize(ctx, self, stack, writer)
            }
            kind => {
                return Err(CodegenError::Serialize {
                    msg: format!("Cannot serialize item kind {:?}", kind),
                    loc: get_loc(self),
                });
            }
        }
    }
}

impl<'a> CSerialize<'a> for Function {
    type Extra = &'a Item;

    fn serialize<W: Write>(
        &self,
        ctx: &BindgenContext,
        item: Self::Extra,
        stack: &mut Vec<String>,
        writer: &mut W,
    ) -> Result<(), CodegenError> {
        if self.kind() != FunctionKind::Function {
            return Err(CodegenError::Serialize {
                msg: format!(
                    "Cannot serialize function kind {:?}",
                    self.kind(),
                ),
                loc: get_loc(item),
            });
        }

        let signature = match ctx.resolve_type(self.signature()).kind() {
            TypeKind::Function(signature) => signature,
            _ => unreachable!(),
        };

        let name = self.name();

        // Function argoments stored as `(name, type_id)` tuples.
        let args = {
            let mut count = 0;

            signature
                .argument_types()
                .iter()
                .cloned()
                .map(|(opt_name, type_id)| {
                    (
                        opt_name.unwrap_or_else(|| {
                            let name = format!("arg_{}", count);
                            count += 1;
                            name
                        }),
                        type_id,
                    )
                })
                .collect::<Vec<_>>()
        };

        // The name used for the wrapper self.
        let wrap_name = format!("{}{}", name, ctx.wrap_static_fns_suffix());
        // The function's return type
        let ret_ty = signature.return_type();

        // Write `ret_ty wrap_name(args) asm("wrap_name");`
        ret_ty.serialize(ctx, (), stack, writer)?;
        write!(writer, " {}(", wrap_name)?;
        if args.is_empty() {
            write!(writer, "void")?;
        } else {
            serialize_sep(
                ", ",
                args.iter(),
                ctx,
                writer,
                |(name, type_id), ctx, buf| {
                    type_id.serialize(ctx, (), &mut vec![name.clone()], buf)
                },
            )?;
        }
        writeln!(writer, ") asm(\"{}\");", wrap_name)?;

        // Write `ret_ty wrap_name(args) { return name(arg_names)' }`
        ret_ty.serialize(ctx, (), stack, writer)?;
        write!(writer, " {}(", wrap_name)?;
        serialize_sep(
            ", ",
            args.iter(),
            ctx,
            writer,
            |(name, type_id), _, buf| {
                type_id.serialize(ctx, (), &mut vec![name.clone()], buf)
            },
        )?;
        write!(writer, ") {{ return {}(", name)?;
        serialize_sep(", ", args.iter(), ctx, writer, |(name, _), _, buf| {
            write!(buf, "{}", name).map_err(From::from)
        })?;
        writeln!(writer, "); }}")?;

        Ok(())
    }
}

impl<'a> CSerialize<'a> for TypeId {
    type Extra = ();

    fn serialize<W: Write>(
        &self,
        ctx: &BindgenContext,
        (): Self::Extra,
        stack: &mut Vec<String>,
        writer: &mut W,
    ) -> Result<(), CodegenError> {
        let item = ctx.resolve_item(*self);
        item.expect_type().serialize(ctx, item, stack, writer)
    }
}

impl<'a> CSerialize<'a> for Type {
    type Extra = &'a Item;

    fn serialize<W: Write>(
        &self,
        ctx: &BindgenContext,
        item: Self::Extra,
        stack: &mut Vec<String>,
        writer: &mut W,
    ) -> Result<(), CodegenError> {
        match self.kind() {
            TypeKind::Void => {
                if self.is_const() {
                    write!(writer, "const ")?;
                }
                write!(writer, "void")?
            }
            TypeKind::NullPtr => {
                if self.is_const() {
                    write!(writer, "const ")?;
                }
                write!(writer, "nullptr_t")?
            }
            TypeKind::Int(int_kind) => {
                if self.is_const() {
                    write!(writer, "const ")?;
                }
                match int_kind {
                    IntKind::Bool => write!(writer, "bool")?,
                    IntKind::SChar => write!(writer, "signed char")?,
                    IntKind::UChar => write!(writer, "unsigned char")?,
                    IntKind::WChar => write!(writer, "wchar_t")?,
                    IntKind::Short => write!(writer, "short")?,
                    IntKind::UShort => write!(writer, "unsigned short")?,
                    IntKind::Int => write!(writer, "int")?,
                    IntKind::UInt => write!(writer, "unsigned int")?,
                    IntKind::Long => write!(writer, "long")?,
                    IntKind::ULong => write!(writer, "unsigned long")?,
                    IntKind::LongLong => write!(writer, "long long")?,
                    IntKind::ULongLong => write!(writer, "unsigned long long")?,
                    IntKind::Char { .. } => write!(writer, "char")?,
                    int_kind => {
                        return Err(CodegenError::Serialize {
                            msg: format!(
                                "Cannot serialize integer kind {:?}",
                                int_kind
                            ),
                            loc: get_loc(item),
                        })
                    }
                }
            }
            TypeKind::Float(float_kind) => {
                if self.is_const() {
                    write!(writer, "const ")?;
                }
                match float_kind {
                    FloatKind::Float => write!(writer, "float")?,
                    FloatKind::Double => write!(writer, "double")?,
                    FloatKind::LongDouble => write!(writer, "long double")?,
                    FloatKind::Float128 => write!(writer, "__float128")?,
                }
            }
            TypeKind::Complex(float_kind) => {
                if self.is_const() {
                    write!(writer, "const ")?;
                }
                match float_kind {
                    FloatKind::Float => write!(writer, "float complex")?,
                    FloatKind::Double => write!(writer, "double complex")?,
                    FloatKind::LongDouble => {
                        write!(writer, "long double complex")?
                    }
                    FloatKind::Float128 => write!(writer, "__complex128")?,
                }
            }
            TypeKind::Alias(type_id) => {
                if let Some(name) = self.name() {
                    if self.is_const() {
                        write!(writer, "const {}", name)?;
                    } else {
                        write!(writer, "{}", name)?;
                    }
                } else {
                    type_id.serialize(ctx, (), stack, writer)?;
                }
            }
            TypeKind::Array(type_id, length) => {
                type_id.serialize(ctx, (), stack, writer)?;
                write!(writer, " [{}]", length)?
            }
            TypeKind::Function(signature) => {
                if self.is_const() {
                    stack.push("const ".to_string());
                }

                signature.return_type().serialize(
                    ctx,
                    (),
                    &mut vec![],
                    writer,
                )?;

                write!(writer, " (")?;
                while let Some(item) = stack.pop() {
                    write!(writer, "{}", item)?;
                }
                write!(writer, ")")?;

                write!(writer, " (")?;
                serialize_sep(
                    ", ",
                    signature.argument_types().iter(),
                    ctx,
                    writer,
                    |(name, type_id), ctx, buf| {
                        let mut stack = vec![];
                        if let Some(name) = name {
                            stack.push(name.clone());
                        }
                        type_id.serialize(ctx, (), &mut stack, buf)
                    },
                )?;
                write!(writer, ")")?
            }
            TypeKind::ResolvedTypeRef(type_id) => {
                if self.is_const() {
                    write!(writer, "const ")?;
                }
                type_id.serialize(ctx, (), stack, writer)?
            }
            TypeKind::Pointer(type_id) => {
                if self.is_const() {
                    stack.push("*const ".to_owned());
                } else {
                    stack.push("*".to_owned());
                }
                type_id.serialize(ctx, (), stack, writer)?
            }
            TypeKind::Comp(comp_info) => {
                if self.is_const() {
                    write!(writer, "const ")?;
                }

                let name = item.canonical_name(ctx);

                match comp_info.kind() {
                    CompKind::Struct => write!(writer, "struct {}", name)?,
                    CompKind::Union => write!(writer, "union {}", name)?,
                };
            }
            ty => {
                return Err(CodegenError::Serialize {
                    msg: format!("Cannot serialize type kind {:?}", ty),
                    loc: get_loc(item),
                })
            }
        };

        if !stack.is_empty() {
            write!(writer, " ")?;
            while let Some(item) = stack.pop() {
                write!(writer, "{}", item)?;
            }
        }

        Ok(())
    }
}

fn serialize_sep<
    W: Write,
    F: FnMut(I::Item, &BindgenContext, &mut W) -> Result<(), CodegenError>,
    I: Iterator,
>(
    sep: &str,
    mut iter: I,
    ctx: &BindgenContext,
    buf: &mut W,
    mut f: F,
) -> Result<(), CodegenError> {
    if let Some(item) = iter.next() {
        f(item, ctx, buf)?;
        let sep = sep.as_bytes();
        for item in iter {
            buf.write_all(sep)?;
            f(item, ctx, buf)?;
        }
    }

    Ok(())
}
