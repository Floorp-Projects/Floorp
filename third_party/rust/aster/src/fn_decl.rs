use syntax::ast;
use syntax::codemap::{DUMMY_SP, Span, respan};
use syntax::ptr::P;

use ident::ToIdent;
use invoke::{Invoke, Identity};
use pat::PatBuilder;
use self_::SelfBuilder;
use ty::TyBuilder;

//////////////////////////////////////////////////////////////////////////////

pub struct FnDeclBuilder<F=Identity> {
    callback: F,
    span: Span,
    args: Vec<ast::Arg>,
    variadic: bool,
}

impl FnDeclBuilder {
    pub fn new() -> FnDeclBuilder {
        FnDeclBuilder::with_callback(Identity)
    }
}

impl<F> FnDeclBuilder<F>
    where F: Invoke<P<ast::FnDecl>>,
{
    pub fn with_callback(callback: F) -> Self {
        FnDeclBuilder {
            callback: callback,
            span: DUMMY_SP,
            args: Vec::new(),
            variadic: false,
        }
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn variadic(mut self) -> Self {
        self.variadic = true;
        self
    }

    pub fn with_self(self, explicit_self: ast::ExplicitSelf) -> Self {
        let self_ident = respan(self.span, "self".to_ident());
        self.with_arg(ast::Arg::from_self(explicit_self, self_ident))
    }

    pub fn self_(self) -> SelfBuilder<Self> {
        SelfBuilder::with_callback(self)
    }

    pub fn with_arg(mut self, arg: ast::Arg) -> Self {
        self.args.push(arg);
        self
    }

    pub fn with_args<I>(mut self, iter: I) -> Self
        where I: IntoIterator<Item=ast::Arg>
    {
        self.args.extend(iter);
        self
    }

    pub fn arg(self) -> ArgBuilder<Self> {
        ArgBuilder::with_callback(self)
    }

    pub fn arg_id<T>(self, id: T) -> ArgPatBuilder<Self>
        where T: ToIdent,
    {
        self.arg().pat().id(id)
    }

    pub fn arg_ref_id<T>(self, id: T) -> ArgPatBuilder<Self>
        where T: ToIdent,
    {
        self.arg().ref_id(id)
    }

    pub fn arg_mut_id<T>(self, id: T) -> ArgPatBuilder<Self>
        where T: ToIdent,
    {
        self.arg().mut_id(id)
    }

    pub fn arg_ref_mut_id<T>(self, id: T) -> ArgPatBuilder<Self>
        where T: ToIdent,
    {
        self.arg().ref_mut_id(id)
    }

    pub fn no_return(self) -> F::Result {
        self.return_().never()
    }

    pub fn default_return(self) -> F::Result {
        let ret_ty = ast::FunctionRetTy::Default(self.span);
        self.build(ret_ty)
    }

    pub fn build_return(self, ty: P<ast::Ty>) -> F::Result {
        self.build(ast::FunctionRetTy::Ty(ty))
    }

    pub fn return_(self) -> TyBuilder<Self> {
        let span = self.span;
        TyBuilder::with_callback(self).span(span)
    }

    pub fn build(self, output: ast::FunctionRetTy) -> F::Result {
        self.callback.invoke(P(ast::FnDecl {
            inputs: self.args,
            output: output,
            variadic: self.variadic,
        }))
    }
}

impl<F> Invoke<ast::Arg> for FnDeclBuilder<F>
    where F: Invoke<P<ast::FnDecl>>
{
    type Result = Self;

    fn invoke(self, arg: ast::Arg) -> Self {
        self.with_arg(arg)
    }
}

impl<F> Invoke<P<ast::Ty>> for FnDeclBuilder<F>
    where F: Invoke<P<ast::FnDecl>>,
{
    type Result = F::Result;

    fn invoke(self, ty: P<ast::Ty>) -> F::Result {
        self.build_return(ty)
    }
}

impl<F> Invoke<ast::ExplicitSelf> for FnDeclBuilder<F>
    where F: Invoke<P<ast::FnDecl>>,
{
    type Result = Self;

    fn invoke(self, explicit_self: ast::ExplicitSelf) -> Self {
        self.with_self(explicit_self)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ArgBuilder<F=Identity> {
    callback: F,
    span: Span,
}

impl ArgBuilder {
    pub fn new() -> Self {
        ArgBuilder::with_callback( Identity)
    }
}

impl<F> ArgBuilder<F>
    where F: Invoke<ast::Arg>,
{
    pub fn with_callback(callback: F) -> ArgBuilder<F> {
        ArgBuilder {
            callback: callback,
            span: DUMMY_SP,
        }
    }

    pub fn span(mut self, span: Span) -> Self {
        self.span = span;
        self
    }

    pub fn with_pat(self, pat: P<ast::Pat>) -> ArgPatBuilder<F> {
        ArgPatBuilder {
            callback: self.callback,
            span: self.span,
            pat: pat,
        }
    }

    pub fn pat(self) -> PatBuilder<Self> {
        PatBuilder::with_callback(self)
    }

    pub fn id<T>(self, id: T) -> ArgPatBuilder<F>
        where T: ToIdent,
    {
        self.pat().id(id)
    }

    pub fn ref_id<T>(self, id: T) -> ArgPatBuilder<F>
        where T: ToIdent,
    {
        self.pat().ref_id(id)
    }

    pub fn mut_id<T>(self, id: T) -> ArgPatBuilder<F>
        where T: ToIdent,
    {
        self.pat().mut_id(id)
    }

    pub fn ref_mut_id<T>(self, id: T) -> ArgPatBuilder<F>
        where T: ToIdent,
    {
        self.pat().ref_mut_id(id)
    }
}

impl<F> Invoke<P<ast::Pat>> for ArgBuilder<F>
    where F: Invoke<ast::Arg>
{
    type Result = ArgPatBuilder<F>;

    fn invoke(self, pat: P<ast::Pat>) -> Self::Result {
        self.with_pat(pat)
    }
}

//////////////////////////////////////////////////////////////////////////////

pub struct ArgPatBuilder<F> {
    callback: F,
    span: Span,
    pat: P<ast::Pat>,
}

impl<F> ArgPatBuilder<F>
    where F: Invoke<ast::Arg>
{
    pub fn with_ty(self, ty: P<ast::Ty>) -> F::Result {
        self.callback.invoke(ast::Arg {
            id: ast::DUMMY_NODE_ID,
            ty: ty,
            pat: self.pat,
        })
    }

    pub fn ty(self) -> TyBuilder<Self> {
        let span = self.span;
        TyBuilder::with_callback(self).span(span)
    }
}

impl<F> Invoke<P<ast::Ty>> for ArgPatBuilder<F>
    where F: Invoke<ast::Arg>
{
    type Result = F::Result;

    fn invoke(self, ty: P<ast::Ty>) -> F::Result {
        self.with_ty(ty)
    }
}
