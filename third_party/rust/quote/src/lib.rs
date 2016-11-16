mod tokens;
pub use tokens::Tokens;

mod to_tokens;
pub use to_tokens::{ToTokens, ByteStr};

#[macro_export]
macro_rules! quote {
    () => {
        $crate::Tokens::new()
    };

    ($($tt:tt)+) => {
        {
            #[allow(unused_imports)]
            use $crate::ToTokens;
            let mut _s = $crate::Tokens::new();
            quote_each_token!(_s $($tt)*);
            _s
        }
    };
}

// Extract the names of all #metavariables and pass them to the $finish macro.
//
// in:   pounded_var_names!(then () a #b c #( #d )* #e)
// out:  then!(() b d e)
#[macro_export]
#[doc(hidden)]
macro_rules! pounded_var_names {
    ($finish:ident ($($found:ident)*) # ( $($inner:tt)* ) $($rest:tt)*) => {
        pounded_var_names!($finish ($($found)*) $($inner)* $($rest)*)
    };

    ($finish:ident ($($found:ident)*) # [ $($inner:tt)* ] $($rest:tt)*) => {
        pounded_var_names!($finish ($($found)*) $($inner)* $($rest)*)
    };

    ($finish:ident ($($found:ident)*) # { $($inner:tt)* } $($rest:tt)*) => {
        pounded_var_names!($finish ($($found)*) $($inner)* $($rest)*)
    };

    ($finish:ident ($($found:ident)*) # $first:ident $($rest:tt)*) => {
        pounded_var_names!($finish ($($found)* $first) $($rest)*)
    };

    ($finish:ident ($($found:ident)*) ( $($inner:tt)* ) $($rest:tt)*) => {
        pounded_var_names!($finish ($($found)*) $($inner)* $($rest)*)
    };

    ($finish:ident ($($found:ident)*) [ $($inner:tt)* ] $($rest:tt)*) => {
        pounded_var_names!($finish ($($found)*) $($inner)* $($rest)*)
    };

    ($finish:ident ($($found:ident)*) { $($inner:tt)* } $($rest:tt)*) => {
        pounded_var_names!($finish ($($found)*) $($inner)* $($rest)*)
    };

    ($finish:ident ($($found:ident)*) $ignore:tt $($rest:tt)*) => {
        pounded_var_names!($finish ($($found)*) $($rest)*)
    };

    ($finish:ident ($($found:ident)*)) => {
        $finish!(() $($found)*)
    };
}

// in:   nested_tuples_pat!(() a b c d e)
// out:  ((((a b) c) d) e)
//
// in:   nested_tuples_pat!(() a)
// out:  a
#[macro_export]
#[doc(hidden)]
macro_rules! nested_tuples_pat {
    (()) => {
        &()
    };

    (() $first:ident $($rest:ident)*) => {
        nested_tuples_pat!(($first) $($rest)*)
    };

    (($pat:pat) $first:ident $($rest:ident)*) => {
        nested_tuples_pat!((($pat, $first)) $($rest)*)
    };

    (($done:pat)) => {
        $done
    };
}

// in:   multi_zip_expr!(() a b c d e)
// out:  a.into_iter().zip(b).zip(c).zip(d).zip(e)
//
// in:   multi_zip_iter!(() a)
// out:  a
#[macro_export]
#[doc(hidden)]
macro_rules! multi_zip_expr {
    (()) => {
        &[]
    };

    (() $single:ident) => {
        $single
    };

    (() $first:ident $($rest:ident)*) => {
        multi_zip_expr!(($first.into_iter()) $($rest)*)
    };

    (($zips:expr) $first:ident $($rest:ident)*) => {
        multi_zip_expr!(($zips.zip($first)) $($rest)*)
    };

    (($done:expr)) => {
        $done
    };
}

#[macro_export]
#[doc(hidden)]
macro_rules! quote_each_token {
    ($tokens:ident) => {};

    ($tokens:ident # ! $($rest:tt)*) => {
        $tokens.append("#");
        $tokens.append("!");
        quote_each_token!($tokens $($rest)*);
    };

    ($tokens:ident # ( $($inner:tt)* ) * $($rest:tt)*) => {
        for pounded_var_names!(nested_tuples_pat () $($inner)*)
        in pounded_var_names!(multi_zip_expr () $($inner)*) {
            quote_each_token!($tokens $($inner)*);
        }
        quote_each_token!($tokens $($rest)*);
    };

    ($tokens:ident # ( $($inner:tt)* ) $sep:tt * $($rest:tt)*) => {
        for (_i, pounded_var_names!(nested_tuples_pat () $($inner)*))
        in pounded_var_names!(multi_zip_expr () $($inner)*).into_iter().enumerate() {
            if _i > 0 {
                $tokens.append(stringify!($sep));
            }
            quote_each_token!($tokens $($inner)*);
        }
        quote_each_token!($tokens $($rest)*);
    };

    ($tokens:ident # [ $($inner:tt)* ] $($rest:tt)*) => {
        $tokens.append("#");
        $tokens.append("[");
        quote_each_token!($tokens $($inner)*);
        $tokens.append("]");
        quote_each_token!($tokens $($rest)*);
    };

    ($tokens:ident # $first:ident $($rest:tt)*) => {
        $first.to_tokens(&mut $tokens);
        quote_each_token!($tokens $($rest)*);
    };

    ($tokens:ident ( $($first:tt)* ) $($rest:tt)*) => {
        $tokens.append("(");
        quote_each_token!($tokens $($first)*);
        $tokens.append(")");
        quote_each_token!($tokens $($rest)*);
    };

    ($tokens:ident [ $($first:tt)* ] $($rest:tt)*) => {
        $tokens.append("[");
        quote_each_token!($tokens $($first)*);
        $tokens.append("]");
        quote_each_token!($tokens $($rest)*);
    };

    ($tokens:ident { $($first:tt)* } $($rest:tt)*) => {
        $tokens.append("{");
        quote_each_token!($tokens $($first)*);
        $tokens.append("}");
        quote_each_token!($tokens $($rest)*);
    };

    ($tokens:ident $first:tt $($rest:tt)*) => {
        $tokens.append(stringify!($first));
        quote_each_token!($tokens $($rest)*);
    };
}
