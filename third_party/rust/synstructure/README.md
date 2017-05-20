# synstructure

> NOTE: What follows is an exerpt from the module level documentation. For full
> details read the docs on [docs.rs](https://docs.rs/synstructure/)

This crate provides helper methods for matching against enum variants, and
extracting bindings to each of the fields in the deriving Struct or Enum in
a generic way.

If you are writing a `#[derive]` which needs to perform some operation on every
field, then you have come to the right place!

## Example Usage

```rust
extern crate syn;
extern crate synstructure;
#[macro_use]
extern crate quote;
use synstructure::{each_field, BindStyle};

type TokenStream = String; // XXX: Dummy to not depend on rustc_macro

fn sum_fields_derive(input: TokenStream) -> TokenStream {
    let source = input.to_string();
    let ast = syn::parse_macro_input(&source).unwrap();

    let match_body = each_field(&ast, &BindStyle::Ref.into(), |bi| quote! {
        sum += #bi as i64;
    });

    let name = &ast.ident;
    let (impl_generics, ty_generics, where_clause) = ast.generics.split_for_impl();
    let result = quote! {
        impl #impl_generics ::sum_fields::SumFields for #name #ty_generics #where_clause {
            fn sum_fields(&self) -> i64 {
                let mut sum = 0i64;
                match *self { #match_body }
                sum
            }
        }
    };

    result.to_string().parse().unwrap()
}

fn main() {}
```

For more example usage, consider investigating the `abomonation_derive` crate,
which makes use of this crate, and is fairly simple.
