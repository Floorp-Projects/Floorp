// Copyright 2018 Syn Developers
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::*;
use punctuated::Punctuated;
use proc_macro2::{Span, TokenStream};
#[cfg(feature = "extra-traits")]
use std::hash::{Hash, Hasher};
#[cfg(feature = "extra-traits")]
use tt::TokenStreamHelper;
#[cfg(feature = "full")]
use std::mem;

ast_enum_of_structs! {
    /// A Rust expression.
    ///
    /// *This type is available if Syn is built with the `"derive"` or `"full"`
    /// feature.*
    ///
    /// # Syntax tree enums
    ///
    /// This type is a syntax tree enum. In Syn this and other syntax tree enums
    /// are designed to be traversed using the following rebinding idiom.
    ///
    /// ```
    /// # use syn::Expr;
    /// #
    /// # fn example(expr: Expr) {
    /// # const IGNORE: &str = stringify! {
    /// let expr: Expr = /* ... */;
    /// # };
    /// match expr {
    ///     Expr::MethodCall(expr) => {
    ///         /* ... */
    ///     }
    ///     Expr::Cast(expr) => {
    ///         /* ... */
    ///     }
    ///     Expr::IfLet(expr) => {
    ///         /* ... */
    ///     }
    ///     /* ... */
    ///     # _ => {}
    /// }
    /// # }
    /// ```
    ///
    /// We begin with a variable `expr` of type `Expr` that has no fields
    /// (because it is an enum), and by matching on it and rebinding a variable
    /// with the same name `expr` we effectively imbue our variable with all of
    /// the data fields provided by the variant that it turned out to be. So for
    /// example above if we ended up in the `MethodCall` case then we get to use
    /// `expr.receiver`, `expr.args` etc; if we ended up in the `IfLet` case we
    /// get to use `expr.pat`, `expr.then_branch`, `expr.else_branch`.
    ///
    /// The pattern is similar if the input expression is borrowed:
    ///
    /// ```
    /// # use syn::Expr;
    /// #
    /// # fn example(expr: &Expr) {
    /// match *expr {
    ///     Expr::MethodCall(ref expr) => {
    /// #   }
    /// #   _ => {}
    /// # }
    /// # }
    /// ```
    ///
    /// This approach avoids repeating the variant names twice on every line.
    ///
    /// ```
    /// # use syn::{Expr, ExprMethodCall};
    /// #
    /// # fn example(expr: Expr) {
    /// # match expr {
    /// Expr::MethodCall(ExprMethodCall { method, args, .. }) => { // repetitive
    /// # }
    /// # _ => {}
    /// # }
    /// # }
    /// ```
    ///
    /// In general, the name to which a syntax tree enum variant is bound should
    /// be a suitable name for the complete syntax tree enum type.
    ///
    /// ```
    /// # use syn::{Expr, ExprField};
    /// #
    /// # fn example(discriminant: &ExprField) {
    /// // Binding is called `base` which is the name I would use if I were
    /// // assigning `*discriminant.base` without an `if let`.
    /// if let Expr::Tuple(ref base) = *discriminant.base {
    /// # }
    /// # }
    /// ```
    ///
    /// A sign that you may not be choosing the right variable names is if you
    /// see names getting repeated in your code, like accessing
    /// `receiver.receiver` or `pat.pat` or `cond.cond`.
    pub enum Expr {
        /// A box expression: `box f`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Box(ExprBox #full {
            pub attrs: Vec<Attribute>,
            pub box_token: Token![box],
            pub expr: Box<Expr>,
        }),

        /// A placement expression: `place <- value`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub InPlace(ExprInPlace #full {
            pub attrs: Vec<Attribute>,
            pub place: Box<Expr>,
            pub arrow_token: Token![<-],
            pub value: Box<Expr>,
        }),

        /// A slice literal expression: `[a, b, c, d]`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Array(ExprArray #full {
            pub attrs: Vec<Attribute>,
            pub bracket_token: token::Bracket,
            pub elems: Punctuated<Expr, Token![,]>,
        }),

        /// A function call expression: `invoke(a, b)`.
        ///
        /// *This type is available if Syn is built with the `"derive"` or
        /// `"full"` feature.*
        pub Call(ExprCall {
            pub attrs: Vec<Attribute>,
            pub func: Box<Expr>,
            pub paren_token: token::Paren,
            pub args: Punctuated<Expr, Token![,]>,
        }),

        /// A method call expression: `x.foo::<T>(a, b)`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub MethodCall(ExprMethodCall #full {
            pub attrs: Vec<Attribute>,
            pub receiver: Box<Expr>,
            pub dot_token: Token![.],
            pub method: Ident,
            pub turbofish: Option<MethodTurbofish>,
            pub paren_token: token::Paren,
            pub args: Punctuated<Expr, Token![,]>,
        }),

        /// A tuple expression: `(a, b, c, d)`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Tuple(ExprTuple #full {
            pub attrs: Vec<Attribute>,
            pub paren_token: token::Paren,
            pub elems: Punctuated<Expr, Token![,]>,
        }),

        /// A binary operation: `a + b`, `a * b`.
        ///
        /// *This type is available if Syn is built with the `"derive"` or
        /// `"full"` feature.*
        pub Binary(ExprBinary {
            pub attrs: Vec<Attribute>,
            pub left: Box<Expr>,
            pub op: BinOp,
            pub right: Box<Expr>,
        }),

        /// A unary operation: `!x`, `*x`.
        ///
        /// *This type is available if Syn is built with the `"derive"` or
        /// `"full"` feature.*
        pub Unary(ExprUnary {
            pub attrs: Vec<Attribute>,
            pub op: UnOp,
            pub expr: Box<Expr>,
        }),

        /// A literal in place of an expression: `1`, `"foo"`.
        ///
        /// *This type is available if Syn is built with the `"derive"` or
        /// `"full"` feature.*
        pub Lit(ExprLit {
            pub attrs: Vec<Attribute>,
            pub lit: Lit,
        }),

        /// A cast expression: `foo as f64`.
        ///
        /// *This type is available if Syn is built with the `"derive"` or
        /// `"full"` feature.*
        pub Cast(ExprCast {
            pub attrs: Vec<Attribute>,
            pub expr: Box<Expr>,
            pub as_token: Token![as],
            pub ty: Box<Type>,
        }),

        /// A type ascription expression: `foo: f64`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Type(ExprType #full {
            pub attrs: Vec<Attribute>,
            pub expr: Box<Expr>,
            pub colon_token: Token![:],
            pub ty: Box<Type>,
        }),

        /// An `if` expression with an optional `else` block: `if expr { ... }
        /// else { ... }`.
        ///
        /// The `else` branch expression may only be an `If`, `IfLet`, or
        /// `Block` expression, not any of the other types of expression.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub If(ExprIf #full {
            pub attrs: Vec<Attribute>,
            pub if_token: Token![if],
            pub cond: Box<Expr>,
            pub then_branch: Block,
            pub else_branch: Option<(Token![else], Box<Expr>)>,
        }),

        /// An `if let` expression with an optional `else` block: `if let pat =
        /// expr { ... } else { ... }`.
        ///
        /// The `else` branch expression may only be an `If`, `IfLet`, or
        /// `Block` expression, not any of the other types of expression.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub IfLet(ExprIfLet #full {
            pub attrs: Vec<Attribute>,
            pub if_token: Token![if],
            pub let_token: Token![let],
            pub pats: Punctuated<Pat, Token![|]>,
            pub eq_token: Token![=],
            pub expr: Box<Expr>,
            pub then_branch: Block,
            pub else_branch: Option<(Token![else], Box<Expr>)>,
        }),

        /// A while loop: `while expr { ... }`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub While(ExprWhile #full {
            pub attrs: Vec<Attribute>,
            pub label: Option<Label>,
            pub while_token: Token![while],
            pub cond: Box<Expr>,
            pub body: Block,
        }),

        /// A while-let loop: `while let pat = expr { ... }`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub WhileLet(ExprWhileLet #full {
            pub attrs: Vec<Attribute>,
            pub label: Option<Label>,
            pub while_token: Token![while],
            pub let_token: Token![let],
            pub pats: Punctuated<Pat, Token![|]>,
            pub eq_token: Token![=],
            pub expr: Box<Expr>,
            pub body: Block,
        }),

        /// A for loop: `for pat in expr { ... }`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub ForLoop(ExprForLoop #full {
            pub attrs: Vec<Attribute>,
            pub label: Option<Label>,
            pub for_token: Token![for],
            pub pat: Box<Pat>,
            pub in_token: Token![in],
            pub expr: Box<Expr>,
            pub body: Block,
        }),

        /// Conditionless loop: `loop { ... }`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Loop(ExprLoop #full {
            pub attrs: Vec<Attribute>,
            pub label: Option<Label>,
            pub loop_token: Token![loop],
            pub body: Block,
        }),

        /// A `match` expression: `match n { Some(n) => {}, None => {} }`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Match(ExprMatch #full {
            pub attrs: Vec<Attribute>,
            pub match_token: Token![match],
            pub expr: Box<Expr>,
            pub brace_token: token::Brace,
            pub arms: Vec<Arm>,
        }),

        /// A closure expression: `|a, b| a + b`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Closure(ExprClosure #full {
            pub attrs: Vec<Attribute>,
            pub movability: Option<Token![static]>,
            pub capture: Option<Token![move]>,
            pub or1_token: Token![|],
            pub inputs: Punctuated<FnArg, Token![,]>,
            pub or2_token: Token![|],
            pub output: ReturnType,
            pub body: Box<Expr>,
        }),

        /// An unsafe block: `unsafe { ... }`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Unsafe(ExprUnsafe #full {
            pub attrs: Vec<Attribute>,
            pub unsafe_token: Token![unsafe],
            pub block: Block,
        }),

        /// A blocked scope: `{ ... }`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Block(ExprBlock #full {
            pub attrs: Vec<Attribute>,
            pub block: Block,
        }),

        /// An assignment expression: `a = compute()`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Assign(ExprAssign #full {
            pub attrs: Vec<Attribute>,
            pub left: Box<Expr>,
            pub eq_token: Token![=],
            pub right: Box<Expr>,
        }),

        /// A compound assignment expression: `counter += 1`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub AssignOp(ExprAssignOp #full {
            pub attrs: Vec<Attribute>,
            pub left: Box<Expr>,
            pub op: BinOp,
            pub right: Box<Expr>,
        }),

        /// Access of a named struct field (`obj.k`) or unnamed tuple struct
        /// field (`obj.0`).
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Field(ExprField #full {
            pub attrs: Vec<Attribute>,
            pub base: Box<Expr>,
            pub dot_token: Token![.],
            pub member: Member,
        }),

        /// A square bracketed indexing expression: `vector[2]`.
        ///
        /// *This type is available if Syn is built with the `"derive"` or
        /// `"full"` feature.*
        pub Index(ExprIndex {
            pub attrs: Vec<Attribute>,
            pub expr: Box<Expr>,
            pub bracket_token: token::Bracket,
            pub index: Box<Expr>,
        }),

        /// A range expression: `1..2`, `1..`, `..2`, `1..=2`, `..=2`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Range(ExprRange #full {
            pub attrs: Vec<Attribute>,
            pub from: Option<Box<Expr>>,
            pub limits: RangeLimits,
            pub to: Option<Box<Expr>>,
        }),

        /// A path like `std::mem::replace` possibly containing generic
        /// parameters and a qualified self-type.
        ///
        /// A plain identifier like `x` is a path of length 1.
        ///
        /// *This type is available if Syn is built with the `"derive"` or
        /// `"full"` feature.*
        pub Path(ExprPath {
            pub attrs: Vec<Attribute>,
            pub qself: Option<QSelf>,
            pub path: Path,
        }),

        /// A referencing operation: `&a` or `&mut a`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Reference(ExprReference #full {
            pub attrs: Vec<Attribute>,
            pub and_token: Token![&],
            pub mutability: Option<Token![mut]>,
            pub expr: Box<Expr>,
        }),

        /// A `break`, with an optional label to break and an optional
        /// expression.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Break(ExprBreak #full {
            pub attrs: Vec<Attribute>,
            pub break_token: Token![break],
            pub label: Option<Lifetime>,
            pub expr: Option<Box<Expr>>,
        }),

        /// A `continue`, with an optional label.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Continue(ExprContinue #full {
            pub attrs: Vec<Attribute>,
            pub continue_token: Token![continue],
            pub label: Option<Lifetime>,
        }),

        /// A `return`, with an optional value to be returned.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Return(ExprReturn #full {
            pub attrs: Vec<Attribute>,
            pub return_token: Token![return],
            pub expr: Option<Box<Expr>>,
        }),

        /// A macro invocation expression: `format!("{}", q)`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Macro(ExprMacro #full {
            pub attrs: Vec<Attribute>,
            pub mac: Macro,
        }),

        /// A struct literal expression: `Point { x: 1, y: 1 }`.
        ///
        /// The `rest` provides the value of the remaining fields as in `S { a:
        /// 1, b: 1, ..rest }`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Struct(ExprStruct #full {
            pub attrs: Vec<Attribute>,
            pub path: Path,
            pub brace_token: token::Brace,
            pub fields: Punctuated<FieldValue, Token![,]>,
            pub dot2_token: Option<Token![..]>,
            pub rest: Option<Box<Expr>>,
        }),

        /// An array literal constructed from one repeated element: `[0u8; N]`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Repeat(ExprRepeat #full {
            pub attrs: Vec<Attribute>,
            pub bracket_token: token::Bracket,
            pub expr: Box<Expr>,
            pub semi_token: Token![;],
            pub len: Box<Expr>,
        }),

        /// A parenthesized expression: `(a + b)`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Paren(ExprParen {
            pub attrs: Vec<Attribute>,
            pub paren_token: token::Paren,
            pub expr: Box<Expr>,
        }),

        /// An expression contained within invisible delimiters.
        ///
        /// This variant is important for faithfully representing the precedence
        /// of expressions and is related to `None`-delimited spans in a
        /// `TokenStream`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Group(ExprGroup #full {
            pub attrs: Vec<Attribute>,
            pub group_token: token::Group,
            pub expr: Box<Expr>,
        }),

        /// A try-expression: `expr?`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Try(ExprTry #full {
            pub attrs: Vec<Attribute>,
            pub expr: Box<Expr>,
            pub question_token: Token![?],
        }),

        /// A catch expression: `do catch { ... }`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Catch(ExprCatch #full {
            pub attrs: Vec<Attribute>,
            pub do_token: Token![do],
            pub catch_token: Token![catch],
            pub block: Block,
        }),

        /// A yield expression: `yield expr`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Yield(ExprYield #full {
            pub attrs: Vec<Attribute>,
            pub yield_token: Token![yield],
            pub expr: Option<Box<Expr>>,
        }),

        /// Tokens in expression position not interpreted by Syn.
        ///
        /// *This type is available if Syn is built with the `"derive"` or
        /// `"full"` feature.*
        pub Verbatim(ExprVerbatim #manual_extra_traits {
            pub tts: TokenStream,
        }),
    }
}

#[cfg(feature = "extra-traits")]
impl Eq for ExprVerbatim {}

#[cfg(feature = "extra-traits")]
impl PartialEq for ExprVerbatim {
    fn eq(&self, other: &Self) -> bool {
        TokenStreamHelper(&self.tts) == TokenStreamHelper(&other.tts)
    }
}

#[cfg(feature = "extra-traits")]
impl Hash for ExprVerbatim {
    fn hash<H>(&self, state: &mut H)
    where
        H: Hasher,
    {
        TokenStreamHelper(&self.tts).hash(state);
    }
}

impl Expr {
    // Not public API.
    #[doc(hidden)]
    #[cfg(feature = "full")]
    pub fn replace_attrs(&mut self, new: Vec<Attribute>) -> Vec<Attribute> {
        match *self {
            Expr::Box(ExprBox { ref mut attrs, .. })
            | Expr::InPlace(ExprInPlace { ref mut attrs, .. })
            | Expr::Array(ExprArray { ref mut attrs, .. })
            | Expr::Call(ExprCall { ref mut attrs, .. })
            | Expr::MethodCall(ExprMethodCall { ref mut attrs, .. })
            | Expr::Tuple(ExprTuple { ref mut attrs, .. })
            | Expr::Binary(ExprBinary { ref mut attrs, .. })
            | Expr::Unary(ExprUnary { ref mut attrs, .. })
            | Expr::Lit(ExprLit { ref mut attrs, .. })
            | Expr::Cast(ExprCast { ref mut attrs, .. })
            | Expr::Type(ExprType { ref mut attrs, .. })
            | Expr::If(ExprIf { ref mut attrs, .. })
            | Expr::IfLet(ExprIfLet { ref mut attrs, .. })
            | Expr::While(ExprWhile { ref mut attrs, .. })
            | Expr::WhileLet(ExprWhileLet { ref mut attrs, .. })
            | Expr::ForLoop(ExprForLoop { ref mut attrs, .. })
            | Expr::Loop(ExprLoop { ref mut attrs, .. })
            | Expr::Match(ExprMatch { ref mut attrs, .. })
            | Expr::Closure(ExprClosure { ref mut attrs, .. })
            | Expr::Unsafe(ExprUnsafe { ref mut attrs, .. })
            | Expr::Block(ExprBlock { ref mut attrs, .. })
            | Expr::Assign(ExprAssign { ref mut attrs, .. })
            | Expr::AssignOp(ExprAssignOp { ref mut attrs, .. })
            | Expr::Field(ExprField { ref mut attrs, .. })
            | Expr::Index(ExprIndex { ref mut attrs, .. })
            | Expr::Range(ExprRange { ref mut attrs, .. })
            | Expr::Path(ExprPath { ref mut attrs, .. })
            | Expr::Reference(ExprReference { ref mut attrs, .. })
            | Expr::Break(ExprBreak { ref mut attrs, .. })
            | Expr::Continue(ExprContinue { ref mut attrs, .. })
            | Expr::Return(ExprReturn { ref mut attrs, .. })
            | Expr::Macro(ExprMacro { ref mut attrs, .. })
            | Expr::Struct(ExprStruct { ref mut attrs, .. })
            | Expr::Repeat(ExprRepeat { ref mut attrs, .. })
            | Expr::Paren(ExprParen { ref mut attrs, .. })
            | Expr::Group(ExprGroup { ref mut attrs, .. })
            | Expr::Try(ExprTry { ref mut attrs, .. })
            | Expr::Catch(ExprCatch { ref mut attrs, .. })
            | Expr::Yield(ExprYield { ref mut attrs, .. }) => mem::replace(attrs, new),
            Expr::Verbatim(_) => {
                // TODO
                Vec::new()
            }
        }
    }
}

ast_enum! {
    /// A struct or tuple struct field accessed in a struct literal or field
    /// expression.
    ///
    /// *This type is available if Syn is built with the `"derive"` or `"full"`
    /// feature.*
    pub enum Member {
        /// A named field like `self.x`.
        Named(Ident),
        /// An unnamed field like `self.0`.
        Unnamed(Index),
    }
}

ast_struct! {
    /// The index of an unnamed tuple struct field.
    ///
    /// *This type is available if Syn is built with the `"derive"` or `"full"`
    /// feature.*
    pub struct Index #manual_extra_traits {
        pub index: u32,
        pub span: Span,
    }
}

impl From<usize> for Index {
    fn from(index: usize) -> Index {
        assert!(index < std::u32::MAX as usize);
        Index {
            index: index as u32,
            span: Span::call_site(),
        }
    }
}

#[cfg(feature = "extra-traits")]
impl Eq for Index {}

#[cfg(feature = "extra-traits")]
impl PartialEq for Index {
    fn eq(&self, other: &Self) -> bool {
        self.index == other.index
    }
}

#[cfg(feature = "extra-traits")]
impl Hash for Index {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.index.hash(state);
    }
}

#[cfg(feature = "full")]
ast_struct! {
    /// The `::<>` explicit type parameters passed to a method call:
    /// `parse::<u64>()`.
    ///
    /// *This type is available if Syn is built with the `"full"` feature.*
    pub struct MethodTurbofish {
        pub colon2_token: Token![::],
        pub lt_token: Token![<],
        pub args: Punctuated<GenericMethodArgument, Token![,]>,
        pub gt_token: Token![>],
    }
}

#[cfg(feature = "full")]
ast_enum! {
    /// An individual generic argument to a method, like `T`.
    ///
    /// *This type is available if Syn is built with the `"full"` feature.*
    pub enum GenericMethodArgument {
        /// A type argument.
        Type(Type),
        /// A const expression. Must be inside of a block.
        ///
        /// NOTE: Identity expressions are represented as Type arguments, as
        /// they are indistinguishable syntactically.
        Const(Expr),
    }
}

#[cfg(feature = "full")]
ast_struct! {
    /// A field-value pair in a struct literal.
    ///
    /// *This type is available if Syn is built with the `"full"` feature.*
    pub struct FieldValue {
        /// Attributes tagged on the field.
        pub attrs: Vec<Attribute>,

        /// Name or index of the field.
        pub member: Member,

        /// The colon in `Struct { x: x }`. If written in shorthand like
        /// `Struct { x }`, there is no colon.
        pub colon_token: Option<Token![:]>,

        /// Value of the field.
        pub expr: Expr,
    }
}

#[cfg(feature = "full")]
ast_struct! {
    /// A lifetime labeling a `for`, `while`, or `loop`.
    ///
    /// *This type is available if Syn is built with the `"full"` feature.*
    pub struct Label {
        pub name: Lifetime,
        pub colon_token: Token![:],
    }
}

#[cfg(feature = "full")]
ast_struct! {
    /// A braced block containing Rust statements.
    ///
    /// *This type is available if Syn is built with the `"full"` feature.*
    pub struct Block {
        pub brace_token: token::Brace,
        /// Statements in a block
        pub stmts: Vec<Stmt>,
    }
}

#[cfg(feature = "full")]
ast_enum! {
    /// A statement, usually ending in a semicolon.
    ///
    /// *This type is available if Syn is built with the `"full"` feature.*
    pub enum Stmt {
        /// A local (let) binding.
        Local(Local),

        /// An item definition.
        Item(Item),

        /// Expr without trailing semicolon.
        Expr(Expr),

        /// Expression with trailing semicolon.
        Semi(Expr, Token![;]),
    }
}

#[cfg(feature = "full")]
ast_struct! {
    /// A local `let` binding: `let x: u64 = s.parse()?`.
    ///
    /// *This type is available if Syn is built with the `"full"` feature.*
    pub struct Local {
        pub attrs: Vec<Attribute>,
        pub let_token: Token![let],
        pub pats: Punctuated<Pat, Token![|]>,
        pub ty: Option<(Token![:], Box<Type>)>,
        pub init: Option<(Token![=], Box<Expr>)>,
        pub semi_token: Token![;],
    }
}

#[cfg(feature = "full")]
ast_enum_of_structs! {
    /// A pattern in a local binding, function signature, match expression, or
    /// various other places.
    ///
    /// *This type is available if Syn is built with the `"full"` feature.*
    ///
    /// # Syntax tree enum
    ///
    /// This type is a [syntax tree enum].
    ///
    /// [syntax tree enum]: enum.Expr.html#syntax-tree-enums
    // Clippy false positive
    // https://github.com/Manishearth/rust-clippy/issues/1241
    #[cfg_attr(feature = "cargo-clippy", allow(enum_variant_names))]
    pub enum Pat {
        /// A pattern that matches any value: `_`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Wild(PatWild {
            pub underscore_token: Token![_],
        }),

        /// A pattern that binds a new variable: `ref mut binding @ SUBPATTERN`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Ident(PatIdent {
            pub by_ref: Option<Token![ref]>,
            pub mutability: Option<Token![mut]>,
            pub ident: Ident,
            pub subpat: Option<(Token![@], Box<Pat>)>,
        }),

        /// A struct or struct variant pattern: `Variant { x, y, .. }`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Struct(PatStruct {
            pub path: Path,
            pub brace_token: token::Brace,
            pub fields: Punctuated<FieldPat, Token![,]>,
            pub dot2_token: Option<Token![..]>,
        }),

        /// A tuple struct or tuple variant pattern: `Variant(x, y, .., z)`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub TupleStruct(PatTupleStruct {
            pub path: Path,
            pub pat: PatTuple,
        }),

        /// A path pattern like `Color::Red`, optionally qualified with a
        /// self-type.
        ///
        /// Unquailfied path patterns can legally refer to variants, structs,
        /// constants or associated constants. Quailfied path patterns like
        /// `<A>::B::C` and `<A as Trait>::B::C` can only legally refer to
        /// associated constants.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Path(PatPath {
            pub qself: Option<QSelf>,
            pub path: Path,
        }),

        /// A tuple pattern: `(a, b)`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Tuple(PatTuple {
            pub paren_token: token::Paren,
            pub front: Punctuated<Pat, Token![,]>,
            pub dot2_token: Option<Token![..]>,
            pub comma_token: Option<Token![,]>,
            pub back: Punctuated<Pat, Token![,]>,
        }),

        /// A box pattern: `box v`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Box(PatBox {
            pub box_token: Token![box],
            pub pat: Box<Pat>,
        }),

        /// A reference pattern: `&mut (first, second)`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Ref(PatRef {
            pub and_token: Token![&],
            pub mutability: Option<Token![mut]>,
            pub pat: Box<Pat>,
        }),

        /// A literal pattern: `0`.
        ///
        /// This holds an `Expr` rather than a `Lit` because negative numbers
        /// are represented as an `Expr::Unary`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Lit(PatLit {
            pub expr: Box<Expr>,
        }),

        /// A range pattern: `1..=2`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Range(PatRange {
            pub lo: Box<Expr>,
            pub limits: RangeLimits,
            pub hi: Box<Expr>,
        }),

        /// A dynamically sized slice pattern: `[a, b, i.., y, z]`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Slice(PatSlice {
            pub bracket_token: token::Bracket,
            pub front: Punctuated<Pat, Token![,]>,
            pub middle: Option<Box<Pat>>,
            pub dot2_token: Option<Token![..]>,
            pub comma_token: Option<Token![,]>,
            pub back: Punctuated<Pat, Token![,]>,
        }),

        /// A macro in expression position.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Macro(PatMacro {
            pub mac: Macro,
        }),

        /// Tokens in pattern position not interpreted by Syn.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Verbatim(PatVerbatim #manual_extra_traits {
            pub tts: TokenStream,
        }),
    }
}

#[cfg(all(feature = "full", feature = "extra-traits"))]
impl Eq for PatVerbatim {}

#[cfg(all(feature = "full", feature = "extra-traits"))]
impl PartialEq for PatVerbatim {
    fn eq(&self, other: &Self) -> bool {
        TokenStreamHelper(&self.tts) == TokenStreamHelper(&other.tts)
    }
}

#[cfg(all(feature = "full", feature = "extra-traits"))]
impl Hash for PatVerbatim {
    fn hash<H>(&self, state: &mut H)
    where
        H: Hasher,
    {
        TokenStreamHelper(&self.tts).hash(state);
    }
}

#[cfg(feature = "full")]
ast_struct! {
    /// One arm of a `match` expression: `0...10 => { return true; }`.
    ///
    /// As in:
    ///
    /// ```rust
    /// # fn f() -> bool {
    /// #     let n = 0;
    /// match n {
    ///     0...10 => {
    ///         return true;
    ///     }
    ///     // ...
    ///     # _ => {}
    /// }
    /// #   false
    /// # }
    /// ```
    ///
    /// *This type is available if Syn is built with the `"full"` feature.*
    pub struct Arm {
        pub attrs: Vec<Attribute>,
        pub leading_vert: Option<Token![|]>,
        pub pats: Punctuated<Pat, Token![|]>,
        pub guard: Option<(Token![if], Box<Expr>)>,
        pub fat_arrow_token: Token![=>],
        pub body: Box<Expr>,
        pub comma: Option<Token![,]>,
    }
}

#[cfg(feature = "full")]
ast_enum! {
    /// Limit types of a range, inclusive or exclusive.
    ///
    /// *This type is available if Syn is built with the `"full"` feature.*
    #[cfg_attr(feature = "clone-impls", derive(Copy))]
    pub enum RangeLimits {
        /// Inclusive at the beginning, exclusive at the end.
        HalfOpen(Token![..]),
        /// Inclusive at the beginning and end.
        Closed(Token![..=]),
    }
}

#[cfg(feature = "full")]
ast_struct! {
    /// A single field in a struct pattern.
    ///
    /// Patterns like the fields of Foo `{ x, ref y, ref mut z }` are treated
    /// the same as `x: x, y: ref y, z: ref mut z` but there is no colon token.
    ///
    /// *This type is available if Syn is built with the `"full"` feature.*
    pub struct FieldPat {
        pub attrs: Vec<Attribute>,
        pub member: Member,
        pub colon_token: Option<Token![:]>,
        pub pat: Box<Pat>,
    }
}

#[cfg(any(feature = "parsing", feature = "printing"))]
#[cfg(feature = "full")]
fn arm_expr_requires_comma(expr: &Expr) -> bool {
    // see https://github.com/rust-lang/rust/blob/eb8f2586e
    //                       /src/libsyntax/parse/classify.rs#L17-L37
    match *expr {
        Expr::Unsafe(..)
        | Expr::Block(..)
        | Expr::If(..)
        | Expr::IfLet(..)
        | Expr::Match(..)
        | Expr::While(..)
        | Expr::WhileLet(..)
        | Expr::Loop(..)
        | Expr::ForLoop(..)
        | Expr::Catch(..) => false,
        _ => true,
    }
}

#[cfg(feature = "parsing")]
pub mod parsing {
    use super::*;
    use path::parsing::qpath;
    #[cfg(feature = "full")]
    use path::parsing::ty_no_eq_after;

    #[cfg(feature = "full")]
    use proc_macro2::TokenStream;
    use synom::Synom;
    use buffer::Cursor;
    #[cfg(feature = "full")]
    use parse_error;
    use synom::PResult;

    // When we're parsing expressions which occur before blocks, like in an if
    // statement's condition, we cannot parse a struct literal.
    //
    // Struct literals are ambiguous in certain positions
    // https://github.com/rust-lang/rfcs/pull/92
    macro_rules! ambiguous_expr {
        ($i:expr, $allow_struct:ident) => {
            ambiguous_expr($i, $allow_struct, true)
        };
    }

    // When we are parsing an optional suffix expression, we cannot allow blocks
    // if structs are not allowed.
    //
    // Example:
    //
    //     if break {} {}
    //
    // is ambiguous between:
    //
    //     if (break {}) {}
    //     if (break) {} {}
    #[cfg(feature = "full")]
    macro_rules! opt_ambiguous_expr {
        ($i:expr, $allow_struct:ident) => {
            option!($i, call!(ambiguous_expr, $allow_struct, $allow_struct))
        };
    }

    impl Synom for Expr {
        named!(parse -> Self, ambiguous_expr!(true));

        fn description() -> Option<&'static str> {
            Some("expression")
        }
    }

    #[cfg(feature = "full")]
    named!(expr_no_struct -> Expr, ambiguous_expr!(false));

    // Parse an arbitrary expression.
    #[cfg(feature = "full")]
    fn ambiguous_expr(i: Cursor, allow_struct: bool, allow_block: bool) -> PResult<Expr> {
        call!(i, assign_expr, allow_struct, allow_block)
    }

    #[cfg(not(feature = "full"))]
    fn ambiguous_expr(i: Cursor, allow_struct: bool, allow_block: bool) -> PResult<Expr> {
        // NOTE: We intentionally skip assign_expr, placement_expr, and
        // range_expr, as they are not parsed in non-full mode.
        call!(i, or_expr, allow_struct, allow_block)
    }

    // Parse a left-associative binary operator.
    macro_rules! binop {
        (
            $name: ident,
            $next: ident,
            $submac: ident!( $($args:tt)* )
        ) => {
            named!($name(allow_struct: bool, allow_block: bool) -> Expr, do_parse!(
                mut e: call!($next, allow_struct, allow_block) >>
                many0!(do_parse!(
                    op: $submac!($($args)*) >>
                    rhs: call!($next, allow_struct, true) >>
                    ({
                        e = ExprBinary {
                            attrs: Vec::new(),
                            left: Box::new(e.into()),
                            op: op,
                            right: Box::new(rhs.into()),
                        }.into();
                    })
                )) >>
                (e)
            ));
        }
    }

    // <placement> = <placement> ..
    // <placement> += <placement> ..
    // <placement> -= <placement> ..
    // <placement> *= <placement> ..
    // <placement> /= <placement> ..
    // <placement> %= <placement> ..
    // <placement> ^= <placement> ..
    // <placement> &= <placement> ..
    // <placement> |= <placement> ..
    // <placement> <<= <placement> ..
    // <placement> >>= <placement> ..
    //
    // NOTE: This operator is right-associative.
    #[cfg(feature = "full")]
    named!(assign_expr(allow_struct: bool, allow_block: bool) -> Expr, do_parse!(
        mut e: call!(placement_expr, allow_struct, allow_block) >>
        alt!(
            do_parse!(
                eq: punct!(=) >>
                // Recurse into self to parse right-associative operator.
                rhs: call!(assign_expr, allow_struct, true) >>
                ({
                    e = ExprAssign {
                        attrs: Vec::new(),
                        left: Box::new(e),
                        eq_token: eq,
                        right: Box::new(rhs),
                    }.into();
                })
            )
            |
            do_parse!(
                op: call!(BinOp::parse_assign_op) >>
                // Recurse into self to parse right-associative operator.
                rhs: call!(assign_expr, allow_struct, true) >>
                ({
                    e = ExprAssignOp {
                        attrs: Vec::new(),
                        left: Box::new(e),
                        op: op,
                        right: Box::new(rhs),
                    }.into();
                })
            )
            |
            epsilon!()
        ) >>
        (e)
    ));

    // <range> <- <range> ..
    //
    // NOTE: The `in place { expr }` version of this syntax is parsed in
    // `atom_expr`, not here.
    //
    // NOTE: This operator is right-associative.
    #[cfg(feature = "full")]
    named!(placement_expr(allow_struct: bool, allow_block: bool) -> Expr, do_parse!(
        mut e: call!(range_expr, allow_struct, allow_block) >>
        alt!(
            do_parse!(
                arrow: punct!(<-) >>
                // Recurse into self to parse right-associative operator.
                rhs: call!(placement_expr, allow_struct, true) >>
                ({
                    e = ExprInPlace {
                        attrs: Vec::new(),
                        // op: BinOp::Place(larrow),
                        place: Box::new(e),
                        arrow_token: arrow,
                        value: Box::new(rhs),
                    }.into();
                })
            )
            |
            epsilon!()
        ) >>
        (e)
    ));

    // <or> ... <or> ..
    // <or> .. <or> ..
    // <or> ..
    //
    // NOTE: This is currently parsed oddly - I'm not sure of what the exact
    // rules are for parsing these expressions are, but this is not correct.
    // For example, `a .. b .. c` is not a legal expression. It should not
    // be parsed as either `(a .. b) .. c` or `a .. (b .. c)` apparently.
    //
    // NOTE: The form of ranges which don't include a preceding expression are
    // parsed by `atom_expr`, rather than by this function.
    #[cfg(feature = "full")]
    named!(range_expr(allow_struct: bool, allow_block: bool) -> Expr, do_parse!(
        mut e: call!(or_expr, allow_struct, allow_block) >>
        many0!(do_parse!(
            limits: syn!(RangeLimits) >>
            // We don't want to allow blocks here if we don't allow structs. See
            // the reasoning for `opt_ambiguous_expr!` above.
            hi: option!(call!(or_expr, allow_struct, allow_struct)) >>
            ({
                e = ExprRange {
                    attrs: Vec::new(),
                    from: Some(Box::new(e)),
                    limits: limits,
                    to: hi.map(|e| Box::new(e)),
                }.into();
            })
        )) >>
        (e)
    ));

    // <and> || <and> ...
    binop!(or_expr, and_expr, map!(punct!(||), BinOp::Or));

    // <compare> && <compare> ...
    binop!(and_expr, compare_expr, map!(punct!(&&), BinOp::And));

    // <bitor> == <bitor> ...
    // <bitor> != <bitor> ...
    // <bitor> >= <bitor> ...
    // <bitor> <= <bitor> ...
    // <bitor> > <bitor> ...
    // <bitor> < <bitor> ...
    //
    // NOTE: This operator appears to be parsed as left-associative, but errors
    // if it is used in a non-associative manner.
    binop!(
        compare_expr,
        bitor_expr,
        alt!(
        punct!(==) => { BinOp::Eq }
        |
        punct!(!=) => { BinOp::Ne }
        |
        // must be above Lt
        punct!(<=) => { BinOp::Le }
        |
        // must be above Gt
        punct!(>=) => { BinOp::Ge }
        |
        do_parse!(
            // Make sure that we don't eat the < part of a <- operator
            not!(punct!(<-)) >>
            t: punct!(<) >>
            (BinOp::Lt(t))
        )
        |
        punct!(>) => { BinOp::Gt }
    )
    );

    // <bitxor> | <bitxor> ...
    binop!(
        bitor_expr,
        bitxor_expr,
        do_parse!(not!(punct!(||)) >> not!(punct!(|=)) >> t: punct!(|) >> (BinOp::BitOr(t)))
    );

    // <bitand> ^ <bitand> ...
    binop!(
        bitxor_expr,
        bitand_expr,
        do_parse!(
            // NOTE: Make sure we aren't looking at ^=.
            not!(punct!(^=)) >> t: punct!(^) >> (BinOp::BitXor(t))
        )
    );

    // <shift> & <shift> ...
    binop!(
        bitand_expr,
        shift_expr,
        do_parse!(
            // NOTE: Make sure we aren't looking at && or &=.
            not!(punct!(&&)) >> not!(punct!(&=)) >> t: punct!(&) >> (BinOp::BitAnd(t))
        )
    );

    // <arith> << <arith> ...
    // <arith> >> <arith> ...
    binop!(
        shift_expr,
        arith_expr,
        alt!(
        punct!(<<) => { BinOp::Shl }
        |
        punct!(>>) => { BinOp::Shr }
    )
    );

    // <term> + <term> ...
    // <term> - <term> ...
    binop!(
        arith_expr,
        term_expr,
        alt!(
        punct!(+) => { BinOp::Add }
        |
        punct!(-) => { BinOp::Sub }
    )
    );

    // <cast> * <cast> ...
    // <cast> / <cast> ...
    // <cast> % <cast> ...
    binop!(
        term_expr,
        cast_expr,
        alt!(
        punct!(*) => { BinOp::Mul }
        |
        punct!(/) => { BinOp::Div }
        |
        punct!(%) => { BinOp::Rem }
    )
    );

    // <unary> as <ty>
    // <unary> : <ty>
    #[cfg(feature = "full")]
    named!(cast_expr(allow_struct: bool, allow_block: bool) -> Expr, do_parse!(
        mut e: call!(unary_expr, allow_struct, allow_block) >>
        many0!(alt!(
            do_parse!(
                as_: keyword!(as) >>
                // We can't accept `A + B` in cast expressions, as it's
                // ambiguous with the + expression.
                ty: call!(Type::without_plus) >>
                ({
                    e = ExprCast {
                        attrs: Vec::new(),
                        expr: Box::new(e),
                        as_token: as_,
                        ty: Box::new(ty),
                    }.into();
                })
            )
            |
            do_parse!(
                colon: punct!(:) >>
                // We can't accept `A + B` in cast expressions, as it's
                // ambiguous with the + expression.
                ty: call!(Type::without_plus) >>
                ({
                    e = ExprType {
                        attrs: Vec::new(),
                        expr: Box::new(e),
                        colon_token: colon,
                        ty: Box::new(ty),
                    }.into();
                })
            )
        )) >>
        (e)
    ));

    // <unary> as <ty>
    #[cfg(not(feature = "full"))]
    named!(cast_expr(allow_struct: bool, allow_block: bool) -> Expr, do_parse!(
        mut e: call!(unary_expr, allow_struct, allow_block) >>
        many0!(do_parse!(
            as_: keyword!(as) >>
            // We can't accept `A + B` in cast expressions, as it's
            // ambiguous with the + expression.
            ty: call!(Type::without_plus) >>
            ({
                e = ExprCast {
                    attrs: Vec::new(),
                    expr: Box::new(e),
                    as_token: as_,
                    ty: Box::new(ty),
                }.into();
            })
        )) >>
        (e)
    ));

    // <UnOp> <trailer>
    // & <trailer>
    // &mut <trailer>
    // box <trailer>
    #[cfg(feature = "full")]
    named!(unary_expr(allow_struct: bool, allow_block: bool) -> Expr, alt!(
        do_parse!(
            op: syn!(UnOp) >>
            expr: call!(unary_expr, allow_struct, true) >>
            (ExprUnary {
                attrs: Vec::new(),
                op: op,
                expr: Box::new(expr),
            }.into())
        )
        |
        do_parse!(
            and: punct!(&) >>
            mutability: option!(keyword!(mut)) >>
            expr: call!(unary_expr, allow_struct, true) >>
            (ExprReference {
                attrs: Vec::new(),
                and_token: and,
                mutability: mutability,
                expr: Box::new(expr),
            }.into())
        )
        |
        do_parse!(
            box_: keyword!(box) >>
            expr: call!(unary_expr, allow_struct, true) >>
            (ExprBox {
                attrs: Vec::new(),
                box_token: box_,
                expr: Box::new(expr),
            }.into())
        )
        |
        call!(trailer_expr, allow_struct, allow_block)
    ));

    // XXX: This duplication is ugly
    #[cfg(not(feature = "full"))]
    named!(unary_expr(allow_struct: bool, allow_block: bool) -> Expr, alt!(
        do_parse!(
            op: syn!(UnOp) >>
            expr: call!(unary_expr, allow_struct, true) >>
            (ExprUnary {
                attrs: Vec::new(),
                op: op,
                expr: Box::new(expr),
            }.into())
        )
        |
        call!(trailer_expr, allow_struct, allow_block)
    ));

    // <atom> (..<args>) ...
    // <atom> . <ident> (..<args>) ...
    // <atom> . <ident> ...
    // <atom> . <lit> ...
    // <atom> [ <expr> ] ...
    // <atom> ? ...
    #[cfg(feature = "full")]
    named!(trailer_expr(allow_struct: bool, allow_block: bool) -> Expr, do_parse!(
        mut e: call!(atom_expr, allow_struct, allow_block) >>
        many0!(alt!(
            tap!(args: and_call => {
                let (paren, args) = args;
                e = ExprCall {
                    attrs: Vec::new(),
                    func: Box::new(e),
                    args: args,
                    paren_token: paren,
                }.into();
            })
            |
            tap!(more: and_method_call => {
                let mut call = more;
                call.receiver = Box::new(e);
                e = call.into();
            })
            |
            tap!(field: and_field => {
                let (token, member) = field;
                e = ExprField {
                    attrs: Vec::new(),
                    base: Box::new(e),
                    dot_token: token,
                    member: member,
                }.into();
            })
            |
            tap!(i: and_index => {
                let (bracket, i) = i;
                e = ExprIndex {
                    attrs: Vec::new(),
                    expr: Box::new(e),
                    bracket_token: bracket,
                    index: Box::new(i),
                }.into();
            })
            |
            tap!(question: punct!(?) => {
                e = ExprTry {
                    attrs: Vec::new(),
                    expr: Box::new(e),
                    question_token: question,
                }.into();
            })
        )) >>
        (e)
    ));

    // XXX: Duplication == ugly
    #[cfg(not(feature = "full"))]
    named!(trailer_expr(allow_struct: bool, allow_block: bool) -> Expr, do_parse!(
        mut e: call!(atom_expr, allow_struct, allow_block) >>
        many0!(alt!(
            tap!(args: and_call => {
                e = ExprCall {
                    attrs: Vec::new(),
                    func: Box::new(e),
                    paren_token: args.0,
                    args: args.1,
                }.into();
            })
            |
            tap!(i: and_index => {
                e = ExprIndex {
                    attrs: Vec::new(),
                    expr: Box::new(e),
                    bracket_token: i.0,
                    index: Box::new(i.1),
                }.into();
            })
        )) >>
        (e)
    ));

    // Parse all atomic expressions which don't have to worry about precedence
    // interactions, as they are fully contained.
    #[cfg(feature = "full")]
    named!(atom_expr(allow_struct: bool, allow_block: bool) -> Expr, alt!(
        syn!(ExprGroup) => { Expr::Group } // must be placed first
        |
        syn!(ExprLit) => { Expr::Lit } // must be before expr_struct
        |
        // must be before expr_path
        cond_reduce!(allow_struct, syn!(ExprStruct)) => { Expr::Struct }
        |
        syn!(ExprParen) => { Expr::Paren } // must be before expr_tup
        |
        syn!(ExprMacro) => { Expr::Macro } // must be before expr_path
        |
        call!(expr_break, allow_struct) // must be before expr_path
        |
        syn!(ExprContinue) => { Expr::Continue } // must be before expr_path
        |
        call!(expr_ret, allow_struct) // must be before expr_path
        |
        syn!(ExprArray) => { Expr::Array }
        |
        syn!(ExprTuple) => { Expr::Tuple }
        |
        syn!(ExprIf) => { Expr::If }
        |
        syn!(ExprIfLet) => { Expr::IfLet }
        |
        syn!(ExprWhile) => { Expr::While }
        |
        syn!(ExprWhileLet) => { Expr::WhileLet }
        |
        syn!(ExprForLoop) => { Expr::ForLoop }
        |
        syn!(ExprLoop) => { Expr::Loop }
        |
        syn!(ExprMatch) => { Expr::Match }
        |
        syn!(ExprCatch) => { Expr::Catch }
        |
        syn!(ExprYield) => { Expr::Yield }
        |
        syn!(ExprUnsafe) => { Expr::Unsafe }
        |
        call!(expr_closure, allow_struct)
        |
        cond_reduce!(allow_block, syn!(ExprBlock)) => { Expr::Block }
        |
        // NOTE: This is the prefix-form of range
        call!(expr_range, allow_struct)
        |
        syn!(ExprPath) => { Expr::Path }
        |
        syn!(ExprRepeat) => { Expr::Repeat }
    ));

    #[cfg(not(feature = "full"))]
    named!(atom_expr(_allow_struct: bool, _allow_block: bool) -> Expr, alt!(
        syn!(ExprLit) => { Expr::Lit }
        |
        syn!(ExprParen) => { Expr::Paren }
        |
        syn!(ExprPath) => { Expr::Path }
    ));

    #[cfg(feature = "full")]
    named!(expr_nosemi -> Expr, map!(alt!(
        syn!(ExprIf) => { Expr::If }
        |
        syn!(ExprIfLet) => { Expr::IfLet }
        |
        syn!(ExprWhile) => { Expr::While }
        |
        syn!(ExprWhileLet) => { Expr::WhileLet }
        |
        syn!(ExprForLoop) => { Expr::ForLoop }
        |
        syn!(ExprLoop) => { Expr::Loop }
        |
        syn!(ExprMatch) => { Expr::Match }
        |
        syn!(ExprCatch) => { Expr::Catch }
        |
        syn!(ExprYield) => { Expr::Yield }
        |
        syn!(ExprUnsafe) => { Expr::Unsafe }
        |
        syn!(ExprBlock) => { Expr::Block }
    ), Expr::from));

    impl Synom for ExprLit {
        named!(parse -> Self, do_parse!(
            lit: syn!(Lit) >>
            (ExprLit {
                attrs: Vec::new(),
                lit: lit,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("literal")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for ExprMacro {
        named!(parse -> Self, do_parse!(
            mac: syn!(Macro) >>
            (ExprMacro {
                attrs: Vec::new(),
                mac: mac,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("macro invocation expression")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for ExprGroup {
        named!(parse -> Self, do_parse!(
            e: grouped!(syn!(Expr)) >>
            (ExprGroup {
                attrs: Vec::new(),
                expr: Box::new(e.1),
                group_token: e.0,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("expression surrounded by invisible delimiters")
        }
    }

    impl Synom for ExprParen {
        named!(parse -> Self, do_parse!(
            e: parens!(syn!(Expr)) >>
            (ExprParen {
                attrs: Vec::new(),
                paren_token: e.0,
                expr: Box::new(e.1),
            })
        ));

        fn description() -> Option<&'static str> {
            Some("parenthesized expression")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for ExprArray {
        named!(parse -> Self, do_parse!(
            elems: brackets!(Punctuated::parse_terminated) >>
            (ExprArray {
                attrs: Vec::new(),
                bracket_token: elems.0,
                elems: elems.1,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("array expression")
        }
    }

    named!(and_call -> (token::Paren, Punctuated<Expr, Token![,]>),
           parens!(Punctuated::parse_terminated));

    #[cfg(feature = "full")]
    named!(and_method_call -> ExprMethodCall, do_parse!(
        dot: punct!(.) >>
        method: syn!(Ident) >>
        turbofish: option!(tuple!(
            punct!(::),
            punct!(<),
            call!(Punctuated::parse_terminated),
            punct!(>)
        )) >>
        args: parens!(Punctuated::parse_terminated) >>
        ({
            ExprMethodCall {
                attrs: Vec::new(),
                // this expr will get overwritten after being returned
                receiver: Box::new(Expr::Verbatim(ExprVerbatim {
                    tts: TokenStream::empty(),
                })),

                method: method,
                turbofish: turbofish.map(|fish| MethodTurbofish {
                    colon2_token: fish.0,
                    lt_token: fish.1,
                    args: fish.2,
                    gt_token: fish.3,
                }),
                args: args.1,
                paren_token: args.0,
                dot_token: dot,
            }
        })
    ));

    #[cfg(feature = "full")]
    impl Synom for GenericMethodArgument {
        // TODO parse const generics as well
        named!(parse -> Self, map!(ty_no_eq_after, GenericMethodArgument::Type));

        fn description() -> Option<&'static str> {
            Some("generic method argument")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for ExprTuple {
        named!(parse -> Self, do_parse!(
            elems: parens!(Punctuated::parse_terminated) >>
            (ExprTuple {
                attrs: Vec::new(),
                elems: elems.1,
                paren_token: elems.0,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("tuple")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for ExprIfLet {
        named!(parse -> Self, do_parse!(
            if_: keyword!(if) >>
            let_: keyword!(let) >>
            pats: call!(Punctuated::parse_separated_nonempty) >>
            eq: punct!(=) >>
            cond: expr_no_struct >>
            then_block: braces!(Block::parse_within) >>
            else_block: option!(else_block) >>
            (ExprIfLet {
                attrs: Vec::new(),
                pats: pats,
                let_token: let_,
                eq_token: eq,
                expr: Box::new(cond),
                then_branch: Block {
                    brace_token: then_block.0,
                    stmts: then_block.1,
                },
                if_token: if_,
                else_branch: else_block,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("`if let` expression")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for ExprIf {
        named!(parse -> Self, do_parse!(
            if_: keyword!(if) >>
            cond: expr_no_struct >>
            then_block: braces!(Block::parse_within) >>
            else_block: option!(else_block) >>
            (ExprIf {
                attrs: Vec::new(),
                cond: Box::new(cond),
                then_branch: Block {
                    brace_token: then_block.0,
                    stmts: then_block.1,
                },
                if_token: if_,
                else_branch: else_block,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("`if` expression")
        }
    }

    #[cfg(feature = "full")]
    named!(else_block -> (Token![else], Box<Expr>), do_parse!(
        else_: keyword!(else) >>
        expr: alt!(
            syn!(ExprIf) => { Expr::If }
            |
            syn!(ExprIfLet) => { Expr::IfLet }
            |
            do_parse!(
                else_block: braces!(Block::parse_within) >>
                (Expr::Block(ExprBlock {
                    attrs: Vec::new(),
                    block: Block {
                        brace_token: else_block.0,
                        stmts: else_block.1,
                    },
                }))
            )
        ) >>
        (else_, Box::new(expr))
    ));

    #[cfg(feature = "full")]
    impl Synom for ExprForLoop {
        named!(parse -> Self, do_parse!(
            label: option!(syn!(Label)) >>
            for_: keyword!(for) >>
            pat: syn!(Pat) >>
            in_: keyword!(in) >>
            expr: expr_no_struct >>
            loop_block: syn!(Block) >>
            (ExprForLoop {
                attrs: Vec::new(),
                for_token: for_,
                in_token: in_,
                pat: Box::new(pat),
                expr: Box::new(expr),
                body: loop_block,
                label: label,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("`for` loop")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for ExprLoop {
        named!(parse -> Self, do_parse!(
            label: option!(syn!(Label)) >>
            loop_: keyword!(loop) >>
            loop_block: syn!(Block) >>
            (ExprLoop {
                attrs: Vec::new(),
                loop_token: loop_,
                body: loop_block,
                label: label,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("`loop`")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for ExprMatch {
        named!(parse -> Self, do_parse!(
            match_: keyword!(match) >>
            obj: expr_no_struct >>
            res: braces!(many0!(Arm::parse)) >>
            (ExprMatch {
                attrs: Vec::new(),
                expr: Box::new(obj),
                match_token: match_,
                brace_token: res.0,
                arms: res.1,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("`match` expression")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for ExprCatch {
        named!(parse -> Self, do_parse!(
            do_: keyword!(do) >>
            catch_: keyword!(catch) >>
            catch_block: syn!(Block) >>
            (ExprCatch {
                attrs: Vec::new(),
                block: catch_block,
                do_token: do_,
                catch_token: catch_,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("`catch` expression")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for ExprYield {
        named!(parse -> Self, do_parse!(
            yield_: keyword!(yield) >>
            expr: option!(syn!(Expr)) >>
            (ExprYield {
                attrs: Vec::new(),
                yield_token: yield_,
                expr: expr.map(Box::new),
            })
        ));

        fn description() -> Option<&'static str> {
            Some("`yield` expression")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for Arm {
        named!(parse -> Self, do_parse!(
            attrs: many0!(Attribute::parse_outer) >>
            leading_vert: option!(punct!(|)) >>
            pats: call!(Punctuated::parse_separated_nonempty) >>
            guard: option!(tuple!(keyword!(if), syn!(Expr))) >>
            fat_arrow: punct!(=>) >>
            body: do_parse!(
                expr: alt!(expr_nosemi | syn!(Expr)) >>
                comma: switch!(value!(arm_expr_requires_comma(&expr)),
                    true => alt!(
                        input_end!() => { |_| None }
                        |
                        punct!(,) => { Some }
                    )
                    |
                    false => option!(punct!(,))
                ) >>
                (expr, comma)
            ) >>
            (Arm {
                fat_arrow_token: fat_arrow,
                attrs: attrs,
                leading_vert: leading_vert,
                pats: pats,
                guard: guard.map(|(if_, guard)| (if_, Box::new(guard))),
                body: Box::new(body.0),
                comma: body.1,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("`match` arm")
        }
    }

    #[cfg(feature = "full")]
    named!(expr_closure(allow_struct: bool) -> Expr, do_parse!(
        movability: option!(keyword!(static)) >>
        capture: option!(keyword!(move)) >>
        or1: punct!(|) >>
        inputs: call!(Punctuated::parse_terminated_with, fn_arg) >>
        or2: punct!(|) >>
        ret_and_body: alt!(
            do_parse!(
                arrow: punct!(->) >>
                ty: syn!(Type) >>
                body: syn!(Block) >>
                (ReturnType::Type(arrow, Box::new(ty)),
                 Expr::Block(ExprBlock {
                     attrs: Vec::new(),
                    block: body,
                }))
            )
            |
            map!(ambiguous_expr!(allow_struct), |e| (ReturnType::Default, e))
        ) >>
        (ExprClosure {
            attrs: Vec::new(),
            movability: movability,
            capture: capture,
            or1_token: or1,
            inputs: inputs,
            or2_token: or2,
            output: ret_and_body.0,
            body: Box::new(ret_and_body.1),
        }.into())
    ));

    #[cfg(feature = "full")]
    named!(fn_arg -> FnArg, do_parse!(
        pat: syn!(Pat) >>
        ty: option!(tuple!(punct!(:), syn!(Type))) >>
        ({
            if let Some((colon, ty)) = ty {
                FnArg::Captured(ArgCaptured {
                    pat: pat,
                    colon_token: colon,
                    ty: ty,
                })
            } else {
                FnArg::Inferred(pat)
            }
        })
    ));

    #[cfg(feature = "full")]
    impl Synom for ExprWhile {
        named!(parse -> Self, do_parse!(
            label: option!(syn!(Label)) >>
            while_: keyword!(while) >>
            cond: expr_no_struct >>
            while_block: syn!(Block) >>
            (ExprWhile {
                attrs: Vec::new(),
                while_token: while_,
                cond: Box::new(cond),
                body: while_block,
                label: label,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("`while` expression")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for ExprWhileLet {
        named!(parse -> Self, do_parse!(
            label: option!(syn!(Label)) >>
            while_: keyword!(while) >>
            let_: keyword!(let) >>
            pats: call!(Punctuated::parse_separated_nonempty) >>
            eq: punct!(=) >>
            value: expr_no_struct >>
            while_block: syn!(Block) >>
            (ExprWhileLet {
                attrs: Vec::new(),
                eq_token: eq,
                let_token: let_,
                while_token: while_,
                pats: pats,
                expr: Box::new(value),
                body: while_block,
                label: label,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("`while let` expression")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for Label {
        named!(parse -> Self, do_parse!(
            name: syn!(Lifetime) >>
            colon: punct!(:) >>
            (Label {
                name: name,
                colon_token: colon,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("`while let` expression")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for ExprContinue {
        named!(parse -> Self, do_parse!(
            cont: keyword!(continue) >>
            label: option!(syn!(Lifetime)) >>
            (ExprContinue {
                attrs: Vec::new(),
                continue_token: cont,
                label: label,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("`continue`")
        }
    }

    #[cfg(feature = "full")]
    named!(expr_break(allow_struct: bool) -> Expr, do_parse!(
        break_: keyword!(break) >>
        label: option!(syn!(Lifetime)) >>
        // We can't allow blocks after a `break` expression when we wouldn't
        // allow structs, as this expression is ambiguous.
        val: opt_ambiguous_expr!(allow_struct) >>
        (ExprBreak {
            attrs: Vec::new(),
            label: label,
            expr: val.map(Box::new),
            break_token: break_,
        }.into())
    ));

    #[cfg(feature = "full")]
    named!(expr_ret(allow_struct: bool) -> Expr, do_parse!(
        return_: keyword!(return) >>
        // NOTE: return is greedy and eats blocks after it even when in a
        // position where structs are not allowed, such as in if statement
        // conditions. For example:
        //
        // if return { println!("A") } {} // Prints "A"
        ret_value: option!(ambiguous_expr!(allow_struct)) >>
        (ExprReturn {
            attrs: Vec::new(),
            expr: ret_value.map(Box::new),
            return_token: return_,
        }.into())
    ));

    #[cfg(feature = "full")]
    impl Synom for ExprStruct {
        named!(parse -> Self, do_parse!(
            path: syn!(Path) >>
            data: braces!(do_parse!(
                fields: call!(Punctuated::parse_terminated) >>
                base: option!(cond!(fields.empty_or_trailing(), do_parse!(
                    dots: punct!(..) >>
                    base: syn!(Expr) >>
                    (dots, base)
                ))) >>
                (fields, base)
            )) >>
            ({
                let (brace, (fields, base)) = data;
                let (dots, rest) = match base.and_then(|b| b) {
                    Some((dots, base)) => (Some(dots), Some(base)),
                    None => (None, None),
                };
                ExprStruct {
                    attrs: Vec::new(),
                    brace_token: brace,
                    path: path,
                    fields: fields,
                    dot2_token: dots,
                    rest: rest.map(Box::new),
                }
            })
        ));

        fn description() -> Option<&'static str> {
            Some("struct literal expression")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for FieldValue {
        named!(parse -> Self, do_parse!(
            attrs: many0!(Attribute::parse_outer) >>
            field_value: alt!(
                tuple!(syn!(Member), map!(punct!(:), Some), syn!(Expr))
                |
                map!(syn!(Ident), |name| (
                    Member::Named(name),
                    None,
                    Expr::Path(ExprPath {
                        attrs: Vec::new(),
                        qself: None,
                        path: name.into(),
                    }),
                ))
            ) >>
            (FieldValue {
                attrs: attrs,
                member: field_value.0,
                colon_token: field_value.1,
                expr: field_value.2,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("field-value pair: `field: value`")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for ExprRepeat {
        named!(parse -> Self, do_parse!(
            data: brackets!(do_parse!(
                value: syn!(Expr) >>
                semi: punct!(;) >>
                times: syn!(Expr) >>
                (value, semi, times)
            )) >>
            (ExprRepeat {
                attrs: Vec::new(),
                expr: Box::new((data.1).0),
                len: Box::new((data.1).2),
                bracket_token: data.0,
                semi_token: (data.1).1,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("repeated array literal: `[val; N]`")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for ExprUnsafe {
        named!(parse -> Self, do_parse!(
            unsafe_: keyword!(unsafe) >>
            b: syn!(Block) >>
            (ExprUnsafe {
                attrs: Vec::new(),
                unsafe_token: unsafe_,
                block: b,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("unsafe block: `unsafe { .. }`")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for ExprBlock {
        named!(parse -> Self, do_parse!(
            b: syn!(Block) >>
            (ExprBlock {
                attrs: Vec::new(),
                block: b,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("block: `{ .. }`")
        }
    }

    #[cfg(feature = "full")]
    named!(expr_range(allow_struct: bool) -> Expr, do_parse!(
        limits: syn!(RangeLimits) >>
        hi: opt_ambiguous_expr!(allow_struct) >>
        (ExprRange {
            attrs: Vec::new(),
            from: None,
            to: hi.map(Box::new),
            limits: limits,
        }.into())
    ));

    #[cfg(feature = "full")]
    impl Synom for RangeLimits {
        named!(parse -> Self, alt!(
            // Must come before Dot2
            punct!(..=) => { RangeLimits::Closed }
            |
            // Must come before Dot2
            punct!(...) => { |dot3| RangeLimits::Closed(Token![..=](dot3.0)) }
            |
            punct!(..) => { RangeLimits::HalfOpen }
        ));

        fn description() -> Option<&'static str> {
            Some("range limit: `..`, `...` or `..=`")
        }
    }

    impl Synom for ExprPath {
        named!(parse -> Self, do_parse!(
            pair: qpath >>
            (ExprPath {
                attrs: Vec::new(),
                qself: pair.0,
                path: pair.1,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("path: `a::b::c`")
        }
    }

    #[cfg(feature = "full")]
    named!(and_field -> (Token![.], Member), tuple!(punct!(.), syn!(Member)));

    named!(and_index -> (token::Bracket, Expr), brackets!(syn!(Expr)));

    #[cfg(feature = "full")]
    impl Synom for Block {
        named!(parse -> Self, do_parse!(
            stmts: braces!(Block::parse_within) >>
            (Block {
                brace_token: stmts.0,
                stmts: stmts.1,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("block: `{ .. }`")
        }
    }

    #[cfg(feature = "full")]
    impl Block {
        named!(pub parse_within -> Vec<Stmt>, do_parse!(
            many0!(punct!(;)) >>
            mut standalone: many0!(do_parse!(
                stmt: syn!(Stmt) >>
                many0!(punct!(;)) >>
                (stmt)
            )) >>
            last: option!(do_parse!(
                attrs: many0!(Attribute::parse_outer) >>
                mut e: syn!(Expr) >>
                ({
                    e.replace_attrs(attrs);
                    Stmt::Expr(e)
                })
            )) >>
            (match last {
                None => standalone,
                Some(last) => {
                    standalone.push(last);
                    standalone
                }
            })
        ));
    }

    #[cfg(feature = "full")]
    impl Synom for Stmt {
        named!(parse -> Self, alt!(
            stmt_mac
            |
            stmt_local
            |
            stmt_item
            |
            stmt_blockexpr
            |
            stmt_expr
        ));

        fn description() -> Option<&'static str> {
            Some("statement")
        }
    }

    #[cfg(feature = "full")]
    named!(stmt_mac -> Stmt, do_parse!(
        attrs: many0!(Attribute::parse_outer) >>
        what: call!(Path::parse_mod_style) >>
        bang: punct!(!) >>
    // Only parse braces here; paren and bracket will get parsed as
    // expression statements
        data: braces!(syn!(TokenStream)) >>
        semi: option!(punct!(;)) >>
        (Stmt::Item(Item::Macro(ItemMacro {
            attrs: attrs,
            ident: None,
            mac: Macro {
                path: what,
                bang_token: bang,
                delimiter: MacroDelimiter::Brace(data.0),
                tts: data.1,
            },
            semi_token: semi,
        })))
    ));

    #[cfg(feature = "full")]
    named!(stmt_local -> Stmt, do_parse!(
        attrs: many0!(Attribute::parse_outer) >>
        let_: keyword!(let) >>
        pats: call!(Punctuated::parse_separated_nonempty) >>
        ty: option!(tuple!(punct!(:), syn!(Type))) >>
        init: option!(tuple!(punct!(=), syn!(Expr))) >>
        semi: punct!(;) >>
        (Stmt::Local(Local {
            attrs: attrs,
            let_token: let_,
            pats: pats,
            ty: ty.map(|(colon, ty)| (colon, Box::new(ty))),
            init: init.map(|(eq, expr)| (eq, Box::new(expr))),
            semi_token: semi,
        }))
    ));

    #[cfg(feature = "full")]
    named!(stmt_item -> Stmt, map!(syn!(Item), |i| Stmt::Item(i)));

    #[cfg(feature = "full")]
    named!(stmt_blockexpr -> Stmt, do_parse!(
        attrs: many0!(Attribute::parse_outer) >>
        mut e: expr_nosemi >>
        // If the next token is a `.` or a `?` it is special-cased to parse as
        // an expression instead of a blockexpression.
        not!(punct!(.)) >>
        not!(punct!(?)) >>
        semi: option!(punct!(;)) >>
        ({
            e.replace_attrs(attrs);
            if let Some(semi) = semi {
                Stmt::Semi(e, semi)
            } else {
                Stmt::Expr(e)
            }
        })
    ));

    #[cfg(feature = "full")]
    named!(stmt_expr -> Stmt, do_parse!(
        attrs: many0!(Attribute::parse_outer) >>
        mut e: syn!(Expr) >>
        semi: punct!(;) >>
        ({
            e.replace_attrs(attrs);
            Stmt::Semi(e, semi)
        })
    ));

    #[cfg(feature = "full")]
    impl Synom for Pat {
        named!(parse -> Self, alt!(
            syn!(PatWild) => { Pat::Wild } // must be before pat_ident
            |
            syn!(PatBox) => { Pat::Box }  // must be before pat_ident
            |
            syn!(PatRange) => { Pat::Range } // must be before pat_lit
            |
            syn!(PatTupleStruct) => { Pat::TupleStruct }  // must be before pat_ident
            |
            syn!(PatStruct) => { Pat::Struct } // must be before pat_ident
            |
            syn!(PatMacro) => { Pat::Macro } // must be before pat_ident
            |
            syn!(PatLit) => { Pat::Lit } // must be before pat_ident
            |
            syn!(PatIdent) => { Pat::Ident } // must be before pat_path
            |
            syn!(PatPath) => { Pat::Path }
            |
            syn!(PatTuple) => { Pat::Tuple }
            |
            syn!(PatRef) => { Pat::Ref }
            |
            syn!(PatSlice) => { Pat::Slice }
        ));

        fn description() -> Option<&'static str> {
            Some("pattern")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for PatWild {
        named!(parse -> Self, map!(
            punct!(_),
            |u| PatWild { underscore_token: u }
        ));

        fn description() -> Option<&'static str> {
            Some("wild pattern: `_`")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for PatBox {
        named!(parse -> Self, do_parse!(
            boxed: keyword!(box) >>
            pat: syn!(Pat) >>
            (PatBox {
                pat: Box::new(pat),
                box_token: boxed,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("box pattern")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for PatIdent {
        named!(parse -> Self, do_parse!(
            by_ref: option!(keyword!(ref)) >>
            mutability: option!(keyword!(mut)) >>
            name: alt!(
                syn!(Ident)
                |
                keyword!(self) => { Into::into }
            ) >>
            not!(punct!(<)) >>
            not!(punct!(::)) >>
            subpat: option!(tuple!(punct!(@), syn!(Pat))) >>
            (PatIdent {
                by_ref: by_ref,
                mutability: mutability,
                ident: name,
                subpat: subpat.map(|(at, pat)| (at, Box::new(pat))),
            })
        ));

        fn description() -> Option<&'static str> {
            Some("pattern identifier binding")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for PatTupleStruct {
        named!(parse -> Self, do_parse!(
            path: syn!(Path) >>
            tuple: syn!(PatTuple) >>
            (PatTupleStruct {
                path: path,
                pat: tuple,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("tuple struct pattern")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for PatStruct {
        named!(parse -> Self, do_parse!(
            path: syn!(Path) >>
            data: braces!(do_parse!(
                fields: call!(Punctuated::parse_terminated) >>
                base: option!(cond!(fields.empty_or_trailing(), punct!(..))) >>
                (fields, base)
            )) >>
            (PatStruct {
                path: path,
                fields: (data.1).0,
                brace_token: data.0,
                dot2_token: (data.1).1.and_then(|m| m),
            })
        ));

        fn description() -> Option<&'static str> {
            Some("struct pattern")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for FieldPat {
        named!(parse -> Self, alt!(
            do_parse!(
                member: syn!(Member) >>
                colon: punct!(:) >>
                pat: syn!(Pat) >>
                (FieldPat {
                    member: member,
                    pat: Box::new(pat),
                    attrs: Vec::new(),
                    colon_token: Some(colon),
                })
            )
            |
            do_parse!(
                boxed: option!(keyword!(box)) >>
                by_ref: option!(keyword!(ref)) >>
                mutability: option!(keyword!(mut)) >>
                ident: syn!(Ident) >>
                ({
                    let mut pat: Pat = PatIdent {
                        by_ref: by_ref,
                        mutability: mutability,
                        ident: ident,
                        subpat: None,
                    }.into();
                    if let Some(boxed) = boxed {
                        pat = PatBox {
                            pat: Box::new(pat),
                            box_token: boxed,
                        }.into();
                    }
                    FieldPat {
                        member: Member::Named(ident),
                        pat: Box::new(pat),
                        attrs: Vec::new(),
                        colon_token: None,
                    }
                })
            )
        ));

        fn description() -> Option<&'static str> {
            Some("field pattern")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for Member {
        named!(parse -> Self, alt!(
            syn!(Ident) => { Member::Named }
            |
            syn!(Index) => { Member::Unnamed }
        ));

        fn description() -> Option<&'static str> {
            Some("field member")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for Index {
        named!(parse -> Self, do_parse!(
            lit: syn!(LitInt) >>
            ({
                if let IntSuffix::None = lit.suffix() {
                    Index { index: lit.value() as u32, span: lit.span() }
                } else {
                    return parse_error();
                }
            })
        ));

        fn description() -> Option<&'static str> {
            Some("field index")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for PatPath {
        named!(parse -> Self, map!(
            syn!(ExprPath),
            |p| PatPath { qself: p.qself, path: p.path }
        ));

        fn description() -> Option<&'static str> {
            Some("path pattern")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for PatTuple {
        named!(parse -> Self, do_parse!(
            data: parens!(do_parse!(
                front: call!(Punctuated::parse_terminated) >>
                dotdot: option!(cond_reduce!(front.empty_or_trailing(),
                    tuple!(punct!(..), option!(punct!(,)))
                )) >>
                back: cond!(match dotdot {
                                Some((_, Some(_))) => true,
                                _ => false,
                            },
                            Punctuated::parse_terminated) >>
                (front, dotdot, back)
            )) >>
            ({
                let (parens, (front, dotdot, back)) = data;
                let (dotdot, trailing) = match dotdot {
                    Some((a, b)) => (Some(a), Some(b)),
                    None => (None, None),
                };
                PatTuple {
                    paren_token: parens,
                    front: front,
                    dot2_token: dotdot,
                    comma_token: trailing.unwrap_or_default(),
                    back: back.unwrap_or_default(),
                }
            })
        ));

        fn description() -> Option<&'static str> {
            Some("tuple pattern")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for PatRef {
        named!(parse -> Self, do_parse!(
            and: punct!(&) >>
            mutability: option!(keyword!(mut)) >>
            pat: syn!(Pat) >>
            (PatRef {
                pat: Box::new(pat),
                mutability: mutability,
                and_token: and,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("reference pattern")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for PatLit {
        named!(parse -> Self, do_parse!(
            lit: pat_lit_expr >>
            (if let Expr::Path(_) = lit {
                return parse_error(); // these need to be parsed by pat_path
            } else {
                PatLit {
                    expr: Box::new(lit),
                }
            })
        ));

        fn description() -> Option<&'static str> {
            Some("literal pattern")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for PatRange {
        named!(parse -> Self, do_parse!(
            lo: pat_lit_expr >>
            limits: syn!(RangeLimits) >>
            hi: pat_lit_expr >>
            (PatRange {
                lo: Box::new(lo),
                hi: Box::new(hi),
                limits: limits,
            })
        ));

        fn description() -> Option<&'static str> {
            Some("range pattern")
        }
    }

    #[cfg(feature = "full")]
    named!(pat_lit_expr -> Expr, do_parse!(
        neg: option!(punct!(-)) >>
        v: alt!(
            syn!(ExprLit) => { Expr::Lit }
            |
            syn!(ExprPath) => { Expr::Path }
        ) >>
        (if let Some(neg) = neg {
            Expr::Unary(ExprUnary {
                attrs: Vec::new(),
                op: UnOp::Neg(neg),
                expr: Box::new(v)
            })
        } else {
            v
        })
    ));

    #[cfg(feature = "full")]
    impl Synom for PatSlice {
        named!(parse -> Self, map!(
            brackets!(do_parse!(
                before: call!(Punctuated::parse_terminated) >>
                middle: option!(do_parse!(
                    dots: punct!(..) >>
                    trailing: option!(punct!(,)) >>
                    (dots, trailing)
                )) >>
                after: cond!(
                    match middle {
                        Some((_, ref trailing)) => trailing.is_some(),
                        _ => false,
                    },
                    Punctuated::parse_terminated
                ) >>
                (before, middle, after)
            )),
            |(brackets, (before, middle, after))| {
                let mut before: Punctuated<Pat, Token![,]> = before;
                let after: Option<Punctuated<Pat, Token![,]>> = after;
                let middle: Option<(Token![..], Option<Token![,]>)> = middle;
                PatSlice {
                    dot2_token: middle.as_ref().map(|m| Token![..]((m.0).0)),
                    comma_token: middle.as_ref().and_then(|m| {
                        m.1.as_ref().map(|m| Token![,](m.0))
                    }),
                    bracket_token: brackets,
                    middle: middle.and_then(|_| {
                        if before.empty_or_trailing() {
                            None
                        } else {
                            Some(Box::new(before.pop().unwrap().into_value()))
                        }
                    }),
                    front: before,
                    back: after.unwrap_or_default(),
                }
            }
        ));

        fn description() -> Option<&'static str> {
            Some("slice pattern")
        }
    }

    #[cfg(feature = "full")]
    impl Synom for PatMacro {
        named!(parse -> Self, map!(syn!(Macro), |mac| PatMacro { mac: mac }));

        fn description() -> Option<&'static str> {
            Some("macro pattern")
        }
    }
}

#[cfg(feature = "printing")]
mod printing {
    use super::*;
    #[cfg(feature = "full")]
    use attr::FilterAttrs;
    use quote::{ToTokens, Tokens};
    use proc_macro2::Literal;

    // If the given expression is a bare `ExprStruct`, wraps it in parenthesis
    // before appending it to `Tokens`.
    #[cfg(feature = "full")]
    fn wrap_bare_struct(tokens: &mut Tokens, e: &Expr) {
        if let Expr::Struct(_) = *e {
            token::Paren::default().surround(tokens, |tokens| {
                e.to_tokens(tokens);
            });
        } else {
            e.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    fn attrs_to_tokens(attrs: &[Attribute], tokens: &mut Tokens) {
        tokens.append_all(attrs.outer());
    }

    #[cfg(not(feature = "full"))]
    fn attrs_to_tokens(_attrs: &[Attribute], _tokens: &mut Tokens) {}

    #[cfg(feature = "full")]
    impl ToTokens for ExprBox {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.box_token.to_tokens(tokens);
            self.expr.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprInPlace {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.place.to_tokens(tokens);
            self.arrow_token.to_tokens(tokens);
            self.value.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprArray {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.bracket_token.surround(tokens, |tokens| {
                self.elems.to_tokens(tokens);
            })
        }
    }

    impl ToTokens for ExprCall {
        fn to_tokens(&self, tokens: &mut Tokens) {
            attrs_to_tokens(&self.attrs, tokens);
            self.func.to_tokens(tokens);
            self.paren_token.surround(tokens, |tokens| {
                self.args.to_tokens(tokens);
            })
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprMethodCall {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.receiver.to_tokens(tokens);
            self.dot_token.to_tokens(tokens);
            self.method.to_tokens(tokens);
            self.turbofish.to_tokens(tokens);
            self.paren_token.surround(tokens, |tokens| {
                self.args.to_tokens(tokens);
            });
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for MethodTurbofish {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.colon2_token.to_tokens(tokens);
            self.lt_token.to_tokens(tokens);
            self.args.to_tokens(tokens);
            self.gt_token.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for GenericMethodArgument {
        fn to_tokens(&self, tokens: &mut Tokens) {
            match *self {
                GenericMethodArgument::Type(ref t) => t.to_tokens(tokens),
                GenericMethodArgument::Const(ref c) => c.to_tokens(tokens),
            }
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprTuple {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.paren_token.surround(tokens, |tokens| {
                self.elems.to_tokens(tokens);
                // If we only have one argument, we need a trailing comma to
                // distinguish ExprTuple from ExprParen.
                if self.elems.len() == 1 && !self.elems.trailing_punct() {
                    <Token![,]>::default().to_tokens(tokens);
                }
            })
        }
    }

    impl ToTokens for ExprBinary {
        fn to_tokens(&self, tokens: &mut Tokens) {
            attrs_to_tokens(&self.attrs, tokens);
            self.left.to_tokens(tokens);
            self.op.to_tokens(tokens);
            self.right.to_tokens(tokens);
        }
    }

    impl ToTokens for ExprUnary {
        fn to_tokens(&self, tokens: &mut Tokens) {
            attrs_to_tokens(&self.attrs, tokens);
            self.op.to_tokens(tokens);
            self.expr.to_tokens(tokens);
        }
    }

    impl ToTokens for ExprLit {
        fn to_tokens(&self, tokens: &mut Tokens) {
            attrs_to_tokens(&self.attrs, tokens);
            self.lit.to_tokens(tokens);
        }
    }

    impl ToTokens for ExprCast {
        fn to_tokens(&self, tokens: &mut Tokens) {
            attrs_to_tokens(&self.attrs, tokens);
            self.expr.to_tokens(tokens);
            self.as_token.to_tokens(tokens);
            self.ty.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprType {
        fn to_tokens(&self, tokens: &mut Tokens) {
            attrs_to_tokens(&self.attrs, tokens);
            self.expr.to_tokens(tokens);
            self.colon_token.to_tokens(tokens);
            self.ty.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    fn maybe_wrap_else(tokens: &mut Tokens, else_: &Option<(Token![else], Box<Expr>)>) {
        if let Some((ref else_token, ref else_)) = *else_ {
            else_token.to_tokens(tokens);

            // If we are not one of the valid expressions to exist in an else
            // clause, wrap ourselves in a block.
            match **else_ {
                Expr::If(_) | Expr::IfLet(_) | Expr::Block(_) => {
                    else_.to_tokens(tokens);
                }
                _ => {
                    token::Brace::default().surround(tokens, |tokens| {
                        else_.to_tokens(tokens);
                    });
                }
            }
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprIf {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.if_token.to_tokens(tokens);
            wrap_bare_struct(tokens, &self.cond);
            self.then_branch.to_tokens(tokens);
            maybe_wrap_else(tokens, &self.else_branch);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprIfLet {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.if_token.to_tokens(tokens);
            self.let_token.to_tokens(tokens);
            self.pats.to_tokens(tokens);
            self.eq_token.to_tokens(tokens);
            wrap_bare_struct(tokens, &self.expr);
            self.then_branch.to_tokens(tokens);
            maybe_wrap_else(tokens, &self.else_branch);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprWhile {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.label.to_tokens(tokens);
            self.while_token.to_tokens(tokens);
            wrap_bare_struct(tokens, &self.cond);
            self.body.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprWhileLet {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.label.to_tokens(tokens);
            self.while_token.to_tokens(tokens);
            self.let_token.to_tokens(tokens);
            self.pats.to_tokens(tokens);
            self.eq_token.to_tokens(tokens);
            wrap_bare_struct(tokens, &self.expr);
            self.body.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprForLoop {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.label.to_tokens(tokens);
            self.for_token.to_tokens(tokens);
            self.pat.to_tokens(tokens);
            self.in_token.to_tokens(tokens);
            wrap_bare_struct(tokens, &self.expr);
            self.body.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprLoop {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.label.to_tokens(tokens);
            self.loop_token.to_tokens(tokens);
            self.body.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprMatch {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.match_token.to_tokens(tokens);
            wrap_bare_struct(tokens, &self.expr);
            self.brace_token.surround(tokens, |tokens| {
                for (i, arm) in self.arms.iter().enumerate() {
                    arm.to_tokens(tokens);
                    // Ensure that we have a comma after a non-block arm, except
                    // for the last one.
                    let is_last = i == self.arms.len() - 1;
                    if !is_last && arm_expr_requires_comma(&arm.body) && arm.comma.is_none() {
                        <Token![,]>::default().to_tokens(tokens);
                    }
                }
            });
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprCatch {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.do_token.to_tokens(tokens);
            self.catch_token.to_tokens(tokens);
            self.block.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprYield {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.yield_token.to_tokens(tokens);
            self.expr.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprClosure {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.movability.to_tokens(tokens);
            self.capture.to_tokens(tokens);
            self.or1_token.to_tokens(tokens);
            for input in self.inputs.pairs() {
                match **input.value() {
                    FnArg::Captured(ArgCaptured {
                        ref pat,
                        ty: Type::Infer(_),
                        ..
                    }) => {
                        pat.to_tokens(tokens);
                    }
                    _ => input.value().to_tokens(tokens),
                }
                input.punct().to_tokens(tokens);
            }
            self.or2_token.to_tokens(tokens);
            self.output.to_tokens(tokens);
            self.body.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprUnsafe {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.unsafe_token.to_tokens(tokens);
            self.block.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprBlock {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.block.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprAssign {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.left.to_tokens(tokens);
            self.eq_token.to_tokens(tokens);
            self.right.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprAssignOp {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.left.to_tokens(tokens);
            self.op.to_tokens(tokens);
            self.right.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprField {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.base.to_tokens(tokens);
            self.dot_token.to_tokens(tokens);
            self.member.to_tokens(tokens);
        }
    }

    impl ToTokens for Member {
        fn to_tokens(&self, tokens: &mut Tokens) {
            match *self {
                Member::Named(ident) => ident.to_tokens(tokens),
                Member::Unnamed(ref index) => index.to_tokens(tokens),
            }
        }
    }

    impl ToTokens for Index {
        fn to_tokens(&self, tokens: &mut Tokens) {
            let mut lit = Literal::i64_unsuffixed(i64::from(self.index));
            lit.set_span(self.span);
            tokens.append(lit);
        }
    }

    impl ToTokens for ExprIndex {
        fn to_tokens(&self, tokens: &mut Tokens) {
            attrs_to_tokens(&self.attrs, tokens);
            self.expr.to_tokens(tokens);
            self.bracket_token.surround(tokens, |tokens| {
                self.index.to_tokens(tokens);
            });
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprRange {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.from.to_tokens(tokens);
            match self.limits {
                RangeLimits::HalfOpen(ref t) => t.to_tokens(tokens),
                RangeLimits::Closed(ref t) => t.to_tokens(tokens),
            }
            self.to.to_tokens(tokens);
        }
    }

    impl ToTokens for ExprPath {
        fn to_tokens(&self, tokens: &mut Tokens) {
            attrs_to_tokens(&self.attrs, tokens);
            ::PathTokens(&self.qself, &self.path).to_tokens(tokens)
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprReference {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.and_token.to_tokens(tokens);
            self.mutability.to_tokens(tokens);
            self.expr.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprBreak {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.break_token.to_tokens(tokens);
            self.label.to_tokens(tokens);
            self.expr.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprContinue {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.continue_token.to_tokens(tokens);
            self.label.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprReturn {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.return_token.to_tokens(tokens);
            self.expr.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprMacro {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.mac.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprStruct {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.path.to_tokens(tokens);
            self.brace_token.surround(tokens, |tokens| {
                self.fields.to_tokens(tokens);
                if self.rest.is_some() {
                    TokensOrDefault(&self.dot2_token).to_tokens(tokens);
                    self.rest.to_tokens(tokens);
                }
            })
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprRepeat {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.bracket_token.surround(tokens, |tokens| {
                self.expr.to_tokens(tokens);
                self.semi_token.to_tokens(tokens);
                self.len.to_tokens(tokens);
            })
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprGroup {
        fn to_tokens(&self, tokens: &mut Tokens) {
            attrs_to_tokens(&self.attrs, tokens);
            self.group_token.surround(tokens, |tokens| {
                self.expr.to_tokens(tokens);
            });
        }
    }

    impl ToTokens for ExprParen {
        fn to_tokens(&self, tokens: &mut Tokens) {
            attrs_to_tokens(&self.attrs, tokens);
            self.paren_token.surround(tokens, |tokens| {
                self.expr.to_tokens(tokens);
            });
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for ExprTry {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.expr.to_tokens(tokens);
            self.question_token.to_tokens(tokens);
        }
    }

    impl ToTokens for ExprVerbatim {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.tts.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for Label {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.name.to_tokens(tokens);
            self.colon_token.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for FieldValue {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.member.to_tokens(tokens);
            if let Some(ref colon_token) = self.colon_token {
                colon_token.to_tokens(tokens);
                self.expr.to_tokens(tokens);
            }
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for Arm {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(&self.attrs);
            self.leading_vert.to_tokens(tokens);
            self.pats.to_tokens(tokens);
            if let Some((ref if_token, ref guard)) = self.guard {
                if_token.to_tokens(tokens);
                guard.to_tokens(tokens);
            }
            self.fat_arrow_token.to_tokens(tokens);
            self.body.to_tokens(tokens);
            self.comma.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for PatWild {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.underscore_token.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for PatIdent {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.by_ref.to_tokens(tokens);
            self.mutability.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            if let Some((ref at_token, ref subpat)) = self.subpat {
                at_token.to_tokens(tokens);
                subpat.to_tokens(tokens);
            }
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for PatStruct {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.path.to_tokens(tokens);
            self.brace_token.surround(tokens, |tokens| {
                self.fields.to_tokens(tokens);
                // NOTE: We need a comma before the dot2 token if it is present.
                if !self.fields.empty_or_trailing() && self.dot2_token.is_some() {
                    <Token![,]>::default().to_tokens(tokens);
                }
                self.dot2_token.to_tokens(tokens);
            });
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for PatTupleStruct {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.path.to_tokens(tokens);
            self.pat.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for PatPath {
        fn to_tokens(&self, tokens: &mut Tokens) {
            ::PathTokens(&self.qself, &self.path).to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for PatTuple {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.paren_token.surround(tokens, |tokens| {
                self.front.to_tokens(tokens);
                if let Some(ref dot2_token) = self.dot2_token {
                    if !self.front.empty_or_trailing() {
                        // Ensure there is a comma before the .. token.
                        <Token![,]>::default().to_tokens(tokens);
                    }
                    dot2_token.to_tokens(tokens);
                    self.comma_token.to_tokens(tokens);
                    if self.comma_token.is_none() && !self.back.is_empty() {
                        // Ensure there is a comma after the .. token.
                        <Token![,]>::default().to_tokens(tokens);
                    }
                }
                self.back.to_tokens(tokens);
            });
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for PatBox {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.box_token.to_tokens(tokens);
            self.pat.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for PatRef {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.and_token.to_tokens(tokens);
            self.mutability.to_tokens(tokens);
            self.pat.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for PatLit {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.expr.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for PatRange {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.lo.to_tokens(tokens);
            match self.limits {
                RangeLimits::HalfOpen(ref t) => t.to_tokens(tokens),
                RangeLimits::Closed(ref t) => Token![...](t.0).to_tokens(tokens),
            }
            self.hi.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for PatSlice {
        fn to_tokens(&self, tokens: &mut Tokens) {
            // XXX: This is a mess, and it will be so easy to screw it up. How
            // do we make this correct itself better?
            self.bracket_token.surround(tokens, |tokens| {
                self.front.to_tokens(tokens);

                // If we need a comma before the middle or standalone .. token,
                // then make sure it's present.
                if !self.front.empty_or_trailing()
                    && (self.middle.is_some() || self.dot2_token.is_some())
                {
                    <Token![,]>::default().to_tokens(tokens);
                }

                // If we have an identifier, we always need a .. token.
                if self.middle.is_some() {
                    self.middle.to_tokens(tokens);
                    TokensOrDefault(&self.dot2_token).to_tokens(tokens);
                } else if self.dot2_token.is_some() {
                    self.dot2_token.to_tokens(tokens);
                }

                // Make sure we have a comma before the back half.
                if !self.back.is_empty() {
                    TokensOrDefault(&self.comma_token).to_tokens(tokens);
                    self.back.to_tokens(tokens);
                } else {
                    self.comma_token.to_tokens(tokens);
                }
            })
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for PatMacro {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.mac.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for PatVerbatim {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.tts.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for FieldPat {
        fn to_tokens(&self, tokens: &mut Tokens) {
            if let Some(ref colon_token) = self.colon_token {
                self.member.to_tokens(tokens);
                colon_token.to_tokens(tokens);
            }
            self.pat.to_tokens(tokens);
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for Block {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.brace_token.surround(tokens, |tokens| {
                tokens.append_all(&self.stmts);
            });
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for Stmt {
        fn to_tokens(&self, tokens: &mut Tokens) {
            match *self {
                Stmt::Local(ref local) => local.to_tokens(tokens),
                Stmt::Item(ref item) => item.to_tokens(tokens),
                Stmt::Expr(ref expr) => expr.to_tokens(tokens),
                Stmt::Semi(ref expr, ref semi) => {
                    expr.to_tokens(tokens);
                    semi.to_tokens(tokens);
                }
            }
        }
    }

    #[cfg(feature = "full")]
    impl ToTokens for Local {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.let_token.to_tokens(tokens);
            self.pats.to_tokens(tokens);
            if let Some((ref colon_token, ref ty)) = self.ty {
                colon_token.to_tokens(tokens);
                ty.to_tokens(tokens);
            }
            if let Some((ref eq_token, ref init)) = self.init {
                eq_token.to_tokens(tokens);
                init.to_tokens(tokens);
            }
            self.semi_token.to_tokens(tokens);
        }
    }
}
