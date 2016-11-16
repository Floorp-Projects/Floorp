use super::*;

/// An item
///
/// The name might be a dummy name in case of anonymous items
#[derive(Debug, Clone, Eq, PartialEq)]
pub struct Item {
    pub ident: Ident,
    pub vis: Visibility,
    pub attrs: Vec<Attribute>,
    pub node: ItemKind,
}

#[derive(Debug, Clone, Eq, PartialEq)]
pub enum ItemKind {
    /// An`extern crate` item, with optional original crate name.
    ///
    /// E.g. `extern crate foo` or `extern crate foo_bar as foo`
    ExternCrate(Option<Ident>),
    /// A use declaration (`use` or `pub use`) item.
    ///
    /// E.g. `use foo;`, `use foo::bar;` or `use foo::bar as FooBar;`
    Use(Box<ViewPath>),
    /// A static item (`static` or `pub static`).
    ///
    /// E.g. `static FOO: i32 = 42;` or `static FOO: &'static str = "bar";`
    Static(Box<Ty>, Mutability, Box<Expr>),
    /// A constant item (`const` or `pub const`).
    ///
    /// E.g. `const FOO: i32 = 42;`
    Const(Box<Ty>, Box<Expr>),
    /// A function declaration (`fn` or `pub fn`).
    ///
    /// E.g. `fn foo(bar: usize) -> usize { .. }`
    Fn(Box<FnDecl>, Unsafety, Constness, Option<Abi>, Generics, Box<Block>),
    /// A module declaration (`mod` or `pub mod`).
    ///
    /// E.g. `mod foo;` or `mod foo { .. }`
    Mod(Vec<Item>),
    /// An external module (`extern` or `pub extern`).
    ///
    /// E.g. `extern {}` or `extern "C" {}`
    ForeignMod(ForeignMod),
    /// A type alias (`type` or `pub type`).
    ///
    /// E.g. `type Foo = Bar<u8>;`
    Ty(Box<Ty>, Generics),
    /// An enum definition (`enum` or `pub enum`).
    ///
    /// E.g. `enum Foo<A, B> { C<A>, D<B> }`
    Enum(Vec<Variant>, Generics),
    /// A struct definition (`struct` or `pub struct`).
    ///
    /// E.g. `struct Foo<A> { x: A }`
    Struct(VariantData, Generics),
    /// A union definition (`union` or `pub union`).
    ///
    /// E.g. `union Foo<A, B> { x: A, y: B }`
    Union(VariantData, Generics),
    /// A Trait declaration (`trait` or `pub trait`).
    ///
    /// E.g. `trait Foo { .. }` or `trait Foo<T> { .. }`
    Trait(Unsafety, Generics, Vec<TyParamBound>, Vec<TraitItem>),
    /// Default trait implementation.
    ///
    /// E.g. `impl Trait for .. {}` or `impl<T> Trait<T> for .. {}`
    DefaultImpl(Unsafety, Path),
    /// An implementation.
    ///
    /// E.g. `impl<A> Foo<A> { .. }` or `impl<A> Trait for Foo<A> { .. }`
    Impl(Unsafety,
         ImplPolarity,
         Generics,
         Option<Path>, // (optional) trait this impl implements
         Box<Ty>, // self
         Vec<ImplItem>),
    /// A macro invocation (which includes macro definition).
    ///
    /// E.g. `macro_rules! foo { .. }` or `foo!(..)`
    Mac(Mac),
}

#[derive(Debug, Clone, Eq, PartialEq)]
pub enum ViewPath {
    /// `foo::bar::baz as quux`
    ///
    /// or just
    ///
    /// `foo::bar::baz` (with `as baz` implicitly on the right)
    Simple(Path, Option<Ident>),

    /// `foo::bar::*`
    Glob(Path),

    /// `foo::bar::{a, b, c}`
    List(Path, Vec<PathListItem>),
}

#[derive(Debug, Clone, Eq, PartialEq)]
pub struct PathListItem {
    pub name: Ident,
    /// renamed in list, e.g. `use foo::{bar as baz};`
    pub rename: Option<Ident>,
}

#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum Unsafety {
    Unsafe,
    Normal,
}

#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum Constness {
    Const,
    NotConst,
}

#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum Defaultness {
    Default,
    Final,
}

#[derive(Debug, Clone, Eq, PartialEq)]
pub struct Abi(pub String);

/// Foreign module declaration.
///
/// E.g. `extern { .. }` or `extern "C" { .. }`
#[derive(Debug, Clone, Eq, PartialEq)]
pub struct ForeignMod {
    pub abi: Option<Abi>,
    pub items: Vec<ForeignItem>,
}

#[derive(Debug, Clone, Eq, PartialEq)]
pub struct ForeignItem {
    pub ident: Ident,
    pub attrs: Vec<Attribute>,
    pub node: ForeignItemKind,
    pub vis: Visibility,
}

/// An item within an `extern` block
#[derive(Debug, Clone, Eq, PartialEq)]
pub enum ForeignItemKind {
    /// A foreign function
    Fn(Box<FnDecl>, Generics),
    /// A foreign static item (`static ext: u8`)
    Static(Box<Ty>, Mutability),
}

/// Represents an item declaration within a trait declaration,
/// possibly including a default implementation. A trait item is
/// either required (meaning it doesn't have an implementation, just a
/// signature) or provided (meaning it has a default implementation).
#[derive(Debug, Clone, Eq, PartialEq)]
pub struct TraitItem {
    pub ident: Ident,
    pub attrs: Vec<Attribute>,
    pub node: TraitItemKind,
}

#[derive(Debug, Clone, Eq, PartialEq)]
pub enum TraitItemKind {
    Const(Ty, Option<Expr>),
    Method(MethodSig, Option<Block>),
    Type(Vec<TyParamBound>, Option<Ty>),
    Macro(Mac),
}

#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum ImplPolarity {
    /// `impl Trait for Type`
    Positive,
    /// `impl !Trait for Type`
    Negative,
}

#[derive(Debug, Clone, Eq, PartialEq)]
pub struct ImplItem {
    pub ident: Ident,
    pub vis: Visibility,
    pub defaultness: Defaultness,
    pub attrs: Vec<Attribute>,
    pub node: ImplItemKind,
}

#[derive(Debug, Clone, Eq, PartialEq)]
pub enum ImplItemKind {
    Const(Ty, Expr),
    Method(MethodSig, Block),
    Type(Ty),
    Macro(Mac),
}

/// Represents a method's signature in a trait declaration,
/// or in an implementation.
#[derive(Debug, Clone, Eq, PartialEq)]
pub struct MethodSig {
    pub unsafety: Unsafety,
    pub constness: Constness,
    pub abi: Option<Abi>,
    pub decl: FnDecl,
    pub generics: Generics,
}

/// Header (not the body) of a function declaration.
///
/// E.g. `fn foo(bar: baz)`
#[derive(Debug, Clone, Eq, PartialEq)]
pub struct FnDecl {
    pub inputs: Vec<FnArg>,
    pub output: FunctionRetTy,
}

/// An argument in a function header.
///
/// E.g. `bar: usize` as in `fn foo(bar: usize)`
#[derive(Debug, Clone, Eq, PartialEq)]
pub enum FnArg {
    SelfRef(Option<Lifetime>, Mutability),
    SelfValue(Mutability),
    Captured(Pat, Ty),
    Ignored(Ty),
}

#[cfg(feature = "parsing")]
pub mod parsing {
    use super::*;
    use {DelimToken, FunctionRetTy, Generics, Ident, Mac, Path, TokenTree, VariantData, Visibility};
    use attr::parsing::outer_attr;
    use data::parsing::{struct_like_body, visibility};
    use expr::parsing::{block, expr, pat};
    use generics::parsing::{generics, lifetime, ty_param_bound, where_clause};
    use ident::parsing::ident;
    use lit::parsing::quoted_string;
    use mac::parsing::delimited;
    use macro_input::{Body, MacroInput};
    use macro_input::parsing::macro_input;
    use ty::parsing::{mutability, path, ty};

    named!(pub item -> Item, alt!(
        item_extern_crate
        |
        item_use
        |
        item_static
        |
        item_const
        |
        item_fn
        |
        item_mod
        |
        item_foreign_mod
        |
        item_ty
        |
        item_struct_or_enum
        |
        item_union
        |
        item_trait
        |
        item_default_impl
        |
        item_impl
        |
        item_mac
    ));

    named!(item_mac -> Item, do_parse!(
        attrs: many0!(outer_attr) >>
        path: ident >>
        punct!("!") >>
        name: option!(ident) >>
        body: delimited >>
        cond!(match body.delim {
            DelimToken::Paren | DelimToken::Bracket => true,
            DelimToken::Brace => false,
        }, punct!(";")) >>
        (Item {
            ident: name.unwrap_or_else(|| Ident::new("")),
            vis: Visibility::Inherited,
            attrs: attrs,
            node: ItemKind::Mac(Mac {
                path: path.into(),
                tts: vec![TokenTree::Delimited(body)],
            }),
        })
    ));

    named!(item_extern_crate -> Item, do_parse!(
        attrs: many0!(outer_attr) >>
        vis: visibility >>
        keyword!("extern") >>
        keyword!("crate") >>
        id: ident >>
        rename: option!(preceded!(
            keyword!("as"),
            ident
        )) >>
        punct!(";") >>
        ({
            let (name, original_name) = match rename {
                Some(rename) => (rename, Some(id)),
                None => (id, None),
            };
            Item {
                ident: name,
                vis: vis,
                attrs: attrs,
                node: ItemKind::ExternCrate(original_name),
            }
        })
    ));

    named!(item_use -> Item, do_parse!(
        attrs: many0!(outer_attr) >>
        vis: visibility >>
        keyword!("use") >>
        what: view_path >>
        punct!(";") >>
        (Item {
            ident: "".into(),
            vis: vis,
            attrs: attrs,
            node: ItemKind::Use(Box::new(what)),
        })
    ));

    named!(view_path -> ViewPath, alt!(
        view_path_glob
        |
        view_path_list
        |
        view_path_list_root
        |
        view_path_simple // must be last
    ));


    named!(view_path_simple -> ViewPath, do_parse!(
        path: path >>
        rename: option!(preceded!(keyword!("as"), ident)) >>
        (ViewPath::Simple(path, rename))
    ));

    named!(view_path_glob -> ViewPath, do_parse!(
        path: path >>
        punct!("::") >>
        punct!("*") >>
        (ViewPath::Glob(path))
    ));

    named!(view_path_list -> ViewPath, do_parse!(
        path: path >>
        punct!("::") >>
        punct!("{") >>
        items: separated_nonempty_list!(punct!(","), path_list_item) >>
        punct!("}") >>
        (ViewPath::List(path, items))
    ));

    named!(view_path_list_root -> ViewPath, do_parse!(
        global: option!(punct!("::")) >>
        punct!("{") >>
        items: separated_nonempty_list!(punct!(","), path_list_item) >>
        punct!("}") >>
        (ViewPath::List(Path {
            global: global.is_some(),
            segments: Vec::new(),
        }, items))
    ));

    named!(path_list_item -> PathListItem, do_parse!(
        name: ident >>
        rename: option!(preceded!(keyword!("as"), ident)) >>
        (PathListItem {
            name: name,
            rename: rename,
        })
    ));

    named!(item_static -> Item, do_parse!(
        attrs: many0!(outer_attr) >>
        vis: visibility >>
        keyword!("static") >>
        mutability: mutability >>
        id: ident >>
        punct!(":") >>
        ty: ty >>
        punct!("=") >>
        value: expr >>
        punct!(";") >>
        (Item {
            ident: id,
            vis: vis,
            attrs: attrs,
            node: ItemKind::Static(Box::new(ty), mutability, Box::new(value)),
        })
    ));

    named!(item_const -> Item, do_parse!(
        attrs: many0!(outer_attr) >>
        vis: visibility >>
        keyword!("const") >>
        id: ident >>
        punct!(":") >>
        ty: ty >>
        punct!("=") >>
        value: expr >>
        punct!(";") >>
        (Item {
            ident: id,
            vis: vis,
            attrs: attrs,
            node: ItemKind::Const(Box::new(ty), Box::new(value)),
        })
    ));

    named!(item_fn -> Item, do_parse!(
        attrs: many0!(outer_attr) >>
        vis: visibility >>
        constness: constness >>
        unsafety: unsafety >>
        abi: option!(preceded!(keyword!("extern"), quoted_string)) >>
        keyword!("fn") >>
        name: ident >>
        generics: generics >>
        punct!("(") >>
        inputs: terminated_list!(punct!(","), fn_arg) >>
        punct!(")") >>
        ret: option!(preceded!(punct!("->"), ty)) >>
        where_clause: where_clause >>
        body: block >>
        (Item {
            ident: name,
            vis: vis,
            attrs: attrs,
            node: ItemKind::Fn(
                Box::new(FnDecl {
                    inputs: inputs,
                    output: ret.map(FunctionRetTy::Ty).unwrap_or(FunctionRetTy::Default),
                }),
                unsafety,
                constness,
                abi.map(Abi),
                Generics {
                    where_clause: where_clause,
                    .. generics
                },
                Box::new(body),
            ),
        })
    ));

    named!(fn_arg -> FnArg, alt!(
        do_parse!(
            punct!("&") >>
            lt: option!(lifetime) >>
            mutability: mutability >>
            keyword!("self") >>
            (FnArg::SelfRef(lt, mutability))
        )
        |
        do_parse!(
            mutability: mutability >>
            keyword!("self") >>
            (FnArg::SelfValue(mutability))
        )
        |
        do_parse!(
            pat: pat >>
            punct!(":") >>
            ty: ty >>
            (FnArg::Captured(pat, ty))
        )
        |
        ty => { FnArg::Ignored }
    ));

    named!(item_mod -> Item, do_parse!(
        attrs: many0!(outer_attr) >>
        vis: visibility >>
        keyword!("mod") >>
        id: ident >>
        punct!("{") >>
        items: many0!(item) >>
        punct!("}") >>
        (Item {
            ident: id,
            vis: vis,
            attrs: attrs,
            node: ItemKind::Mod(items),
        })
    ));

    named!(item_foreign_mod -> Item, do_parse!(
        attrs: many0!(outer_attr) >>
        keyword!("extern") >>
        abi: option!(quoted_string) >>
        punct!("{") >>
        items: many0!(foreign_item) >>
        punct!("}") >>
        (Item {
            ident: "".into(),
            vis: Visibility::Inherited,
            attrs: attrs,
            node: ItemKind::ForeignMod(ForeignMod {
                abi: abi.map(Abi),
                items: items,
            }),
        })
    ));

    named!(foreign_item -> ForeignItem, alt!(
        foreign_fn
        |
        foreign_static
    ));

    named!(foreign_fn -> ForeignItem, do_parse!(
        attrs: many0!(outer_attr) >>
        vis: visibility >>
        keyword!("fn") >>
        name: ident >>
        generics: generics >>
        punct!("(") >>
        inputs: terminated_list!(punct!(","), fn_arg) >>
        punct!(")") >>
        ret: option!(preceded!(punct!("->"), ty)) >>
        where_clause: where_clause >>
        punct!(";") >>
        (ForeignItem {
            ident: name,
            attrs: attrs,
            node: ForeignItemKind::Fn(
                Box::new(FnDecl {
                    inputs: inputs,
                    output: ret.map(FunctionRetTy::Ty).unwrap_or(FunctionRetTy::Default),
                }),
                Generics {
                    where_clause: where_clause,
                    .. generics
                },
            ),
            vis: vis,
        })
    ));

    named!(foreign_static -> ForeignItem, do_parse!(
        attrs: many0!(outer_attr) >>
        vis: visibility >>
        keyword!("static") >>
        mutability: mutability >>
        id: ident >>
        punct!(":") >>
        ty: ty >>
        punct!(";") >>
        (ForeignItem {
            ident: id,
            attrs: attrs,
            node: ForeignItemKind::Static(Box::new(ty), mutability),
            vis: vis,
        })
    ));

    named!(item_ty -> Item, do_parse!(
        attrs: many0!(outer_attr) >>
        vis: visibility >>
        keyword!("type") >>
        id: ident >>
        generics: generics >>
        punct!("=") >>
        ty: ty >>
        punct!(";") >>
        (Item {
            ident: id,
            vis: vis,
            attrs: attrs,
            node: ItemKind::Ty(Box::new(ty), generics),
        })
    ));

    named!(item_struct_or_enum -> Item, map!(
        macro_input,
        |def: MacroInput| Item {
            ident: def.ident,
            vis: def.vis,
            attrs: def.attrs,
            node: match def.body {
                Body::Enum(variants) => {
                    ItemKind::Enum(variants, def.generics)
                }
                Body::Struct(variant_data) => {
                    ItemKind::Struct(variant_data, def.generics)
                }
            }
        }
    ));

    named!(item_union -> Item, do_parse!(
        attrs: many0!(outer_attr) >>
        vis: visibility >>
        keyword!("union") >>
        id: ident >>
        generics: generics >>
        where_clause: where_clause >>
        fields: struct_like_body >>
        (Item {
            ident: id,
            vis: vis,
            attrs: attrs,
            node: ItemKind::Union(
                VariantData::Struct(fields),
                Generics {
                    where_clause: where_clause,
                    .. generics
                },
            ),
        })
    ));

    named!(item_trait -> Item, do_parse!(
        attrs: many0!(outer_attr) >>
        vis: visibility >>
        unsafety: unsafety >>
        keyword!("trait") >>
        id: ident >>
        generics: generics >>
        bounds: opt_vec!(preceded!(
            punct!(":"),
            separated_nonempty_list!(punct!("+"), ty_param_bound)
        )) >>
        where_clause: where_clause >>
        punct!("{") >>
        body: many0!(trait_item) >>
        punct!("}") >>
        (Item {
            ident: id,
            vis: vis,
            attrs: attrs,
            node: ItemKind::Trait(
                unsafety,
                Generics {
                    where_clause: where_clause,
                    .. generics
                },
                bounds,
                body,
            ),
        })
    ));

    named!(item_default_impl -> Item, do_parse!(
        attrs: many0!(outer_attr) >>
        unsafety: unsafety >>
        keyword!("impl") >>
        path: path >>
        keyword!("for") >>
        punct!("..") >>
        punct!("{") >>
        punct!("}") >>
        (Item {
            ident: "".into(),
            vis: Visibility::Inherited,
            attrs: attrs,
            node: ItemKind::DefaultImpl(unsafety, path),
        })
    ));

    named!(trait_item -> TraitItem, alt!(
        trait_item_const
        |
        trait_item_method
        |
        trait_item_type
        |
        trait_item_mac
    ));

    named!(trait_item_const -> TraitItem, do_parse!(
        attrs: many0!(outer_attr) >>
        keyword!("const") >>
        id: ident >>
        punct!(":") >>
        ty: ty >>
        value: option!(preceded!(punct!("="), expr)) >>
        punct!(";") >>
        (TraitItem {
            ident: id,
            attrs: attrs,
            node: TraitItemKind::Const(ty, value),
        })
    ));

    named!(trait_item_method -> TraitItem, do_parse!(
        attrs: many0!(outer_attr) >>
        constness: constness >>
        unsafety: unsafety >>
        abi: option!(preceded!(keyword!("extern"), quoted_string)) >>
        keyword!("fn") >>
        name: ident >>
        generics: generics >>
        punct!("(") >>
        inputs: terminated_list!(punct!(","), fn_arg) >>
        punct!(")") >>
        ret: option!(preceded!(punct!("->"), ty)) >>
        where_clause: where_clause >>
        body: option!(block) >>
        cond!(body.is_none(), punct!(";")) >>
        (TraitItem {
            ident: name,
            attrs: attrs,
            node: TraitItemKind::Method(
                MethodSig {
                    unsafety: unsafety,
                    constness: constness,
                    abi: abi.map(Abi),
                    decl: FnDecl {
                        inputs: inputs,
                        output: ret.map(FunctionRetTy::Ty).unwrap_or(FunctionRetTy::Default),
                    },
                    generics: Generics {
                        where_clause: where_clause,
                        .. generics
                    },
                },
                body,
            ),
        })
    ));

    named!(trait_item_type -> TraitItem, do_parse!(
        attrs: many0!(outer_attr) >>
        keyword!("type") >>
        id: ident >>
        bounds: opt_vec!(preceded!(
            punct!(":"),
            separated_nonempty_list!(punct!("+"), ty_param_bound)
        )) >>
        default: option!(preceded!(punct!("="), ty)) >>
        punct!(";") >>
        (TraitItem {
            ident: id,
            attrs: attrs,
            node: TraitItemKind::Type(bounds, default),
        })
    ));

    named!(trait_item_mac -> TraitItem, do_parse!(
        attrs: many0!(outer_attr) >>
        id: ident >>
        punct!("!") >>
        body: delimited >>
        cond!(match body.delim {
            DelimToken::Paren | DelimToken::Bracket => true,
            DelimToken::Brace => false,
        }, punct!(";")) >>
        (TraitItem {
            ident: id.clone(),
            attrs: attrs,
            node: TraitItemKind::Macro(Mac {
                path: id.into(),
                tts: vec![TokenTree::Delimited(body)],
            }),
        })
    ));

    named!(item_impl -> Item, do_parse!(
        attrs: many0!(outer_attr) >>
        unsafety: unsafety >>
        keyword!("impl") >>
        generics: generics >>
        polarity_path: alt!(
            do_parse!(
                polarity: impl_polarity >>
                path: path >>
                keyword!("for") >>
                ((polarity, Some(path)))
            )
            |
            epsilon!() => { |_| (ImplPolarity::Positive, None) }
        ) >>
        self_ty: ty >>
        where_clause: where_clause >>
        punct!("{") >>
        body: many0!(impl_item) >>
        punct!("}") >>
        (Item {
            ident: "".into(),
            vis: Visibility::Inherited,
            attrs: attrs,
            node: ItemKind::Impl(
                unsafety,
                polarity_path.0,
                Generics {
                    where_clause: where_clause,
                    .. generics
                },
                polarity_path.1,
                Box::new(self_ty),
                body,
            ),
        })
    ));

    named!(impl_item -> ImplItem, alt!(
        impl_item_const
        |
        impl_item_method
        |
        impl_item_type
        |
        impl_item_macro
    ));

    named!(impl_item_const -> ImplItem, do_parse!(
        attrs: many0!(outer_attr) >>
        vis: visibility >>
        defaultness: defaultness >>
        keyword!("const") >>
        id: ident >>
        punct!(":") >>
        ty: ty >>
        punct!("=") >>
        value: expr >>
        punct!(";") >>
        (ImplItem {
            ident: id,
            vis: vis,
            defaultness: defaultness,
            attrs: attrs,
            node: ImplItemKind::Const(ty, value),
        })
    ));

    named!(impl_item_method -> ImplItem, do_parse!(
        attrs: many0!(outer_attr) >>
        vis: visibility >>
        defaultness: defaultness >>
        constness: constness >>
        unsafety: unsafety >>
        abi: option!(preceded!(keyword!("extern"), quoted_string)) >>
        keyword!("fn") >>
        name: ident >>
        generics: generics >>
        punct!("(") >>
        inputs: terminated_list!(punct!(","), fn_arg) >>
        punct!(")") >>
        ret: option!(preceded!(punct!("->"), ty)) >>
        where_clause: where_clause >>
        body: block >>
        (ImplItem {
            ident: name,
            vis: vis,
            defaultness: defaultness,
            attrs: attrs,
            node: ImplItemKind::Method(
                MethodSig {
                    unsafety: unsafety,
                    constness: constness,
                    abi: abi.map(Abi),
                    decl: FnDecl {
                        inputs: inputs,
                        output: ret.map(FunctionRetTy::Ty).unwrap_or(FunctionRetTy::Default),
                    },
                    generics: Generics {
                        where_clause: where_clause,
                        .. generics
                    },
                },
                body,
            ),
        })
    ));

    named!(impl_item_type -> ImplItem, do_parse!(
        attrs: many0!(outer_attr) >>
        vis: visibility >>
        defaultness: defaultness >>
        keyword!("type") >>
        id: ident >>
        punct!("=") >>
        ty: ty >>
        punct!(";") >>
        (ImplItem {
            ident: id,
            vis: vis,
            defaultness: defaultness,
            attrs: attrs,
            node: ImplItemKind::Type(ty),
        })
    ));

    named!(impl_item_macro -> ImplItem, do_parse!(
        attrs: many0!(outer_attr) >>
        id: ident >>
        punct!("!") >>
        body: delimited >>
        cond!(match body.delim {
            DelimToken::Paren | DelimToken::Bracket => true,
            DelimToken::Brace => false,
        }, punct!(";")) >>
        (ImplItem {
            ident: id.clone(),
            vis: Visibility::Inherited,
            defaultness: Defaultness::Final,
            attrs: attrs,
            node: ImplItemKind::Macro(Mac {
                path: id.into(),
                tts: vec![TokenTree::Delimited(body)],
            }),
        })
    ));

    named!(impl_polarity -> ImplPolarity, alt!(
        punct!("!") => { |_| ImplPolarity::Negative }
        |
        epsilon!() => { |_| ImplPolarity::Positive }
    ));

    named!(constness -> Constness, alt!(
        keyword!("const") => { |_| Constness::Const }
        |
        epsilon!() => { |_| Constness::NotConst }
    ));

    named!(unsafety -> Unsafety, alt!(
        keyword!("unsafe") => { |_| Unsafety::Unsafe }
        |
        epsilon!() => { |_| Unsafety::Normal }
    ));

    named!(defaultness -> Defaultness, alt!(
        keyword!("default") => { |_| Defaultness::Default }
        |
        epsilon!() => { |_| Defaultness::Final }
    ));
}

#[cfg(feature = "printing")]
mod printing {
    use super::*;
    use {Delimited, DelimToken, FunctionRetTy, TokenTree};
    use attr::FilterAttrs;
    use data::VariantData;
    use quote::{Tokens, ToTokens};

    impl ToTokens for Item {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            match self.node {
                ItemKind::ExternCrate(ref original) => {
                    tokens.append("extern");
                    tokens.append("crate");
                    if let Some(ref original) = *original {
                        original.to_tokens(tokens);
                        tokens.append("as");
                    }
                    self.ident.to_tokens(tokens);
                    tokens.append(";");
                }
                ItemKind::Use(ref view_path) => {
                    self.vis.to_tokens(tokens);
                    tokens.append("use");
                    view_path.to_tokens(tokens);
                    tokens.append(";");
                }
                ItemKind::Static(ref ty, ref mutability, ref expr) => {
                    self.vis.to_tokens(tokens);
                    tokens.append("static");
                    mutability.to_tokens(tokens);
                    self.ident.to_tokens(tokens);
                    tokens.append(":");
                    ty.to_tokens(tokens);
                    tokens.append("=");
                    expr.to_tokens(tokens);
                    tokens.append(";");
                }
                ItemKind::Const(ref ty, ref expr) => {
                    self.vis.to_tokens(tokens);
                    tokens.append("const");
                    self.ident.to_tokens(tokens);
                    tokens.append(":");
                    ty.to_tokens(tokens);
                    tokens.append("=");
                    expr.to_tokens(tokens);
                    tokens.append(";");
                }
                ItemKind::Fn(ref decl, unsafety, constness, ref abi, ref generics, ref block) => {
                    self.vis.to_tokens(tokens);
                    constness.to_tokens(tokens);
                    unsafety.to_tokens(tokens);
                    abi.to_tokens(tokens);
                    tokens.append("fn");
                    self.ident.to_tokens(tokens);
                    generics.to_tokens(tokens);
                    tokens.append("(");
                    tokens.append_separated(&decl.inputs, ",");
                    tokens.append(")");
                    if let FunctionRetTy::Ty(ref ty) = decl.output {
                        tokens.append("->");
                        ty.to_tokens(tokens);
                    }
                    generics.where_clause.to_tokens(tokens);
                    block.to_tokens(tokens);
                }
                ItemKind::Mod(ref items) => {
                    self.vis.to_tokens(tokens);
                    tokens.append("mod");
                    self.ident.to_tokens(tokens);
                    tokens.append("{");
                    tokens.append_all(items);
                    tokens.append("}");
                }
                ItemKind::ForeignMod(ref foreign_mod) => {
                    self.vis.to_tokens(tokens);
                    match foreign_mod.abi {
                        Some(ref abi) => abi.to_tokens(tokens),
                        None => tokens.append("extern"),
                    }
                    tokens.append("{");
                    tokens.append_all(&foreign_mod.items);
                    tokens.append("}");
                }
                ItemKind::Ty(ref ty, ref generics) => {
                    self.vis.to_tokens(tokens);
                    tokens.append("type");
                    self.ident.to_tokens(tokens);
                    generics.to_tokens(tokens);
                    tokens.append("=");
                    ty.to_tokens(tokens);
                    tokens.append(";");
                }
                ItemKind::Enum(ref variants, ref generics) => {
                    self.vis.to_tokens(tokens);
                    tokens.append("enum");
                    self.ident.to_tokens(tokens);
                    generics.to_tokens(tokens);
                    generics.where_clause.to_tokens(tokens);
                    tokens.append("{");
                    for variant in variants {
                        variant.to_tokens(tokens);
                        tokens.append(",");
                    }
                    tokens.append("}");
                }
                ItemKind::Struct(ref variant_data, ref generics) => {
                    self.vis.to_tokens(tokens);
                    tokens.append("struct");
                    self.ident.to_tokens(tokens);
                    generics.to_tokens(tokens);
                    generics.where_clause.to_tokens(tokens);
                    variant_data.to_tokens(tokens);
                    match *variant_data {
                        VariantData::Struct(_) => {
                            // no semicolon
                        }
                        VariantData::Tuple(_) |
                        VariantData::Unit => tokens.append(";"),
                    }
                }
                ItemKind::Union(ref variant_data, ref generics) => {
                    self.vis.to_tokens(tokens);
                    tokens.append("union");
                    self.ident.to_tokens(tokens);
                    generics.to_tokens(tokens);
                    generics.where_clause.to_tokens(tokens);
                    variant_data.to_tokens(tokens);
                }
                ItemKind::Trait(unsafety, ref generics, ref bound, ref items) => {
                    self.vis.to_tokens(tokens);
                    unsafety.to_tokens(tokens);
                    tokens.append("trait");
                    self.ident.to_tokens(tokens);
                    if !bound.is_empty() {
                        tokens.append(":");
                        tokens.append_separated(bound, "+");
                    }
                    generics.to_tokens(tokens);
                    generics.where_clause.to_tokens(tokens);
                    tokens.append("{");
                    tokens.append_all(items);
                    tokens.append("}");
                }
                ItemKind::DefaultImpl(unsafety, ref path) => {
                    unsafety.to_tokens(tokens);
                    tokens.append("impl");
                    path.to_tokens(tokens);
                    tokens.append("for");
                    tokens.append("..");
                    tokens.append("{");
                    tokens.append("}");
                }
                ItemKind::Impl(unsafety, polarity, ref generics, ref path, ref ty, ref items) => {
                    unsafety.to_tokens(tokens);
                    tokens.append("impl");
                    generics.to_tokens(tokens);
                    if let Some(ref path) = *path {
                        polarity.to_tokens(tokens);
                        path.to_tokens(tokens);
                        tokens.append("for");
                    }
                    ty.to_tokens(tokens);
                    generics.where_clause.to_tokens(tokens);
                    tokens.append("{");
                    tokens.append_all(items);
                    tokens.append("}");
                }
                ItemKind::Mac(ref mac) => {
                    mac.path.to_tokens(tokens);
                    tokens.append("!");
                    self.ident.to_tokens(tokens);
                    for tt in &mac.tts {
                        tt.to_tokens(tokens);
                    }
                    match mac.tts.last() {
                        Some(&TokenTree::Delimited(Delimited { delim: DelimToken::Brace, .. })) => {
                            // no semicolon
                        }
                        _ => tokens.append(";"),
                    }
                }
            }
        }
    }

    impl ToTokens for ViewPath {
        fn to_tokens(&self, tokens: &mut Tokens) {
            match *self {
                ViewPath::Simple(ref path, ref rename) => {
                    path.to_tokens(tokens);
                    if let Some(ref rename) = *rename {
                        tokens.append("as");
                        rename.to_tokens(tokens);
                    }
                }
                ViewPath::Glob(ref path) => {
                    path.to_tokens(tokens);
                    tokens.append("::");
                    tokens.append("*");
                }
                ViewPath::List(ref path, ref items) => {
                    path.to_tokens(tokens);
                    if path.global || !path.segments.is_empty() {
                        tokens.append("::");
                    }
                    tokens.append("{");
                    tokens.append_separated(items, ",");
                    tokens.append("}");
                }
            }
        }
    }

    impl ToTokens for PathListItem {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.name.to_tokens(tokens);
            if let Some(ref rename) = self.rename {
                tokens.append("as");
                rename.to_tokens(tokens);
            }
        }
    }

    impl ToTokens for TraitItem {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            match self.node {
                TraitItemKind::Const(ref ty, ref expr) => {
                    tokens.append("const");
                    self.ident.to_tokens(tokens);
                    tokens.append(":");
                    ty.to_tokens(tokens);
                    if let Some(ref expr) = *expr {
                        tokens.append("=");
                        expr.to_tokens(tokens);
                    }
                    tokens.append(";");
                }
                TraitItemKind::Method(ref sig, ref block) => {
                    sig.unsafety.to_tokens(tokens);
                    sig.abi.to_tokens(tokens);
                    tokens.append("fn");
                    self.ident.to_tokens(tokens);
                    sig.generics.to_tokens(tokens);
                    tokens.append("(");
                    tokens.append_separated(&sig.decl.inputs, ",");
                    tokens.append(")");
                    if let FunctionRetTy::Ty(ref ty) = sig.decl.output {
                        tokens.append("->");
                        ty.to_tokens(tokens);
                    }
                    sig.generics.where_clause.to_tokens(tokens);
                    match *block {
                        Some(ref block) => block.to_tokens(tokens),
                        None => tokens.append(";"),
                    }
                }
                TraitItemKind::Type(ref bound, ref default) => {
                    tokens.append("type");
                    self.ident.to_tokens(tokens);
                    if !bound.is_empty() {
                        tokens.append(":");
                        tokens.append_separated(bound, "+");
                    }
                    if let Some(ref default) = *default {
                        tokens.append("=");
                        default.to_tokens(tokens);
                    }
                    tokens.append(";");
                }
                TraitItemKind::Macro(ref mac) => {
                    mac.to_tokens(tokens);
                    match mac.tts.last() {
                        Some(&TokenTree::Delimited(Delimited { delim: DelimToken::Brace, .. })) => {
                            // no semicolon
                        }
                        _ => tokens.append(";"),
                    }
                }
            }
        }
    }

    impl ToTokens for ImplItem {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            match self.node {
                ImplItemKind::Const(ref ty, ref expr) => {
                    self.vis.to_tokens(tokens);
                    self.defaultness.to_tokens(tokens);
                    tokens.append("const");
                    self.ident.to_tokens(tokens);
                    tokens.append(":");
                    ty.to_tokens(tokens);
                    tokens.append("=");
                    expr.to_tokens(tokens);
                    tokens.append(";");
                }
                ImplItemKind::Method(ref sig, ref block) => {
                    self.vis.to_tokens(tokens);
                    self.defaultness.to_tokens(tokens);
                    sig.unsafety.to_tokens(tokens);
                    sig.abi.to_tokens(tokens);
                    tokens.append("fn");
                    self.ident.to_tokens(tokens);
                    sig.generics.to_tokens(tokens);
                    tokens.append("(");
                    tokens.append_separated(&sig.decl.inputs, ",");
                    tokens.append(")");
                    if let FunctionRetTy::Ty(ref ty) = sig.decl.output {
                        tokens.append("->");
                        ty.to_tokens(tokens);
                    }
                    sig.generics.where_clause.to_tokens(tokens);
                    block.to_tokens(tokens);
                }
                ImplItemKind::Type(ref ty) => {
                    self.vis.to_tokens(tokens);
                    self.defaultness.to_tokens(tokens);
                    tokens.append("type");
                    self.ident.to_tokens(tokens);
                    tokens.append("=");
                    ty.to_tokens(tokens);
                    tokens.append(";");
                }
                ImplItemKind::Macro(ref mac) => {
                    mac.to_tokens(tokens);
                    match mac.tts.last() {
                        Some(&TokenTree::Delimited(Delimited { delim: DelimToken::Brace, .. })) => {
                            // no semicolon
                        }
                        _ => tokens.append(";"),
                    }
                }
            }
        }
    }

    impl ToTokens for ForeignItem {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            match self.node {
                ForeignItemKind::Fn(ref decl, ref generics) => {
                    self.vis.to_tokens(tokens);
                    tokens.append("fn");
                    self.ident.to_tokens(tokens);
                    generics.to_tokens(tokens);
                    tokens.append("(");
                    tokens.append_separated(&decl.inputs, ",");
                    tokens.append(")");
                    if let FunctionRetTy::Ty(ref ty) = decl.output {
                        tokens.append("->");
                        ty.to_tokens(tokens);
                    }
                    generics.where_clause.to_tokens(tokens);
                    tokens.append(";");
                }
                ForeignItemKind::Static(ref ty, mutability) => {
                    self.vis.to_tokens(tokens);
                    tokens.append("static");
                    mutability.to_tokens(tokens);
                    self.ident.to_tokens(tokens);
                    tokens.append(":");
                    ty.to_tokens(tokens);
                    tokens.append(";");
                }
            }
        }
    }

    impl ToTokens for FnArg {
        fn to_tokens(&self, tokens: &mut Tokens) {
            match *self {
                FnArg::SelfRef(ref lifetime, mutability) => {
                    tokens.append("&");
                    lifetime.to_tokens(tokens);
                    mutability.to_tokens(tokens);
                    tokens.append("self");
                }
                FnArg::SelfValue(mutability) => {
                    mutability.to_tokens(tokens);
                    tokens.append("self");
                }
                FnArg::Captured(ref pat, ref ty) => {
                    pat.to_tokens(tokens);
                    tokens.append(":");
                    ty.to_tokens(tokens);
                }
                FnArg::Ignored(ref ty) => {
                    ty.to_tokens(tokens);
                }
            }
        }
    }

    impl ToTokens for Unsafety {
        fn to_tokens(&self, tokens: &mut Tokens) {
            match *self {
                Unsafety::Unsafe => tokens.append("unsafe"),
                Unsafety::Normal => {
                    // nothing
                }
            }
        }
    }

    impl ToTokens for Constness {
        fn to_tokens(&self, tokens: &mut Tokens) {
            match *self {
                Constness::Const => tokens.append("const"),
                Constness::NotConst => {
                    // nothing
                }
            }
        }
    }

    impl ToTokens for Defaultness {
        fn to_tokens(&self, tokens: &mut Tokens) {
            match *self {
                Defaultness::Default => tokens.append("default"),
                Defaultness::Final => {
                    // nothing
                }
            }
        }
    }

    impl ToTokens for ImplPolarity {
        fn to_tokens(&self, tokens: &mut Tokens) {
            match *self {
                ImplPolarity::Negative => tokens.append("!"),
                ImplPolarity::Positive => {
                    // nothing
                }
            }
        }
    }

    impl ToTokens for Abi {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append("extern");
            self.0.to_tokens(tokens);
        }
    }
}
