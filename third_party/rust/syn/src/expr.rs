use super::*;

#[derive(Debug, Clone, Eq, PartialEq)]
pub enum Expr {
    /// A `box x` expression.
    Box(Box<Expr>),
    /// An array (`[a, b, c, d]`)
    Vec(Vec<Expr>),
    /// A function call
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
    /// A tuple (`(a, b, c, d)`)
    Tup(Vec<Expr>),
    /// A binary operation (For example: `a + b`, `a * b`)
    Binary(BinOp, Box<Expr>, Box<Expr>),
    /// A unary operation (For example: `!x`, `*x`)
    Unary(UnOp, Box<Expr>),
    /// A literal (For example: `1`, `"foo"`)
    Lit(Lit),
    /// A cast (`foo as f64`)
    Cast(Box<Expr>, Box<Ty>),
    /// Type ascription (`foo: f64`)
    Type(Box<Expr>, Box<Ty>),
    /// An `if` block, with an optional else block
    ///
    /// `if expr { block } else { expr }`
    If(Box<Expr>, Block, Option<Box<Expr>>),
    /// An `if let` expression with an optional else block
    ///
    /// `if let pat = expr { block } else { expr }`
    ///
    /// This is desugared to a `match` expression.
    IfLet(Box<Pat>, Box<Expr>, Block, Option<Box<Expr>>),
    /// A while loop, with an optional label
    ///
    /// `'label: while expr { block }`
    While(Box<Expr>, Block, Option<Ident>),
    /// A while-let loop, with an optional label
    ///
    /// `'label: while let pat = expr { block }`
    ///
    /// This is desugared to a combination of `loop` and `match` expressions.
    WhileLet(Box<Pat>, Box<Expr>, Block, Option<Ident>),
    /// A for loop, with an optional label
    ///
    /// `'label: for pat in expr { block }`
    ///
    /// This is desugared to a combination of `loop` and `match` expressions.
    ForLoop(Box<Pat>, Box<Expr>, Block, Option<Ident>),
    /// Conditionless loop (can be exited with break, continue, or return)
    ///
    /// `'label: loop { block }`
    Loop(Block, Option<Ident>),
    /// A `match` block.
    Match(Box<Expr>, Vec<Arm>),
    /// A closure (for example, `move |a, b, c| {a + b + c}`)
    Closure(CaptureBy, Box<FnDecl>, Block),
    /// A block (`{ ... }` or `unsafe { ... }`)
    Block(BlockCheckMode, Block),

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
    /// A `break`, with an optional label to break
    Break(Option<Ident>),
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

#[derive(Debug, Clone, Eq, PartialEq)]
pub struct FieldValue {
    pub ident: Ident,
    pub expr: Expr,
}

/// A Block (`{ .. }`).
///
/// E.g. `{ .. }` as in `fn foo() { .. }`
#[derive(Debug, Clone, Eq, PartialEq)]
pub struct Block {
    /// Statements in a block
    pub stmts: Vec<Stmt>,
}

#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum BlockCheckMode {
    Default,
    Unsafe,
}

#[derive(Debug, Clone, Eq, PartialEq)]
pub enum Stmt {
    /// A local (let) binding.
    Local(Box<Local>),

    /// An item definition.
    Item(Box<Item>),

    /// Expr without trailing semi-colon.
    Expr(Box<Expr>),

    Semi(Box<Expr>),

    Mac(Box<(Mac, MacStmtStyle, Vec<Attribute>)>),
}

#[derive(Debug, Copy, Clone, Eq, PartialEq)]
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
#[derive(Debug, Clone, Eq, PartialEq)]
pub struct Local {
    pub pat: Box<Pat>,
    pub ty: Option<Box<Ty>>,
    /// Initializer expression to set the value, if any
    pub init: Option<Box<Expr>>,
    pub attrs: Vec<Attribute>,
}

#[derive(Debug, Clone, Eq, PartialEq)]
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
    Lit(Box<Lit>),
    /// A range pattern, e.g. `1...2`
    Range(Box<Lit>, Box<Lit>),
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
#[derive(Debug, Clone, Eq, PartialEq)]
pub struct Arm {
    pub attrs: Vec<Attribute>,
    pub pats: Vec<Pat>,
    pub guard: Option<Box<Expr>>,
    pub body: Box<Expr>,
}

/// A capture clause
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum CaptureBy {
    Value,
    Ref,
}

/// Limit types of a range (inclusive or exclusive)
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
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
#[derive(Debug, Clone, Eq, PartialEq)]
pub struct FieldPat {
    /// The identifier for the field
    pub ident: Ident,
    /// The pattern the field is destructured to
    pub pat: Box<Pat>,
    pub is_shorthand: bool,
}

#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum BindingMode {
    ByRef(Mutability),
    ByValue(Mutability),
}

#[cfg(feature = "parsing")]
pub mod parsing {
    use super::*;
    use {BinOp, Delimited, DelimToken, FnArg, FnDecl, FunctionRetTy, Ident, Lifetime, TokenTree,
         Ty};
    use attr::parsing::outer_attr;
    use generics::parsing::lifetime;
    use ident::parsing::ident;
    use item::parsing::item;
    use lit::parsing::{digits, lit};
    use mac::parsing::mac;
    use nom::IResult::Error;
    use op::parsing::{assign_op, binop, unop};
    use ty::parsing::{mutability, path, qpath, ty};

    named!(pub expr -> Expr, do_parse!(
        mut e: alt!(
            expr_lit // must be before expr_struct
            |
            expr_struct // must be before expr_path
            |
            expr_paren // must be before expr_tup
            |
            expr_mac // must be before expr_path
            |
            expr_break // must be before expr_path
            |
            expr_continue // must be before expr_path
            |
            expr_ret // must be before expr_path
            |
            expr_box
            |
            expr_vec
            |
            expr_tup
            |
            expr_unary
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
            expr_closure
            |
            expr_block
            |
            expr_range
            |
            expr_path
            |
            expr_addr_of
            |
            expr_repeat
        ) >>
        many0!(alt!(
            tap!(args: and_call => {
                e = Expr::Call(Box::new(e), args);
            })
            |
            tap!(more: and_method_call => {
                let (method, ascript, mut args) = more;
                args.insert(0, e);
                e = Expr::MethodCall(method, ascript, args);
            })
            |
            tap!(more: and_binary => {
                let (op, other) = more;
                e = Expr::Binary(op, Box::new(e), Box::new(other));
            })
            |
            tap!(ty: and_cast => {
                e = Expr::Cast(Box::new(e), Box::new(ty));
            })
            |
            tap!(ty: and_ascription => {
                e = Expr::Type(Box::new(e), Box::new(ty));
            })
            |
            tap!(v: and_assign => {
                e = Expr::Assign(Box::new(e), Box::new(v));
            })
            |
            tap!(more: and_assign_op => {
                let (op, v) = more;
                e = Expr::AssignOp(op, Box::new(e), Box::new(v));
            })
            |
            tap!(field: and_field => {
                e = Expr::Field(Box::new(e), field);
            })
            |
            tap!(field: and_tup_field => {
                e = Expr::TupField(Box::new(e), field as usize);
            })
            |
            tap!(i: and_index => {
                e = Expr::Index(Box::new(e), Box::new(i));
            })
            |
            tap!(more: and_range => {
                let (limits, hi) = more;
                e = Expr::Range(Some(Box::new(e)), hi.map(Box::new), limits);
            })
            |
            tap!(_try: punct!("?") => {
                e = Expr::Try(Box::new(e));
            })
        )) >>
        (e)
    ));

    named!(expr_mac -> Expr, map!(mac, Expr::Mac));

    named!(expr_paren -> Expr, do_parse!(
        punct!("(") >>
        e: expr >>
        punct!(")") >>
        (Expr::Paren(Box::new(e)))
    ));

    named!(expr_box -> Expr, do_parse!(
        keyword!("box") >>
        inner: expr >>
        (Expr::Box(Box::new(inner)))
    ));

    named!(expr_vec -> Expr, do_parse!(
        punct!("[") >>
        elems: terminated_list!(punct!(","), expr) >>
        punct!("]") >>
        (Expr::Vec(elems))
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

    named!(expr_tup -> Expr, do_parse!(
        punct!("(") >>
        elems: terminated_list!(punct!(","), expr) >>
        punct!(")") >>
        (Expr::Tup(elems))
    ));

    named!(and_binary -> (BinOp, Expr), tuple!(binop, expr));

    named!(expr_unary -> Expr, do_parse!(
        operator: unop >>
        operand: expr >>
        (Expr::Unary(operator, Box::new(operand)))
    ));

    named!(expr_lit -> Expr, map!(lit, Expr::Lit));

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
            value: expr >>
            (Cond::Let(pat, value))
        )
        |
        map!(expr, Cond::Expr)
    ));

    named!(expr_if -> Expr, do_parse!(
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
                    (Expr::Block(BlockCheckMode::Default, Block {
                        stmts: else_block,
                    }))
                )
            )
        )) >>
        (match cond {
            Cond::Let(pat, expr) => Expr::IfLet(
                Box::new(pat),
                Box::new(expr),
                Block {
                    stmts: then_block,
                },
                else_block.map(Box::new),
            ),
            Cond::Expr(cond) => Expr::If(
                Box::new(cond),
                Block {
                    stmts: then_block,
                },
                else_block.map(Box::new),
            ),
        })
    ));

    named!(expr_for_loop -> Expr, do_parse!(
        lbl: option!(terminated!(label, punct!(":"))) >>
        keyword!("for") >>
        pat: pat >>
        keyword!("in") >>
        expr: expr >>
        loop_block: block >>
        (Expr::ForLoop(Box::new(pat), Box::new(expr), loop_block, lbl))
    ));

    named!(expr_loop -> Expr, do_parse!(
        lbl: option!(terminated!(label, punct!(":"))) >>
        keyword!("loop") >>
        loop_block: block >>
        (Expr::Loop(loop_block, lbl))
    ));

    named!(expr_match -> Expr, do_parse!(
        keyword!("match") >>
        obj: expr >>
        punct!("{") >>
        arms: many0!(do_parse!(
            attrs: many0!(outer_attr) >>
            pats: separated_nonempty_list!(punct!("|"), pat) >>
            guard: option!(preceded!(keyword!("if"), expr)) >>
            punct!("=>") >>
            body: alt!(
                map!(block, |blk| Expr::Block(BlockCheckMode::Default, blk))
                |
                expr
            ) >>
            option!(punct!(",")) >>
            (Arm {
                attrs: attrs,
                pats: pats,
                guard: guard.map(Box::new),
                body: Box::new(body),
            })
        )) >>
        punct!("}") >>
        (Expr::Match(Box::new(obj), arms))
    ));

    named!(expr_closure -> Expr, do_parse!(
        capture: capture_by >>
        punct!("|") >>
        inputs: terminated_list!(punct!(","), closure_arg) >>
        punct!("|") >>
        ret_and_body: alt!(
            do_parse!(
                punct!("->") >>
                ty: ty >>
                body: block >>
                ((FunctionRetTy::Ty(ty), body))
            )
            |
            map!(expr, |e| (
                FunctionRetTy::Default,
                Block {
                    stmts: vec![Stmt::Expr(Box::new(e))],
                },
            ))
        ) >>
        (Expr::Closure(
            capture,
            Box::new(FnDecl {
                inputs: inputs,
                output: ret_and_body.0,
            }),
            ret_and_body.1,
        ))
    ));

    named!(closure_arg -> FnArg, do_parse!(
        pat: pat >>
        ty: option!(preceded!(punct!(":"), ty)) >>
        (FnArg::Captured(pat, ty.unwrap_or(Ty::Infer)))
    ));

    named!(expr_while -> Expr, do_parse!(
        lbl: option!(terminated!(label, punct!(":"))) >>
        keyword!("while") >>
        cond: cond >>
        while_block: block >>
        (match cond {
            Cond::Let(pat, expr) => Expr::WhileLet(
                Box::new(pat),
                Box::new(expr),
                while_block,
                lbl,
            ),
            Cond::Expr(cond) => Expr::While(
                Box::new(cond),
                while_block,
                lbl,
            ),
        })
    ));

    named!(expr_continue -> Expr, do_parse!(
        keyword!("continue") >>
        lbl: option!(label) >>
        (Expr::Continue(lbl))
    ));

    named!(expr_break -> Expr, do_parse!(
        keyword!("break") >>
        lbl: option!(label) >>
        (Expr::Break(lbl))
    ));

    named!(expr_ret -> Expr, do_parse!(
        keyword!("return") >>
        ret_value: option!(expr) >>
        (Expr::Ret(ret_value.map(Box::new)))
    ));

    named!(expr_struct -> Expr, do_parse!(
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
        (Expr::Struct(path, fields, base.map(Box::new)))
    ));

    named!(field_value -> FieldValue, do_parse!(
        name: ident >>
        punct!(":") >>
        value: expr >>
        (FieldValue {
            ident: name,
            expr: value,
        })
    ));

    named!(expr_repeat -> Expr, do_parse!(
        punct!("[") >>
        value: expr >>
        punct!(";") >>
        times: expr >>
        punct!("]") >>
        (Expr::Repeat(Box::new(value), Box::new(times)))
    ));

    named!(expr_block -> Expr, do_parse!(
        rules: block_check_mode >>
        b: block >>
        (Expr::Block(rules, Block {
            stmts: b.stmts,
        }))
    ));

    named!(expr_range -> Expr, do_parse!(
        limits: range_limits >>
        hi: option!(expr) >>
        (Expr::Range(None, hi.map(Box::new), limits))
    ));

    named!(range_limits -> RangeLimits, alt!(
        punct!("...") => { |_| RangeLimits::Closed }
        |
        punct!("..") => { |_| RangeLimits::HalfOpen }
    ));

    named!(expr_path -> Expr, map!(qpath, |(qself, path)| Expr::Path(qself, path)));

    named!(expr_addr_of -> Expr, do_parse!(
        punct!("&") >>
        mutability: mutability >>
        expr: expr >>
        (Expr::AddrOf(mutability, Box::new(expr)))
    ));

    named!(and_assign -> Expr, preceded!(punct!("="), expr));

    named!(and_assign_op -> (BinOp, Expr), tuple!(assign_op, expr));

    named!(and_field -> Ident, preceded!(punct!("."), ident));

    named!(and_tup_field -> u64, preceded!(punct!("."), digits));

    named!(and_index -> Expr, delimited!(punct!("["), expr, punct!("]")));

    named!(and_range -> (RangeLimits, Option<Expr>), tuple!(range_limits, option!(expr)));

    named!(pub block -> Block, do_parse!(
        punct!("{") >>
        stmts: within_block >>
        punct!("}") >>
        (Block {
            stmts: stmts,
        })
    ));

    named!(block_check_mode -> BlockCheckMode, alt!(
        keyword!("unsafe") => { |_| BlockCheckMode::Unsafe }
        |
        epsilon!() => { |_| BlockCheckMode::Default }
    ));

    named!(within_block -> Vec<Stmt>, do_parse!(
        many0!(punct!(";")) >>
        mut standalone: many0!(terminated!(standalone_stmt, many0!(punct!(";")))) >>
        last: option!(expr) >>
        (match last {
            None => standalone,
            Some(last) => {
                standalone.push(Stmt::Expr(Box::new(last)));
                standalone
            }
        })
    ));

    named!(standalone_stmt -> Stmt, alt!(
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
        mac: mac >>
        semi: option!(punct!(";")) >>
        ({
            let style = if semi.is_some() {
                MacStmtStyle::Semicolon
            } else if let Some(&TokenTree::Delimited(Delimited { delim, .. })) = mac.tts.last() {
                match delim {
                    DelimToken::Paren | DelimToken::Bracket => MacStmtStyle::NoBraces,
                    DelimToken::Brace => MacStmtStyle::Braces,
                }
            } else {
                MacStmtStyle::NoBraces
            };
            Stmt::Mac(Box::new((mac, style, attrs)))
        })
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
        match *e {
            Expr::If(_, _, _) |
            Expr::IfLet(_, _, _, _) |
            Expr::While(_, _, _) |
            Expr::WhileLet(_, _, _, _) |
            Expr::ForLoop(_, _, _, _) |
            Expr::Loop(_, _) |
            Expr::Match(_, _) |
            Expr::Block(_, _) => false,

            _ => true,
        }
    }

    named!(stmt_expr -> Stmt, do_parse!(
        e: expr >>
        semi: option!(punct!(";")) >>
        (if semi.is_some() {
            Stmt::Semi(Box::new(e))
        } else if requires_semi(&e) {
            return Error;
        } else {
            Stmt::Expr(Box::new(e))
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
        pat_ident // must be before pat_path
        |
        pat_path
        |
        pat_tuple
        |
        pat_ref
        |
        pat_lit
    // TODO: Vec
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
        name: ident >>
        not!(peek!(punct!("<"))) >>
        not!(peek!(punct!("::"))) >>
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
            ident: ident >>
            punct!(":") >>
            pat: pat >>
            (FieldPat {
                ident: ident,
                pat: Box::new(pat),
                is_shorthand: false,
            })
        )
        |
        do_parse!(
            mode: option!(keyword!("ref")) >>
            mutability: mutability >>
            ident: ident >>
            (FieldPat {
                ident: ident.clone(),
                pat: Box::new(Pat::Ident(
                    if mode.is_some() {
                        BindingMode::ByRef(mutability)
                    } else {
                        BindingMode::ByValue(mutability)
                    },
                    ident,
                    None,
                )),
                is_shorthand: true,
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

    named!(pat_lit -> Pat, map!(lit, |lit| Pat::Lit(Box::new(lit))));

    named!(pat_range -> Pat, do_parse!(
        lo: lit >>
        punct!("...") >>
        hi: lit >>
        (Pat::Range(Box::new(lo), Box::new(hi)))
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
    use {FnArg, FunctionRetTy, Mutability, Ty};
    use attr::FilterAttrs;
    use quote::{Tokens, ToTokens};

    impl ToTokens for Expr {
        fn to_tokens(&self, tokens: &mut Tokens) {
            match *self {
                Expr::Box(ref inner) => {
                    tokens.append("box");
                    inner.to_tokens(tokens);
                }
                Expr::Vec(ref tys) => {
                    tokens.append("[");
                    tokens.append_separated(tys, ",");
                    tokens.append("]");
                }
                Expr::Call(ref func, ref args) => {
                    func.to_tokens(tokens);
                    tokens.append("(");
                    tokens.append_separated(args, ",");
                    tokens.append(")");
                }
                Expr::MethodCall(ref ident, ref ascript, ref args) => {
                    args[0].to_tokens(tokens);
                    tokens.append(".");
                    ident.to_tokens(tokens);
                    if ascript.len() > 0 {
                        tokens.append("::");
                        tokens.append("<");
                        tokens.append_separated(ascript, ",");
                        tokens.append(">");
                    }
                    tokens.append("(");
                    tokens.append_separated(&args[1..], ",");
                    tokens.append(")");
                }
                Expr::Tup(ref fields) => {
                    tokens.append("(");
                    tokens.append_separated(fields, ",");
                    if fields.len() == 1 {
                        tokens.append(",");
                    }
                    tokens.append(")");
                }
                Expr::Binary(op, ref left, ref right) => {
                    left.to_tokens(tokens);
                    op.to_tokens(tokens);
                    right.to_tokens(tokens);
                }
                Expr::Unary(op, ref expr) => {
                    op.to_tokens(tokens);
                    expr.to_tokens(tokens);
                }
                Expr::Lit(ref lit) => lit.to_tokens(tokens),
                Expr::Cast(ref expr, ref ty) => {
                    expr.to_tokens(tokens);
                    tokens.append("as");
                    ty.to_tokens(tokens);
                }
                Expr::Type(ref expr, ref ty) => {
                    expr.to_tokens(tokens);
                    tokens.append(":");
                    ty.to_tokens(tokens);
                }
                Expr::If(ref cond, ref then_block, ref else_block) => {
                    tokens.append("if");
                    cond.to_tokens(tokens);
                    then_block.to_tokens(tokens);
                    if let Some(ref else_block) = *else_block {
                        tokens.append("else");
                        else_block.to_tokens(tokens);
                    }
                }
                Expr::IfLet(ref pat, ref expr, ref then_block, ref else_block) => {
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
                Expr::While(ref cond, ref body, ref label) => {
                    if let Some(ref label) = *label {
                        label.to_tokens(tokens);
                        tokens.append(":");
                    }
                    tokens.append("while");
                    cond.to_tokens(tokens);
                    body.to_tokens(tokens);
                }
                Expr::WhileLet(ref pat, ref expr, ref body, ref label) => {
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
                Expr::ForLoop(ref pat, ref expr, ref body, ref label) => {
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
                Expr::Loop(ref body, ref label) => {
                    if let Some(ref label) = *label {
                        label.to_tokens(tokens);
                        tokens.append(":");
                    }
                    tokens.append("loop");
                    body.to_tokens(tokens);
                }
                Expr::Match(ref expr, ref arms) => {
                    tokens.append("match");
                    expr.to_tokens(tokens);
                    tokens.append("{");
                    tokens.append_all(arms);
                    tokens.append("}");
                }
                Expr::Closure(capture, ref decl, ref body) => {
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
                        FunctionRetTy::Default => {
                            if body.stmts.len() == 1 {
                                if let Stmt::Expr(ref expr) = body.stmts[0] {
                                    expr.to_tokens(tokens);
                                } else {
                                    body.to_tokens(tokens);
                                }
                            } else {
                                body.to_tokens(tokens);
                            }
                        }
                        FunctionRetTy::Ty(ref ty) => {
                            tokens.append("->");
                            ty.to_tokens(tokens);
                            body.to_tokens(tokens);
                        }
                    }
                }
                Expr::Block(rules, ref block) => {
                    rules.to_tokens(tokens);
                    block.to_tokens(tokens);
                }
                Expr::Assign(ref var, ref expr) => {
                    var.to_tokens(tokens);
                    tokens.append("=");
                    expr.to_tokens(tokens);
                }
                Expr::AssignOp(op, ref var, ref expr) => {
                    var.to_tokens(tokens);
                    tokens.append(op.assign_op().unwrap());
                    expr.to_tokens(tokens);
                }
                Expr::Field(ref expr, ref field) => {
                    expr.to_tokens(tokens);
                    tokens.append(".");
                    field.to_tokens(tokens);
                }
                Expr::TupField(ref expr, field) => {
                    expr.to_tokens(tokens);
                    tokens.append(".");
                    tokens.append(&field.to_string());
                }
                Expr::Index(ref expr, ref index) => {
                    expr.to_tokens(tokens);
                    tokens.append("[");
                    index.to_tokens(tokens);
                    tokens.append("]");
                }
                Expr::Range(ref from, ref to, limits) => {
                    from.to_tokens(tokens);
                    match limits {
                        RangeLimits::HalfOpen => tokens.append(".."),
                        RangeLimits::Closed => tokens.append("..."),
                    }
                    to.to_tokens(tokens);
                }
                Expr::Path(None, ref path) => path.to_tokens(tokens),
                Expr::Path(Some(ref qself), ref path) => {
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
                Expr::AddrOf(mutability, ref expr) => {
                    tokens.append("&");
                    mutability.to_tokens(tokens);
                    expr.to_tokens(tokens);
                }
                Expr::Break(ref opt_label) => {
                    tokens.append("break");
                    opt_label.to_tokens(tokens);
                }
                Expr::Continue(ref opt_label) => {
                    tokens.append("continue");
                    opt_label.to_tokens(tokens);
                }
                Expr::Ret(ref opt_expr) => {
                    tokens.append("return");
                    opt_expr.to_tokens(tokens);
                }
                Expr::Mac(ref mac) => mac.to_tokens(tokens),
                Expr::Struct(ref path, ref fields, ref base) => {
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
                Expr::Repeat(ref expr, ref times) => {
                    tokens.append("[");
                    expr.to_tokens(tokens);
                    tokens.append(";");
                    times.to_tokens(tokens);
                    tokens.append("]");
                }
                Expr::Paren(ref expr) => {
                    tokens.append("(");
                    expr.to_tokens(tokens);
                    tokens.append(")");
                }
                Expr::Try(ref expr) => {
                    expr.to_tokens(tokens);
                    tokens.append("?");
                }
            }
        }
    }

    impl ToTokens for FieldValue {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.ident.to_tokens(tokens);
            tokens.append(":");
            self.expr.to_tokens(tokens);
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
            match *self.body {
                Expr::Block(_, _) => {
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
                        None => tokens.append_separated(pats, ","),
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
                Pat::Slice(ref _before, ref _dots, ref _after) => unimplemented!(),
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
            for stmt in &self.stmts {
                stmt.to_tokens(tokens);
            }
            tokens.append("}");
        }
    }

    impl ToTokens for BlockCheckMode {
        fn to_tokens(&self, tokens: &mut Tokens) {
            match *self {
                BlockCheckMode::Default => {
                    // nothing
                }
                BlockCheckMode::Unsafe => tokens.append("unsafe"),
            }
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
                    for attr in attrs.outer() {
                        attr.to_tokens(tokens);
                    }
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
