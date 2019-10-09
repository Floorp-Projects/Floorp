macro_rules! ast_struct {
    (
        [$($attrs_pub:tt)*]
        struct $name:ident #full $($rest:tt)*
    ) => {
        #[cfg(feature = "full")]
        #[cfg_attr(feature = "extra-traits", derive(Debug, Eq, PartialEq, Hash))]
        #[cfg_attr(feature = "clone-impls", derive(Clone))]
        $($attrs_pub)* struct $name $($rest)*

        #[cfg(not(feature = "full"))]
        #[cfg_attr(feature = "extra-traits", derive(Debug, Eq, PartialEq, Hash))]
        #[cfg_attr(feature = "clone-impls", derive(Clone))]
        $($attrs_pub)* struct $name {
            _noconstruct: (),
        }

        #[cfg(all(not(feature = "full"), feature = "printing"))]
        impl ::quote::ToTokens for $name {
            fn to_tokens(&self, _: &mut ::proc_macro2::TokenStream) {
                unreachable!()
            }
        }
    };

    (
        [$($attrs_pub:tt)*]
        struct $name:ident #manual_extra_traits $($rest:tt)*
    ) => {
        #[cfg_attr(feature = "extra-traits", derive(Debug))]
        #[cfg_attr(feature = "clone-impls", derive(Clone))]
        $($attrs_pub)* struct $name $($rest)*
    };

    (
        [$($attrs_pub:tt)*]
        struct $name:ident #manual_extra_traits_debug $($rest:tt)*
    ) => {
        #[cfg_attr(feature = "clone-impls", derive(Clone))]
        $($attrs_pub)* struct $name $($rest)*
    };

    (
        [$($attrs_pub:tt)*]
        struct $name:ident $($rest:tt)*
    ) => {
        #[cfg_attr(feature = "extra-traits", derive(Debug, Eq, PartialEq, Hash))]
        #[cfg_attr(feature = "clone-impls", derive(Clone))]
        $($attrs_pub)* struct $name $($rest)*
    };

    ($($t:tt)*) => {
        strip_attrs_pub!(ast_struct!($($t)*));
    };
}

macro_rules! ast_enum {
    // Drop the `#no_visit` attribute, if present.
    (
        [$($attrs_pub:tt)*]
        enum $name:ident #no_visit $($rest:tt)*
    ) => (
        ast_enum!([$($attrs_pub)*] enum $name $($rest)*);
    );

    (
        [$($attrs_pub:tt)*]
        enum $name:ident #manual_extra_traits $($rest:tt)*
    ) => (
        #[cfg_attr(feature = "extra-traits", derive(Debug))]
        #[cfg_attr(feature = "clone-impls", derive(Clone))]
        $($attrs_pub)* enum $name $($rest)*
    );

    (
        [$($attrs_pub:tt)*]
        enum $name:ident $($rest:tt)*
    ) => (
        #[cfg_attr(feature = "extra-traits", derive(Debug, Eq, PartialEq, Hash))]
        #[cfg_attr(feature = "clone-impls", derive(Clone))]
        $($attrs_pub)* enum $name $($rest)*
    );

    ($($t:tt)*) => {
        strip_attrs_pub!(ast_enum!($($t)*));
    };
}

macro_rules! ast_enum_of_structs {
    (
        $(#[$enum_attr:meta])*
        $pub:ident $enum:ident $name:ident #$tag:ident $body:tt
        $($remaining:tt)*
    ) => {
        ast_enum!($(#[$enum_attr])* $pub $enum $name #$tag $body);
        ast_enum_of_structs_impl!($pub $enum $name $body $($remaining)*);
    };

    (
        $(#[$enum_attr:meta])*
        $pub:ident $enum:ident $name:ident $body:tt
        $($remaining:tt)*
    ) => {
        ast_enum!($(#[$enum_attr])* $pub $enum $name $body);
        ast_enum_of_structs_impl!($pub $enum $name $body $($remaining)*);
    };
}

macro_rules! ast_enum_of_structs_impl {
    (
        $pub:ident $enum:ident $name:ident {
            $(
                $(#[$variant_attr:meta])*
                $variant:ident $( ($member:ident) )*,
            )*
        }

        $($remaining:tt)*
    ) => {
        check_keyword_matches!(pub $pub);
        check_keyword_matches!(enum $enum);

        $(
            $(
                impl From<$member> for $name {
                    fn from(e: $member) -> $name {
                        $name::$variant(e)
                    }
                }
            )*
        )*

        #[cfg(feature = "printing")]
        generate_to_tokens! {
            $($remaining)*
            ()
            tokens
            $name { $($variant $($member)*,)* }
        }
    };
}

#[cfg(feature = "printing")]
macro_rules! generate_to_tokens {
    (do_not_generate_to_tokens $($foo:tt)*) => ();

    (($($arms:tt)*) $tokens:ident $name:ident { $variant:ident, $($next:tt)*}) => {
        generate_to_tokens!(
            ($($arms)* $name::$variant => {})
            $tokens $name { $($next)* }
        );
    };

    (($($arms:tt)*) $tokens:ident $name:ident { $variant:ident $member:ident, $($next:tt)*}) => {
        generate_to_tokens!(
            ($($arms)* $name::$variant(_e) => _e.to_tokens($tokens),)
            $tokens $name { $($next)* }
        );
    };

    (($($arms:tt)*) $tokens:ident $name:ident {}) => {
        impl ::quote::ToTokens for $name {
            fn to_tokens(&self, $tokens: &mut ::proc_macro2::TokenStream) {
                match self {
                    $($arms)*
                }
            }
        }
    };
}

macro_rules! strip_attrs_pub {
    ($mac:ident!($(#[$m:meta])* $pub:ident $($t:tt)*)) => {
        check_keyword_matches!(pub $pub);

        $mac!([$(#[$m])* $pub] $($t)*);
    };
}

macro_rules! check_keyword_matches {
    (struct struct) => {};
    (enum enum) => {};
    (pub pub) => {};
}
