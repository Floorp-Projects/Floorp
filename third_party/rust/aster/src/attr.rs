use std::iter::IntoIterator;

use syntax::ast;
use syntax::attr;
use syntax::codemap::{DUMMY_SP, Span, respan};
use syntax::ptr::P;
use syntax::symbol::Symbol;

use invoke::{Invoke, Identity};
use lit::LitBuilder;
use symbol::ToSymbol;

//////////////////////////////////////////////////////////////////////////////

pub struct AttrBuilder<F=Identity> {
    callback: F,
    span: Span,
    style: ast::AttrStyle,
    is_sugared_doc: bool,
}

impl AttrBuilder {
    pub fn new() -> Self {
        AttrBuilder::with_callback(Identity)
    }
}

impl<F> AttrBuilder<F>
    where F: Invoke<ast::Attribute>,
{
    pub fn with_callback(callback: F) -> Self {
        AttrBuilder {
            callback: callback,
            span: DUMMY_SP,
            style: ast::AttrStyle::Outer,
            is_sugared_doc: false,
        }
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn inner(mut self) -> Self {
        self.style = ast::AttrStyle::Inner;
        self
    }

    pub fn outer(mut self) -> Self {
        self.style = ast::AttrStyle::Outer;
        self
    }

    pub fn build_meta_item(self, item: ast::MetaItem) -> F::Result {
        let attr = ast::Attribute {
            id: attr::mk_attr_id(),
            style: self.style,
            value: item,
            is_sugared_doc: self.is_sugared_doc,
            span: self.span,
        };
        self.callback.invoke(attr)
    }

    pub fn named<N>(self, name: N) -> NamedAttrBuilder<Self>
        where N: ToSymbol
    {
        NamedAttrBuilder::with_callback(name, self)
    }

    pub fn word<T>(self, word: T) -> F::Result
        where T: ToSymbol
    {
        let item = ast::MetaItem {
            name: word.to_symbol(),
            node: ast::MetaItemKind::Word,
            span: self.span,
        };
        self.build_meta_item(item)
    }

    pub fn list<T>(self, word: T) -> AttrListBuilder<Self>
        where T: ToSymbol
    {
        AttrListBuilder::with_callback(word, self)
    }

    pub fn name_value<T>(self, name: T) -> LitBuilder<AttrNameValueBuilder<Self>>
        where T: ToSymbol,
    {
        LitBuilder::with_callback(AttrNameValueBuilder::with_callback(name, self))
    }

    pub fn automatically_derived(self) -> F::Result {
        self.word("automatically_derived")
    }

    pub fn inline(self) -> F::Result {
        self.word("inline")
    }

    pub fn test(self) -> F::Result {
        self.word("test")
    }

    pub fn allow<I, T>(self, iter: I) -> F::Result
        where I: IntoIterator<Item=T>,
              T: ToSymbol,
    {
        self.list("allow").words(iter).build()
    }

    pub fn warn<I, T>(self, iter: I) -> F::Result
        where I: IntoIterator<Item=T>,
              T: ToSymbol,
    {
        self.list("warn").words(iter).build()
    }

    pub fn deny<I, T>(self, iter: I) -> F::Result
        where I: IntoIterator<Item=T>,
              T: ToSymbol,
    {
        self.list("deny").words(iter).build()
    }

    pub fn features<I, T>(self, iter: I) -> F::Result
        where I: IntoIterator<Item=T>,
              T: ToSymbol,
    {
        self.list("feature").words(iter).build()
    }

    pub fn plugins<I, T>(self, iter: I) -> F::Result
        where I: IntoIterator<Item=T>,
              T: ToSymbol,
    {
        self.list("plugin").words(iter).build()
    }

    /**
     * Create a #[doc = "..."] node. Note that callers of this must make sure to prefix their
     * comments with either "///" or "/\*\*" if an outer comment, or "//!" or "/\*!" if an inner
     * comment.
     */
    pub fn doc<T>(mut self, doc: T) -> F::Result
        where T: ToSymbol,
    {
        self.is_sugared_doc = true;
        self.name_value("doc").str(doc)
    }
}

impl<F> Invoke<ast::MetaItem> for AttrBuilder<F>
    where F: Invoke<ast::Attribute>,
{
    type Result = F::Result;

    fn invoke(self, item: ast::MetaItem) -> F::Result {
        self.build_meta_item(item)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct NamedAttrBuilder<F> {
    callback: F,
    span: Span,
    name: Symbol,
}

impl<F> NamedAttrBuilder<F>
    where F: Invoke<ast::MetaItem>
{
    pub fn with_callback<T>(name: T, callback: F) -> Self
        where T: ToSymbol
    {
        NamedAttrBuilder {
            callback: callback,
            span: DUMMY_SP,
            name: name.to_symbol(),
        }
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn word(self) -> F::Result {
        let item = ast::MetaItem {
            name: self.name,
            node: ast::MetaItemKind::Word,
            span: self.span,
        };
        self.callback.invoke(item)
    }

    pub fn list(self) -> AttrListBuilder<F> {
        AttrListBuilder::with_callback(self.name, self.callback).span(self.span)
    }

    pub fn name_value(self) -> LitBuilder<AttrNameValueBuilder<F>> {
        LitBuilder::with_callback(AttrNameValueBuilder::with_callback(self.name, self.callback))
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct AttrListBuilder<F> {
    callback: F,
    span: Span,
    name: Symbol,
    items: Vec<ast::NestedMetaItem>,
}

impl<F> AttrListBuilder<F>
    where F: Invoke<ast::MetaItem>,
{
    pub fn with_callback<T>(name: T, callback: F) -> Self
        where T: ToSymbol,
    {
        AttrListBuilder {
            callback: callback,
            span: DUMMY_SP,
            name: name.to_symbol(),
            items: vec![],
        }
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn with_meta_items<I>(mut self, iter: I) -> Self
        where I: IntoIterator<Item=ast::MetaItem>,
    {
        let span = self.span;
        self.items.extend(iter.into_iter().map(|meta_item| {
            respan(span, ast::NestedMetaItemKind::MetaItem(meta_item))
        }));
        self
    }

    pub fn with_meta_item(mut self, item: ast::MetaItem) -> Self {
        self.items.push(respan(self.span, ast::NestedMetaItemKind::MetaItem(item)));
        self
    }

    pub fn words<I, T>(self, iter: I) -> Self
        where I: IntoIterator<Item=T>,
              T: ToSymbol,
    {
        let iter = iter.into_iter();
        let span = self.span;
        self.with_meta_items(iter.map(|item| ast::MetaItem {
            name: item.to_symbol(),
            node: ast::MetaItemKind::Word,
            span: span,
        }))
    }

    pub fn word<T>(self, word: T) -> Self
        where T: ToSymbol,
    {
        let span = self.span;
        self.with_meta_item(ast::MetaItem {
            name: word.to_symbol(),
            node: ast::MetaItemKind::Word,
            span: span,
        })
    }

    pub fn list<T>(self, name: T) -> AttrListBuilder<Self>
        where T: ToSymbol,
    {
        AttrListBuilder::with_callback(name, self)
    }

    pub fn name_value<T>(self, name: T) -> LitBuilder<AttrNameValueBuilder<Self>>
        where T: ToSymbol,
    {
        let span = self.span;
        LitBuilder::with_callback(AttrNameValueBuilder {
            callback: self,
            name: name.to_symbol(),
            span: span,
        })
    }

    pub fn build(self) -> F::Result {
        let item = ast::MetaItem {
            name: self.name,
            node: ast::MetaItemKind::List(self.items),
            span: self.span,
        };
        self.callback.invoke(item)
    }
}

impl<F> Invoke<ast::MetaItem> for AttrListBuilder<F>
    where F: Invoke<ast::MetaItem>,
{
    type Result = Self;

    fn invoke(self, item: ast::MetaItem) -> Self {
        self.with_meta_item(item)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct AttrNameValueBuilder<F> {
    callback: F,
    span: Span,
    name: Symbol,
}

impl<F> AttrNameValueBuilder<F> {
    pub fn with_callback<T>(name: T, callback: F) -> Self
        where T: ToSymbol
    {
        AttrNameValueBuilder {
            callback: callback,
            span: DUMMY_SP,
            name: name.to_symbol(),
        }
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }
}

impl<F: Invoke<ast::MetaItem>> Invoke<P<ast::Lit>> for AttrNameValueBuilder<F> {
    type Result = F::Result;

    fn invoke(self, value: P<ast::Lit>) -> F::Result {
        let item = ast::MetaItem {
            name: self.name,
            node: ast::MetaItemKind::NameValue((*value).clone()),
            span: self.span,
        };
        self.callback.invoke(item)
    }
}
