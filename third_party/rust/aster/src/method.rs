use syntax::abi::Abi;
use syntax::ast;
use syntax::codemap::{DUMMY_SP, respan, Span};
use syntax::ptr::P;

use fn_decl::FnDeclBuilder;
use generics::GenericsBuilder;
use invoke::{Invoke, Identity};

//////////////////////////////////////////////////////////////////////////////

pub struct MethodSigBuilder<F=Identity> {
    callback: F,
    span: Span,
    abi: Abi,
    generics: ast::Generics,
    unsafety: ast::Unsafety,
    constness: ast::Constness,
}

impl MethodSigBuilder {
    pub fn new() -> Self {
        MethodSigBuilder::with_callback(Identity)
    }
}

impl<F> MethodSigBuilder<F>
    where F: Invoke<ast::MethodSig>,
{
    pub fn with_callback(callback: F) -> Self {
        MethodSigBuilder {
            callback: callback,
            span: DUMMY_SP,
            abi: Abi::Rust,
            generics: GenericsBuilder::new().build(),
            unsafety: ast::Unsafety::Normal,
            constness: ast::Constness::NotConst,
        }
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

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

    pub fn with_generics(mut self, generics: ast::Generics) -> Self {
        self.generics = generics;
        self
    }

    pub fn generics(self) -> GenericsBuilder<Self> {
        GenericsBuilder::with_callback(self)
    }

    pub fn build_fn_decl(self, fn_decl: P<ast::FnDecl>) -> F::Result {
        self.callback.invoke(ast::MethodSig {
            unsafety: self.unsafety,
            constness: respan(self.span, self.constness),
            abi: self.abi,
            decl: fn_decl,
            generics: self.generics,
        })
    }

    pub fn fn_decl(self) -> FnDeclBuilder<Self> {
        FnDeclBuilder::with_callback(self)
    }
}

impl<F> Invoke<ast::Generics> for MethodSigBuilder<F>
    where F: Invoke<ast::MethodSig>,
{
    type Result = Self;

    fn invoke(self, generics: ast::Generics) -> Self {
        self.with_generics(generics)
    }
}

impl<F> Invoke<P<ast::FnDecl>> for MethodSigBuilder<F>
    where F: Invoke<ast::MethodSig>,
{
    type Result = F::Result;

    fn invoke(self, fn_decl: P<ast::FnDecl>) -> Self::Result {
        self.build_fn_decl(fn_decl)
    }
}
