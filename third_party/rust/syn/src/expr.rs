use super::*;

/// An expression.
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct Expr {
    /// Type of the expression.
    pub node: ExprKind,

    /// Attributes tagged on the expression.
    pub attrs: Vec<Attribute>,
}

impl From<ExprKind> for Expr {
    fn from(node: ExprKind) -> Expr {
        Expr {
            node: node,
            attrs: Vec::new(),
        }
    }
}

#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub enum ExprKind {
    /// A `box x` expression.
    Box(Box<Expr>),

    /// First expr is the place; second expr is the value.
    ///
    /// E.g. 'plae <- val'.
    InPlace(Box<Expr>, Box<Expr>),

    /// An array, e.g. `[a, b, c, d]`.
    Array(Vec<Expr>),

    /// A function call.
    ///
    /// The first field resolves to the function itself,
    /// and the second field is the list of arguments
    Call(Box<Expr>, Vec<Expr>),

    /// A method call (`x.foo::<Bar, Baz>(a, b, c, d)`)
    ///
    /// The `Ident` is the identifier for the method name.
    /// The vector of `Ty`s are the ascripted type parameters for the method
    /// (within the angle brackets).
    ///
    /// The first element of the vector of `Expr`s is the expression that evaluates
    /// to the object on which the method is being called on (the receiver),
    /// and the remaining elements are the rest of the arguments.
    ///
    /// Thus, `x.foo::<Bar, Baz>(a, b, c, d)` is represented as
    /// `ExprKind::MethodCall(foo, [Bar, Baz], [x, a, b, c, d])`.
    MethodCall(Ident, Vec<Ty>, Vec<Expr>),

    /// A tuple, e.g. `(a, b, c, d)`.
    Tup(Vec<Expr>),

    /// A binary operation, e.g. `a + b`, `a * b`.
    Binary(BinOp, Box<Expr>, Box<Expr>),

    /// A unary operation, e.g. `!x`, `*x`.
    Unary(UnOp, Box<Expr>),

    /// A literal, e.g. `1`, `"foo"`.
    Lit(Lit),

    /// A cast, e.g. `foo as f64`.
    Cast(Box<Expr>, Box<Ty>),

    /// A type ascription, e.g. `foo: f64`.
    Type(Box<Expr>, Box<Ty>),

    /// An `if` block, with an optional else block
    ///
    /// E.g., `if expr { block } else { expr }`
    If(Box<Expr>, Block, Option<Box<Expr>>),

    /// An `if let` expression with an optional else block
    ///
    /// E.g., `if let pat = expr { block } else { expr }`
    ///
    /// This is desugared to a `match` expression.
    IfLet(Box<Pat>, Box<Expr>, Block, Option<Box<Expr>>),

    /// A while loop, with an optional label
    ///
    /// E.g., `'label: while expr { block }`
    While(Box<Expr>, Block, Option<Ident>),

    /// A while-let loop, with an optional label.
    ///
    /// E.g., `'label: while let pat = expr { block }`
    ///
    /// This is desugared to a combination of `loop` and `match` expressions.
    WhileLet(Box<Pat>, Box<Expr>, Block, Option<Ident>),

    /// A for loop, with an optional label.
    ///
    /// E.g., `'label: for pat in expr { block }`
    ///
    /// This is desugared to a combination of `loop` and `match` expressions.
    ForLoop(Box<Pat>, Box<Expr>, Block, Option<Ident>),

    /// Conditionless loop with an optional label.
    ///
    /// E.g. `'label: loop { block }`
    Loop(Block, Option<Ident>),

    /// A `match` block.
    Match(Box<Expr>, Vec<Arm>),

    /// A closure (for example, `move |a, b, c| a + b + c`)
    Closure(CaptureBy, Box<FnDecl>, Box<Expr>),

    /// A block (`{ ... }` or `unsafe { ... }`)
    Block(Unsafety, Block),

    /// An assignment (`a = foo()`)
    Assign(Box<Expr>, Box<Expr>),

    /// An assignment with an operator
    ///
    /// For example, `a += 1`.
    AssignOp(BinOp, Box<Expr>, Box<Expr>),

    /// Access of a named struct field (`obj.foo`)
    Field(Box<Expr>, Ident),

    /// Access of an unnamed field of a struct or tuple-struct
    ///
    /// For example, `foo.0`.
    TupField(Box<Expr>, usize),

    /// An indexing operation (`foo[2]`)
    Index(Box<Expr>, Box<Expr>),

    /// A range (`1..2`, `1..`, `..2`, `1...2`, `1...`, `...2`)
    Range(Option<Box<Expr>>, Option<Box<Expr>>, RangeLimits),

    /// Variable reference, possibly containing `::` and/or type
    /// parameters, e.g. foo::bar::<baz>.
    ///
    /// Optionally "qualified",
    /// E.g. `<Vec<T> as SomeTrait>::SomeType`.
    Path(Option<QSelf>, Path),

    /// A referencing operation (`&a` or `&mut a`)
    AddrOf(Mutability, Box<Expr>),

    /// A `break`, with an optional label to break, and an optional expression
    Break(Option<Ident>, Option<Box<Expr>>),

    /// A `continue`, with an optional label
    Continue(Option<Ident>),

    /// A `return`, with an optional value to be returned
    Ret(Option<Box<Expr>>),

    /// A macro invocation; pre-expansion
    Mac(Mac),

    /// A struct literal expression.
    ///
    /// For example, `Foo {x: 1, y: 2}`, or
    /// `Foo {x: 1, .. base}`, where `base` is the `Option<Expr>`.
    Struct(Path, Vec<FieldValue>, Option<Box<Expr>>),

    /// An array literal constructed from one repeated element.
    ///
    /// For example, `[1; 5]`. The first expression is the element
    /// to be repeated; the second is the number of times to repeat it.
    Repeat(Box<Expr>, Box<Expr>),

    /// No-op: used solely so we can pretty-print faithfully
    Paren(Box<Expr>),

    /// `expr?`
    Try(Box<Expr>),
}

/// A field-value pair in a struct literal.
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct FieldValue {
    /// Name of the field.
    pub ident: Ident,

    /// Value of the field.
    pub expr: Expr,

    /// Whether this is a shorthand field, e.g. `Struct { x }`
    /// instead of `Struct { x: x }`.
    pub is_shorthand: bool,

    /// Attributes tagged on the field.
    pub attrs: Vec<Attribute>,
}

/// A Block (`{ .. }`).
///
/// E.g. `{ .. }` as in `fn foo() { .. }`
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct Block {
    /// Statements in a block
    pub stmts: Vec<Stmt>,
}

/// A statement, usually ending in a semicolon.
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub enum Stmt {
    /// A local (let) binding.
    Local(Box<Local>),

    /// An item definition.
    Item(Box<Item>),

    /// Expr without trailing semicolon.
    Expr(Box<Expr>),

    /// Expression with trailing semicolon;
    Semi(Box<Expr>),

    /// Macro invocation.
    Mac(Box<(Mac, MacStmtStyle, Vec<Attribute>)>),
}

/// How a macro was invoked.
#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub enum MacStmtStyle {
    /// The macro statement had a trailing semicolon, e.g. `foo! { ... };`
    /// `foo!(...);`, `foo![...];`
    Semicolon,

    /// The macro statement had braces; e.g. foo! { ... }
    Braces,

    /// The macro statement had parentheses or brackets and no semicolon; e.g.
    /// `foo!(...)`. All of these will end up being converted into macro
    /// expressions.
    NoBraces,
}

/// Local represents a `let` statement, e.g., `let <pat>:<ty> = <expr>;`
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct Local {
    pub pat: Box<Pat>,
    pub ty: Option<Box<Ty>>,

    /// Initializer expression to set the value, if any
    pub init: Option<Box<Expr>>,
    pub attrs: Vec<Attribute>,
}

#[derive(Debug, Clone, Eq, PartialEq, Hash)]
// Clippy false positive
// https://github.com/Manishearth/rust-clippy/issues/1241
#[cfg_attr(feature = "cargo-clippy", allow(enum_variant_names))]
pub enum Pat {
    /// Represents a wildcard pattern (`_`)
    Wild,

    /// A `Pat::Ident` may either be a new bound variable (`ref mut binding @ OPT_SUBPATTERN`),
    /// or a unit struct/variant pattern, or a const pattern (in the last two cases the third
    /// field must be `None`). Disambiguation cannot be done with parser alone, so it happens
    /// during name resolution.
    Ident(BindingMode, Ident, Option<Box<Pat>>),

    /// A struct or struct variant pattern, e.g. `Variant {x, y, ..}`.
    /// The `bool` is `true` in the presence of a `..`.
    Struct(Path, Vec<FieldPat>, bool),

    /// A tuple struct/variant pattern `Variant(x, y, .., z)`.
    /// If the `..` pattern fragment is present, then `Option<usize>` denotes its position.
    /// 0 <= position <= subpats.len()
    TupleStruct(Path, Vec<Pat>, Option<usize>),

    /// A possibly qualified path pattern.
    /// Unquailfied path patterns `A::B::C` can legally refer to variants, structs, constants
    /// or associated constants. Quailfied path patterns `<A>::B::C`/`<A as Trait>::B::C` can
    /// only legally refer to associated constants.
    Path(Option<QSelf>, Path),

    /// A tuple pattern `(a, b)`.
    /// If the `..` pattern fragment is present, then `Option<usize>` denotes its position.
    /// 0 <= position <= subpats.len()
    Tuple(Vec<Pat>, Option<usize>),
    /// A `box` pattern
    Box(Box<Pat>),
    /// A reference pattern, e.g. `&mut (a, b)`
    Ref(Box<Pat>, Mutability),
    /// A literal
    Lit(Box<Expr>),
    /// A range pattern, e.g. `1...2`
    Range(Box<Expr>, Box<Expr>),
    /// `[a, b, ..i, y, z]` is represented as:
    ///     `Pat::Slice(box [a, b], Some(i), box [y, z])`
    Slice(Vec<Pat>, Option<Box<Pat>>, Vec<Pat>),
    /// A macro pattern; pre-expansion
    Mac(Mac),
}

/// An arm of a 'match'.
///
/// E.g. `0...10 => { println!("match!") }` as in
///
/// ```rust,ignore
/// match n {
///     0...10 => { println!("match!") },
///     // ..
/// }
/// ```
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct Arm {
    pub attrs: Vec<Attribute>,
    pub pats: Vec<Pat>,
    pub guard: Option<Box<Expr>>,
    pub body: Box<Expr>,
}

/// A capture clause
#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub enum CaptureBy {
    Value,
    Ref,
}

/// Limit types of a range (inclusive or exclusive)
#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub enum RangeLimits {
    /// Inclusive at the beginning, exclusive at the end
    HalfOpen,
    /// Inclusive at the beginning and end
    Closed,
}

/// A single field in a struct pattern
///
/// Patterns like the fields of Foo `{ x, ref y, ref mut z }`
/// are treated the same as `x: x, y: ref y, z: ref mut z`,
/// except `is_shorthand` is true
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct FieldPat {
    /// The identifier for the field
    pub ident: Ident,
    /// The pattern the field is destructured to
    pub pat: Box<Pat>,
    pub is_shorthand: bool,
    pub attrs: Vec<Attribute>,
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub enum BindingMode {
    ByRef(Mutability),
    ByValue(Mutability),
}

#[cfg(feature = "parsing")]
pub mod parsing {
    use super::*;
    use {BinOp, Delimited, DelimToken, FnArg, FnDecl, FunctionRetTy, Ident, Lifetime, Mac,
         TokenTree, Ty, UnOp, Unsafety};
    use attr::parsing::outer_attr;
    use generics::parsing::lifetime;
    use ident::parsing::{ident, wordlike};
    use item::parsing::item;
    use lit::parsing::{digits, lit};
    use mac::parsing::{mac, token_trees};
    use synom::IResult::{self, Error};
    use op::parsing::{assign_op, binop, unop};
    use ty::parsing::{mutability, path, qpath, ty, unsafety};

    // Struct literals are ambiguous in certain positions
    // https://github.com/rust-lang/rfcs/pull/92
    macro_rules! named_ambiguous_expr {
        ($name:ident -> $o:ty, $allow_struct:ident, $submac:ident!( $($args:tt)* )) => {
            fn $name(i: &str, $allow_struct: bool) -> $crate::synom::IResult<&str, $o> {
                $submac!(i, $($args)*)
            }
        };
    }

    macro_rules! ambiguous_expr {
        ($i:expr, $allow_struct:ident) => {
            ambiguous_expr($i, $allow_struct, true)
        };
    }

    named!(pub expr -> Expr, ambiguous_expr!(true));

    named!(expr_no_struct -> Expr, ambiguous_expr!(false));

    #[cfg_attr(feature = "cargo-clippy", allow(cyclomatic_complexity))]
    fn ambiguous_expr(i: &str, allow_struct: bool, allow_block: bool) -> IResult<&str, Expr> {
        do_parse!(
            i,
            mut e: alt!(
                expr_lit // must be before expr_struct
                |
                cond_reduce!(allow_struct, expr_struct) // must be before expr_path
                |
                expr_paren // must be before expr_tup
                |
                expr_mac // must be before expr_path
                |
                call!(expr_break, allow_struct) // must be before expr_path
                |
                expr_continue // must be before expr_path
                |
                call!(expr_ret, allow_struct) // must be before expr_path
                |
                call!(expr_box, allow_struct)
                |
                expr_in_place
                |
                expr_array
                |
                expr_tup
                |
                call!(expr_unary, allow_struct)
                |
                expr_if
                |
                expr_while
                |
                expr_for_loop
                |
                expr_loop
                |
                expr_match
                |
                call!(expr_closure, allow_struct)
                |
                cond_reduce!(allow_block, expr_block)
                |
                call!(expr_range, allow_struct)
                |
                expr_path
                |
                call!(expr_addr_of, allow_struct)
                |
                expr_repeat
            ) >>
            many0!(alt!(
                tap!(args: and_call => {
                    e = ExprKind::Call(Box::new(e.into()), args);
                })
                |
                tap!(more: and_method_call => {
                    let (method, ascript, mut args) = more;
                    args.insert(0, e.into());
                    e = ExprKind::MethodCall(method, ascript, args);
                })
                |
                tap!(more: call!(and_binary, allow_struct) => {
                    let (op, other) = more;
                    e = ExprKind::Binary(op, Box::new(e.into()), Box::new(other));
                })
                |
                tap!(ty: and_cast => {
                    e = ExprKind::Cast(Box::new(e.into()), Box::new(ty));
                })
                |
                tap!(ty: and_ascription => {
                    e = ExprKind::Type(Box::new(e.into()), Box::new(ty));
                })
                |
                tap!(v: call!(and_assign, allow_struct) => {
                    e = ExprKind::Assign(Box::new(e.into()), Box::new(v));
                })
                |
                tap!(more: call!(and_assign_op, allow_struct) => {
                    let (op, v) = more;
                    e = ExprKind::AssignOp(op, Box::new(e.into()), Box::new(v));
                })
                |
                tap!(field: and_field => {
                    e = ExprKind::Field(Box::new(e.into()), field);
                })
                |
                tap!(field: and_tup_field => {
                    e = ExprKind::TupField(Box::new(e.into()), field as usize);
                })
                |
                tap!(i: and_index => {
                    e = ExprKind::Index(Box::new(e.into()), Box::new(i));
                })
                |
                tap!(more: call!(and_range, allow_struct) => {
                    let (limits, hi) = more;
                    e = ExprKind::Range(Some(Box::new(e.into())), hi.map(Box::new), limits);
                })
                |
                tap!(_try: punct!("?") => {
                    e = ExprKind::Try(Box::new(e.into()));
                })
            )) >>
            (e.into())
        )
    }

    named!(expr_mac -> ExprKind, map!(mac, ExprKind::Mac));

    named!(expr_paren -> ExprKind, do_parse!(
        punct!("(") >>
        e: expr >>
        punct!(")") >>
        (ExprKind::Paren(Box::new(e)))
    ));

    named_ambiguous_expr!(expr_box -> ExprKind, allow_struct, do_parse!(
        keyword!("box") >>
        inner: ambiguous_expr!(allow_struct) >>
        (ExprKind::Box(Box::new(inner)))
    ));

    named!(expr_in_place -> ExprKind, do_parse!(
        keyword!("in") >>
        place: expr_no_struct >>
        punct!("{") >>
        value: within_block >>
        punct!("}") >>
        (ExprKind::InPlace(
            Box::new(place),
            Box::new(ExprKind::Block(Unsafety::Normal, Block {
                stmts: value,
            }).into()),
        ))
    ));

    named!(expr_array -> ExprKind, do_parse!(
        punct!("[") >>
        elems: terminated_list!(punct!(","), expr) >>
        punct!("]") >>
        (ExprKind::Array(elems))
    ));

    named!(and_call -> Vec<Expr>, do_parse!(
        punct!("(") >>
        args: terminated_list!(punct!(","), expr) >>
        punct!(")") >>
        (args)
    ));

    named!(and_method_call -> (Ident, Vec<Ty>, Vec<Expr>), do_parse!(
        punct!(".") >>
        method: ident >>
        ascript: opt_vec!(preceded!(
            punct!("::"),
            delimited!(
                punct!("<"),
                terminated_list!(punct!(","), ty),
                punct!(">")
            )
        )) >>
        punct!("(") >>
        args: terminated_list!(punct!(","), expr) >>
        punct!(")") >>
        (method, ascript, args)
    ));

    named!(expr_tup -> ExprKind, do_parse!(
        punct!("(") >>
        elems: terminated_list!(punct!(","), expr) >>
        punct!(")") >>
        (ExprKind::Tup(elems))
    ));

    named_ambiguous_expr!(and_binary -> (BinOp, Expr), allow_struct, tuple!(
        binop,
        ambiguous_expr!(allow_struct)
    ));

    named_ambiguous_expr!(expr_unary -> ExprKind, allow_struct, do_parse!(
        operator: unop >>
        operand: ambiguous_expr!(allow_struct) >>
        (ExprKind::Unary(operator, Box::new(operand)))
    ));

    named!(expr_lit -> ExprKind, map!(lit, ExprKind::Lit));

    named!(and_cast -> Ty, do_parse!(
        keyword!("as") >>
        ty: ty >>
        (ty)
    ));

    named!(and_ascription -> Ty, preceded!(punct!(":"), ty));

    enum Cond {
        Let(Pat, Expr),
        Expr(Expr),
    }

    named!(cond -> Cond, alt!(
        do_parse!(
            keyword!("let") >>
            pat: pat >>
            punct!("=") >>
            value: expr_no_struct >>
            (Cond::Let(pat, value))
        )
        |
        map!(expr_no_struct, Cond::Expr)
    ));

    named!(expr_if -> ExprKind, do_parse!(
        keyword!("if") >>
        cond: cond >>
        punct!("{") >>
        then_block: within_block >>
        punct!("}") >>
        else_block: option!(preceded!(
            keyword!("else"),
            alt!(
                expr_if
                |
                do_parse!(
                    punct!("{") >>
                    else_block: within_block >>
                    punct!("}") >>
                    (ExprKind::Block(Unsafety::Normal, Block {
                        stmts: else_block,
                    }).into())
                )
            )
        )) >>
        (match cond {
            Cond::Let(pat, expr) => ExprKind::IfLet(
                Box::new(pat),
                Box::new(expr),
                Block {
                    stmts: then_block,
                },
                else_block.map(|els| Box::new(els.into())),
            ),
            Cond::Expr(cond) => ExprKind::If(
                Box::new(cond),
                Block {
                    stmts: then_block,
                },
                else_block.map(|els| Box::new(els.into())),
            ),
        })
    ));

    named!(expr_for_loop -> ExprKind, do_parse!(
        lbl: option!(terminated!(label, punct!(":"))) >>
        keyword!("for") >>
        pat: pat >>
        keyword!("in") >>
        expr: expr_no_struct >>
        loop_block: block >>
        (ExprKind::ForLoop(Box::new(pat), Box::new(expr), loop_block, lbl))
    ));

    named!(expr_loop -> ExprKind, do_parse!(
        lbl: option!(terminated!(label, punct!(":"))) >>
        keyword!("loop") >>
        loop_block: block >>
        (ExprKind::Loop(loop_block, lbl))
    ));

    named!(expr_match -> ExprKind, do_parse!(
        keyword!("match") >>
        obj: expr_no_struct >>
        punct!("{") >>
        mut arms: many0!(do_parse!(
            arm: match_arm >>
            cond!(arm_requires_comma(&arm), punct!(",")) >>
            cond!(!arm_requires_comma(&arm), option!(punct!(","))) >>
            (arm)
        )) >>
        last_arm: option!(match_arm) >>
        punct!("}") >>
        (ExprKind::Match(Box::new(obj), {
            arms.extend(last_arm);
            arms
        }))
    ));

    fn arm_requires_comma(arm: &Arm) -> bool {
        if let ExprKind::Block(Unsafety::Normal, _) = arm.body.node {
            false
        } else {
            true
        }
    }

    named!(match_arm -> Arm, do_parse!(
        attrs: many0!(outer_attr) >>
        pats: separated_nonempty_list!(punct!("|"), pat) >>
        guard: option!(preceded!(keyword!("if"), expr)) >>
        punct!("=>") >>
        body: alt!(
            map!(block, |blk| ExprKind::Block(Unsafety::Normal, blk).into())
            |
            expr
        ) >>
        (Arm {
            attrs: attrs,
            pats: pats,
            guard: guard.map(Box::new),
            body: Box::new(body),
        })
    ));

    named_ambiguous_expr!(expr_closure -> ExprKind, allow_struct, do_parse!(
        capture: capture_by >>
        punct!("|") >>
        inputs: terminated_list!(punct!(","), closure_arg) >>
        punct!("|") >>
        ret_and_body: alt!(
            do_parse!(
                punct!("->") >>
                ty: ty >>
                body: block >>
                (FunctionRetTy::Ty(ty), ExprKind::Block(Unsafety::Normal, body).into())
            )
            |
            map!(ambiguous_expr!(allow_struct), |e| (FunctionRetTy::Default, e))
        ) >>
        (ExprKind::Closure(
            capture,
            Box::new(FnDecl {
                inputs: inputs,
                output: ret_and_body.0,
                variadic: false,
            }),
            Box::new(ret_and_body.1),
        ))
    ));

    named!(closure_arg -> FnArg, do_parse!(
        pat: pat >>
        ty: option!(preceded!(punct!(":"), ty)) >>
        (FnArg::Captured(pat, ty.unwrap_or(Ty::Infer)))
    ));

    named!(expr_while -> ExprKind, do_parse!(
        lbl: option!(terminated!(label, punct!(":"))) >>
        keyword!("while") >>
        cond: cond >>
        while_block: block >>
        (match cond {
            Cond::Let(pat, expr) => ExprKind::WhileLet(
                Box::new(pat),
                Box::new(expr),
                while_block,
                lbl,
            ),
            Cond::Expr(cond) => ExprKind::While(
                Box::new(cond),
                while_block,
                lbl,
            ),
        })
    ));

    named!(expr_continue -> ExprKind, do_parse!(
        keyword!("continue") >>
        lbl: option!(label) >>
        (ExprKind::Continue(lbl))
    ));

    named_ambiguous_expr!(expr_break -> ExprKind, allow_struct, do_parse!(
        keyword!("break") >>
        lbl: option!(label) >>
        val: option!(call!(ambiguous_expr, allow_struct, false)) >>
        (ExprKind::Break(lbl, val.map(Box::new)))
    ));

    named_ambiguous_expr!(expr_ret -> ExprKind, allow_struct, do_parse!(
        keyword!("return") >>
        ret_value: option!(ambiguous_expr!(allow_struct)) >>
        (ExprKind::Ret(ret_value.map(Box::new)))
    ));

    named!(expr_struct -> ExprKind, do_parse!(
        path: path >>
        punct!("{") >>
        fields: separated_list!(punct!(","), field_value) >>
        base: option!(do_parse!(
            cond!(!fields.is_empty(), punct!(",")) >>
            punct!("..") >>
            base: expr >>
            (base)
        )) >>
        cond!(!fields.is_empty() && base.is_none(), option!(punct!(","))) >>
        punct!("}") >>
        (ExprKind::Struct(path, fields, base.map(Box::new)))
    ));

    named!(field_value -> FieldValue, alt!(
        do_parse!(
            name: wordlike >>
            punct!(":") >>
            value: expr >>
            (FieldValue {
                ident: name,
                expr: value,
                is_shorthand: false,
                attrs: Vec::new(),
            })
        )
        |
        map!(ident, |name: Ident| FieldValue {
            ident: name.clone(),
            expr: ExprKind::Path(None, name.into()).into(),
            is_shorthand: true,
            attrs: Vec::new(),
        })
    ));

    named!(expr_repeat -> ExprKind, do_parse!(
        punct!("[") >>
        value: expr >>
        punct!(";") >>
        times: expr >>
        punct!("]") >>
        (ExprKind::Repeat(Box::new(value), Box::new(times)))
    ));

    named!(expr_block -> ExprKind, do_parse!(
        rules: unsafety >>
        b: block >>
        (ExprKind::Block(rules, Block {
            stmts: b.stmts,
        }))
    ));

    named_ambiguous_expr!(expr_range -> ExprKind, allow_struct, do_parse!(
        limits: range_limits >>
        hi: option!(ambiguous_expr!(allow_struct)) >>
        (ExprKind::Range(None, hi.map(Box::new), limits))
    ));

    named!(range_limits -> RangeLimits, alt!(
        punct!("...") => { |_| RangeLimits::Closed }
        |
        punct!("..") => { |_| RangeLimits::HalfOpen }
    ));

    named!(expr_path -> ExprKind, map!(qpath, |(qself, path)| ExprKind::Path(qself, path)));

    named_ambiguous_expr!(expr_addr_of -> ExprKind, allow_struct, do_parse!(
        punct!("&") >>
        mutability: mutability >>
        expr: ambiguous_expr!(allow_struct) >>
        (ExprKind::AddrOf(mutability, Box::new(expr)))
    ));

    named_ambiguous_expr!(and_assign -> Expr, allow_struct, preceded!(
        punct!("="),
        ambiguous_expr!(allow_struct)
    ));

    named_ambiguous_expr!(and_assign_op -> (BinOp, Expr), allow_struct, tuple!(
        assign_op,
        ambiguous_expr!(allow_struct)
    ));

    named!(and_field -> Ident, preceded!(punct!("."), ident));

    named!(and_tup_field -> u64, preceded!(punct!("."), digits));

    named!(and_index -> Expr, delimited!(punct!("["), expr, punct!("]")));

    named_ambiguous_expr!(and_range -> (RangeLimits, Option<Expr>), allow_struct, tuple!(
        range_limits,
        option!(call!(ambiguous_expr, allow_struct, false))
    ));

    named!(pub block -> Block, do_parse!(
        punct!("{") >>
        stmts: within_block >>
        punct!("}") >>
        (Block {
            stmts: stmts,
        })
    ));

    named!(pub within_block -> Vec<Stmt>, do_parse!(
        many0!(punct!(";")) >>
        mut standalone: many0!(terminated!(stmt, many0!(punct!(";")))) >>
        last: option!(expr) >>
        (match last {
            None => standalone,
            Some(last) => {
                standalone.push(Stmt::Expr(Box::new(last)));
                standalone
            }
        })
    ));

    named!(pub stmt -> Stmt, alt!(
        stmt_mac
        |
        stmt_local
        |
        stmt_item
        |
        stmt_expr
    ));

    named!(stmt_mac -> Stmt, do_parse!(
        attrs: many0!(outer_attr) >>
        what: path >>
        punct!("!") >>
    // Only parse braces here; paren and bracket will get parsed as
    // expression statements
        punct!("{") >>
        tts: token_trees >>
        punct!("}") >>
        semi: option!(punct!(";")) >>
        (Stmt::Mac(Box::new((
            Mac {
                path: what,
                tts: vec![TokenTree::Delimited(Delimited {
                    delim: DelimToken::Brace,
                    tts: tts,
                })],
            },
            if semi.is_some() {
                MacStmtStyle::Semicolon
            } else {
                MacStmtStyle::Braces
            },
            attrs,
        ))))
    ));

    named!(stmt_local -> Stmt, do_parse!(
        attrs: many0!(outer_attr) >>
        keyword!("let") >>
        pat: pat >>
        ty: option!(preceded!(punct!(":"), ty)) >>
        init: option!(preceded!(punct!("="), expr)) >>
        punct!(";") >>
        (Stmt::Local(Box::new(Local {
            pat: Box::new(pat),
            ty: ty.map(Box::new),
            init: init.map(Box::new),
            attrs: attrs,
        })))
    ));

    named!(stmt_item -> Stmt, map!(item, |i| Stmt::Item(Box::new(i))));

    fn requires_semi(e: &Expr) -> bool {
        match e.node {
            ExprKind::If(_, _, _) |
            ExprKind::IfLet(_, _, _, _) |
            ExprKind::While(_, _, _) |
            ExprKind::WhileLet(_, _, _, _) |
            ExprKind::ForLoop(_, _, _, _) |
            ExprKind::Loop(_, _) |
            ExprKind::Match(_, _) |
            ExprKind::Block(_, _) => false,

            _ => true,
        }
    }

    named!(stmt_expr -> Stmt, do_parse!(
        attrs: many0!(outer_attr) >>
        mut e: expr >>
        semi: option!(punct!(";")) >>
        ({
            e.attrs = attrs;
            if semi.is_some() {
                Stmt::Semi(Box::new(e))
            } else if requires_semi(&e) {
                return Error;
            } else {
                Stmt::Expr(Box::new(e))
            }
        })
    ));

    named!(pub pat -> Pat, alt!(
        pat_wild // must be before pat_ident
        |
        pat_box // must be before pat_ident
        |
        pat_range // must be before pat_lit
        |
        pat_tuple_struct // must be before pat_ident
        |
        pat_struct // must be before pat_ident
        |
        pat_mac // must be before pat_ident
        |
        pat_lit // must be before pat_ident
        |
        pat_ident // must be before pat_path
        |
        pat_path
        |
        pat_tuple
        |
        pat_ref
        |
        pat_slice
    ));

    named!(pat_mac -> Pat, map!(mac, Pat::Mac));

    named!(pat_wild -> Pat, map!(keyword!("_"), |_| Pat::Wild));

    named!(pat_box -> Pat, do_parse!(
        keyword!("box") >>
        pat: pat >>
        (Pat::Box(Box::new(pat)))
    ));

    named!(pat_ident -> Pat, do_parse!(
        mode: option!(keyword!("ref")) >>
        mutability: mutability >>
        name: alt!(
            ident
            |
            keyword!("self") => { Into::into }
        ) >>
        not!(punct!("<")) >>
        not!(punct!("::")) >>
        subpat: option!(preceded!(punct!("@"), pat)) >>
        (Pat::Ident(
            if mode.is_some() {
                BindingMode::ByRef(mutability)
            } else {
                BindingMode::ByValue(mutability)
            },
            name,
            subpat.map(Box::new),
        ))
    ));

    named!(pat_tuple_struct -> Pat, do_parse!(
        path: path >>
        tuple: pat_tuple_helper >>
        (Pat::TupleStruct(path, tuple.0, tuple.1))
    ));

    named!(pat_struct -> Pat, do_parse!(
        path: path >>
        punct!("{") >>
        fields: separated_list!(punct!(","), field_pat) >>
        more: option!(preceded!(
            cond!(!fields.is_empty(), punct!(",")),
            punct!("..")
        )) >>
        cond!(!fields.is_empty() && more.is_none(), option!(punct!(","))) >>
        punct!("}") >>
        (Pat::Struct(path, fields, more.is_some()))
    ));

    named!(field_pat -> FieldPat, alt!(
        do_parse!(
            ident: wordlike >>
            punct!(":") >>
            pat: pat >>
            (FieldPat {
                ident: ident,
                pat: Box::new(pat),
                is_shorthand: false,
                attrs: Vec::new(),
            })
        )
        |
        do_parse!(
            boxed: option!(keyword!("box")) >>
            mode: option!(keyword!("ref")) >>
            mutability: mutability >>
            ident: ident >>
            ({
                let mut pat = Pat::Ident(
                    if mode.is_some() {
                        BindingMode::ByRef(mutability)
                    } else {
                        BindingMode::ByValue(mutability)
                    },
                    ident.clone(),
                    None,
                );
                if boxed.is_some() {
                    pat = Pat::Box(Box::new(pat));
                }
                FieldPat {
                    ident: ident,
                    pat: Box::new(pat),
                    is_shorthand: true,
                    attrs: Vec::new(),
                }
            })
        )
    ));

    named!(pat_path -> Pat, map!(qpath, |(qself, path)| Pat::Path(qself, path)));

    named!(pat_tuple -> Pat, map!(
        pat_tuple_helper,
        |(pats, dotdot)| Pat::Tuple(pats, dotdot)
    ));

    named!(pat_tuple_helper -> (Vec<Pat>, Option<usize>), do_parse!(
        punct!("(") >>
        mut elems: separated_list!(punct!(","), pat) >>
        dotdot: option!(do_parse!(
            cond!(!elems.is_empty(), punct!(",")) >>
            punct!("..") >>
            rest: many0!(preceded!(punct!(","), pat)) >>
            cond!(!rest.is_empty(), option!(punct!(","))) >>
            (rest)
        )) >>
        cond!(!elems.is_empty() && dotdot.is_none(), option!(punct!(","))) >>
        punct!(")") >>
        (match dotdot {
            Some(rest) => {
                let pos = elems.len();
                elems.extend(rest);
                (elems, Some(pos))
            }
            None => (elems, None),
        })
    ));

    named!(pat_ref -> Pat, do_parse!(
        punct!("&") >>
        mutability: mutability >>
        pat: pat >>
        (Pat::Ref(Box::new(pat), mutability))
    ));

    named!(pat_lit -> Pat, do_parse!(
        lit: pat_lit_expr >>
        (if let ExprKind::Path(_, _) = lit.node {
            return IResult::Error; // these need to be parsed by pat_path
        } else {
            Pat::Lit(Box::new(lit))
        })
    ));

    named!(pat_range -> Pat, do_parse!(
        lo: pat_lit_expr >>
        punct!("...") >>
        hi: pat_lit_expr >>
        (Pat::Range(Box::new(lo), Box::new(hi)))
    ));

    named!(pat_lit_expr -> Expr, do_parse!(
        neg: option!(punct!("-")) >>
        v: alt!(
            lit => { ExprKind::Lit }
            |
            path => { |p| ExprKind::Path(None, p) }
        ) >>
        (if neg.is_some() {
            ExprKind::Unary(UnOp::Neg, Box::new(v.into())).into()
        } else {
            v.into()
        })
    ));

    named!(pat_slice -> Pat, do_parse!(
        punct!("[") >>
        mut before: separated_list!(punct!(","), pat) >>
        after: option!(do_parse!(
            comma_before_dots: option!(cond_reduce!(!before.is_empty(), punct!(","))) >>
            punct!("..") >>
            after: many0!(preceded!(punct!(","), pat)) >>
            cond!(!after.is_empty(), option!(punct!(","))) >>
            (comma_before_dots.is_some(), after)
        )) >>
        cond!(after.is_none(), option!(punct!(","))) >>
        punct!("]") >>
        (match after {
            None => Pat::Slice(before, None, Vec::new()),
            Some((true, after)) => {
                if before.is_empty() {
                    return IResult::Error;
                }
                Pat::Slice(before, Some(Box::new(Pat::Wild)), after)
            }
            Some((false, after)) => {
                let rest = before.pop().unwrap_or(Pat::Wild);
                Pat::Slice(before, Some(Box::new(rest)), after)
            }
        })
    ));

    named!(capture_by -> CaptureBy, alt!(
        keyword!("move") => { |_| CaptureBy::Value }
        |
        epsilon!() => { |_| CaptureBy::Ref }
    ));

    named!(label -> Ident, map!(lifetime, |lt: Lifetime| lt.ident));
}

#[cfg(feature = "printing")]
mod printing {
    use super::*;
    use {FnArg, FunctionRetTy, Mutability, Ty, Unsafety};
    use attr::FilterAttrs;
    use quote::{Tokens, ToTokens};

    impl ToTokens for Expr {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            match self.node {
                ExprKind::Box(ref inner) => {
                    tokens.append("box");
                    inner.to_tokens(tokens);
                }
                ExprKind::InPlace(ref place, ref value) => {
                    tokens.append("in");
                    place.to_tokens(tokens);
                    value.to_tokens(tokens);
                }
                ExprKind::Array(ref tys) => {
                    tokens.append("[");
                    tokens.append_separated(tys, ",");
                    tokens.append("]");
                }
                ExprKind::Call(ref func, ref args) => {
                    func.to_tokens(tokens);
                    tokens.append("(");
                    tokens.append_separated(args, ",");
                    tokens.append(")");
                }
                ExprKind::MethodCall(ref ident, ref ascript, ref args) => {
                    args[0].to_tokens(tokens);
                    tokens.append(".");
                    ident.to_tokens(tokens);
                    if !ascript.is_empty() {
                        tokens.append("::");
                        tokens.append("<");
                        tokens.append_separated(ascript, ",");
                        tokens.append(">");
                    }
                    tokens.append("(");
                    tokens.append_separated(&args[1..], ",");
                    tokens.append(")");
                }
                ExprKind::Tup(ref fields) => {
                    tokens.append("(");
                    tokens.append_separated(fields, ",");
                    if fields.len() == 1 {
                        tokens.append(",");
                    }
                    tokens.append(")");
                }
                ExprKind::Binary(op, ref left, ref right) => {
                    left.to_tokens(tokens);
                    op.to_tokens(tokens);
                    right.to_tokens(tokens);
                }
                ExprKind::Unary(op, ref expr) => {
                    op.to_tokens(tokens);
                    expr.to_tokens(tokens);
                }
                ExprKind::Lit(ref lit) => lit.to_tokens(tokens),
                ExprKind::Cast(ref expr, ref ty) => {
                    expr.to_tokens(tokens);
                    tokens.append("as");
                    ty.to_tokens(tokens);
                }
                ExprKind::Type(ref expr, ref ty) => {
                    expr.to_tokens(tokens);
                    tokens.append(":");
                    ty.to_tokens(tokens);
                }
                ExprKind::If(ref cond, ref then_block, ref else_block) => {
                    tokens.append("if");
                    cond.to_tokens(tokens);
                    then_block.to_tokens(tokens);
                    if let Some(ref else_block) = *else_block {
                        tokens.append("else");
                        else_block.to_tokens(tokens);
                    }
                }
                ExprKind::IfLet(ref pat, ref expr, ref then_block, ref else_block) => {
                    tokens.append("if");
                    tokens.append("let");
                    pat.to_tokens(tokens);
                    tokens.append("=");
                    expr.to_tokens(tokens);
                    then_block.to_tokens(tokens);
                    if let Some(ref else_block) = *else_block {
                        tokens.append("else");
                        else_block.to_tokens(tokens);
                    }
                }
                ExprKind::While(ref cond, ref body, ref label) => {
                    if let Some(ref label) = *label {
                        label.to_tokens(tokens);
                        tokens.append(":");
                    }
                    tokens.append("while");
                    cond.to_tokens(tokens);
                    body.to_tokens(tokens);
                }
                ExprKind::WhileLet(ref pat, ref expr, ref body, ref label) => {
                    if let Some(ref label) = *label {
                        label.to_tokens(tokens);
                        tokens.append(":");
                    }
                    tokens.append("while");
                    tokens.append("let");
                    pat.to_tokens(tokens);
                    tokens.append("=");
                    expr.to_tokens(tokens);
                    body.to_tokens(tokens);
                }
                ExprKind::ForLoop(ref pat, ref expr, ref body, ref label) => {
                    if let Some(ref label) = *label {
                        label.to_tokens(tokens);
                        tokens.append(":");
                    }
                    tokens.append("for");
                    pat.to_tokens(tokens);
                    tokens.append("in");
                    expr.to_tokens(tokens);
                    body.to_tokens(tokens);
                }
                ExprKind::Loop(ref body, ref label) => {
                    if let Some(ref label) = *label {
                        label.to_tokens(tokens);
                        tokens.append(":");
                    }
                    tokens.append("loop");
                    body.to_tokens(tokens);
                }
                ExprKind::Match(ref expr, ref arms) => {
                    tokens.append("match");
                    expr.to_tokens(tokens);
                    tokens.append("{");
                    tokens.append_all(arms);
                    tokens.append("}");
                }
                ExprKind::Closure(capture, ref decl, ref expr) => {
                    capture.to_tokens(tokens);
                    tokens.append("|");
                    for (i, input) in decl.inputs.iter().enumerate() {
                        if i > 0 {
                            tokens.append(",");
                        }
                        match *input {
                            FnArg::Captured(ref pat, Ty::Infer) => {
                                pat.to_tokens(tokens);
                            }
                            _ => input.to_tokens(tokens),
                        }
                    }
                    tokens.append("|");
                    match decl.output {
                        FunctionRetTy::Default => { /* nothing */ }
                        FunctionRetTy::Ty(ref ty) => {
                            tokens.append("->");
                            ty.to_tokens(tokens);
                        }
                    }
                    expr.to_tokens(tokens);
                }
                ExprKind::Block(rules, ref block) => {
                    rules.to_tokens(tokens);
                    block.to_tokens(tokens);
                }
                ExprKind::Assign(ref var, ref expr) => {
                    var.to_tokens(tokens);
                    tokens.append("=");
                    expr.to_tokens(tokens);
                }
                ExprKind::AssignOp(op, ref var, ref expr) => {
                    var.to_tokens(tokens);
                    tokens.append(op.assign_op().unwrap());
                    expr.to_tokens(tokens);
                }
                ExprKind::Field(ref expr, ref field) => {
                    expr.to_tokens(tokens);
                    tokens.append(".");
                    field.to_tokens(tokens);
                }
                ExprKind::TupField(ref expr, field) => {
                    expr.to_tokens(tokens);
                    tokens.append(".");
                    tokens.append(&field.to_string());
                }
                ExprKind::Index(ref expr, ref index) => {
                    expr.to_tokens(tokens);
                    tokens.append("[");
                    index.to_tokens(tokens);
                    tokens.append("]");
                }
                ExprKind::Range(ref from, ref to, limits) => {
                    from.to_tokens(tokens);
                    match limits {
                        RangeLimits::HalfOpen => tokens.append(".."),
                        RangeLimits::Closed => tokens.append("..."),
                    }
                    to.to_tokens(tokens);
                }
                ExprKind::Path(None, ref path) => path.to_tokens(tokens),
                ExprKind::Path(Some(ref qself), ref path) => {
                    tokens.append("<");
                    qself.ty.to_tokens(tokens);
                    if qself.position > 0 {
                        tokens.append("as");
                        for (i, segment) in path.segments
                                .iter()
                                .take(qself.position)
                                .enumerate() {
                            if i > 0 || path.global {
                                tokens.append("::");
                            }
                            segment.to_tokens(tokens);
                        }
                    }
                    tokens.append(">");
                    for segment in path.segments.iter().skip(qself.position) {
                        tokens.append("::");
                        segment.to_tokens(tokens);
                    }
                }
                ExprKind::AddrOf(mutability, ref expr) => {
                    tokens.append("&");
                    mutability.to_tokens(tokens);
                    expr.to_tokens(tokens);
                }
                ExprKind::Break(ref opt_label, ref opt_val) => {
                    tokens.append("break");
                    opt_label.to_tokens(tokens);
                    opt_val.to_tokens(tokens);
                }
                ExprKind::Continue(ref opt_label) => {
                    tokens.append("continue");
                    opt_label.to_tokens(tokens);
                }
                ExprKind::Ret(ref opt_expr) => {
                    tokens.append("return");
                    opt_expr.to_tokens(tokens);
                }
                ExprKind::Mac(ref mac) => mac.to_tokens(tokens),
                ExprKind::Struct(ref path, ref fields, ref base) => {
                    path.to_tokens(tokens);
                    tokens.append("{");
                    tokens.append_separated(fields, ",");
                    if let Some(ref base) = *base {
                        if !fields.is_empty() {
                            tokens.append(",");
                        }
                        tokens.append("..");
                        base.to_tokens(tokens);
                    }
                    tokens.append("}");
                }
                ExprKind::Repeat(ref expr, ref times) => {
                    tokens.append("[");
                    expr.to_tokens(tokens);
                    tokens.append(";");
                    times.to_tokens(tokens);
                    tokens.append("]");
                }
                ExprKind::Paren(ref expr) => {
                    tokens.append("(");
                    expr.to_tokens(tokens);
                    tokens.append(")");
                }
                ExprKind::Try(ref expr) => {
                    expr.to_tokens(tokens);
                    tokens.append("?");
                }
            }
        }
    }

    impl ToTokens for FieldValue {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.ident.to_tokens(tokens);
            if !self.is_shorthand {
                tokens.append(":");
                self.expr.to_tokens(tokens);
            }
        }
    }

    impl ToTokens for Arm {
        fn to_tokens(&self, tokens: &mut Tokens) {
            for attr in &self.attrs {
                attr.to_tokens(tokens);
            }
            tokens.append_separated(&self.pats, "|");
            if let Some(ref guard) = self.guard {
                tokens.append("if");
                guard.to_tokens(tokens);
            }
            tokens.append("=>");
            self.body.to_tokens(tokens);
            match self.body.node {
                ExprKind::Block(Unsafety::Normal, _) => {
                    // no comma
                }
                _ => tokens.append(","),
            }
        }
    }

    impl ToTokens for Pat {
        fn to_tokens(&self, tokens: &mut Tokens) {
            match *self {
                Pat::Wild => tokens.append("_"),
                Pat::Ident(mode, ref ident, ref subpat) => {
                    mode.to_tokens(tokens);
                    ident.to_tokens(tokens);
                    if let Some(ref subpat) = *subpat {
                        tokens.append("@");
                        subpat.to_tokens(tokens);
                    }
                }
                Pat::Struct(ref path, ref fields, dots) => {
                    path.to_tokens(tokens);
                    tokens.append("{");
                    tokens.append_separated(fields, ",");
                    if dots {
                        if !fields.is_empty() {
                            tokens.append(",");
                        }
                        tokens.append("..");
                    }
                    tokens.append("}");
                }
                Pat::TupleStruct(ref path, ref pats, dotpos) => {
                    path.to_tokens(tokens);
                    tokens.append("(");
                    match dotpos {
                        Some(pos) => {
                            if pos > 0 {
                                tokens.append_separated(&pats[..pos], ",");
                                tokens.append(",");
                            }
                            tokens.append("..");
                            if pos < pats.len() {
                                tokens.append(",");
                                tokens.append_separated(&pats[pos..], ",");
                            }
                        }
                        None => tokens.append_separated(pats, ","),
                    }
                    tokens.append(")");
                }
                Pat::Path(None, ref path) => path.to_tokens(tokens),
                Pat::Path(Some(ref qself), ref path) => {
                    tokens.append("<");
                    qself.ty.to_tokens(tokens);
                    if qself.position > 0 {
                        tokens.append("as");
                        for (i, segment) in path.segments
                                .iter()
                                .take(qself.position)
                                .enumerate() {
                            if i > 0 || path.global {
                                tokens.append("::");
                            }
                            segment.to_tokens(tokens);
                        }
                    }
                    tokens.append(">");
                    for segment in path.segments.iter().skip(qself.position) {
                        tokens.append("::");
                        segment.to_tokens(tokens);
                    }
                }
                Pat::Tuple(ref pats, dotpos) => {
                    tokens.append("(");
                    match dotpos {
                        Some(pos) => {
                            if pos > 0 {
                                tokens.append_separated(&pats[..pos], ",");
                                tokens.append(",");
                            }
                            tokens.append("..");
                            if pos < pats.len() {
                                tokens.append(",");
                                tokens.append_separated(&pats[pos..], ",");
                            }
                        }
                        None => {
                            tokens.append_separated(pats, ",");
                            if pats.len() == 1 {
                                tokens.append(",");
                            }
                        }
                    }
                    tokens.append(")");
                }
                Pat::Box(ref inner) => {
                    tokens.append("box");
                    inner.to_tokens(tokens);
                }
                Pat::Ref(ref target, mutability) => {
                    tokens.append("&");
                    mutability.to_tokens(tokens);
                    target.to_tokens(tokens);
                }
                Pat::Lit(ref lit) => lit.to_tokens(tokens),
                Pat::Range(ref lo, ref hi) => {
                    lo.to_tokens(tokens);
                    tokens.append("...");
                    hi.to_tokens(tokens);
                }
                Pat::Slice(ref before, ref rest, ref after) => {
                    tokens.append("[");
                    tokens.append_separated(before, ",");
                    if let Some(ref rest) = *rest {
                        if !before.is_empty() {
                            tokens.append(",");
                        }
                        if **rest != Pat::Wild {
                            rest.to_tokens(tokens);
                        }
                        tokens.append("..");
                        if !after.is_empty() {
                            tokens.append(",");
                        }
                        tokens.append_separated(after, ",");
                    }
                    tokens.append("]");
                }
                Pat::Mac(ref mac) => mac.to_tokens(tokens),
            }
        }
    }

    impl ToTokens for FieldPat {
        fn to_tokens(&self, tokens: &mut Tokens) {
            if !self.is_shorthand {
                self.ident.to_tokens(tokens);
                tokens.append(":");
            }
            self.pat.to_tokens(tokens);
        }
    }

    impl ToTokens for BindingMode {
        fn to_tokens(&self, tokens: &mut Tokens) {
            match *self {
                BindingMode::ByRef(Mutability::Immutable) => {
                    tokens.append("ref");
                }
                BindingMode::ByRef(Mutability::Mutable) => {
                    tokens.append("ref");
                    tokens.append("mut");
                }
                BindingMode::ByValue(Mutability::Immutable) => {}
                BindingMode::ByValue(Mutability::Mutable) => {
                    tokens.append("mut");
                }
            }
        }
    }

    impl ToTokens for CaptureBy {
        fn to_tokens(&self, tokens: &mut Tokens) {
            match *self {
                CaptureBy::Value => tokens.append("move"),
                CaptureBy::Ref => {
                    // nothing
                }
            }
        }
    }

    impl ToTokens for Block {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append("{");
            tokens.append_all(&self.stmts);
            tokens.append("}");
        }
    }

    impl ToTokens for Stmt {
        fn to_tokens(&self, tokens: &mut Tokens) {
            match *self {
                Stmt::Local(ref local) => local.to_tokens(tokens),
                Stmt::Item(ref item) => item.to_tokens(tokens),
                Stmt::Expr(ref expr) => expr.to_tokens(tokens),
                Stmt::Semi(ref expr) => {
                    expr.to_tokens(tokens);
                    tokens.append(";");
                }
                Stmt::Mac(ref mac) => {
                    let (ref mac, style, ref attrs) = **mac;
                    tokens.append_all(attrs.outer());
                    mac.to_tokens(tokens);
                    match style {
                        MacStmtStyle::Semicolon => tokens.append(";"),
                        MacStmtStyle::Braces | MacStmtStyle::NoBraces => {
                            // no semicolon
                        }
                    }
                }
            }
        }
    }

    impl ToTokens for Local {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            tokens.append("let");
            self.pat.to_tokens(tokens);
            if let Some(ref ty) = self.ty {
                tokens.append(":");
                ty.to_tokens(tokens);
            }
            if let Some(ref init) = self.init {
                tokens.append("=");
                init.to_tokens(tokens);
            }
            tokens.append(";");
        }
    }
}
