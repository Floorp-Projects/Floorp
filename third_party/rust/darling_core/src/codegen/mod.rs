use quote::Tokens;

mod default_expr;
mod error;
mod field;
mod fmi_impl;
mod from_derive_impl;
mod from_field;
mod from_variant_impl;
mod outer_from_impl;
mod trait_impl;
mod variant;
mod variant_data;

pub use self::default_expr::DefaultExpression;
pub use self::field::Field;
pub use self::fmi_impl::FmiImpl;
pub use self::from_derive_impl::FromDeriveInputImpl;
pub use self::from_field::FromFieldImpl;
pub use self::from_variant_impl::FromVariantImpl;
pub use self::outer_from_impl::OuterFromImpl;
pub use self::trait_impl::TraitImpl;
pub use self::variant::Variant;
pub use self::variant_data::VariantDataGen;

use options::ForwardAttrs;

/// Infrastructure for generating an attribute extractor.
pub trait ExtractAttribute {
    fn local_declarations(&self) -> Tokens;

    fn immutable_declarations(&self) -> Tokens;

    /// Gets the list of attribute names that should be parsed by the extractor.
    fn attr_names(&self) -> &[&str];

    fn forwarded_attrs(&self) -> Option<&ForwardAttrs>;

    /// Gets the name used by the generated impl to return to the `syn` item passed as input.
    fn param_name(&self) -> Tokens;

    /// Gets the core from-meta-item loop that should be used on matching attributes.
    fn core_loop(&self) -> Tokens;

    fn declarations(&self) -> Tokens {
        if !self.attr_names().is_empty() {
            self.local_declarations()
        } else {
            self.immutable_declarations()
        }
    }

    /// Generates the main extraction loop.
    fn extractor(&self) -> Tokens {
        let declarations = self.declarations();

        let will_parse_any = !self.attr_names().is_empty();
        let will_fwd_any = self.forwarded_attrs().map(|fa| !fa.is_empty()).unwrap_or_default();

        if !(will_parse_any || will_fwd_any) {
            return quote! {
                #declarations
            };
        }

        let input = self.param_name();

        /// The block for parsing attributes whose names have been claimed by the target
        /// struct. If no attributes were claimed, this is a pass-through.
        let parse_handled = if will_parse_any {
            let attr_names = self.attr_names();
            let core_loop = self.core_loop();
            quote!(
                #(#attr_names)|* => {
                    if let ::syn::MetaItem::List(_, ref __items) = __attr.value {
                        #core_loop
                    } else {
                        // darling currently only supports list-style
                        continue
                    }
                }
            )
        } else {
            quote!()
        };

        /// Specifies the behavior for unhandled attributes. They will either be silently ignored or 
        /// forwarded to the inner struct for later analysis.
        let forward_unhandled = if will_fwd_any {
            forwards_to_local(self.forwarded_attrs().unwrap())
        } else {
            quote!(_ => continue)
        };

        quote!(
            #declarations
            let mut __fwd_attrs: ::darling::export::Vec<::syn::Attribute> = vec![];

            for __attr in &#input.attrs {
                // Filter attributes based on name
                match __attr.name() {
                    #parse_handled
                    #forward_unhandled
                }
            }
        )
    }
}

fn forwards_to_local(behavior: &ForwardAttrs) -> Tokens {
    let push_command = quote!(__fwd_attrs.push(__attr.clone()));
    match *behavior {
        ForwardAttrs::All => quote!(_ => #push_command),
        ForwardAttrs::Only(ref idents) => {
            let names = idents.as_strs();
            quote!(
                #(#names)|* => #push_command,
                _ => continue,
            )
        }
    }
}