/// A macro for defining #[cfg] if-else statements.
///
/// This is similar to the `if/elif` C preprocessor macro by allowing definition
/// of a cascade of `#[cfg]` cases, emitting the implementation which matches
/// first.
///
/// This allows you to conveniently provide a long list #[cfg]'d blocks of code
/// without having to rewrite each clause multiple times.
macro_rules! cfg_if {
    ($(
        if #[cfg($($meta:meta),*)] { $($it:item)* }
    ) else * else {
        $($it2:item)*
    }) => {
        __cfg_if_items! {
            () ;
            $( ( ($($meta),*) ($($it)*) ), )*
            ( () ($($it2)*) ),
        }
    }
}

macro_rules! __cfg_if_items {
    (($($not:meta,)*) ; ) => {};
    (($($not:meta,)*) ; ( ($($m:meta),*) ($($it:item)*) ), $($rest:tt)*) => {
        __cfg_if_apply! { cfg(all(not(any($($not),*)), $($m,)*)), $($it)* }
        __cfg_if_items! { ($($not,)* $($m,)*) ; $($rest)* }
    }
}

macro_rules! __cfg_if_apply {
    ($m:meta, $($it:item)*) => {
        $(#[$m] $it)*
    }
}

macro_rules! s {
    ($($(#[$attr:meta])* pub struct $i:ident { $($field:tt)* })*) => ($(
        __item! {
            #[repr(C)]
            $(#[$attr])*
            pub struct $i { $($field)* }
        }
        impl ::dox::Copy for $i {}
        impl ::dox::Clone for $i {
            fn clone(&self) -> $i { *self }
        }
    )*)
}

macro_rules! f {
    ($(pub fn $i:ident($($arg:ident: $argty:ty),*) -> $ret:ty {
        $($body:stmt);*
    })*) => ($(
        #[inline]
        #[cfg(not(dox))]
        pub unsafe extern fn $i($($arg: $argty),*) -> $ret {
            $($body);*
        }

        #[cfg(dox)]
        #[allow(dead_code)]
        pub unsafe extern fn $i($($arg: $argty),*) -> $ret {
            loop {}
        }
    )*)
}

macro_rules! __item {
    ($i:item) => ($i)
}
