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
//!     pub fn Run(&self) -> nsresult {
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
//!     pub fn Run(&self) -> nsresult {
//!         // Fields defined on the `Init` struct will be directly on the
//!         // generated struct.
//!         println!("{}", self.i);
//!         NS_OK
//!     }
//! }
//! ```
//!
//! [`xpcom`]: ../xpcom/index.html
//! [`nsIRunnable`]: ../xpcom/struct.nsIRunnable.html
//! [`RefCounted`]: ../xpcom/struct.RefCounted.html
//! [`RefPtr`]: ../xpcom/struct.RefPtr.html

// NOTE: We use some really big quote! invocations, so we need a high recursion
// limit.
#![recursion_limit="256"]

#[macro_use]
extern crate quote;

#[macro_use]
extern crate syn;

extern crate proc_macro;

#[macro_use]
extern crate lazy_static;

use proc_macro::TokenStream;

use quote::{ToTokens, Tokens};

use syn::*;

use syn::punctuated::Punctuated;

use std::collections::{HashMap, HashSet};
use std::error::Error;

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
    fn base(&self) -> Result<Option<&'static Interface>, Box<Error>> {
        Ok(if let Some(base) = self.base {
            Some(*IFACES.get(base).ok_or_else(
                || format!("Base interface {} does not exist", base)
            )?)
        } else {
            None
        })
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
    fn to_tokens(&self, tokens: &mut Tokens) {
        match *self {
            RefcntKind::NonAtomic => quote!(xpcom::Refcnt).to_tokens(tokens),
            RefcntKind::Atomic => quote!(xpcom::AtomicRefcnt).to_tokens(tokens),
        }
    }
}

/// Scans through the attributes on a struct, and extracts the type of the refcount to use.
fn get_refcnt_kind(attrs: &[Attribute]) -> Result<RefcntKind, Box<Error>> {
    for attr in attrs {
        if let Some(Meta::NameValue(ref attr)) = attr.interpret_meta() {
            if attr.ident.as_ref() != "refcnt" {
                continue;
            }

            let value = if let Lit::Str(ref s) = attr.lit {
                s.value()
            } else {
                Err("Unexpected non-string value in #[refcnt]")?
            };

            return if value == "nonatomic" {
                Ok(RefcntKind::NonAtomic)
            } else if value == "atomic" {
                Ok(RefcntKind::Atomic)
            } else {
                Err("Unexpected value in #[refcnt]. \
                     Expected `nonatomic`, or `atomic`")?
            };
        }
    }

    Err("Expected #[refcnt] attribute")?
}

/// Scan the attributes looking for an #[xpimplements] attribute. The identifier
/// arguments passed to this attribute are the interfaces which the type wants to
/// directly implement.
fn get_bases(attrs: &[Attribute]) -> Result<Vec<&'static Interface>, Box<Error>> {
    let mut inherits = Vec::new();
    for attr in attrs {
        if let Some(Meta::List(ref attr)) = attr.interpret_meta() {
            if attr.ident.as_ref() != "xpimplements" {
                continue;
            }

            for item in &attr.nested {
                if let NestedMeta::Meta(Meta::Word(ref iface)) = *item {
                    if let Some(&iface) = IFACES.get(iface.as_ref()) {
                        inherits.push(iface);
                    } else {
                        Err(format!("Unexpected invalid base interface `{}` in \
                                     #[xpimplements(..)]", iface))?
                    }
                } else {
                    Err("Unexpected non-identifier in #[xpimplements(..)]")?
                }
            }
        }
    }
    Ok(inherits)
}

/// Extract the fields list from the input struct.
fn get_fields(di: &DeriveInput) -> Result<&Punctuated<Field, Token![,]>, Box<Error>> {
    match di.data {
        Data::Struct(DataStruct {
            fields: Fields::Named(ref named), ..
        }) => Ok(&named.named),
        _ => Err("The initializer struct must be a standard \
                  named value struct definition".into())
    }
}

/// Takes the `Init*` struct in, and generates a `DeriveInput` for the "real" struct.
fn gen_real_struct(init: &DeriveInput, bases: &[&Interface], refcnt_ty: RefcntKind) -> Result<DeriveInput, Box<Error>> {
    // Determine the name for the real struct based on the name of the
    // initializer struct's name.
    if !init.ident.as_ref().starts_with("Init") {
        Err("The target struct's name must begin with Init")?
    }
    let name: Ident = init.ident.as_ref()[4..].into();
    let vis = &init.vis;

    let bases = bases.iter().map(|base| {
        let ident = Ident::from(format!("__base_{}", base.name));
        let vtable = Ident::from(format!("{}VTable", base.name));
        quote!(#ident : *const xpcom::interfaces::#vtable)
     });

    let fields = get_fields(init)?;
    Ok(parse_quote! {
        #[repr(C)]
        #vis struct #name {
            #(#bases,)*
            __refcnt: #refcnt_ty,
            #fields
        }
     })
}

/// Generates the `extern "system"` methods which are actually included in the
/// VTable for the given interface.
///
/// These methods attempt to invoke the `recover_self` method to translate from
/// the passed-in raw pointer to the actual `&self` value, and it is expected to
/// be in scope.
fn gen_vtable_methods(iface: &Interface) -> Result<Tokens, Box<Error>> {
    let base_ty = Ident::from(iface.name);

    let base_methods = if let Some(base) = iface.base()? {
        gen_vtable_methods(base)?
    } else {
        quote!{}
    };

    let methods = iface.methods
        .map_err(|reason| format!("Interface {} cannot be implemented in rust \
                                   because {} is not supported yet", iface.name, reason))?;

    let mut method_defs = Vec::new();
    for method in methods {
        let name = Ident::from(method.name);
        let ret = syn::parse_str::<Type>(method.ret)?;

        let mut params = Vec::new();
        let mut args = Vec::new();
        for param in method.params {
            let name = Ident::from(param.name);
            let ty = syn::parse_str::<Type>(param.ty)?;

            params.push(quote!{#name : #ty,});
            args.push(quote!{#name,});
        }

        method_defs.push(quote!{
            unsafe extern "system" fn #name (this: *const #base_ty, #(#params)*) -> #ret {
                let lt = ();
                recover_self(this, &lt).#name(#(#args)*)
            }
        });
    }

    Ok(quote!{
        #base_methods
        #(#method_defs)*
    })
}

/// Generates the VTable for a given base interface. This assumes that the
/// implementations of each of the `extern "system"` methods are in scope.
fn gen_inner_vtable(iface: &Interface) -> Result<Tokens, Box<Error>> {
    let vtable_ty = Ident::from(format!("{}VTable", iface.name));

    let methods = iface.methods
        .map_err(|reason| format!("Interface {} cannot be implemented in rust \
                                   because {} is not supported yet", iface.name, reason))?;

    // Generate the vtable for the base interface.
    let base_vtable = if let Some(base) = iface.base()? {
        let vt = gen_inner_vtable(base)?;
        quote!{__base: #vt,}
    } else {
        quote!{}
    };

    // Include each of the method definitions for this interface.
    let vtable_init = methods.into_iter().map(|method| {
        let name = Ident::from(method.name);
        quote!{ #name : #name , }
    }).collect::<Vec<_>>();

    Ok(quote!(#vtable_ty {
        #base_vtable
        #(#vtable_init)*
    }))
}

fn gen_root_vtable(name: &Ident, base: &Interface) -> Result<Tokens, Box<Error>> {
    let field = Ident::from(format!("__base_{}", base.name));
    let vtable_ty = Ident::from(format!("{}VTable", base.name));
    let methods = gen_vtable_methods(base)?;
    let value = gen_inner_vtable(base)?;

    // Define the `recover_self` method. This performs an offset calculation to
    // recover a pointer to the original struct from a pointer to the given
    // VTable field.
    Ok(quote!{#field: {
        // NOTE: The &'a () dummy lifetime parameter is useful as it easily
        // allows the caller to limit the lifetime of the returned parameter
        // to a local lifetime, preventing the calling of methods with
        // receivers like `&'static self`.
        #[inline]
        unsafe fn recover_self<'a, T>(this: *const T, _: &'a ()) -> &'a #name {
            // Calculate the offset of the field in our struct.
            // XXX: Should we use the fact that our type is #[repr(C)] to avoid
            // this?
            let base = 0x1000;
            let member = &(*(0x1000 as *const #name)).#field
                as *const _ as usize;
            let off = member - base;

            // Offset the pointer by that offset.
            &*((this as usize - off) as *const #name)
        }

        // The method implementations which will be used to build the vtable.
        #methods

        // The actual VTable definition.
        static VTABLE: #vtable_ty = #value;
        &VTABLE
    },})
}

/// Generate the cast implementations. This generates the implementation details
/// for the `Coerce` trait, and the `QueryInterface` method. The first return
/// value is the `QueryInterface` implementation, and the second is the `Coerce`
/// implementation.
fn gen_casts(
    seen: &mut HashSet<&'static str>,
    iface: &Interface,
    name: &Ident,
    coerce_name: &Ident,
    vtable_field: &Ident,
) -> Result<(Tokens, Tokens), Box<Error>> {
    if !seen.insert(iface.name) {
        return Ok((quote!{}, quote!{}));
    }

    // Generate the cast implementations for the base interfaces.
    let (base_qi, base_coerce) = if let Some(base) = iface.base()? {
        gen_casts(
            seen,
            base,
            name,
            coerce_name,
            vtable_field,
        )?
    } else {
        (quote!{}, quote!{})
    };

    // Add the if statment to QueryInterface for the base class.
    let base_name = Ident::from(iface.name);

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
    let coerce = quote! {
        #base_coerce

        impl #coerce_name for ::xpcom::interfaces::#base_name {
            fn coerce_from(v: &#name) -> &Self {
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

/// The root xpcom procedural macro definition.
fn xpcom(init: DeriveInput) -> Result<Tokens, Box<Error>> {
    if !init.generics.params.is_empty() || !init.generics.where_clause.is_none() {
        return Err("Cannot #[derive(xpcom)] on a generic type, due to \
                    rust limitations. It is not possible to instantiate \
                    a static with a generic type parameter, meaning that \
                    generic types cannot have their VTables instantiated \
                    correctly.".into());
    }

    let bases = get_bases(&init.attrs)?;
    if bases.is_empty() {
        return Err("Types with #[derive(xpcom)] must implement at least one \
                    interface. Interfaces can be implemented by adding the \
                    #[xpimplements(nsIFoo, nsIBar)] attribute to the struct \
                    declaration.".into());
    }

    // Determine what reference count type to use, and generate the real struct.
    let refcnt_ty = get_refcnt_kind(&init.attrs)?;
    let real = gen_real_struct(&init, &bases, refcnt_ty)?;

    let name_init = &init.ident;
    let name = &real.ident;
    let coerce_name = Ident::from(format!("{}Coerce", name.as_ref()));

    // Generate a VTable for each of the base interfaces.
    let mut vtables = Vec::new();
    for base in &bases {
        vtables.push(gen_root_vtable(name, base)?);
    }

    // Generate the field initializers for the final struct, moving each field
    // out of the original __init struct.
    let inits = get_fields(&init)?.iter().map(|field| {
        let id = &field.ident;
        quote!{ #id : __init.#id, }
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
            name,
            &coerce_name,
            &Ident::from(format!("__base_{}", base.name)),
        )?;
        qi_impl.push(qi);
        coerce_impl.push(coerce);
    }

    Ok(quote! {
        #real

        impl #name {
            /// This method is used for
            fn allocate(__init: #name_init) -> ::xpcom::RefPtr<Self> {
                #[allow(unused_imports)]
                use ::xpcom::*;
                #[allow(unused_imports)]
                use ::xpcom::interfaces::*;
                #[allow(unused_imports)]
                use ::xpcom::reexports::{libc, nsACString, nsAString, nsresult};

                unsafe {
                    // NOTE: This is split into multiple lines to make the
                    // output more readable.
                    let value = #name {
                        #(#vtables)*
                        __refcnt: #refcnt_ty::new(),
                        #(#inits)*
                    };
                    let boxed = ::std::boxed::Box::new(value);
                    let raw = ::std::boxed::Box::into_raw(boxed);
                    ::xpcom::RefPtr::from_raw(raw).unwrap()
                }
            }

            /// Automatically generated implementation of AddRef for nsISupports.
            #vis unsafe fn AddRef(&self) -> ::xpcom::interfaces::nsrefcnt {
                self.__refcnt.inc()
            }

            /// Automatically generated implementation of Release for nsISupports.
            #vis unsafe fn Release(&self) -> ::xpcom::interfaces::nsrefcnt {
                let new = self.__refcnt.dec();
                if new == 0 {
                    // XXX: dealloc
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
            #vis fn query_interface<T: ::xpcom::XpCom>(&self) ->
                ::std::option::Option<::xpcom::RefPtr<T>>
            {
                let mut ga = ::xpcom::GetterAddrefs::<T>::new();
                unsafe {
                    if ::xpcom::reexports::NsresultExt::succeeded(
                        self.QueryInterface(&T::IID, ga.void_ptr()),
                    ) {
                        ga.refptr()
                    } else {
                        None
                    }
                }
            }

            /// Coerce this type safely to any of the interfaces which it
            /// implements without `AddRef`ing it.
            #vis fn coerce<T: #coerce_name>(&self) -> &T {
                T::coerce_from(self)
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
        #vis trait #coerce_name {
            /// Convert a value of the `#[derive(xpcom)]` type into the
            /// implementing interface type.
            fn coerce_from(v: &#name) -> &Self;
        }

        #(#coerce_impl)*

        unsafe impl ::xpcom::RefCounted for #name {
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
pub fn xpcom_internal(input: TokenStream) -> TokenStream {
    xpcom(parse(input).expect("Invalid derive input"))
        .expect("#[derive(xpcom)] failed").into()
}
