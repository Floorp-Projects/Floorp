macro_rules! parser {
    ($submac:ident!( $($args:tt)* )) => {
        fn parse(input: $crate::CompleteStr<'a>) -> $crate::IResult<$crate::CompleteStr<'a>, Self> {
            $submac!(input, $($args)*)
        }
    };
}

macro_rules! weedle {
    ($i:expr, $t:ty) => {
        <$t as $crate::Parse<'a>>::parse($i)
    };
}

macro_rules! ast_types {
    (@extract_type struct $name:ident<'a> $($rest:tt)*) => ($name<'a>);
    (@extract_type struct $name:ident $($rest:tt)*) => ($name);
    (@extract_type enum $name:ident<'a> $($rest:tt)*) => ($name<'a>);
    (@extract_type enum $name:ident $($rest:tt)*) => ($name);

    () => ();
    (
        $(#[$attr:meta])*
        struct $name:ident<'a> {
            $($fields:tt)*
        }
        $($rest:tt)*
    ) => (
        __ast_struct! {
            @launch_pad
            $(#[$attr])*
            $name
            [ 'a ]
            [ ]
            { $($fields)* }
        }
        ast_types!($($rest)*);
    );
    (
        $(#[$attr:meta])*
        struct $name:ident<$($generics:ident),+> where [$($bounds:tt)+] {
            $($fields:tt)*
        }
        $($rest:tt)*
    ) => (
        __ast_struct! {
            @launch_pad
            $(#[$attr])*
            $name
            [$($generics)+]
            [$($bounds)+]
            { $($fields)* }
        }
        ast_types!($($rest)*);
    );
    (
        $(#[$attr:meta])*
        struct $name:ident {
            $($fields:tt)*
        }
        $($rest:tt)*
    ) => (
        __ast_struct! {
            @launch_pad
            $(#[$attr])*
            $name
            [ ]
            [ ]
            { $($fields)* }
        }
        ast_types!($($rest)*);
    );

    (
        $(#[$attr:meta])*
        struct $name:ident<'a> (
            $($fields:tt)*
        )
        $($rest:tt)*
    ) => (
        __ast_tuple_struct! {
            @launch_pad
            $(#[$attr])*
            $name
            [ 'a ]
            ( $($fields)* )
        }
        ast_types!($($rest)*);
    );
    (
        $(#[$attr:meta])*
        struct $name:ident (
            $($fields:tt)*
        )
        $($rest:tt)*
    ) => (
        __ast_tuple_struct! {
            @launch_pad
            $(#[$attr])*
            $name
            [ ]
            ( $($fields)* )
        }
        ast_types!($($rest)*);
    );

    (
        $(#[$attr:meta])*
        enum $name:ident<'a> {
            $($variants:tt)*
        }
        $($rest:tt)*
    ) => (
        __ast_enum! {
            @launch_pad
            $(#[$attr])*
            $name
            [ 'a ]
            { $($variants)* }
        }
        ast_types!($($rest)*);
    );
    (
        $(#[$attr:meta])*
        enum $name:ident {
            $($variants:tt)*
        }
        $($rest:tt)*
    ) => (
        __ast_enum! {
            @launch_pad
            $(#[$attr])*
            $name
            [ ]
            { $($variants)* }
        }
        ast_types!($($rest)*);
    );
}

macro_rules! __ast_tuple_struct {
    (@launch_pad
        $(#[$attr:meta])*
        $name:ident
        [ $($maybe_a:tt)* ]
        ( $inner:ty = $submac:ident!( $($args:tt)* ), )
    ) => (
        $(#[$attr])*
        #[derive(Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
        pub struct $name<$($maybe_a)*>(pub $inner);

        impl<'a> $crate::Parse<'a> for $name<$($maybe_a)*> {
            fn parse(input: $crate::CompleteStr<'a>) -> $crate::IResult<$crate::CompleteStr<'a>, Self> {
                use $crate::nom::lib::std::result::Result::*;

                match $submac!(input, $($args)*) {
                    Err(e) => Err(e),
                    Ok((i, inner)) => Ok((i, $name(inner))),
                }
            }
        }
    );
    (@launch_pad
        $(#[$attr:meta])*
        $name:ident
        [ $($maybe_a:tt)* ]
        ( $inner:ty, )
    ) => (
        __ast_tuple_struct! {
            @launch_pad
            $(#[$attr])*
            $name
            [ $($maybe_a)* ]
            ( $inner = weedle!($inner), )
        }
    );
}

macro_rules! __ast_struct {
    (@build_struct_decl
        {
            $(#[$attr:meta])*
            $name:ident
            [ $($generics:tt)* ]
            $($field:ident : $type:ty)*
        }
        { }
    ) => {
        $(#[$attr])*
        #[derive(Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
        pub struct $name<$($generics)*> {
            $(pub $field : $type,)*
        }
    };
    (@build_struct_decl
        { $($prev:tt)* }
        { $field:ident : $type:ty, $($rest:tt)* }
    ) => (
        __ast_struct! {
            @build_struct_decl
            { $($prev)* $field : $type }
            { $($rest)* }
        }
    );
    (@build_struct_decl
        { $($prev:tt)* }
        { $field:ident : $type:ty = $submac:ident!( $($args:tt)* ), $($rest:tt)* }
    ) => (
        __ast_struct! {
            @build_struct_decl
            { $($prev)* $field : $type }
            { $($rest)* }
        }
    );
    (@build_struct_decl
        { $($prev:tt)* }
        { $field:ident : $type:ty = marker, $($rest:tt)* }
    ) => (
        __ast_struct! {
            @build_struct_decl
            { $($prev)* $field : $type }
            { $($rest)* }
        }
    );

    (@build_parser
        { $i:expr, $($field:ident)* }
        { }
    ) => ({
        use $crate::nom::lib::std::result::Result::Ok;
        Ok(($i, Self { $($field,)* }))
    });
    (@build_parser
        { $i:expr, $($prev:tt)* }
        { $field:ident : $type:ty = $submac:ident!( $($args:tt)* ), $($rest:tt)* }
    ) => ({
        use $crate::nom::lib::std::result::Result::*;

        match $submac!($i, $($args)*) {
            Err(e) => Err(e),
            Ok((i, $field)) => {
                __ast_struct! {
                    @build_parser
                    { i, $($prev)* $field }
                    { $($rest)* }
                }
            },
        }
    });
    (@build_parser
        { $($prev:tt)* }
        { $field:ident : $type:ty = marker, $($rest:tt)* }
    ) => ({
        let $field = $crate::std::default::Default::default();

        __ast_struct! {
            @build_parser
            { $($prev)* $field }
            { $($rest)* }
        }
    });
    (@build_parser
        { $($prev:tt)* }
        { $field:ident : $type:ty, $($rest:tt)* }
    ) => (
        __ast_struct! {
            @build_parser
            { $($prev)* }
            { $field : $type = weedle!($type), $($rest)* }
        }
    );

    (
        @launch_pad
        $(#[$attr:meta])*
        $name:ident
        [ ]
        [ ]
        { $($fields:tt)* }
    ) => {
        __ast_struct! {
            @build_struct_decl
            {
                $(#[$attr])*
                $name
                [ ]
            }
            { $($fields)* }
        }

        impl<'a> $crate::Parse<'a> for $name {
            fn parse(input: $crate::CompleteStr<'a>) -> $crate::IResult<$crate::CompleteStr<'a>, Self> {
                __ast_struct! {
                    @build_parser
                    { input, }
                    { $($fields)* }
                }
            }
        }
    };
    (
        @launch_pad
        $(#[$attr:meta])*
        $name:ident
        [ 'a ]
        [ ]
        { $($fields:tt)* }
    ) => {
        __ast_struct! {
            @build_struct_decl
            {
                $(#[$attr])*
                $name
                [ 'a ]
            }
            { $($fields)* }
        }

        impl<'a> $crate::Parse<'a> for $name<'a> {
            fn parse(input: $crate::CompleteStr<'a>) -> $crate::IResult<$crate::CompleteStr<'a>, Self> {
                __ast_struct! {
                    @build_parser
                    { input, }
                    { $($fields)* }
                }
            }
        }
    };
    (
        @launch_pad
        $(#[$attr:meta])*
        $name:ident
        [$($generics:ident)+]
        [$($bounds:tt)+]
        { $($fields:tt)* }
    ) => {
        __ast_struct! {
            @build_struct_decl
            {
                $(#[$attr])*
                $name
                [$($generics),+]
            }
            { $($fields)* }
        }

        impl<'a, $($generics),+> $crate::Parse<'a> for $name<$($generics),+> where $($bounds)+ {
            fn parse(input: $crate::CompleteStr<'a>) -> $crate::IResult<$crate::CompleteStr<'a>, Self> {
                __ast_struct! {
                    @build_parser
                    { input, }
                    { $($fields)* }
                }
            }
        }
    };
}

macro_rules! __ast_enum {
    (@build_enum_decl
        {
            $(#[$attr:meta])*
            $name:ident
            [ $($maybe_a:tt)* ]
            $($variant:ident($member:ty))*
        }
        { }
    ) => (
        $(#[$attr])*
        #[derive(Clone, Debug, Eq, PartialEq, Ord, PartialOrd, Hash)]
        pub enum $name<$($maybe_a)*> {
            $($variant($member),)*
        }
    );
    (@build_enum_decl
        { $($prev:tt)* }
        { $variant:ident($member:ty), $($rest:tt)* }
    ) => (
        __ast_enum! {
            @build_enum_decl
            { $($prev)* $variant($member) }
            { $($rest)* }
        }
    );
    (@build_enum_decl
        { $($prev:tt)* }
        { $(#[$attr:meta])* $variant:ident( $($member:tt)* ), $($rest:tt)* }
    ) => (
        __ast_enum! {
            @build_enum_decl
            { $($prev)* $variant(ast_types! { @extract_type $($member)* }) }
            { $($rest)* }
        }
    );

    (@build_sub_types { }) => ();
    (@build_sub_types
        { $variant:ident($member:ty), $($rest:tt)* }
    ) => (
        __ast_enum! {
            @build_sub_types
            { $($rest)* }
        }
    );
    (@build_sub_types
        { $(#[$attr:meta])* $variant:ident( $($member:tt)* ), $($rest:tt)* }
    ) => (
        ast_types! {
            $(#[$attr])*
            $($member)*
        }
        __ast_enum! {
            @build_sub_types
            { $($rest)* }
        }
    );


    (@build_conversions $name:ident [ $($maybe_a:tt)* ] { }) => ();
    (@build_conversions
        $name:ident
        [ $($maybe_a:tt)* ]
        { $variant:ident($member:ty), $($rest:tt)* }
    ) => (
        impl<$($maybe_a)*> From<$member> for $name<$($maybe_a)*> {
            fn from(x: $member) -> Self {
                $name::$variant(x)
            }
        }
        __ast_enum! {
            @build_conversions
            $name
            [ $($maybe_a)* ]
            { $($rest)* }
        }
    );
    (@build_conversions
        $name:ident
        [ $($maybe_a:tt)* ]
        { $(#[$attr:meta])* $variant:ident( $($member:tt)* ), $($rest:tt)* }
    ) => (
        __ast_enum! {
            @build_conversions
            $name
            [ $($maybe_a)* ]
            { $variant(ast_types! { @extract_type $($member)* }), $($rest)* }
        }
    );

    (@build_parse
        { $name:ident [ $($maybe_a:tt)* ] $($member:ty)* }
        { }
    ) => (
        impl<'a> $crate::Parse<'a> for $name<$($maybe_a)*> {
            parser!(alt!(
                $(weedle!($member) => {From::from})|*
            ));
        }
    );
    (@build_parse
        { $($prev:tt)* }
        { $variant:ident($member:ty), $($rest:tt)* }
    ) => (
        __ast_enum! {
            @build_parse
            { $($prev)* $member }
            { $($rest)* }
        }
    );
    (@build_parse
        { $($prev:tt)* }
        { $(#[$attr:meta])* $variant:ident( $($member:tt)* ), $($rest:tt)* }
    ) => (
        __ast_enum! {
            @build_parse
            { $($prev)* ast_types! { @extract_type $($member)* } }
            { $($rest)* }
        }
    );

    (@launch_pad
        $(#[$attr:meta])*
        $name:ident
        [ $($maybe_a:tt)* ]
        { $($variants:tt)* }
    ) => (
        __ast_enum! {
            @build_enum_decl
            { $(#[$attr])* $name [ $($maybe_a)* ] }
            { $($variants)* }
        }

        __ast_enum! {
            @build_sub_types
            { $($variants)* }
        }

        __ast_enum! {
            @build_conversions
            $name
            [ $($maybe_a)* ]
            { $($variants)* }
        }

        __ast_enum! {
            @build_parse
            { $name [ $($maybe_a)* ] }
            { $($variants)* }
        }
    );
}

#[cfg(test)]
macro_rules! test {
    (@arg $parsed:ident) => {};
    (@arg $parsed:ident $($lhs:tt).+ == $rhs:expr; $($rest:tt)*) => {
        assert_eq!($parsed.$($lhs).+, $rhs);
        test!(@arg $parsed $($rest)*);
    };
    (@arg $parsed:ident $($lhs:tt).+(); $($rest:tt)*) => {
        assert!($parsed.$($lhs).+());
        test!(@arg $parsed $($rest)*);
    };
    (@arg $parsed:ident $($lhs:tt).+() == $rhs:expr; $($rest:tt)*) => {
        assert_eq!($parsed.$($lhs).+(), $rhs);
        test!(@arg $parsed $($rest)*);
    };
    (err $name:ident { $raw:expr => $typ:ty }) => {
        #[test]
        fn $name() {
            <$typ>::parse($crate::nom::types::CompleteStr($raw)).unwrap_err();
        }
    };
    ($name:ident { $raw:expr => $rem:expr; $typ:ty => $val:expr }) => {
        #[test]
        fn $name() {
            let (rem, parsed) = <$typ>::parse($crate::nom::types::CompleteStr($raw)).unwrap();
            assert_eq!(rem, $crate::nom::types::CompleteStr($rem));
            assert_eq!(parsed, $val);
        }
    };
    ($name:ident { $raw:expr => $rem:expr; $typ:ty; $($body:tt)* }) => {
        #[test]
        fn $name() {
            let (_rem, _parsed) = <$typ>::parse($crate::nom::types::CompleteStr($raw)).unwrap();
            assert_eq!(_rem, $crate::nom::types::CompleteStr($rem));
            test!(@arg _parsed $($body)*);
        }
    };
}

#[cfg(test)]
macro_rules! test_variants {
    ($struct_:ident { $( $variant:ident == $value:expr ),* $(,)* }) => {
        #[allow(non_snake_case)]
        mod $struct_ {
            $(
                mod $variant {
                    use $crate::types::*;
                    use $crate::nom::types::CompleteStr;
                    #[test]
                    fn should_parse() {
                        let (rem, parsed) = $struct_::parse(CompleteStr($value)).unwrap();
                        assert_eq!(rem, CompleteStr(""));
                        match parsed {
                            $struct_::$variant(_) => {},
                            _ => { panic!("Failed to parse"); }
                        }
                    }
                }
            )*
        }
    };
}
