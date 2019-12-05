#![recursion_limit = "512"]

extern crate proc_macro;
extern crate proc_macro2;
#[macro_use]
extern crate syn;
#[macro_use]
extern crate quote;

use std::iter::{self, FromIterator};
use syn::spanned::Spanned;
use syn::visit::Visit;
use syn::fold::Fold;
use quote::ToTokens;
use proc_macro2::Span;


/// From `procedural_masquerade` crate
#[doc(hidden)]
fn _extract_input(derive_input: &str) -> &str {
	let mut input = derive_input;

	for expected in &["#[allow(unused)]", "enum", "ProceduralMasqueradeDummyType", "{", "Input", "=", "(0,", "stringify!", "("] {
		input = input.trim_start();
		assert!(input.starts_with(expected), "expected prefix {:?} not found in {:?}", expected, derive_input);
		input = &input[expected.len()..];
	}

	for expected in [")", ").0,", "}"].iter().rev() {
		input = input.trim_end();
		assert!(input.ends_with(expected), "expected suffix {:?} not found in {:?}", expected, derive_input);
		let end = input.len() - expected.len();
		input = &input[..end];
	}

	input
}


#[doc(hidden)]
#[allow(non_snake_case)]
#[proc_macro_derive(__rental_traits)]
pub fn __rental_traits(input: proc_macro::TokenStream) -> proc_macro::TokenStream {
	let mut tokens = proc_macro2::TokenStream::new();

	let max_arity = _extract_input(&input.to_string()).parse::<usize>().expect("Input must be an integer literal.");
	write_rental_traits(&mut tokens, max_arity);

	tokens.into()
}


#[doc(hidden)]
#[allow(non_snake_case)]
#[proc_macro_derive(__rental_structs_and_impls)]
pub fn __rental_structs_and_impls(input: proc_macro::TokenStream) -> proc_macro::TokenStream {
	let mut tokens = proc_macro2::TokenStream::new();

	for item in syn::parse_str::<syn::File>(_extract_input(&input.to_string())).expect("Failed to parse items in module body.").items.iter() {
		match *item {
			syn::Item::Use(..) => {
				item.to_tokens(&mut tokens);
			},
			syn::Item::Type(..) => {
				item.to_tokens(&mut tokens);
			},
			syn::Item::Struct(ref struct_info) => {
				write_rental_struct_and_impls(&mut tokens, &struct_info);
			},
			_ => panic!("Item must be a `use` or `struct`."),
		}
	}

	tokens.into()
}


fn write_rental_traits(tokens: &mut proc_macro2::TokenStream, max_arity: usize) {
	let call_site: Span = Span::call_site();

	let mut lt_params = vec![syn::LifetimeDef::new(syn::Lifetime::new("'a0", call_site))];

	for arity in 2 .. max_arity + 1 {
		let trait_ident = &syn::Ident::new(&format!("Rental{}", arity), call_site);
		let lt_param = syn::LifetimeDef::new(syn::Lifetime::new(&format!("'a{}", arity - 1), call_site));
		lt_params[arity - 2].bounds.push(lt_param.lifetime.clone());
		lt_params.push(lt_param);

		let lt_params_iter = &lt_params;
		quote!(
			#[doc(hidden)]
			pub unsafe trait #trait_ident<#(#lt_params_iter),*> {
				type Borrow;
				type BorrowMut;
			}
		).to_tokens(tokens);
	}
}


fn write_rental_struct_and_impls(tokens: &mut proc_macro2::TokenStream, struct_info: &syn::ItemStruct) {
	let def_site: Span = Span::call_site(); // FIXME: hygiene
	let call_site: Span = Span::call_site();

	if let syn::Visibility::Inherited = struct_info.vis {
		panic!("Struct `{}` must be non-private.", struct_info.ident);
	}

	let attribs = get_struct_attribs(struct_info);
	let (fields, fields_brace) = prepare_fields(struct_info);

	let struct_generics = &struct_info.generics;
	let struct_rlt_args = &fields.iter().fold(Vec::new(), |mut rlt_args, field| { rlt_args.extend(field.self_rlt_args.iter()); rlt_args });
	if let Some(collide) = struct_rlt_args.iter().find(|rlt_arg| struct_generics.lifetimes().any(|lt_def| lt_def.lifetime == ***rlt_arg)) {
		panic!("Struct `{}` lifetime parameter `{}` collides with rental lifetime.", struct_info.ident, collide);
	}
	let last_rlt_arg = &struct_rlt_args[struct_rlt_args.len() - 1];
	let static_rlt_args = &iter::repeat(syn::Lifetime::new("'static", def_site)).take(struct_rlt_args.len()).collect::<Vec<_>>();
	let self_rlt_args = &iter::repeat(syn::Lifetime::new("'__s", def_site)).take(struct_rlt_args.len()).collect::<Vec<_>>();

	let item_ident = &struct_info.ident;
	let item_vis = &struct_info.vis;
	let item_ident_str = syn::LitStr::new(&item_ident.to_string(), item_ident.span());

	let (struct_impl_params, struct_impl_args, struct_where_clause) = struct_generics.split_for_impl();
	let where_extra = if let Some(ref struct_where_clause) = struct_where_clause {
		if struct_where_clause.predicates.is_empty() {
			quote!(where)
		} else if struct_where_clause.predicates.trailing_punct() {
			quote!()
		} else {
			quote!(,)
		}
	} else {
		quote!(where)
	};
	let struct_lt_params = &struct_generics.lifetimes().collect::<Vec<_>>();
	let struct_nonlt_params = &struct_generics.params.iter().filter(|param| if let syn::GenericParam::Lifetime(..) = **param { false } else { true }).collect::<Vec<_>>();
	let struct_lt_args = &struct_lt_params.iter().map(|lt_def| &lt_def.lifetime).collect::<Vec<_>>();

	let struct_nonlt_args = &struct_nonlt_params.iter().map(|param| match **param {
		syn::GenericParam::Type(ref ty) => &ty.ident,
		syn::GenericParam::Const(ref co) => &co.ident,
		syn::GenericParam::Lifetime(..) => unreachable!(),
	}).collect::<Vec<_>>();

	let rental_trait_ident = syn::Ident::new(&format!("Rental{}", struct_rlt_args.len()), def_site);
	let field_idents = &fields.iter().map(|field| &field.name).collect::<Vec<_>>();
	let local_idents = field_idents;
	let field_ident_strs = &field_idents.iter().map(|ident| syn::LitStr::new(&ident.to_string(), ident.span())).collect::<Vec<_>>();

	let (ref self_ref_param, ref self_lt_ref_param, ref self_mut_param, ref self_move_param, ref self_arg) = if attribs.is_deref_suffix {
		(quote!(_self: &Self), quote!(_self: &'__s Self),  quote!(_self: &mut Self), quote!(_self: Self), quote!(_self))
	} else {
		(quote!(&self), quote!(&'__s self), quote!(&mut self), quote!(self), quote!(self))
	};

	let borrow_ident = syn::Ident::new(&(struct_info.ident.to_string() + "_Borrow"), call_site);
	let borrow_mut_ident = syn::Ident::new(&(struct_info.ident.to_string() + "_BorrowMut"), call_site);
	let borrow_quotes = &make_borrow_quotes(self_arg, &fields, attribs.is_rental_mut);
	let borrow_tys = &borrow_quotes.iter().map(|&BorrowQuotes{ref ty, ..}| ty).collect::<Vec<_>>();
	let borrow_ty_hacks = &borrow_quotes.iter().map(|&BorrowQuotes{ref ty_hack, ..}| ty_hack).collect::<Vec<_>>();
	let borrow_suffix_ty = &borrow_tys[fields.len() - 1];
	let borrow_exprs = &borrow_quotes.iter().map(|&BorrowQuotes{ref expr, ..}| expr).collect::<Vec<_>>();
	let borrow_suffix_expr = &borrow_exprs[fields.len() - 1];
	let borrow_mut_tys = &borrow_quotes.iter().map(|&BorrowQuotes{ref mut_ty, ..}| mut_ty).collect::<Vec<_>>();
	let borrow_mut_ty_hacks = &borrow_quotes.iter().map(|&BorrowQuotes{ref mut_ty_hack, ..}| mut_ty_hack).collect::<Vec<_>>();
	let borrow_mut_suffix_ty = &borrow_mut_tys[fields.len() - 1];
	let borrow_mut_exprs = &borrow_quotes.iter().map(|&BorrowQuotes{ref mut_expr, ..}| mut_expr).collect::<Vec<_>>();
	let borrow_mut_suffix_expr = &borrow_mut_exprs[fields.len() - 1];

	let struct_rlt_params = &struct_rlt_args.iter().zip(struct_rlt_args.iter().skip(1)).map(|(rlt_arg, next_rlt_arg)| {
		syn::LifetimeDef {
			attrs: Vec::with_capacity(0),
			lifetime: (*rlt_arg).clone(),
			bounds: vec![(*next_rlt_arg).clone()].into_iter().collect(),
			colon_token: Default::default(),
		}
	}).chain(Some(syn::LifetimeDef {
			attrs: Vec::with_capacity(0),
			lifetime: struct_rlt_args[struct_rlt_args.len() - 1].clone(),
			bounds: syn::punctuated::Punctuated::new(),
			colon_token: Default::default(),
	})).collect::<Vec<_>>();

	let borrow_lt_params = &struct_rlt_params.iter().cloned()
		.chain( struct_lt_params.iter().map(|lt_def| {
			let mut lt_def = (*lt_def).clone();
			lt_def.bounds.push(struct_rlt_args[0].clone());
			lt_def
		})).collect::<Vec<_>>();

	let field_tys = &fields.iter().map(|field| &field.erased.ty).collect::<Vec<_>>();
	let head_ident = &local_idents[0];
	let head_ident_rep = &iter::repeat(&head_ident).take(fields.len() - 1).collect::<Vec<_>>();
	let head_ty = &fields[0].orig_ty;
	let tail_tys = &field_tys.iter().skip(1).collect::<Vec<_>>();
	let tail_idents = &local_idents.iter().skip(1).collect::<Vec<_>>();
	let tail_closure_tys = &fields.iter().skip(1).map(|field| syn::Ident::new(&format!("__F{}", field.name), call_site)).collect::<Vec<_>>();
	let tail_closure_quotes = make_tail_closure_quotes(&fields, borrow_quotes, attribs.is_rental_mut);
	let tail_closure_bounds = &tail_closure_quotes.iter().map(|&ClosureQuotes{ref bound, ..}| bound).collect::<Vec<_>>();
	let tail_closure_exprs = &tail_closure_quotes.iter().map(|&ClosureQuotes{ref expr, ..}| expr).collect::<Vec<_>>();
	let tail_try_closure_bounds = &tail_closure_quotes.iter().map(|&ClosureQuotes{ref try_bound, ..}| try_bound).collect::<Vec<_>>();
	let tail_try_closure_exprs = &tail_closure_quotes.iter().map(|&ClosureQuotes{ref try_expr, ..}| try_expr).collect::<Vec<_>>();
	let suffix_ident = &field_idents[fields.len() - 1];
	let suffix_ty = &fields[fields.len() - 1].erased.ty;
	let suffix_rlt_args = &fields[fields.len() - 1].self_rlt_args.iter().chain(fields[fields.len() - 1].used_rlt_args.iter()).collect::<Vec<_>>();
	let suffix_ident_str = syn::LitStr::new(&suffix_ident.to_string(), suffix_ident.span());
	let suffix_orig_ty = &fields[fields.len() - 1].orig_ty;

	let rstruct = syn::ItemStruct{
		ident: struct_info.ident.clone(),
		vis: item_vis.clone(),
		attrs: attribs.doc.clone(),
		fields: syn::Fields::Named(syn::FieldsNamed{
			brace_token: fields_brace,
			named: fields.iter().enumerate().map(|(i, field)| {
				let mut field_erased = field.erased.clone();
				if i < fields.len() - 1 {
					field_erased.attrs.push(parse_quote!(#[allow(dead_code)]));
				}
				field_erased
			}).rev().collect(),
		}),
		generics: struct_info.generics.clone(),
		struct_token: struct_info.struct_token,
		semi_token: None,
	};

	let borrow_struct = syn::ItemStruct{
		ident: borrow_ident.clone(),
		vis: item_vis.clone(),
		attrs: Vec::with_capacity(0),
		fields: syn::Fields::Named(syn::FieldsNamed{
			brace_token: Default::default(),
			named: fields.iter().zip(borrow_tys).enumerate().map(|(idx, (field, borrow_ty))| {
				let mut field = field.erased.clone();
				field.vis = if !attribs.is_rental_mut || idx == fields.len() - 1 { item_vis.clone() } else { syn::Visibility::Inherited };
				field.ty = syn::parse::<syn::Type>((**borrow_ty).clone().into()).unwrap();
				field
			}).collect(),
		}),
		generics: {
			let mut gen = struct_generics.clone();
			let params = borrow_lt_params.iter().map(|lt| syn::GenericParam::Lifetime(lt.clone()))
				.chain(gen.type_params().map(|p| syn::GenericParam::Type(p.clone())))
				.chain(gen.const_params().map(|p| syn::GenericParam::Const(p.clone())))
				.collect();
			gen.params = params;
			gen
		},
		struct_token: Default::default(),
		semi_token: None,
	};

	let borrow_mut_struct = syn::ItemStruct{
		ident: borrow_mut_ident.clone(),
		vis: item_vis.clone(),
		attrs: Vec::with_capacity(0),
		fields: syn::Fields::Named(syn::FieldsNamed{
			brace_token: Default::default(),
			named: fields.iter().zip(borrow_mut_tys).enumerate().map(|(idx, (field, borrow_mut_ty))| {
				let mut field = field.erased.clone();
				field.vis = if idx == fields.len() - 1 || !attribs.is_rental_mut { (*item_vis).clone() } else { syn::Visibility::Inherited };
				field.ty = syn::parse::<syn::Type>((**borrow_mut_ty).clone().into()).unwrap();
				field
			}).collect(),
		}),
		generics: {
			let mut gen = struct_generics.clone();
			let params = borrow_lt_params.iter().map(|lt| syn::GenericParam::Lifetime(lt.clone()))
				.chain(gen.type_params().map(|p| syn::GenericParam::Type(p.clone())))
				.chain(gen.const_params().map(|p| syn::GenericParam::Const(p.clone())))
				.collect();
			gen.params = params;
			gen
		},
		struct_token: Default::default(),
		semi_token: None,
	};

	let prefix_tys = &fields.iter().map(|field| &field.erased.ty).take(fields.len() - 1).collect::<Vec<_>>();
	let static_assert_prefix_stable_derefs = &prefix_tys.iter().map(|field| {
		if attribs.is_rental_mut {
			quote_spanned!(field.span()/*.resolved_at(def_site)*/ => __rental_prelude::static_assert_mut_stable_deref::<#field>();)
		} else {
			quote_spanned!(field.span()/*.resolved_at(def_site)*/ => __rental_prelude::static_assert_stable_deref::<#field>();)
		}
	}).collect::<Vec<_>>();
	let prefix_clone_traits = iter::repeat(quote!(__rental_prelude::CloneStableDeref)).take(prefix_tys.len());

	let struct_span = struct_info.span()/*.resolved_at(def_site)*/;
	let suffix_ty_span = suffix_ty.span()/*.resolved_at(def_site)*/;

	quote_spanned!(struct_span =>
		#rstruct

		/// Shared borrow of a rental struct.
		#[allow(non_camel_case_types, non_snake_case, dead_code)]
		#borrow_struct

		/// Mutable borrow of a rental struct.
		#[allow(non_camel_case_types, non_snake_case, dead_code)]
		#borrow_mut_struct
	).to_tokens(tokens);

	quote_spanned!(struct_span =>
		#[allow(dead_code)]
		impl<#(#borrow_lt_params,)* #(#struct_nonlt_params),*> #borrow_ident<#(#struct_rlt_args,)* #(#struct_lt_args,)* #(#struct_nonlt_args),*> #struct_where_clause {
			fn unify_tys_hack(#(#local_idents: #borrow_ty_hacks),*) -> #borrow_ident<#(#struct_rlt_args,)* #(#struct_lt_args,)* #(#struct_nonlt_args),*> {
				#borrow_ident {
					#(#field_idents: #local_idents,)*
				}
			}
		}
	).to_tokens(tokens);

	quote_spanned!(struct_span =>
		#[allow(dead_code)]
		impl<#(#borrow_lt_params,)* #(#struct_nonlt_params),*> #borrow_mut_ident<#(#struct_rlt_args,)* #(#struct_lt_args,)* #(#struct_nonlt_args),*> #struct_where_clause {
			fn unify_tys_hack(#(#local_idents: #borrow_mut_ty_hacks),*) -> #borrow_mut_ident<#(#struct_rlt_args,)* #(#struct_lt_args,)* #(#struct_nonlt_args),*> {
				#borrow_mut_ident {
					#(#field_idents: #local_idents,)*
				}
			}
		}
	).to_tokens(tokens);

	quote_spanned!(struct_span =>
		unsafe impl<#(#borrow_lt_params,)* #(#struct_nonlt_params),*> __rental_prelude::#rental_trait_ident<#(#struct_rlt_args),*> for #item_ident #struct_impl_args #struct_where_clause {
			type Borrow = #borrow_ident<#(#struct_rlt_args,)* #(#struct_lt_args,)* #(#struct_nonlt_args),*>;
			type BorrowMut = #borrow_mut_ident<#(#struct_rlt_args,)* #(#struct_lt_args,)* #(#struct_nonlt_args),*>;
		}
	).to_tokens(tokens);

	quote_spanned!(struct_span =>
		#[allow(dead_code, unused_mut, unused_unsafe, non_camel_case_types)]
		impl #struct_impl_params #item_ident #struct_impl_args #struct_where_clause {
			/// Create a new instance of the rental struct.
			///
			/// The first argument provided is the head, followed by a series of closures, one for each tail field. Each of these closures will receive, as its arguments, a borrow of the previous field, followed by borrows of the remaining prefix fields if the struct is a shared rental. If the struct is a mutable rental, only the immediately preceding field is passed.
			pub fn new<#(#tail_closure_tys),*>(
				mut #head_ident: #head_ty,
				#(#tail_idents: #tail_closure_tys),*
			) -> Self where #(#tail_closure_tys: #tail_closure_bounds),*
			{
				#(#static_assert_prefix_stable_derefs)*

				#(let mut #tail_idents = unsafe { __rental_prelude::transmute::<_, #tail_tys>(#tail_closure_exprs) };)*

				#item_ident {
					#(#field_idents: #local_idents,)*
				}
			}

			/// Attempt to create a new instance of the rental struct.
			///
			/// As `new`, but each closure returns a `Result`. If one of them fails, execution is short-circuited and a tuple of the error and the original head value is returned to you.
			pub fn try_new<#(#tail_closure_tys,)* __E>(
				mut #head_ident: #head_ty,
				#(#tail_idents: #tail_closure_tys),*
			) -> __rental_prelude::RentalResult<Self, __E, #head_ty> where
				#(#tail_closure_tys: #tail_try_closure_bounds,)*
			{
				#(#static_assert_prefix_stable_derefs)*

				#(let mut #tail_idents = {
					let temp = #tail_try_closure_exprs.map(|t| unsafe { __rental_prelude::transmute::<_, #tail_tys>(t) });
					match temp {
						__rental_prelude::Result::Ok(t) => t,
						__rental_prelude::Result::Err(e) => return __rental_prelude::Result::Err(__rental_prelude::RentalError(e.into(), #head_ident_rep)),
					}
				};)*

				__rental_prelude::Result::Ok(#item_ident {
					#(#field_idents: #local_idents,)*
				})
			}

			/// Attempt to create a new instance of the rental struct.
			///
			/// As `try_new`, but only the error value is returned upon failure; the head value is dropped. This method interacts more smoothly with existing error conversions.
			pub fn try_new_or_drop<#(#tail_closure_tys,)* __E>(
				mut #head_ident: #head_ty,
				#(#tail_idents: #tail_closure_tys),*
			) -> __rental_prelude::Result<Self, __E> where
				#(#tail_closure_tys: #tail_try_closure_bounds,)*
			{
				#(#static_assert_prefix_stable_derefs)*

				#(let mut #tail_idents = {
					let temp = #tail_try_closure_exprs.map(|t| unsafe { __rental_prelude::transmute::<_, #tail_tys>(t) });
					match temp {
						__rental_prelude::Result::Ok(t) => t,
						__rental_prelude::Result::Err(e) => return __rental_prelude::Result::Err(e.into()),
					}
				};)*

				__rental_prelude::Result::Ok(#item_ident {
					#(#field_idents: #local_idents,)*
				})
			}

			/// Return lifetime-erased shared borrows of the fields of the struct.
			///
			/// This is unsafe because the erased lifetimes are fake. Use this only if absolutely necessary and be very mindful of what the true lifetimes are.
			pub unsafe fn all_erased(#self_ref_param) -> <Self as __rental_prelude::#rental_trait_ident>::Borrow {
				#borrow_ident::unify_tys_hack(#(__rental_prelude::transmute(#borrow_exprs),)*)
			}

			/// Return a lifetime-erased mutable borrow of the suffix of the struct.
			///
			/// This is unsafe because the erased lifetimes are fake. Use this only if absolutely necessary and be very mindful of what the true lifetimes are.
			pub unsafe fn all_mut_erased(#self_mut_param) -> <Self as __rental_prelude::#rental_trait_ident>::BorrowMut {
				#borrow_mut_ident::unify_tys_hack(#(__rental_prelude::transmute(#borrow_mut_exprs),)*)
			}

			/// Execute a closure on the shared suffix of the struct.
			///
			/// The closure may return any value not bounded by one of the special rental lifetimes of the struct.
			pub fn rent<__F, __R>(#self_ref_param, f: __F) -> __R where
				__F: for<#(#suffix_rlt_args,)*> __rental_prelude::FnOnce(#borrow_suffix_ty) -> __R,
				__R: #(#struct_lt_args +)*,
			{
				f(#borrow_suffix_expr)
			}

			/// Execute a closure on the mutable suffix of the struct.
			///
			/// The closure may return any value not bounded by one of the special rental lifetimes of the struct.
			pub fn rent_mut<__F, __R>(#self_mut_param, f: __F) -> __R where
				__F: for<#(#suffix_rlt_args,)*> __rental_prelude::FnOnce(#borrow_mut_suffix_ty) -> __R,
				__R: #(#struct_lt_args +)*,
			{
				f(#borrow_mut_suffix_expr)
			}

			/// Return a shared reference from the shared suffix of the struct.
			///
			/// This is a subtle variation of `rent` where it is legal to return a reference bounded by a rental lifetime, because that lifetime is reborrowed away before it is returned to you.
			pub fn ref_rent<__F, __R>(#self_ref_param, f: __F) -> &__R where
				__F: for<#(#suffix_rlt_args,)*> __rental_prelude::FnOnce(#borrow_suffix_ty) -> &#last_rlt_arg __R,
				__R: ?Sized //#(#struct_lt_args +)*,
			{
				f(#borrow_suffix_expr)
			}

			/// Optionally return a shared reference from the shared suffix of the struct.
			///
			/// This is a subtle variation of `rent` where it is legal to return a reference bounded by a rental lifetime, because that lifetime is reborrowed away before it is returned to you.
			pub fn maybe_ref_rent<__F, __R>(#self_ref_param, f: __F) -> __rental_prelude::Option<&__R> where
				__F: for<#(#suffix_rlt_args,)*> __rental_prelude::FnOnce(#borrow_suffix_ty) -> __rental_prelude::Option<&#last_rlt_arg __R>,
				__R: ?Sized //#(#struct_lt_args +)*,
			{
				f(#borrow_suffix_expr)
			}

			/// Try to return a shared reference from the shared suffix of the struct, or an error on failure.
			///
			/// This is a subtle variation of `rent` where it is legal to return a reference bounded by a rental lifetime, because that lifetime is reborrowed away before it is returned to you.
			pub fn try_ref_rent<__F, __R, __E>(#self_ref_param, f: __F) -> __rental_prelude::Result<&__R, __E> where
				__F: for<#(#suffix_rlt_args,)*> __rental_prelude::FnOnce(#borrow_suffix_ty) -> __rental_prelude::Result<&#last_rlt_arg __R, __E>,
				__R: ?Sized //#(#struct_lt_args +)*,
			{
				f(#borrow_suffix_expr)
			}

			/// Return a mutable reference from the mutable suffix of the struct.
			///
			/// This is a subtle variation of `rent_mut` where it is legal to return a reference bounded by a rental lifetime, because that lifetime is reborrowed away before it is returned to you.
			pub fn ref_rent_mut<__F, __R>(#self_mut_param, f: __F) -> &mut __R where
				__F: for<#(#suffix_rlt_args,)*> __rental_prelude::FnOnce(#borrow_mut_suffix_ty) -> &#last_rlt_arg  mut __R,
				__R: ?Sized //#(#struct_lt_args +)*,
			{
				f(#borrow_mut_suffix_expr)
			}

			/// Optionally return a mutable reference from the mutable suffix of the struct.
			///
			/// This is a subtle variation of `rent_mut` where it is legal to return a reference bounded by a rental lifetime, because that lifetime is reborrowed away before it is returned to you.
			pub fn maybe_ref_rent_mut<__F, __R>(#self_mut_param, f: __F) -> __rental_prelude::Option<&mut __R> where
				__F: for<#(#suffix_rlt_args,)*> __rental_prelude::FnOnce(#borrow_mut_suffix_ty) -> __rental_prelude::Option<&#last_rlt_arg  mut __R>,
				__R: ?Sized //#(#struct_lt_args +)*,
			{
				f(#borrow_mut_suffix_expr)
			}

			/// Try to return a mutable reference from the mutable suffix of the struct, or an error on failure.
			///
			/// This is a subtle variation of `rent_mut` where it is legal to return a reference bounded by a rental lifetime, because that lifetime is reborrowed away before it is returned to you.
			pub fn try_ref_rent_mut<__F, __R, __E>(#self_mut_param, f: __F) -> __rental_prelude::Result<&mut __R, __E> where
				__F: for<#(#suffix_rlt_args,)*> __rental_prelude::FnOnce(#borrow_mut_suffix_ty) -> __rental_prelude::Result<&#last_rlt_arg  mut __R, __E>,
				__R: ?Sized //#(#struct_lt_args +)*,
			{
				f(#borrow_mut_suffix_expr)
			}

			/// Drop the rental struct and return the original head value to you.
			pub fn into_head(#self_move_param) -> #head_ty {
				let Self{#head_ident, ..} = #self_arg;
				#head_ident
			}
		}
	).to_tokens(tokens);

	if !attribs.is_rental_mut {
		quote_spanned!(struct_span =>
			#[allow(dead_code)]
			impl #struct_impl_params #item_ident #struct_impl_args #struct_where_clause {
				/// Return a shared reference to the head field of the struct.
				pub fn head(#self_ref_param) -> &<#head_ty as __rental_prelude::Deref>::Target {
					&*#self_arg.#head_ident
				}

				/// Execute a closure on shared borrows of the fields of the struct.
				///
				/// The closure may return any value not bounded by one of the special rental lifetimes of the struct.
				pub fn rent_all<__F, __R>(#self_ref_param, f: __F) -> __R where
					__F: for<#(#struct_rlt_args,)*> __rental_prelude::FnOnce(#borrow_ident<#(#struct_rlt_args,)* #(#struct_lt_args,)* #(#struct_nonlt_args),*>) -> __R,
					__R: #(#struct_lt_args +)*,
				{
					f(unsafe { #item_ident::all_erased(#self_arg) })
				}

				/// Return a shared reference from shared borrows of the fields of the struct.
				///
				/// This is a subtle variation of `rent_all` where it is legal to return a reference bounded by a rental lifetime, because that lifetime is reborrowed away before it is returned to you.
				pub fn ref_rent_all<__F, __R>(#self_ref_param, f: __F) -> &__R where
					__F: for<#(#struct_rlt_args,)*> __rental_prelude::FnOnce(#borrow_ident<#(#struct_rlt_args,)* #(#struct_lt_args,)* #(#struct_nonlt_args),*>) -> &#last_rlt_arg __R,
					__R: ?Sized //#(#struct_lt_args +)*,
				{
					f(unsafe { #item_ident::all_erased(#self_arg) })
				}

				/// Optionally return a shared reference from shared borrows of the fields of the struct.
				///
				/// This is a subtle variation of `rent_all` where it is legal to return a reference bounded by a rental lifetime, because that lifetime is reborrowed away before it is returned to you.
				pub fn maybe_ref_rent_all<__F, __R>(#self_ref_param, f: __F) -> __rental_prelude::Option<&__R> where
					__F: for<#(#struct_rlt_args,)*> __rental_prelude::FnOnce(#borrow_ident<#(#struct_rlt_args,)* #(#struct_lt_args,)* #(#struct_nonlt_args),*>) -> __rental_prelude::Option<&#last_rlt_arg __R>,
					__R: ?Sized //#(#struct_lt_args +)*,
				{
					f(unsafe { #item_ident::all_erased(#self_arg) })
				}

				/// Try to return a shared reference from shared borrows of the fields of the struct, or an error on failure.
				///
				/// This is a subtle variation of `rent_all` where it is legal to return a reference bounded by a rental lifetime, because that lifetime is reborrowed away before it is returned to you.
				pub fn try_ref_rent_all<__F, __R, __E>(#self_ref_param, f: __F) -> __rental_prelude::Result<&__R, __E> where
					__F: for<#(#struct_rlt_args,)*> __rental_prelude::FnOnce(#borrow_ident<#(#struct_rlt_args,)* #(#struct_lt_args,)* #(#struct_nonlt_args),*>) -> __rental_prelude::Result<&#last_rlt_arg __R, __E>,
					__R: ?Sized //#(#struct_lt_args +)*,
				{
					f(unsafe { #item_ident::all_erased(#self_arg) })
				}

				/// Execute a closure on shared borrows of the prefix fields and a mutable borrow of the suffix field of the struct.
				///
				/// The closure may return any value not bounded by one of the special rental lifetimes of the struct.
				pub fn rent_all_mut<__F, __R>(#self_mut_param, f: __F) -> __R where
					__F: for<#(#struct_rlt_args,)*> __rental_prelude::FnOnce(#borrow_mut_ident<#(#struct_rlt_args,)* #(#struct_lt_args,)* #(#struct_nonlt_args),*>) -> __R,
					__R: #(#struct_lt_args +)*,
				{
					f(unsafe { #item_ident::all_mut_erased(#self_arg) })
				}

				/// Return a mutable reference from shared borrows of the prefix fields and a mutable borrow of the suffix field of the struct.
				///
				/// This is a subtle variation of `rent_all_mut` where it is legal to return a reference bounded by a rental lifetime, because that lifetime is reborrowed away before it is returned to you.
				pub fn ref_rent_all_mut<__F, __R>(#self_mut_param, f: __F) -> &mut __R where
					__F: for<#(#struct_rlt_args,)*> __rental_prelude::FnOnce(#borrow_mut_ident<#(#struct_rlt_args,)* #(#struct_lt_args,)* #(#struct_nonlt_args),*>) -> &#last_rlt_arg mut __R,
					__R: ?Sized //#(#struct_lt_args +)*,
				{
					f(unsafe { #item_ident::all_mut_erased(#self_arg) })
				}

				/// Optionally return a mutable reference from shared borrows of the prefix fields and a mutable borrow of the suffix field of the struct.
				///
				/// This is a subtle variation of `rent_all_mut` where it is legal to return a reference bounded by a rental lifetime, because that lifetime is reborrowed away before it is returned to you.
				pub fn maybe_ref_rent_all_mut<__F, __R>(#self_mut_param, f: __F) -> __rental_prelude::Option<&mut __R> where
					__F: for<#(#struct_rlt_args,)*> __rental_prelude::FnOnce(#borrow_mut_ident<#(#struct_rlt_args,)* #(#struct_lt_args,)* #(#struct_nonlt_args),*>) -> __rental_prelude::Option<&#last_rlt_arg mut __R>,
					__R: ?Sized //#(#struct_lt_args +)*,
				{
					f(unsafe { #item_ident::all_mut_erased(#self_arg) })
				}

				/// Try to return a mutable reference from shared borrows of the prefix fields and a mutable borrow of the suffix field of the struct, or an error on failure.
				///
				/// This is a subtle variation of `rent_all_mut` where it is legal to return a reference bounded by a rental lifetime, because that lifetime is reborrowed away before it is returned to you.
				pub fn try_ref_rent_all_mut<__F, __R, __E>(#self_mut_param, f: __F) -> __rental_prelude::Result<&mut __R, __E> where
					__F: for<#(#struct_rlt_args,)*> __rental_prelude::FnOnce(#borrow_mut_ident<#(#struct_rlt_args,)* #(#struct_lt_args,)* #(#struct_nonlt_args),*>) -> __rental_prelude::Result<&#last_rlt_arg mut __R, __E>,
					__R: ?Sized //#(#struct_lt_args +)*,
				{
					f(unsafe { #item_ident::all_mut_erased(#self_arg) })
				}
			}
		).to_tokens(tokens);
	}

	if attribs.is_debug {
		if attribs.is_rental_mut {
			quote_spanned!(struct_info.ident.span()/*.resolved_at(def_site)*/ =>
				impl #struct_impl_params __rental_prelude::fmt::Debug for #item_ident #struct_impl_args #struct_where_clause #where_extra #suffix_ty: __rental_prelude::fmt::Debug {
					fn fmt(&self, f: &mut __rental_prelude::fmt::Formatter) -> __rental_prelude::fmt::Result {
						f.debug_struct(#item_ident_str)
							.field(#suffix_ident_str, &self.#suffix_ident)
							.finish()
					}
				}
			).to_tokens(tokens);
		} else {
			quote_spanned!(struct_info.ident.span()/*.resolved_at(def_site)*/ =>
				impl #struct_impl_params __rental_prelude::fmt::Debug for #item_ident #struct_impl_args #struct_where_clause #where_extra #(#field_tys: __rental_prelude::fmt::Debug),* {
					fn fmt(&self, f: &mut __rental_prelude::fmt::Formatter) -> __rental_prelude::fmt::Result {
						f.debug_struct(#item_ident_str)
							#(.field(#field_ident_strs, &self.#field_idents))*
							.finish()
					}
				}
			).to_tokens(tokens);
		}
	}

	if attribs.is_clone {
		quote_spanned!(struct_info.ident.span()/*.resolved_at(def_site)*/ =>
			impl #struct_impl_params __rental_prelude::Clone for #item_ident #struct_impl_args #struct_where_clause #where_extra #(#prefix_tys: #prefix_clone_traits,)* #suffix_ty: __rental_prelude::Clone {
				fn clone(&self) -> Self {
					#item_ident {
						#(#local_idents: __rental_prelude::Clone::clone(&self.#field_idents),)*
					}
				}
			}
		).to_tokens(tokens);
	}

//	if fields[fields.len() - 1].subrental.is_some() {
//		quote_spanned!(struct_span =>
//			impl<#(#borrow_lt_params,)* #(#struct_nonlt_params),*> __rental_prelude::IntoSuffix for #borrow_ident<#(#struct_rlt_args,)* #(#struct_lt_args,)* #(#struct_nonlt_args),*> #struct_where_clause {
//				type Suffix = <#borrow_suffix_ty as __rental_prelude::IntoSuffix>::Suffix;
//
//				#[allow(non_shorthand_field_patterns)]
//				fn into_suffix(self) -> <Self as __rental_prelude::IntoSuffix>::Suffix {
//					let #borrow_ident{#suffix_ident: suffix, ..};
//					suffix.into_suffix()
//				}
//			}
//		).to_tokens(tokens);
//
//		quote_spanned!(struct_span =>
//			impl<#(#borrow_lt_params,)* #(#struct_nonlt_params),*> __rental_prelude::IntoSuffix for #borrow_mut_ident<#(#struct_rlt_args,)* #(#struct_lt_args,)* #(#struct_nonlt_args),*> #struct_where_clause {
//				type Suffix = <#borrow_mut_suffix_ty as __rental_prelude::IntoSuffix>::Suffix;
//
//				#[allow(non_shorthand_field_patterns)]
//				fn into_suffix(self) -> <Self as __rental_prelude::IntoSuffix>::Suffix {
//					let #borrow_mut_ident{#suffix_ident: suffix, ..};
//					suffix.into_suffix()
//				}
//			}
//		).to_tokens(tokens);
//
//		if attribs.is_deref_suffix {
//			quote_spanned!(suffix_ty_span =>
//				impl #struct_impl_params __rental_prelude::Deref for #item_ident #struct_impl_args #struct_where_clause {
//					type Target = <#suffix_ty as __rental_prelude::Deref>::Target;
//
//					fn deref(&self) -> &<Self as __rental_prelude::Deref>::Target {
//						#item_ident::ref_rent(self, |suffix| &**__rental_prelude::IntoSuffix::into_suffix(suffix))
//					}
//				}
//			).to_tokens(tokens);
//		}
//
//		if attribs.is_deref_mut_suffix {
//			quote_spanned!(suffix_ty_span =>
//				impl #struct_impl_params __rental_prelude::DerefMut for #item_ident #struct_impl_args #struct_where_clause {
//					fn deref_mut(&mut self) -> &mut <Self as __rental_prelude::Deref>::Target {
//						#item_ident.ref_rent_mut(self, |suffix| &mut **__rental_prelude::IntoSuffix::into_suffix(suffix))
//					}
//				}
//			).to_tokens(tokens);
//		}
//	} else {
		quote_spanned!(struct_span =>
			impl<#(#borrow_lt_params,)* #(#struct_nonlt_params),*> __rental_prelude::IntoSuffix for #borrow_ident<#(#struct_rlt_args,)* #(#struct_lt_args,)* #(#struct_nonlt_args),*> #struct_where_clause {
				type Suffix = #borrow_suffix_ty;

				#[allow(non_shorthand_field_patterns)]
				fn into_suffix(self) -> <Self as __rental_prelude::IntoSuffix>::Suffix {
					let #borrow_ident{#suffix_ident: suffix, ..} = self;
					suffix
				}
			}
		).to_tokens(tokens);

		quote_spanned!(struct_span =>
			impl<#(#borrow_lt_params,)* #(#struct_nonlt_params),*> __rental_prelude::IntoSuffix for #borrow_mut_ident<#(#struct_rlt_args,)* #(#struct_lt_args,)* #(#struct_nonlt_args),*> #struct_where_clause {
				type Suffix = #borrow_mut_suffix_ty;

				#[allow(non_shorthand_field_patterns)]
				fn into_suffix(self) -> <Self as __rental_prelude::IntoSuffix>::Suffix {
					let #borrow_mut_ident{#suffix_ident: suffix, ..} = self;
					suffix
				}
			}
		).to_tokens(tokens);

		if attribs.is_deref_suffix {
			quote_spanned!(suffix_ty_span =>
				impl #struct_impl_params __rental_prelude::Deref for #item_ident #struct_impl_args #struct_where_clause #where_extra {
					type Target = <#suffix_ty as __rental_prelude::Deref>::Target;

					fn deref(&self) -> &<Self as __rental_prelude::Deref>::Target {
						#item_ident::ref_rent(self, |suffix| &**suffix)
					}
				}
			).to_tokens(tokens);
		}

		if attribs.is_deref_mut_suffix {
			quote_spanned!(suffix_ty_span =>
				impl #struct_impl_params __rental_prelude::DerefMut for #item_ident #struct_impl_args #struct_where_clause {
					fn deref_mut(&mut self) -> &mut <Self as __rental_prelude::Deref>::Target {
						#item_ident::ref_rent_mut(self, |suffix| &mut **suffix)
					}
				}
			).to_tokens(tokens);
		}
//	}

	if attribs.is_deref_suffix {
		quote_spanned!(suffix_ty_span =>
			impl #struct_impl_params __rental_prelude::AsRef<<Self as __rental_prelude::Deref>::Target> for #item_ident #struct_impl_args #struct_where_clause {
				fn as_ref(&self) -> &<Self as __rental_prelude::Deref>::Target {
					&**self
				}
			}
		).to_tokens(tokens);
	}

	if attribs.is_deref_mut_suffix {
		quote_spanned!(suffix_ty_span =>
			impl #struct_impl_params __rental_prelude::AsMut<<Self as __rental_prelude::Deref>::Target> for #item_ident #struct_impl_args #struct_where_clause {
				fn as_mut(&mut self) -> &mut <Self as __rental_prelude::Deref>::Target {
					&mut **self
				}
			}
		).to_tokens(tokens);
	}

	if attribs.is_covariant {
		quote_spanned!(struct_info.ident.span()/*.resolved_at(def_site)*/ =>
			#[allow(dead_code)]
			impl #struct_impl_params #item_ident #struct_impl_args #struct_where_clause {
				/// Borrow all fields of the struct by reborrowing away the rental lifetimes.
				///
				/// This is safe because the lifetimes are verified to be covariant first.
				pub fn all<'__s>(#self_lt_ref_param) -> <Self as __rental_prelude::#rental_trait_ident>::Borrow {
					unsafe {
						let _covariant = __rental_prelude::PhantomData::<#borrow_ident<#(#static_rlt_args,)* #(#struct_lt_args,)* #(#struct_nonlt_args),*>>;
						let _covariant: __rental_prelude::PhantomData<#borrow_ident<#(#self_rlt_args,)* #(#struct_lt_args,)* #(#struct_nonlt_args),*>> = _covariant; 

						#item_ident::all_erased(#self_arg)
					}
				}

				/// Borrow the suffix field of the struct by reborrowing away the rental lifetimes.
				///
				/// This is safe because the lifetimes are verified to be covariant first.
				pub fn suffix(#self_ref_param) -> <<Self as __rental_prelude::#rental_trait_ident>::Borrow as __rental_prelude::IntoSuffix>::Suffix {
					&#self_arg.#suffix_ident
				}
			}
		).to_tokens(tokens);
	}

	if let Some(ref map_suffix_param) = attribs.map_suffix_param {
		let mut replacer = MapTyParamReplacer{
			ty_param: map_suffix_param,
		};

		let mapped_suffix_param = replacer.fold_type_param(map_suffix_param.clone());
		let mapped_ty = replacer.fold_path(parse_quote!(#item_ident #struct_impl_args));
		let mapped_suffix_ty = replacer.fold_type(suffix_orig_ty.clone());

		let mut map_where_clause = syn::WhereClause{
			where_token: Default::default(),
			predicates: syn::punctuated::Punctuated::from_iter(
				struct_generics.type_params().filter(|p| p.ident != map_suffix_param.ident).filter_map(|p| {
					let mut mapped_param = p.clone();
					mapped_param.bounds = syn::punctuated::Punctuated::new();

					for mapped_bound in p.bounds.iter().filter(|b| {
						let mut finder = MapTyParamFinder {
							ty_param: map_suffix_param,
							found: false,
						};
						finder.visit_type_param_bound(b);
						finder.found
					}).map(|b| {
						let mut replacer = MapTyParamReplacer{
							ty_param: map_suffix_param,
						};

						replacer.fold_type_param_bound(b.clone())
					}) {
						mapped_param.bounds.push(mapped_bound)
					}

					if mapped_param.bounds.len() > 0 {
						Some(syn::punctuated::Pair::Punctuated(parse_quote!(#mapped_param), Default::default()))
					} else {
						None
					}
				}).chain(
					struct_where_clause.iter().flat_map(|w| w.predicates.iter()).filter(|p| {
						let mut finder = MapTyParamFinder {
							ty_param: map_suffix_param,
							found: false,
						};
						finder.visit_where_predicate(p);
						finder.found
					}).map(|p| {
						let mut replacer = MapTyParamReplacer{
							ty_param: map_suffix_param,
						};

						syn::punctuated::Pair::Punctuated(replacer.fold_where_predicate(p.clone()), Default::default())
					})
				)
			)
		};

		let mut try_map_where_clause = map_where_clause.clone();
		map_where_clause.predicates.push(parse_quote!(
			__F: for<#(#suffix_rlt_args),*> __rental_prelude::FnOnce(#suffix_orig_ty) -> #mapped_suffix_ty
		));
		try_map_where_clause.predicates.push(parse_quote!(
			__F: for<#(#suffix_rlt_args),*> __rental_prelude::FnOnce(#suffix_orig_ty) -> __rental_prelude::Result<#mapped_suffix_ty, __E>
		));

		quote_spanned!(struct_span =>
			#[allow(dead_code)]
			impl #struct_impl_params #item_ident #struct_impl_args #struct_where_clause {
				/// Maps the suffix field of the rental struct to a different type.
				///
				/// Consumes the rental struct and applies the closure to the suffix field. A new rental struct is then constructed with the original prefix and new suffix.
				pub fn map<#mapped_suffix_param, __F>(#self_move_param, __f: __F) -> #mapped_ty #map_where_clause {
					let #item_ident{ #(#field_idents,)* } = #self_arg;

					let #suffix_ident = __f(#suffix_ident);

					#item_ident{ #(#field_idents: #local_idents,)* }
				}

				/// Try to map the suffix field of the rental struct to a different type.
				///
				/// As `map`, but the closure may fail. Upon failure, the tail is dropped, and the error is returned to you along with the head.
				pub fn try_map<#mapped_suffix_param, __F, __E>(#self_move_param, __f: __F) -> __rental_prelude::RentalResult<#mapped_ty, __E, #head_ty> #try_map_where_clause {
					let #item_ident{ #(#field_idents,)* } = #self_arg;

					match __f(#suffix_ident) {
						__rental_prelude::Result::Ok(#suffix_ident) => __rental_prelude::Result::Ok(#item_ident { #(#field_idents: #local_idents,)* }),
						__rental_prelude::Result::Err(__e) => __rental_prelude::Result::Err(__rental_prelude::RentalError(__e, #head_ident)),
					}
				}

				/// Try to map the suffix field of the rental struct to a different type.
				///
				/// As `map`, but the closure may fail. Upon failure, the struct is dropped and the error is returned.
				pub fn try_map_or_drop<#mapped_suffix_param, __F, __E>(#self_move_param, __f: __F) -> __rental_prelude::Result<#mapped_ty, __E> #try_map_where_clause {
					let #item_ident{ #(#field_idents,)* } = #self_arg;

					let #suffix_ident = __f(#suffix_ident)?;

					Ok(#item_ident{ #(#field_idents: #local_idents,)* })
				}
			}
		).to_tokens(tokens);
	}
}


fn get_struct_attribs(struct_info: &syn::ItemStruct) -> RentalStructAttribs
{
	let mut rattribs = struct_info.attrs.clone();

	let mut is_rental_mut = false;
	let mut is_debug = false;
	let mut is_clone = false;
	let mut is_deref_suffix = false;
	let mut is_deref_mut_suffix = false;
	let mut is_covariant = false;
	let mut map_suffix_param = None;

	if let Some(rental_pos) = rattribs.iter()/*.filter(|attr| !attr.is_sugared_doc)*/.position(|attr| match attr.parse_meta().expect(&format!("Struct `{}` Attribute `{}` is not properly formatted.", struct_info.ident, attr.path.clone().into_token_stream())) {
		syn::Meta::Path(ref attr_ident) => {
			is_rental_mut = match attr_ident.segments.first().map(|s| s.ident.to_string()).as_ref().map(String::as_str) {
				Some("rental") => false,
				Some("rental_mut") => true,
				_ => return false,
			};

			true
		},
		syn::Meta::List(ref list) => {
			is_rental_mut = match list.path.segments.first().map(|s| s.ident.to_string()).as_ref().map(String::as_str) {
				Some("rental") => false,
				Some("rental_mut") => true,
				_ => return false,
			};

			let leftover = list.nested.iter().filter(|nested| {
				if let syn::NestedMeta::Meta(ref meta) = **nested {
					match *meta {
						syn::Meta::Path(ref ident) => {
							match ident.segments.first().map(|s| s.ident.to_string()).as_ref().map(String::as_str) {
								Some("debug") => {
									is_debug = true;
									false
								},
								Some("clone") => {
									is_clone = true;
									false
								},
								Some("deref_suffix") => {
									is_deref_suffix = true;
									false
								},
								Some("deref_mut_suffix") => {
									is_deref_suffix = true;
									is_deref_mut_suffix = true;
									false
								},
								Some("covariant") => {
									is_covariant = true;
									false
								},
								Some("map_suffix") => {
									panic!("Struct `{}` `map_suffix` flag expects ` = \"T\"`.", struct_info.ident);
								},
								_ => true,
							}
						},
						syn::Meta::NameValue(ref name_value) => {
							match name_value.path.segments.first().map(|s| s.ident.to_string()).as_ref().map(String::as_str) {
								Some("map_suffix") => {
									if let syn::Lit::Str(ref ty_param_str) = name_value.lit {
										let ty_param_str = ty_param_str.value();
										let ty_param = struct_info.generics.type_params().find(|ty_param| {
											if ty_param.ident.to_string() == ty_param_str {
												return true;
											}

											false
										}).unwrap_or_else(|| {
											panic!("Struct `{}` `map_suffix` param `{}` does not name a type parameter.", struct_info.ident, ty_param_str);
										});

										let mut finder = MapTyParamFinder{
											ty_param: &ty_param,
											found: false,
										};

										if struct_info.fields.iter().take(struct_info.fields.iter().count() - 1).any(|f| {
											finder.found = false;
											finder.visit_field(f);
											finder.found
										}) {
											panic!("Struct `{}` `map_suffix` type param `{}` appears in a prefix field.", struct_info.ident, ty_param_str);
										}

										finder.found = false;
										finder.visit_field(struct_info.fields.iter().last().unwrap());
										if !finder.found {
											panic!("Struct `{}` `map_suffix` type param `{}` does not appear in the suffix field.", struct_info.ident, ty_param_str);
										}

										map_suffix_param = Some(ty_param.clone());
										false
									} else {
										panic!("Struct `{}` `map_suffix` flag expects ` = \"T\"`.", struct_info.ident);
									}
								},
								_ => true,
							}
						},
						_ => true,
					}
				} else {
					true
				}
			}).count();

			if leftover > 0 {
				panic!("Struct `{}` rental attribute takes optional arguments: `debug`, `clone`, `deref_suffix`, `deref_mut_suffix`, `covariant`, and `map_suffix = \"T\"`.", struct_info.ident);
			}

			true
		},
		_ => false,
	}) {
		rattribs.remove(rental_pos);
	} else {
		panic!("Struct `{}` must have a `rental` or `rental_mut` attribute.", struct_info.ident);
	}

	if rattribs.iter().any(|attr| attr.path != syn::parse_str::<syn::Path>("doc").unwrap()) {
		panic!("Struct `{}` must not have attributes other than one `rental` or `rental_mut`.", struct_info.ident);
	}

	if is_rental_mut && is_clone {
		panic!("Struct `{}` cannot be both `rental_mut` and `clone`.", struct_info.ident);
	}

	RentalStructAttribs{
		doc: rattribs,
		is_rental_mut: is_rental_mut,
		is_debug: is_debug,
		is_clone: is_clone,
		is_deref_suffix: is_deref_suffix,
		is_deref_mut_suffix: is_deref_mut_suffix,
		is_covariant: is_covariant,
		map_suffix_param: map_suffix_param,
	}
}


fn prepare_fields(struct_info: &syn::ItemStruct) -> (Vec<RentalField>, syn::token::Brace) {
	let def_site: Span = Span::call_site(); // FIXME: hygiene
	let call_site: Span = Span::call_site();

	let (fields, fields_brace) = match struct_info.fields {
		syn::Fields::Named(ref fields) => (&fields.named, fields.brace_token),
		syn::Fields::Unnamed(..) => panic!("Struct `{}` must not be a tuple struct.", struct_info.ident),
		_ => panic!("Struct `{}` must have at least 2 fields.", struct_info.ident),
	};

	if fields.len() < 2 {
		panic!("Struct `{}` must have at least 2 fields.", struct_info.ident);
	}

	let mut rfields = Vec::with_capacity(fields.len());
	for (field_idx, field) in fields.iter().enumerate() {
		if field.vis != syn::Visibility::Inherited {
			panic!(
				"Struct `{}` field `{}` must be private.",
				struct_info.ident,
				field.ident.as_ref().map(|ident| ident.to_string()).unwrap_or_else(|| field_idx.to_string())
			);
		}

		let mut rfattribs = field.attrs.clone();
		let mut subrental = None;
		let mut target_ty_hack = None;

		if let Some(sr_pos) = rfattribs.iter().position(|attr| match attr.parse_meta() {
			Ok(syn::Meta::List(ref list)) if list.path.segments.first().map(|s| s.ident.to_string()).as_ref().map(String::as_str) == Some("subrental") => {
				panic!(
					"`subrental` attribute on struct `{}` field `{}` expects ` = arity`.",
					struct_info.ident,
					field.ident.as_ref().map(|ident| ident.to_string()).unwrap_or_else(|| field_idx.to_string())
				);
			},
			Ok(syn::Meta::Path(ref word)) if word.segments.first().map(|s| s.ident.to_string()).as_ref().map(String::as_str) == Some("subrental") => {
				panic!(
					"`subrental` attribute on struct `{}` field `{}` expects ` = arity`.",
					struct_info.ident,
					field.ident.as_ref().map(|ident| ident.to_string()).unwrap_or_else(|| field_idx.to_string())
				);
			},
			Ok(syn::Meta::NameValue(ref name_value)) if name_value.path.segments.first().map(|s| s.ident.to_string()).as_ref().map(String::as_str) == Some("subrental") => {
				match name_value.lit {
					syn::Lit::Int(ref arity) => {
						subrental = Some(Subrental{
							arity: arity.base10_parse::<usize>().unwrap(),
							rental_trait_ident: syn::Ident::new(&format!("Rental{}", arity.base10_parse::<usize>().unwrap()), def_site),
						})
					},
					_ => panic!(
						"`subrental` attribute on struct `{}` field `{}` expects ` = arity`.",
						struct_info.ident,
						field.ident.as_ref().map(|ident| ident.to_string()).unwrap_or_else(|| field_idx.to_string())
					),
				}

				true
			},
			_ => false,
		}) {
			rfattribs.remove(sr_pos);
		}

		if subrental.is_some() && field_idx == fields.len() - 1 {
			panic!(
				"struct `{}` field `{}` cannot be a subrental because it is the suffix field.",
				struct_info.ident,
				field.ident.as_ref().map(|ident| ident.to_string()).unwrap_or_else(|| field_idx.to_string())
			);
		}

		if let Some(tth_pos) = rfattribs.iter().position(|a|
			match a.parse_meta() {
				Ok(syn::Meta::NameValue(syn::MetaNameValue{ref path, lit: syn::Lit::Str(ref ty_str), ..})) if path.segments.first().map(|s| s.ident.to_string()).as_ref().map(String::as_str) == Some("target_ty") => {
					if let Ok(ty) = syn::parse_str::<syn::Type>(&ty_str.value()) {
						target_ty_hack = Some(ty);

						true
					} else {
						panic!(
							"`target_ty` attribute on struct `{}` field `{}` has an invalid ty string.",
							struct_info.ident,
							field.ident.as_ref().map(|ident| ident.to_string()).unwrap_or_else(|| field_idx.to_string())
						);
					}
				},
				_ => false,
			}
		) {
			rfattribs.remove(tth_pos);
		}

		if field_idx == fields.len() - 1 && target_ty_hack.is_some() {
			panic!(
				"Struct `{}` field `{}` cannot have `target_ty` attribute because it is the suffix field.",
				struct_info.ident,
				field.ident.as_ref().map(|ident| ident.to_string()).unwrap_or_else(|| field_idx.to_string())
			);
		}

		let target_ty_hack = target_ty_hack.as_ref().map(|ty| (*ty).clone()).or_else(|| if field_idx < fields.len() - 1 {
			let guess = if let syn::Type::Path(ref ty_path) = field.ty {
				match ty_path.path.segments[ty_path.path.segments.len() - 1] {
					syn::PathSegment{ref ident, arguments: syn::PathArguments::AngleBracketed(syn::AngleBracketedGenericArguments{ref args, ..})} => {
						if let Some(&syn::GenericArgument::Type(ref ty)) = args.first() {
							if ident == "Vec" {
								Some(syn::Type::Slice(syn::TypeSlice{bracket_token: Default::default(), elem: Box::new(ty.clone())}))
							} else {
								Some(ty.clone())
							}
						} else {
							None
						}
					},
					syn::PathSegment{ref ident, arguments: syn::PathArguments::None} => {
						if ident == "String" {
							Some(parse_quote!(str))
						} else {
							None
						}
					},
					_ => {
						None
					},
				}
			} else if let syn::Type::Reference(syn::TypeReference{elem: ref box_ty, ..}) = field.ty {
				Some((**box_ty).clone())
			} else {
				None
			};

			let guess = guess.or_else(|| if field_idx == 0 {
				let field_ty = &field.ty;
				Some(parse_quote!(<#field_ty as __rental_prelude::Deref>::Target))
			} else {
				None
			});

			if guess.is_none() {
				panic!("Struct `{}` field `{}` must be a type path with 1 type param, `String`, or a reference.", struct_info.ident, field.ident.as_ref().unwrap())
			}

			guess
		} else {
			None
		});

		if subrental.is_some() {
			if match target_ty_hack {
				Some(syn::Type::Path(ref ty_path)) => ty_path.qself.is_some(),
				Some(_) => true,
				_ => false,
			} {
				panic!(
					"Struct `{}` field `{}` must have an unqualified path for its `target_ty` to be a valid subrental.",
					struct_info.ident,
					field.ident.as_ref().map(|ident| ident.to_string()).unwrap_or_else(|| field_idx.to_string())
				);
			}
		}

		let target_ty_hack_erased = target_ty_hack.as_ref().map(|tth| {
			let mut eraser = RentalLifetimeEraser{
				fields: &rfields,
				used_rlt_args: &mut Vec::new(),
			};

			eraser.fold_type(tth.clone())
		});

		let mut self_rlt_args = Vec::new();
		if let Some(Subrental{arity: sr_arity, ..}) = subrental {
			let field_ident = field.ident.as_ref().unwrap();
			for sr_idx in 0 .. sr_arity {
				self_rlt_args.push(syn::Lifetime::new(&format!("'{}_{}", field_ident, sr_idx), call_site));
			}
		} else {
			let field_ident = field.ident.as_ref().unwrap();
			self_rlt_args.push(syn::Lifetime::new(&format!("'{}", field_ident), call_site));
		}

		let mut used_rlt_args = Vec::new();
		let rty = {
			let mut eraser = RentalLifetimeEraser{
				fields: &rfields,
				used_rlt_args: &mut used_rlt_args,
			};

			eraser.fold_type(field.ty.clone())
		};

		if rfattribs.iter().any(|attr| match attr.parse_meta() { Ok(syn::Meta::NameValue(syn::MetaNameValue{ref path, ..})) if path.segments.first().map(|s| s.ident.to_string()).as_ref().map(String::as_str) == Some("doc") => false, _ => true }) {
			panic!(
				"Struct `{}` field `{}` must not have attributes other than one `subrental` and `target_ty`.",
				struct_info.ident,
				field.ident.as_ref().map(|ident| ident.to_string()).unwrap_or_else(|| field_idx.to_string())
			);
		}

		rfields.push(RentalField{
			name: field.ident.as_ref().unwrap().clone(),
			orig_ty: field.ty.clone(),
			erased: syn::Field{
				colon_token: field.colon_token,
				ident: field.ident.clone(),
				vis: field.vis.clone(),
				attrs: rfattribs,
				ty: rty,
			},
			subrental: subrental,
			self_rlt_args: self_rlt_args,
			used_rlt_args: used_rlt_args,
			target_ty_hack: target_ty_hack,
			target_ty_hack_erased: target_ty_hack_erased,
		});
	}

	(rfields, fields_brace)
}


fn make_borrow_quotes(self_arg: &proc_macro2::TokenStream, fields: &[RentalField], is_rental_mut: bool) -> Vec<BorrowQuotes> {
	let call_site: Span = Span::call_site();

	(0 .. fields.len()).map(|idx| {
		let (field_ty, deref) = if idx == fields.len() - 1 {
			let orig_ty = &fields[idx].orig_ty;
			(
				quote!(#orig_ty),
				quote!()
			)
		} else {
			let orig_ty = &fields[idx].orig_ty;
			(
				quote!(<#orig_ty as __rental_prelude::Deref>::Target),
				quote!(*)
			)
		};

		let field_ty_hack = fields[idx].target_ty_hack.as_ref().unwrap_or(&fields[idx].orig_ty);
		let field_ty_hack_erased = fields[idx].target_ty_hack_erased.as_ref().unwrap_or(&fields[idx].erased.ty);

		if let Some(ref subrental) = fields[idx].subrental {
			let field_ident = &fields[idx].name;
			let rental_trait_ident = &subrental.rental_trait_ident;
			let field_rlt_args = &fields[idx].self_rlt_args;

			let (ref borrow_ty_hack, ref borrow_mut_ty_hack, ref field_args) = if let syn::Type::Path(syn::TypePath{ref qself, path: ref ty_path}) = *field_ty_hack {
				let seg_idx = ty_path.segments.len() - 1;
				let ty_name = &ty_path.segments[seg_idx].ident.to_string();

				let mut borrow_ty_path = ty_path.clone();
				borrow_ty_path.segments[seg_idx].ident = syn::Ident::new(&format!("{}_Borrow", ty_name), call_site);
				borrow_ty_path.segments[seg_idx].arguments = syn::PathArguments::None;

				let mut borrow_mut_ty_path = ty_path.clone();
				borrow_mut_ty_path.segments[seg_idx].ident = syn::Ident::new(&format!("{}_BorrowMut", ty_name), call_site);
				borrow_mut_ty_path.segments[seg_idx].arguments = syn::PathArguments::None;

				match ty_path.segments[seg_idx].arguments {
					syn::PathArguments::AngleBracketed(ref args) => {
						(
							syn::Type::Path(syn::TypePath{qself: qself.clone(), path: borrow_ty_path}),
							syn::Type::Path(syn::TypePath{qself: qself.clone(), path: borrow_mut_ty_path}),
							args.args.iter().collect::<Vec<_>>(),
						)
					},
					syn::PathArguments::None => {
						(
							syn::Type::Path(syn::TypePath{qself: qself.clone(), path: borrow_ty_path}),
							syn::Type::Path(syn::TypePath{qself: qself.clone(), path: borrow_mut_ty_path}),
							Vec::with_capacity(0),
						)
					},
					_ => panic!("Field `{}` must have angle-bracketed args.", fields[idx].name),
				}
			} else {
				panic!("Field `{}` must be a type path.", fields[idx].name)
			};

			BorrowQuotes {
				ty: if idx == fields.len() - 1 || !is_rental_mut {
					quote!(<#field_ty as __rental_prelude::#rental_trait_ident<#(#field_rlt_args),*>>::Borrow)
				} else {
					quote!(__rental_prelude::PhantomData<<#field_ty as __rental_prelude::#rental_trait_ident<#(#field_rlt_args),*>>::Borrow>)
				},
				ty_hack: if idx == fields.len() - 1 || !is_rental_mut {
					quote!(#borrow_ty_hack<#(#field_rlt_args,)* #(#field_args),*>)
				} else {
					quote!(__rental_prelude::PhantomData<#borrow_ty_hack<#(#field_rlt_args,)* #(#field_args),*>>)
				},
				expr: if idx == fields.len() - 1 || !is_rental_mut {
					quote!(unsafe { <#field_ty_hack_erased>::all_erased(&#deref #self_arg.#field_ident) })
				} else {
					quote!(__rental_prelude::PhantomData::<()>)
				},

				mut_ty: if idx == fields.len() - 1 {
					quote!(<#field_ty as __rental_prelude::#rental_trait_ident<#(#field_rlt_args),*>>::BorrowMut)
				} else if !is_rental_mut {
					quote!(<#field_ty as __rental_prelude::#rental_trait_ident<#(#field_rlt_args),*>>::Borrow)
				} else {
					quote!(__rental_prelude::PhantomData<<#field_ty as __rental_prelude::#rental_trait_ident<#(#field_rlt_args),*>>::BorrowMut>)
				},
				mut_ty_hack: if idx == fields.len() - 1 {
					quote!(#borrow_mut_ty_hack<#(#field_rlt_args,)* #(#field_args),*>)
				} else if !is_rental_mut {
					quote!(#borrow_ty_hack<#(#field_rlt_args,)* #(#field_args),*>)
				} else {
					quote!(__rental_prelude::PhantomData<#borrow_mut_ty_hack<#(#field_rlt_args,)* #(#field_args),*>>)
				},
				mut_expr: if idx == fields.len() - 1 {
					quote!(unsafe { <#field_ty_hack_erased>::all_mut_erased(&mut #deref #self_arg.#field_ident) })
				} else if !is_rental_mut {
					quote!(unsafe { <#field_ty_hack_erased>::all_erased(&#deref #self_arg.#field_ident) })
				} else {
					quote!(__rental_prelude::PhantomData::<()>)
				},

				new_ty: if !is_rental_mut  {
					//quote!(<#field_ty as __rental_prelude::#rental_trait_ident<#(#field_rlt_args),*>>::Borrow)
					quote!(#borrow_ty_hack<#(#field_rlt_args,)* #(#field_args),*>)
				} else {
					//quote!(<#field_ty as __rental_prelude::#rental_trait_ident<#(#field_rlt_args),*>>::BorrowMut)
					quote!(#borrow_mut_ty_hack<#(#field_rlt_args,)* #(#field_args),*>)
				},
				new_expr: if !is_rental_mut {
					quote!(unsafe { <#field_ty_hack_erased>::all_erased(&#deref #field_ident) })
				} else {
					quote!(unsafe { <#field_ty_hack_erased>::all_mut_erased(&mut #deref #field_ident) })
				},
			}
		} else {
			let field_ident = &fields[idx].name;
			let field_rlt_arg = &fields[idx].self_rlt_args[0];

			BorrowQuotes {
				ty: if idx == fields.len() - 1 || !is_rental_mut {
					quote!(&#field_rlt_arg (#field_ty))
				} else {
					quote!(__rental_prelude::PhantomData<&#field_rlt_arg #field_ty>)
				},
				ty_hack: if idx == fields.len() - 1 || !is_rental_mut {
					quote!(&#field_rlt_arg (#field_ty_hack))
				} else {
					quote!(__rental_prelude::PhantomData<&#field_rlt_arg #field_ty_hack>)
				},
				expr: if idx == fields.len() - 1 || !is_rental_mut {
					quote!(&#deref #self_arg.#field_ident)
				} else {
					quote!(__rental_prelude::PhantomData::<()>)
				},

				mut_ty: if idx == fields.len() - 1 {
					quote!(&#field_rlt_arg mut (#field_ty))
				} else if !is_rental_mut {
					quote!(&#field_rlt_arg (#field_ty))
				} else {
					quote!(__rental_prelude::PhantomData<&#field_rlt_arg mut #field_ty>)
				},
				mut_ty_hack: if idx == fields.len() - 1 {
					quote!(&#field_rlt_arg mut (#field_ty_hack))
				} else if !is_rental_mut {
					quote!(&#field_rlt_arg (#field_ty_hack))
				} else {
					quote!(__rental_prelude::PhantomData<&#field_rlt_arg mut #field_ty_hack>)
				},
				mut_expr: if idx == fields.len() - 1 {
					quote!(&mut #deref #self_arg.#field_ident)
				} else if !is_rental_mut {
					quote!(&#deref #self_arg.#field_ident)
				} else {
					quote!(__rental_prelude::PhantomData::<()>)
				},

				new_ty: if !is_rental_mut {
					//quote!(&#field_rlt_arg #field_ty)
					quote!(&#field_rlt_arg (#field_ty_hack))
				} else {
					//quote!(&#field_rlt_arg mut #field_ty)
					quote!(&#field_rlt_arg mut (#field_ty_hack))
				},
				new_expr: if !is_rental_mut {
					quote!(& #deref #field_ident)
				} else {
					quote!(&mut #deref #field_ident)
				},
			}
		}
	}).collect()
}


fn make_tail_closure_quotes(fields: &[RentalField], borrows: &[BorrowQuotes], is_rental_mut: bool) -> Vec<ClosureQuotes> {
	(1 .. fields.len()).map(|idx| {
		let local_name = &fields[idx].name;
		let field_ty = &fields[idx].orig_ty;

		if !is_rental_mut {
			let prev_new_tys_reverse = &borrows[0 .. idx].iter().map(|b| &b.new_ty).rev().collect::<Vec<_>>();
			let prev_new_exprs_reverse = &borrows[0 .. idx].iter().map(|b| &b.new_expr).rev().collect::<Vec<_>>();;
			let mut prev_rlt_args = Vec::<syn::Lifetime>::new();
			for prev_field in &fields[0 .. idx] {
				prev_rlt_args.extend(prev_field.self_rlt_args.iter().cloned());
			}
			let prev_rlt_args = &prev_rlt_args;

			ClosureQuotes {
				bound: quote!(for<#(#prev_rlt_args),*> __rental_prelude::FnOnce(#(#prev_new_tys_reverse),*) -> #field_ty),
				expr: quote!(#local_name(#(#prev_new_exprs_reverse),*)),
				try_bound: quote!(for<#(#prev_rlt_args),*> __rental_prelude::FnOnce(#(#prev_new_tys_reverse),*) -> __rental_prelude::Result<#field_ty, __E>),
				try_expr: quote!(#local_name(#(#prev_new_exprs_reverse),*)),
			}
		} else {
			let prev_new_ty = &borrows[idx - 1].new_ty;
			let prev_new_expr = &borrows[idx - 1].new_expr;
			let prev_rlt_args = &fields[idx - 1].self_rlt_args.iter().chain(&fields[idx - 1].used_rlt_args).collect::<Vec<_>>();

			ClosureQuotes {
				bound: quote!(for<#(#prev_rlt_args),*> __rental_prelude::FnOnce(#prev_new_ty) -> #field_ty),
				expr: quote!(#local_name(#prev_new_expr)),
				try_bound: quote!(for<#(#prev_rlt_args),*> __rental_prelude::FnOnce(#prev_new_ty) -> __rental_prelude::Result<#field_ty, __E>),
				try_expr: quote!(#local_name(#prev_new_expr)),
			}
		}
	}).collect()
}


struct RentalStructAttribs {
	pub doc: Vec<syn::Attribute>,
	pub is_rental_mut: bool,
	pub is_debug: bool,
	pub is_clone: bool,
	pub is_deref_suffix: bool,
	pub is_deref_mut_suffix: bool,
	pub is_covariant: bool,
	pub map_suffix_param: Option<syn::TypeParam>,
}


struct RentalField {
	pub name: syn::Ident,
	pub orig_ty: syn::Type,
	pub erased: syn::Field,
	pub subrental: Option<Subrental>,
	pub self_rlt_args: Vec<syn::Lifetime>,
	pub used_rlt_args: Vec<syn::Lifetime>,
	pub target_ty_hack: Option<syn::Type>,
	pub target_ty_hack_erased: Option<syn::Type>,
}


struct Subrental {
	arity: usize,
	rental_trait_ident: syn::Ident,
}


struct BorrowQuotes {
	pub ty: proc_macro2::TokenStream,
	pub ty_hack: proc_macro2::TokenStream,
	pub expr: proc_macro2::TokenStream,
	pub mut_ty: proc_macro2::TokenStream,
	pub mut_ty_hack: proc_macro2::TokenStream,
	pub mut_expr: proc_macro2::TokenStream,
	pub new_ty: proc_macro2::TokenStream,
	pub new_expr: proc_macro2::TokenStream,
}


struct ClosureQuotes {
	pub bound: proc_macro2::TokenStream,
	pub expr: proc_macro2::TokenStream,
	pub try_bound: proc_macro2::TokenStream,
	pub try_expr: proc_macro2::TokenStream,
}


struct RentalLifetimeEraser<'a> {
	pub fields: &'a [RentalField],
	pub used_rlt_args: &'a mut Vec<syn::Lifetime>,
}


impl<'a> syn::fold::Fold for RentalLifetimeEraser<'a> {
	fn fold_lifetime(&mut self, lifetime: syn::Lifetime) -> syn::Lifetime {
		let def_site: Span = Span::call_site(); // FIXME: hygiene

		if self.fields.iter().any(|field| field.self_rlt_args.contains(&lifetime)) {
			if !self.used_rlt_args.contains(&lifetime) {
				self.used_rlt_args.push(lifetime.clone());
			}

			syn::Lifetime::new("'static", def_site)
		} else {
			lifetime
		}
	}
}


struct MapTyParamFinder<'p> {
	pub ty_param: &'p syn::TypeParam,
	pub found: bool,
}


impl<'p, 'ast> syn::visit::Visit<'ast> for MapTyParamFinder<'p> {
	fn visit_path(&mut self, path: &'ast syn::Path) {
		if path.leading_colon.is_none() && path.segments.len() == 1 && path.segments[0].ident == self.ty_param.ident && path.segments[0].arguments == syn::PathArguments::None {
			self.found = true;
		} else {
			for seg in &path.segments {
				self.visit_path_segment(seg)
			};
		}
	}
}


struct MapTyParamReplacer<'p> {
	pub ty_param: &'p syn::TypeParam,
}


impl<'p> syn::fold::Fold for MapTyParamReplacer<'p> {
	fn fold_path(&mut self, path: syn::Path) -> syn::Path {
		if path.leading_colon.is_none() && path.segments.len() == 1 && path.segments[0].ident == self.ty_param.ident && path.segments[0].arguments == syn::PathArguments::None {
			let def_site: Span = Span::call_site(); // FIXME: hygiene

			syn::Path{
				leading_colon: None,
				segments: syn::punctuated::Punctuated::from_iter(Some(
					syn::punctuated::Pair::End(syn::PathSegment{ident: syn::Ident::new("__U", def_site), arguments: syn::PathArguments::None})
				)),
			}
		} else {
			let syn::Path{
				leading_colon,
				segments,
			} = path;

			syn::Path{
				leading_colon: leading_colon,
				segments: syn::punctuated::Punctuated::from_iter(segments.into_pairs().map(|p| match p {
					syn::punctuated::Pair::Punctuated(seg, punc) => syn::punctuated::Pair::Punctuated(self.fold_path_segment(seg), punc),
					syn::punctuated::Pair::End(seg) => syn::punctuated::Pair::End(self.fold_path_segment(seg)),
				}))
			}
		}
	}


	fn fold_type_param(&mut self, mut ty_param: syn::TypeParam) -> syn::TypeParam {
		if ty_param.ident == self.ty_param.ident {
			let def_site: Span = Span::call_site(); // FIXME: hygiene
			ty_param.ident = syn::Ident::new("__U", def_site);
		}

		let bounds = syn::punctuated::Punctuated::from_iter(ty_param.bounds.iter().map(|b| self.fold_type_param_bound(b.clone())));
		ty_param.bounds = bounds;

		ty_param
	}
}


