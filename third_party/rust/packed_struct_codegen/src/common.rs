extern crate syn;

#[cfg(feature="std")]
pub fn collections_prefix() -> syn::Ty {
    syn::parse_type("::std").unwrap()
}

#[cfg(not(feature="std"))]
pub fn collections_prefix() -> syn::Ty {
    syn::parse_type("::alloc").unwrap()
}

#[cfg(feature="std")]
pub fn result_type() -> syn::Ty {
    syn::parse_type("::std::result::Result").expect("result type parse error")
}

#[cfg(not(feature="std"))]
pub fn result_type() -> syn::Ty {
    syn::parse_type("::core::result::Result").expect("result type parse error")
}


pub fn alloc_supported() -> bool {
    #[cfg(any(feature="std", feature="alloc"))]
    {
        true
    }
    #[cfg(not(any(feature="std", feature="alloc")))]
    {
        false
    }}


pub fn include_debug_codegen() -> bool {
    alloc_supported()    
}
