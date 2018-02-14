use Ident;

pub trait ToIdent {
    fn to_ident(&self) -> Ident;
}

impl ToIdent for Ident {
    fn to_ident(&self) -> Ident {
        self.clone()
    }
}

impl<'a> ToIdent for &'a str {
    fn to_ident(&self) -> Ident {
        (**self).into()
    }
}

impl ToIdent for String {
    fn to_ident(&self) -> Ident {
        self.clone().into()
    }
}

impl<'a, T> ToIdent for &'a T
    where T: ToIdent
{
    fn to_ident(&self) -> Ident {
        (**self).to_ident()
    }
}

impl<'a, T> ToIdent for &'a mut T
    where T: ToIdent
{
    fn to_ident(&self) -> Ident {
        (**self).to_ident()
    }
}
