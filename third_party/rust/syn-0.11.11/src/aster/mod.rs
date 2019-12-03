use super::*;

pub mod generics;
pub mod ident;
pub mod invoke;
pub mod lifetime;
pub mod path;
pub mod qpath;
pub mod ty;
pub mod ty_param;
pub mod where_predicate;

pub fn id<I>(id: I) -> Ident
    where I: Into<Ident>
{
    id.into()
}

pub fn from_generics(generics: Generics) -> generics::GenericsBuilder {
    generics::GenericsBuilder::from_generics(generics)
}

pub fn where_predicate() -> where_predicate::WherePredicateBuilder {
    where_predicate::WherePredicateBuilder::new()
}

pub fn ty() -> ty::TyBuilder {
    ty::TyBuilder::new()
}

pub fn path() -> path::PathBuilder {
    path::PathBuilder::new()
}
