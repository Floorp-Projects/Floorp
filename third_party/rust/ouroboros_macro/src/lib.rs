use inflector::Inflector;
use proc_macro::TokenStream;
use proc_macro2::TokenStream as TokenStream2;
use proc_macro2::{Group, Span, TokenTree};
use proc_macro_error::proc_macro_error;
use quote::{format_ident, quote, ToTokens};
use syn::{
    Attribute, Error, Fields, GenericArgument, GenericParam, Generics, Ident, ItemStruct, Lifetime,
    PathArguments, Type, Visibility, WhereClause,
};

#[derive(Clone, Copy, PartialEq)]
enum FieldType {
    /// Not borrowed by other parts of the struct.
    Tail,
    /// Immutably borrowed by at least one other field.
    Borrowed,
    /// Mutably borrowed by one other field.
    BorrowedMut,
}

impl FieldType {
    fn is_tail(self) -> bool {
        self == Self::Tail
    }
}

struct BorrowRequest {
    index: usize,
    mutable: bool,
}

struct StructFieldInfo {
    name: Ident,
    typ: Type,
    field_type: FieldType,
    vis: Visibility,
    borrows: Vec<BorrowRequest>,
    /// If this is true and borrows is empty, the struct will borrow from self in the future but
    /// does not require a builder to be initialized. It should not be able to be removed from the
    /// struct with into_heads.
    self_referencing: bool,
    /// If it is None, the user has not specified whether or not the field is covariant. If this is
    /// Some(false), we should avoid making borrow_* or borrow_*_mut functions as they will not
    /// be able to compile.
    covariant: Option<bool>,
}

impl StructFieldInfo {
    fn builder_name(&self) -> Ident {
        format_ident!("{}_builder", self.name)
    }

    fn illegal_ref_name(&self) -> Ident {
        format_ident!("{}_illegal_static_reference", self.name)
    }

    // Returns code which takes a variable with the same name and type as this field and turns it
    // into a static reference to its dereffed contents. For example, suppose a field
    // `test: Box<i32>`. This method would generate code that looks like:
    // ```rust
    // // Variable name taken from self.illegal_ref_name()
    // let test_illegal_static_reference = unsafe {
    //     ::ouroboros::macro_help::stable_deref_and_change_lifetime(
    //         &((*result.as_ptr()).field)
    //     )
    // };
    // ```
    fn make_illegal_static_reference(&self) -> TokenStream2 {
        let field_name = &self.name;
        let ref_name = self.illegal_ref_name();
        quote! {
            let #ref_name = unsafe {
                ::ouroboros::macro_help::stable_deref_and_change_lifetime(
                    &((*result.as_ptr()).#field_name)
                )
            };
        }
    }

    /// Like make_illegal_static_reference, but provides a mutable reference instead.
    fn make_illegal_static_mut_reference(&self) -> TokenStream2 {
        let field_name = &self.name;
        let ref_name = self.illegal_ref_name();
        quote! {
            let #ref_name = unsafe {
                ::ouroboros::macro_help::stable_deref_and_change_lifetime_mut(
                    &mut ((*result.as_mut_ptr()).#field_name)
                )
            };
        }
    }

    /// Generates an error requesting that the user explicitly specify whether or not the
    /// field's type is covariant.
    fn covariance_error(&self) {
        let error = concat!(
            "Ouroboros cannot automatically determine if this type is covariant.\n\n",
            "If it is covariant, it should be legal to convert any instance of that type to an ",
            "instance of that type where all usages of 'this are replaced with a smaller ",
            "lifetime. For example, Box<&'this i32> is covariant because it is legal to use it as ",
            "a Box<&'a i32> where 'this: 'a. In contrast, Fn(&'this i32) cannot be used as ",
            "Fn(&'a i32).\n\n",
            "To resolve this error, add #[covariant] or #[not_covariant] to the field.\n",
        );
        proc_macro_error::emit_error!(self.typ, error);
    }
}

const STD_CONTAINER_TYPES: &[&str] = &["Box", "Arc", "Rc"];

/// Returns Some((type_name, element_type)) if the provided type appears to be Box, Arc, or Rc from
/// the standard library. Returns None if not.
fn apparent_std_container_type(raw_type: &Type) -> Option<(&'static str, &Type)> {
    let tpath = if let Type::Path(x) = raw_type {
        x
    } else {
        return None;
    };
    let segment = if let Some(segment) = tpath.path.segments.last() {
        segment
    } else {
        return None;
    };
    let args = if let PathArguments::AngleBracketed(args) = &segment.arguments {
        args
    } else {
        return None;
    };
    if args.args.len() != 1 {
        return None;
    }
    let arg = args.args.first().unwrap();
    let eltype = if let GenericArgument::Type(x) = arg {
        x
    } else {
        return None;
    };
    for type_name in STD_CONTAINER_TYPES {
        if segment.ident == type_name {
            return Some((type_name, eltype));
        }
    }
    None
}

enum ArgType {
    /// Used when the initial value of a field can be passed directly into the constructor.
    Plain(TokenStream2),
    /// Used when a field requires self references and thus requires something that implements
    /// a builder function trait instead of a simple plain type.
    TraitBound(TokenStream2),
}

fn deref_type(field_type: &Type, do_chain_hack: bool) -> Result<TokenStream2, Error> {
    if do_chain_hack {
        if let Some((_std_type, eltype)) = apparent_std_container_type(field_type) {
            return Ok(quote! { #eltype });
        }
        Err(Error::new_spanned(
            &field_type,
            concat!(
                "Borrowed fields must be of type Box<T>, Arc<T>, or Rc<T> when chain_hack is ",
                "used. Either change the field to one of the listed types or remove chain_hack."
            ),
        ))
    } else {
        Ok(quote! { <#field_type as ::core::ops::Deref>::Target })
    }
}

fn make_constructor_arg_type_impl(
    for_field: &StructFieldInfo,
    other_fields: &[StructFieldInfo],
    fake_lifetime: &Ident,
    make_builder_return_type: impl FnOnce() -> TokenStream2,
    do_chain_hack: bool,
) -> Result<ArgType, Error> {
    let field_type = &for_field.typ;
    if for_field.borrows.is_empty() {
        // Even if self_referencing is true, as long as borrows is empty, we don't need to use a
        // builder to construct it.
        let field_type =
            replace_this_with_lifetime(field_type.into_token_stream(), fake_lifetime.clone());
        Ok(ArgType::Plain(quote! { #field_type }))
    } else {
        let mut field_builder_params = Vec::new();
        for borrow in &for_field.borrows {
            if borrow.mutable {
                let field = &other_fields[borrow.index];
                let field_type = &field.typ;
                let content_type = deref_type(field_type, do_chain_hack)?;
                field_builder_params.push(quote! {
                    &'this mut #content_type
                });
            } else {
                let field = &other_fields[borrow.index];
                let field_type = &field.typ;
                let content_type = deref_type(field_type, do_chain_hack)?;
                field_builder_params.push(quote! {
                    &'this #content_type
                });
            }
        }
        let return_type = make_builder_return_type();
        let bound =
            quote! { for<'this> ::core::ops::FnOnce(#(#field_builder_params),*) -> #return_type };
        Ok(ArgType::TraitBound(bound))
    }
}

/// Returns a trait bound if `for_field` refers to any other fields, and a plain type if not. This
/// is the type used in the constructor to initialize the value of `for_field`.
fn make_constructor_arg_type(
    for_field: &StructFieldInfo,
    other_fields: &[StructFieldInfo],
    fake_lifetime: &Ident,
    do_chain_hack: bool,
    make_async: bool,
) -> Result<ArgType, Error> {
    let field_type = &for_field.typ;
    let return_ty_constructor = || {
        if make_async {
            quote! { ::std::pin::Pin<::std::boxed::Box<dyn ::core::future::Future<Output=#field_type> + 'this>> }
        } else {
            quote! { #field_type }
        }
    };
    make_constructor_arg_type_impl(
        for_field,
        other_fields,
        fake_lifetime,
        return_ty_constructor,
        do_chain_hack,
    )
}

/// Like make_constructor_arg_type, but used for the try_new constructor.
fn make_try_constructor_arg_type(
    for_field: &StructFieldInfo,
    other_fields: &[StructFieldInfo],
    fake_lifetime: &Ident,
    do_chain_hack: bool,
    make_async: bool,
) -> Result<ArgType, Error> {
    let field_type = &for_field.typ;
    let return_ty_constructor = || {
        if make_async {
            quote! { ::std::pin::Pin<::std::boxed::Box<dyn ::core::future::Future<Output=::core::result::Result<#field_type, Error_>> + 'this>> }
        } else {
            quote! { ::core::result::Result<#field_type, Error_> }
        }
    };
    make_constructor_arg_type_impl(
        for_field,
        other_fields,
        fake_lifetime,
        return_ty_constructor,
        do_chain_hack,
    )
}

/// Makes phantom data definitions so that we don't get unused template parameter errors.
fn make_template_consumers(generics: &Generics) -> impl Iterator<Item = (TokenStream2, Ident)> {
    generics
        .params
        .clone()
        .into_iter()
        .map(|param| match param {
            GenericParam::Type(ty) => {
                let ident = &ty.ident;
                (
                    quote! { #ident },
                    format_ident!(
                        "_consume_template_type_{}",
                        ident.to_string().to_snake_case()
                    ),
                )
            }
            GenericParam::Lifetime(lt) => {
                let lifetime = &lt.lifetime;
                let ident = &lifetime.ident;
                (
                    quote! { &#lifetime () },
                    format_ident!("_consume_template_lifetime_{}", ident),
                )
            }
            GenericParam::Const(..) => unimplemented!(),
        })
}

fn replace_this_with_lifetime(input: TokenStream2, lifetime: Ident) -> TokenStream2 {
    input
        .into_iter()
        .map(|token| match &token {
            TokenTree::Ident(ident) => {
                if ident == "this" {
                    TokenTree::Ident(lifetime.clone())
                } else {
                    token
                }
            }
            TokenTree::Group(group) => TokenTree::Group(Group::new(
                group.delimiter(),
                replace_this_with_lifetime(group.stream(), lifetime.clone()),
            )),
            _ => token,
        })
        .collect()
}

fn handle_borrows_attr(
    field_info: &mut [StructFieldInfo],
    attr: &Attribute,
    borrows: &mut Vec<BorrowRequest>,
) -> Result<(), Error> {
    let mut borrow_mut = false;
    let mut waiting_for_comma = false;
    let tokens = attr.tokens.clone();
    let possible_error = Error::new_spanned(&tokens, "Invalid syntax for borrows() macro.");
    let tokens = if let Some(TokenTree::Group(group)) = tokens.into_iter().next() {
        group.stream()
    } else {
        return Err(possible_error);
    };
    for token in tokens {
        if let TokenTree::Ident(ident) = token {
            if waiting_for_comma {
                return Err(Error::new_spanned(&ident, "Expected comma."));
            }
            let istr = ident.to_string();
            if istr == "mut" {
                if borrow_mut {
                    return Err(Error::new_spanned(&ident, "Unexpected double 'mut'"));
                }
                borrow_mut = true;
            } else {
                let index = field_info.iter().position(|item| item.name == istr);
                let index = if let Some(v) = index {
                    v
                } else {
                    return Err(Error::new_spanned(
                        &ident,
                        concat!(
                            "Unknown identifier, make sure that it is spelled ",
                            "correctly and defined above the location it is borrowed."
                        ),
                    ));
                };
                if borrow_mut {
                    if field_info[index].field_type == FieldType::Borrowed {
                        return Err(Error::new_spanned(
                            &ident,
                            "Cannot borrow mutably, this field was previously borrowed immutably.",
                        ));
                    }
                    if field_info[index].field_type == FieldType::BorrowedMut {
                        return Err(Error::new_spanned(&ident, "Cannot borrow mutably twice."));
                    }
                    field_info[index].field_type = FieldType::BorrowedMut;
                } else {
                    if field_info[index].field_type == FieldType::BorrowedMut {
                        return Err(Error::new_spanned(
                            &ident,
                            "Cannot borrow as immutable as it was previously borrowed mutably.",
                        ));
                    }
                    field_info[index].field_type = FieldType::Borrowed;
                }
                borrows.push(BorrowRequest {
                    index,
                    mutable: borrow_mut,
                });
                waiting_for_comma = true;
                borrow_mut = false;
            }
        } else if let TokenTree::Punct(punct) = token {
            if punct.as_char() == ',' {
                if waiting_for_comma {
                    waiting_for_comma = false;
                } else {
                    return Err(Error::new_spanned(&punct, "Unexpected extra comma."));
                }
            } else {
                return Err(Error::new_spanned(
                    &punct,
                    "Unexpected punctuation, expected comma or identifier.",
                ));
            }
        } else {
            return Err(Error::new_spanned(
                &token,
                "Unexpected token, expected comma or identifier.",
            ));
        }
    }
    Ok(())
}

/// Returns true if the specified type can be assumed to be covariant.
fn type_is_covariant(ty: &syn::Type, in_template: bool) -> bool {
    use syn::Type::*;
    match ty {
        Array(arr) => type_is_covariant(&*arr.elem, in_template),
        BareFn(f) => {
            for arg in f.inputs.iter() {
                if !type_is_covariant(&arg.ty, true) {
                    return false;
                }
            }
            if let syn::ReturnType::Type(_, ty) = &f.output {
                type_is_covariant(ty, true)
            } else {
                true
            }
        }
        Group(ty) => type_is_covariant(&ty.elem, in_template),
        ImplTrait(..) => false, // Unusable in struct definition.
        Infer(..) => false,     // Unusable in struct definition.
        Macro(..) => false,     // Assume false since we don't know.
        Never(..) => false,
        Paren(ty) => type_is_covariant(&ty.elem, in_template),
        Path(path) => {
            if let Some(qself) = &path.qself {
                if !type_is_covariant(&qself.ty, in_template) {
                    return false;
                }
            }
            let mut is_covariant = false;
            // If the type is Box, Arc, or Rc, we can assume it to be covariant.
            if apparent_std_container_type(ty).is_some() {
                is_covariant = true;
            }
            for segment in path.path.segments.iter() {
                let args = &segment.arguments;
                if let syn::PathArguments::AngleBracketed(args) = &args {
                    for arg in args.args.iter() {
                        if let syn::GenericArgument::Type(ty) = arg {
                            if !type_is_covariant(ty, !is_covariant) {
                                return false;
                            }
                        } else if let syn::GenericArgument::Lifetime(lt) = arg {
                            if lt.ident.to_string() == "this" && !is_covariant {
                                return false;
                            }
                        }
                    }
                } else if let syn::PathArguments::Parenthesized(args) = &args {
                    for arg in args.inputs.iter() {
                        if !type_is_covariant(arg, true) {
                            return false;
                        }
                    }
                    if let syn::ReturnType::Type(_, ty) = &args.output {
                        if !type_is_covariant(ty, true) {
                            return false;
                        }
                    }
                }
            }
            true
        }
        Ptr(ptr) => type_is_covariant(&ptr.elem, in_template),
        // Ignore the actual lifetime of the reference because Rust can automatically convert those.
        Reference(rf) => !in_template && type_is_covariant(&rf.elem, in_template),
        Slice(sl) => type_is_covariant(&sl.elem, in_template),
        // I don't think this is reachable but panic just in case.
        TraitObject(..) => false,
        Tuple(tup) => {
            for ty in tup.elems.iter() {
                if !type_is_covariant(ty, in_template) {
                    return false;
                }
            }
            false
        }
        // As of writing this, syn parses all the types we could need.
        Verbatim(..) => unimplemented!(),
        _ => unimplemented!(),
    }
}

/// Creates the struct that will actually store the data. This involves properly organizing the
/// fields, collecting metadata about them, reversing the order everything is stored in, and
/// converting any uses of 'this to 'static.
fn create_actual_struct(
    visibility: &Visibility,
    original_struct_def: &ItemStruct,
) -> Result<(TokenStream2, Ident, Vec<StructFieldInfo>), Error> {
    let mut actual_struct_def = original_struct_def.clone();
    actual_struct_def.vis = visibility.clone();
    let mut field_info = Vec::new();
    match &mut actual_struct_def.fields {
        Fields::Named(fields) => {
            for field in &mut fields.named {
                let mut borrows = Vec::new();
                let mut self_referencing = false;
                let covariant = type_is_covariant(&field.ty, false);
                let mut covariant = if covariant { Some(true) } else { None };
                let mut remove_attrs = Vec::new();
                for (index, attr) in field.attrs.iter().enumerate() {
                    let path = &attr.path;
                    if path.leading_colon.is_some() {
                        continue;
                    }
                    if path.segments.len() != 1 {
                        continue;
                    }
                    if path.segments.first().unwrap().ident == "borrows" {
                        if self_referencing {
                            panic!("TODO: Nice error, used #[borrows()] twice.");
                        }
                        self_referencing = true;
                        handle_borrows_attr(&mut field_info[..], attr, &mut borrows)?;
                        remove_attrs.push(index);
                    }
                    if path.segments.first().unwrap().ident == "covariant" {
                        covariant = Some(true);
                        remove_attrs.push(index);
                    }
                    if path.segments.first().unwrap().ident == "not_covariant" {
                        covariant = Some(false);
                        remove_attrs.push(index);
                    }
                }
                for index in remove_attrs.into_iter().rev() {
                    field.attrs.remove(index);
                }
                field.attrs.push(syn::parse_quote! { #[doc(hidden)] });
                // We should not be able to access the field outside of the hidden module where
                // everything is generated.
                let with_vis = submodule_contents_visiblity(&field.vis.clone());
                field.vis = syn::Visibility::Inherited;
                field_info.push(StructFieldInfo {
                    name: field.ident.clone().expect("Named field has no name."),
                    typ: field.ty.clone(),
                    field_type: FieldType::Tail,
                    vis: with_vis,
                    borrows,
                    self_referencing,
                    covariant,
                });
            }
        }
        Fields::Unnamed(_fields) => {
            return Err(Error::new(
                Span::call_site(),
                "Tuple structs are not supported yet.",
            ))
        }
        Fields::Unit => {
            return Err(Error::new(
                Span::call_site(),
                "Unit structs cannot be self-referential.",
            ))
        }
    }
    if field_info.len() < 2 {
        return Err(Error::new(
            Span::call_site(),
            "Self-referencing structs must have at least 2 fields.",
        ));
    }
    let mut has_non_tail = false;
    for field in &field_info {
        if !field.field_type.is_tail() {
            has_non_tail = true;
            break;
        }
    }
    if !has_non_tail {
        return Err(Error::new(
            Span::call_site(),
            &format!(
                concat!(
                    "Self-referencing struct cannot be made entirely of tail fields, try adding ",
                    "#[borrows({0})] to a field defined after {0}."
                ),
                field_info[0].name
            ),
        ));
    }
    // Reverse the order of all fields. We ensure that items in the struct are only dependent
    // on references to items above them. Rust drops items in a struct in forward declaration order.
    // This would cause parents being dropped before children, necessitating the reversal.
    match &mut actual_struct_def.fields {
        Fields::Named(fields) => {
            let reversed = fields.named.iter().rev().cloned().collect();
            fields.named = reversed;
        }
        Fields::Unnamed(_fields) => unreachable!("Error handled earlier."),
        Fields::Unit => unreachable!("Error handled earlier."),
    }

    let fake_lifetime =
        if let Some(GenericParam::Lifetime(param)) = actual_struct_def.generics.params.first() {
            param.lifetime.ident.clone()
        } else {
            format_ident!("static")
        };

    // Finally, replace the fake 'this lifetime with 'static.
    let actual_struct_def =
        replace_this_with_lifetime(quote! { #actual_struct_def }, fake_lifetime.clone());

    Ok((actual_struct_def, fake_lifetime, field_info))
}

// Takes the generics parameters from the original struct and turns them into arguments.
fn make_generic_arguments(generic_params: &Generics) -> Vec<TokenStream2> {
    let mut arguments = Vec::new();
    for generic in generic_params.params.clone() {
        match generic {
            GenericParam::Type(typ) => {
                let ident = &typ.ident;
                arguments.push(quote! { #ident });
            }
            GenericParam::Lifetime(lt) => {
                let lifetime = &lt.lifetime;
                arguments.push(quote! { #lifetime });
            }
            GenericParam::Const(_) => unimplemented!("Const generics are not supported yet."),
        }
    }
    arguments
}

fn create_builder_and_constructor(
    struct_visibility: &Visibility,
    struct_name: &Ident,
    builder_struct_name: &Ident,
    fake_lifetime: &Ident,
    generic_params: &Generics,
    generic_args: &[TokenStream2],
    field_info: &[StructFieldInfo],
    do_chain_hack: bool,
    do_no_doc: bool,
    do_pub_extras: bool,
    make_async: bool,
) -> Result<(TokenStream2, TokenStream2), Error> {
    let visibility = if do_pub_extras {
        struct_visibility.clone()
    } else {
        syn::parse_quote! { pub(super) }
    };
    let documentation = format!(
        concat!(
            "Constructs a new instance of this self-referential struct. (See also ",
            "[`{0}::build()`]({0}::build)). Each argument is a field of ",
            "the new struct. Fields that refer to other fields inside the struct are initialized ",
            "using functions instead of directly passing their value. The arguments are as ",
            "follows:\n\n| Argument | Suggested Use |\n| --- | --- |\n",
        ),
        builder_struct_name.to_string()
    );
    let builder_documentation = concat!(
        "A more verbose but stable way to construct self-referencing structs. It is ",
        "comparable to using `StructName { field1: value1, field2: value2 }` rather than ",
        "`StructName::new(value1, value2)`. This has the dual benefit of making your code ",
        "both easier to refactor and more readable. Call [`build()`](Self::build) to ",
        "construct the actual struct. The fields of this struct should be used as follows:\n\n",
        "| Field | Suggested Use |\n| --- | --- |\n",
    )
    .to_owned();
    let build_fn_documentation = format!(
        concat!(
            "Calls [`{0}::new()`]({0}::new) using the provided values. This is preferrable over ",
            "calling `new()` directly for the reasons listed above. "
        ),
        struct_name.to_string()
    );
    let mut doc_table = "".to_owned();
    let mut code: Vec<TokenStream2> = Vec::new();
    let mut params: Vec<TokenStream2> = Vec::new();
    let mut builder_struct_generic_producers: Vec<_> = generic_params
        .params
        .iter()
        .map(|param| quote! { #param })
        .collect();
    let mut builder_struct_generic_consumers = Vec::from(generic_args);
    let mut builder_struct_fields = Vec::new();
    let mut builder_struct_field_names = Vec::new();

    code.push(quote! { let mut result = ::core::mem::MaybeUninit::<Self>::uninit(); });

    for field in field_info {
        let field_name = &field.name;

        let arg_type = make_constructor_arg_type(
            &field,
            &field_info[..],
            fake_lifetime,
            do_chain_hack,
            make_async,
        )?;
        if let ArgType::Plain(plain_type) = arg_type {
            // No fancy builder function, we can just move the value directly into the struct.
            params.push(quote! { #field_name: #plain_type });
            builder_struct_fields.push(quote! { #field_name: #plain_type });
            builder_struct_field_names.push(quote! { #field_name });
            doc_table += &format!(
                "| `{}` | Directly pass in the value this field should contain |\n",
                field_name.to_string()
            );
        } else if let ArgType::TraitBound(bound_type) = arg_type {
            // Trait bounds are much trickier. We need a special syntax to accept them in the
            // contructor, and generic parameters need to be added to the builder struct to make
            // it work.
            let builder_name = field.builder_name();
            params.push(quote! { #builder_name : impl #bound_type });
            // Ok so hear me out basically without this thing here my IDE thinks the rest of the
            // code is a string and it all turns green.
            {}
            doc_table += &format!(
                "| `{}` | Use a function or closure: `(",
                builder_name.to_string()
            );
            let mut builder_args = Vec::new();
            for (index, borrow) in field.borrows.iter().enumerate() {
                let borrowed_name = &field_info[borrow.index].name;
                builder_args.push(format_ident!("{}_illegal_static_reference", borrowed_name));
                doc_table += &format!(
                    "{}: &{}_",
                    borrowed_name.to_string(),
                    if borrow.mutable { "mut " } else { "" },
                );
                if index < field.borrows.len() - 1 {
                    doc_table += ", ";
                }
            }
            doc_table += &format!(") -> {}: _` | \n", field_name.to_string());
            if make_async {
                code.push(quote! { let #field_name = #builder_name (#(#builder_args),*).await; });
            } else {
                code.push(quote! { let #field_name = #builder_name (#(#builder_args),*); });
            }
            let generic_type_name =
                format_ident!("{}Builder_", to_class_case(field_name.to_string().as_str()));

            builder_struct_generic_producers.push(quote! { #generic_type_name: #bound_type });
            builder_struct_generic_consumers.push(quote! { #generic_type_name });
            builder_struct_fields.push(quote! { #builder_name: #generic_type_name });
            builder_struct_field_names.push(quote! { #builder_name });
        }
        let field_type = &field.typ;
        let field_type = replace_this_with_lifetime(quote! { #field_type }, fake_lifetime.clone());
        code.push(quote! { unsafe {
            ((&mut (*result.as_mut_ptr()).#field_name) as *mut #field_type).write(#field_name);
        }});

        if field.field_type == FieldType::Borrowed {
            code.push(field.make_illegal_static_reference());
        } else if field.field_type == FieldType::BorrowedMut {
            code.push(field.make_illegal_static_mut_reference());
        }
    }

    let documentation = if !do_no_doc {
        let documentation = documentation + &doc_table;
        quote! {
            #[doc=#documentation]
        }
    } else {
        quote! { #[doc(hidden)] }
    };

    let builder_documentation = if !do_no_doc {
        let builder_documentation = builder_documentation + &doc_table;
        quote! {
            #[doc=#builder_documentation]
        }
    } else {
        quote! { #[doc(hidden)] }
    };

    let constructor_fn = if make_async {
        quote! { async fn new_async }
    } else {
        quote! { fn new }
    };
    let constructor_def = quote! {
        #documentation
        #visibility #constructor_fn(#(#params),*) -> #struct_name <#(#generic_args),*> {
            #(#code)*
            unsafe { result.assume_init() }
        }
    };
    let generic_where = &generic_params.where_clause;
    let builder_fn = if make_async {
        quote! { async fn build }
    } else {
        quote! { fn build }
    };
    let builder_code = if make_async {
        quote! {
            #struct_name::new_async(
                #(self.#builder_struct_field_names),*
            ).await
        }
    } else {
        quote! {
            #struct_name::new(
                #(self.#builder_struct_field_names),*
            )
        }
    };
    let builder_def = quote! {
        #builder_documentation
        #visibility struct #builder_struct_name <#(#builder_struct_generic_producers),*> #generic_where {
            #(#visibility #builder_struct_fields),*
        }
        impl<#(#builder_struct_generic_producers),*> #builder_struct_name <#(#builder_struct_generic_consumers),*> #generic_where {
            #[doc=#build_fn_documentation]
            #visibility #builder_fn(self) -> #struct_name <#(#generic_args),*> {
                #builder_code
            }
        }
    };
    Ok((builder_def, constructor_def))
}

fn create_try_builder_and_constructor(
    struct_visibility: &Visibility,
    struct_name: &Ident,
    builder_struct_name: &Ident,
    fake_lifetime: &Ident,
    generic_params: &Generics,
    generic_args: &[TokenStream2],
    field_info: &[StructFieldInfo],
    do_chain_hack: bool,
    do_no_doc: bool,
    do_pub_extras: bool,
    make_async: bool,
) -> Result<(TokenStream2, TokenStream2), Error> {
    let visibility = if do_pub_extras {
        struct_visibility.clone()
    } else {
        syn::parse_quote! { pub(super) }
    };
    let mut head_recover_code = Vec::new();
    for field in field_info {
        if !field.self_referencing {
            let field_name = &field.name;
            head_recover_code.push(quote! { #field_name });
        }
    }
    for (_ty, ident) in make_template_consumers(generic_params) {
        head_recover_code.push(quote! { #ident: ::core::marker::PhantomData });
    }
    let mut current_head_index = 0;

    let documentation = format!(
        concat!(
            "(See also [`{0}::try_build()`]({0}::try_build).) Like [`new`](Self::new), but ",
            "builders for [self-referencing fields](https://docs.rs/ouroboros/latest/ouroboros/attr.self_referencing.html#definitions) ",
            "can return results. If any of them fail, `Err` is returned. If all of them ",
            "succeed, `Ok` is returned. The arguments are as follows:\n\n",
            "| Argument | Suggested Use |\n| --- | --- |\n",
        ),
        builder_struct_name.to_string()
    );
    let or_recover_documentation = format!(
        concat!(
            "(See also [`{0}::try_build_or_recover()`]({0}::try_build_or_recover).) Like ",
            "[`try_new`](Self::try_new), but all ",
            "[head fields](https://docs.rs/ouroboros/latest/ouroboros/attr.self_referencing.html#definitions) ",
            "are returned in the case of an error. The arguments are as follows:\n\n",
            "| Argument | Suggested Use |\n| --- | --- |\n",
        ),
        builder_struct_name.to_string()
    );
    let builder_documentation = concat!(
        "A more verbose but stable way to construct self-referencing structs. It is ",
        "comparable to using `StructName { field1: value1, field2: value2 }` rather than ",
        "`StructName::new(value1, value2)`. This has the dual benefit of makin your code ",
        "both easier to refactor and more readable. Call [`try_build()`](Self::try_build) or ",
        "[`try_build_or_recover()`](Self::try_build_or_recover) to ",
        "construct the actual struct. The fields of this struct should be used as follows:\n\n",
        "| Field | Suggested Use |\n| --- | --- |\n",
    )
    .to_owned();
    let build_fn_documentation = format!(
        concat!(
            "Calls [`{0}::try_new()`]({0}::try_new) using the provided values. This is ",
            "preferrable over calling `try_new()` directly for the reasons listed above. "
        ),
        struct_name.to_string()
    );
    let build_or_recover_fn_documentation = format!(
        concat!(
            "Calls [`{0}::try_new_or_recover()`]({0}::try_new_or_recover) using the provided ",
            "values. This is preferrable over calling `try_new_or_recover()` directly for the ",
            "reasons listed above. "
        ),
        struct_name.to_string()
    );
    let mut doc_table = "".to_owned();
    let mut or_recover_code: Vec<TokenStream2> = Vec::new();
    let mut params: Vec<TokenStream2> = Vec::new();
    let mut builder_struct_generic_producers: Vec<_> = generic_params
        .params
        .iter()
        .map(|param| quote! { #param })
        .collect();
    let mut builder_struct_generic_consumers = Vec::from(generic_args);
    let mut builder_struct_fields = Vec::new();
    let mut builder_struct_field_names = Vec::new();

    or_recover_code.push(quote! { let mut result = ::core::mem::MaybeUninit::<Self>::uninit(); });

    for field in field_info {
        let field_name = &field.name;

        let arg_type = make_try_constructor_arg_type(
            &field,
            &field_info[..],
            fake_lifetime,
            do_chain_hack,
            make_async,
        )?;
        if let ArgType::Plain(plain_type) = arg_type {
            // No fancy builder function, we can just move the value directly into the struct.
            params.push(quote! { #field_name: #plain_type });
            builder_struct_fields.push(quote! { #field_name: #plain_type });
            builder_struct_field_names.push(quote! { #field_name });
            doc_table += &format!(
                "| `{}` | Directly pass in the value this field should contain |\n",
                field_name.to_string()
            );
            if !field.self_referencing {
                head_recover_code[current_head_index] = quote! {
                    #field_name: unsafe { ::core::ptr::read(&(*result.as_ptr()).#field_name as *const _) }
                };
                current_head_index += 1;
            }
        } else if let ArgType::TraitBound(bound_type) = arg_type {
            // Trait bounds are much trickier. We need a special syntax to accept them in the
            // contructor, and generic parameters need to be added to the builder struct to make
            // it work.
            let builder_name = field.builder_name();
            params.push(quote! { #builder_name : impl #bound_type });
            // Ok so hear me out basically without this thing here my IDE thinks the rest of the
            // code is a string and it all turns green.
            {}
            doc_table += &format!(
                "| `{}` | Use a function or closure: `(",
                builder_name.to_string()
            );
            let mut builder_args = Vec::new();
            for (index, borrow) in field.borrows.iter().enumerate() {
                let borrowed_name = &field_info[borrow.index].name;
                builder_args.push(format_ident!("{}_illegal_static_reference", borrowed_name));
                doc_table += &format!(
                    "{}: &{}_",
                    borrowed_name.to_string(),
                    if borrow.mutable { "mut " } else { "" },
                );
                if index < field.borrows.len() - 1 {
                    doc_table += ", ";
                }
            }
            doc_table += &format!(") -> Result<{}: _, Error_>` | \n", field_name.to_string());
            let builder_value = if make_async {
                quote! { #builder_name (#(#builder_args),*).await }
            } else {
                quote! { #builder_name (#(#builder_args),*) }
            };
            or_recover_code.push(quote! {
                let #field_name = match #builder_value {
                    ::core::result::Result::Ok(value) => value,
                    ::core::result::Result::Err(err)
                        => return ::core::result::Result::Err((err, Heads { #(#head_recover_code),* })),
                };
            });
            let generic_type_name =
                format_ident!("{}Builder_", to_class_case(field_name.to_string().as_str()));

            builder_struct_generic_producers.push(quote! { #generic_type_name: #bound_type });
            builder_struct_generic_consumers.push(quote! { #generic_type_name });
            builder_struct_fields.push(quote! { #builder_name: #generic_type_name });
            builder_struct_field_names.push(quote! { #builder_name });
        }
        let field_type = &field.typ;
        let field_type = replace_this_with_lifetime(quote! { #field_type }, fake_lifetime.clone());
        let line = quote! { unsafe {
            ((&mut (*result.as_mut_ptr()).#field_name) as *mut #field_type).write(#field_name);
        }};
        or_recover_code.push(line);

        if field.field_type == FieldType::Borrowed {
            or_recover_code.push(field.make_illegal_static_reference());
        } else if field.field_type == FieldType::BorrowedMut {
            or_recover_code.push(field.make_illegal_static_mut_reference());
        }
    }
    let documentation = if !do_no_doc {
        let documentation = documentation + &doc_table;
        quote! {
            #[doc=#documentation]
        }
    } else {
        quote! { #[doc(hidden)] }
    };
    let or_recover_documentation = if !do_no_doc {
        let or_recover_documentation = or_recover_documentation + &doc_table;
        quote! {
            #[doc=#or_recover_documentation]
        }
    } else {
        quote! { #[doc(hidden)] }
    };
    let builder_documentation = if !do_no_doc {
        let builder_documentation = builder_documentation + &doc_table;
        quote! {
            #[doc=#builder_documentation]
        }
    } else {
        quote! { #[doc(hidden)] }
    };
    let or_recover_ident = if make_async {
        quote! { try_new_or_recover_async }
    } else {
        quote! { try_new_or_recover }
    };
    let or_recover_constructor_fn = if make_async {
        quote! { async fn #or_recover_ident }
    } else {
        quote! { fn #or_recover_ident }
    };
    let constructor_fn = if make_async {
        quote! { async fn try_new_async }
    } else {
        quote! { fn try_new }
    };
    let constructor_code = if make_async {
        quote! { #struct_name::#or_recover_ident(#(#builder_struct_field_names),*).await.map_err(|(error, _heads)| error) }
    } else {
        quote! { #struct_name::#or_recover_ident(#(#builder_struct_field_names),*).map_err(|(error, _heads)| error) }
    };
    let constructor_def = quote! {
        #documentation
        #visibility #constructor_fn<Error_>(#(#params),*) -> ::core::result::Result<#struct_name <#(#generic_args),*>, Error_> {
            #constructor_code
        }
        #or_recover_documentation
        #visibility #or_recover_constructor_fn<Error_>(#(#params),*) -> ::core::result::Result<#struct_name <#(#generic_args),*>, (Error_, Heads<#(#generic_args),*>)> {
            #(#or_recover_code)*
            ::core::result::Result::Ok(unsafe { result.assume_init() })
        }
    };
    builder_struct_generic_producers.push(quote! { Error_ });
    builder_struct_generic_consumers.push(quote! { Error_ });
    let generic_where = &generic_params.where_clause;
    let builder_fn = if make_async {
        quote! { async fn try_build }
    } else {
        quote! { fn try_build }
    };
    let or_recover_builder_fn = if make_async {
        quote! { async fn try_build_or_recover }
    } else {
        quote! { fn try_build_or_recover }
    };
    let builder_code = if make_async {
        quote! {
            #struct_name::try_new_async(
                #(self.#builder_struct_field_names),*
            ).await
        }
    } else {
        quote! {
            #struct_name::try_new(
                #(self.#builder_struct_field_names),*
            )
        }
    };
    let or_recover_builder_code = if make_async {
        quote! {
            #struct_name::try_new_or_recover_async(
                #(self.#builder_struct_field_names),*
            ).await
        }
    } else {
        quote! {
            #struct_name::try_new_or_recover(
                #(self.#builder_struct_field_names),*
            )
        }
    };
    let builder_def = quote! {
        #builder_documentation
        #visibility struct #builder_struct_name <#(#builder_struct_generic_producers),*> #generic_where {
            #(#visibility #builder_struct_fields),*
        }
        impl<#(#builder_struct_generic_producers),*> #builder_struct_name <#(#builder_struct_generic_consumers),*> #generic_where {
            #[doc=#build_fn_documentation]
            #visibility #builder_fn(self) -> ::core::result::Result<#struct_name <#(#generic_args),*>, Error_> {
                #builder_code
            }
            #[doc=#build_or_recover_fn_documentation]
            #visibility #or_recover_builder_fn(self) -> ::core::result::Result<#struct_name <#(#generic_args),*>, (Error_, Heads<#(#generic_args),*>)> {
                #or_recover_builder_code
            }
        }
    };
    Ok((builder_def, constructor_def))
}

fn make_with_functions(
    field_info: &[StructFieldInfo],
    do_no_doc: bool,
) -> Result<Vec<TokenStream2>, Error> {
    let mut users = Vec::new();
    for field in field_info {
        let visibility = &field.vis;
        let field_name = &field.name;
        let field_type = &field.typ;
        // If the field is not a tail, we need to serve up the same kind of reference that other
        // fields in the struct may have borrowed to ensure safety.
        if field.field_type == FieldType::Tail {
            let user_name = format_ident!("with_{}", &field.name);
            let documentation = format!(
                concat!(
                    "Provides an immutable reference to `{0}`. This method was generated because ",
                    "`{0}` is a [tail field](https://docs.rs/ouroboros/latest/ouroboros/attr.self_referencing.html#definitions)."
                ),
                field.name.to_string()
            );
            let documentation = if !do_no_doc {
                quote! {
                    #[doc=#documentation]
                }
            } else {
                quote! { #[doc(hidden)] }
            };
            users.push(quote! {
                #documentation
                #visibility fn #user_name <'outer_borrow, ReturnType>(
                    &'outer_borrow self,
                    user: impl for<'this> ::core::ops::FnOnce(&'outer_borrow #field_type) -> ReturnType,
                ) -> ReturnType {
                    user(&self. #field_name)
                }
            });
            if field.covariant == Some(true) {
                let borrower_name = format_ident!("borrow_{}", &field.name);
                users.push(quote! {
                    #documentation
                    #visibility fn #borrower_name<'this>(
                        &'this self,
                    ) -> &'this #field_type {
                        &self.#field_name
                    }
                });
            } else if field.covariant.is_none() {
                field.covariance_error();
            }
            // If it is not borrowed at all it's safe to allow mutably borrowing it.
            let user_name = format_ident!("with_{}_mut", &field.name);
            let documentation = format!(
                concat!(
                    "Provides a mutable reference to `{0}`. This method was generated because ",
                    "`{0}` is a [tail field](https://docs.rs/ouroboros/latest/ouroboros/attr.self_referencing.html#definitions). ",
                    "No `borrow_{0}_mut` function was generated because Rust's borrow checker is ",
                    "currently unable to guarantee that such a method would be used safely."
                ),
                field.name.to_string()
            );
            let documentation = if !do_no_doc {
                quote! {
                    #[doc=#documentation]
                }
            } else {
                quote! { #[doc(hidden)] }
            };
            users.push(quote! {
                #documentation
                #visibility fn #user_name <'outer_borrow, ReturnType>(
                    &'outer_borrow mut self,
                    user: impl for<'this> ::core::ops::FnOnce(&'outer_borrow mut #field_type) -> ReturnType,
                ) -> ReturnType {
                    user(&mut self. #field_name)
                }
            });
        } else if field.field_type == FieldType::Borrowed {
            let user_name = format_ident!("with_{}", &field.name);
            let documentation = format!(
                concat!(
                    "Provides limited immutable access to `{0}`. This method was generated ",
                    "because the contents of `{0}` are immutably borrowed by other fields."
                ),
                field.name.to_string()
            );
            let documentation = if !do_no_doc {
                quote! {
                    #[doc=#documentation]
                }
            } else {
                quote! { #[doc(hidden)] }
            };
            users.push(quote! {
                #documentation
                #visibility fn #user_name <'outer_borrow, ReturnType>(
                    &'outer_borrow self,
                    user: impl for<'this> ::core::ops::FnOnce(&'outer_borrow #field_type) -> ReturnType,
                ) -> ReturnType {
                    user(&self.#field_name)
                }
            });
            if field.self_referencing {
                if field.covariant == Some(false) {
                    // Skip the other functions, they will cause compiler errors.
                    continue;
                } else if field.covariant.is_none() {
                    field.covariance_error();
                }
            }
            let borrower_name = format_ident!("borrow_{}", &field.name);
            users.push(quote! {
                #documentation
                #visibility fn #borrower_name<'this>(
                    &'this self,
                ) -> &'this #field_type {
                    &self.#field_name
                }
            });
        } else if field.field_type == FieldType::BorrowedMut {
            // Do not generate anything becaue if it is borrowed mutably once, we should not be able
            // to get any other kinds of references to it.
        }
    }
    Ok(users)
}

fn make_with_all_function(
    struct_visibility: &syn::Visibility,
    struct_name: &Ident,
    fake_lifetime: &Ident,
    field_info: &[StructFieldInfo],
    generic_params: &Generics,
    generic_args: &[TokenStream2],
    do_no_doc: bool,
    do_pub_extras: bool,
) -> Result<(TokenStream2, TokenStream2), Error> {
    let visibility = if do_pub_extras {
        struct_visibility.clone()
    } else {
        syn::parse_quote! { pub(super) }
    };
    let mut fields = Vec::new();
    let mut field_assignments = Vec::new();
    let mut mut_fields = Vec::new();
    let mut mut_field_assignments = Vec::new();
    // I don't think the reverse is necessary but it does make the expanded code more uniform.
    for field in field_info.iter().rev() {
        let field_name = &field.name;
        let field_type = &field.typ;
        if field.field_type == FieldType::Tail {
            fields.push(quote! { #visibility #field_name: &'outer_borrow #field_type });
            field_assignments.push(quote! { #field_name: &self.#field_name });
            mut_fields.push(quote! { #visibility #field_name: &'outer_borrow mut #field_type });
            mut_field_assignments.push(quote! { #field_name: &mut self.#field_name });
        } else if field.field_type == FieldType::Borrowed {
            let ass = quote! { #field_name: unsafe {
                ::ouroboros::macro_help::stable_deref_and_change_lifetime(
                    &self.#field_name
                )
            } };
            let deref_type = quote! { <#field_type as ::std::ops::Deref>::Target };
            fields.push(quote! { #visibility #field_name: &'this #deref_type });
            field_assignments.push(ass.clone());
            mut_fields.push(quote! { #visibility #field_name: &'this #deref_type });
            mut_field_assignments.push(ass);
        } else if field.field_type == FieldType::BorrowedMut {
            // Add nothing because we cannot borrow something that has already been mutably
            // borrowed.
        }
    }

    let new_generic_params = if generic_params.params.is_empty() {
        quote! { <'outer_borrow, 'this> }
    } else {
        for (ty, ident) in make_template_consumers(generic_params) {
            fields.push(quote! { #ident: ::core::marker::PhantomData<#ty> });
            mut_fields.push(quote! { #ident: ::core::marker::PhantomData<#ty> });
            field_assignments.push(quote! { #ident: ::core::marker::PhantomData });
            mut_field_assignments.push(quote! { #ident: ::core::marker::PhantomData });
        }
        let mut new_generic_params = generic_params.clone();
        new_generic_params
            .params
            .insert(0, syn::parse_quote! { 'this });
        new_generic_params
            .params
            .insert(0, syn::parse_quote! { 'outer_borrow });
        quote! { #new_generic_params }
    };
    let new_generic_args = {
        let mut args = Vec::from(generic_args);
        args.insert(0, quote! { 'this });
        args.insert(0, quote! { 'outer_borrow });
        args
    };

    let struct_documentation = format!(
        concat!(
            "A struct for holding immutable references to all ",
            "[tail and immutably borrowed fields](https://docs.rs/ouroboros/latest/ouroboros/attr.self_referencing.html#definitions) in an instance of ",
            "[`{0}`]({0})."
        ),
        struct_name.to_string()
    );
    let mut_struct_documentation = format!(
        concat!(
            "A struct for holding mutable references to all ",
            "[tail fields](https://docs.rs/ouroboros/latest/ouroboros/attr.self_referencing.html#definitions) in an instance of ",
            "[`{0}`]({0})."
        ),
        struct_name.to_string()
    );
    let ltname = format!("'{}", fake_lifetime);
    let lifetime = Lifetime::new(&ltname, Span::call_site());
    let generic_where = if let Some(clause) = &generic_params.where_clause {
        let mut clause = clause.clone();
        let extra: WhereClause = syn::parse_quote! { where #lifetime: 'this };
        clause
            .predicates
            .push(extra.predicates.first().unwrap().clone());
        clause
    } else {
        syn::parse_quote! { where #lifetime: 'this }
    };
    let struct_defs = quote! {
        #[doc=#struct_documentation]
        #visibility struct BorrowedFields #new_generic_params #generic_where { #(#fields),* }
        #[doc=#mut_struct_documentation]
        #visibility struct BorrowedMutFields #new_generic_params #generic_where { #(#mut_fields),* }
    };
    let borrowed_fields_type = quote! { BorrowedFields<#(#new_generic_args),*> };
    let borrowed_mut_fields_type = quote! { BorrowedMutFields<#(#new_generic_args),*> };
    let documentation = concat!(
        "This method provides immutable references to all ",
        "[tail and immutably borrowed fields](https://docs.rs/ouroboros/latest/ouroboros/attr.self_referencing.html#definitions).",
    );
    let mut_documentation = concat!(
        "This method provides mutable references to all ",
        "[tail fields](https://docs.rs/ouroboros/latest/ouroboros/attr.self_referencing.html#definitions).",
    );
    let documentation = if !do_no_doc {
        quote! {
            #[doc=#documentation]
        }
    } else {
        quote! { #[doc(hidden)] }
    };
    let mut_documentation = if !do_no_doc {
        quote! {
            #[doc=#mut_documentation]
        }
    } else {
        quote! { #[doc(hidden)] }
    };
    let fn_defs = quote! {
        #documentation
        #visibility fn with <'outer_borrow, ReturnType>(
            &'outer_borrow self,
            user: impl for<'this> ::core::ops::FnOnce(#borrowed_fields_type) -> ReturnType
        ) -> ReturnType {
            user(BorrowedFields {
                #(#field_assignments),*
            })
        }
        #mut_documentation
        #visibility fn with_mut <'outer_borrow, ReturnType>(
            &'outer_borrow mut self,
            user: impl for<'this> ::core::ops::FnOnce(#borrowed_mut_fields_type) -> ReturnType
        ) -> ReturnType {
            user(BorrowedMutFields {
                #(#mut_field_assignments),*
            })
        }
    };
    Ok((struct_defs, fn_defs))
}

/// Returns the Heads struct and a function to convert the original struct into a Heads instance.
fn make_into_heads(
    struct_visibility: &Visibility,
    struct_name: &Ident,
    field_info: &[StructFieldInfo],
    generic_params: &Generics,
    generic_args: &[TokenStream2],
    do_no_doc: bool,
    do_pub_extras: bool,
) -> (TokenStream2, TokenStream2) {
    let visibility = if do_pub_extras {
        struct_visibility.clone()
    } else {
        syn::parse_quote! { pub(super) }
    };
    let mut code = Vec::new();
    let mut field_initializers = Vec::new();
    let mut head_fields = Vec::new();
    // Drop everything in the reverse order of what it was declared in. Fields that come later
    // are only dependent on fields that came before them.
    for field in field_info.iter().rev() {
        let field_name = &field.name;
        if !field.self_referencing {
            code.push(quote! { let #field_name = self.#field_name; });
            field_initializers.push(quote! { #field_name });
            let field_type = &field.typ;
            head_fields.push(quote! { #visibility #field_name: #field_type });
        } else {
            // Heads are fields that do not borrow anything.
            code.push(quote! { ::core::mem::drop(self.#field_name); });
        }
    }
    for (ty, ident) in make_template_consumers(generic_params) {
        head_fields.push(quote! { #ident: ::core::marker::PhantomData<#ty> });
        field_initializers.push(quote! { #ident: ::core::marker::PhantomData });
    }
    let documentation = format!(
        concat!(
            "A struct which contains only the ",
            "[head fields](https://docs.rs/ouroboros/latest/ouroboros/attr.self_referencing.html#definitions) of [`{0}`]({0})."
        ),
        struct_name.to_string()
    );
    let generic_where = &generic_params.where_clause;
    let heads_struct_def = quote! {
        #[doc=#documentation]
        #visibility struct Heads #generic_params #generic_where {
            #(#head_fields),*
        }
    };
    let documentation = concat!(
        "This function drops all internally referencing fields and returns only the ",
        "[head fields](https://docs.rs/ouroboros/latest/ouroboros/attr.self_referencing.html#definitions) of this struct."
    ).to_owned();

    let documentation = if !do_no_doc {
        quote! {
            #[doc=#documentation]
        }
    } else {
        quote! { #[doc(hidden)] }
    };

    let into_heads_fn = quote! {
        #documentation
        #[allow(clippy::drop_ref)]
        #[allow(clippy::drop_copy)]
        #visibility fn into_heads(self) -> Heads<#(#generic_args),*> {
            #(#code)*
            Heads {
                #(#field_initializers),*
            }
        }
    };
    (heads_struct_def, into_heads_fn)
}

fn make_type_asserts(
    field_info: &[StructFieldInfo],
    generic_params: &Generics,
    _generic_args: &[TokenStream2],
) -> TokenStream2 {
    let generic_where = &generic_params.where_clause;
    let mut checks = Vec::new();
    for field in field_info {
        let field_type = &field.typ;
        if let Some((std_type, _eltype)) = apparent_std_container_type(field_type) {
            let checker_name = match std_type {
                "Box" => "is_std_box_type",
                "Arc" => "is_std_arc_type",
                "Rc" => "is_std_rc_type",
                _ => unreachable!(),
            };
            let checker_name = format_ident!("{}", checker_name);
            let static_field_type =
                replace_this_with_lifetime(quote! { #field_type }, format_ident!("static"));
            checks.push(quote! {
                ::ouroboros::macro_help::CheckIfTypeIsStd::<#static_field_type>::#checker_name();
            });
        }
    }
    quote! {
        fn type_asserts #generic_params() #generic_where {
            #(#checks)*
        }
    }
}

fn submodule_contents_visiblity(original_visibility: &Visibility) -> Visibility {
    match original_visibility {
        // inherited: allow parent of inner submodule to see
        Visibility::Inherited => syn::parse_quote! { pub(super) },
        // restricted: add an extra super if needed
        Visibility::Restricted(ref restricted) => {
            let is_first_component_super = restricted
                .path
                .segments
                .first()
                .map(|segm| segm.ident == "super")
                .unwrap_or(false);
            if restricted.path.leading_colon.is_none() && is_first_component_super {
                let mut new_visibility = restricted.clone();
                new_visibility.in_token = Some(
                    restricted
                        .in_token
                        .clone()
                        .unwrap_or_else(|| syn::parse_quote! { in }),
                );
                new_visibility.path.segments = std::iter::once(syn::parse_quote! { super })
                    .chain(restricted.path.segments.iter().cloned())
                    .collect();
                Visibility::Restricted(new_visibility)
            } else {
                original_visibility.clone()
            }
        }
        // others are absolute, can use them as-is
        _ => original_visibility.clone(),
    }
}

fn self_referencing_impl(
    original_struct_def: ItemStruct,
    do_chain_hack: bool,
    do_no_doc: bool,
    do_pub_extras: bool,
) -> Result<TokenStream, Error> {
    let struct_name = &original_struct_def.ident;
    let mod_name = format_ident!("ouroboros_impl_{}", struct_name.to_string().to_snake_case());
    let visibility = &original_struct_def.vis;
    let submodule_contents_visiblity = submodule_contents_visiblity(visibility);

    let (actual_struct_def, fake_lifetime, field_info) =
        create_actual_struct(&submodule_contents_visiblity, &original_struct_def)?;

    let generic_params = original_struct_def.generics.clone();
    let generic_args = make_generic_arguments(&generic_params);

    let builder_struct_name = format_ident!("{}Builder", struct_name);
    let (builder_def, constructor_def) = create_builder_and_constructor(
        &submodule_contents_visiblity,
        &struct_name,
        &builder_struct_name,
        &fake_lifetime,
        &generic_params,
        &generic_args,
        &field_info[..],
        do_chain_hack,
        do_no_doc,
        do_pub_extras,
        false,
    )?;
    let async_builder_struct_name = format_ident!("{}AsyncBuilder", struct_name);
    let (async_builder_def, async_constructor_def) = create_builder_and_constructor(
        &submodule_contents_visiblity,
        &struct_name,
        &async_builder_struct_name,
        &fake_lifetime,
        &generic_params,
        &generic_args,
        &field_info[..],
        do_chain_hack,
        do_no_doc,
        do_pub_extras,
        true,
    )?;
    let try_builder_struct_name = format_ident!("{}TryBuilder", struct_name);
    let (try_builder_def, try_constructor_def) = create_try_builder_and_constructor(
        &submodule_contents_visiblity,
        &struct_name,
        &try_builder_struct_name,
        &fake_lifetime,
        &generic_params,
        &generic_args,
        &field_info[..],
        do_chain_hack,
        do_no_doc,
        do_pub_extras,
        false,
    )?;
    let async_try_builder_struct_name = format_ident!("{}AsyncTryBuilder", struct_name);
    let (async_try_builder_def, async_try_constructor_def) = create_try_builder_and_constructor(
        &submodule_contents_visiblity,
        &struct_name,
        &async_try_builder_struct_name,
        &fake_lifetime,
        &generic_params,
        &generic_args,
        &field_info[..],
        do_chain_hack,
        do_no_doc,
        do_pub_extras,
        true,
    )?;

    let users = make_with_functions(&field_info[..], do_no_doc)?;
    let (with_all_struct_defs, with_all_fn_defs) = make_with_all_function(
        &submodule_contents_visiblity,
        struct_name,
        &fake_lifetime,
        &field_info[..],
        &generic_params,
        &generic_args,
        do_no_doc,
        do_pub_extras,
    )?;
    let (heads_struct_def, into_heads_fn) = make_into_heads(
        &submodule_contents_visiblity,
        struct_name,
        &field_info[..],
        &generic_params,
        &generic_args,
        do_no_doc,
        do_pub_extras,
    );
    // These check that types like Box, Arc, and Rc refer to those types in the std lib and have not
    // been overridden.
    let type_asserts_def = make_type_asserts(&field_info[..], &generic_params, &generic_args);

    let extra_visibility = if do_pub_extras {
        visibility.clone()
    } else {
        syn::Visibility::Inherited
    };

    let generic_where = &generic_params.where_clause;
    Ok(TokenStream::from(quote! {
        #[doc="Encapsulates implementation details for a self-referencing struct. This module is only visible when using --document-private-items."]
        mod #mod_name {
            use super::*;
            #actual_struct_def
            #builder_def
            #async_builder_def
            #try_builder_def
            #async_try_builder_def
            #with_all_struct_defs
            #heads_struct_def
            impl #generic_params #struct_name <#(#generic_args),*> #generic_where {
                #constructor_def
                #async_constructor_def
                #try_constructor_def
                #async_try_constructor_def
                #(#users)*
                #with_all_fn_defs
                #into_heads_fn
            }
            #type_asserts_def
        }
        #visibility use #mod_name :: #struct_name;
        #extra_visibility use #mod_name :: #builder_struct_name;
        #extra_visibility use #mod_name :: #async_builder_struct_name;
        #extra_visibility use #mod_name :: #try_builder_struct_name;
        #extra_visibility use #mod_name :: #async_try_builder_struct_name;
    }))
}

#[proc_macro_error]
#[proc_macro_attribute]
pub fn self_referencing(attr: TokenStream, item: TokenStream) -> TokenStream {
    let mut do_chain_hack = false;
    let mut do_no_doc = false;
    let mut do_pub_extras = false;
    let mut expecting_comma = false;
    for token in <TokenStream as std::convert::Into<TokenStream2>>::into(attr).into_iter() {
        if let TokenTree::Ident(ident) = &token {
            if expecting_comma {
                return Error::new(token.span(), "Unexpected identifier, expected comma.")
                    .to_compile_error()
                    .into();
            }
            match &ident.to_string()[..] {
                "chain_hack" => do_chain_hack = true,
                "no_doc" => do_no_doc = true,
                "pub_extras" => do_pub_extras = true,
                _ => {
                    return Error::new_spanned(
                        &ident,
                        "Unknown identifier, expected 'chain_hack', 'no_doc', or 'pub_extras'.",
                    )
                    .to_compile_error()
                    .into()
                }
            }
            expecting_comma = true;
        } else if let TokenTree::Punct(punct) = &token {
            if !expecting_comma {
                return Error::new(token.span(), "Unexpected punctuation, expected identifier.")
                    .to_compile_error()
                    .into();
            }
            if punct.as_char() != ',' {
                return Error::new(token.span(), "Unknown punctuation, expected comma.")
                    .to_compile_error()
                    .into();
            }
            expecting_comma = false;
        } else {
            return Error::new(token.span(), "Unknown syntax, expected identifier.")
                .to_compile_error()
                .into();
        }
    }
    let original_struct_def: ItemStruct = syn::parse_macro_input!(item);
    match self_referencing_impl(original_struct_def, do_chain_hack, do_no_doc, do_pub_extras) {
        Ok(content) => content,
        Err(err) => err.to_compile_error().into(),
    }
}

/// Functionality inspired by `Inflector`, reimplemented here to avoid the
/// `regex` dependency.
fn to_class_case(s: &str) -> String {
    s.split('_')
        .flat_map(|word| {
            let mut chars = word.chars();
            let first = chars.next();
            // Unicode allows for a single character to become multiple characters when converting between cases.
            first
                .into_iter()
                .flat_map(|c| c.to_uppercase())
                .chain(chars.flat_map(|c| c.to_lowercase()))
        })
        .collect()
}
