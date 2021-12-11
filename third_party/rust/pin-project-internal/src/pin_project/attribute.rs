use proc_macro2::TokenStream;
use quote::quote;
use syn::{
    parse::{Parse, ParseStream},
    *,
};

use super::PIN;
use crate::utils::SliceExt;

// To generate the correct `Unpin` implementation and the projection methods,
// we need to collect the types of the pinned fields.
// However, since proc-macro-attribute is applied before `cfg` and `cfg_attr`
// on fields, we cannot be collecting field types properly at this timing.
// So instead of generating the `Unpin` implementation and the projection
// methods here, delegate their processing to proc-macro-derive.
//
// At this stage, only attributes are parsed and the following attributes are
// added to the attributes of the item.
// * `#[derive(InternalDerive)]` - An internal helper macro that does the above
//   processing.
// * `#[pin(__private(#args))]` - Pass the argument of `#[pin_project]` to
//   proc-macro-derive (`InternalDerive`).

pub(super) fn parse_attribute(args: &TokenStream, input: TokenStream) -> Result<TokenStream> {
    let Input { attrs, body } = syn::parse2(input)?;

    Ok(quote! {
        #(#attrs)*
        #[derive(::pin_project::__private::__PinProjectInternalDerive)]
        // Use `__private` to prevent users from trying to control `InternalDerive`
        // manually. `__private` does not guarantee compatibility between patch
        // versions, so it should be sufficient for this purpose in most cases.
        #[pin(__private(#args))]
        #body
    })
}

#[allow(dead_code)] // https://github.com/rust-lang/rust/issues/56750
struct Input {
    attrs: Vec<Attribute>,
    body: TokenStream,
}

impl Parse for Input {
    fn parse(input: ParseStream<'_>) -> Result<Self> {
        let attrs = input.call(Attribute::parse_outer)?;

        let ahead = input.fork();
        let _: Visibility = ahead.parse()?;
        if !ahead.peek(Token![struct]) && !ahead.peek(Token![enum]) {
            // If we check this only on proc-macro-derive, it may generate unhelpful error
            // messages. So it is preferable to be able to detect it here.
            Err(error!(
                input.parse::<TokenStream>()?,
                "#[pin_project] attribute may only be used on structs or enums"
            ))
        } else if let Some(attr) = attrs.find(PIN) {
            Err(error!(attr, "#[pin] attribute may only be used on fields of structs or variants"))
        } else if let Some(attr) = attrs.find("pin_project") {
            Err(error!(attr, "duplicate #[pin_project] attribute"))
        } else {
            Ok(Self { attrs, body: input.parse()? })
        }
    }
}
