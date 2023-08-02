#![cfg_attr(not(feature = "std"), no_std)]
#![allow(dead_code, unused_imports)]

#[cfg(not(feature = "std"))]
extern crate alloc;

#[cfg(not(feature = "std"))]
use alloc::{
    boxed::Box,
    format,
    string::{String, ToString},
    vec::Vec,
};

use derive_more::{Binary, Display, Octal, UpperHex};

mod structs {
    use super::*;

    mod unit {
        use super::*;

        #[derive(Display)]
        struct Unit;

        #[derive(Display)]
        struct Tuple();

        #[derive(Display)]
        struct Struct {}

        #[test]
        fn assert() {
            assert_eq!(Unit.to_string(), "Unit");
            assert_eq!(Tuple().to_string(), "Tuple");
            assert_eq!(Struct {}.to_string(), "Struct");
        }

        mod str {
            use super::*;

            #[derive(Display)]
            #[display("unit")]
            pub struct Unit;

            #[derive(Display)]
            #[display("tuple")]
            pub struct Tuple();

            #[derive(Display)]
            #[display("struct")]
            pub struct Struct {}

            #[test]
            fn assert() {
                assert_eq!(Unit.to_string(), "unit");
                assert_eq!(Tuple().to_string(), "tuple");
                assert_eq!(Struct {}.to_string(), "struct");
            }
        }

        mod interpolated {
            use super::*;

            #[derive(Display)]
            #[display("unit: {}", 0)]
            pub struct Unit;

            #[derive(Display)]
            #[display("tuple: {}", 0)]
            pub struct Tuple();

            #[derive(Display)]
            #[display("struct: {}", 0)]
            pub struct Struct {}

            #[test]
            fn assert() {
                assert_eq!(Unit.to_string(), "unit: 0");
                assert_eq!(Tuple().to_string(), "tuple: 0");
                assert_eq!(Struct {}.to_string(), "struct: 0");
            }
        }
    }

    mod single_field {
        use super::*;

        #[derive(Display)]
        struct Tuple(i32);

        #[derive(Binary)]
        struct Binary(i32);

        #[derive(Display)]
        struct Struct {
            field: i32,
        }

        #[derive(Octal)]
        struct Octal {
            field: i32,
        }

        #[test]
        fn assert() {
            assert_eq!(Tuple(0).to_string(), "0");
            assert_eq!(format!("{:b}", Binary(10)), "1010");
            assert_eq!(Struct { field: 0 }.to_string(), "0");
            assert_eq!(format!("{:o}", Octal { field: 10 }).to_string(), "12");
        }

        mod str {
            use super::*;

            #[derive(Display)]
            #[display("tuple")]
            struct Tuple(i32);

            #[derive(Display)]
            #[display("struct")]
            struct Struct {
                field: i32,
            }

            #[test]
            fn assert() {
                assert_eq!(Tuple(0).to_string(), "tuple");
                assert_eq!(Struct { field: 0 }.to_string(), "struct");
            }
        }

        mod interpolated {
            use super::*;

            #[derive(Display)]
            #[display("tuple: {_0} {}", _0)]
            struct Tuple(i32);

            #[derive(Display)]
            #[display("struct: {field} {}", field)]
            struct Struct {
                field: i32,
            }

            #[test]
            fn assert() {
                assert_eq!(Tuple(0).to_string(), "tuple: 0 0");
                assert_eq!(Struct { field: 0 }.to_string(), "struct: 0 0");
            }
        }
    }

    mod multi_field {
        use super::*;

        mod str {
            use super::*;

            #[derive(Display)]
            #[display("tuple")]
            struct Tuple(i32, i32);

            #[derive(Display)]
            #[display("struct")]
            struct Struct {
                field1: i32,
                field2: i32,
            }

            #[test]
            fn assert() {
                assert_eq!(Tuple(1, 2).to_string(), "tuple");
                assert_eq!(
                    Struct {
                        field1: 1,
                        field2: 2,
                    }
                    .to_string(),
                    "struct",
                );
            }
        }

        mod interpolated {
            use super::*;

            #[derive(Display)]
            #[display(
            "{_0} {ident} {_1} {} {}",
            _1, _0 + _1, ident = 123, _1 = _0,
            )]
            struct Tuple(i32, i32);

            #[derive(Display)]
            #[display(
            "{field1} {ident} {field2} {} {}",
            field2, field1 + field2, ident = 123, field2 = field1,
            )]
            struct Struct {
                field1: i32,
                field2: i32,
            }

            #[test]
            fn assert() {
                assert_eq!(Tuple(1, 2).to_string(), "1 123 1 2 3");
                assert_eq!(
                    Struct {
                        field1: 1,
                        field2: 2,
                    }
                    .to_string(),
                    "1 123 1 2 3",
                );
            }
        }
    }
}

mod enums {
    use super::*;

    mod no_variants {
        use super::*;

        #[derive(Display)]
        enum Void {}

        const fn assert<T: Display>() {}
        const _: () = assert::<Void>();
    }

    mod unit_variant {
        use super::*;

        #[derive(Display)]
        enum Enum {
            Unit,
            Unnamed(),
            Named {},
            #[display("STR_UNIT")]
            StrUnit,
            #[display("STR_UNNAMED")]
            StrUnnamed(),
            #[display("STR_NAMED")]
            StrNamed {},
        }

        #[test]
        fn assert() {
            assert_eq!(Enum::Unit.to_string(), "Unit");
            assert_eq!(Enum::Unnamed().to_string(), "Unnamed");
            assert_eq!(Enum::Named {}.to_string(), "Named");
            assert_eq!(Enum::StrUnit.to_string(), "STR_UNIT");
            assert_eq!(Enum::StrUnnamed().to_string(), "STR_UNNAMED");
            assert_eq!(Enum::StrNamed {}.to_string(), "STR_NAMED");
        }
    }

    mod single_field_variant {
        use super::*;

        #[derive(Display)]
        enum Enum {
            Unnamed(i32),
            Named {
                field: i32,
            },
            #[display("unnamed")]
            StrUnnamed(i32),
            #[display("named")]
            StrNamed {
                field: i32,
            },
            #[display("{_0} {}", _0)]
            InterpolatedUnnamed(i32),
            #[display("{field} {}", field)]
            InterpolatedNamed {
                field: i32,
            },
        }

        #[test]
        fn assert() {
            assert_eq!(Enum::Unnamed(1).to_string(), "1");
            assert_eq!(Enum::Named { field: 1 }.to_string(), "1");
            assert_eq!(Enum::StrUnnamed(1).to_string(), "unnamed");
            assert_eq!(Enum::StrNamed { field: 1 }.to_string(), "named");
            assert_eq!(Enum::InterpolatedUnnamed(1).to_string(), "1 1");
            assert_eq!(Enum::InterpolatedNamed { field: 1 }.to_string(), "1 1");
        }
    }

    mod multi_field_variant {
        use super::*;

        #[derive(Display)]
        enum Enum {
            #[display("unnamed")]
            StrUnnamed(i32, i32),
            #[display("named")]
            StrNamed { field1: i32, field2: i32 },
            #[display(
            "{_0} {ident} {_1} {} {}",
            _1, _0 + _1, ident = 123, _1 = _0,
            )]
            InterpolatedUnnamed(i32, i32),
            #[display(
            "{field1} {ident} {field2} {} {}",
            field2, field1 + field2, ident = 123, field2 = field1,
            )]
            InterpolatedNamed { field1: i32, field2: i32 },
        }

        #[test]
        fn assert() {
            assert_eq!(Enum::StrUnnamed(1, 2).to_string(), "unnamed");
            assert_eq!(
                Enum::StrNamed {
                    field1: 1,
                    field2: 2,
                }
                .to_string(),
                "named",
            );
            assert_eq!(Enum::InterpolatedUnnamed(1, 2).to_string(), "1 123 1 2 3");
            assert_eq!(
                Enum::InterpolatedNamed {
                    field1: 1,
                    field2: 2,
                }
                .to_string(),
                "1 123 1 2 3",
            );
        }
    }
}

mod generic {
    use super::*;

    trait Bound {}

    impl Bound for () {}

    fn display_bound<T: Bound>(_: &T) -> &'static str {
        "()"
    }

    #[derive(Display)]
    #[display("Generic {}", field)]
    struct NamedGenericStruct<T> {
        field: T,
    }
    #[test]
    fn named_generic_struct() {
        assert_eq!(NamedGenericStruct { field: 1 }.to_string(), "Generic 1");
    }

    #[derive(Display)]
    #[display("Generic {field}")]
    struct InterpolatedNamedGenericStruct<T> {
        field: T,
    }
    #[test]
    fn interpolated_named_generic_struct() {
        assert_eq!(
            InterpolatedNamedGenericStruct { field: 1 }.to_string(),
            "Generic 1",
        );
    }

    #[derive(Display)]
    #[display("Generic {field:<>width$.prec$} {field}")]
    struct InterpolatedNamedGenericStructWidthPrecision<T> {
        field: T,
        width: usize,
        prec: usize,
    }
    #[test]
    fn interpolated_named_generic_struct_width_precision() {
        assert_eq!(
            InterpolatedNamedGenericStructWidthPrecision {
                field: 1.2345,
                width: 9,
                prec: 2,
            }
            .to_string(),
            "Generic <<<<<1.23 1.2345",
        );
    }

    #[derive(Display)]
    struct AutoNamedGenericStruct<T> {
        field: T,
    }
    #[test]
    fn auto_named_generic_struct() {
        assert_eq!(AutoNamedGenericStruct { field: 1 }.to_string(), "1");
    }

    #[derive(Display)]
    #[display("{alias}", alias = field)]
    struct AliasedNamedGenericStruct<T> {
        field: T,
    }
    #[test]
    fn aliased_named_generic_struct() {
        assert_eq!(AliasedNamedGenericStruct { field: 1 }.to_string(), "1");
    }

    #[derive(Display)]
    #[display("{field1}", field1 = field2)]
    struct AliasedFieldNamedGenericStruct<T> {
        field1: T,
        field2: i32,
    }
    #[test]
    fn aliased_field_named_generic_struct() {
        assert_eq!(
            AliasedFieldNamedGenericStruct {
                field1: (),
                field2: 1,
            }
            .to_string(),
            "1",
        );
    }

    #[derive(Display)]
    #[display("Generic {}", _0)]
    struct UnnamedGenericStruct<T>(T);
    #[test]
    fn unnamed_generic_struct() {
        assert_eq!(UnnamedGenericStruct(2).to_string(), "Generic 2");
    }

    #[derive(Display)]
    #[display("Generic {_0}")]
    struct InterpolatedUnnamedGenericStruct<T>(T);
    #[test]
    fn interpolated_unnamed_generic_struct() {
        assert_eq!(InterpolatedUnnamedGenericStruct(2).to_string(), "Generic 2");
    }

    #[derive(Display)]
    struct AutoUnnamedGenericStruct<T>(T);
    #[test]
    fn auto_unnamed_generic_struct() {
        assert_eq!(AutoUnnamedGenericStruct(2).to_string(), "2");
    }

    #[derive(Display)]
    #[display("{alias}", alias = _0)]
    struct AliasedUnnamedGenericStruct<T>(T);
    #[test]
    fn aliased_unnamed_generic_struct() {
        assert_eq!(AliasedUnnamedGenericStruct(2).to_string(), "2");
    }

    #[derive(Display)]
    #[display("{_0}", _0 = _1)]
    struct AliasedFieldUnnamedGenericStruct<T>(T, i32);
    #[test]
    fn aliased_field_unnamed_generic_struct() {
        assert_eq!(AliasedFieldUnnamedGenericStruct((), 2).to_string(), "2");
    }

    #[derive(Display)]
    enum GenericEnum<A, B> {
        #[display("Gen::A {}", field)]
        A { field: A },
        #[display("Gen::B {}", _0)]
        B(B),
    }
    #[test]
    fn generic_enum() {
        assert_eq!(GenericEnum::A::<_, u8> { field: 1 }.to_string(), "Gen::A 1");
        assert_eq!(GenericEnum::B::<u8, _>(2).to_string(), "Gen::B 2");
    }

    #[derive(Display)]
    enum InterpolatedGenericEnum<A, B> {
        #[display("Gen::A {field}")]
        A { field: A },
        #[display("Gen::B {_0}")]
        B(B),
    }
    #[test]
    fn interpolated_generic_enum() {
        assert_eq!(
            InterpolatedGenericEnum::A::<_, u8> { field: 1 }.to_string(),
            "Gen::A 1",
        );
        assert_eq!(
            InterpolatedGenericEnum::B::<u8, _>(2).to_string(),
            "Gen::B 2",
        );
    }

    #[derive(Display)]
    enum AutoGenericEnum<A, B> {
        A { field: A },
        B(B),
    }
    #[test]
    fn auto_generic_enum() {
        assert_eq!(AutoGenericEnum::A::<_, u8> { field: 1 }.to_string(), "1");
        assert_eq!(AutoGenericEnum::B::<u8, _>(2).to_string(), "2");
    }

    #[derive(Display)]
    #[display("{} {} <-> {0:o} {1:#x} <-> {0:?} {1:X?}", a, b)]
    struct MultiTraitNamedGenericStruct<A, B> {
        a: A,
        b: B,
    }
    #[test]
    fn multi_trait_named_generic_struct() {
        let s = MultiTraitNamedGenericStruct { a: 8u8, b: 255 };
        assert_eq!(s.to_string(), "8 255 <-> 10 0xff <-> 8 FF");
    }

    #[derive(Display)]
    #[display("{} {b} <-> {0:o} {1:#x} <-> {0:?} {1:X?}", a, b)]
    struct InterpolatedMultiTraitNamedGenericStruct<A, B> {
        a: A,
        b: B,
    }
    #[test]
    fn interpolated_multi_trait_named_generic_struct() {
        let s = InterpolatedMultiTraitNamedGenericStruct { a: 8u8, b: 255 };
        assert_eq!(s.to_string(), "8 255 <-> 10 0xff <-> 8 FF");
    }

    #[derive(Display)]
    #[display("{} {} {{}} {0:o} {1:#x} - {0:>4?} {1:^4X?}", _0, _1)]
    struct MultiTraitUnnamedGenericStruct<A, B>(A, B);
    #[test]
    fn multi_trait_unnamed_generic_struct() {
        let s = MultiTraitUnnamedGenericStruct(8u8, 255);
        assert_eq!(s.to_string(), "8 255 {} 10 0xff -    8  FF ");
    }

    #[derive(Display)]
    #[display("{} {_1} {{}} {0:o} {1:#x} - {0:>4?} {1:^4X?}", _0, _1)]
    struct InterpolatedMultiTraitUnnamedGenericStruct<A, B>(A, B);
    #[test]
    fn interpolated_multi_trait_unnamed_generic_struct() {
        let s = InterpolatedMultiTraitUnnamedGenericStruct(8u8, 255);
        assert_eq!(s.to_string(), "8 255 {} 10 0xff -    8  FF ");
    }

    #[derive(Display)]
    #[display("{}", 3 * 4)]
    struct UnusedGenericStruct<T>(T);
    #[test]
    fn unused_generic_struct() {
        let s = UnusedGenericStruct(());
        assert_eq!(s.to_string(), "12");
    }

    mod associated_type_field_enumerator {
        use super::*;

        trait Trait {
            type Type;
        }

        struct Struct;

        impl Trait for Struct {
            type Type = i32;
        }

        #[test]
        fn auto_generic_named_struct_associated() {
            #[derive(Display)]
            struct AutoGenericNamedStructAssociated<T: Trait> {
                field: <T as Trait>::Type,
            }

            let s = AutoGenericNamedStructAssociated::<Struct> { field: 10 };
            assert_eq!(s.to_string(), "10");
        }

        #[test]
        fn auto_generic_unnamed_struct_associated() {
            #[derive(Display)]
            struct AutoGenericUnnamedStructAssociated<T: Trait>(<T as Trait>::Type);

            let s = AutoGenericUnnamedStructAssociated::<Struct>(10);
            assert_eq!(s.to_string(), "10");
        }

        #[test]
        fn auto_generic_enum_associated() {
            #[derive(Display)]
            enum AutoGenericEnumAssociated<T: Trait> {
                Enumerator(<T as Trait>::Type),
            }

            let e = AutoGenericEnumAssociated::<Struct>::Enumerator(10);
            assert_eq!(e.to_string(), "10");
        }
    }

    mod complex_type_field_enumerator {
        use super::*;

        #[derive(Display)]
        struct Struct<T>(T);

        #[test]
        fn auto_generic_named_struct_complex() {
            #[derive(Display)]
            struct AutoGenericNamedStructComplex<T> {
                field: Struct<T>,
            }

            let s = AutoGenericNamedStructComplex { field: Struct(10) };
            assert_eq!(s.to_string(), "10");
        }

        #[test]
        fn auto_generic_unnamed_struct_complex() {
            #[derive(Display)]
            struct AutoGenericUnnamedStructComplex<T>(Struct<T>);

            let s = AutoGenericUnnamedStructComplex(Struct(10));
            assert_eq!(s.to_string(), "10");
        }

        #[test]
        fn auto_generic_enum_complex() {
            #[derive(Display)]
            enum AutoGenericEnumComplex<T> {
                Enumerator(Struct<T>),
            }

            let e = AutoGenericEnumComplex::Enumerator(Struct(10));
            assert_eq!(e.to_string(), "10")
        }
    }

    mod reference {
        use super::*;

        #[test]
        fn auto_generic_reference() {
            #[derive(Display)]
            struct AutoGenericReference<'a, T>(&'a T);

            let s = AutoGenericReference(&10);
            assert_eq!(s.to_string(), "10");
        }

        #[test]
        fn auto_generic_static_reference() {
            #[derive(Display)]
            struct AutoGenericStaticReference<T: 'static>(&'static T);

            let s = AutoGenericStaticReference(&10);
            assert_eq!(s.to_string(), "10");
        }
    }

    mod indirect {
        use super::*;

        #[derive(Display)]
        struct Struct<T>(T);

        #[test]
        fn auto_generic_indirect() {
            #[derive(Display)]
            struct AutoGenericIndirect<T: 'static>(Struct<&'static T>);

            const V: i32 = 10;
            let s = AutoGenericIndirect(Struct(&V));
            assert_eq!(s.to_string(), "10");
        }
    }

    mod bound {
        use super::*;

        #[test]
        fn simple() {
            #[derive(Display)]
            #[display("{} {}", _0, _1)]
            struct Struct<T1, T2>(T1, T2);

            let s = Struct(10, 20);
            assert_eq!(s.to_string(), "10 20");
        }

        #[test]
        fn underscored_simple() {
            #[derive(Display)]
            #[display("{_0} {_1}")]
            struct Struct<T1, T2>(T1, T2);

            let s = Struct(10, 20);
            assert_eq!(s.to_string(), "10 20");
        }

        #[test]
        fn redundant() {
            #[derive(Display)]
            #[display(bound(T1: ::core::fmt::Display, T2: ::core::fmt::Display))]
            #[display("{} {}", _0, _1)]
            struct Struct<T1, T2>(T1, T2);

            let s = Struct(10, 20);
            assert_eq!(s.to_string(), "10 20");
        }

        #[test]
        fn underscored_redundant() {
            #[derive(Display)]
            #[display(bound(T1: ::core::fmt::Display, T2: ::core::fmt::Display))]
            #[display("{_0} {_1}")]
            struct Struct<T1, T2>(T1, T2);

            let s = Struct(10, 20);
            assert_eq!(s.to_string(), "10 20");
        }

        #[test]
        fn complex() {
            trait Trait1 {
                fn function1(&self) -> &'static str;
            }

            trait Trait2 {
                fn function2(&self) -> &'static str;
            }

            impl Trait1 for i32 {
                fn function1(&self) -> &'static str {
                    "WHAT"
                }
            }

            impl Trait2 for i32 {
                fn function2(&self) -> &'static str {
                    "EVER"
                }
            }

            #[derive(Display)]
            #[display(bound(T1: Trait1 + Trait2, T2: Trait1 + Trait2))]
            #[display("{} {} {} {}", _0.function1(), _0, _1.function2(), _1)]
            struct Struct<T1, T2>(T1, T2);

            let s = Struct(10, 20);
            assert_eq!(s.to_string(), "WHAT 10 EVER 20");
        }

        #[test]
        fn underscored_complex() {
            trait Trait1 {
                fn function1(&self) -> &'static str;
            }

            trait Trait2 {
                fn function2(&self) -> &'static str;
            }

            impl Trait1 for i32 {
                fn function1(&self) -> &'static str {
                    "WHAT"
                }
            }

            impl Trait2 for i32 {
                fn function2(&self) -> &'static str {
                    "EVER"
                }
            }

            #[derive(Display)]
            #[display(bound(T1: Trait1 + Trait2, T2: Trait1 + Trait2))]
            #[display("{} {_0} {} {_1}", _0.function1(), _1.function2())]
            struct Struct<T1, T2>(T1, T2);

            let s = Struct(10, 20);
            assert_eq!(s.to_string(), "WHAT 10 EVER 20");
        }
    }
}
