#![cfg_attr(not(feature = "std"), no_std)]
#![allow(dead_code)]

#[cfg(not(feature = "std"))]
extern crate alloc;

#[cfg(not(feature = "std"))]
use alloc::{
    borrow::Cow,
    borrow::ToOwned,
    boxed::Box,
    string::{String, ToString},
};
use core::mem;
#[cfg(feature = "std")]
use std::borrow::Cow;

use derive_more::Into;
use static_assertions::assert_not_impl_any;

/// Nasty [`mem::transmute()`] that works in generic contexts
/// by [`mem::forget`]ing stuff.
///
/// It's OK for tests!
unsafe fn transmute<From, To>(from: From) -> To {
    let to = unsafe { mem::transmute_copy(&from) };
    mem::forget(from);
    to
}

#[derive(Debug, PartialEq)]
#[repr(transparent)]
struct Wrapped<T>(T);

#[derive(Debug, PartialEq)]
#[repr(transparent)]
struct Transmuted<T>(T);

impl<T> From<Wrapped<T>> for Transmuted<T> {
    fn from(from: Wrapped<T>) -> Self {
        // SAFETY: repr(transparent)
        unsafe { transmute(from) }
    }
}

impl<T> From<&Wrapped<T>> for &Transmuted<T> {
    fn from(from: &Wrapped<T>) -> Self {
        // SAFETY: repr(transparent)
        unsafe { transmute(from) }
    }
}

impl<T> From<&mut Wrapped<T>> for &mut Transmuted<T> {
    fn from(from: &mut Wrapped<T>) -> Self {
        // SAFETY: repr(transparent)
        unsafe { transmute(from) }
    }
}

mod unit {
    use super::*;

    #[derive(Debug, Into, PartialEq)]
    struct Unit;

    #[derive(Debug, Into, PartialEq)]
    struct Tuple();

    #[derive(Debug, Into, PartialEq)]
    struct Struct {}

    #[test]
    fn assert() {
        assert_eq!((), Unit.into());
        assert_eq!((), Tuple().into());
        assert_eq!((), Struct {}.into());
    }

    mod generic {
        use super::*;

        #[derive(Debug, Into, PartialEq)]
        struct Unit<const N: usize>;

        #[derive(Debug, Into, PartialEq)]
        struct Tuple<const N: usize>();

        #[derive(Debug, Into, PartialEq)]
        struct Struct<const N: usize> {}

        #[test]
        fn assert() {
            assert_eq!((), Unit::<1>.into());
            assert_eq!((), Tuple::<1>().into());
            assert_eq!((), Struct::<1> {}.into());
        }
    }
}

mod single_field {
    use super::*;

    #[derive(Debug, Into, PartialEq)]
    struct Tuple(i32);

    #[derive(Debug, Into, PartialEq)]
    struct Struct {
        field: i32,
    }

    #[test]
    fn assert() {
        assert_eq!(42, Tuple(42).into());
        assert_eq!(42, Struct { field: 42 }.into());
    }

    mod skip {
        use super::*;

        #[derive(Debug, Into, PartialEq)]
        struct Tuple(#[into(skip)] i32);

        #[derive(Debug, Into, PartialEq)]
        struct Struct {
            #[into(skip)]
            field: i32,
        }

        #[test]
        fn assert() {
            assert_eq!((), Tuple(42).into());
            assert_eq!((), Struct { field: 42 }.into());
        }
    }

    mod types {
        use super::*;

        #[derive(Debug, Into, PartialEq)]
        #[into(i64)]
        #[into(i128)]
        struct Tuple(i32);

        #[derive(Debug, Into, PartialEq)]
        #[into(Box<str>, Cow<'_, str>)]
        struct Struct {
            field: String,
        }

        #[test]
        fn assert() {
            assert_not_impl_any!(Tuple: Into<i32>);
            assert_not_impl_any!(Struct: Into<String>);

            assert_eq!(42_i64, Tuple(42).into());
            assert_eq!(42_i128, Tuple(42).into());
            assert_eq!(
                Box::<str>::from("42".to_owned()),
                Struct {
                    field: "42".to_string(),
                }
                .into(),
            );
            assert_eq!(
                Cow::Borrowed("42"),
                Cow::<str>::from(Struct {
                    field: "42".to_string(),
                }),
            );
        }

        mod ref_ {
            use super::*;

            #[derive(Debug, Into, PartialEq)]
            #[into(ref)]
            struct Unnamed(i32);

            #[derive(Debug, Into, PartialEq)]
            #[into(ref)]
            struct Named {
                field: i32,
            }

            #[test]
            fn assert() {
                assert_eq!(&42, <&i32>::from(&Unnamed(42)));
                assert_eq!(&42, <&i32>::from(&Named { field: 42 }));
            }

            mod types {
                use super::*;

                #[derive(Debug, Into, PartialEq)]
                #[into(ref(i32, Unnamed))]
                struct Tuple(Unnamed);

                #[derive(Debug, Into, PartialEq)]
                #[into(ref(i32, Named))]
                struct Struct {
                    field: Named,
                }

                #[test]
                fn assert() {
                    assert_eq!(&42, <&i32>::from(&Tuple(Unnamed(42))));
                    assert_eq!(&Unnamed(42), <&Unnamed>::from(&Tuple(Unnamed(42))));
                    assert_eq!(
                        &42,
                        <&i32>::from(&Struct {
                            field: Named { field: 42 },
                        }),
                    );
                    assert_eq!(
                        &Named { field: 42 },
                        <&Named>::from(&Struct {
                            field: Named { field: 42 },
                        }),
                    );
                }
            }
        }

        mod ref_mut {
            use super::*;

            #[derive(Debug, Into, PartialEq)]
            #[into(ref_mut)]
            struct Unnamed(i32);

            #[derive(Debug, Into, PartialEq)]
            #[into(ref_mut)]
            struct Named {
                field: i32,
            }

            #[test]
            fn assert() {
                assert_eq!(&mut 42, <&mut i32>::from(&mut Unnamed(42)));
                assert_eq!(&mut 42, <&mut i32>::from(&mut Named { field: 42 }));
            }

            mod types {
                use super::*;

                #[derive(Debug, Into, PartialEq)]
                #[into(ref_mut(i32, Unnamed))]
                struct Tuple(Unnamed);

                #[derive(Debug, Into, PartialEq)]
                #[into(ref_mut(i32, Named))]
                struct Struct {
                    field: Named,
                }

                #[test]
                fn assert() {
                    assert_eq!(&mut 42, <&mut i32>::from(&mut Tuple(Unnamed(42))));
                    assert_eq!(
                        &mut Unnamed(42),
                        <&mut Unnamed>::from(&mut Tuple(Unnamed(42))),
                    );
                    assert_eq!(
                        &mut 42,
                        <&mut i32>::from(&mut Struct {
                            field: Named { field: 42 },
                        }),
                    );
                    assert_eq!(
                        &mut Named { field: 42 },
                        <&mut Named>::from(&mut Struct {
                            field: Named { field: 42 },
                        }),
                    );
                }
            }
        }
    }

    mod generic {
        use super::*;

        #[derive(Debug, Into, PartialEq)]
        struct Tuple<T>(Wrapped<T>);

        #[derive(Debug, Into, PartialEq)]
        struct Struct<T> {
            field: Wrapped<T>,
        }

        #[test]
        fn assert() {
            assert_eq!(Wrapped(42), Tuple(Wrapped(42)).into());
            assert_eq!(Wrapped(42), Struct { field: Wrapped(42) }.into());
        }

        mod skip {
            use super::*;

            #[derive(Debug, Into, PartialEq)]
            struct Tuple<T>(#[into(skip)] Wrapped<T>);

            #[derive(Debug, Into, PartialEq)]
            struct Struct<T> {
                #[into(skip)]
                field: Wrapped<T>,
            }

            #[test]
            fn assert() {
                assert_eq!((), Tuple(Wrapped(42)).into());
                assert_eq!((), Struct { field: Wrapped(42) }.into());
            }
        }

        mod types {
            use super::*;

            #[derive(Debug, Into, PartialEq)]
            #[into(Transmuted<T>)]
            struct Tuple<T>(Wrapped<T>);

            #[derive(Debug, Into, PartialEq)]
            #[into(Transmuted<T>)]
            struct Struct<T> {
                field: Wrapped<T>,
            }

            #[test]
            fn assert() {
                assert_eq!(Transmuted(42), Tuple(Wrapped(42)).into());
                assert_eq!(Transmuted(42), Struct { field: Wrapped(42) }.into());
            }
        }

        mod ref_ {
            use super::*;

            #[derive(Debug, Into, PartialEq)]
            #[into(ref)]
            struct Tuple<T>(Wrapped<T>);

            #[derive(Debug, Into, PartialEq)]
            #[into(ref)]
            struct Struct<T> {
                field: Wrapped<T>,
            }

            #[test]
            fn assert() {
                assert_eq!(&Wrapped(42), <&Wrapped<_>>::from(&Tuple(Wrapped(42))));
                assert_eq!(
                    &Wrapped(42),
                    <&Wrapped<_>>::from(&Struct { field: Wrapped(42) })
                );
            }

            mod types {
                use super::*;

                #[derive(Debug, Into, PartialEq)]
                #[into(ref(Transmuted<T>))]
                struct Tuple<T>(Wrapped<T>);

                #[derive(Debug, Into, PartialEq)]
                #[into(ref(Transmuted<T>))]
                struct Struct<T> {
                    field: Wrapped<T>,
                }

                #[test]
                fn assert() {
                    assert_eq!(
                        &Transmuted(42),
                        <&Transmuted<_>>::from(&Tuple(Wrapped(42))),
                    );
                    assert_eq!(
                        &Transmuted(42),
                        <&Transmuted<_>>::from(&Struct { field: Wrapped(42) }),
                    );
                }
            }
        }

        mod ref_mut {
            use super::*;

            #[derive(Debug, Into, PartialEq)]
            #[into(ref_mut)]
            struct Tuple<T>(Wrapped<T>);

            #[derive(Debug, Into, PartialEq)]
            #[into(ref_mut)]
            struct Struct<T> {
                field: Wrapped<T>,
            }

            #[test]
            fn assert() {
                assert_eq!(
                    &mut Wrapped(42),
                    <&mut Wrapped<_>>::from(&mut Tuple(Wrapped(42)))
                );
                assert_eq!(
                    &mut Wrapped(42),
                    <&mut Wrapped<_>>::from(&mut Struct { field: Wrapped(42) }),
                );
            }

            mod types {
                use super::*;

                #[derive(Debug, Into, PartialEq)]
                #[into(ref_mut(Transmuted<T>))]
                struct Tuple<T>(Wrapped<T>);

                #[derive(Debug, Into, PartialEq)]
                #[into(ref_mut(Transmuted<T>))]
                struct Struct<T> {
                    field: Wrapped<T>,
                }

                #[test]
                fn assert() {
                    assert_eq!(
                        &mut Transmuted(42),
                        <&mut Transmuted<_>>::from(&mut Tuple(Wrapped(42))),
                    );
                    assert_eq!(
                        &mut Transmuted(42),
                        <&mut Transmuted<_>>::from(&mut Struct { field: Wrapped(42) }),
                    );
                }
            }
        }

        mod indirect {
            use super::*;

            #[derive(Debug, Into, PartialEq)]
            struct Tuple<T: 'static>(&'static Wrapped<T>);

            #[derive(Debug, Into, PartialEq)]
            struct Struct<T: 'static> {
                field: &'static Wrapped<T>,
            }

            #[test]
            fn assert() {
                assert_eq!(&Wrapped(42), <&Wrapped<_>>::from(Tuple(&Wrapped(42))));
                assert_eq!(
                    &Wrapped(42),
                    <&Wrapped<_>>::from(Struct {
                        field: &Wrapped(42),
                    }),
                );
            }
        }

        mod bounded {
            use super::*;

            #[derive(Debug, Into, PartialEq)]
            struct Tuple<T: Clone>(Wrapped<T>);

            #[derive(Debug, Into, PartialEq)]
            struct Struct<T: Clone> {
                field: Wrapped<T>,
            }

            #[test]
            fn assert() {
                assert_eq!(Wrapped(42), Tuple(Wrapped(42)).into());
                assert_eq!(Wrapped(42), Struct { field: Wrapped(42) }.into());
            }
        }

        mod r#const {
            use super::*;

            #[derive(Debug, Into, PartialEq)]
            struct Tuple<const N: usize, T>(Wrapped<T>);

            #[derive(Debug, Into, PartialEq)]
            struct Struct<T, const N: usize> {
                field: Wrapped<T>,
            }

            #[test]
            fn assert() {
                assert_eq!(Wrapped(1), Tuple::<1, _>(Wrapped(1)).into());
                assert_eq!(Wrapped(1), Struct::<_, 1> { field: Wrapped(1) }.into());
            }
        }
    }
}

mod multi_field {
    use super::*;

    #[derive(Debug, Into, PartialEq)]
    struct Tuple(i32, i64);

    #[derive(Debug, Into, PartialEq)]
    struct Struct {
        field1: i32,
        field2: i64,
    }

    #[test]
    fn assert() {
        assert_eq!((1, 2_i64), Tuple(1, 2_i64).into());
        assert_eq!(
            (1, 2_i64),
            Struct {
                field1: 1,
                field2: 2_i64,
            }
            .into(),
        );
    }

    mod skip {
        use super::*;

        #[derive(Debug, Into, PartialEq)]
        struct Tuple(i32, #[into(skip)] i64);

        #[derive(Debug, Into, PartialEq)]
        struct Struct {
            #[into(skip)]
            field1: i32,
            field2: i64,
        }

        #[test]
        fn assert() {
            assert_eq!(1, Tuple(1, 2_i64).into());
            assert_eq!(
                2_i64,
                Struct {
                    field1: 1,
                    field2: 2_i64,
                }
                .into(),
            );
        }
    }

    mod types {
        use super::*;

        #[derive(Debug, Into, PartialEq)]
        #[into((i32, i64))]
        #[into((i64, i128))]
        struct Tuple(i16, i32);

        #[derive(Debug, Into, PartialEq)]
        #[into((Box<str>, i32), (Cow<'_, str>, i64))]
        struct Struct {
            field1: String,
            field2: i32,
        }

        #[test]
        fn assert() {
            assert_not_impl_any!(Tuple: Into<(i16, i32)>);
            assert_not_impl_any!(Struct: Into<(String, i32)>);

            assert_eq!((1, 2_i64), Tuple(1_i16, 2).into());
            assert_eq!((1_i64, 2_i128), Tuple(1_i16, 2).into());
            assert_eq!(
                (Box::<str>::from("42".to_owned()), 1),
                Struct {
                    field1: "42".to_string(),
                    field2: 1,
                }
                .into(),
            );
            assert_eq!(
                (Cow::Borrowed("42"), 1_i64),
                Struct {
                    field1: "42".to_string(),
                    field2: 1,
                }
                .into(),
            );
        }

        mod ref_ {
            use super::*;

            #[derive(Debug, Into, PartialEq)]
            #[into(ref)]
            struct Unnamed(i32, i64);

            #[derive(Debug, Into, PartialEq)]
            #[into(ref)]
            struct Named {
                field1: i32,
                field2: i64,
            }

            #[test]
            fn assert() {
                assert_eq!((&1, &2_i64), (&Unnamed(1, 2_i64)).into());
                assert_eq!(
                    (&1, &2_i64),
                    (&Named {
                        field1: 1,
                        field2: 2_i64,
                    })
                        .into(),
                );
            }

            mod types {
                use super::*;

                #[derive(Debug, Into, PartialEq)]
                #[into(ref(
                    (Transmuted<i32>, Transmuted<i64>),
                    (Transmuted<i32>, Wrapped<i64>)),
                )]
                struct Tuple(Wrapped<i32>, Wrapped<i64>);

                #[derive(Debug, Into, PartialEq)]
                #[into(ref(
                    (Transmuted<i32>, Transmuted<i64>),
                    (Transmuted<i32>, Wrapped<i64>)),
                )]
                struct Struct {
                    field1: Wrapped<i32>,
                    field2: Wrapped<i64>,
                }

                #[test]
                fn assert() {
                    assert_eq!(
                        (&Transmuted(1), &Transmuted(2_i64)),
                        (&Tuple(Wrapped(1), Wrapped(2_i64))).into(),
                    );
                    assert_eq!(
                        (&Transmuted(1), &Wrapped(2_i64)),
                        (&Tuple(Wrapped(1), Wrapped(2_i64))).into(),
                    );
                    assert_eq!(
                        (&Transmuted(1), &Transmuted(2_i64)),
                        (&Struct {
                            field1: Wrapped(1),
                            field2: Wrapped(2_i64),
                        })
                            .into(),
                    );
                    assert_eq!(
                        (&Transmuted(1), &Wrapped(2_i64)),
                        (&Struct {
                            field1: Wrapped(1),
                            field2: Wrapped(2_i64),
                        })
                            .into(),
                    );
                }
            }
        }

        mod ref_mut {
            use super::*;

            #[derive(Debug, Into, PartialEq)]
            #[into(ref_mut)]
            struct Unnamed(i32, i64);

            #[derive(Debug, Into, PartialEq)]
            #[into(ref_mut)]
            struct Named {
                field1: i32,
                field2: i64,
            }

            #[test]
            fn assert() {
                assert_eq!((&mut 1, &mut 2_i64), (&mut Unnamed(1, 2_i64)).into());
                assert_eq!(
                    (&mut 1, &mut 2_i64),
                    (&mut Named {
                        field1: 1,
                        field2: 2_i64,
                    })
                        .into(),
                );
            }

            mod types {
                use super::*;

                #[derive(Debug, Into, PartialEq)]
                #[into(ref_mut(
                    (Transmuted<i32>, Transmuted<i64>),
                    (Transmuted<i32>, Wrapped<i64>)),
                )]
                struct Tuple(Wrapped<i32>, Wrapped<i64>);

                #[derive(Debug, Into, PartialEq)]
                #[into(ref_mut(
                    (Transmuted<i32>, Transmuted<i64>),
                    (Transmuted<i32>, Wrapped<i64>)),
                )]
                struct Struct {
                    field1: Wrapped<i32>,
                    field2: Wrapped<i64>,
                }

                #[test]
                fn assert() {
                    assert_eq!(
                        (&mut Transmuted(1), &mut Transmuted(2_i64)),
                        (&mut Tuple(Wrapped(1), Wrapped(2_i64))).into(),
                    );
                    assert_eq!(
                        (&mut Transmuted(1), &mut Wrapped(2_i64)),
                        (&mut Tuple(Wrapped(1), Wrapped(2_i64))).into(),
                    );
                    assert_eq!(
                        (&mut Transmuted(1), &mut Transmuted(2_i64)),
                        (&mut Struct {
                            field1: Wrapped(1),
                            field2: Wrapped(2_i64),
                        })
                            .into(),
                    );
                    assert_eq!(
                        (&mut Transmuted(1), &mut Wrapped(2_i64)),
                        (&mut Struct {
                            field1: Wrapped(1),
                            field2: Wrapped(2_i64),
                        })
                            .into(),
                    );
                }
            }
        }
    }

    mod generic {
        use super::*;

        #[derive(Debug, Into, PartialEq)]
        struct Tuple<A, B>(Wrapped<A>, Wrapped<B>);

        #[derive(Debug, Into, PartialEq)]
        struct Struct<A, B> {
            field1: Wrapped<A>,
            field2: Wrapped<B>,
        }

        #[test]
        fn assert() {
            assert_eq!(
                (Wrapped(1), Wrapped(2)),
                Tuple(Wrapped(1), Wrapped(2)).into(),
            );
            assert_eq!(
                (Wrapped(1), Wrapped(2)),
                Struct {
                    field1: Wrapped(1),
                    field2: Wrapped(2),
                }
                .into(),
            );
        }

        mod skip {
            use super::*;

            #[derive(Debug, Into, PartialEq)]
            struct Tuple<A, B>(Wrapped<A>, #[into(skip)] Wrapped<B>);

            #[derive(Debug, Into, PartialEq)]
            struct Struct<A, B> {
                #[into(skip)]
                field1: Wrapped<A>,
                field2: Wrapped<B>,
            }

            #[test]
            fn assert() {
                assert_eq!(Wrapped(1), Tuple(Wrapped(1), Wrapped(2)).into());
                assert_eq!(
                    Wrapped(2),
                    Struct {
                        field1: Wrapped(1),
                        field2: Wrapped(2),
                    }
                    .into(),
                );
            }
        }

        mod types {
            use super::*;

            #[derive(Debug, Into, PartialEq)]
            #[into((Transmuted<A>, Transmuted<B>))]
            struct Tuple<A, B>(Wrapped<A>, Wrapped<B>);

            #[derive(Debug, Into, PartialEq)]
            #[into((Transmuted<A>, Transmuted<B>))]
            struct Struct<A, B> {
                field1: Wrapped<A>,
                field2: Wrapped<B>,
            }

            #[test]
            fn assert() {
                assert_eq!(
                    (Transmuted(1), Transmuted(2)),
                    Tuple(Wrapped(1), Wrapped(2)).into(),
                );
                assert_eq!(
                    (Transmuted(1), Transmuted(2)),
                    Struct {
                        field1: Wrapped(1),
                        field2: Wrapped(2),
                    }
                    .into(),
                );
            }
        }

        mod ref_ {
            use super::*;

            #[derive(Debug, Into, PartialEq)]
            #[into(ref)]
            struct Tuple<A, B>(Wrapped<A>, Wrapped<B>);

            #[derive(Debug, Into, PartialEq)]
            #[into(ref)]
            struct Struct<A, B> {
                field1: Wrapped<A>,
                field2: Wrapped<B>,
            }

            #[test]
            fn assert() {
                assert_eq!(
                    (&Wrapped(1), &Wrapped(2)),
                    (&Tuple(Wrapped(1), Wrapped(2))).into(),
                );
                assert_eq!(
                    (&Wrapped(1), &Wrapped(2)),
                    (&Struct {
                        field1: Wrapped(1),
                        field2: Wrapped(2),
                    })
                        .into(),
                );
            }

            mod types {
                use super::*;

                #[derive(Debug, Into, PartialEq)]
                #[into(ref((Transmuted<A>, Transmuted<B>)))]
                struct Tuple<A, B>(Wrapped<A>, Wrapped<B>);

                #[derive(Debug, Into, PartialEq)]
                #[into(ref((Transmuted<A>, Transmuted<B>)))]
                struct Struct<A, B> {
                    field1: Wrapped<A>,
                    field2: Wrapped<B>,
                }

                #[test]
                fn assert() {
                    assert_eq!(
                        (&Transmuted(1), &Transmuted(2)),
                        (&Tuple(Wrapped(1), Wrapped(2))).into(),
                    );
                    assert_eq!(
                        (&Transmuted(1), &Transmuted(2)),
                        (&Struct {
                            field1: Wrapped(1),
                            field2: Wrapped(2),
                        })
                            .into(),
                    );
                }
            }
        }

        mod ref_mut {
            use super::*;

            #[derive(Debug, Into, PartialEq)]
            #[into(ref_mut)]
            struct Tuple<A, B>(Wrapped<A>, Wrapped<B>);

            #[derive(Debug, Into, PartialEq)]
            #[into(ref_mut)]
            struct Struct<A, B> {
                field1: Wrapped<A>,
                field2: Wrapped<B>,
            }

            #[test]
            fn assert() {
                assert_eq!(
                    (&mut Wrapped(1), &mut Wrapped(2)),
                    (&mut Tuple(Wrapped(1), Wrapped(2))).into(),
                );
                assert_eq!(
                    (&mut Wrapped(1), &mut Wrapped(2)),
                    (&mut Struct {
                        field1: Wrapped(1),
                        field2: Wrapped(2),
                    })
                        .into(),
                );
            }

            mod types {
                use super::*;

                #[derive(Debug, Into, PartialEq)]
                #[into(ref_mut((Transmuted<A>, Transmuted<B>)))]
                struct Tuple<A, B>(Wrapped<A>, Wrapped<B>);

                #[derive(Debug, Into, PartialEq)]
                #[into(ref_mut((Transmuted<A>, Transmuted<B>)))]
                struct Struct<A, B> {
                    field1: Wrapped<A>,
                    field2: Wrapped<B>,
                }

                #[test]
                fn assert() {
                    assert_eq!(
                        (&mut Transmuted(1), &mut Transmuted(2)),
                        (&mut Tuple(Wrapped(1), Wrapped(2))).into(),
                    );
                    assert_eq!(
                        (&mut Transmuted(1), &mut Transmuted(2)),
                        (&mut Struct {
                            field1: Wrapped(1),
                            field2: Wrapped(2),
                        })
                            .into(),
                    );
                }
            }
        }

        mod indirect {
            use super::*;

            #[derive(Debug, Into, PartialEq)]
            struct Tuple<A: 'static, B: 'static>(
                &'static Wrapped<A>,
                &'static Wrapped<B>,
            );

            #[derive(Debug, Into, PartialEq)]
            struct Struct<A: 'static, B: 'static> {
                field1: &'static Wrapped<A>,
                field2: &'static Wrapped<B>,
            }

            #[test]
            fn assert() {
                assert_eq!(
                    (&Wrapped(1), &Wrapped(2)),
                    Tuple(&Wrapped(1), &Wrapped(2)).into(),
                );
                assert_eq!(
                    (&Wrapped(1), &Wrapped(2)),
                    (Struct {
                        field1: &Wrapped(1),
                        field2: &Wrapped(2),
                    })
                    .into(),
                );
            }
        }

        mod bounded {
            use super::*;

            #[derive(Debug, Into, PartialEq)]
            struct Tuple<A: Clone, B: Clone>(Wrapped<A>, Wrapped<B>);

            #[derive(Debug, Into, PartialEq)]
            struct Struct<A: Clone, B: Clone> {
                field1: Wrapped<A>,
                field2: Wrapped<B>,
            }

            #[test]
            fn assert() {
                assert_eq!(
                    (Wrapped(1), Wrapped(2)),
                    Tuple(Wrapped(1), Wrapped(2)).into(),
                );
                assert_eq!(
                    (Wrapped(1), Wrapped(2)),
                    Struct {
                        field1: Wrapped(1),
                        field2: Wrapped(2),
                    }
                    .into(),
                );
            }
        }

        mod r#const {
            use super::*;

            #[derive(Debug, Into, PartialEq)]
            struct Tuple<const N: usize, A, B>(Wrapped<A>, Wrapped<B>);

            #[derive(Debug, Into, PartialEq)]
            struct Struct<A, const N: usize, B> {
                field1: Wrapped<A>,
                field2: Wrapped<B>,
            }

            #[test]
            fn assert() {
                assert_eq!(
                    (Wrapped(1), Wrapped(2)),
                    Tuple::<1, _, _>(Wrapped(1), Wrapped(2)).into(),
                );
                assert_eq!(
                    (Wrapped(1), Wrapped(2)),
                    Struct::<_, 1, _> {
                        field1: Wrapped(1),
                        field2: Wrapped(2),
                    }
                    .into(),
                );
            }
        }
    }
}
