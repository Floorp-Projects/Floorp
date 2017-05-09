//! Intermediate representation of variables.

use super::context::{BindgenContext, ItemId};
use super::dot::DotAttributes;
use super::function::cursor_mangling;
use super::int::IntKind;
use super::item::Item;
use super::ty::{FloatKind, TypeKind};
use cexpr;
use clang;
use parse::{ClangItemParser, ClangSubItemParser, ParseError, ParseResult};
use std::io;
use std::num::Wrapping;

/// The type for a constant variable.
#[derive(Debug)]
pub enum VarType {
    /// A boolean.
    Bool(bool),
    /// An integer.
    Int(i64),
    /// A floating point number.
    Float(f64),
    /// A character.
    Char(u8),
    /// A string, not necessarily well-formed utf-8.
    String(Vec<u8>),
}

/// A `Var` is our intermediate representation of a variable.
#[derive(Debug)]
pub struct Var {
    /// The name of the variable.
    name: String,
    /// The mangled name of the variable.
    mangled_name: Option<String>,
    /// The type of the variable.
    ty: ItemId,
    /// The value of the variable, that needs to be suitable for `ty`.
    val: Option<VarType>,
    /// Whether this variable is const.
    is_const: bool,
}

impl Var {
    /// Construct a new `Var`.
    pub fn new(name: String,
               mangled: Option<String>,
               ty: ItemId,
               val: Option<VarType>,
               is_const: bool)
               -> Var {
        assert!(!name.is_empty());
        Var {
            name: name,
            mangled_name: mangled,
            ty: ty,
            val: val,
            is_const: is_const,
        }
    }

    /// Is this variable `const` qualified?
    pub fn is_const(&self) -> bool {
        self.is_const
    }

    /// The value of this constant variable, if any.
    pub fn val(&self) -> Option<&VarType> {
        self.val.as_ref()
    }

    /// Get this variable's type.
    pub fn ty(&self) -> ItemId {
        self.ty
    }

    /// Get this variable's name.
    pub fn name(&self) -> &str {
        &self.name
    }

    /// Get this variable's mangled name.
    pub fn mangled_name(&self) -> Option<&str> {
        self.mangled_name.as_ref().map(|n| &**n)
    }
}

impl DotAttributes for Var {
    fn dot_attributes<W>(&self,
                         _ctx: &BindgenContext,
                         out: &mut W)
                         -> io::Result<()>
        where W: io::Write,
    {
        if self.is_const {
            try!(writeln!(out, "<tr><td>const</td><td>true</td></tr>"));
        }

        if let Some(ref mangled) = self.mangled_name {
            try!(writeln!(out,
                          "<tr><td>mangled name</td><td>{}</td></tr>",
                          mangled));
        }

        Ok(())
    }
}

impl ClangSubItemParser for Var {
    fn parse(cursor: clang::Cursor,
             ctx: &mut BindgenContext)
             -> Result<ParseResult<Self>, ParseError> {
        use clang_sys::*;
        use cexpr::expr::EvalResult;
        use cexpr::literal::CChar;
        match cursor.kind() {
            CXCursor_MacroDefinition => {

                if let Some(visitor) = ctx.parse_callbacks() {
                    visitor.parsed_macro(&cursor.spelling());
                }

                let value = parse_macro(ctx, &cursor, ctx.translation_unit());

                let (id, value) = match value {
                    Some(v) => v,
                    None => return Err(ParseError::Continue),
                };

                assert!(!id.is_empty(), "Empty macro name?");

                let previously_defined = ctx.parsed_macro(&id);

                // NB: It's important to "note" the macro even if the result is
                // not an integer, otherwise we might loose other kind of
                // derived macros.
                ctx.note_parsed_macro(id.clone(), value.clone());

                if previously_defined {
                    let name = String::from_utf8(id).unwrap();
                    warn!("Duplicated macro definition: {}", name);
                    return Err(ParseError::Continue);
                }

                // NOTE: Unwrapping, here and above, is safe, because the
                // identifier of a token comes straight from clang, and we
                // enforce utf8 there, so we should have already panicked at
                // this point.
                let name = String::from_utf8(id).unwrap();
                let (type_kind, val) = match value {
                    EvalResult::Invalid => return Err(ParseError::Continue),
                    EvalResult::Float(f) => {
                        (TypeKind::Float(FloatKind::Double), VarType::Float(f))
                    }
                    EvalResult::Char(c) => {
                        let c = match c {
                            CChar::Char(c) => {
                                assert_eq!(c.len_utf8(), 1);
                                c as u8
                            }
                            CChar::Raw(c) => {
                                assert!(c <= ::std::u8::MAX as u64);
                                c as u8
                            }
                        };

                        (TypeKind::Int(IntKind::U8), VarType::Char(c))
                    }
                    EvalResult::Str(val) => {
                        let char_ty =
                            Item::builtin_type(TypeKind::Int(IntKind::U8),
                                               true,
                                               ctx);
                        (TypeKind::Pointer(char_ty), VarType::String(val))
                    }
                    EvalResult::Int(Wrapping(value)) => {
                        let kind = ctx.parse_callbacks()
                            .and_then(|c| c.int_macro(&name, value))
                            .unwrap_or_else(|| if value < 0 {
                                if value < i32::min_value() as i64 {
                                    IntKind::LongLong
                                } else {
                                    IntKind::Int
                                }
                            } else if value > u32::max_value() as i64 {
                                IntKind::ULongLong
                            } else {
                                IntKind::UInt
                            });

                        (TypeKind::Int(kind), VarType::Int(value))
                    }
                };

                let ty = Item::builtin_type(type_kind, true, ctx);

                Ok(ParseResult::New(Var::new(name, None, ty, Some(val), true),
                                    Some(cursor)))
            }
            CXCursor_VarDecl => {
                let name = cursor.spelling();
                if name.is_empty() {
                    warn!("Empty constant name?");
                    return Err(ParseError::Continue);
                }

                let ty = cursor.cur_type();

                // XXX this is redundant, remove!
                let is_const = ty.is_const();

                let ty = match Item::from_ty(&ty, cursor, None, ctx) {
                    Ok(ty) => ty,
                    Err(e) => {
                        assert_eq!(ty.kind(),
                                   CXType_Auto,
                                   "Couldn't resolve constant type, and it \
                                   wasn't an nondeductible auto type!");
                        return Err(e);
                    }
                };

                // Note: Ty might not be totally resolved yet, see
                // tests/headers/inner_const.hpp
                //
                // That's fine because in that case we know it's not a literal.
                let canonical_ty = ctx.safe_resolve_type(ty)
                    .and_then(|t| t.safe_canonical_type(ctx));

                let is_integer = canonical_ty.map_or(false, |t| t.is_integer());
                let is_float = canonical_ty.map_or(false, |t| t.is_float());

                // TODO: We could handle `char` more gracefully.
                // TODO: Strings, though the lookup is a bit more hard (we need
                // to look at the canonical type of the pointee too, and check
                // is char, u8, or i8 I guess).
                let value = if is_integer {
                    let kind = match *canonical_ty.unwrap().kind() {
                        TypeKind::Int(kind) => kind,
                        _ => unreachable!(),
                    };

                    let mut val = cursor.evaluate()
                        .and_then(|v| v.as_int())
                        .map(|val| val as i64);
                    if val.is_none() || !kind.signedness_matches(val.unwrap()) {
                        let tu = ctx.translation_unit();
                        val = get_integer_literal_from_cursor(&cursor, tu);
                    }

                    val.map(|val| if kind == IntKind::Bool {
                        VarType::Bool(val != 0)
                    } else {
                        VarType::Int(val)
                    })
                } else if is_float {
                    cursor.evaluate()
                        .and_then(|v| v.as_double())
                        .map(VarType::Float)
                } else {
                    cursor.evaluate()
                        .and_then(|v| v.as_literal_string())
                        .map(VarType::String)
                };

                let mangling = cursor_mangling(ctx, &cursor);
                let var = Var::new(name, mangling, ty, value, is_const);

                Ok(ParseResult::New(var, Some(cursor)))
            }
            _ => {
                /* TODO */
                Err(ParseError::Continue)
            }
        }
    }
}

/// Try and parse a macro using all the macros parsed until now.
fn parse_macro(ctx: &BindgenContext,
               cursor: &clang::Cursor,
               unit: &clang::TranslationUnit)
               -> Option<(Vec<u8>, cexpr::expr::EvalResult)> {
    use cexpr::{expr, nom};

    let cexpr_tokens = match unit.cexpr_tokens(cursor) {
        None => return None,
        Some(tokens) => tokens,
    };

    let parser = expr::IdentifierParser::new(ctx.parsed_macros());
    let result = parser.macro_definition(&cexpr_tokens);

    match result {
        nom::IResult::Done(_, (id, val)) => Some((id.into(), val)),
        _ => None,
    }
}

fn parse_int_literal_tokens(cursor: &clang::Cursor,
                            unit: &clang::TranslationUnit)
                            -> Option<i64> {
    use cexpr::{expr, nom};
    use cexpr::expr::EvalResult;

    let cexpr_tokens = match unit.cexpr_tokens(cursor) {
        None => return None,
        Some(tokens) => tokens,
    };

    // TODO(emilio): We can try to parse other kinds of literals.
    match expr::expr(&cexpr_tokens) {
        nom::IResult::Done(_, EvalResult::Int(Wrapping(val))) => Some(val),
        _ => None,
    }
}

fn get_integer_literal_from_cursor(cursor: &clang::Cursor,
                                   unit: &clang::TranslationUnit)
                                   -> Option<i64> {
    use clang_sys::*;
    let mut value = None;
    cursor.visit(|c| {
        match c.kind() {
            CXCursor_IntegerLiteral |
            CXCursor_UnaryOperator => {
                value = parse_int_literal_tokens(&c, unit);
            }
            CXCursor_UnexposedExpr => {
                value = get_integer_literal_from_cursor(&c, unit);
            }
            _ => (),
        }
        if value.is_some() {
            CXChildVisit_Break
        } else {
            CXChildVisit_Continue
        }
    });
    value
}
