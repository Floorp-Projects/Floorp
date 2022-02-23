/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! This crate provides the `#[derive(xpcom)]` custom derive. This custom derive
//! is used in order to implement [`xpcom`] interfaces.
//!
//! # Usage
//!
//! The easiest way to explain this crate is probably with a usage example. I'll
//! show you the example, and then we'll destructure it and walk through what
//! each component is doing.
//!
//! ```ignore
//! // Declaring an XPCOM Struct
//! #[derive(xpcom)]
//! #[xpimplements(nsIRunnable)]
//! #[refcnt = "atomic"]
//! struct InitImplRunnable {
//!     i: i32,
//! }
//!
//! // Implementing methods on an XPCOM Struct
//! impl ImplRunnable {
//!     unsafe fn Run(&self) -> nsresult {
//!         println!("{}", self.i);
//!         NS_OK
//!     }
//! }
//! ```
//!
//! ## Declaring an XPCOM Struct
//!
//! ```ignore
//! // This derive should be placed on the initialization struct in order to
//! // trigger the procedural macro.
//! #[derive(xpcom)]
//!
//! // The xpimplements attribute should be passed the names of the IDL
//! // interfaces which you want to implement. These can be separated by commas
//! // if you want to implement multiple interfaces.
//! //
//! // Some methods use types which we cannot bind to in rust. Interfaces
//! // like those cannot be implemented, and a compile-time error will occur
//! // if they are listed in this attribute.
//! #[xpimplements(nsIRunnable)]
//!
//! // The refcnt attribute can have one of the following values:
//! //  * "atomic" == atomic reference count
//! //    ~= NS_DECL_THREADSAFE_ISUPPORTS in C++
//! //  * "nonatomic" == non atomic reference count
//! //    ~= NS_DECL_ISUPPORTS in C++
//! #[refcnt = "atomic"]
//!
//! // The struct with the attribute on its name must start with `Init`.
//! // The custom derive will define the actual underlying struct. For
//! // example, placing the derive on a struct named `InitFoo` will cause
//! // an underlying `Foo` struct to be generated.
//! //
//! // It is a compile time error to put the `#[derive(xpcom)]` derive on
//! // an enum, union, or tuple struct.
//! struct InitImplRunnable {
//!     // Fields in the `Init` struct will also be in the underlying struct.
//!     i: i32,
//! }
//! ```
//!
//! The above example will generate an underlying `ImplRunnable` struct, which will implement
//! the [`nsIRunnable`] XPCOM interface. The following methods will be
//! automatically implemented on it:
//!
//! ```ignore
//! // Automatic nsISupports implementation
//! unsafe fn AddRef(&self) -> nsrefcnt;
//! unsafe fn Release(&self) -> nsrefcnt;
//! unsafe fn QueryInterface(&self, uuid: &nsIID, result: *mut *mut libc::c_void) -> nsresult;
//!
//! // Allocates and initializes a new instance of this type. The values will
//! // be moved from the `Init` struct which is passed in.
//! fn allocate(init: InitImplRunnable) -> RefPtr<Self>;
//!
//! // Helper for performing the `query_interface` operation to case to a
//! // specific interface.
//! fn query_interface<T: XpCom>(&self) -> Option<RefPtr<T>>;
//!
//! // Coerce function for cheaply casting to our base interfaces.
//! fn coerce<T: ImplRunnableCoerce>(&self) -> &T;
//! ```
//!
//! The [`RefCounted`] interface will also be implemented, so that the type can
//! be used within the [`RefPtr`] type.
//!
//! The `coerce` and `query_interface` methods are backed by the generated
//! `*Coerce` trait. This trait is impl-ed for every interface implemented by
//! the trait. For example:
//!
//! ```ignore
//! pub trait ImplRunnableCoerce {
//!     fn coerce_from(x: &ImplRunnable) -> &Self;
//! }
//! impl ImplRunnableCoerce for nsIRunnable { .. }
//! impl ImplRunnableCoerce for nsISupports { .. }
//! ```
//!
//! ## Implementing methods on an XPCOM Struct
//!
//! ```ignore
//! // Methods should be implemented directly on the generated struct. All
//! // methods other than `AddRef`, `Release`, and `QueryInterface` must be
//! // implemented manually.
//! impl ImplRunnable {
//!     // The method should have the same name as the corresponding C++ method.
//!     unsafe fn Run(&self) -> nsresult {
//!         // Fields defined on the `Init` struct will be directly on the
//!         // generated struct.
//!         println!("{}", self.i);
//!         NS_OK
//!     }
//! }
//! ```
//!
//! XPCOM methods implemented in Rust have signatures similar to methods
//! implemented in C++.
//!
//! ```ignore
//! // nsISupports foo(in long long bar, in AString baz);
//! unsafe fn Foo(&self, bar: i64, baz: *const nsAString,
//!               _retval: *mut *const nsISupports) -> nsresult;
//!
//! // AString qux(in nsISupports ham);
//! unsafe fn Qux(&self, ham: *const nsISupports,
//!               _retval: *mut nsAString) -> nsresult;
//! ```
//!
//! This is a little tedious, so the `xpcom_method!` macro provides a convenient
//! way to generate wrappers around more idiomatic Rust methods.
//!
//! [`xpcom`]: ../xpcom/index.html
//! [`nsIRunnable`]: ../xpcom/struct.nsIRunnable.html
//! [`RefCounted`]: ../xpcom/struct.RefCounted.html
//! [`RefPtr`]: ../xpcom/struct.RefPtr.html

use lazy_static::lazy_static;
use proc_macro2::{Span, TokenStream};
use quote::{format_ident, quote, ToTokens};
use std::collections::{HashMap, HashSet};
use syn::punctuated::Punctuated;
use syn::{
    parse_macro_input, parse_quote, Attribute, Data, DataStruct, DeriveInput, Field, Fields, Ident,
    Lit, Meta, NestedMeta, Token, Type,
};

macro_rules! bail {
    (@($t:expr), $s:expr) => {
        return Err(syn::Error::new_spanned(&$t, &$s[..]))
    };
    (@($t:expr), $f:expr, $($e:expr),*) => {
        return Err(syn::Error::new_spanned(&$t, &format!($f, $($e),*)[..]))
    };
    ($s:expr) => {
        return Err(syn::Error::new(Span::call_site(), &$s[..]))
    };
    ($f:expr, $($e:expr),*) => {
        return Err(syn::Error::new(Span::call_site(), &format!($f, $($e),*)[..]))
    };
}

/* These are the structs generated by the rust_macros.py script */

/// A single parameter to an XPCOM method.
#[derive(Debug)]
struct Param {
    name: &'static str,
    ty: &'static str,
}

/// A single method on an XPCOM interface.
#[derive(Debug)]
struct Method {
    name: &'static str,
    params: &'static [Param],
    ret: &'static str,
}

/// An XPCOM interface. `methods` will be `Err("reason")` if the interface
/// cannot be implemented in rust code.
#[derive(Debug)]
struct Interface {
    name: &'static str,
    base: Option<&'static str>,
    methods: Result<&'static [Method], &'static str>,
}

impl Interface {
    fn base(&self) -> Option<&'static Interface> {
        Some(IFACES[self.base?])
    }

    fn methods(&self) -> Result<&'static [Method], syn::Error> {
        match self.methods {
            Ok(methods) => Ok(methods),
            Err(reason) => Err(syn::Error::new(
                Span::call_site(),
                &format!(
                    "Interface {} cannot be implemented in rust \
                     because {} is not supported yet",
                    self.name, reason
                ),
            )),
        }
    }
}

lazy_static! {
    /// This item contains the information generated by the procedural macro in
    /// the form of a `HashMap` from interface names to their descriptions.
    static ref IFACES: HashMap<&'static str, &'static Interface> = {
        let lists: &[&[Interface]] =
            include!(concat!(env!("MOZ_TOPOBJDIR"), "/dist/xpcrs/bt/all.rs"));

        let mut hm = HashMap::new();
        for &list in lists {
            for iface in list {
                hm.insert(iface.name, iface);
            }
        }
        hm
    };
}

/// The type of the reference count to use for the struct.
#[derive(Debug, Eq, PartialEq, Copy, Clone)]
enum RefcntKind {
    Atomic,
    NonAtomic,
}

/// Produces the tokens for the type representation.
impl ToTokens for RefcntKind {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        match *self {
            RefcntKind::NonAtomic => quote!(xpcom::Refcnt).to_tokens(tokens),
            RefcntKind::Atomic => quote!(xpcom::AtomicRefcnt).to_tokens(tokens),
        }
    }
}

/// Scans through the attributes on a struct, and extracts the type of the refcount to use.
fn get_refcnt_kind(attrs: &[Attribute]) -> Result<RefcntKind, syn::Error> {
    for attr in attrs {
        if let Meta::NameValue(syn::MetaNameValue {
            ref path, ref lit, ..
        }) = attr.parse_meta()?
        {
            if !path.is_ident("refcnt") {
                continue;
            }

            let value = if let Lit::Str(ref s) = lit {
                s.value()
            } else {
                bail!(@(attr), "Unexpected non-string value in #[refcnt]");
            };

            return if value == "nonatomic" {
                Ok(RefcntKind::NonAtomic)
            } else if value == "atomic" {
                Ok(RefcntKind::Atomic)
            } else {
                bail!(@(attr), "Unexpected value in #[refcnt]. \
                                Expected `nonatomic`, or `atomic`");
            };
        }
    }

    bail!("Expected #[refcnt] attribute")
}

/// Scan the attributes looking for an #[xpimplements] attribute. The identifier
/// arguments passed to this attribute are the interfaces which the type wants to
/// directly implement.
fn get_bases(attrs: &[Attribute]) -> Result<Vec<&'static Interface>, syn::Error> {
    let mut inherits = Vec::new();
    for attr in attrs {
        if let Meta::List(syn::MetaList {
            ref path,
            ref nested,
            ..
        }) = attr.parse_meta()?
        {
            if !path.is_ident("xpimplements") {
                continue;
            }

            for item in nested.iter() {
                let iface = match *item {
                    NestedMeta::Meta(syn::Meta::Path(ref iface)) => iface,
                    _ => bail!(@(attr), "Unexpected non-identifier in #[xpimplements(..)]"),
                };
                let ident = match iface.get_ident() {
                    Some(ref iface) => iface.to_string(),
                    _ => bail!(@(attr), "Too many components in xpimplements path"),
                };
                if let Some(&iface) = IFACES.get(ident.as_str()) {
                    inherits.push(iface);
                } else {
                    bail!(@(attr), "Unexpected invalid base interface `{}` in #[xpimplements(..)]", ident);
                }
            }
        }
    }
    Ok(inherits)
}

/// Extract the fields list from the input struct.
fn get_fields(di: &DeriveInput) -> Result<&Punctuated<Field, Token![,]>, syn::Error> {
    match di.data {
        Data::Struct(DataStruct {
            fields: Fields::Named(ref named),
            ..
        }) => Ok(&named.named),
        _ => bail!(@(di), "The initializer struct must be a standard named \
                          value struct definition"),
    }
}

/// Takes the `Init*` struct in, and generates a `DeriveInput` for the "real" struct.
fn gen_real_struct(
    init: &DeriveInput,
    bases: &[&Interface],
    refcnt_ty: RefcntKind,
) -> Result<DeriveInput, syn::Error> {
    // Determine the name for the real struct based on the name of the
    // initializer struct's name.
    if !init.ident.to_string().starts_with("Init") {
        bail!(@(init.ident), "The target struct's name must begin with Init");
    }
    let name = Ident::new(&init.ident.to_string()[4..], init.ident.span());
    let vis = &init.vis;

    let bases = bases.iter().map(|base| {
        let ident = format_ident!("__base_{}", base.name);
        let vtable = format_ident!("{}VTable", base.name);
        quote!(#ident : *const xpcom::interfaces::#vtable)
    });

    let fields = get_fields(init)?;
    let (impl_generics, _, where_clause) = init.generics.split_for_impl();
    Ok(parse_quote! {
       #[repr(C)]
       #vis struct #name #impl_generics #where_clause {
           #(#bases,)*
           __refcnt: #refcnt_ty,
           #fields
       }
    })
}

/// Generates the `extern "system"` methods which are actually included in the
/// VTable for the given interface.
///
/// `idx` must be the offset in pointers of the pointer to this vtable in the
/// struct `real`. This is soundness-critical, as it will be used to offset
/// pointers received from xpcom back to the concrete implementation.
fn gen_vtable_methods(
    real: &DeriveInput,
    iface: &Interface,
    vtable_index: usize,
) -> Result<TokenStream, syn::Error> {
    let base_ty = format_ident!("{}", iface.name);

    let base_methods = if let Some(base) = iface.base() {
        gen_vtable_methods(real, base, vtable_index)?
    } else {
        quote! {}
    };

    let ty_name = &real.ident;
    let (impl_generics, ty_generics, where_clause) = real.generics.split_for_impl();

    let mut method_defs = Vec::new();
    for method in iface.methods()? {
        let ret = syn::parse_str::<Type>(method.ret)?;

        let mut params = Vec::new();
        let mut args = Vec::new();
        for param in method.params {
            let name = format_ident!("{}", param.name);
            let ty = syn::parse_str::<Type>(param.ty)?;

            params.push(quote! {#name : #ty,});
            args.push(quote! {#name,});
        }

        let name = format_ident!("{}", method.name);
        method_defs.push(quote! {
            unsafe extern "system" fn #name #impl_generics (
                this: *const #base_ty, #(#params)*
            ) -> #ret #where_clause {
                let this: &#ty_name #ty_generics =
                    ::xpcom::reexports::transmute_from_vtable_ptr(&this, #vtable_index);
                this.#name(#(#args)*)
            }
        });
    }

    Ok(quote! {
        #base_methods
        #(#method_defs)*
    })
}

/// Generates the VTable for a given base interface. This assumes that the
/// implementations of each of the `extern "system"` methods are in scope.
fn gen_inner_vtable(real: &DeriveInput, iface: &Interface) -> Result<TokenStream, syn::Error> {
    let vtable_ty = format_ident!("{}VTable", iface.name);

    // Generate the vtable for the base interface.
    let base_vtable = if let Some(base) = iface.base() {
        let vt = gen_inner_vtable(real, base)?;
        quote! {__base: #vt,}
    } else {
        quote! {}
    };

    // Include each of the method definitions for this interface.
    let (_, ty_generics, _) = real.generics.split_for_impl();
    let turbofish = ty_generics.as_turbofish();
    let vtable_init = iface
        .methods()?
        .into_iter()
        .map(|method| {
            let name = format_ident!("{}", method.name);
            quote! { #name : #name #turbofish, }
        })
        .collect::<Vec<_>>();

    Ok(quote!(#vtable_ty {
        #base_vtable
        #(#vtable_init)*
    }))
}

fn gen_root_vtable(
    real: &DeriveInput,
    base: &Interface,
    idx: usize,
) -> Result<TokenStream, syn::Error> {
    let field = format_ident!("__base_{}", base.name);
    let vtable_ty = format_ident!("{}VTable", base.name);

    let (impl_generics, ty_generics, where_clause) = real.generics.split_for_impl();
    let turbofish = ty_generics.as_turbofish();

    let methods = gen_vtable_methods(real, base, idx)?;
    let vtable = gen_inner_vtable(real, base)?;

    // Define the `recover_self` method. This performs an offset calculation to
    // recover a pointer to the original struct from a pointer to the given
    // VTable field.
    Ok(quote! {#field: {
        // The method implementations which will be used to build the vtable.
        #methods

        // The actual VTable definition. This is in a separate method in order
        // to allow it to be generic.
        #[inline]
        fn get_vtable #impl_generics () -> &'static ::xpcom::reexports::VTableExtra<#vtable_ty> #where_clause {
            &::xpcom::reexports::VTableExtra {
                #[cfg(not(windows))]
                offset: {
                    // NOTE: workaround required to avoid depending on the
                    // unstable const expression feature `const {}`.
                    const OFFSET: isize = -((::std::mem::size_of::<usize>() * #idx) as isize);
                    OFFSET
                },
                #[cfg(not(windows))]
                typeinfo: 0 as *const _,
                vtable: #vtable,
            }
        }
        &get_vtable #turbofish ().vtable
    },})
}

/// Generate the cast implementations. This generates the implementation details
/// for the `Coerce` trait, and the `QueryInterface` method. The first return
/// value is the `QueryInterface` implementation, and the second is the `Coerce`
/// implementation.
fn gen_casts(
    seen: &mut HashSet<&'static str>,
    iface: &Interface,
    real: &DeriveInput,
    coerce_name: &Ident,
    vtable_field: &Ident,
) -> Result<(TokenStream, TokenStream), syn::Error> {
    if !seen.insert(iface.name) {
        return Ok((quote! {}, quote! {}));
    }

    // Generate the cast implementations for the base interfaces.
    let (base_qi, base_coerce) = if let Some(base) = iface.base() {
        gen_casts(seen, base, real, coerce_name, vtable_field)?
    } else {
        (quote! {}, quote! {})
    };

    // Add the if statment to QueryInterface for the base class.
    let base_name = format_ident!("{}", iface.name);

    let qi = quote! {
        #base_qi
        if *uuid == #base_name::IID {
            // Implement QueryInterface in terms of coersions.
            self.addref();
            *result = self.coerce::<#base_name>()
                as *const #base_name
                as *const ::xpcom::reexports::libc::c_void
                as *mut ::xpcom::reexports::libc::c_void;
            return ::xpcom::reexports::NS_OK;
        }
    };

    // Add an implementation of the `*Coerce` trait for the base interface.
    let name = &real.ident;
    let (impl_generics, ty_generics, where_clause) = real.generics.split_for_impl();
    let coerce = quote! {
        #base_coerce

        impl #impl_generics #coerce_name #ty_generics for ::xpcom::interfaces::#base_name #where_clause {
            fn coerce_from(v: &#name #ty_generics) -> &Self {
                unsafe {
                    // Get the address of the VTable field. This should be a
                    // pointer to a pointer to a vtable, which we can then cast
                    // into a pointer to our interface.
                    &*(&(v.#vtable_field)
                        as *const *const _
                        as *const ::xpcom::interfaces::#base_name)
                }
            }
        }
    };

    Ok((qi, coerce))
}

fn check_generics(generics: &syn::Generics) -> Result<(), syn::Error> {
    for param in &generics.params {
        let tp = match param {
            syn::GenericParam::Type(tp) => tp,
            syn::GenericParam::Lifetime(lp) => bail!(
                @(lp),
                "Cannot #[derive(xpcom)] on types with lifetime parameters. \
                Implementors of XPCOM interfaces must not contain non-'static \
                lifetimes.",
            ),
            // XXX: Once const generics become stable, it may be as simple as
            // removing this bail! to support them.
            syn::GenericParam::Const(cp) => {
                bail!(@(cp), "Cannot #[derive(xpcom)] on types with const generics.")
            }
        };

        let mut static_lt = false;
        for bound in &tp.bounds {
            match bound {
                syn::TypeParamBound::Lifetime(lt) if lt.ident == "static" => {
                    static_lt = true;
                    break;
                }
                _ => {}
            }
        }

        if !static_lt {
            bail!(
                @(param),
                "Every generic parameter for xpcom implementation must have a \
                'static lifetime bound declared in the generics. Implicit \
                lifetime bounds or lifetime bounds in where clauses are not \
                detected by the macro and will be ignored. \
                Implementors of XPCOM interfaces must not contain non-'static \
                lifetimes.",
            );
        }
    }
    Ok(())
}

/// The root xpcom procedural macro definition.
fn xpcom(init: DeriveInput) -> Result<TokenStream, syn::Error> {
    check_generics(&init.generics)?;

    let bases = get_bases(&init.attrs)?;
    if bases.is_empty() {
        bail!(
            "Types with #[derive(xpcom)] must implement at least one \
             interface. Interfaces can be implemented by adding the \
             #[xpimplements(nsIFoo, nsIBar)] attribute to the struct \
             declaration."
        );
    }

    // Ensure that all our base interface methods have unique names.
    let mut method_names = HashMap::new();
    for base in &bases {
        for method in base.methods()? {
            if let Some(existing) = method_names.insert(method.name, base.name) {
                bail!(
                    "The method `{0}` is declared on both `{1}` and `{2}`,
                     but a Rust type cannot implement two methods with the \
                     same name. You can add the `[binaryname(Renamed{0})]` \
                     XPIDL attribute to one of the declarations to rename it.",
                    method.name,
                    existing,
                    base.name
                );
            }
        }
    }

    // Determine what reference count type to use, and generate the real struct.
    let refcnt_ty = get_refcnt_kind(&init.attrs)?;
    let real = gen_real_struct(&init, &bases, refcnt_ty)?;

    let name_init = &init.ident;
    let name = &real.ident;
    let coerce_name = format_ident!("{}Coerce", name);

    // Generate a VTable for each of the base interfaces.
    let mut vtables = Vec::new();
    for (idx, base) in bases.iter().enumerate() {
        vtables.push(gen_root_vtable(&real, base, idx)?);
    }

    // Generate the field initializers for the final struct, moving each field
    // out of the original __init struct.
    let inits = get_fields(&init)?.iter().map(|field| {
        let id = &field.ident;
        quote! { #id : __init.#id, }
    });

    let vis = &real.vis;

    // Generate the implementation for QueryInterface and Coerce.
    let mut seen = HashSet::new();
    let mut qi_impl = Vec::new();
    let mut coerce_impl = Vec::new();
    for base in &bases {
        let (qi, coerce) = gen_casts(
            &mut seen,
            base,
            &real,
            &coerce_name,
            &format_ident!("__base_{}", base.name),
        )?;
        qi_impl.push(qi);
        coerce_impl.push(coerce);
    }

    let size_for_logs = if real.generics.params.is_empty() {
        quote!(::std::mem::size_of::<Self>() as u32)
    } else {
        // Refcount logging requires all types with the same name to have the
        // same size, and generics aren't taken into account when creating our
        // name string, so we need to make sure that all possible instantiations
        // report the same size. To do that, we fake a size based on the number
        // of vtable pointers and the known refcount field.
        let fake_size_npointers = bases.len() + 1;
        quote!((::std::mem::size_of::<usize>() * #fake_size_npointers) as u32)
    };

    let (impl_generics, ty_generics, where_clause) = real.generics.split_for_impl();
    let name_for_logs = quote!(
        concat!(module_path!(), "::", stringify!(#name #ty_generics), "\0").as_ptr()
            as *const ::xpcom::reexports::libc::c_char
    );
    Ok(quote! {
        #real

        impl #impl_generics #name #ty_generics #where_clause {
            /// This method is used for
            fn allocate(__init: #name_init #ty_generics) -> ::xpcom::RefPtr<Self> {
                #[allow(unused_imports)]
                use ::xpcom::*;
                #[allow(unused_imports)]
                use ::xpcom::interfaces::*;
                #[allow(unused_imports)]
                use ::xpcom::reexports::{
                    libc, nsACString, nsAString, nsCString, nsString, nsresult
                };

                // Helper for asserting that for all instantiations, this
                // object has the 'static lifetime.
                fn xpcom_types_must_be_static<T: 'static>(t: &T) {}

                unsafe {
                    // NOTE: This is split into multiple lines to make the
                    // output more readable.
                    let value = #name {
                        #(#vtables)*
                        __refcnt: #refcnt_ty::new(),
                        #(#inits)*
                    };
                    let boxed = ::std::boxed::Box::new(value);
                    xpcom_types_must_be_static(&*boxed);
                    let raw = ::std::boxed::Box::into_raw(boxed);
                    ::xpcom::RefPtr::from_raw(raw).unwrap()
                }
            }

            /// Automatically generated implementation of AddRef for nsISupports.
            #vis unsafe fn AddRef(&self) -> ::xpcom::interfaces::nsrefcnt {
                let new = self.__refcnt.inc();
                ::xpcom::trace_refcnt::NS_LogAddRef(
                    self as *const _ as *mut ::xpcom::reexports::libc::c_void,
                    new,
                    #name_for_logs,
                    #size_for_logs,
                );
                new
            }

            /// Automatically generated implementation of Release for nsISupports.
            #vis unsafe fn Release(&self) -> ::xpcom::interfaces::nsrefcnt {
                let new = self.__refcnt.dec();
                ::xpcom::trace_refcnt::NS_LogRelease(
                    self as *const _ as *mut ::xpcom::reexports::libc::c_void,
                    new,
                    #name_for_logs,
                    #size_for_logs,
                );
                if new == 0 {
                    // dealloc
                    ::std::boxed::Box::from_raw(self as *const Self as *mut Self);
                }
                new
            }

            /// Automatically generated implementation of QueryInterface for
            /// nsISupports.
            #vis unsafe fn QueryInterface(&self,
                                          uuid: *const ::xpcom::nsIID,
                                          result: *mut *mut ::xpcom::reexports::libc::c_void)
                                          -> ::xpcom::reexports::nsresult {
                #[allow(unused_imports)]
                use ::xpcom::*;
                #[allow(unused_imports)]
                use ::xpcom::interfaces::*;

                #(#qi_impl)*

                ::xpcom::reexports::NS_ERROR_NO_INTERFACE
            }

            /// Perform a QueryInterface call on this object, attempting to
            /// dynamically cast it to the requested interface type. Returns
            /// Some(RefPtr<T>) if the cast succeeded, and None otherwise.
            #vis fn query_interface<XPCOM_InterfaceType: ::xpcom::XpCom>(&self)
                -> ::std::option::Option<::xpcom::RefPtr<XPCOM_InterfaceType>>
            {
                let mut ga = ::xpcom::GetterAddrefs::<XPCOM_InterfaceType>::new();
                unsafe {
                    if self.QueryInterface(&XPCOM_InterfaceType::IID, ga.void_ptr()).succeeded() {
                        ga.refptr()
                    } else {
                        None
                    }
                }
            }

            /// Coerce this type safely to any of the interfaces which it
            /// implements without `AddRef`ing it.
            #vis fn coerce<XPCOM_InterfaceType: #coerce_name #ty_generics>(&self) -> &XPCOM_InterfaceType {
                XPCOM_InterfaceType::coerce_from(self)
            }
        }

        /// This trait is implemented on the interface types which this
        /// `#[derive(xpcom)]` type can be safely ane cheaply coerced to using
        /// the `coerce` method.
        ///
        /// The trait and its method should usually not be used directly, but
        /// rather acts as a trait bound and implementation for the `coerce`
        /// methods.
        #[doc(hidden)]
        #vis trait #coerce_name #impl_generics #where_clause {
            /// Convert a value of the `#[derive(xpcom)]` type into the
            /// implementing interface type.
            fn coerce_from(v: &#name #ty_generics) -> &Self;
        }

        #(#coerce_impl)*

        unsafe impl #impl_generics ::xpcom::RefCounted for #name #ty_generics #where_clause {
            unsafe fn addref(&self) {
                self.AddRef();
            }

            unsafe fn release(&self) {
                self.Release();
            }
        }
    })
}

#[proc_macro_derive(xpcom, attributes(xpimplements, refcnt))]
pub fn xpcom_internal(input: proc_macro::TokenStream) -> proc_macro::TokenStream {
    let input = parse_macro_input!(input as DeriveInput);
    match xpcom(input) {
        Ok(ts) => ts.into(),
        Err(err) => err.to_compile_error().into(),
    }
}
