use proc_macro_hack::proc_macro_hack;
pub use unic_langid_impl::{subtags, LanguageIdentifier};

#[proc_macro_hack]
pub use unic_langid_macros_impl::langid;

#[proc_macro_hack]
pub use unic_langid_macros_impl::lang;

#[proc_macro_hack]
pub use unic_langid_macros_impl::script;

#[proc_macro_hack]
pub use unic_langid_macros_impl::region;

#[proc_macro_hack]
pub use unic_langid_macros_impl::variant_fn as variant;
