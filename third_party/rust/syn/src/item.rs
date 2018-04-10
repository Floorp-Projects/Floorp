// Copyright 2018 Syn Developers
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::*;
use derive::{Data, DeriveInput};
use punctuated::Punctuated;
use proc_macro2::TokenStream;
use token::{Brace, Paren};

#[cfg(feature = "extra-traits")]
use tt::TokenStreamHelper;
#[cfg(feature = "extra-traits")]
use std::hash::{Hash, Hasher};

ast_enum_of_structs! {
    /// Things that can appear directly inside of a module or scope.
    ///
    /// *This type is available if Syn is built with the `"full"` feature.*
    ///
    /// # Syntax tree enum
    ///
    /// This type is a [syntax tree enum].
    ///
    /// [syntax tree enum]: enum.Expr.html#syntax-tree-enums
    pub enum Item {
        /// An `extern crate` item: `extern crate serde`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub ExternCrate(ItemExternCrate {
            pub attrs: Vec<Attribute>,
            pub vis: Visibility,
            pub extern_token: Token![extern],
            pub crate_token: Token![crate],
            pub ident: Ident,
            pub rename: Option<(Token![as], Ident)>,
            pub semi_token: Token![;],
        }),

        /// A use declaration: `use std::collections::HashMap`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Use(ItemUse {
            pub attrs: Vec<Attribute>,
            pub vis: Visibility,
            pub use_token: Token![use],
            pub leading_colon: Option<Token![::]>,
            pub tree: UseTree,
            pub semi_token: Token![;],
        }),

        /// A static item: `static BIKE: Shed = Shed(42)`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Static(ItemStatic {
            pub attrs: Vec<Attribute>,
            pub vis: Visibility,
            pub static_token: Token![static],
            pub mutability: Option<Token![mut]>,
            pub ident: Ident,
            pub colon_token: Token![:],
            pub ty: Box<Type>,
            pub eq_token: Token![=],
            pub expr: Box<Expr>,
            pub semi_token: Token![;],
        }),

        /// A constant item: `const MAX: u16 = 65535`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Const(ItemConst {
            pub attrs: Vec<Attribute>,
            pub vis: Visibility,
            pub const_token: Token![const],
            pub ident: Ident,
            pub colon_token: Token![:],
            pub ty: Box<Type>,
            pub eq_token: Token![=],
            pub expr: Box<Expr>,
            pub semi_token: Token![;],
        }),

        /// A free-standing function: `fn process(n: usize) -> Result<()> { ...
        /// }`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Fn(ItemFn {
            pub attrs: Vec<Attribute>,
            pub vis: Visibility,
            pub constness: Option<Token![const]>,
            pub unsafety: Option<Token![unsafe]>,
            pub abi: Option<Abi>,
            pub ident: Ident,
            pub decl: Box<FnDecl>,
            pub block: Box<Block>,
        }),

        /// A module or module declaration: `mod m` or `mod m { ... }`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Mod(ItemMod {
            pub attrs: Vec<Attribute>,
            pub vis: Visibility,
            pub mod_token: Token![mod],
            pub ident: Ident,
            pub content: Option<(token::Brace, Vec<Item>)>,
            pub semi: Option<Token![;]>,
        }),

        /// A block of foreign items: `extern "C" { ... }`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub ForeignMod(ItemForeignMod {
            pub attrs: Vec<Attribute>,
            pub abi: Abi,
            pub brace_token: token::Brace,
            pub items: Vec<ForeignItem>,
        }),

        /// A type alias: `type Result<T> = std::result::Result<T, MyError>`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Type(ItemType {
            pub attrs: Vec<Attribute>,
            pub vis: Visibility,
            pub type_token: Token![type],
            pub ident: Ident,
            pub generics: Generics,
            pub eq_token: Token![=],
            pub ty: Box<Type>,
            pub semi_token: Token![;],
        }),

        /// A struct definition: `struct Foo<A> { x: A }`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Struct(ItemStruct {
            pub attrs: Vec<Attribute>,
            pub vis: Visibility,
            pub struct_token: Token![struct],
            pub ident: Ident,
            pub generics: Generics,
            pub fields: Fields,
            pub semi_token: Option<Token![;]>,
        }),

        /// An enum definition: `enum Foo<A, B> { C<A>, D<B> }`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Enum(ItemEnum {
            pub attrs: Vec<Attribute>,
            pub vis: Visibility,
            pub enum_token: Token![enum],
            pub ident: Ident,
            pub generics: Generics,
            pub brace_token: token::Brace,
            pub variants: Punctuated<Variant, Token![,]>,
        }),

        /// A union definition: `union Foo<A, B> { x: A, y: B }`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Union(ItemUnion {
            pub attrs: Vec<Attribute>,
            pub vis: Visibility,
            pub union_token: Token![union],
            pub ident: Ident,
            pub generics: Generics,
            pub fields: FieldsNamed,
        }),

        /// A trait definition: `pub trait Iterator { ... }`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Trait(ItemTrait {
            pub attrs: Vec<Attribute>,
            pub vis: Visibility,
            pub unsafety: Option<Token![unsafe]>,
            pub auto_token: Option<Token![auto]>,
            pub trait_token: Token![trait],
            pub ident: Ident,
            pub generics: Generics,
            pub colon_token: Option<Token![:]>,
            pub supertraits: Punctuated<TypeParamBound, Token![+]>,
            pub brace_token: token::Brace,
            pub items: Vec<TraitItem>,
        }),

        /// An impl block providing trait or associated items: `impl<A> Trait
        /// for Data<A> { ... }`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Impl(ItemImpl {
            pub attrs: Vec<Attribute>,
            pub defaultness: Option<Token![default]>,
            pub unsafety: Option<Token![unsafe]>,
            pub impl_token: Token![impl],
            pub generics: Generics,
            /// Trait this impl implements.
            pub trait_: Option<(Option<Token![!]>, Path, Token![for])>,
            /// The Self type of the impl.
            pub self_ty: Box<Type>,
            pub brace_token: token::Brace,
            pub items: Vec<ImplItem>,
        }),

        /// A macro invocation, which includes `macro_rules!` definitions.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Macro(ItemMacro {
            pub attrs: Vec<Attribute>,
            /// The `example` in `macro_rules! example { ... }`.
            pub ident: Option<Ident>,
            pub mac: Macro,
            pub semi_token: Option<Token![;]>,
        }),

        /// A 2.0-style declarative macro introduced by the `macro` keyword.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Macro2(ItemMacro2 #manual_extra_traits {
            pub attrs: Vec<Attribute>,
            pub vis: Visibility,
            pub macro_token: Token![macro],
            pub ident: Ident,
            pub paren_token: Paren,
            pub args: TokenStream,
            pub brace_token: Brace,
            pub body: TokenStream,
        }),

        /// Tokens forming an item not interpreted by Syn.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Verbatim(ItemVerbatim #manual_extra_traits {
            pub tts: TokenStream,
        }),
    }
}

#[cfg(feature = "extra-traits")]
impl Eq for ItemMacro2 {}

#[cfg(feature = "extra-traits")]
impl PartialEq for ItemMacro2 {
    fn eq(&self, other: &Self) -> bool {
        self.attrs == other.attrs && self.vis == other.vis && self.macro_token == other.macro_token
            && self.ident == other.ident && self.paren_token == other.paren_token
            && TokenStreamHelper(&self.args) == TokenStreamHelper(&other.args)
            && self.brace_token == other.brace_token
            && TokenStreamHelper(&self.body) == TokenStreamHelper(&other.body)
    }
}

#[cfg(feature = "extra-traits")]
impl Hash for ItemMacro2 {
    fn hash<H>(&self, state: &mut H)
    where
        H: Hasher,
    {
        self.attrs.hash(state);
        self.vis.hash(state);
        self.macro_token.hash(state);
        self.ident.hash(state);
        self.paren_token.hash(state);
        TokenStreamHelper(&self.args).hash(state);
        self.brace_token.hash(state);
        TokenStreamHelper(&self.body).hash(state);
    }
}

#[cfg(feature = "extra-traits")]
impl Eq for ItemVerbatim {}

#[cfg(feature = "extra-traits")]
impl PartialEq for ItemVerbatim {
    fn eq(&self, other: &Self) -> bool {
        TokenStreamHelper(&self.tts) == TokenStreamHelper(&other.tts)
    }
}

#[cfg(feature = "extra-traits")]
impl Hash for ItemVerbatim {
    fn hash<H>(&self, state: &mut H)
    where
        H: Hasher,
    {
        TokenStreamHelper(&self.tts).hash(state);
    }
}

impl From<DeriveInput> for Item {
    fn from(input: DeriveInput) -> Item {
        match input.data {
            Data::Struct(data) => Item::Struct(ItemStruct {
                attrs: input.attrs,
                vis: input.vis,
                struct_token: data.struct_token,
                ident: input.ident,
                generics: input.generics,
                fields: data.fields,
                semi_token: data.semi_token,
            }),
            Data::Enum(data) => Item::Enum(ItemEnum {
                attrs: input.attrs,
                vis: input.vis,
                enum_token: data.enum_token,
                ident: input.ident,
                generics: input.generics,
                brace_token: data.brace_token,
                variants: data.variants,
            }),
            Data::Union(data) => Item::Union(ItemUnion {
                attrs: input.attrs,
                vis: input.vis,
                union_token: data.union_token,
                ident: input.ident,
                generics: input.generics,
                fields: data.fields,
            }),
        }
    }
}

ast_enum_of_structs! {
    /// A suffix of an import tree in a `use` item: `Type as Renamed` or `*`.
    ///
    /// *This type is available if Syn is built with the `"full"` feature.*
    ///
    /// # Syntax tree enum
    ///
    /// This type is a [syntax tree enum].
    ///
    /// [syntax tree enum]: enum.Expr.html#syntax-tree-enums
    pub enum UseTree {
        /// A path prefix of imports in a `use` item: `std::...`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Path(UsePath {
            pub ident: Ident,
            pub colon2_token: Token![::],
            pub tree: Box<UseTree>,
        }),

        /// An identifier imported by a `use` item: `HashMap`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Name(UseName {
            pub ident: Ident,
        }),

        /// An renamed identifier imported by a `use` item: `HashMap as Map`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Rename(UseRename {
            pub ident: Ident,
            pub as_token: Token![as],
            pub rename: Ident,
        }),

        /// A glob import in a `use` item: `*`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Glob(UseGlob {
            pub star_token: Token![*],
        }),

        /// A braced group of imports in a `use` item: `{A, B, C}`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Group(UseGroup {
            pub brace_token: token::Brace,
            pub items: Punctuated<UseTree, Token![,]>,
        }),
    }
}

ast_enum_of_structs! {
    /// An item within an `extern` block.
    ///
    /// *This type is available if Syn is built with the `"full"` feature.*
    ///
    /// # Syntax tree enum
    ///
    /// This type is a [syntax tree enum].
    ///
    /// [syntax tree enum]: enum.Expr.html#syntax-tree-enums
    pub enum ForeignItem {
        /// A foreign function in an `extern` block.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Fn(ForeignItemFn {
            pub attrs: Vec<Attribute>,
            pub vis: Visibility,
            pub ident: Ident,
            pub decl: Box<FnDecl>,
            pub semi_token: Token![;],
        }),

        /// A foreign static item in an `extern` block: `static ext: u8`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Static(ForeignItemStatic {
            pub attrs: Vec<Attribute>,
            pub vis: Visibility,
            pub static_token: Token![static],
            pub mutability: Option<Token![mut]>,
            pub ident: Ident,
            pub colon_token: Token![:],
            pub ty: Box<Type>,
            pub semi_token: Token![;],
        }),

        /// A foreign type in an `extern` block: `type void`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Type(ForeignItemType {
            pub attrs: Vec<Attribute>,
            pub vis: Visibility,
            pub type_token: Token![type],
            pub ident: Ident,
            pub semi_token: Token![;],
        }),

        /// Tokens in an `extern` block not interpreted by Syn.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Verbatim(ForeignItemVerbatim #manual_extra_traits {
            pub tts: TokenStream,
        }),
    }
}

#[cfg(feature = "extra-traits")]
impl Eq for ForeignItemVerbatim {}

#[cfg(feature = "extra-traits")]
impl PartialEq for ForeignItemVerbatim {
    fn eq(&self, other: &Self) -> bool {
        TokenStreamHelper(&self.tts) == TokenStreamHelper(&other.tts)
    }
}

#[cfg(feature = "extra-traits")]
impl Hash for ForeignItemVerbatim {
    fn hash<H>(&self, state: &mut H)
    where
        H: Hasher,
    {
        TokenStreamHelper(&self.tts).hash(state);
    }
}

ast_enum_of_structs! {
    /// An item declaration within the definition of a trait.
    ///
    /// *This type is available if Syn is built with the `"full"` feature.*
    ///
    /// # Syntax tree enum
    ///
    /// This type is a [syntax tree enum].
    ///
    /// [syntax tree enum]: enum.Expr.html#syntax-tree-enums
    pub enum TraitItem {
        /// An associated constant within the definition of a trait.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Const(TraitItemConst {
            pub attrs: Vec<Attribute>,
            pub const_token: Token![const],
            pub ident: Ident,
            pub colon_token: Token![:],
            pub ty: Type,
            pub default: Option<(Token![=], Expr)>,
            pub semi_token: Token![;],
        }),

        /// A trait method within the definition of a trait.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Method(TraitItemMethod {
            pub attrs: Vec<Attribute>,
            pub sig: MethodSig,
            pub default: Option<Block>,
            pub semi_token: Option<Token![;]>,
        }),

        /// An associated type within the definition of a trait.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Type(TraitItemType {
            pub attrs: Vec<Attribute>,
            pub type_token: Token![type],
            pub ident: Ident,
            pub generics: Generics,
            pub colon_token: Option<Token![:]>,
            pub bounds: Punctuated<TypeParamBound, Token![+]>,
            pub default: Option<(Token![=], Type)>,
            pub semi_token: Token![;],
        }),

        /// A macro invocation within the definition of a trait.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Macro(TraitItemMacro {
            pub attrs: Vec<Attribute>,
            pub mac: Macro,
            pub semi_token: Option<Token![;]>,
        }),

        /// Tokens within the definition of a trait not interpreted by Syn.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Verbatim(TraitItemVerbatim #manual_extra_traits {
            pub tts: TokenStream,
        }),
    }
}

#[cfg(feature = "extra-traits")]
impl Eq for TraitItemVerbatim {}

#[cfg(feature = "extra-traits")]
impl PartialEq for TraitItemVerbatim {
    fn eq(&self, other: &Self) -> bool {
        TokenStreamHelper(&self.tts) == TokenStreamHelper(&other.tts)
    }
}

#[cfg(feature = "extra-traits")]
impl Hash for TraitItemVerbatim {
    fn hash<H>(&self, state: &mut H)
    where
        H: Hasher,
    {
        TokenStreamHelper(&self.tts).hash(state);
    }
}

ast_enum_of_structs! {
    /// An item within an impl block.
    ///
    /// *This type is available if Syn is built with the `"full"` feature.*
    ///
    /// # Syntax tree enum
    ///
    /// This type is a [syntax tree enum].
    ///
    /// [syntax tree enum]: enum.Expr.html#syntax-tree-enums
    pub enum ImplItem {
        /// An associated constant within an impl block.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Const(ImplItemConst {
            pub attrs: Vec<Attribute>,
            pub vis: Visibility,
            pub defaultness: Option<Token![default]>,
            pub const_token: Token![const],
            pub ident: Ident,
            pub colon_token: Token![:],
            pub ty: Type,
            pub eq_token: Token![=],
            pub expr: Expr,
            pub semi_token: Token![;],
        }),

        /// A method within an impl block.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Method(ImplItemMethod {
            pub attrs: Vec<Attribute>,
            pub vis: Visibility,
            pub defaultness: Option<Token![default]>,
            pub sig: MethodSig,
            pub block: Block,
        }),

        /// An associated type within an impl block.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Type(ImplItemType {
            pub attrs: Vec<Attribute>,
            pub vis: Visibility,
            pub defaultness: Option<Token![default]>,
            pub type_token: Token![type],
            pub ident: Ident,
            pub generics: Generics,
            pub eq_token: Token![=],
            pub ty: Type,
            pub semi_token: Token![;],
        }),

        /// A macro invocation within an impl block.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Macro(ImplItemMacro {
            pub attrs: Vec<Attribute>,
            pub mac: Macro,
            pub semi_token: Option<Token![;]>,
        }),

        /// Tokens within an impl block not interpreted by Syn.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Verbatim(ImplItemVerbatim #manual_extra_traits {
            pub tts: TokenStream,
        }),
    }
}

#[cfg(feature = "extra-traits")]
impl Eq for ImplItemVerbatim {}

#[cfg(feature = "extra-traits")]
impl PartialEq for ImplItemVerbatim {
    fn eq(&self, other: &Self) -> bool {
        TokenStreamHelper(&self.tts) == TokenStreamHelper(&other.tts)
    }
}

#[cfg(feature = "extra-traits")]
impl Hash for ImplItemVerbatim {
    fn hash<H>(&self, state: &mut H)
    where
        H: Hasher,
    {
        TokenStreamHelper(&self.tts).hash(state);
    }
}

ast_struct! {
    /// A method's signature in a trait or implementation: `unsafe fn
    /// initialize(&self)`.
    ///
    /// *This type is available if Syn is built with the `"full"` feature.*
    pub struct MethodSig {
        pub constness: Option<Token![const]>,
        pub unsafety: Option<Token![unsafe]>,
        pub abi: Option<Abi>,
        pub ident: Ident,
        pub decl: FnDecl,
    }
}

ast_struct! {
    /// Header of a function declaration, without including the body.
    ///
    /// *This type is available if Syn is built with the `"full"` feature.*
    pub struct FnDecl {
        pub fn_token: Token![fn],
        pub generics: Generics,
        pub paren_token: token::Paren,
        pub inputs: Punctuated<FnArg, Token![,]>,
        pub variadic: Option<Token![...]>,
        pub output: ReturnType,
    }
}

ast_enum_of_structs! {
    /// An argument in a function signature: the `n: usize` in `fn f(n: usize)`.
    ///
    /// *This type is available if Syn is built with the `"full"` feature.*
    ///
    /// # Syntax tree enum
    ///
    /// This type is a [syntax tree enum].
    ///
    /// [syntax tree enum]: enum.Expr.html#syntax-tree-enums
    pub enum FnArg {
        /// Self captured by reference in a function signature: `&self` or `&mut
        /// self`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub SelfRef(ArgSelfRef {
            pub and_token: Token![&],
            pub lifetime: Option<Lifetime>,
            pub mutability: Option<Token![mut]>,
            pub self_token: Token![self],
        }),

        /// Self captured by value in a function signature: `self` or `mut
        /// self`.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub SelfValue(ArgSelf {
            pub mutability: Option<Token![mut]>,
            pub self_token: Token![self],
        }),

        /// An explicitly typed pattern captured by a function signature.
        ///
        /// *This type is available if Syn is built with the `"full"` feature.*
        pub Captured(ArgCaptured {
            pub pat: Pat,
            pub colon_token: Token![:],
            pub ty: Type,
        }),

        /// A pattern whose type is inferred captured by a function signature.
        pub Inferred(Pat),
        /// A type not bound to any pattern in a function signature.
        pub Ignored(Type),
    }
}

#[cfg(feature = "parsing")]
pub mod parsing {
    use super::*;

    use synom::Synom;

    impl_synom!(Item "item" alt!(
        syn!(ItemExternCrate) => { Item::ExternCrate }
        |
        syn!(ItemUse) => { Item::Use }
        |
        syn!(ItemStatic) => { Item::Static }
        |
        syn!(ItemConst) => { Item::Const }
        |
        syn!(ItemFn) => { Item::Fn }
        |
        syn!(ItemMod) => { Item::Mod }
        |
        syn!(ItemForeignMod) => { Item::ForeignMod }
        |
        syn!(ItemType) => { Item::Type }
        |
        syn!(ItemStruct) => { Item::Struct }
        |
        syn!(ItemEnum) => { Item::Enum }
        |
        syn!(ItemUnion) => { Item::Union }
        |
        syn!(ItemTrait) => { Item::Trait }
        |
        syn!(ItemImpl) => { Item::Impl }
        |
        syn!(ItemMacro) => { Item::Macro }
        |
        syn!(ItemMacro2) => { Item::Macro2 }
    ));

    impl_synom!(ItemMacro "macro item" do_parse!(
        attrs: many0!(Attribute::parse_outer) >>
        what: call!(Path::parse_mod_style) >>
        bang: punct!(!) >>
        ident: option!(syn!(Ident)) >>
        body: call!(tt::delimited) >>
        semi: cond!(!is_brace(&body.0), punct!(;)) >>
        (ItemMacro {
            attrs: attrs,
            ident: ident,
            mac: Macro {
                path: what,
                bang_token: bang,
                delimiter: body.0,
                tts: body.1,
            },
            semi_token: semi,
        })
    ));

    // TODO: figure out the actual grammar; is body required to be braced?
    impl_synom!(ItemMacro2 "macro2 item" do_parse!(
        attrs: many0!(Attribute::parse_outer) >>
        vis: syn!(Visibility) >>
        macro_: keyword!(macro) >>
        ident: syn!(Ident) >>
        args: call!(tt::parenthesized) >>
        body: call!(tt::braced) >>
        (ItemMacro2 {
            attrs: attrs,
            vis: vis,
            macro_token: macro_,
            ident: ident,
            paren_token: args.0,
            args: args.1,
            brace_token: body.0,
            body: body.1,
        })
    ));

    impl_synom!(ItemExternCrate "extern crate item" do_parse!(
        attrs: many0!(Attribute::parse_outer) >>
        vis: syn!(Visibility) >>
        extern_: keyword!(extern) >>
        crate_: keyword!(crate) >>
        ident: syn!(Ident) >>
        rename: option!(tuple!(keyword!(as), syn!(Ident))) >>
        semi: punct!(;) >>
        (ItemExternCrate {
            attrs: attrs,
            vis: vis,
            extern_token: extern_,
            crate_token: crate_,
            ident: ident,
            rename: rename,
            semi_token: semi,
        })
    ));

    impl_synom!(ItemUse "use item" do_parse!(
        attrs: many0!(Attribute::parse_outer) >>
        vis: syn!(Visibility) >>
        use_: keyword!(use) >>
        leading_colon: option!(punct!(::)) >>
        tree: syn!(UseTree) >>
        semi: punct!(;) >>
        (ItemUse {
            attrs: attrs,
            vis: vis,
            use_token: use_,
            leading_colon: leading_colon,
            tree: tree,
            semi_token: semi,
        })
    ));

    named!(use_element -> Ident, alt!(
        syn!(Ident)
        |
        keyword!(self) => { Into::into }
        |
        keyword!(super) => { Into::into }
        |
        keyword!(crate) => { Into::into }
    ));

    impl_synom!(UseTree "use tree" alt!(
        syn!(UseRename) => { UseTree::Rename }
        |
        syn!(UsePath) => { UseTree::Path }
        |
        syn!(UseName) => { UseTree::Name }
        |
        syn!(UseGlob) => { UseTree::Glob }
        |
        syn!(UseGroup) => { UseTree::Group }
    ));

    impl_synom!(UsePath "use path" do_parse!(
        ident: call!(use_element) >>
        colon2_token: punct!(::) >>
        tree: syn!(UseTree) >>
        (UsePath {
            ident: ident,
            colon2_token: colon2_token,
            tree: Box::new(tree),
        })
    ));

    impl_synom!(UseName "use name" do_parse!(
        ident: call!(use_element) >>
        (UseName {
            ident: ident,
        })
    ));

    impl_synom!(UseRename "use rename" do_parse!(
        ident: call!(use_element) >>
        as_token: keyword!(as) >>
        rename: syn!(Ident) >>
        (UseRename {
            ident: ident,
            as_token: as_token,
            rename: rename,
        })
    ));

    impl_synom!(UseGlob "use glob" do_parse!(
        star: punct!(*) >>
        (UseGlob {
            star_token: star,
        })
    ));

    impl_synom!(UseGroup "use group" do_parse!(
        list: braces!(Punctuated::parse_terminated) >>
        (UseGroup {
            brace_token: list.0,
            items: list.1,
        })
    ));

    impl_synom!(ItemStatic "static item" do_parse!(
        attrs: many0!(Attribute::parse_outer) >>
        vis: syn!(Visibility) >>
        static_: keyword!(static) >>
        mutability: option!(keyword!(mut)) >>
        ident: syn!(Ident) >>
        colon: punct!(:) >>
        ty: syn!(Type) >>
        eq: punct!(=) >>
        value: syn!(Expr) >>
        semi: punct!(;) >>
        (ItemStatic {
            attrs: attrs,
            vis: vis,
            static_token: static_,
            mutability: mutability,
            ident: ident,
            colon_token: colon,
            ty: Box::new(ty),
            eq_token: eq,
            expr: Box::new(value),
            semi_token: semi,
        })
    ));

    impl_synom!(ItemConst "const item" do_parse!(
        attrs: many0!(Attribute::parse_outer) >>
        vis: syn!(Visibility) >>
        const_: keyword!(const) >>
        ident: syn!(Ident) >>
        colon: punct!(:) >>
        ty: syn!(Type) >>
        eq: punct!(=) >>
        value: syn!(Expr) >>
        semi: punct!(;) >>
        (ItemConst {
            attrs: attrs,
            vis: vis,
            const_token: const_,
            ident: ident,
            colon_token: colon,
            ty: Box::new(ty),
            eq_token: eq,
            expr: Box::new(value),
            semi_token: semi,
        })
    ));

    impl_synom!(ItemFn "fn item" do_parse!(
        outer_attrs: many0!(Attribute::parse_outer) >>
        vis: syn!(Visibility) >>
        constness: option!(keyword!(const)) >>
        unsafety: option!(keyword!(unsafe)) >>
        abi: option!(syn!(Abi)) >>
        fn_: keyword!(fn) >>
        ident: syn!(Ident) >>
        generics: syn!(Generics) >>
        inputs: parens!(Punctuated::parse_terminated) >>
        ret: syn!(ReturnType) >>
        where_clause: option!(syn!(WhereClause)) >>
        inner_attrs_stmts: braces!(tuple!(
            many0!(Attribute::parse_inner),
            call!(Block::parse_within)
        )) >>
        (ItemFn {
            attrs: {
                let mut attrs = outer_attrs;
                attrs.extend((inner_attrs_stmts.1).0);
                attrs
            },
            vis: vis,
            constness: constness,
            unsafety: unsafety,
            abi: abi,
            decl: Box::new(FnDecl {
                fn_token: fn_,
                paren_token: inputs.0,
                inputs: inputs.1,
                output: ret,
                variadic: None,
                generics: Generics {
                    where_clause: where_clause,
                    .. generics
                },
            }),
            ident: ident,
            block: Box::new(Block {
                brace_token: inner_attrs_stmts.0,
                stmts: (inner_attrs_stmts.1).1,
            }),
        })
    ));

    impl Synom for FnArg {
        named!(parse -> Self, alt!(
            do_parse!(
                and: punct!(&) >>
                lt: option!(syn!(Lifetime)) >>
                mutability: option!(keyword!(mut)) >>
                self_: keyword!(self) >>
                not!(punct!(:)) >>
                (ArgSelfRef {
                    lifetime: lt,
                    mutability: mutability,
                    and_token: and,
                    self_token: self_,
                }.into())
            )
            |
            do_parse!(
                mutability: option!(keyword!(mut)) >>
                self_: keyword!(self) >>
                not!(punct!(:)) >>
                (ArgSelf {
                    mutability: mutability,
                    self_token: self_,
                }.into())
            )
            |
            do_parse!(
                pat: syn!(Pat) >>
                colon: punct!(:) >>
                ty: syn!(Type) >>
                (ArgCaptured {
                    pat: pat,
                    ty: ty,
                    colon_token: colon,
                }.into())
            )
            |
            syn!(Type) => { FnArg::Ignored }
        ));

        fn description() -> Option<&'static str> {
            Some("function argument")
        }
    }

    impl_synom!(ItemMod "mod item" do_parse!(
        outer_attrs: many0!(Attribute::parse_outer) >>
        vis: syn!(Visibility) >>
        mod_: keyword!(mod) >>
        ident: syn!(Ident) >>
        content_semi: alt!(
            punct!(;) => {|semi| (
                Vec::new(),
                None,
                Some(semi),
            )}
            |
            braces!(
                tuple!(
                    many0!(Attribute::parse_inner),
                    many0!(Item::parse)
                )
            ) => {|(brace, (inner_attrs, items))| (
                inner_attrs,
                Some((brace, items)),
                None,
            )}
        ) >>
        (ItemMod {
            attrs: {
                let mut attrs = outer_attrs;
                attrs.extend(content_semi.0);
                attrs
            },
            vis: vis,
            mod_token: mod_,
            ident: ident,
            content: content_semi.1,
            semi: content_semi.2,
        })
    ));

    impl_synom!(ItemForeignMod "foreign mod item" do_parse!(
        attrs: many0!(Attribute::parse_outer) >>
        abi: syn!(Abi) >>
        items: braces!(many0!(ForeignItem::parse)) >>
        (ItemForeignMod {
            attrs: attrs,
            abi: abi,
            brace_token: items.0,
            items: items.1,
        })
    ));

    impl_synom!(ForeignItem "foreign item" alt!(
        syn!(ForeignItemFn) => { ForeignItem::Fn }
        |
        syn!(ForeignItemStatic) => { ForeignItem::Static }
        |
        syn!(ForeignItemType) => { ForeignItem::Type }
    ));

    impl_synom!(ForeignItemFn "foreign function" do_parse!(
        attrs: many0!(Attribute::parse_outer) >>
        vis: syn!(Visibility) >>
        fn_: keyword!(fn) >>
        ident: syn!(Ident) >>
        generics: syn!(Generics) >>
        inputs: parens!(do_parse!(
            args: call!(Punctuated::parse_terminated) >>
            variadic: option!(cond_reduce!(args.empty_or_trailing(), punct!(...))) >>
            (args, variadic)
        )) >>
        ret: syn!(ReturnType) >>
        where_clause: option!(syn!(WhereClause)) >>
        semi: punct!(;) >>
        ({
            let (parens, (inputs, variadic)) = inputs;
            ForeignItemFn {
                ident: ident,
                attrs: attrs,
                semi_token: semi,
                decl: Box::new(FnDecl {
                    fn_token: fn_,
                    paren_token: parens,
                    inputs: inputs,
                    variadic: variadic,
                    output: ret,
                    generics: Generics {
                        where_clause: where_clause,
                        .. generics
                    },
                }),
                vis: vis,
            }
        })
    ));

    impl_synom!(ForeignItemStatic "foreign static" do_parse!(
        attrs: many0!(Attribute::parse_outer) >>
        vis: syn!(Visibility) >>
        static_: keyword!(static) >>
        mutability: option!(keyword!(mut)) >>
        ident: syn!(Ident) >>
        colon: punct!(:) >>
        ty: syn!(Type) >>
        semi: punct!(;) >>
        (ForeignItemStatic {
            ident: ident,
            attrs: attrs,
            semi_token: semi,
            ty: Box::new(ty),
            mutability: mutability,
            static_token: static_,
            colon_token: colon,
            vis: vis,
        })
    ));

    impl_synom!(ForeignItemType "foreign type" do_parse!(
        attrs: many0!(Attribute::parse_outer) >>
        vis: syn!(Visibility) >>
        type_: keyword!(type) >>
        ident: syn!(Ident) >>
        semi: punct!(;) >>
        (ForeignItemType {
            attrs: attrs,
            vis: vis,
            type_token: type_,
            ident: ident,
            semi_token: semi,
        })
    ));

    impl_synom!(ItemType "type item" do_parse!(
        attrs: many0!(Attribute::parse_outer) >>
        vis: syn!(Visibility) >>
        type_: keyword!(type) >>
        ident: syn!(Ident) >>
        generics: syn!(Generics) >>
        where_clause: option!(syn!(WhereClause)) >>
        eq: punct!(=) >>
        ty: syn!(Type) >>
        semi: punct!(;) >>
        (ItemType {
            attrs: attrs,
            vis: vis,
            type_token: type_,
            ident: ident,
            generics: Generics {
                where_clause: where_clause,
                ..generics
            },
            eq_token: eq,
            ty: Box::new(ty),
            semi_token: semi,
        })
    ));

    impl_synom!(ItemStruct "struct item" switch!(
        map!(syn!(DeriveInput), Into::into),
        Item::Struct(item) => value!(item)
        |
        _ => reject!()
    ));

    impl_synom!(ItemEnum "enum item" switch!(
        map!(syn!(DeriveInput), Into::into),
        Item::Enum(item) => value!(item)
        |
        _ => reject!()
    ));

    impl_synom!(ItemUnion "union item" do_parse!(
        attrs: many0!(Attribute::parse_outer) >>
        vis: syn!(Visibility) >>
        union_: keyword!(union) >>
        ident: syn!(Ident) >>
        generics: syn!(Generics) >>
        where_clause: option!(syn!(WhereClause)) >>
        fields: syn!(FieldsNamed) >>
        (ItemUnion {
            attrs: attrs,
            vis: vis,
            union_token: union_,
            ident: ident,
            generics: Generics {
                where_clause: where_clause,
                .. generics
            },
            fields: fields,
        })
    ));

    impl_synom!(ItemTrait "trait item" do_parse!(
        attrs: many0!(Attribute::parse_outer) >>
        vis: syn!(Visibility) >>
        unsafety: option!(keyword!(unsafe)) >>
        auto_: option!(keyword!(auto)) >>
        trait_: keyword!(trait) >>
        ident: syn!(Ident) >>
        generics: syn!(Generics) >>
        colon: option!(punct!(:)) >>
        bounds: cond!(colon.is_some(), Punctuated::parse_separated_nonempty) >>
        where_clause: option!(syn!(WhereClause)) >>
        body: braces!(many0!(TraitItem::parse)) >>
        (ItemTrait {
            attrs: attrs,
            vis: vis,
            unsafety: unsafety,
            auto_token: auto_,
            trait_token: trait_,
            ident: ident,
            generics: Generics {
                where_clause: where_clause,
                .. generics
            },
            colon_token: colon,
            supertraits: bounds.unwrap_or_default(),
            brace_token: body.0,
            items: body.1,
        })
    ));

    impl_synom!(TraitItem "trait item" alt!(
        syn!(TraitItemConst) => { TraitItem::Const }
        |
        syn!(TraitItemMethod) => { TraitItem::Method }
        |
        syn!(TraitItemType) => { TraitItem::Type }
        |
        syn!(TraitItemMacro) => { TraitItem::Macro }
    ));

    impl_synom!(TraitItemConst "const trait item" do_parse!(
        attrs: many0!(Attribute::parse_outer) >>
        const_: keyword!(const) >>
        ident: syn!(Ident) >>
        colon: punct!(:) >>
        ty: syn!(Type) >>
        default: option!(tuple!(punct!(=), syn!(Expr))) >>
        semi: punct!(;) >>
        (TraitItemConst {
            attrs: attrs,
            const_token: const_,
            ident: ident,
            colon_token: colon,
            ty: ty,
            default: default,
            semi_token: semi,
        })
    ));

    impl_synom!(TraitItemMethod "method trait item" do_parse!(
        outer_attrs: many0!(Attribute::parse_outer) >>
        constness: option!(keyword!(const)) >>
        unsafety: option!(keyword!(unsafe)) >>
        abi: option!(syn!(Abi)) >>
        fn_: keyword!(fn) >>
        ident: syn!(Ident) >>
        generics: syn!(Generics) >>
        inputs: parens!(Punctuated::parse_terminated) >>
        ret: syn!(ReturnType) >>
        where_clause: option!(syn!(WhereClause)) >>
        body: option!(braces!(
            tuple!(many0!(Attribute::parse_inner),
                   call!(Block::parse_within))
        )) >>
        semi: cond!(body.is_none(), punct!(;)) >>
        ({
            let (inner_attrs, stmts) = match body {
                Some((b, (inner_attrs, stmts))) => (inner_attrs, Some((stmts, b))),
                None => (Vec::new(), None),
            };
            TraitItemMethod {
                attrs: {
                    let mut attrs = outer_attrs;
                    attrs.extend(inner_attrs);
                    attrs
                },
                sig: MethodSig {
                    constness: constness,
                    unsafety: unsafety,
                    abi: abi,
                    ident: ident,
                    decl: FnDecl {
                        inputs: inputs.1,
                        output: ret,
                        fn_token: fn_,
                        paren_token: inputs.0,
                        variadic: None,
                        generics: Generics {
                            where_clause: where_clause,
                            .. generics
                        },
                    },
                },
                default: stmts.map(|stmts| {
                    Block {
                        stmts: stmts.0,
                        brace_token: stmts.1,
                    }
                }),
                semi_token: semi,
            }
        })
    ));

    impl_synom!(TraitItemType "trait item type" do_parse!(
        attrs: many0!(Attribute::parse_outer) >>
        type_: keyword!(type) >>
        ident: syn!(Ident) >>
        generics: syn!(Generics) >>
        colon: option!(punct!(:)) >>
        bounds: cond!(colon.is_some(), Punctuated::parse_separated_nonempty) >>
        where_clause: option!(syn!(WhereClause)) >>
        default: option!(tuple!(punct!(=), syn!(Type))) >>
        semi: punct!(;) >>
        (TraitItemType {
            attrs: attrs,
            type_token: type_,
            ident: ident,
            generics: Generics {
                where_clause: where_clause,
                .. generics
            },
            colon_token: colon,
            bounds: bounds.unwrap_or_default(),
            default: default,
            semi_token: semi,
        })
    ));

    impl_synom!(TraitItemMacro "trait item macro" do_parse!(
        attrs: many0!(Attribute::parse_outer) >>
        mac: syn!(Macro) >>
        semi: cond!(!is_brace(&mac.delimiter), punct!(;)) >>
        (TraitItemMacro {
            attrs: attrs,
            mac: mac,
            semi_token: semi,
        })
    ));

    impl_synom!(ItemImpl "impl item" do_parse!(
        outer_attrs: many0!(Attribute::parse_outer) >>
        defaultness: option!(keyword!(default)) >>
        unsafety: option!(keyword!(unsafe)) >>
        impl_: keyword!(impl) >>
        generics: syn!(Generics) >>
        polarity_path: alt!(
            do_parse!(
                polarity: option!(punct!(!)) >>
                path: syn!(Path) >>
                for_: keyword!(for) >>
                (Some((polarity, path, for_)))
            )
            |
            epsilon!() => { |_| None }
        ) >>
        self_ty: syn!(Type) >>
        where_clause: option!(syn!(WhereClause)) >>
        inner: braces!(tuple!(
            many0!(Attribute::parse_inner),
            many0!(ImplItem::parse)
        )) >>
        (ItemImpl {
            attrs: {
                let mut attrs = outer_attrs;
                attrs.extend((inner.1).0);
                attrs
            },
            defaultness: defaultness,
            unsafety: unsafety,
            impl_token: impl_,
            generics: Generics {
                where_clause: where_clause,
                .. generics
            },
            trait_: polarity_path,
            self_ty: Box::new(self_ty),
            brace_token: inner.0,
            items: (inner.1).1,
        })
    ));

    impl_synom!(ImplItem "item in impl block" alt!(
        syn!(ImplItemConst) => { ImplItem::Const }
        |
        syn!(ImplItemMethod) => { ImplItem::Method }
        |
        syn!(ImplItemType) => { ImplItem::Type }
        |
        syn!(ImplItemMacro) => { ImplItem::Macro }
    ));

    impl_synom!(ImplItemConst "const item in impl block" do_parse!(
        attrs: many0!(Attribute::parse_outer) >>
        vis: syn!(Visibility) >>
        defaultness: option!(keyword!(default)) >>
        const_: keyword!(const) >>
        ident: syn!(Ident) >>
        colon: punct!(:) >>
        ty: syn!(Type) >>
        eq: punct!(=) >>
        value: syn!(Expr) >>
        semi: punct!(;) >>
        (ImplItemConst {
            attrs: attrs,
            vis: vis,
            defaultness: defaultness,
            const_token: const_,
            ident: ident,
            colon_token: colon,
            ty: ty,
            eq_token: eq,
            expr: value,
            semi_token: semi,
        })
    ));

    impl_synom!(ImplItemMethod "method in impl block" do_parse!(
        outer_attrs: many0!(Attribute::parse_outer) >>
        vis: syn!(Visibility) >>
        defaultness: option!(keyword!(default)) >>
        constness: option!(keyword!(const)) >>
        unsafety: option!(keyword!(unsafe)) >>
        abi: option!(syn!(Abi)) >>
        fn_: keyword!(fn) >>
        ident: syn!(Ident) >>
        generics: syn!(Generics) >>
        inputs: parens!(Punctuated::parse_terminated) >>
        ret: syn!(ReturnType) >>
        where_clause: option!(syn!(WhereClause)) >>
        inner_attrs_stmts: braces!(tuple!(
            many0!(Attribute::parse_inner),
            call!(Block::parse_within)
        )) >>
        (ImplItemMethod {
            attrs: {
                let mut attrs = outer_attrs;
                attrs.extend((inner_attrs_stmts.1).0);
                attrs
            },
            vis: vis,
            defaultness: defaultness,
            sig: MethodSig {
                constness: constness,
                unsafety: unsafety,
                abi: abi,
                ident: ident,
                decl: FnDecl {
                    fn_token: fn_,
                    paren_token: inputs.0,
                    inputs: inputs.1,
                    output: ret,
                    generics: Generics {
                        where_clause: where_clause,
                        .. generics
                    },
                    variadic: None,
                },
            },
            block: Block {
                brace_token: inner_attrs_stmts.0,
                stmts: (inner_attrs_stmts.1).1,
            },
        })
    ));

    impl_synom!(ImplItemType "type in impl block" do_parse!(
        attrs: many0!(Attribute::parse_outer) >>
        vis: syn!(Visibility) >>
        defaultness: option!(keyword!(default)) >>
        type_: keyword!(type) >>
        ident: syn!(Ident) >>
        generics: syn!(Generics) >>
        eq: punct!(=) >>
        ty: syn!(Type) >>
        semi: punct!(;) >>
        (ImplItemType {
            attrs: attrs,
            vis: vis,
            defaultness: defaultness,
            type_token: type_,
            ident: ident,
            generics: generics,
            eq_token: eq,
            ty: ty,
            semi_token: semi,
        })
    ));

    impl_synom!(ImplItemMacro "macro in impl block" do_parse!(
        attrs: many0!(Attribute::parse_outer) >>
        mac: syn!(Macro) >>
        semi: cond!(!is_brace(&mac.delimiter), punct!(;)) >>
        (ImplItemMacro {
            attrs: attrs,
            mac: mac,
            semi_token: semi,
        })
    ));

    fn is_brace(delimiter: &MacroDelimiter) -> bool {
        match *delimiter {
            MacroDelimiter::Brace(_) => true,
            MacroDelimiter::Paren(_) | MacroDelimiter::Bracket(_) => false,
        }
    }
}

#[cfg(feature = "printing")]
mod printing {
    use super::*;
    use attr::FilterAttrs;
    use quote::{ToTokens, Tokens};

    impl ToTokens for ItemExternCrate {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.extern_token.to_tokens(tokens);
            self.crate_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            if let Some((ref as_token, ref rename)) = self.rename {
                as_token.to_tokens(tokens);
                rename.to_tokens(tokens);
            }
            self.semi_token.to_tokens(tokens);
        }
    }

    impl ToTokens for ItemUse {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.use_token.to_tokens(tokens);
            self.leading_colon.to_tokens(tokens);
            self.tree.to_tokens(tokens);
            self.semi_token.to_tokens(tokens);
        }
    }

    impl ToTokens for ItemStatic {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.static_token.to_tokens(tokens);
            self.mutability.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.colon_token.to_tokens(tokens);
            self.ty.to_tokens(tokens);
            self.eq_token.to_tokens(tokens);
            self.expr.to_tokens(tokens);
            self.semi_token.to_tokens(tokens);
        }
    }

    impl ToTokens for ItemConst {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.const_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.colon_token.to_tokens(tokens);
            self.ty.to_tokens(tokens);
            self.eq_token.to_tokens(tokens);
            self.expr.to_tokens(tokens);
            self.semi_token.to_tokens(tokens);
        }
    }

    impl ToTokens for ItemFn {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.constness.to_tokens(tokens);
            self.unsafety.to_tokens(tokens);
            self.abi.to_tokens(tokens);
            NamedDecl(&self.decl, self.ident).to_tokens(tokens);
            self.block.brace_token.surround(tokens, |tokens| {
                tokens.append_all(self.attrs.inner());
                tokens.append_all(&self.block.stmts);
            });
        }
    }

    impl ToTokens for ItemMod {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.mod_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            if let Some((ref brace, ref items)) = self.content {
                brace.surround(tokens, |tokens| {
                    tokens.append_all(self.attrs.inner());
                    tokens.append_all(items);
                });
            } else {
                TokensOrDefault(&self.semi).to_tokens(tokens);
            }
        }
    }

    impl ToTokens for ItemForeignMod {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.abi.to_tokens(tokens);
            self.brace_token.surround(tokens, |tokens| {
                tokens.append_all(&self.items);
            });
        }
    }

    impl ToTokens for ItemType {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.type_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.generics.to_tokens(tokens);
            self.generics.where_clause.to_tokens(tokens);
            self.eq_token.to_tokens(tokens);
            self.ty.to_tokens(tokens);
            self.semi_token.to_tokens(tokens);
        }
    }

    impl ToTokens for ItemEnum {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.enum_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.generics.to_tokens(tokens);
            self.generics.where_clause.to_tokens(tokens);
            self.brace_token.surround(tokens, |tokens| {
                self.variants.to_tokens(tokens);
            });
        }
    }

    impl ToTokens for ItemStruct {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.struct_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.generics.to_tokens(tokens);
            match self.fields {
                Fields::Named(ref fields) => {
                    self.generics.where_clause.to_tokens(tokens);
                    fields.to_tokens(tokens);
                }
                Fields::Unnamed(ref fields) => {
                    fields.to_tokens(tokens);
                    self.generics.where_clause.to_tokens(tokens);
                    TokensOrDefault(&self.semi_token).to_tokens(tokens);
                }
                Fields::Unit => {
                    self.generics.where_clause.to_tokens(tokens);
                    TokensOrDefault(&self.semi_token).to_tokens(tokens);
                }
            }
        }
    }

    impl ToTokens for ItemUnion {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.union_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.generics.to_tokens(tokens);
            self.generics.where_clause.to_tokens(tokens);
            self.fields.to_tokens(tokens);
        }
    }

    impl ToTokens for ItemTrait {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.unsafety.to_tokens(tokens);
            self.auto_token.to_tokens(tokens);
            self.trait_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.generics.to_tokens(tokens);
            if !self.supertraits.is_empty() {
                TokensOrDefault(&self.colon_token).to_tokens(tokens);
                self.supertraits.to_tokens(tokens);
            }
            self.generics.where_clause.to_tokens(tokens);
            self.brace_token.surround(tokens, |tokens| {
                tokens.append_all(&self.items);
            });
        }
    }

    impl ToTokens for ItemImpl {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.defaultness.to_tokens(tokens);
            self.unsafety.to_tokens(tokens);
            self.impl_token.to_tokens(tokens);
            self.generics.to_tokens(tokens);
            if let Some((ref polarity, ref path, ref for_token)) = self.trait_ {
                polarity.to_tokens(tokens);
                path.to_tokens(tokens);
                for_token.to_tokens(tokens);
            }
            self.self_ty.to_tokens(tokens);
            self.generics.where_clause.to_tokens(tokens);
            self.brace_token.surround(tokens, |tokens| {
                tokens.append_all(self.attrs.inner());
                tokens.append_all(&self.items);
            });
        }
    }

    impl ToTokens for ItemMacro {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.mac.path.to_tokens(tokens);
            self.mac.bang_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            match self.mac.delimiter {
                MacroDelimiter::Paren(ref paren) => {
                    paren.surround(tokens, |tokens| self.mac.tts.to_tokens(tokens));
                }
                MacroDelimiter::Brace(ref brace) => {
                    brace.surround(tokens, |tokens| self.mac.tts.to_tokens(tokens));
                }
                MacroDelimiter::Bracket(ref bracket) => {
                    bracket.surround(tokens, |tokens| self.mac.tts.to_tokens(tokens));
                }
            }
            self.semi_token.to_tokens(tokens);
        }
    }

    impl ToTokens for ItemMacro2 {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.macro_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.paren_token.surround(tokens, |tokens| {
                self.args.to_tokens(tokens);
            });
            self.brace_token.surround(tokens, |tokens| {
                self.body.to_tokens(tokens);
            });
        }
    }

    impl ToTokens for ItemVerbatim {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.tts.to_tokens(tokens);
        }
    }

    impl ToTokens for UsePath {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.ident.to_tokens(tokens);
            self.colon2_token.to_tokens(tokens);
            self.tree.to_tokens(tokens);
        }
    }

    impl ToTokens for UseName {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.ident.to_tokens(tokens);
        }
    }

    impl ToTokens for UseRename {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.ident.to_tokens(tokens);
            self.as_token.to_tokens(tokens);
            self.rename.to_tokens(tokens);
        }
    }

    impl ToTokens for UseGlob {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.star_token.to_tokens(tokens);
        }
    }

    impl ToTokens for UseGroup {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.brace_token.surround(tokens, |tokens| {
                self.items.to_tokens(tokens);
            });
        }
    }

    impl ToTokens for TraitItemConst {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.const_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.colon_token.to_tokens(tokens);
            self.ty.to_tokens(tokens);
            if let Some((ref eq_token, ref default)) = self.default {
                eq_token.to_tokens(tokens);
                default.to_tokens(tokens);
            }
            self.semi_token.to_tokens(tokens);
        }
    }

    impl ToTokens for TraitItemMethod {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.sig.to_tokens(tokens);
            match self.default {
                Some(ref block) => {
                    block.brace_token.surround(tokens, |tokens| {
                        tokens.append_all(self.attrs.inner());
                        tokens.append_all(&block.stmts);
                    });
                }
                None => {
                    TokensOrDefault(&self.semi_token).to_tokens(tokens);
                }
            }
        }
    }

    impl ToTokens for TraitItemType {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.type_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.generics.to_tokens(tokens);
            if !self.bounds.is_empty() {
                TokensOrDefault(&self.colon_token).to_tokens(tokens);
                self.bounds.to_tokens(tokens);
            }
            self.generics.where_clause.to_tokens(tokens);
            if let Some((ref eq_token, ref default)) = self.default {
                eq_token.to_tokens(tokens);
                default.to_tokens(tokens);
            }
            self.semi_token.to_tokens(tokens);
        }
    }

    impl ToTokens for TraitItemMacro {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.mac.to_tokens(tokens);
            self.semi_token.to_tokens(tokens);
        }
    }

    impl ToTokens for TraitItemVerbatim {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.tts.to_tokens(tokens);
        }
    }

    impl ToTokens for ImplItemConst {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.defaultness.to_tokens(tokens);
            self.const_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.colon_token.to_tokens(tokens);
            self.ty.to_tokens(tokens);
            self.eq_token.to_tokens(tokens);
            self.expr.to_tokens(tokens);
            self.semi_token.to_tokens(tokens);
        }
    }

    impl ToTokens for ImplItemMethod {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.defaultness.to_tokens(tokens);
            self.sig.to_tokens(tokens);
            self.block.brace_token.surround(tokens, |tokens| {
                tokens.append_all(self.attrs.inner());
                tokens.append_all(&self.block.stmts);
            });
        }
    }

    impl ToTokens for ImplItemType {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.defaultness.to_tokens(tokens);
            self.type_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.generics.to_tokens(tokens);
            self.eq_token.to_tokens(tokens);
            self.ty.to_tokens(tokens);
            self.semi_token.to_tokens(tokens);
        }
    }

    impl ToTokens for ImplItemMacro {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.mac.to_tokens(tokens);
            self.semi_token.to_tokens(tokens);
        }
    }

    impl ToTokens for ImplItemVerbatim {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.tts.to_tokens(tokens);
        }
    }

    impl ToTokens for ForeignItemFn {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            NamedDecl(&self.decl, self.ident).to_tokens(tokens);
            self.semi_token.to_tokens(tokens);
        }
    }

    impl ToTokens for ForeignItemStatic {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.static_token.to_tokens(tokens);
            self.mutability.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.colon_token.to_tokens(tokens);
            self.ty.to_tokens(tokens);
            self.semi_token.to_tokens(tokens);
        }
    }

    impl ToTokens for ForeignItemType {
        fn to_tokens(&self, tokens: &mut Tokens) {
            tokens.append_all(self.attrs.outer());
            self.vis.to_tokens(tokens);
            self.type_token.to_tokens(tokens);
            self.ident.to_tokens(tokens);
            self.semi_token.to_tokens(tokens);
        }
    }

    impl ToTokens for ForeignItemVerbatim {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.tts.to_tokens(tokens);
        }
    }

    impl ToTokens for MethodSig {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.constness.to_tokens(tokens);
            self.unsafety.to_tokens(tokens);
            self.abi.to_tokens(tokens);
            NamedDecl(&self.decl, self.ident).to_tokens(tokens);
        }
    }

    struct NamedDecl<'a>(&'a FnDecl, Ident);

    impl<'a> ToTokens for NamedDecl<'a> {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.0.fn_token.to_tokens(tokens);
            self.1.to_tokens(tokens);
            self.0.generics.to_tokens(tokens);
            self.0.paren_token.surround(tokens, |tokens| {
                self.0.inputs.to_tokens(tokens);
                if self.0.variadic.is_some() && !self.0.inputs.empty_or_trailing() {
                    <Token![,]>::default().to_tokens(tokens);
                }
                self.0.variadic.to_tokens(tokens);
            });
            self.0.output.to_tokens(tokens);
            self.0.generics.where_clause.to_tokens(tokens);
        }
    }

    impl ToTokens for ArgSelfRef {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.and_token.to_tokens(tokens);
            self.lifetime.to_tokens(tokens);
            self.mutability.to_tokens(tokens);
            self.self_token.to_tokens(tokens);
        }
    }

    impl ToTokens for ArgSelf {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.mutability.to_tokens(tokens);
            self.self_token.to_tokens(tokens);
        }
    }

    impl ToTokens for ArgCaptured {
        fn to_tokens(&self, tokens: &mut Tokens) {
            self.pat.to_tokens(tokens);
            self.colon_token.to_tokens(tokens);
            self.ty.to_tokens(tokens);
        }
    }
}
