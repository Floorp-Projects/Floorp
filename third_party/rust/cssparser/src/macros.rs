/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use matches::matches;
use std::mem::MaybeUninit;

/// Expands to a `match` expression with string patterns,
/// matching case-insensitively in the ASCII range.
///
/// The patterns must not contain ASCII upper case letters. (They must be already be lower-cased.)
///
/// # Example
///
/// ```rust
/// #[macro_use] extern crate cssparser;
///
/// # fn main() {}  // Make doctest not wrap everythig in its own main
/// # fn dummy(function_name: &String) { let _ =
/// match_ignore_ascii_case! { &function_name,
///     "rgb" => parse_rgb(..),
/// #   #[cfg(not(something))]
///     "rgba" => parse_rgba(..),
///     "hsl" => parse_hsl(..),
///     "hsla" => parse_hsla(..),
///     _ => Err(format!("unknown function: {}", function_name))
/// }
/// # ;}
/// # use std::ops::RangeFull;
/// # fn parse_rgb(_: RangeFull) -> Result<(), String> { Ok(()) }
/// # fn parse_rgba(_: RangeFull) -> Result<(), String> { Ok(()) }
/// # fn parse_hsl(_: RangeFull) -> Result<(), String> { Ok(()) }
/// # fn parse_hsla(_: RangeFull) -> Result<(), String> { Ok(()) }
/// ```
#[macro_export]
macro_rules! match_ignore_ascii_case {
    ( $input:expr,
        $(
            $( #[$meta: meta] )*
            $( $pattern: pat )|+ $( if $guard: expr )? => $then: expr
        ),+
        $(,)?
    ) => {
        {
            // This dummy module works around the feature gate
            // `error[E0658]: procedural macros cannot be expanded to statements`
            // by forcing the macro to be in an item context
            // rather than expression/statement context,
            // even though the macro only expands to items.
            mod cssparser_internal {
                $crate::_cssparser_internal_max_len! {
                    $( $( $pattern )+ )+
                }
            }
            _cssparser_internal_to_lowercase!($input, cssparser_internal::MAX_LENGTH => lowercase);
            // "A" is a short string that we know is different for every string pattern,
            // since we’ve verified that none of them include ASCII upper case letters.
            match lowercase.unwrap_or("A") {
                $(
                    $( #[$meta] )*
                    $( $pattern )|+ $( if $guard )? => $then,
                )+
            }
        }
    };
}

/// Define a function `$name(&str) -> Option<&'static $ValueType>`
///
/// The function finds a match for the input string
/// in a [`phf` map](https://github.com/sfackler/rust-phf)
/// and returns a reference to the corresponding value.
/// Matching is case-insensitive in the ASCII range.
///
/// ## Example:
///
/// ```rust
/// #[macro_use] extern crate cssparser;
///
/// # fn main() {}  // Make doctest not wrap everything in its own main
///
/// fn color_rgb(input: &str) -> Option<(u8, u8, u8)> {
///     ascii_case_insensitive_phf_map! {
///         keyword -> (u8, u8, u8) = {
///             "red" => (255, 0, 0),
///             "green" => (0, 255, 0),
///             "blue" => (0, 0, 255),
///         }
///     }
///     keyword(input).cloned()
/// }
#[macro_export]
macro_rules! ascii_case_insensitive_phf_map {
    ($name: ident -> $ValueType: ty = { $( $key: tt => $value: expr ),+ }) => {
        ascii_case_insensitive_phf_map!($name -> $ValueType = { $( $key => $value, )+ })
    };
    ($name: ident -> $ValueType: ty = { $( $key: tt => $value: expr, )+ }) => {
        pub fn $name(input: &str) -> Option<&'static $ValueType> {
            // This dummy module works around a feature gate,
            // see comment on the similar module in `match_ignore_ascii_case!` above.
            mod _cssparser_internal {
                $crate::_cssparser_internal_max_len! {
                    $( $key )+
                }
            }
            use $crate::_cssparser_internal_phf as phf;
            static MAP: phf::Map<&'static str, $ValueType> = phf::phf_map! {
                $(
                    $key => $value,
                )*
            };
            _cssparser_internal_to_lowercase!(input, _cssparser_internal::MAX_LENGTH => lowercase);
            lowercase.and_then(|s| MAP.get(s))
        }
    }
}

/// Implementation detail of match_ignore_ascii_case! and ascii_case_insensitive_phf_map! macros.
///
/// **This macro is not part of the public API. It can change or be removed between any versions.**
///
/// Define a local variable named `$output`
/// and assign it the result of calling `_cssparser_internal_to_lowercase`
/// with a stack-allocated buffer of length `$BUFFER_SIZE`.
#[macro_export]
#[doc(hidden)]
macro_rules! _cssparser_internal_to_lowercase {
    ($input: expr, $BUFFER_SIZE: expr => $output: ident) => {
        #[allow(unsafe_code)]
        let mut buffer = unsafe {
            ::std::mem::MaybeUninit::<[::std::mem::MaybeUninit<u8>; $BUFFER_SIZE]>::uninit()
                .assume_init()
        };
        let input: &str = $input;
        let $output = $crate::_cssparser_internal_to_lowercase(&mut buffer, input);
    };
}

/// Implementation detail of match_ignore_ascii_case! and ascii_case_insensitive_phf_map! macros.
///
/// **This function is not part of the public API. It can change or be removed between any verisons.**
///
/// If `input` is larger than buffer, return `None`.
/// Otherwise, return `input` ASCII-lowercased, using `buffer` as temporary space if necessary.
#[doc(hidden)]
#[allow(non_snake_case)]
pub fn _cssparser_internal_to_lowercase<'a>(
    buffer: &'a mut [MaybeUninit<u8>],
    input: &'a str,
) -> Option<&'a str> {
    if let Some(buffer) = buffer.get_mut(..input.len()) {
        if let Some(first_uppercase) = input.bytes().position(|byte| matches!(byte, b'A'..=b'Z')) {
            unsafe {
                // This cast doesn’t change the pointer’s validity
                // since `u8` has the same layout as `MaybeUninit<u8>`:
                let input_bytes = &*(input.as_bytes() as *const [u8] as *const [MaybeUninit<u8>]);

                buffer.copy_from_slice(&*input_bytes);

                // Same as above re layout, plus these bytes have been initialized:
                let buffer = &mut *(buffer as *mut [MaybeUninit<u8>] as *mut [u8]);

                buffer[first_uppercase..].make_ascii_lowercase();
                // `buffer` was initialized to a copy of `input`
                // (which is `&str` so well-formed UTF-8)
                // then ASCII-lowercased (which preserves UTF-8 well-formedness):
                Some(::std::str::from_utf8_unchecked(buffer))
            }
        } else {
            // Input is already lower-case
            Some(input)
        }
    } else {
        // Input is longer than buffer, which has the length of the longest expected string:
        // none of the expected strings would match.
        None
    }
}

#[cfg(feature = "dummy_match_byte")]
macro_rules! match_byte {
    ($value:expr, $($rest:tt)* ) => {
        match $value {
            $(
                $rest
            )+
        }
    };
}
