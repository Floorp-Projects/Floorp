//! A macro for defining `#[cfg]` if-else statements.
//!
//! The macro provided by this crate, `cfg_if`, is similar to the `if/elif` C
//! preprocessor macro by allowing definition of a cascade of `#[cfg]` cases,
//! emitting the implementation which matches first.
//!
//! This allows you to conveniently provide a long list `#[cfg]`'d blocks of code
//! without having to rewrite each clause multiple times.
//!
//! # Example
//!
//! ```
//! #[macro_use]
//! extern crate cfg_if;
//!
//! cfg_if! {
//!     if #[cfg(unix)] {
//!         fn foo() { /* unix specific functionality */ }
//!     } else if #[cfg(target_pointer_width = "32")] {
//!         fn foo() { /* non-unix, 32-bit functionality */ }
//!     } else {
//!         fn foo() { /* fallback implementation */ }
//!     }
//! }
//!
//! # fn main() {}
//! ```

#![no_std]

#![doc(html_root_url = "https://docs.rs/cfg-if")]
#![deny(missing_docs)]
#![cfg_attr(test, deny(warnings))]

#[macro_export(local_inner_macros)]
macro_rules! cfg_if {
    // match if/else chains with a final `else`
    ($(
        if #[cfg($($meta:meta),*)] { $($it:item)* }
    ) else * else {
        $($it2:item)*
    }) => {
        cfg_if! {
            @__items
            () ;
            $( ( ($($meta),*) ($($it)*) ), )*
            ( () ($($it2)*) ),
        }
    };

    // match if/else chains lacking a final `else`
    (
        if #[cfg($($i_met:meta),*)] { $($i_it:item)* }
        $(
            else if #[cfg($($e_met:meta),*)] { $($e_it:item)* }
        )*
    ) => {
        cfg_if! {
            @__items
            () ;
            ( ($($i_met),*) ($($i_it)*) ),
            $( ( ($($e_met),*) ($($e_it)*) ), )*
            ( () () ),
        }
    };

    // Internal and recursive macro to emit all the items
    //
    // Collects all the negated cfgs in a list at the beginning and after the
    // semicolon is all the remaining items
    (@__items ($($not:meta,)*) ; ) => {};
    (@__items ($($not:meta,)*) ; ( ($($m:meta),*) ($($it:item)*) ), $($rest:tt)*) => {
        // Emit all items within one block, applying an approprate #[cfg]. The
        // #[cfg] will require all `$m` matchers specified and must also negate
        // all previous matchers.
        cfg_if! { @__apply cfg(all($($m,)* not(any($($not),*)))), $($it)* }

        // Recurse to emit all other items in `$rest`, and when we do so add all
        // our `$m` matchers to the list of `$not` matchers as future emissions
        // will have to negate everything we just matched as well.
        cfg_if! { @__items ($($not,)* $($m,)*) ; $($rest)* }
    };

    // Internal macro to Apply a cfg attribute to a list of items
    (@__apply $m:meta, $($it:item)*) => {
        $(#[$m] $it)*
    };
}

#[cfg(test)]
mod tests {
    cfg_if! {
        if #[cfg(test)] {
            use core::option::Option as Option2;
            fn works1() -> Option2<u32> { Some(1) }
        } else {
            fn works1() -> Option<u32> { None }
        }
    }

    cfg_if! {
        if #[cfg(foo)] {
            fn works2() -> bool { false }
        } else if #[cfg(test)] {
            fn works2() -> bool { true }
        } else {
            fn works2() -> bool { false }
        }
    }

    cfg_if! {
        if #[cfg(foo)] {
            fn works3() -> bool { false }
        } else {
            fn works3() -> bool { true }
        }
    }

    cfg_if! {
        if #[cfg(test)] {
            use core::option::Option as Option3;
            fn works4() -> Option3<u32> { Some(1) }
        }
    }

    cfg_if! {
        if #[cfg(foo)] {
            fn works5() -> bool { false }
        } else if #[cfg(test)] {
            fn works5() -> bool { true }
        }
    }

    #[test]
    fn it_works() {
        assert!(works1().is_some());
        assert!(works2());
        assert!(works3());
        assert!(works4().is_some());
        assert!(works5());
    }
}
