#![cfg_attr(feature = "unstable", allow(wrong_self_convention))]

use std::iter::IntoIterator;

use syntax::abi::Abi;
use syntax::ast;
use syntax::codemap::{DUMMY_SP, Span, respan};
use syntax::ptr::P;
use syntax::symbol::keywords;

use attr::AttrBuilder;
use block::BlockBuilder;
use constant::{Const, ConstBuilder};
use fn_decl::FnDeclBuilder;
use generics::GenericsBuilder;
use ident::ToIdent;
use invoke::{Invoke, Identity};
use mac::MacBuilder;
use method::MethodSigBuilder;
use path::PathBuilder;
use struct_field::StructFieldBuilder;
use symbol::ToSymbol;
use ty::TyBuilder;
use ty_param::TyParamBoundBuilder;
use variant::VariantBuilder;
use variant_data::{
    VariantDataBuilder,
    VariantDataStructBuilder,
    VariantDataTupleBuilder,
};

//////////////////////////////////////////////////////////////////////////////

pub struct ItemBuilder<F=Identity> {
    callback: F,
    span: Span,
    attrs: Vec<ast::Attribute>,
    vis: ast::Visibility,
}

impl ItemBuilder {
    pub fn new() -> Self {
        ItemBuilder::with_callback(Identity)
    }
}

impl<F> ItemBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    pub fn with_callback(callback: F) -> Self {
        ItemBuilder {
            callback: callback,
            span: DUMMY_SP,
            attrs: vec![],
            vis: ast::Visibility::Inherited,
        }
    }

    pub fn build(self, item: P<ast::Item>) -> F::Result {
        self.callback.invoke(item)
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn with_attrs<I>(mut self, iter: I) -> Self
        where I: IntoIterator<Item=ast::Attribute>,
    {
        self.attrs.extend(iter);
        self
    }

    pub fn with_attr(mut self, attr: ast::Attribute) -> Self {
        self.attrs.push(attr);
        self
    }

    pub fn attr(self) -> AttrBuilder<Self> {
        AttrBuilder::with_callback(self)
    }

    pub fn pub_(mut self) -> Self {
        self.vis = ast::Visibility::Public;
        self
    }

    pub fn build_item_kind<T>(self, id: T, item_kind: ast::ItemKind) -> F::Result
        where T: ToIdent,
    {
        let item = ast::Item {
            ident: id.to_ident(),
            attrs: self.attrs,
            id: ast::DUMMY_NODE_ID,
            node: item_kind,
            vis: self.vis,
            span: self.span,
        };
        self.callback.invoke(P(item))
    }

    pub fn fn_<T>(self, id: T) -> FnDeclBuilder<ItemFnDeclBuilder<F>>
        where T: ToIdent,
    {
        let id = id.to_ident();
        let span = self.span;
        FnDeclBuilder::with_callback(ItemFnDeclBuilder {
            builder: self,
            span: span,
            id: id,
        })
    }

    pub fn mod_<T>(self, id: T) -> ItemModBuilder<F>
        where T: ToIdent,
    {
        ItemModBuilder {
            ident: id.to_ident(),
            vis: self.vis.clone(),
            attrs: vec![],
            span: self.span,
            items: vec![],
            builder: self,
        }
    }

    pub fn build_use(self, view_path: ast::ViewPath_) -> F::Result {
        let item = ast::ItemKind::Use(P(respan(self.span, view_path)));
        self.build_item_kind(keywords::Invalid.ident(), item)
    }

    pub fn use_(self) -> PathBuilder<ItemUseBuilder<F>> {
        PathBuilder::with_callback(ItemUseBuilder {
            builder: self,
        })
    }

    pub fn struct_<T>(self, id: T) -> ItemStructBuilder<F>
        where T: ToIdent,
    {
        let id = id.to_ident();
        let generics = GenericsBuilder::new().build();

        ItemStructBuilder {
            is_union: false,
            builder: self,
            id: id,
            generics: generics,
        }
    }

    pub fn union_<T>(self, id: T) -> ItemStructBuilder<F>
        where T: ToIdent,
    {
        let id = id.to_ident();
        let generics = GenericsBuilder::new().build();

        ItemStructBuilder {
            is_union: true,
            builder: self,
            id: id,
            generics: generics,
        }
    }

    pub fn unit_struct<T>(self, id: T) -> F::Result
        where T: ToIdent,
    {
        let id = id.to_ident();
        let data = VariantDataBuilder::new().unit();
        let generics = GenericsBuilder::new().build();

        let struct_ = ast::ItemKind::Struct(data, generics);
        self.build_item_kind(id, struct_)
    }

    pub fn tuple_struct<T>(self, id: T) -> ItemTupleStructBuilder<F>
        where T: ToIdent,
    {
        let id = id.to_ident();
        let generics = GenericsBuilder::new().build();

        ItemTupleStructBuilder {
            builder: self,
            id: id,
            generics: generics,
            fields: vec![],
        }
    }

    pub fn enum_<T>(self, id: T) -> ItemEnumBuilder<F>
        where T: ToIdent,
    {
        let id = id.to_ident();
        let span = self.span;
        let generics = GenericsBuilder::new().span(span).build();

        ItemEnumBuilder {
            builder: self,
            id: id,
            generics: generics,
            variants: vec![],
        }
    }

    pub fn extern_crate<T>(self, id: T) -> ItemExternCrateBuilder<F>
        where T: ToIdent,
    {
        let id = id.to_ident();

        ItemExternCrateBuilder {
            builder: self,
            id: id,
        }
    }

    pub fn mac(self) -> MacBuilder<ItemMacBuilder<F>> {
        self.mac_id(keywords::Invalid.ident())
    }

    pub fn mac_id<T>(self, id: T) -> MacBuilder<ItemMacBuilder<F>>
        where T: ToIdent,
    {
        let span = self.span;
        MacBuilder::with_callback(ItemMacBuilder {
            builder: self,
            id: id.to_ident(),
        }).span(span)
    }

    pub fn type_<T>(self, id: T) -> ItemTyBuilder<F>
        where T: ToIdent,
    {
        let id = id.to_ident();
        let generics = GenericsBuilder::new().build();

        ItemTyBuilder {
            builder: self,
            id: id,
            generics: generics,
        }
    }

    pub fn trait_<T>(self, id: T) -> ItemTraitBuilder<F>
        where T: ToIdent,
    {
        ItemTraitBuilder {
            builder: self,
            id: id.to_ident(),
            unsafety: ast::Unsafety::Normal,
            generics: GenericsBuilder::new().build(),
            bounds: vec![],
            items: vec![],
        }
    }

    pub fn impl_(self) -> ItemImplBuilder<F> {
        let generics = GenericsBuilder::new().build();

        ItemImplBuilder {
            builder: self,
            unsafety: ast::Unsafety::Normal,
            polarity: ast::ImplPolarity::Positive,
            generics: generics,
            trait_ref: None,
            items: vec![],
        }
    }

    pub fn const_<T>(self, id: T) -> ConstBuilder<ItemConstBuilder<F>>
        where T: ToIdent,
    {
        ConstBuilder::with_callback(ItemConstBuilder {
            builder: self,
            id: id.to_ident(),
        })
    }
}

impl<F> Invoke<ast::Attribute> for ItemBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    type Result = Self;

    fn invoke(self, attr: ast::Attribute) -> Self {
        self.with_attr(attr)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ItemFnDeclBuilder<F> {
    builder: ItemBuilder<F>,
    span: Span,
    id: ast::Ident,
}

impl<F> Invoke<P<ast::FnDecl>> for ItemFnDeclBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    type Result = ItemFnBuilder<F>;

    fn invoke(self, fn_decl: P<ast::FnDecl>) -> ItemFnBuilder<F> {
        let generics = GenericsBuilder::new().build();

        ItemFnBuilder {
            builder: self.builder,
            span: self.span,
            id: self.id,
            fn_decl: fn_decl,
            unsafety: ast::Unsafety::Normal,
            constness: ast::Constness::NotConst,
            abi: Abi::Rust,
            generics: generics,
        }
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ItemFnBuilder<F> {
    builder: ItemBuilder<F>,
    span: Span,
    id: ast::Ident,
    fn_decl: P<ast::FnDecl>,
    unsafety: ast::Unsafety,
    constness: ast::Constness,
    abi: Abi,
    generics: ast::Generics,
}

impl<F> ItemFnBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    pub fn unsafe_(mut self) -> Self {
        self.unsafety = ast::Unsafety::Unsafe;
        self
    }

    pub fn const_(mut self) -> Self {
        self.constness = ast::Constness::Const;
        self
    }

    pub fn abi(mut self, abi: Abi) -> Self {
        self.abi = abi;
        self
    }

    pub fn generics(self) -> GenericsBuilder<Self> {
        GenericsBuilder::with_callback(self)
    }

    pub fn build(self, block: P<ast::Block>) -> F::Result {
        self.builder.build_item_kind(self.id, ast::ItemKind::Fn(
            self.fn_decl,
            self.unsafety,
            respan(self.span, self.constness),
            self.abi,
            self.generics,
            block,
        ))
    }

    pub fn block(self) -> BlockBuilder<Self> {
        BlockBuilder::with_callback(self)
    }
}

impl<F> Invoke<ast::Generics> for ItemFnBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    type Result = Self;

    fn invoke(mut self, generics: ast::Generics) -> Self {
        self.generics = generics;
        self
    }
}

impl<F> Invoke<P<ast::Block>> for ItemFnBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    type Result = F::Result;

    fn invoke(self, block: P<ast::Block>) -> F::Result {
        self.build(block)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ItemUseBuilder<F> {
    builder: ItemBuilder<F>,
}

impl<F> Invoke<ast::Path> for ItemUseBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    type Result = ItemUsePathBuilder<F>;

    fn invoke(self, path: ast::Path) -> ItemUsePathBuilder<F> {
        ItemUsePathBuilder {
            builder: self.builder,
            path: path,
        }
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ItemUsePathBuilder<F> {
    builder: ItemBuilder<F>,
    path: ast::Path,
}

impl<F> ItemUsePathBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    pub fn as_<T>(self, id: T) -> F::Result
        where T: ToIdent,
    {
        self.builder.build_use(ast::ViewPathSimple(id.to_ident(), self.path))
    }

    pub fn build(self) -> F::Result {
        let id = {
            let segment = self.path.segments.last().expect("path with no segments!");
            segment.identifier
        };
        self.as_(id)
    }

    pub fn glob(self) -> F::Result {
        self.builder.build_use(ast::ViewPathGlob(self.path))
    }

    pub fn list(self) -> ItemUsePathListBuilder<F> {
        let span =  self.builder.span;
        ItemUsePathListBuilder {
            builder: self.builder,
            span: span,
            path: self.path,
            idents: Vec::new(),
        }
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ItemUsePathListBuilder<F> {
    builder: ItemBuilder<F>,
    span: Span,
    path: ast::Path,
    idents: Vec<ast::PathListItem>,
}

impl<F> ItemUsePathListBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn self_(mut self) -> Self {
        self.idents.push(respan(self.span, ast::PathListItem_ {
            name: keywords::SelfValue.ident(),
            rename: None,
            id: ast::DUMMY_NODE_ID,
        }));
        self
    }

    pub fn id<T>(mut self, id: T) -> Self
        where T: ToIdent,
    {
        self.idents.push(respan(self.span, ast::PathListItem_ {
            name: id.to_ident(),
            rename: None,
            id: ast::DUMMY_NODE_ID,
        }));
        self
    }

    pub fn build(self) -> F::Result {
        self.builder.build_use(ast::ViewPathList(self.path, self.idents))
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ItemStructBuilder<F> {
    is_union: bool,
    builder: ItemBuilder<F>,
    id: ast::Ident,
    generics: ast::Generics,
}

impl<F> ItemStructBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    pub fn with_generics(mut self, generics: ast::Generics) -> Self {
        self.generics = generics;
        self
    }

    pub fn generics(self) -> GenericsBuilder<Self> {
        GenericsBuilder::with_callback(self)
    }

    pub fn with_fields<I>(self, iter: I) -> VariantDataStructBuilder<Self>
        where I: IntoIterator<Item=ast::StructField>,
    {
        let span = self.builder.span;
        VariantDataBuilder::with_callback(self).span(span).struct_().with_fields(iter)
    }

    pub fn with_field(self, field: ast::StructField) -> VariantDataStructBuilder<Self> {
        let span = self.builder.span;
        VariantDataBuilder::with_callback(self).span(span).struct_().with_field(field)
    }

    pub fn field<T>(self, id: T) -> StructFieldBuilder<VariantDataStructBuilder<Self>>
        where T: ToIdent,
    {
        let span = self.builder.span;
        VariantDataBuilder::with_callback(self).span(span).struct_().field(id)
    }

    pub fn build(self) -> F::Result {
        VariantDataBuilder::with_callback(self).struct_().build()
    }
}

impl<F> Invoke<ast::Generics> for ItemStructBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    type Result = Self;

    fn invoke(self, generics: ast::Generics) -> Self {
        self.with_generics(generics)
    }
}

impl<F> Invoke<ast::VariantData> for ItemStructBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    type Result = F::Result;

    fn invoke(self, data: ast::VariantData) -> F::Result {
        let kind = if self.is_union {
            ast::ItemKind::Union(data, self.generics)
        } else {
            ast::ItemKind::Struct(data, self.generics)
        };

        self.builder.build_item_kind(self.id, kind)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ItemTupleStructBuilder<F> {
    builder: ItemBuilder<F>,
    id: ast::Ident,
    generics: ast::Generics,
    fields: Vec<ast::StructField>,
}

impl<F> ItemTupleStructBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    pub fn generics(self) -> GenericsBuilder<Self> {
        GenericsBuilder::with_callback(self)
    }

    pub fn with_tys<I>(mut self, iter: I) -> Self
        where I: IntoIterator<Item=P<ast::Ty>>,
    {
        for ty in iter {
            self = self.ty().build(ty);
        }
        self
    }

    pub fn ty(self) -> TyBuilder<Self> {
        let span = self.builder.span;
        TyBuilder::with_callback(self).span(span)
    }

    pub fn field(self) -> StructFieldBuilder<Self> {
        let span = self.builder.span;
        StructFieldBuilder::unnamed_with_callback(self).span(span)
    }

    pub fn build(self) -> F::Result {
        let data = ast::VariantData::Tuple(self.fields, ast::DUMMY_NODE_ID);
        let struct_ = ast::ItemKind::Struct(data, self.generics);
        self.builder.build_item_kind(self.id, struct_)
    }
}

impl<F> Invoke<ast::Generics> for ItemTupleStructBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    type Result = Self;

    fn invoke(mut self, generics: ast::Generics) -> Self {
        self.generics = generics;
        self
    }
}

impl<F> Invoke<P<ast::Ty>> for ItemTupleStructBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    type Result = Self;

    fn invoke(self, ty: P<ast::Ty>) -> Self {
        self.field().build_ty(ty)
    }
}

impl<F> Invoke<ast::StructField> for ItemTupleStructBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    type Result = Self;

    fn invoke(mut self, field: ast::StructField) -> Self {
        self.fields.push(field);
        self
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ItemEnumBuilder<F> {
    builder: ItemBuilder<F>,
    id: ast::Ident,
    generics: ast::Generics,
    variants: Vec<ast::Variant>,
}

impl<F> ItemEnumBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    pub fn generics(self) -> GenericsBuilder<Self> {
        let span = self.builder.span;
        GenericsBuilder::with_callback(self).span(span)
    }

    pub fn with_variants<I>(mut self, iter: I) -> Self
        where I: IntoIterator<Item=ast::Variant>,
    {
        self.variants.extend(iter);
        self
    }

    pub fn with_variant(mut self, variant: ast::Variant) -> Self {
        self.variants.push(variant);
        self
    }

    pub fn with_variant_(self, variant: ast::Variant_) -> Self {
        let variant = respan(self.builder.span, variant);
        self.with_variant(variant)
    }

    pub fn ids<I, T>(mut self, ids: I) -> Self
        where I: IntoIterator<Item=T>,
              T: ToIdent,
    {
        for id in ids {
            self = self.id(id);
        }
        self
    }

    pub fn id<T>(self, id: T) -> Self
        where T: ToIdent,
    {
        self.variant(id).unit()
    }

    pub fn tuple<T>(self, id: T) -> StructFieldBuilder<VariantDataTupleBuilder<VariantBuilder<Self>>>
        where T: ToIdent,
    {
        self.variant(id).tuple()
    }

    pub fn struct_<T>(self, id: T) -> VariantDataStructBuilder<VariantBuilder<Self>>
        where T: ToIdent,
    {
        self.variant(id).struct_()
    }

    pub fn variant<T>(self, id: T) -> VariantBuilder<Self>
        where T: ToIdent,
    {
        let span = self.builder.span;
        VariantBuilder::with_callback(id, self).span(span)
    }

    pub fn build(self) -> F::Result {
        let enum_def = ast::EnumDef {
            variants: self.variants,
        };
        let enum_ = ast::ItemKind::Enum(enum_def, self.generics);
        self.builder.build_item_kind(self.id, enum_)
    }
}

impl<F> Invoke<ast::Generics> for ItemEnumBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    type Result = Self;

    fn invoke(mut self, generics: ast::Generics) -> Self {
        self.generics = generics;
        self
    }
}

impl<F> Invoke<ast::Variant> for ItemEnumBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    type Result = Self;

    fn invoke(self, variant: ast::Variant) -> Self {
        self.with_variant(variant)
    }
}

//////////////////////////////////////////////////////////////////////////////

/// A builder for extern crate items
pub struct ItemExternCrateBuilder<F> {
    builder: ItemBuilder<F>,
    id: ast::Ident,
}

impl<F> ItemExternCrateBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    pub fn with_name<N>(self, name: N) -> F::Result
        where N: ToSymbol
    {
        let extern_ = ast::ItemKind::ExternCrate(Some(name.to_symbol()));
        self.builder.build_item_kind(self.id, extern_)
    }

    pub fn build(self) -> F::Result {
        let extern_ = ast::ItemKind::ExternCrate(None);
        self.builder.build_item_kind(self.id, extern_)
    }
}

//////////////////////////////////////////////////////////////////////////////

/// A builder for macro invocation items.
///
/// Specifying the macro path returns a `MacBuilder`, which is used to
/// add expressions to the macro invocation.
pub struct ItemMacBuilder<F> {
    builder: ItemBuilder<F>,
    id: ast::Ident,
}

impl<F> Invoke<ast::Mac> for ItemMacBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    type Result = F::Result;

    fn invoke(self, mac: ast::Mac) -> F::Result {
        self.builder.build_item_kind(self.id, ast::ItemKind::Mac(mac))
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ItemTyBuilder<F> {
    builder: ItemBuilder<F>,
    id: ast::Ident,
    generics: ast::Generics,
}

impl<F> ItemTyBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    pub fn generics(self) -> GenericsBuilder<Self> {
        GenericsBuilder::with_callback(self)
    }

    pub fn ty(self) -> TyBuilder<Self> {
        let span = self.builder.span;
        TyBuilder::with_callback(self).span(span)
    }

    pub fn build_ty(self, ty: P<ast::Ty>) -> F::Result {
        let ty_ = ast::ItemKind::Ty(ty, self.generics);
        self.builder.build_item_kind(self.id, ty_)
    }
}

impl<F> Invoke<ast::Generics> for ItemTyBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    type Result = Self;

    fn invoke(mut self, generics: ast::Generics) -> Self {
        self.generics = generics;
        self
    }
}

impl<F> Invoke<P<ast::Ty>> for ItemTyBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    type Result = F::Result;

    fn invoke(self, ty: P<ast::Ty>) -> F::Result {
        self.build_ty(ty)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ItemTraitBuilder<F> {
    builder: ItemBuilder<F>,
    id: ast::Ident,
    unsafety: ast::Unsafety,
    generics: ast::Generics,
    bounds: Vec<ast::TyParamBound>,
    items: Vec<ast::TraitItem>,
}

impl<F> ItemTraitBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    pub fn unsafe_(mut self) -> Self {
        self.unsafety = ast::Unsafety::Unsafe;
        self
    }

    pub fn with_generics(mut self, generics: ast::Generics) -> Self {
        self.generics = generics;
        self
    }

    pub fn generics(self) -> GenericsBuilder<Self> {
        GenericsBuilder::with_callback(self)
    }

    pub fn with_bounds<I>(mut self, iter: I) -> Self
        where I: Iterator<Item=ast::TyParamBound>,
    {
        self.bounds.extend(iter);
        self
    }

    pub fn with_bound(mut self, bound: ast::TyParamBound) -> Self {
        self.bounds.push(bound);
        self
    }

    pub fn bound(self) -> TyParamBoundBuilder<Self> {
        TyParamBoundBuilder::with_callback(self)
    }

    pub fn with_items<I>(mut self, items: I) -> Self
        where I: IntoIterator<Item=ast::TraitItem>,
    {
        self.items.extend(items);
        self
    }

    pub fn with_item(mut self, item: ast::TraitItem) -> Self {
        self.items.push(item);
        self
    }

    pub fn item<T>(self, id: T) -> ItemTraitItemBuilder<Self>
        where T: ToIdent,
    {
        ItemTraitItemBuilder::with_callback(id, self)
    }

    pub fn const_<T>(self, id: T) -> ConstBuilder<ItemTraitItemBuilder<Self>>
        where T: ToIdent,
    {
        self.item(id).const_()
    }

    pub fn method<T>(self, id: T) -> MethodSigBuilder<ItemTraitItemBuilder<Self>>
        where T: ToIdent,
    {
        self.item(id).method()
    }

    pub fn type_<T>(self, id: T) -> ItemTraitTypeBuilder<Self>
        where T: ToIdent,
    {
        self.item(id).type_()
    }

    pub fn build(self) -> F::Result {
        self.builder.build_item_kind(self.id, ast::ItemKind::Trait(
            self.unsafety,
            self.generics,
            self.bounds,
            self.items,
        ))
    }
}

impl<F> Invoke<ast::Generics> for ItemTraitBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    type Result = Self;

    fn invoke(self, generics: ast::Generics) -> Self {
        self.with_generics(generics)
    }
}

impl<F> Invoke<ast::TyParamBound> for ItemTraitBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    type Result = Self;

    fn invoke(self, bound: ast::TyParamBound) -> Self {
        self.with_bound(bound)
    }
}

impl<F> Invoke<ast::TraitItem> for ItemTraitBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    type Result = Self;

    fn invoke(self, item: ast::TraitItem) -> Self {
        self.with_item(item)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ItemTraitItemBuilder<F=Identity> {
    callback: F,
    id: ast::Ident,
    attrs: Vec<ast::Attribute>,
    span: Span,
}

impl ItemTraitItemBuilder {
    pub fn new<T>(id: T) -> Self
        where T: ToIdent,
    {
        Self::with_callback(id, Identity)
    }
}

impl<F> ItemTraitItemBuilder<F>
    where F: Invoke<ast::TraitItem>,
{
    pub fn with_callback<T>(id: T, callback: F) -> Self
        where F: Invoke<ast::TraitItem>,
              T: ToIdent,
    {
        ItemTraitItemBuilder {
            callback: callback,
            id: id.to_ident(),
            attrs: vec![],
            span: DUMMY_SP,
        }
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn with_attrs<I>(mut self, iter: I) -> Self
        where I: IntoIterator<Item=ast::Attribute>,
    {
        self.attrs.extend(iter);
        self
    }

    pub fn with_attr(mut self, attr: ast::Attribute) -> Self {
        self.attrs.push(attr);
        self
    }

    pub fn attr(self) -> AttrBuilder<Self> {
        AttrBuilder::with_callback(self)
    }

    pub fn const_(self) -> ConstBuilder<Self> {
        ConstBuilder::with_callback(self)
    }

    pub fn method(self) -> MethodSigBuilder<Self> {
        MethodSigBuilder::with_callback(self)
    }

    pub fn type_(self) -> ItemTraitTypeBuilder<F> {
        ItemTraitTypeBuilder {
            builder: self,
            bounds: vec![],
        }
    }

    pub fn build_item(self, node: ast::TraitItemKind) -> F::Result {
        let item = ast::TraitItem {
            id: ast::DUMMY_NODE_ID,
            ident: self.id,
            attrs: self.attrs,
            node: node,
            span: self.span,
        };
        self.callback.invoke(item)
    }
}

impl<F> Invoke<ast::Attribute> for ItemTraitItemBuilder<F>
    where F: Invoke<ast::TraitItem>,
{
    type Result = Self;

    fn invoke(self, attr: ast::Attribute) -> Self {
        self.with_attr(attr)
    }
}

impl<F> Invoke<Const> for ItemTraitItemBuilder<F>
    where F: Invoke<ast::TraitItem>,
{
    type Result = F::Result;

    fn invoke(self, const_: Const) -> F::Result {
        let node = ast::TraitItemKind::Const(
            const_.ty,
            const_.expr);
        self.build_item(node)
    }
}

impl<F> Invoke<ast::MethodSig> for ItemTraitItemBuilder<F>
    where F: Invoke<ast::TraitItem>,
{
    type Result = ItemTraitMethodBuilder<F>;

    fn invoke(self, method: ast::MethodSig) -> Self::Result {
        ItemTraitMethodBuilder {
            builder: self,
            method: method,
        }
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ItemTraitMethodBuilder<F> {
    builder: ItemTraitItemBuilder<F>,
    method: ast::MethodSig,
}

impl<F> ItemTraitMethodBuilder<F>
    where F: Invoke<ast::TraitItem>,
{
    pub fn build_option_block(self, block: Option<P<ast::Block>>) -> F::Result {
        let node = ast::TraitItemKind::Method(self.method, block);
        self.builder.build_item(node)
    }

    pub fn build_block(self, block: P<ast::Block>) -> F::Result {
        self.build_option_block(Some(block))
    }

    pub fn build(self) -> F::Result {
        self.build_option_block(None)
    }
}

impl<F> Invoke<P<ast::Block>> for ItemTraitMethodBuilder<F>
    where F: Invoke<ast::TraitItem>,
{
    type Result = F::Result;

    fn invoke(self, block: P<ast::Block>) -> Self::Result {
        self.build_block(block)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ItemTraitTypeBuilder<F> {
    builder: ItemTraitItemBuilder<F>,
    bounds: Vec<ast::TyParamBound>,
}

impl<F> ItemTraitTypeBuilder<F>
    where F: Invoke<ast::TraitItem>,
{
    pub fn with_bounds<I>(mut self, iter: I) -> Self
        where I: Iterator<Item=ast::TyParamBound>,
    {
        self.bounds.extend(iter);
        self
    }

    pub fn with_bound(mut self, bound: ast::TyParamBound) -> Self {
        self.bounds.push(bound);
        self
    }

    pub fn bound(self) -> TyParamBoundBuilder<Self> {
        TyParamBoundBuilder::with_callback(self)
    }

    pub fn build_option_ty(self, ty: Option<P<ast::Ty>>) -> F::Result {
        let bounds = P::from_vec(self.bounds);
        let node = ast::TraitItemKind::Type(bounds.into_vec(), ty);
        self.builder.build_item(node)
    }

    pub fn build_ty(self, ty: P<ast::Ty>) -> F::Result {
        self.build_option_ty(Some(ty))
    }

    pub fn ty(self) -> TyBuilder<Self> {
        let span = self.builder.span;
        TyBuilder::with_callback(self).span(span)
    }

    pub fn build(self) -> F::Result {
        self.build_option_ty(None)
    }
}

impl<F> Invoke<ast::TyParamBound> for ItemTraitTypeBuilder<F>
    where F: Invoke<ast::TraitItem>,
{
    type Result = ItemTraitTypeBuilder<F>;

    fn invoke(self, bound: ast::TyParamBound) -> Self::Result {
        self.with_bound(bound)
    }
}

impl<F> Invoke<P<ast::Ty>> for ItemTraitTypeBuilder<F>
    where F: Invoke<ast::TraitItem>,
{
    type Result = F::Result;

    fn invoke(self, ty: P<ast::Ty>) -> Self::Result {
        self.build_ty(ty)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ItemImplBuilder<F> {
    builder: ItemBuilder<F>,
    unsafety: ast::Unsafety,
    polarity: ast::ImplPolarity,
    generics: ast::Generics,
    trait_ref: Option<ast::TraitRef>,
    items: Vec<ast::ImplItem>,
}

impl<F> ItemImplBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    pub fn unsafe_(mut self) -> Self {
        self.unsafety = ast::Unsafety::Unsafe;
        self
    }

    pub fn negative(mut self) -> Self {
        self.polarity = ast::ImplPolarity::Negative;
        self
    }

    pub fn with_generics(mut self, generics: ast::Generics) -> Self {
        self.generics = generics;
        self
    }

    pub fn generics(self) -> GenericsBuilder<Self> {
        GenericsBuilder::with_callback(self)
    }

    pub fn with_trait(mut self, trait_ref: ast::TraitRef) -> Self {
        self.trait_ref = Some(trait_ref);
        self
    }

    pub fn trait_(self) -> PathBuilder<Self> {
        PathBuilder::with_callback(self)
    }

    pub fn ty(self) -> TyBuilder<Self> {
        let span = self.builder.span;
        TyBuilder::with_callback(self).span(span)
    }

    pub fn build_ty(self, ty: P<ast::Ty>) -> F::Result {
        let ty_ = ast::ItemKind::Impl(
            self.unsafety,
            self.polarity,
            self.generics,
            self.trait_ref,
            ty,
            self.items);
        self.builder.build_item_kind(keywords::Invalid.ident(), ty_)
    }

    pub fn with_items<I>(mut self, items: I) -> Self
        where I: IntoIterator<Item=ast::ImplItem>,
    {
        self.items.extend(items);
        self
    }

    pub fn with_item(mut self, item: ast::ImplItem) -> Self {
        self.items.push(item);
        self
    }

    pub fn item<T>(self, id: T) -> ItemImplItemBuilder<Self>
        where T: ToIdent,
    {
        ItemImplItemBuilder::with_callback(id, self)
    }

    pub fn const_<T>(self, id: T) -> ConstBuilder<ItemImplItemBuilder<Self>>
        where T: ToIdent,
    {
        self.item(id).const_()
    }

    pub fn method<T>(self, id: T) -> MethodSigBuilder<ItemImplItemBuilder<Self>>
        where T: ToIdent,
    {
        self.item(id).method()
    }

    pub fn type_<T>(self, id: T) -> TyBuilder<ItemImplItemBuilder<Self>>
        where T: ToIdent,
    {
        self.item(id).type_()
    }
}

impl<F> Invoke<ast::Generics> for ItemImplBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    type Result = Self;

    fn invoke(self, generics: ast::Generics) -> Self {
        self.with_generics(generics)
    }
}

impl<F> Invoke<ast::Path> for ItemImplBuilder<F>
    where F: Invoke<P<ast::Item>>
{
    type Result = Self;

    fn invoke(self, path: ast::Path) -> Self {
        self.with_trait(ast::TraitRef {
            path: path,
            ref_id: ast::DUMMY_NODE_ID,
        })
    }
}

impl<F> Invoke<ast::ImplItem> for ItemImplBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    type Result = Self;

    fn invoke(self, item: ast::ImplItem) -> Self {
        self.with_item(item)
    }
}

impl<F> Invoke<P<ast::Ty>> for ItemImplBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    type Result = F::Result;

    fn invoke(self, ty: P<ast::Ty>) -> F::Result {
        self.build_ty(ty)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ItemImplItemBuilder<F=Identity> {
    callback: F,
    id: ast::Ident,
    vis: ast::Visibility,
    defaultness: ast::Defaultness,
    attrs: Vec<ast::Attribute>,
    span: Span,
}

impl ItemImplItemBuilder {
    pub fn new<T>(id: T) -> Self
        where T: ToIdent,
    {
        Self::with_callback(id, Identity)
    }
}

impl<F> ItemImplItemBuilder<F>
    where F: Invoke<ast::ImplItem>,
{
    pub fn with_callback<T>(id: T, callback: F) -> Self
        where F: Invoke<ast::ImplItem>,
              T: ToIdent,
    {
        ItemImplItemBuilder {
            callback: callback,
            id: id.to_ident(),
            vis: ast::Visibility::Inherited,
            defaultness: ast::Defaultness::Final,
            attrs: vec![],
            span: DUMMY_SP,
        }
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }
    
    pub fn with_attrs<I>(mut self, iter: I) -> Self
        where I: IntoIterator<Item=ast::Attribute>,
    {
        self.attrs.extend(iter);
        self
    }

    pub fn with_attr(mut self, attr: ast::Attribute) -> Self {
        self.attrs.push(attr);
        self
    }

    pub fn attr(self) -> AttrBuilder<Self> {
        AttrBuilder::with_callback(self)
    }

    pub fn pub_(mut self) -> Self {
        self.vis = ast::Visibility::Public;
        self
    }

    pub fn default(mut self) -> Self {
        self.defaultness = ast::Defaultness::Default;
        self
    }

    pub fn const_(self) -> ConstBuilder<Self> {
        ConstBuilder::with_callback(self)
    }

    pub fn build_method(self, method: ast::MethodSig) -> ItemImplMethodBuilder<F> {
        ItemImplMethodBuilder {
            builder: self,
            method: method,
        }
    }

    pub fn method(self) -> MethodSigBuilder<Self> {
        MethodSigBuilder::with_callback(self)
    }

    pub fn type_(self) -> TyBuilder<Self> {
        let span = self.span;
        TyBuilder::with_callback(self).span(span)
    }

    pub fn mac(self) -> MacBuilder<Self> {
        MacBuilder::with_callback(self)
    }

    pub fn build_item(self, node: ast::ImplItemKind) -> F::Result {
        let item = ast::ImplItem {
            id: ast::DUMMY_NODE_ID,
            ident: self.id,
            vis: self.vis,
            defaultness: self.defaultness,
            attrs: self.attrs,
            node: node,
            span: self.span,
        };
        self.callback.invoke(item)
    }
}

impl<F> Invoke<ast::Attribute> for ItemImplItemBuilder<F>
    where F: Invoke<ast::ImplItem>,
{
    type Result = Self;

    fn invoke(self, attr: ast::Attribute) -> Self {
        self.with_attr(attr)
    }
}

impl<F> Invoke<Const> for ItemImplItemBuilder<F>
    where F: Invoke<ast::ImplItem>,
{
    type Result = F::Result;

    fn invoke(self, const_: Const) -> F::Result {
        let node = ast::ImplItemKind::Const(const_.ty, const_.expr.expect("an expr is required for a const impl item"));
        self.build_item(node)
    }
}

impl<F> Invoke<ast::MethodSig> for ItemImplItemBuilder<F>
    where F: Invoke<ast::ImplItem>,
{
    type Result = ItemImplMethodBuilder<F>;

    fn invoke(self, method: ast::MethodSig) -> Self::Result {
        self.build_method(method)
    }
}

impl<F> Invoke<P<ast::Ty>> for ItemImplItemBuilder<F>
    where F: Invoke<ast::ImplItem>,
{
    type Result = F::Result;

    fn invoke(self, ty: P<ast::Ty>) -> F::Result {
        let node = ast::ImplItemKind::Type(ty);
        self.build_item(node)
    }
}

impl<F> Invoke<ast::Mac> for ItemImplItemBuilder<F>
    where F: Invoke<ast::ImplItem>,
{
    type Result = F::Result;

    fn invoke(self, mac: ast::Mac) -> F::Result {
        let node = ast::ImplItemKind::Macro(mac);
        self.build_item(node)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ItemImplMethodBuilder<F> {
    builder: ItemImplItemBuilder<F>,
    method: ast::MethodSig,
}

impl<F> ItemImplMethodBuilder<F>
    where F: Invoke<ast::ImplItem>,
{
    pub fn build_block(self, block: P<ast::Block>) -> F::Result {
        let node = ast::ImplItemKind::Method(self.method, block);
        self.builder.build_item(node)
    }

    pub fn block(self) -> BlockBuilder<Self> {
        BlockBuilder::with_callback(self)
    }
}

impl<F> Invoke<P<ast::Block>> for ItemImplMethodBuilder<F>
    where F: Invoke<ast::ImplItem>,
{
    type Result = F::Result;

    fn invoke(self, block: P<ast::Block>) -> Self::Result {
        self.build_block(block)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ItemConstBuilder<F> {
    builder: ItemBuilder<F>,
    id: ast::Ident,
}

impl<F> Invoke<Const> for ItemConstBuilder<F>
    where F: Invoke<P<ast::Item>>,
{
    type Result = F::Result;

    fn invoke(self, const_: Const) -> F::Result {
        let ty = ast::ItemKind::Const(const_.ty, const_.expr.expect("an expr is required for a const item"));
        self.builder.build_item_kind(self.id, ty)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ItemModBuilder<F> {
    builder: ItemBuilder<F>,
    ident: ast::Ident,
    vis: ast::Visibility,
    attrs: Vec<ast::Attribute>,
    span: Span,
    items: Vec<P<ast::Item>>,
}

impl<F> ItemModBuilder<F>
{
    pub fn item(self) -> ItemBuilder<Self> {
        ItemBuilder::with_callback(self)
    }

    pub fn build(self) -> F::Result
        where F: Invoke<P<ast::Item>>,
    {
        let item = ast::Item {
            ident: self.ident,
            attrs: self.attrs,
            id: ast::DUMMY_NODE_ID,
            vis: self.vis,
            span: self.span,
            node: ast::ItemKind::Mod(ast::Mod {
                inner: DUMMY_SP,
                items: self.items
            }),
        };

        self.builder.callback.invoke(P(item))
    }
}

impl<F> Invoke<P<ast::Item>> for ItemModBuilder<F> {
    type Result = Self;
    
    fn invoke(mut self, item: P<ast::Item>) -> Self {
        self.items.push(item);
        self
    }
}
