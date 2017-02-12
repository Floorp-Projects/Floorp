use syntax::ast;

use symbol::ToSymbol;

//////////////////////////////////////////////////////////////////////////////

pub trait ToIdent {
    fn to_ident(&self) -> ast::Ident;
}

impl ToIdent for ast::Ident {
    fn to_ident(&self) -> ast::Ident {
        *self
    }
}

impl ToIdent for ast::Name {
    fn to_ident(&self) -> ast::Ident {
        ast::Ident::with_empty_ctxt(*self)
    }
}

impl<'a> ToIdent for &'a str {
    fn to_ident(&self) -> ast::Ident {
        self.to_symbol().to_ident()
    }
}

impl ToIdent for String {
    fn to_ident(&self) -> ast::Ident {
        (&**self).to_ident()
    }
}

impl<'a, T> ToIdent for &'a T where T: ToIdent {
    fn to_ident(&self) -> ast::Ident {
        (**self).to_ident()
    }
}

impl<'a, T> ToIdent for &'a mut T where T: ToIdent {
    fn to_ident(&self) -> ast::Ident {
        (**self).to_ident()
    }
}
