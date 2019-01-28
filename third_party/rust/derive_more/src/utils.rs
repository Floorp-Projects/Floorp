use proc_macro2::{Span, TokenStream};
use syn::{
    parse_str, Field, FieldsNamed, FieldsUnnamed, GenericParam,
    Generics, Ident, Index, Type, TypeParamBound, WhereClause,
};

pub fn numbered_vars(count: usize, prefix: &str) -> Vec<Ident> {
    (0..count)
        .map(|i| Ident::new(&format!("__{}{}", prefix, i), Span::call_site()))
        .collect()
}

pub fn number_idents(count: usize) -> Vec<Index> {
    (0..count).map(Index::from).collect()
}

pub fn field_idents<'a>(fields: &'a [&'a Field]) -> Vec<&'a Ident> {
    fields
        .iter()
        .map(|f| {
            f.ident
                .as_ref()
                .expect("Tried to get field names of a tuple struct")
        }).collect()
}

pub fn get_field_types_iter<'a>(fields: &'a [&'a Field]) -> Box<Iterator<Item = &'a Type> + 'a> {
    Box::new(fields.iter().map(|f| &f.ty))
}

pub fn get_field_types<'a>(fields: &'a [&'a Field]) -> Vec<&'a Type> {
    get_field_types_iter(fields).collect()
}

pub fn add_extra_type_param_bound_op_output<'a>(
    generics: &'a Generics,
    trait_ident: &'a Ident,
) -> Generics {
    let mut generics = generics.clone();
    for type_param in &mut generics.type_params_mut() {
        let type_ident = &type_param.ident;
        let bound: TypeParamBound =
            parse_str(&quote!(::std::ops::#trait_ident<Output=#type_ident>).to_string()).unwrap();
        type_param.bounds.push(bound)
    }

    generics
}

pub fn add_extra_ty_param_bound_op<'a>(generics: &'a Generics, trait_ident: &'a Ident) -> Generics {
    add_extra_ty_param_bound(generics, &quote!(::std::ops::#trait_ident))
}

pub fn add_extra_ty_param_bound<'a>(generics: &'a Generics, bound: &'a TokenStream) -> Generics {
    let mut generics = generics.clone();
    let bound: TypeParamBound = parse_str(&bound.to_string()).unwrap();
    for type_param in &mut generics.type_params_mut() {
        type_param.bounds.push(bound.clone())
    }

    generics
}

pub fn add_extra_generic_param(generics: &Generics, generic_param: TokenStream) -> Generics {
    let generic_param: GenericParam = parse_str(&generic_param.to_string()).unwrap();
    let mut generics = generics.clone();
    generics.params.push(generic_param);

    generics
}

pub fn add_extra_where_clauses(generics: &Generics, type_where_clauses: TokenStream) -> Generics {
    let mut type_where_clauses: WhereClause = parse_str(&type_where_clauses.to_string()).unwrap();
    let mut new_generics = generics.clone();
    if let Some(old_where) = new_generics.where_clause {
        type_where_clauses.predicates.extend(old_where.predicates)
    }
    new_generics.where_clause = Some(type_where_clauses);

    new_generics
}

pub fn add_where_clauses_for_new_ident<'a>(
    generics: &'a Generics,
    fields: &[&'a Field],
    type_ident: &Ident,
    type_where_clauses: TokenStream,
) -> Generics {
    let generic_param = if fields.len() > 1 {
        quote!(#type_ident: ::std::marker::Copy)
    } else {
        quote!(#type_ident)
    };

    let generics = add_extra_where_clauses(generics, type_where_clauses);
    add_extra_generic_param(&generics, generic_param)
}

pub fn unnamed_to_vec(fields: &FieldsUnnamed) -> Vec<&Field> {
    fields.unnamed.iter().collect()
}

pub fn named_to_vec(fields: &FieldsNamed) -> Vec<&Field> {
    fields.named.iter().collect()
}
