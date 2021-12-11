pub use serde::de::{Deserialize, IntoDeserializer};

use value::{Value, Table, Array};

/// Construct a [`toml::Value`] from TOML syntax.
///
/// [`toml::Value`]: value/enum.Value.html
///
/// ```rust
/// #[macro_use]
/// extern crate toml;
///
/// fn main() {
///     let cargo_toml = toml! {
///         [package]
///         name = "toml"
///         version = "0.4.5"
///         authors = ["Alex Crichton <alex@alexcrichton.com>"]
///
///         [badges]
///         travis-ci = { repository = "alexcrichton/toml-rs" }
///
///         [dependencies]
///         serde = "1.0"
///
///         [dev-dependencies]
///         serde_derive = "1.0"
///         serde_json = "1.0"
///     };
///
///     println!("{:#?}", cargo_toml);
/// }
/// ```
#[macro_export]
macro_rules! toml {
    ($($toml:tt)+) => {{
        let table = $crate::value::Table::new();
        let mut root = $crate::Value::Table(table);
        toml_internal!(@toplevel root [] $($toml)+);
        root
    }};
}

// TT-muncher to parse TOML syntax into a toml::Value.
//
//    @toplevel -- Parse tokens outside of an inline table or inline array. In
//                 this state, `[table headers]` and `[[array headers]]` are
//                 allowed and `key = value` pairs are not separated by commas.
//
//    @topleveldatetime -- Helper to parse a Datetime from string and insert it
//                 into a table, continuing in the @toplevel state.
//
//    @path -- Turn a path segment into a string. Segments that look like idents
//                 are stringified, while quoted segments like `"cfg(windows)"`
//                 are not.
//
//    @value -- Parse the value part of a `key = value` pair, which may be a
//                 primitive or inline table or inline array.
//
//    @table -- Parse the contents of an inline table, returning them as a
//                 toml::Value::Table.
//
//    @tabledatetime -- Helper to parse a Datetime from string and insert it
//                 into a table, continuing in the @table state.
//
//    @array -- Parse the contents of an inline array, returning them as a
//                 toml::Value::Array.
//
//    @arraydatetime -- Helper to parse a Datetime from string and push it into
//                 an array, continuing in the @array state.
//
//    @trailingcomma -- Helper to append a comma to a sequence of tokens if the
//                 sequence is non-empty and does not already end in a trailing
//                 comma.
//
#[macro_export]
#[doc(hidden)]
macro_rules! toml_internal {
    // Base case, no elements remaining.
    (@toplevel $root:ident [$($path:tt)*]) => {};

    // Parse negative number `key = -value`.
    (@toplevel $root:ident [$($path:tt)*] $($($k:tt)-+).+ = - $v:tt $($rest:tt)*) => {
        toml_internal!(@toplevel $root [$($path)*] $($($k)-+).+ = (-$v) $($rest)*);
    };

    // Parse positive number `key = +value`.
    (@toplevel $root:ident [$($path:tt)*] $($($k:tt)-+).+ = + $v:tt $($rest:tt)*) => {
        toml_internal!(@toplevel $root [$($path)*] $($($k)-+).+ = ($v) $($rest)*);
    };

    // Parse offset datetime `key = 1979-05-27T00:32:00.999999-07:00`.
    (@toplevel $root:ident [$($path:tt)*] $($($k:tt)-+).+ = $yr:tt - $mo:tt - $dhr:tt : $min:tt : $sec:tt . $frac:tt - $tzh:tt : $tzm:tt $($rest:tt)*) => {
        toml_internal!(@topleveldatetime $root [$($path)*] $($($k)-+).+ = ($yr - $mo - $dhr : $min : $sec . $frac - $tzh : $tzm) $($rest)*);
    };
    // Space instead of T.
    (@toplevel $root:ident [$($path:tt)*] $($($k:tt)-+).+ = $yr:tt - $mo:tt - $day:tt $hr:tt : $min:tt : $sec:tt . $frac:tt - $tzh:tt : $tzm:tt $($rest:tt)*) => {
        toml_internal!(@topleveldatetime $root [$($path)*] $($($k)-+).+ = ($yr - $mo - $day T $hr : $min : $sec . $frac - $tzh : $tzm) $($rest)*);
    };

    // Parse offset datetime `key = 1979-05-27T00:32:00-07:00`.
    (@toplevel $root:ident [$($path:tt)*] $($($k:tt)-+).+ = $yr:tt - $mo:tt - $dhr:tt : $min:tt : $sec:tt - $tzh:tt : $tzm:tt $($rest:tt)*) => {
        toml_internal!(@topleveldatetime $root [$($path)*] $($($k)-+).+ = ($yr - $mo - $dhr : $min : $sec - $tzh : $tzm) $($rest)*);
    };
    // Space instead of T.
    (@toplevel $root:ident [$($path:tt)*] $($($k:tt)-+).+ = $yr:tt - $mo:tt - $day:tt $hr:tt : $min:tt : $sec:tt - $tzh:tt : $tzm:tt $($rest:tt)*) => {
        toml_internal!(@topleveldatetime $root [$($path)*] $($($k)-+).+ = ($yr - $mo - $day T $hr : $min : $sec - $tzh : $tzm) $($rest)*);
    };

    // Parse local datetime `key = 1979-05-27T00:32:00.999999`.
    (@toplevel $root:ident [$($path:tt)*] $($($k:tt)-+).+ = $yr:tt - $mo:tt - $dhr:tt : $min:tt : $sec:tt . $frac:tt $($rest:tt)*) => {
        toml_internal!(@topleveldatetime $root [$($path)*] $($($k)-+).+ = ($yr - $mo - $dhr : $min : $sec . $frac) $($rest)*);
    };
    // Space instead of T.
    (@toplevel $root:ident [$($path:tt)*] $($($k:tt)-+).+ = $yr:tt - $mo:tt - $day:tt $hr:tt : $min:tt : $sec:tt . $frac:tt $($rest:tt)*) => {
        toml_internal!(@topleveldatetime $root [$($path)*] $($($k)-+).+ = ($yr - $mo - $day T $hr : $min : $sec . $frac) $($rest)*);
    };

    // Parse offset datetime `key = 1979-05-27T07:32:00Z` and local datetime `key = 1979-05-27T07:32:00`.
    (@toplevel $root:ident [$($path:tt)*] $($($k:tt)-+).+ = $yr:tt - $mo:tt - $dhr:tt : $min:tt : $sec:tt $($rest:tt)*) => {
        toml_internal!(@topleveldatetime $root [$($path)*] $($($k)-+).+ = ($yr - $mo - $dhr : $min : $sec) $($rest)*);
    };
    // Space instead of T.
    (@toplevel $root:ident [$($path:tt)*] $($($k:tt)-+).+ = $yr:tt - $mo:tt - $day:tt $hr:tt : $min:tt : $sec:tt $($rest:tt)*) => {
        toml_internal!(@topleveldatetime $root [$($path)*] $($($k)-+).+ = ($yr - $mo - $day T $hr : $min : $sec) $($rest)*);
    };

    // Parse local date `key = 1979-05-27`.
    (@toplevel $root:ident [$($path:tt)*] $($($k:tt)-+).+ = $yr:tt - $mo:tt - $day:tt $($rest:tt)*) => {
        toml_internal!(@topleveldatetime $root [$($path)*] $($($k)-+).+ = ($yr - $mo - $day) $($rest)*);
    };

    // Parse local time `key = 00:32:00.999999`.
    (@toplevel $root:ident [$($path:tt)*] $($($k:tt)-+).+ = $hr:tt : $min:tt : $sec:tt . $frac:tt $($rest:tt)*) => {
        toml_internal!(@topleveldatetime $root [$($path)*] $($($k)-+).+ = ($hr : $min : $sec . $frac) $($rest)*);
    };

    // Parse local time `key = 07:32:00`.
    (@toplevel $root:ident [$($path:tt)*] $($($k:tt)-+).+ = $hr:tt : $min:tt : $sec:tt $($rest:tt)*) => {
        toml_internal!(@topleveldatetime $root [$($path)*] $($($k)-+).+ = ($hr : $min : $sec) $($rest)*);
    };

    // Parse any other `key = value` including string, inline array, inline
    // table, number, and boolean.
    (@toplevel $root:ident [$($path:tt)*] $($($k:tt)-+).+ = $v:tt $($rest:tt)*) => {{
        $crate::macros::insert_toml(
            &mut $root,
            &[$($path)* $(&concat!($("-", toml_internal!(@path $k),)+)[1..], )+],
            toml_internal!(@value $v));
        toml_internal!(@toplevel $root [$($path)*] $($rest)*);
    }};

    // Parse array header `[[bin]]`.
    (@toplevel $root:ident $oldpath:tt [[$($($path:tt)-+).+]] $($rest:tt)*) => {
        $crate::macros::push_toml(
            &mut $root,
            &[$(&concat!($("-", toml_internal!(@path $path),)+)[1..],)+]);
        toml_internal!(@toplevel $root [$(&concat!($("-", toml_internal!(@path $path),)+)[1..],)+] $($rest)*);
    };

    // Parse table header `[patch.crates-io]`.
    (@toplevel $root:ident $oldpath:tt [$($($path:tt)-+).+] $($rest:tt)*) => {
        $crate::macros::insert_toml(
            &mut $root,
            &[$(&concat!($("-", toml_internal!(@path $path),)+)[1..],)+],
            $crate::Value::Table($crate::value::Table::new()));
        toml_internal!(@toplevel $root [$(&concat!($("-", toml_internal!(@path $path),)+)[1..],)+] $($rest)*);
    };

    // Parse datetime from string and insert into table.
    (@topleveldatetime $root:ident [$($path:tt)*] $($($k:tt)-+).+ = ($($datetime:tt)+) $($rest:tt)*) => {
        $crate::macros::insert_toml(
            &mut $root,
            &[$($path)* $(&concat!($("-", toml_internal!(@path $k),)+)[1..], )+],
            $crate::Value::Datetime(concat!($(stringify!($datetime)),+).parse().unwrap()));
        toml_internal!(@toplevel $root [$($path)*] $($rest)*);
    };

    // Turn a path segment into a string.
    (@path $ident:ident) => {
        stringify!($ident)
    };

    // For a path segment that is not an ident, expect that it is already a
    // quoted string, like in `[target."cfg(windows)".dependencies]`.
    (@path $quoted:tt) => {
        $quoted
    };

    // Construct a Value from an inline table.
    (@value { $($inline:tt)* }) => {{
        let mut table = $crate::Value::Table($crate::value::Table::new());
        toml_internal!(@trailingcomma (@table table) $($inline)*);
        table
    }};

    // Construct a Value from an inline array.
    (@value [ $($inline:tt)* ]) => {{
        let mut array = $crate::value::Array::new();
        toml_internal!(@trailingcomma (@array array) $($inline)*);
        $crate::Value::Array(array)
    }};

    (@value (-nan)) => {
        $crate::Value::Float(-::std::f64::NAN)
    };

    (@value (nan)) => {
        $crate::Value::Float(::std::f64::NAN)
    };

    (@value nan) => {
        $crate::Value::Float(::std::f64::NAN)
    };

    (@value (-inf)) => {
        $crate::Value::Float(::std::f64::NEG_INFINITY)
    };

    (@value (inf)) => {
        $crate::Value::Float(::std::f64::INFINITY)
    };

    (@value inf) => {
        $crate::Value::Float(::std::f64::INFINITY)
    };

    // Construct a Value from any other type, probably string or boolean or number.
    (@value $v:tt) => {{
        // TODO: Implement this with something like serde_json::to_value instead.
        let de = $crate::macros::IntoDeserializer::<$crate::de::Error>::into_deserializer($v);
        <$crate::Value as $crate::macros::Deserialize>::deserialize(de).unwrap()
    }};

    // Base case of inline table.
    (@table $root:ident) => {};

    // Parse negative number `key = -value`.
    (@table $root:ident $($($k:tt)-+).+ = - $v:tt , $($rest:tt)*) => {
        toml_internal!(@table $root $($($k)-+).+ = (-$v) , $($rest)*);
    };

    // Parse positive number `key = +value`.
    (@table $root:ident $($($k:tt)-+).+ = + $v:tt , $($rest:tt)*) => {
        toml_internal!(@table $root $($($k)-+).+ = ($v) , $($rest)*);
    };

    // Parse offset datetime `key = 1979-05-27T00:32:00.999999-07:00`.
    (@table $root:ident $($($k:tt)-+).+ = $yr:tt - $mo:tt - $dhr:tt : $min:tt : $sec:tt . $frac:tt - $tzh:tt : $tzm:tt , $($rest:tt)*) => {
        toml_internal!(@tabledatetime $root $($($k)-+).+ = ($yr - $mo - $dhr : $min : $sec . $frac - $tzh : $tzm) $($rest)*);
    };
    // Space instead of T.
    (@table $root:ident $($($k:tt)-+).+ = $yr:tt - $mo:tt - $day:tt $hr:tt : $min:tt : $sec:tt . $frac:tt - $tzh:tt : $tzm:tt , $($rest:tt)*) => {
        toml_internal!(@tabledatetime $root $($($k)-+).+ = ($yr - $mo - $day T $hr : $min : $sec . $frac - $tzh : $tzm) $($rest)*);
    };

    // Parse offset datetime `key = 1979-05-27T00:32:00-07:00`.
    (@table $root:ident $($($k:tt)-+).+ = $yr:tt - $mo:tt - $dhr:tt : $min:tt : $sec:tt - $tzh:tt : $tzm:tt , $($rest:tt)*) => {
        toml_internal!(@tabledatetime $root $($($k)-+).+ = ($yr - $mo - $dhr : $min : $sec - $tzh : $tzm) $($rest)*);
    };
    // Space instead of T.
    (@table $root:ident $($($k:tt)-+).+ = $yr:tt - $mo:tt - $day:tt $hr:tt : $min:tt : $sec:tt - $tzh:tt : $tzm:tt , $($rest:tt)*) => {
        toml_internal!(@tabledatetime $root $($($k)-+).+ = ($yr - $mo - $day T $hr : $min : $sec - $tzh : $tzm) $($rest)*);
    };

    // Parse local datetime `key = 1979-05-27T00:32:00.999999`.
    (@table $root:ident $($($k:tt)-+).+ = $yr:tt - $mo:tt - $dhr:tt : $min:tt : $sec:tt . $frac:tt , $($rest:tt)*) => {
        toml_internal!(@tabledatetime $root $($($k)-+).+ = ($yr - $mo - $dhr : $min : $sec . $frac) $($rest)*);
    };
    // Space instead of T.
    (@table $root:ident $($($k:tt)-+).+ = $yr:tt - $mo:tt - $day:tt $hr:tt : $min:tt : $sec:tt . $frac:tt , $($rest:tt)*) => {
        toml_internal!(@tabledatetime $root $($($k)-+).+ = ($yr - $mo - $day T $hr : $min : $sec . $frac) $($rest)*);
    };

    // Parse offset datetime `key = 1979-05-27T07:32:00Z` and local datetime `key = 1979-05-27T07:32:00`.
    (@table $root:ident $($($k:tt)-+).+ = $yr:tt - $mo:tt - $dhr:tt : $min:tt : $sec:tt , $($rest:tt)*) => {
        toml_internal!(@tabledatetime $root $($($k)-+).+ = ($yr - $mo - $dhr : $min : $sec) $($rest)*);
    };
    // Space instead of T.
    (@table $root:ident $($($k:tt)-+).+ = $yr:tt - $mo:tt - $day:tt $hr:tt : $min:tt : $sec:tt , $($rest:tt)*) => {
        toml_internal!(@tabledatetime $root $($($k)-+).+ = ($yr - $mo - $day T $hr : $min : $sec) $($rest)*);
    };

    // Parse local date `key = 1979-05-27`.
    (@table $root:ident $($($k:tt)-+).+ = $yr:tt - $mo:tt - $day:tt , $($rest:tt)*) => {
        toml_internal!(@tabledatetime $root $($($k)-+).+ = ($yr - $mo - $day) $($rest)*);
    };

    // Parse local time `key = 00:32:00.999999`.
    (@table $root:ident $($($k:tt)-+).+ = $hr:tt : $min:tt : $sec:tt . $frac:tt , $($rest:tt)*) => {
        toml_internal!(@tabledatetime $root $($($k)-+).+ = ($hr : $min : $sec . $frac) $($rest)*);
    };

    // Parse local time `key = 07:32:00`.
    (@table $root:ident $($($k:tt)-+).+ = $hr:tt : $min:tt : $sec:tt , $($rest:tt)*) => {
        toml_internal!(@tabledatetime $root $($($k)-+).+ = ($hr : $min : $sec) $($rest)*);
    };

    // Parse any other type, probably string or boolean or number.
    (@table $root:ident $($($k:tt)-+).+ = $v:tt , $($rest:tt)*) => {
        $crate::macros::insert_toml(
            &mut $root,
            &[$(&concat!($("-", toml_internal!(@path $k),)+)[1..], )+],
            toml_internal!(@value $v));
        toml_internal!(@table $root $($rest)*);
    };

    // Parse a Datetime from string and continue in @table state.
    (@tabledatetime $root:ident $($($k:tt)-+).+ = ($($datetime:tt)*) $($rest:tt)*) => {
        $crate::macros::insert_toml(
            &mut $root,
            &[$(&concat!($("-", toml_internal!(@path $k),)+)[1..], )+],
            $crate::Value::Datetime(concat!($(stringify!($datetime)),+).parse().unwrap()));
        toml_internal!(@table $root $($rest)*);
    };

    // Base case of inline array.
    (@array $root:ident) => {};

    // Parse negative number `-value`.
    (@array $root:ident - $v:tt , $($rest:tt)*) => {
        toml_internal!(@array $root (-$v) , $($rest)*);
    };

    // Parse positive number `+value`.
    (@array $root:ident + $v:tt , $($rest:tt)*) => {
        toml_internal!(@array $root ($v) , $($rest)*);
    };

    // Parse offset datetime `1979-05-27T00:32:00.999999-07:00`.
    (@array $root:ident $yr:tt - $mo:tt - $dhr:tt : $min:tt : $sec:tt . $frac:tt - $tzh:tt : $tzm:tt , $($rest:tt)*) => {
        toml_internal!(@arraydatetime $root ($yr - $mo - $dhr : $min : $sec . $frac - $tzh : $tzm) $($rest)*);
    };
    // Space instead of T.
    (@array $root:ident $yr:tt - $mo:tt - $day:tt $hr:tt : $min:tt : $sec:tt . $frac:tt - $tzh:tt : $tzm:tt , $($rest:tt)*) => {
        toml_internal!(@arraydatetime $root ($yr - $mo - $day T $hr : $min : $sec . $frac - $tzh : $tzm) $($rest)*);
    };

    // Parse offset datetime `1979-05-27T00:32:00-07:00`.
    (@array $root:ident $yr:tt - $mo:tt - $dhr:tt : $min:tt : $sec:tt - $tzh:tt : $tzm:tt , $($rest:tt)*) => {
        toml_internal!(@arraydatetime $root ($yr - $mo - $dhr : $min : $sec - $tzh : $tzm) $($rest)*);
    };
    // Space instead of T.
    (@array $root:ident $yr:tt - $mo:tt - $day:tt $hr:tt : $min:tt : $sec:tt - $tzh:tt : $tzm:tt , $($rest:tt)*) => {
        toml_internal!(@arraydatetime $root ($yr - $mo - $day T $hr : $min : $sec - $tzh : $tzm) $($rest)*);
    };

    // Parse local datetime `1979-05-27T00:32:00.999999`.
    (@array $root:ident $yr:tt - $mo:tt - $dhr:tt : $min:tt : $sec:tt . $frac:tt , $($rest:tt)*) => {
        toml_internal!(@arraydatetime $root ($yr - $mo - $dhr : $min : $sec . $frac) $($rest)*);
    };
    // Space instead of T.
    (@array $root:ident $yr:tt - $mo:tt - $day:tt $hr:tt : $min:tt : $sec:tt . $frac:tt , $($rest:tt)*) => {
        toml_internal!(@arraydatetime $root ($yr - $mo - $day T $hr : $min : $sec . $frac) $($rest)*);
    };

    // Parse offset datetime `1979-05-27T07:32:00Z` and local datetime `1979-05-27T07:32:00`.
    (@array $root:ident $yr:tt - $mo:tt - $dhr:tt : $min:tt : $sec:tt , $($rest:tt)*) => {
        toml_internal!(@arraydatetime $root ($yr - $mo - $dhr : $min : $sec) $($rest)*);
    };
    // Space instead of T.
    (@array $root:ident $yr:tt - $mo:tt - $day:tt $hr:tt : $min:tt : $sec:tt , $($rest:tt)*) => {
        toml_internal!(@arraydatetime $root ($yr - $mo - $day T $hr : $min : $sec) $($rest)*);
    };

    // Parse local date `1979-05-27`.
    (@array $root:ident $yr:tt - $mo:tt - $day:tt , $($rest:tt)*) => {
        toml_internal!(@arraydatetime $root ($yr - $mo - $day) $($rest)*);
    };

    // Parse local time `00:32:00.999999`.
    (@array $root:ident $hr:tt : $min:tt : $sec:tt . $frac:tt , $($rest:tt)*) => {
        toml_internal!(@arraydatetime $root ($hr : $min : $sec . $frac) $($rest)*);
    };

    // Parse local time `07:32:00`.
    (@array $root:ident $hr:tt : $min:tt : $sec:tt , $($rest:tt)*) => {
        toml_internal!(@arraydatetime $root ($hr : $min : $sec) $($rest)*);
    };

    // Parse any other type, probably string or boolean or number.
    (@array $root:ident $v:tt , $($rest:tt)*) => {
        $root.push(toml_internal!(@value $v));
        toml_internal!(@array $root $($rest)*);
    };

    // Parse a Datetime from string and continue in @array state.
    (@arraydatetime $root:ident ($($datetime:tt)*) $($rest:tt)*) => {
        $root.push($crate::Value::Datetime(concat!($(stringify!($datetime)),+).parse().unwrap()));
        toml_internal!(@array $root $($rest)*);
    };

    // No trailing comma required if the tokens are empty.
    (@trailingcomma ($($args:tt)*)) => {
        toml_internal!($($args)*);
    };

    // Tokens end with a trailing comma, do not append another one.
    (@trailingcomma ($($args:tt)*) ,) => {
        toml_internal!($($args)* ,);
    };

    // Tokens end with something other than comma, append a trailing comma.
    (@trailingcomma ($($args:tt)*) $last:tt) => {
        toml_internal!($($args)* $last ,);
    };

    // Not yet at the last token.
    (@trailingcomma ($($args:tt)*) $first:tt $($rest:tt)+) => {
        toml_internal!(@trailingcomma ($($args)* $first) $($rest)+);
    };
}

// Called when parsing a `key = value` pair.
// Inserts an entry into the table at the given path.
pub fn insert_toml(root: &mut Value, path: &[&str], value: Value) {
    *traverse(root, path) = value;
}

// Called when parsing an `[[array header]]`.
// Pushes an empty table onto the array at the given path.
pub fn push_toml(root: &mut Value, path: &[&str]) {
    let target = traverse(root, path);
    if !target.is_array() {
        *target = Value::Array(Array::new());
    }
    target.as_array_mut().unwrap().push(Value::Table(Table::new()));
}

fn traverse<'a>(root: &'a mut Value, path: &[&str]) -> &'a mut Value {
    let mut cur = root;
    for &key in path {
        // Lexical lifetimes :D
        let cur1 = cur;
        let cur2;

        // From the TOML spec:
        //
        // > Each double-bracketed sub-table will belong to the most recently
        // > defined table element above it.
        if cur1.is_array() {
            cur2 = cur1.as_array_mut().unwrap().last_mut().unwrap();
        } else {
            cur2 = cur1;
        };

        // We are about to index into this value, so it better be a table.
        if !cur2.is_table() {
            *cur2 = Value::Table(Table::new());
        }

        if !cur2.as_table().unwrap().contains_key(key) {
            // Insert an empty table for the next loop iteration to point to.
            let empty = Value::Table(Table::new());
            cur2.as_table_mut().unwrap().insert(key.to_owned(), empty);
        }

        // Step into the current table.
        cur = cur2.as_table_mut().unwrap().get_mut(key).unwrap();
    }
    cur
}
