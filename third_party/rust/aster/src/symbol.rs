use syntax::ast;
use syntax::symbol::{InternedString, Symbol};

//////////////////////////////////////////////////////////////////////////////

pub trait ToSymbol {
    fn to_symbol(&self) -> Symbol;
}

impl ToSymbol for Symbol {
    fn to_symbol(&self) -> Symbol {
        *self
    }
}

impl<'a> ToSymbol for &'a str {
    fn to_symbol(&self) -> Symbol {
        Symbol::intern(self)
    }
}

impl ToSymbol for ast::Ident {
    fn to_symbol(&self) -> Symbol {
        self.name
    }
}

impl ToSymbol for InternedString {
    fn to_symbol(&self) -> Symbol {
        Symbol::intern(self)
    }
}

impl<'a, T> ToSymbol for &'a T where T: ToSymbol {
    fn to_symbol(&self) -> Symbol {
        (**self).to_symbol()
    }
}

impl<'a, T> ToSymbol for &'a mut T where T: ToSymbol {
    fn to_symbol(&self) -> Symbol {
        (**self).to_symbol()
    }
}
