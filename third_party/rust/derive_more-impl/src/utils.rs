#![cfg_attr(
    not(all(feature = "add", feature = "mul")),
    allow(dead_code),
    allow(unused_mut)
)]

use proc_macro2::TokenStream;
use quote::{format_ident, quote, ToTokens};
use syn::{
    parse_quote, punctuated::Punctuated, spanned::Spanned, Attribute, Data,
    DeriveInput, Error, Field, Fields, FieldsNamed, FieldsUnnamed, GenericParam,
    Generics, Ident, ImplGenerics, Index, Result, Token, Type, TypeGenerics,
    TypeParamBound, Variant, WhereClause,
};

#[cfg(any(feature = "from", feature = "into"))]
pub(crate) use self::{either::Either, fields_ext::FieldsExt};

#[derive(Clone, Copy, Default)]
pub struct DeterministicState;

impl std::hash::BuildHasher for DeterministicState {
    type Hasher = std::collections::hash_map::DefaultHasher;

    fn build_hasher(&self) -> Self::Hasher {
        Self::Hasher::default()
    }
}

pub type HashMap<K, V> = std::collections::HashMap<K, V, DeterministicState>;
pub type HashSet<K> = std::collections::HashSet<K, DeterministicState>;

#[derive(Clone, Copy, Debug, Eq, PartialEq, Hash)]
pub enum RefType {
    No,
    Ref,
    Mut,
}

impl RefType {
    pub fn lifetime(self) -> TokenStream {
        match self {
            RefType::No => quote! {},
            _ => quote! { '__deriveMoreLifetime },
        }
    }

    pub fn reference(self) -> TokenStream {
        match self {
            RefType::No => quote! {},
            RefType::Ref => quote! { & },
            RefType::Mut => quote! { &mut },
        }
    }

    pub fn mutability(self) -> TokenStream {
        match self {
            RefType::Mut => quote! { mut },
            _ => quote! {},
        }
    }

    pub fn pattern_ref(self) -> TokenStream {
        match self {
            RefType::Ref => quote! { ref },
            RefType::Mut => quote! { ref mut },
            RefType::No => quote! {},
        }
    }

    pub fn reference_with_lifetime(self) -> TokenStream {
        if !self.is_ref() {
            return quote! {};
        }
        let lifetime = self.lifetime();
        let mutability = self.mutability();
        quote! { &#lifetime #mutability }
    }

    pub fn is_ref(self) -> bool {
        !matches!(self, RefType::No)
    }

    pub fn from_attr_name(name: &str) -> Self {
        match name {
            "owned" => RefType::No,
            "ref" => RefType::Ref,
            "ref_mut" => RefType::Mut,
            _ => panic!("`{name}` is not a `RefType`"),
        }
    }
}

pub fn numbered_vars(count: usize, prefix: &str) -> Vec<Ident> {
    (0..count).map(|i| format_ident!("__{prefix}{i}")).collect()
}

pub fn field_idents<'a>(fields: &'a [&'a Field]) -> Vec<&'a Ident> {
    fields
        .iter()
        .map(|f| {
            f.ident
                .as_ref()
                .expect("Tried to get field names of a tuple struct")
        })
        .collect()
}

pub fn get_field_types_iter<'a>(
    fields: &'a [&'a Field],
) -> Box<dyn Iterator<Item = &'a Type> + 'a> {
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
        let bound: TypeParamBound = parse_quote! {
            ::core::ops::#trait_ident<Output=#type_ident>
        };
        type_param.bounds.push(bound)
    }

    generics
}

pub fn add_extra_ty_param_bound_op<'a>(
    generics: &'a Generics,
    trait_ident: &'a Ident,
) -> Generics {
    add_extra_ty_param_bound(generics, &quote! { ::core::ops::#trait_ident })
}

pub fn add_extra_ty_param_bound<'a>(
    generics: &'a Generics,
    bound: &'a TokenStream,
) -> Generics {
    let mut generics = generics.clone();
    let bound: TypeParamBound = parse_quote! { #bound };
    for type_param in &mut generics.type_params_mut() {
        type_param.bounds.push(bound.clone())
    }

    generics
}

pub fn add_extra_ty_param_bound_ref<'a>(
    generics: &'a Generics,
    bound: &'a TokenStream,
    ref_type: RefType,
) -> Generics {
    match ref_type {
        RefType::No => add_extra_ty_param_bound(generics, bound),
        _ => {
            let generics = generics.clone();
            let idents = generics.type_params().map(|x| &x.ident);
            let ref_with_lifetime = ref_type.reference_with_lifetime();
            add_extra_where_clauses(
                &generics,
                quote! {
                    where #(#ref_with_lifetime #idents: #bound),*
                },
            )
        }
    }
}

pub fn add_extra_generic_param(
    generics: &Generics,
    generic_param: TokenStream,
) -> Generics {
    let generic_param: GenericParam = parse_quote! { #generic_param };
    let mut generics = generics.clone();
    generics.params.push(generic_param);

    generics
}

pub fn add_extra_generic_type_param(
    generics: &Generics,
    generic_param: TokenStream,
) -> Generics {
    let generic_param: GenericParam = parse_quote! { #generic_param };
    let lifetimes: Vec<GenericParam> =
        generics.lifetimes().map(|x| x.clone().into()).collect();
    let type_params: Vec<GenericParam> =
        generics.type_params().map(|x| x.clone().into()).collect();
    let const_params: Vec<GenericParam> =
        generics.const_params().map(|x| x.clone().into()).collect();
    let mut generics = generics.clone();
    generics.params = Default::default();
    generics.params.extend(lifetimes);
    generics.params.extend(type_params);
    generics.params.push(generic_param);
    generics.params.extend(const_params);

    generics
}

pub fn add_extra_where_clauses(
    generics: &Generics,
    type_where_clauses: TokenStream,
) -> Generics {
    let mut type_where_clauses: WhereClause = parse_quote! { #type_where_clauses };
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
    sized: bool,
) -> Generics {
    let generic_param = if fields.len() > 1 {
        quote! { #type_ident: ::core::marker::Copy }
    } else if sized {
        quote! { #type_ident }
    } else {
        quote! { #type_ident: ?::core::marker::Sized }
    };

    let generics = add_extra_where_clauses(generics, type_where_clauses);
    add_extra_generic_type_param(&generics, generic_param)
}

pub fn unnamed_to_vec(fields: &FieldsUnnamed) -> Vec<&Field> {
    fields.unnamed.iter().collect()
}

pub fn named_to_vec(fields: &FieldsNamed) -> Vec<&Field> {
    fields.named.iter().collect()
}

fn panic_one_field(trait_name: &str, trait_attr: &str) -> ! {
    panic!(
        "derive({trait_name}) only works when forwarding to a single field. \
         Try putting #[{trait_attr}] or #[{trait_attr}(ignore)] on the fields in the struct",
    )
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum DeriveType {
    Unnamed,
    Named,
    Enum,
}

pub struct State<'input> {
    pub input: &'input DeriveInput,
    pub trait_name: &'static str,
    pub trait_ident: Ident,
    pub method_ident: Ident,
    pub trait_path: TokenStream,
    pub trait_path_params: Vec<TokenStream>,
    pub trait_attr: String,
    pub derive_type: DeriveType,
    pub fields: Vec<&'input Field>,
    pub variants: Vec<&'input Variant>,
    pub variant_states: Vec<State<'input>>,
    pub variant: Option<&'input Variant>,
    pub generics: Generics,
    pub default_info: FullMetaInfo,
    full_meta_infos: Vec<FullMetaInfo>,
}

#[derive(Default, Clone)]
pub struct AttrParams {
    pub enum_: Vec<&'static str>,
    pub variant: Vec<&'static str>,
    pub struct_: Vec<&'static str>,
    pub field: Vec<&'static str>,
}

impl AttrParams {
    pub fn new(params: Vec<&'static str>) -> AttrParams {
        AttrParams {
            enum_: params.clone(),
            struct_: params.clone(),
            variant: params.clone(),
            field: params,
        }
    }
    pub fn struct_(params: Vec<&'static str>) -> AttrParams {
        AttrParams {
            enum_: vec![],
            struct_: params,
            variant: vec![],
            field: vec![],
        }
    }

    pub fn ignore_and_forward() -> AttrParams {
        AttrParams::new(vec!["ignore", "forward"])
    }
}

impl<'input> State<'input> {
    pub fn new<'arg_input>(
        input: &'arg_input DeriveInput,
        trait_name: &'static str,
        trait_attr: String,
    ) -> Result<State<'arg_input>> {
        State::new_impl(input, trait_name, trait_attr, AttrParams::default(), true)
    }

    pub fn with_field_ignore<'arg_input>(
        input: &'arg_input DeriveInput,
        trait_name: &'static str,
        trait_attr: String,
    ) -> Result<State<'arg_input>> {
        State::new_impl(
            input,
            trait_name,
            trait_attr,
            AttrParams::new(vec!["ignore"]),
            true,
        )
    }

    pub fn with_field_ignore_and_forward<'arg_input>(
        input: &'arg_input DeriveInput,
        trait_name: &'static str,
        trait_attr: String,
    ) -> Result<State<'arg_input>> {
        State::new_impl(
            input,
            trait_name,
            trait_attr,
            AttrParams::new(vec!["ignore", "forward"]),
            true,
        )
    }

    pub fn with_field_ignore_and_refs<'arg_input>(
        input: &'arg_input DeriveInput,
        trait_name: &'static str,
        trait_attr: String,
    ) -> Result<State<'arg_input>> {
        State::new_impl(
            input,
            trait_name,
            trait_attr,
            AttrParams::new(vec!["ignore", "owned", "ref", "ref_mut"]),
            true,
        )
    }

    pub fn with_attr_params<'arg_input>(
        input: &'arg_input DeriveInput,
        trait_name: &'static str,
        trait_attr: String,
        allowed_attr_params: AttrParams,
    ) -> Result<State<'arg_input>> {
        State::new_impl(input, trait_name, trait_attr, allowed_attr_params, true)
    }

    pub fn with_type_bound<'arg_input>(
        input: &'arg_input DeriveInput,
        trait_name: &'static str,
        trait_attr: String,
        allowed_attr_params: AttrParams,
        add_type_bound: bool,
    ) -> Result<State<'arg_input>> {
        Self::new_impl(
            input,
            trait_name,
            trait_attr,
            allowed_attr_params,
            add_type_bound,
        )
    }

    fn new_impl<'arg_input>(
        input: &'arg_input DeriveInput,
        trait_name: &'static str,
        trait_attr: String,
        allowed_attr_params: AttrParams,
        add_type_bound: bool,
    ) -> Result<State<'arg_input>> {
        let trait_name = trait_name.trim_end_matches("ToInner");
        let trait_ident = format_ident!("{trait_name}");
        let method_ident = format_ident!("{trait_attr}");
        let trait_path = quote! { ::derive_more::#trait_ident };
        let (derive_type, fields, variants): (_, Vec<_>, Vec<_>) = match input.data {
            Data::Struct(ref data_struct) => match data_struct.fields {
                Fields::Unnamed(ref fields) => {
                    (DeriveType::Unnamed, unnamed_to_vec(fields), vec![])
                }

                Fields::Named(ref fields) => {
                    (DeriveType::Named, named_to_vec(fields), vec![])
                }
                Fields::Unit => (DeriveType::Named, vec![], vec![]),
            },
            Data::Enum(ref data_enum) => (
                DeriveType::Enum,
                vec![],
                data_enum.variants.iter().collect(),
            ),
            Data::Union(_) => {
                panic!("cannot derive({trait_name}) for union")
            }
        };
        let attrs: Vec<_> = if derive_type == DeriveType::Enum {
            variants.iter().map(|v| &v.attrs).collect()
        } else {
            fields.iter().map(|f| &f.attrs).collect()
        };

        let (allowed_attr_params_outer, allowed_attr_params_inner) =
            if derive_type == DeriveType::Enum {
                (&allowed_attr_params.enum_, &allowed_attr_params.variant)
            } else {
                (&allowed_attr_params.struct_, &allowed_attr_params.field)
            };

        let struct_meta_info =
            get_meta_info(&trait_attr, &input.attrs, allowed_attr_params_outer)?;
        let meta_infos: Result<Vec<_>> = attrs
            .iter()
            .map(|attrs| get_meta_info(&trait_attr, attrs, allowed_attr_params_inner))
            .collect();
        let meta_infos = meta_infos?;
        let first_match = meta_infos
            .iter()
            .filter_map(|info| info.enabled.map(|_| info))
            .next();

        // Default to enabled true, except when first attribute has explicit
        // enabling.
        //
        // Except for derive Error.
        //
        // The way `else` case works is that if any field have any valid
        // attribute specified, then all fields without any attributes
        // specified are filtered out from `State::enabled_fields`.
        //
        // However, derive Error *infers* fields and there are cases when
        // one of the fields may have an attribute specified, but another field
        // would be inferred. So, for derive Error macro we default enabled
        // to true unconditionally (i.e., even if some fields have attributes
        // specified).
        let default_enabled = if trait_name == "Error" {
            true
        } else {
            first_match.map_or(true, |info| !info.enabled.unwrap())
        };

        let defaults = struct_meta_info.into_full(FullMetaInfo {
            enabled: default_enabled,
            forward: false,
            // Default to owned true, except when first attribute has one of owned,
            // ref or ref_mut
            // - not a single attribute means default true
            // - an attribute, but non of owned, ref or ref_mut means default true
            // - an attribute, and owned, ref or ref_mut means default false
            owned: first_match.map_or(true, |info| {
                info.owned.is_none() && info.ref_.is_none() || info.ref_mut.is_none()
            }),
            ref_: false,
            ref_mut: false,
            info: MetaInfo::default(),
        });

        let full_meta_infos: Vec<_> = meta_infos
            .into_iter()
            .map(|info| info.into_full(defaults.clone()))
            .collect();

        let variant_states: Result<Vec<_>> = if derive_type == DeriveType::Enum {
            variants
                .iter()
                .zip(full_meta_infos.iter().cloned())
                .map(|(variant, info)| {
                    State::from_variant(
                        input,
                        trait_name,
                        trait_attr.clone(),
                        allowed_attr_params.clone(),
                        variant,
                        info,
                    )
                })
                .collect()
        } else {
            Ok(vec![])
        };

        let generics = if add_type_bound {
            add_extra_ty_param_bound(&input.generics, &trait_path)
        } else {
            input.generics.clone()
        };

        Ok(State {
            input,
            trait_name,
            trait_ident,
            method_ident,
            trait_path,
            trait_path_params: vec![],
            trait_attr,
            // input,
            fields,
            variants,
            variant_states: variant_states?,
            variant: None,
            derive_type,
            generics,
            full_meta_infos,
            default_info: defaults,
        })
    }

    pub fn from_variant<'arg_input>(
        input: &'arg_input DeriveInput,
        trait_name: &'static str,
        trait_attr: String,
        allowed_attr_params: AttrParams,
        variant: &'arg_input Variant,
        default_info: FullMetaInfo,
    ) -> Result<State<'arg_input>> {
        let trait_name = trait_name.trim_end_matches("ToInner");
        let trait_ident = format_ident!("{trait_name}");
        let method_ident = format_ident!("{trait_attr}");
        let trait_path = quote! { ::derive_more::#trait_ident };
        let (derive_type, fields): (_, Vec<_>) = match variant.fields {
            Fields::Unnamed(ref fields) => {
                (DeriveType::Unnamed, unnamed_to_vec(fields))
            }

            Fields::Named(ref fields) => (DeriveType::Named, named_to_vec(fields)),
            Fields::Unit => (DeriveType::Named, vec![]),
        };

        let meta_infos: Result<Vec<_>> = fields
            .iter()
            .map(|f| &f.attrs)
            .map(|attrs| get_meta_info(&trait_attr, attrs, &allowed_attr_params.field))
            .collect();
        let meta_infos = meta_infos?;
        let full_meta_infos: Vec<_> = meta_infos
            .into_iter()
            .map(|info| info.into_full(default_info.clone()))
            .collect();

        let generics = add_extra_ty_param_bound(&input.generics, &trait_path);

        Ok(State {
            input,
            trait_name,
            trait_path,
            trait_path_params: vec![],
            trait_attr,
            trait_ident,
            method_ident,
            // input,
            fields,
            variants: vec![],
            variant_states: vec![],
            variant: Some(variant),
            derive_type,
            generics,
            full_meta_infos,
            default_info,
        })
    }
    pub fn add_trait_path_type_param(&mut self, param: TokenStream) {
        self.trait_path_params.push(param);
    }

    pub fn assert_single_enabled_field<'state>(
        &'state self,
    ) -> SingleFieldData<'input, 'state> {
        if self.derive_type == DeriveType::Enum {
            panic_one_field(self.trait_name, &self.trait_attr);
        }
        let data = self.enabled_fields_data();
        if data.fields.len() != 1 {
            panic_one_field(self.trait_name, &self.trait_attr);
        };
        SingleFieldData {
            input_type: data.input_type,
            field: data.fields[0],
            field_type: data.field_types[0],
            member: data.members[0].clone(),
            info: data.infos[0].clone(),
            field_ident: data.field_idents[0].clone(),
            trait_path: data.trait_path,
            trait_path_with_params: data.trait_path_with_params.clone(),
            casted_trait: data.casted_traits[0].clone(),
            impl_generics: data.impl_generics.clone(),
            ty_generics: data.ty_generics.clone(),
            where_clause: data.where_clause,
            multi_field_data: data,
        }
    }

    pub fn enabled_fields_data<'state>(&'state self) -> MultiFieldData<'input, 'state> {
        if self.derive_type == DeriveType::Enum {
            panic!("cannot derive({}) for enum", self.trait_name)
        }
        let fields = self.enabled_fields();
        let field_idents = self.enabled_fields_idents();
        let field_indexes = self.enabled_fields_indexes();
        let field_types: Vec<_> = fields.iter().map(|f| &f.ty).collect();
        let members: Vec<_> = field_idents
            .iter()
            .map(|ident| quote! { self.#ident })
            .collect();
        let trait_path = &self.trait_path;
        let trait_path_with_params = if !self.trait_path_params.is_empty() {
            let params = self.trait_path_params.iter();
            quote! { #trait_path<#(#params),*> }
        } else {
            self.trait_path.clone()
        };

        let casted_traits: Vec<_> = field_types
            .iter()
            .map(|field_type| quote! { <#field_type as #trait_path_with_params> })
            .collect();
        let (impl_generics, ty_generics, where_clause) = self.generics.split_for_impl();
        let input_type = &self.input.ident;
        let (variant_name, variant_type) = self.variant.map_or_else(
            || (None, quote! { #input_type }),
            |v| {
                let variant_name = &v.ident;
                (Some(variant_name), quote! { #input_type::#variant_name })
            },
        );
        MultiFieldData {
            input_type,
            variant_type,
            variant_name,
            variant_info: self.default_info.clone(),
            fields,
            field_types,
            field_indexes,
            members,
            infos: self.enabled_infos(),
            field_idents,
            method_ident: &self.method_ident,
            trait_path,
            trait_path_with_params,
            casted_traits,
            impl_generics,
            ty_generics,
            where_clause,
            state: self,
        }
    }

    pub fn enabled_variant_data<'state>(
        &'state self,
    ) -> MultiVariantData<'input, 'state> {
        if self.derive_type != DeriveType::Enum {
            panic!("can only derive({}) for enum", self.trait_name)
        }
        let variants = self.enabled_variants();
        let trait_path = &self.trait_path;
        let (impl_generics, ty_generics, where_clause) = self.generics.split_for_impl();
        MultiVariantData {
            input_type: &self.input.ident,
            variants,
            variant_states: self.enabled_variant_states(),
            infos: self.enabled_infos(),
            trait_path,
            impl_generics,
            ty_generics,
            where_clause,
        }
    }

    fn enabled_variants(&self) -> Vec<&'input Variant> {
        self.variants
            .iter()
            .zip(self.full_meta_infos.iter().map(|info| info.enabled))
            .filter(|(_, ig)| *ig)
            .map(|(v, _)| *v)
            .collect()
    }

    fn enabled_variant_states(&self) -> Vec<&State<'input>> {
        self.variant_states
            .iter()
            .zip(self.full_meta_infos.iter().map(|info| info.enabled))
            .filter(|(_, ig)| *ig)
            .map(|(v, _)| v)
            .collect()
    }

    pub fn enabled_fields(&self) -> Vec<&'input Field> {
        self.fields
            .iter()
            .zip(self.full_meta_infos.iter().map(|info| info.enabled))
            .filter(|(_, ig)| *ig)
            .map(|(f, _)| *f)
            .collect()
    }

    fn field_idents(&self) -> Vec<TokenStream> {
        if self.derive_type == DeriveType::Named {
            self.fields
                .iter()
                .map(|f| {
                    f.ident
                        .as_ref()
                        .expect("Tried to get field names of a tuple struct")
                        .to_token_stream()
                })
                .collect()
        } else {
            let count = self.fields.len();
            (0..count)
                .map(|i| Index::from(i).to_token_stream())
                .collect()
        }
    }

    fn enabled_fields_idents(&self) -> Vec<TokenStream> {
        self.field_idents()
            .into_iter()
            .zip(self.full_meta_infos.iter().map(|info| info.enabled))
            .filter(|(_, ig)| *ig)
            .map(|(f, _)| f)
            .collect()
    }

    fn enabled_fields_indexes(&self) -> Vec<usize> {
        self.full_meta_infos
            .iter()
            .map(|info| info.enabled)
            .enumerate()
            .filter(|(_, ig)| *ig)
            .map(|(i, _)| i)
            .collect()
    }
    fn enabled_infos(&self) -> Vec<FullMetaInfo> {
        self.full_meta_infos
            .iter()
            .filter(|info| info.enabled)
            .cloned()
            .collect()
    }
}

#[derive(Clone)]
pub struct SingleFieldData<'input, 'state> {
    pub input_type: &'input Ident,
    pub field: &'input Field,
    pub field_type: &'input Type,
    pub field_ident: TokenStream,
    pub member: TokenStream,
    pub info: FullMetaInfo,
    pub trait_path: &'state TokenStream,
    pub trait_path_with_params: TokenStream,
    pub casted_trait: TokenStream,
    pub impl_generics: ImplGenerics<'state>,
    pub ty_generics: TypeGenerics<'state>,
    pub where_clause: Option<&'state WhereClause>,
    multi_field_data: MultiFieldData<'input, 'state>,
}

#[derive(Clone)]
pub struct MultiFieldData<'input, 'state> {
    pub input_type: &'input Ident,
    pub variant_type: TokenStream,
    pub variant_name: Option<&'input Ident>,
    pub variant_info: FullMetaInfo,
    pub fields: Vec<&'input Field>,
    pub field_types: Vec<&'input Type>,
    pub field_idents: Vec<TokenStream>,
    pub field_indexes: Vec<usize>,
    pub members: Vec<TokenStream>,
    pub infos: Vec<FullMetaInfo>,
    pub method_ident: &'state Ident,
    pub trait_path: &'state TokenStream,
    pub trait_path_with_params: TokenStream,
    pub casted_traits: Vec<TokenStream>,
    pub impl_generics: ImplGenerics<'state>,
    pub ty_generics: TypeGenerics<'state>,
    pub where_clause: Option<&'state WhereClause>,
    pub state: &'state State<'input>,
}

pub struct MultiVariantData<'input, 'state> {
    pub input_type: &'input Ident,
    pub variants: Vec<&'input Variant>,
    pub variant_states: Vec<&'state State<'input>>,
    pub infos: Vec<FullMetaInfo>,
    pub trait_path: &'state TokenStream,
    pub impl_generics: ImplGenerics<'state>,
    pub ty_generics: TypeGenerics<'state>,
    pub where_clause: Option<&'state WhereClause>,
}

impl<'input, 'state> MultiFieldData<'input, 'state> {
    pub fn initializer<T: ToTokens>(&self, initializers: &[T]) -> TokenStream {
        let MultiFieldData {
            variant_type,
            field_idents,
            ..
        } = self;
        if self.state.derive_type == DeriveType::Named {
            quote! { #variant_type{#(#field_idents: #initializers),*} }
        } else {
            quote! { #variant_type(#(#initializers),*) }
        }
    }
    pub fn matcher<T: ToTokens>(
        &self,
        indexes: &[usize],
        bindings: &[T],
    ) -> TokenStream {
        let MultiFieldData { variant_type, .. } = self;
        let full_bindings = (0..self.state.fields.len()).map(|i| {
            indexes.iter().position(|index| i == *index).map_or_else(
                || quote! { _ },
                |found_index| bindings[found_index].to_token_stream(),
            )
        });
        if self.state.derive_type == DeriveType::Named {
            let field_idents = self.state.field_idents();
            quote! { #variant_type{#(#field_idents: #full_bindings),*} }
        } else {
            quote! { #variant_type(#(#full_bindings),*) }
        }
    }
}

impl<'input, 'state> SingleFieldData<'input, 'state> {
    pub fn initializer<T: ToTokens>(&self, initializers: &[T]) -> TokenStream {
        self.multi_field_data.initializer(initializers)
    }
}

fn get_meta_info(
    trait_attr: &str,
    attrs: &[Attribute],
    allowed_attr_params: &[&str],
) -> Result<MetaInfo> {
    let mut it = attrs.iter().filter(|a| {
        a.meta
            .path()
            .segments
            .first()
            .map(|p| p.ident == trait_attr)
            .unwrap_or_default()
    });

    let mut info = MetaInfo::default();

    let Some(attr) = it.next() else {
        return Ok(info);
    };

    if allowed_attr_params.is_empty() {
        return Err(Error::new(attr.span(), "Attribute is not allowed here"));
    }

    info.enabled = Some(true);

    if let Some(another_attr) = it.next() {
        return Err(Error::new(
            another_attr.span(),
            "Only a single attribute is allowed",
        ));
    }

    let list = match &attr.meta {
        syn::Meta::Path(_) => {
            if allowed_attr_params.contains(&"ignore") {
                return Ok(info);
            } else {
                return Err(Error::new(
                    attr.span(),
                    format!(
                        "Empty attribute is not allowed, add one of the following parameters: {}",
                        allowed_attr_params.join(", "),
                    ),
                ));
            }
        }
        syn::Meta::List(list) => list,
        syn::Meta::NameValue(val) => {
            return Err(Error::new(
                val.span(),
                "Attribute doesn't support name-value format here",
            ));
        }
    };

    parse_punctuated_nested_meta(
        &mut info,
        &list.parse_args_with(Punctuated::parse_terminated)?,
        allowed_attr_params,
        None,
    )?;

    Ok(info)
}

fn parse_punctuated_nested_meta(
    info: &mut MetaInfo,
    meta: &Punctuated<polyfill::Meta, Token![,]>,
    allowed_attr_params: &[&str],
    wrapper_name: Option<&str>,
) -> Result<()> {
    for meta in meta.iter() {
        match meta {
            polyfill::Meta::List(list) if list.path.is_ident("not") => {
                if wrapper_name.is_some() {
                    // Only single top-level `not` attribute is allowed.
                    return Err(Error::new(
                        list.span(),
                        "Attribute doesn't support multiple multiple or nested `not` parameters",
                    ));
                }
                parse_punctuated_nested_meta(
                    info,
                    &list.parse_args_with(Punctuated::parse_terminated)?,
                    allowed_attr_params,
                    Some("not"),
                )?;
            }

            polyfill::Meta::List(list) => {
                let path = &list.path;
                if !allowed_attr_params.iter().any(|param| path.is_ident(param)) {
                    return Err(Error::new(
                        meta.span(),
                        format!(
                            "Attribute nested parameter not supported. \
                             Supported attribute parameters are: {}",
                            allowed_attr_params.join(", "),
                        ),
                    ));
                }

                let mut parse_nested = true;

                let attr_name = path.get_ident().unwrap().to_string();
                match (wrapper_name, attr_name.as_str()) {
                    (None, "owned") => info.owned = Some(true),
                    (None, "ref") => info.ref_ = Some(true),
                    (None, "ref_mut") => info.ref_mut = Some(true),

                    #[cfg(any(feature = "from", feature = "into"))]
                    (None, "types")
                    | (Some("owned"), "types")
                    | (Some("ref"), "types")
                    | (Some("ref_mut"), "types") => {
                        parse_nested = false;
                        for meta in &list.parse_args_with(
                            Punctuated::<polyfill::NestedMeta, syn::token::Comma>::parse_terminated,
                        )? {
                            let typ: syn::Type = match meta {
                                polyfill::NestedMeta::Meta(meta) => {
                                    let polyfill::Meta::Path(path) = meta else {
                                        return Err(Error::new(
                                            meta.span(),
                                            format!(
                                                "Attribute doesn't support type {}",
                                                quote! { #meta },
                                            ),
                                        ));
                                    };
                                    syn::TypePath {
                                        qself: None,
                                        path: path.clone().into(),
                                    }
                                    .into()
                                }
                                polyfill::NestedMeta::Lit(syn::Lit::Str(s)) => s.parse()?,
                                polyfill::NestedMeta::Lit(lit) => return Err(Error::new(
                                    lit.span(),
                                    "Attribute doesn't support nested literals here",
                                )),
                            };

                            for ref_type in wrapper_name
                                .map(|n| vec![RefType::from_attr_name(n)])
                                .unwrap_or_else(|| {
                                    vec![RefType::No, RefType::Ref, RefType::Mut]
                                })
                            {
                                if info
                                    .types
                                    .entry(ref_type)
                                    .or_default()
                                    .replace(typ.clone())
                                    .is_some()
                                {
                                    return Err(Error::new(
                                        typ.span(),
                                        format!(
                                            "Duplicate type `{}` specified",
                                            quote! { #path },
                                        ),
                                    ));
                                }
                            }
                        }
                    }

                    _ => {
                        return Err(Error::new(
                            list.span(),
                            format!(
                                "Attribute doesn't support nested parameter `{}` here",
                                quote! { #path },
                            ),
                        ))
                    }
                };

                if parse_nested {
                    parse_punctuated_nested_meta(
                        info,
                        &list.parse_args_with(Punctuated::parse_terminated)?,
                        allowed_attr_params,
                        Some(&attr_name),
                    )?;
                }
            }

            polyfill::Meta::Path(path) => {
                if !allowed_attr_params.iter().any(|param| path.is_ident(param)) {
                    return Err(Error::new(
                        meta.span(),
                        format!(
                            "Attribute parameter not supported. \
                             Supported attribute parameters are: {}",
                            allowed_attr_params.join(", "),
                        ),
                    ));
                }

                let attr_name = path.get_ident().unwrap().to_string();
                match (wrapper_name, attr_name.as_str()) {
                    (None, "ignore") => info.enabled = Some(false),
                    (None, "forward") => info.forward = Some(true),
                    (Some("not"), "forward") => info.forward = Some(false),
                    (None, "owned") => info.owned = Some(true),
                    (None, "ref") => info.ref_ = Some(true),
                    (None, "ref_mut") => info.ref_mut = Some(true),
                    (None, "source") => info.source = Some(true),
                    (Some("not"), "source") => info.source = Some(false),
                    (None, "backtrace") => info.backtrace = Some(true),
                    (Some("not"), "backtrace") => info.backtrace = Some(false),
                    _ => {
                        return Err(Error::new(
                            path.span(),
                            format!(
                                "Attribute doesn't support parameter `{}` here",
                                quote! { #path }
                            ),
                        ))
                    }
                }
            }
        }
    }

    Ok(())
}

// TODO: Remove this eventually, once all macros migrate to
//       custom typed attributes parsing.
/// Polyfill for [`syn`] 1.x AST.
pub(crate) mod polyfill {
    use proc_macro2::TokenStream;
    use quote::ToTokens;
    use syn::{
        ext::IdentExt as _,
        parse::{Parse, ParseStream, Parser},
        token, Token,
    };

    #[derive(Clone)]
    pub(crate) enum PathOrKeyword {
        Path(syn::Path),
        Keyword(syn::Ident),
    }

    impl Parse for PathOrKeyword {
        fn parse(input: ParseStream<'_>) -> syn::Result<Self> {
            if input.fork().parse::<syn::Path>().is_ok() {
                return input.parse().map(Self::Path);
            }
            syn::Ident::parse_any(input).map(Self::Keyword)
        }
    }

    impl ToTokens for PathOrKeyword {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            match self {
                Self::Path(p) => p.to_tokens(tokens),
                Self::Keyword(i) => i.to_tokens(tokens),
            }
        }
    }

    impl PathOrKeyword {
        pub(crate) fn is_ident<I: ?Sized>(&self, ident: &I) -> bool
        where
            syn::Ident: PartialEq<I>,
        {
            match self {
                Self::Path(p) => p.is_ident(ident),
                Self::Keyword(i) => i == ident,
            }
        }

        pub fn get_ident(&self) -> Option<&syn::Ident> {
            match self {
                Self::Path(p) => p.get_ident(),
                Self::Keyword(i) => Some(i),
            }
        }
    }

    impl From<PathOrKeyword> for syn::Path {
        fn from(p: PathOrKeyword) -> Self {
            match p {
                PathOrKeyword::Path(p) => p,
                PathOrKeyword::Keyword(i) => i.into(),
            }
        }
    }

    #[derive(Clone)]
    pub(crate) struct MetaList {
        pub(crate) path: PathOrKeyword,
        pub(crate) tokens: TokenStream,
    }

    impl Parse for MetaList {
        fn parse(input: ParseStream<'_>) -> syn::Result<Self> {
            let path = input.parse::<PathOrKeyword>()?;
            let tokens;
            _ = syn::parenthesized!(tokens in input);
            Ok(Self {
                path,
                tokens: tokens.parse()?,
            })
        }
    }

    impl ToTokens for MetaList {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.path.to_tokens(tokens);
            token::Paren::default()
                .surround(tokens, |tokens| self.tokens.to_tokens(tokens))
        }
    }

    impl MetaList {
        pub fn parse_args_with<F: Parser>(&self, parser: F) -> syn::Result<F::Output> {
            parser.parse2(self.tokens.clone())
        }
    }

    #[derive(Clone)]
    pub(crate) enum Meta {
        Path(PathOrKeyword),
        List(MetaList),
    }

    impl Parse for Meta {
        fn parse(input: ParseStream<'_>) -> syn::Result<Self> {
            let path = input.parse::<PathOrKeyword>()?;
            Ok(if input.peek(token::Paren) {
                let tokens;
                _ = syn::parenthesized!(tokens in input);
                Self::List(MetaList {
                    path,
                    tokens: tokens.parse()?,
                })
            } else {
                Self::Path(path)
            })
        }
    }

    impl ToTokens for Meta {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            match self {
                Self::Path(p) => p.to_tokens(tokens),
                Self::List(l) => l.to_tokens(tokens),
            }
        }
    }

    #[derive(Clone)]
    pub(crate) enum NestedMeta {
        Meta(Meta),
        Lit(syn::Lit),
    }

    impl Parse for NestedMeta {
        fn parse(input: ParseStream<'_>) -> syn::Result<Self> {
            if input.peek(syn::Lit)
                && !(input.peek(syn::LitBool) && input.peek2(Token![=]))
            {
                input.parse().map(Self::Lit)
            } else if input.peek(syn::Ident::peek_any)
                || input.peek(Token![::]) && input.peek3(syn::Ident::peek_any)
            {
                input.parse().map(Self::Meta)
            } else {
                Err(input.error("expected identifier or literal"))
            }
        }
    }

    impl ToTokens for NestedMeta {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            match self {
                Self::Meta(m) => m.to_tokens(tokens),
                Self::Lit(l) => l.to_tokens(tokens),
            }
        }
    }
}

#[derive(Clone, Debug, Default)]
pub struct FullMetaInfo {
    pub enabled: bool,
    pub forward: bool,
    pub owned: bool,
    pub ref_: bool,
    pub ref_mut: bool,
    pub info: MetaInfo,
}

#[derive(Clone, Debug, Default)]
pub struct MetaInfo {
    pub enabled: Option<bool>,
    pub forward: Option<bool>,
    pub owned: Option<bool>,
    pub ref_: Option<bool>,
    pub ref_mut: Option<bool>,
    pub source: Option<bool>,
    pub backtrace: Option<bool>,
    #[cfg(any(feature = "from", feature = "into"))]
    pub types: HashMap<RefType, HashSet<syn::Type>>,
}

impl MetaInfo {
    fn into_full(self, defaults: FullMetaInfo) -> FullMetaInfo {
        FullMetaInfo {
            enabled: self.enabled.unwrap_or(defaults.enabled),
            forward: self.forward.unwrap_or(defaults.forward),
            owned: self.owned.unwrap_or(defaults.owned),
            ref_: self.ref_.unwrap_or(defaults.ref_),
            ref_mut: self.ref_mut.unwrap_or(defaults.ref_mut),
            info: self,
        }
    }
}

impl FullMetaInfo {
    pub fn ref_types(&self) -> Vec<RefType> {
        let mut ref_types = vec![];
        if self.owned {
            ref_types.push(RefType::No);
        }
        if self.ref_ {
            ref_types.push(RefType::Ref);
        }
        if self.ref_mut {
            ref_types.push(RefType::Mut);
        }
        ref_types
    }
}

pub fn get_if_type_parameter_used_in_type(
    type_parameters: &HashSet<syn::Ident>,
    ty: &syn::Type,
) -> Option<syn::Type> {
    is_type_parameter_used_in_type(type_parameters, ty).then(|| match ty {
        syn::Type::Reference(syn::TypeReference { elem: ty, .. }) => (**ty).clone(),
        ty => ty.clone(),
    })
}

pub fn is_type_parameter_used_in_type(
    type_parameters: &HashSet<syn::Ident>,
    ty: &syn::Type,
) -> bool {
    match ty {
        syn::Type::Path(ty) => {
            if let Some(qself) = &ty.qself {
                if is_type_parameter_used_in_type(type_parameters, &qself.ty) {
                    return true;
                }
            }

            if let Some(segment) = ty.path.segments.first() {
                if type_parameters.contains(&segment.ident) {
                    return true;
                }
            }

            ty.path.segments.iter().any(|segment| {
                if let syn::PathArguments::AngleBracketed(arguments) =
                    &segment.arguments
                {
                    arguments.args.iter().any(|argument| match argument {
                        syn::GenericArgument::Type(ty) => {
                            is_type_parameter_used_in_type(type_parameters, ty)
                        }
                        syn::GenericArgument::Constraint(constraint) => {
                            type_parameters.contains(&constraint.ident)
                        }
                        _ => false,
                    })
                } else {
                    false
                }
            })
        }

        syn::Type::Reference(ty) => {
            is_type_parameter_used_in_type(type_parameters, &ty.elem)
        }

        _ => false,
    }
}

#[cfg(any(feature = "from", feature = "into"))]
mod either {
    use proc_macro2::TokenStream;
    use quote::ToTokens;

    /// Either [`Left`] or [`Right`].
    ///
    /// [`Left`]: Either::Left
    /// [`Right`]: Either::Right
    pub(crate) enum Either<L, R> {
        /// Left variant.
        Left(L),

        /// Right variant.
        Right(R),
    }

    impl<L, R, T> Iterator for Either<L, R>
    where
        L: Iterator<Item = T>,
        R: Iterator<Item = T>,
    {
        type Item = T;

        fn next(&mut self) -> Option<Self::Item> {
            match self {
                Either::Left(left) => left.next(),
                Either::Right(right) => right.next(),
            }
        }
    }

    impl<L, R> ToTokens for Either<L, R>
    where
        L: ToTokens,
        R: ToTokens,
    {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            match self {
                Either::Left(l) => l.to_tokens(tokens),
                Either::Right(r) => r.to_tokens(tokens),
            }
        }
    }
}

#[cfg(any(feature = "from", feature = "into"))]
mod fields_ext {
    use std::{cmp, iter};

    use proc_macro2::TokenStream;
    use syn::{punctuated, spanned::Spanned as _};

    use crate::parsing;

    use super::Either;

    /// Abstraction over `.len()` method to use it on type parameters.
    pub(crate) trait Len {
        /// Returns number of fields.
        fn len(&self) -> usize;
    }

    impl Len for syn::Fields {
        fn len(&self) -> usize {
            self.len()
        }
    }

    impl<T> Len for Vec<T> {
        fn len(&self) -> usize {
            self.len()
        }
    }

    /// [`syn::Fields`] extension.
    pub(crate) trait FieldsExt: Len {
        /// Validates the provided [`parsing::Type`] against these [`syn::Fields`].
        fn validate_type<'t>(
            &self,
            ty: &'t parsing::Type,
        ) -> syn::Result<
            Either<punctuated::Iter<'t, TokenStream>, iter::Once<&'t TokenStream>>,
        > {
            match ty {
                parsing::Type::Tuple { items, .. } if self.len() > 1 => {
                    match self.len().cmp(&items.len()) {
                        cmp::Ordering::Greater => {
                            return Err(syn::Error::new(
                                ty.span(),
                                format!(
                                    "wrong tuple length: expected {}, found {}. \
                                     Consider adding {} more type{}: `({})`",
                                    self.len(),
                                    items.len(),
                                    self.len() - items.len(),
                                    if self.len() - items.len() > 1 {
                                        "s"
                                    } else {
                                        ""
                                    },
                                    items
                                        .iter()
                                        .map(|item| item.to_string())
                                        .chain(
                                            (0..(self.len() - items.len()))
                                                .map(|_| "_".to_string())
                                        )
                                        .collect::<Vec<_>>()
                                        .join(", "),
                                ),
                            ));
                        }
                        cmp::Ordering::Less => {
                            return Err(syn::Error::new(
                                ty.span(),
                                format!(
                                    "wrong tuple length: expected {}, found {}. \
                                     Consider removing last {} type{}: `({})`",
                                    self.len(),
                                    items.len(),
                                    items.len() - self.len(),
                                    if items.len() - self.len() > 1 {
                                        "s"
                                    } else {
                                        ""
                                    },
                                    items
                                        .iter()
                                        .take(self.len())
                                        .map(ToString::to_string)
                                        .collect::<Vec<_>>()
                                        .join(", "),
                                ),
                            ));
                        }
                        cmp::Ordering::Equal => {}
                    }
                }
                parsing::Type::Other(other) if self.len() > 1 => {
                    if self.len() > 1 {
                        return Err(syn::Error::new(
                            other.span(),
                            format!(
                                "expected tuple: `({}, {})`",
                                other,
                                (0..(self.len() - 1))
                                    .map(|_| "_")
                                    .collect::<Vec<_>>()
                                    .join(", "),
                            ),
                        ));
                    }
                }
                parsing::Type::Tuple { .. } | parsing::Type::Other(_) => {}
            }
            Ok(match ty {
                parsing::Type::Tuple { items, .. } => Either::Left(items.iter()),
                parsing::Type::Other(other) => Either::Right(iter::once(other)),
            })
        }
    }

    impl<T: Len + ?Sized> FieldsExt for T {}
}
