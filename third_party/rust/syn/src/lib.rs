#![cfg_attr(feature = "clippy", feature(plugin))]
#![cfg_attr(feature = "clippy", plugin(clippy))]

#[cfg(feature = "printing")]
extern crate quote;

#[cfg(feature = "parsing")]
extern crate unicode_xid;

#[cfg(feature = "parsing")]
#[macro_use]
mod nom;

#[cfg(feature = "parsing")]
#[macro_use]
mod helper;

#[cfg(feature = "parsing")]
mod escape;

mod attr;
pub use attr::{Attribute, AttrStyle, MetaItem};

mod constant;
pub use constant::ConstExpr;

mod data;
pub use data::{Field, Variant, VariantData, Visibility};

#[cfg(feature = "full")]
mod expr;
#[cfg(feature = "full")]
pub use expr::{Arm, BindingMode, Block, BlockCheckMode, CaptureBy, Expr, FieldPat, Local,
               MacStmtStyle, Pat, RangeLimits, Stmt};

mod generics;
pub use generics::{Generics, Lifetime, LifetimeDef, TraitBoundModifier, TyParam, TyParamBound,
                   WhereBoundPredicate, WhereClause, WherePredicate, WhereRegionPredicate};

#[cfg(feature = "full")]
mod krate;
#[cfg(feature = "full")]
pub use krate::Crate;

mod ident;
pub use ident::Ident;

#[cfg(feature = "full")]
mod item;
#[cfg(feature = "full")]
pub use item::{Abi, Constness, Defaultness, FnArg, FnDecl, ForeignItemKind, ForeignItem,
               ForeignMod, ImplItem, ImplItemKind, ImplPolarity, Item, ItemKind, MethodSig,
               PathListItem, TraitItem, TraitItemKind, Unsafety, ViewPath};

mod lit;
pub use lit::{FloatTy, IntTy, Lit, StrStyle};

#[cfg(feature = "full")]
mod mac;
#[cfg(feature = "full")]
pub use mac::{BinOpToken, DelimToken, Delimited, Mac, Token, TokenTree};

mod macro_input;
pub use macro_input::{Body, MacroInput};

mod op;
pub use op::{BinOp, UnOp};

#[cfg(feature = "parsing")]
mod space;

mod ty;
pub use ty::{AngleBracketedParameterData, BareFnArg, BareFnTy, FunctionRetTy, MutTy, Mutability,
             ParenthesizedParameterData, Path, PathParameters, PathSegment, PolyTraitRef, QSelf,
             Ty, TypeBinding};

#[cfg(feature = "aster")]
pub mod aster;

#[cfg(feature = "visit")]
pub mod visit;

#[cfg(feature = "parsing")]
pub use parsing::*;

#[cfg(feature = "parsing")]
mod parsing {
    use super::*;
    use {generics, macro_input, ty};
    use nom::IResult;

    #[cfg(feature = "full")]
    use {expr, item, krate, mac};

    pub fn parse_macro_input(input: &str) -> Result<MacroInput, String> {
        unwrap("macro input", macro_input::parsing::macro_input, input)
    }

    #[cfg(feature = "full")]
    pub fn parse_crate(input: &str) -> Result<Crate, String> {
        unwrap("crate", krate::parsing::krate, input)
    }

    #[cfg(feature = "full")]
    pub fn parse_item(input: &str) -> Result<Item, String> {
        unwrap("item", item::parsing::item, input)
    }

    #[cfg(feature = "full")]
    pub fn parse_expr(input: &str) -> Result<Expr, String> {
        unwrap("expression", expr::parsing::expr, input)
    }

    pub fn parse_type(input: &str) -> Result<Ty, String> {
        unwrap("type", ty::parsing::ty, input)
    }

    pub fn parse_path(input: &str) -> Result<Path, String> {
        unwrap("path", ty::parsing::path, input)
    }

    pub fn parse_where_clause(input: &str) -> Result<WhereClause, String> {
        unwrap("where clause", generics::parsing::where_clause, input)
    }

    #[cfg(feature = "full")]
    pub fn parse_token_trees(input: &str) -> Result<Vec<TokenTree>, String> {
        unwrap("token trees", mac::parsing::token_trees, input)
    }

    fn unwrap<T>(name: &'static str,
                 f: fn(&str) -> IResult<&str, T>,
                 input: &str)
                 -> Result<T, String> {
        match f(input) {
            IResult::Done(rest, t) => {
                if rest.is_empty() {
                    Ok(t)
                } else if rest.len() == input.len() {
                    // parsed nothing
                    Err(format!("failed to parse {}: {:?}", name, rest))
                } else {
                    Err(format!("failed to parse tokens after {}: {:?}", name, rest))
                }
            }
            IResult::Error => Err(format!("failed to parse {}: {:?}", name, input)),
        }
    }
}
