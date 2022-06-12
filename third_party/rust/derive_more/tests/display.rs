#![allow(dead_code, unused_imports)]
#[macro_use]
extern crate derive_more;

use std::path::PathBuf;

// Here just to make sure that this doesn't conflict with
// the derives in some way
use std::fmt::Binary;

#[derive(Display, Octal, Binary)]
struct MyInt(i32);

#[derive(UpperHex)]
enum IntEnum {
    U8(u8),
    I8(i8),
}

#[derive(Display)]
#[display(fmt = "({}, {})", x, y)]
struct Point2D {
    x: i32,
    y: i32,
}

#[derive(Display)]
#[display(fmt = "{}", "self.sign()")]
struct PositiveOrNegative {
    x: i32,
}

impl PositiveOrNegative {
    fn sign(&self) -> &str {
        if self.x >= 0 {
            "Positive"
        } else {
            "Negative"
        }
    }
}

#[derive(Display)]
#[display(fmt = "{}", message)]
struct Error {
    message: &'static str,
    backtrace: (),
}

impl Error {
    fn new(message: &'static str) -> Self {
        Self {
            message,
            backtrace: (),
        }
    }
}

#[derive(Display)]
enum E {
    Uint(u32),
    #[display(fmt = "I am B {:b}", i)]
    Binary {
        i: i8,
    },
    #[display(fmt = "I am C {}", "_0.display()")]
    Path(PathBuf),
}

#[derive(Display)]
#[display(fmt = "Java EE")]
enum EE {
    A,
    B,
}

#[derive(Display)]
#[display(fmt = "Hello there!")]
union U {
    i: u32,
}

#[derive(Octal)]
#[octal(fmt = "7")]
struct S;

#[derive(UpperHex)]
#[upper_hex(fmt = "UpperHex")]
struct UH;

#[derive(DebugCustom)]
#[debug(fmt = "MyDebug")]
struct D;

#[derive(Display)]
struct Unit;

#[derive(Display)]
struct UnitStruct {}

#[derive(Display)]
enum EmptyEnum {}

#[derive(Display)]
#[display(fmt = "Generic")]
struct Generic<T>(T);

#[derive(Display)]
#[display(fmt = "Here's a prefix for {} and a suffix")]
enum Affix {
    A(u32),
    #[display(fmt = "{} -- {}", wat, stuff)]
    B {
        wat: String,
        stuff: bool,
    },
}

#[derive(Debug, Display)]
#[display(fmt = "{:?}", self)]
struct DebugStructAsDisplay;

#[test]
fn check_display() {
    assert_eq!(MyInt(-2).to_string(), "-2");
    assert_eq!(format!("{:b}", MyInt(9)), "1001");
    assert_eq!(format!("{:#b}", MyInt(9)), "0b1001");
    assert_eq!(format!("{:o}", MyInt(9)), "11");
    assert_eq!(format!("{:X}", IntEnum::I8(-1)), "FF");
    assert_eq!(format!("{:#X}", IntEnum::U8(255)), "0xFF");
    assert_eq!(Point2D { x: 3, y: 4 }.to_string(), "(3, 4)");
    assert_eq!(PositiveOrNegative { x: 123 }.to_string(), "Positive");
    assert_eq!(PositiveOrNegative { x: 0 }.to_string(), "Positive");
    assert_eq!(PositiveOrNegative { x: -465 }.to_string(), "Negative");
    assert_eq!(Error::new("Error").to_string(), "Error");
    assert_eq!(E::Uint(2).to_string(), "2");
    assert_eq!(E::Binary { i: -2 }.to_string(), "I am B 11111110");
    assert_eq!(E::Path("abc".into()).to_string(), "I am C abc");
    assert_eq!(EE::A.to_string(), "Java EE");
    assert_eq!(EE::B.to_string(), "Java EE");
    assert_eq!(U { i: 2 }.to_string(), "Hello there!");
    assert_eq!(format!("{:o}", S), "7");
    assert_eq!(format!("{:X}", UH), "UpperHex");
    assert_eq!(format!("{:?}", D), "MyDebug");
    assert_eq!(Unit.to_string(), "Unit");
    assert_eq!(UnitStruct {}.to_string(), "UnitStruct");
    assert_eq!(Generic(()).to_string(), "Generic");
    assert_eq!(
        Affix::A(2).to_string(),
        "Here's a prefix for 2 and a suffix"
    );
    assert_eq!(
        Affix::B {
            wat: "things".to_owned(),
            stuff: false,
        }
        .to_string(),
        "Here's a prefix for things -- false and a suffix"
    );
    assert_eq!(DebugStructAsDisplay.to_string(), "DebugStructAsDisplay");
}

mod generic {
    #[derive(Display)]
    #[display(fmt = "Generic {}", field)]
    struct NamedGenericStruct<T> {
        field: T,
    }
    #[test]
    fn named_generic_struct() {
        assert_eq!(NamedGenericStruct { field: 1 }.to_string(), "Generic 1");
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
    #[display(fmt = "Generic {}", "_0")]
    struct UnnamedGenericStruct<T>(T);
    #[test]
    fn unnamed_generic_struct() {
        assert_eq!(UnnamedGenericStruct(2).to_string(), "Generic 2");
    }

    #[derive(Display)]
    struct AutoUnnamedGenericStruct<T>(T);
    #[test]
    fn auto_unnamed_generic_struct() {
        assert_eq!(AutoUnnamedGenericStruct(2).to_string(), "2");
    }

    #[derive(Display)]
    enum GenericEnum<A, B> {
        #[display(fmt = "Gen::A {}", field)]
        A { field: A },
        #[display(fmt = "Gen::B {}", "_0")]
        B(B),
    }
    #[test]
    fn generic_enum() {
        assert_eq!(GenericEnum::A::<_, u8> { field: 1 }.to_string(), "Gen::A 1");
        assert_eq!(GenericEnum::B::<u8, _>(2).to_string(), "Gen::B 2");
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
    #[display(fmt = "{} {} <-> {0:o} {1:#x} <-> {0:?} {1:X?}", a, b)]
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
    #[display(fmt = "{} {} {{}} {0:o} {1:#x} - {0:>4?} {1:^4X?}", "_0", "_1")]
    struct MultiTraitUnnamedGenericStruct<A, B>(A, B);
    #[test]
    fn multi_trait_unnamed_generic_struct() {
        let s = MultiTraitUnnamedGenericStruct(8u8, 255);
        assert_eq!(s.to_string(), "8 255 {} 10 0xff -    8  FF ");
    }

    #[derive(Display)]
    #[display(fmt = "{}", "3 * 4")]
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
            #[display(fmt = "{} {}", _0, _1)]
            struct Struct<T1, T2>(T1, T2);

            let s = Struct(10, 20);
            assert_eq!(s.to_string(), "10 20");
        }

        #[test]
        fn redundant() {
            #[derive(Display)]
            #[display(bound = "T1: ::core::fmt::Display, T2: ::core::fmt::Display")]
            #[display(fmt = "{} {}", _0, _1)]
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
            #[display(bound = "T1: Trait1 + Trait2, T2: Trait1 + Trait2")]
            #[display(fmt = "{} {} {} {}", "_0.function1()", _0, "_1.function2()", _1)]
            struct Struct<T1, T2>(T1, T2);

            let s = Struct(10, 20);
            assert_eq!(s.to_string(), "WHAT 10 EVER 20");
        }
    }
}
