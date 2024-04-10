/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::util::{
    create_metadata_items, ident_to_string, mod_path, try_metadata_value_from_usize,
};
use proc_macro2::{Span, TokenStream};
use quote::quote;
use syn::{spanned::Spanned, FnArg, Ident, Pat, Receiver, ReturnType, Type};

pub(crate) struct FnSignature {
    pub kind: FnKind,
    pub span: Span,
    pub mod_path: String,
    pub ident: Ident,
    pub name: String,
    pub is_async: bool,
    pub receiver: Option<ReceiverArg>,
    pub args: Vec<NamedArg>,
    pub return_ty: TokenStream,
    // Does this the return type look like a result?
    // Only use this in UDL mode.
    // In general, it's not reliable because it fails for type aliases.
    pub looks_like_result: bool,
}

impl FnSignature {
    pub(crate) fn new_function(sig: syn::Signature) -> syn::Result<Self> {
        Self::new(FnKind::Function, sig)
    }

    pub(crate) fn new_method(self_ident: Ident, sig: syn::Signature) -> syn::Result<Self> {
        Self::new(FnKind::Method { self_ident }, sig)
    }

    pub(crate) fn new_constructor(self_ident: Ident, sig: syn::Signature) -> syn::Result<Self> {
        Self::new(FnKind::Constructor { self_ident }, sig)
    }

    pub(crate) fn new_trait_method(
        self_ident: Ident,
        sig: syn::Signature,
        index: u32,
    ) -> syn::Result<Self> {
        Self::new(FnKind::TraitMethod { self_ident, index }, sig)
    }

    pub(crate) fn new(kind: FnKind, sig: syn::Signature) -> syn::Result<Self> {
        let span = sig.span();
        let ident = sig.ident;
        let looks_like_result = looks_like_result(&sig.output);
        let output = match sig.output {
            ReturnType::Default => quote! { () },
            ReturnType::Type(_, ty) => quote! { #ty },
        };
        let is_async = sig.asyncness.is_some();

        if is_async && matches!(kind, FnKind::Constructor { .. }) {
            return Err(syn::Error::new(
                span,
                "Async constructors are not supported",
            ));
        }

        let mut input_iter = sig.inputs.into_iter().map(Arg::try_from).peekable();

        let receiver = input_iter
            .next_if(|a| matches!(a, Ok(a) if a.is_receiver()))
            .map(|a| match a {
                Ok(Arg {
                    kind: ArgKind::Receiver(r),
                    ..
                }) => r,
                _ => unreachable!(),
            });
        let args = input_iter
            .map(|a| {
                a.and_then(|a| match a.kind {
                    ArgKind::Named(named) => Ok(named),
                    ArgKind::Receiver(_) => {
                        Err(syn::Error::new(a.span, "Unexpected receiver argument"))
                    }
                })
            })
            .collect::<syn::Result<Vec<_>>>()?;
        let mod_path = mod_path()?;

        Ok(Self {
            kind,
            span,
            mod_path,
            name: ident_to_string(&ident),
            ident,
            is_async,
            receiver,
            args,
            return_ty: output,
            looks_like_result,
        })
    }

    pub fn return_impl(&self) -> TokenStream {
        let return_ty = &self.return_ty;
        quote! {
            <#return_ty as ::uniffi::LowerReturn<crate::UniFfiTag>>
        }
    }

    /// Generate a closure that tries to lift all arguments into a tuple.
    ///
    /// The closure moves all scaffolding arguments into itself and returns:
    ///   - The lifted argument tuple on success
    ///   - The field name and error on failure (`Err(&'static str, anyhow::Error>`)
    pub fn lift_closure(&self, self_lift: Option<TokenStream>) -> TokenStream {
        let arg_lifts = self.args.iter().map(|arg| {
            let ident = &arg.ident;
            let lift_impl = arg.lift_impl();
            let name = &arg.name;
            quote! {
                match #lift_impl::try_lift(#ident) {
                    Ok(v) => v,
                    Err(e) => return Err((#name, e)),
                }
            }
        });
        let all_lifts = self_lift.into_iter().chain(arg_lifts);
        quote! {
            move || Ok((
                #(#all_lifts,)*
            ))
        }
    }

    /// Call a Rust function from a [Self::lift_closure] success.
    ///
    /// This takes an Ok value returned by `lift_closure` with the name `uniffi_args` and generates
    /// a series of parameters to pass to the Rust function.
    pub fn rust_call_params(&self, self_lift: bool) -> TokenStream {
        let start_idx = if self_lift { 1 } else { 0 };
        let args = self.args.iter().enumerate().map(|(i, arg)| {
            let idx = syn::Index::from(i + start_idx);
            let ty = &arg.ty;
            match &arg.ref_type {
                None => quote! { uniffi_args.#idx },
                Some(ref_type) => quote! {
                    <#ty as ::std::borrow::Borrow<#ref_type>>::borrow(&uniffi_args.#idx)
                },
            }
        });
        quote! { #(#args),* }
    }

    /// Write expressions for each of our arguments
    pub fn write_exprs<'a>(
        &'a self,
        buf_ident: &'a Ident,
    ) -> impl Iterator<Item = TokenStream> + 'a {
        self.args.iter().map(|a| a.write_expr(buf_ident))
    }

    /// Parameters expressions for each of our arguments
    pub fn params(&self) -> impl Iterator<Item = TokenStream> + '_ {
        self.args.iter().map(NamedArg::param)
    }

    /// Name of the scaffolding function to generate for this function
    pub fn scaffolding_fn_ident(&self) -> syn::Result<Ident> {
        let name = &self.name;
        let name = match &self.kind {
            FnKind::Function => uniffi_meta::fn_symbol_name(&self.mod_path, name),
            FnKind::Method { self_ident } | FnKind::TraitMethod { self_ident, .. } => {
                uniffi_meta::method_symbol_name(&self.mod_path, &ident_to_string(self_ident), name)
            }
            FnKind::Constructor { self_ident } => uniffi_meta::constructor_symbol_name(
                &self.mod_path,
                &ident_to_string(self_ident),
                name,
            ),
        };
        Ok(Ident::new(&name, Span::call_site()))
    }

    /// Scaffolding parameters expressions for each of our arguments
    pub fn scaffolding_params(&self) -> impl Iterator<Item = TokenStream> + '_ {
        self.args.iter().map(NamedArg::scaffolding_param)
    }

    /// Generate metadata items for this function
    pub(crate) fn metadata_expr(&self) -> syn::Result<TokenStream> {
        let Self {
            name,
            return_ty,
            is_async,
            mod_path,
            ..
        } = &self;
        let args_len = try_metadata_value_from_usize(
            // Use param_lifts to calculate this instead of sig.inputs to avoid counting any self
            // params
            self.args.len(),
            "UniFFI limits functions to 256 arguments",
        )?;
        let arg_metadata_calls = self.args.iter().map(NamedArg::arg_metadata);

        match &self.kind {
            FnKind::Function => Ok(quote! {
                ::uniffi::MetadataBuffer::from_code(::uniffi::metadata::codes::FUNC)
                    .concat_str(#mod_path)
                    .concat_str(#name)
                    .concat_bool(#is_async)
                    .concat_value(#args_len)
                    #(#arg_metadata_calls)*
                    .concat(<#return_ty as ::uniffi::LowerReturn<crate::UniFfiTag>>::TYPE_ID_META)
            }),

            FnKind::Method { self_ident } => {
                let object_name = ident_to_string(self_ident);
                Ok(quote! {
                    ::uniffi::MetadataBuffer::from_code(::uniffi::metadata::codes::METHOD)
                        .concat_str(#mod_path)
                        .concat_str(#object_name)
                        .concat_str(#name)
                        .concat_bool(#is_async)
                        .concat_value(#args_len)
                        #(#arg_metadata_calls)*
                        .concat(<#return_ty as ::uniffi::LowerReturn<crate::UniFfiTag>>::TYPE_ID_META)
                })
            }

            FnKind::TraitMethod { self_ident, index } => {
                let object_name = ident_to_string(self_ident);
                Ok(quote! {
                    ::uniffi::MetadataBuffer::from_code(::uniffi::metadata::codes::TRAIT_METHOD)
                        .concat_str(#mod_path)
                        .concat_str(#object_name)
                        .concat_u32(#index)
                        .concat_str(#name)
                        .concat_bool(#is_async)
                        .concat_value(#args_len)
                        #(#arg_metadata_calls)*
                        .concat(<#return_ty as ::uniffi::LowerReturn<crate::UniFfiTag>>::TYPE_ID_META)
                })
            }

            FnKind::Constructor { self_ident } => {
                let object_name = ident_to_string(self_ident);
                Ok(quote! {
                    ::uniffi::MetadataBuffer::from_code(::uniffi::metadata::codes::CONSTRUCTOR)
                        .concat_str(#mod_path)
                        .concat_str(#object_name)
                        .concat_str(#name)
                        .concat_value(#args_len)
                        #(#arg_metadata_calls)*
                        .concat(<#return_ty as ::uniffi::LowerReturn<crate::UniFfiTag>>::TYPE_ID_META)
                })
            }
        }
    }

    pub(crate) fn metadata_items(&self) -> syn::Result<TokenStream> {
        let Self { name, .. } = &self;
        match &self.kind {
            FnKind::Function => Ok(create_metadata_items(
                "func",
                name,
                self.metadata_expr()?,
                Some(self.checksum_symbol_name()),
            )),

            FnKind::Method { self_ident } => {
                let object_name = ident_to_string(self_ident);
                Ok(create_metadata_items(
                    "method",
                    &format!("{object_name}_{name}"),
                    self.metadata_expr()?,
                    Some(self.checksum_symbol_name()),
                ))
            }

            FnKind::TraitMethod { self_ident, .. } => {
                let object_name = ident_to_string(self_ident);
                Ok(create_metadata_items(
                    "method",
                    &format!("{object_name}_{name}"),
                    self.metadata_expr()?,
                    Some(self.checksum_symbol_name()),
                ))
            }

            FnKind::Constructor { self_ident } => {
                let object_name = ident_to_string(self_ident);
                Ok(create_metadata_items(
                    "constructor",
                    &format!("{object_name}_{name}"),
                    self.metadata_expr()?,
                    Some(self.checksum_symbol_name()),
                ))
            }
        }
    }

    pub(crate) fn checksum_symbol_name(&self) -> String {
        let name = &self.name;
        match &self.kind {
            FnKind::Function => uniffi_meta::fn_checksum_symbol_name(&self.mod_path, name),
            FnKind::Method { self_ident } | FnKind::TraitMethod { self_ident, .. } => {
                uniffi_meta::method_checksum_symbol_name(
                    &self.mod_path,
                    &ident_to_string(self_ident),
                    name,
                )
            }
            FnKind::Constructor { self_ident } => uniffi_meta::constructor_checksum_symbol_name(
                &self.mod_path,
                &ident_to_string(self_ident),
                name,
            ),
        }
    }
}

pub(crate) struct Arg {
    pub(crate) span: Span,
    pub(crate) kind: ArgKind,
}

pub(crate) enum ArgKind {
    Receiver(ReceiverArg),
    Named(NamedArg),
}

impl Arg {
    pub(crate) fn is_receiver(&self) -> bool {
        matches!(self.kind, ArgKind::Receiver(_))
    }
}

impl TryFrom<FnArg> for Arg {
    type Error = syn::Error;

    fn try_from(syn_arg: FnArg) -> syn::Result<Self> {
        let span = syn_arg.span();
        let kind = match syn_arg {
            FnArg::Typed(p) => match *p.pat {
                Pat::Ident(i) => Ok(ArgKind::Named(NamedArg::new(i.ident, &p.ty))),
                _ => Err(syn::Error::new_spanned(p, "Argument name missing")),
            },
            FnArg::Receiver(receiver) => Ok(ArgKind::Receiver(ReceiverArg::from(receiver))),
        }?;

        Ok(Self { span, kind })
    }
}

pub(crate) enum ReceiverArg {
    Ref,
    Arc,
}

impl From<Receiver> for ReceiverArg {
    fn from(receiver: Receiver) -> Self {
        if let Type::Path(p) = *receiver.ty {
            if let Some(segment) = p.path.segments.last() {
                // This comparison will fail if a user uses a typedef for Arc.  Maybe we could
                // implement some system like TYPE_ID_META to figure this out from the type system.
                // However, this seems good enough for now.
                if segment.ident == "Arc" {
                    return ReceiverArg::Arc;
                }
            }
        }
        Self::Ref
    }
}

pub(crate) struct NamedArg {
    pub(crate) ident: Ident,
    pub(crate) name: String,
    pub(crate) ty: TokenStream,
    pub(crate) ref_type: Option<Type>,
}

impl NamedArg {
    pub(crate) fn new(ident: Ident, ty: &Type) -> Self {
        match ty {
            Type::Reference(r) => {
                let inner = &r.elem;
                Self {
                    name: ident_to_string(&ident),
                    ident,
                    ty: quote! { <#inner as ::uniffi::LiftRef<crate::UniFfiTag>>::LiftType },
                    ref_type: Some(*inner.clone()),
                }
            }
            _ => Self {
                name: ident_to_string(&ident),
                ident,
                ty: quote! { #ty },
                ref_type: None,
            },
        }
    }

    pub(crate) fn lift_impl(&self) -> TokenStream {
        let ty = &self.ty;
        quote! { <#ty as ::uniffi::Lift<crate::UniFfiTag>> }
    }

    pub(crate) fn lower_impl(&self) -> TokenStream {
        let ty = &self.ty;
        quote! { <#ty as ::uniffi::Lower<crate::UniFfiTag>> }
    }

    /// Generate the parameter for this Arg
    pub(crate) fn param(&self) -> TokenStream {
        let ident = &self.ident;
        let ty = &self.ty;
        quote! { #ident: #ty }
    }

    /// Generate the scaffolding parameter for this Arg
    pub(crate) fn scaffolding_param(&self) -> TokenStream {
        let ident = &self.ident;
        let lift_impl = self.lift_impl();
        quote! { #ident: #lift_impl::FfiType }
    }

    /// Generate the expression to write the scaffolding parameter for this arg
    pub(crate) fn write_expr(&self, buf_ident: &Ident) -> TokenStream {
        let ident = &self.ident;
        let lower_impl = self.lower_impl();
        quote! { #lower_impl::write(#ident, &mut #buf_ident) }
    }

    pub(crate) fn arg_metadata(&self) -> TokenStream {
        let name = &self.name;
        let lift_impl = self.lift_impl();
        quote! {
            .concat_str(#name)
            .concat(#lift_impl::TYPE_ID_META)
        }
    }
}

fn looks_like_result(return_type: &ReturnType) -> bool {
    if let ReturnType::Type(_, ty) = return_type {
        if let Type::Path(p) = &**ty {
            if let Some(seg) = p.path.segments.last() {
                if seg.ident == "Result" {
                    return true;
                }
            }
        }
    }

    false
}

#[derive(Debug)]
pub(crate) enum FnKind {
    Function,
    Constructor { self_ident: Ident },
    Method { self_ident: Ident },
    TraitMethod { self_ident: Ident, index: u32 },
}
