#![no_std]
#![warn(unsafe_code)]
#![warn(rust_2018_idioms, single_use_lifetimes)]
#![allow(dead_code)]

use core::{marker::PhantomPinned, pin::Pin};
use pin_project::{pin_project, pinned_drop, UnsafeUnpin};

#[test]
fn test_pin_project() {
    #[pin_project]
    struct Struct<T, U> {
        #[pin]
        field1: T,
        field2: U,
    }

    let mut foo = Struct { field1: 1, field2: 2 };

    let mut foo_orig = Pin::new(&mut foo);
    let foo = foo_orig.as_mut().project();

    let x: Pin<&mut i32> = foo.field1;
    assert_eq!(*x, 1);

    let y: &mut i32 = foo.field2;
    assert_eq!(*y, 2);

    assert_eq!(foo_orig.as_ref().field1, 1);
    assert_eq!(foo_orig.as_ref().field2, 2);

    let mut foo = Struct { field1: 1, field2: 2 };

    let foo = Pin::new(&mut foo).project();

    let __StructProjection { field1, field2 } = foo;
    let _: Pin<&mut i32> = field1;
    let _: &mut i32 = field2;

    #[pin_project]
    struct TupleStruct<T, U>(#[pin] T, U);

    let mut bar = TupleStruct(1, 2);

    let bar = Pin::new(&mut bar).project();

    let x: Pin<&mut i32> = bar.0;
    assert_eq!(*x, 1);

    let y: &mut i32 = bar.1;
    assert_eq!(*y, 2);

    #[pin_project]
    #[derive(Eq, PartialEq, Debug)]
    enum Enum<A, B, C, D> {
        Variant1(#[pin] A, B),
        Variant2 {
            #[pin]
            field1: C,
            field2: D,
        },
        None,
    }

    let mut baz = Enum::Variant1(1, 2);

    let mut baz_orig = Pin::new(&mut baz);
    let baz = baz_orig.as_mut().project();

    match baz {
        __EnumProjection::Variant1(x, y) => {
            let x: Pin<&mut i32> = x;
            assert_eq!(*x, 1);

            let y: &mut i32 = y;
            assert_eq!(*y, 2);
        }
        __EnumProjection::Variant2 { field1, field2 } => {
            let _x: Pin<&mut i32> = field1;
            let _y: &mut i32 = field2;
        }
        __EnumProjection::None => {}
    }

    assert_eq!(Pin::into_ref(baz_orig).get_ref(), &Enum::Variant1(1, 2));

    let mut baz = Enum::Variant2 { field1: 3, field2: 4 };

    let mut baz = Pin::new(&mut baz).project();

    match &mut baz {
        __EnumProjection::Variant1(x, y) => {
            let _x: &mut Pin<&mut i32> = x;
            let _y: &mut &mut i32 = y;
        }
        __EnumProjection::Variant2 { field1, field2 } => {
            let x: &mut Pin<&mut i32> = field1;
            assert_eq!(**x, 3);

            let y: &mut &mut i32 = field2;
            assert_eq!(**y, 4);
        }
        __EnumProjection::None => {}
    }

    if let __EnumProjection::Variant2 { field1, field2 } = baz {
        let x: Pin<&mut i32> = field1;
        assert_eq!(*x, 3);

        let y: &mut i32 = field2;
        assert_eq!(*y, 4);
    }
}

#[test]
fn enum_project_set() {
    #[pin_project]
    #[derive(Eq, PartialEq, Debug)]
    enum Bar {
        Variant1(#[pin] u8),
        Variant2(bool),
    }

    let mut bar = Bar::Variant1(25);
    let mut bar_orig = Pin::new(&mut bar);
    let bar_proj = bar_orig.as_mut().project();

    match bar_proj {
        __BarProjection::Variant1(val) => {
            let new_bar = Bar::Variant2(val.as_ref().get_ref() == &25);
            bar_orig.set(new_bar);
        }
        _ => unreachable!(),
    }

    assert_eq!(bar, Bar::Variant2(true));
}

#[test]
fn where_clause_and_associated_type_fields() {
    #[pin_project]
    struct Struct1<I>
    where
        I: Iterator,
    {
        #[pin]
        field1: I,
        field2: I::Item,
    }

    #[pin_project]
    struct Struct2<I, J>
    where
        I: Iterator<Item = J>,
    {
        #[pin]
        field1: I,
        field2: J,
    }

    #[pin_project]
    pub struct Struct3<T>
    where
        T: 'static,
    {
        field: T,
    }

    trait Static: 'static {}

    impl<T> Static for Struct3<T> {}

    #[pin_project]
    enum Enum<I>
    where
        I: Iterator,
    {
        Variant1(#[pin] I),
        Variant2(I::Item),
    }
}

#[allow(explicit_outlives_requirements)] // https://github.com/rust-lang/rust/issues/60993
#[test]
fn unsized_in_where_clause() {
    #[pin_project]
    struct Struct<I>
    where
        I: ?Sized,
    {
        #[pin]
        field: I,
    }
}

#[test]
fn derive_copy() {
    #[pin_project]
    #[derive(Clone, Copy)]
    struct Struct<T> {
        val: T,
    }

    fn is_copy<T: Copy>() {}

    is_copy::<Struct<u8>>();
}

#[test]
fn move_out() {
    struct NotCopy;

    #[pin_project]
    struct Struct {
        val: NotCopy,
    }

    let foo = Struct { val: NotCopy };
    let _val: NotCopy = foo.val;

    #[pin_project]
    enum Enum {
        Variant(NotCopy),
    }

    let bar = Enum::Variant(NotCopy);
    let _val: NotCopy = match bar {
        Enum::Variant(val) => val,
    };
}

#[test]
fn trait_bounds_on_type_generics() {
    #[pin_project]
    pub struct Struct1<'a, T: ?Sized> {
        field: &'a mut T,
    }

    #[pin_project]
    pub struct Struct2<'a, T: ::core::fmt::Debug> {
        field: &'a mut T,
    }

    #[pin_project]
    pub struct Struct3<'a, T: core::fmt::Debug> {
        field: &'a mut T,
    }

    #[pin_project]
    pub struct Struct4<'a, T: core::fmt::Debug + core::fmt::Display> {
        field: &'a mut T,
    }

    #[pin_project]
    pub struct Struct5<'a, T: core::fmt::Debug + ?Sized> {
        field: &'a mut T,
    }

    #[pin_project]
    pub struct Struct6<'a, T: core::fmt::Debug = [u8; 16]> {
        field: &'a mut T,
    }

    let _: Struct6<'_> = Struct6 { field: &mut [0u8; 16] };

    #[pin_project]
    pub struct Struct7<T: 'static> {
        field: T,
    }

    trait Static: 'static {}

    impl<T> Static for Struct7<T> {}

    #[pin_project]
    pub struct Struct8<'a, 'b: 'a> {
        field1: &'a u8,
        field2: &'b u8,
    }

    #[pin_project]
    pub struct TupleStruct<'a, T: ?Sized>(&'a mut T);

    #[pin_project]
    enum Enum<'a, T: ?Sized> {
        Variant(&'a mut T),
    }
}

#[test]
fn overlapping_lifetime_names() {
    #[pin_project]
    pub struct Foo<'pin, T> {
        #[pin]
        field: &'pin mut T,
    }
}

#[test]
fn combine() {
    #[pin_project(PinnedDrop, UnsafeUnpin)]
    pub struct Foo<T> {
        field1: u8,
        #[pin]
        field2: T,
    }

    #[pinned_drop]
    impl<T> PinnedDrop for Foo<T> {
        fn drop(self: Pin<&mut Self>) {}
    }

    #[allow(unsafe_code)]
    unsafe impl<T: Unpin> UnsafeUnpin for Foo<T> {}
}

#[test]
fn private_type_in_public_type() {
    #[pin_project]
    pub struct PublicStruct<T> {
        #[pin]
        inner: PrivateStruct<T>,
    }

    struct PrivateStruct<T>(T);
}

#[test]
fn lifetime_project() {
    #[pin_project]
    struct Struct1<T, U> {
        #[pin]
        pinned: T,
        unpinned: U,
    }

    #[pin_project]
    struct Struct2<'a, T, U> {
        #[pin]
        pinned: &'a mut T,
        unpinned: U,
    }

    #[pin_project]
    enum Enum<T, U> {
        Variant {
            #[pin]
            pinned: T,
            unpinned: U,
        },
    }

    impl<T, U> Struct1<T, U> {
        fn get_pin_ref<'a>(self: Pin<&'a Self>) -> Pin<&'a T> {
            self.project_ref().pinned
        }
        fn get_pin_mut<'a>(self: Pin<&'a mut Self>) -> Pin<&'a mut T> {
            self.project().pinned
        }
    }

    impl<'b, T, U> Struct2<'b, T, U> {
        fn get_pin_ref<'a>(self: Pin<&'a Self>) -> Pin<&'a &'b mut T> {
            self.project_ref().pinned
        }
        fn get_pin_mut<'a>(self: Pin<&'a mut Self>) -> Pin<&'a mut &'b mut T> {
            self.project().pinned
        }
    }

    impl<T, U> Enum<T, U> {
        fn get_pin_ref<'a>(self: Pin<&'a Self>) -> Pin<&'a T> {
            match self.project_ref() {
                __EnumProjectionRef::Variant { pinned, .. } => pinned,
            }
        }
        fn get_pin_mut<'a>(self: Pin<&'a mut Self>) -> Pin<&'a mut T> {
            match self.project() {
                __EnumProjection::Variant { pinned, .. } => pinned,
            }
        }
    }
}

#[rustversion::since(1.36)]
#[test]
fn lifetime_project_elided() {
    #[pin_project]
    struct Struct1<T, U> {
        #[pin]
        pinned: T,
        unpinned: U,
    }

    #[pin_project]
    struct Struct2<'a, T, U> {
        #[pin]
        pinned: &'a mut T,
        unpinned: U,
    }

    #[pin_project]
    enum Enum<T, U> {
        Variant {
            #[pin]
            pinned: T,
            unpinned: U,
        },
    }

    impl<T, U> Struct1<T, U> {
        fn get_pin_ref(self: Pin<&Self>) -> Pin<&T> {
            self.project_ref().pinned
        }
        fn get_pin_mut(self: Pin<&mut Self>) -> Pin<&mut T> {
            self.project().pinned
        }
    }

    impl<'b, T, U> Struct2<'b, T, U> {
        fn get_pin_ref(self: Pin<&Self>) -> Pin<&&'b mut T> {
            self.project_ref().pinned
        }
        fn get_pin_mut(self: Pin<&mut Self>) -> Pin<&mut &'b mut T> {
            self.project().pinned
        }
    }

    impl<T, U> Enum<T, U> {
        fn get_pin_ref(self: Pin<&Self>) -> Pin<&T> {
            match self.project_ref() {
                __EnumProjectionRef::Variant { pinned, .. } => pinned,
            }
        }
        fn get_pin_mut(self: Pin<&mut Self>) -> Pin<&mut T> {
            match self.project() {
                __EnumProjection::Variant { pinned, .. } => pinned,
            }
        }
    }
}

mod visibility {
    use pin_project::pin_project;

    #[pin_project]
    pub(crate) struct A {
        pub b: u8,
    }
}

#[test]
fn visibility() {
    let mut x = visibility::A { b: 0 };
    let x = Pin::new(&mut x);
    let y = x.as_ref().project_ref();
    let _: &u8 = y.b;
    let y = x.project();
    let _: &mut u8 = y.b;
}

#[test]
fn trivial_bounds() {
    #[pin_project]
    pub struct NoGenerics {
        #[pin]
        field: PhantomPinned,
    }
}

#[test]
fn dst() {
    #[pin_project]
    pub struct A<T: ?Sized> {
        x: T,
    }

    let _: &mut A<dyn core::fmt::Debug> = &mut A { x: 0u8 } as _;

    #[pin_project]
    pub struct B<T: ?Sized> {
        #[pin]
        x: T,
    }

    #[pin_project]
    pub struct C<T: ?Sized>(T);

    #[pin_project]
    pub struct D<T: ?Sized>(#[pin] T);
}

#[test]
fn dyn_type() {
    #[pin_project]
    struct Struct1 {
        a: i32,
        f: dyn core::fmt::Debug,
    }

    #[pin_project]
    struct Struct2 {
        a: i32,
        #[pin]
        f: dyn core::fmt::Debug,
    }

    #[pin_project]
    struct Struct3 {
        a: i32,
        f: dyn core::fmt::Debug + Send,
    }

    #[pin_project]
    struct Struct4 {
        a: i32,
        #[pin]
        f: dyn core::fmt::Debug + Send,
    }
}

#[test]
fn self_in_where_clause() {
    pub trait Trait {}

    #[pin_project]
    pub struct Struct1<T>
    where
        Self: Trait,
    {
        x: T,
    }

    impl<T> Trait for Struct1<T> {}

    pub trait Trait2 {
        type Foo;
    }

    #[pin_project]
    pub struct Struct2<T>
    where
        Self: Trait2<Foo = Struct1<T>>,
        <Self as Trait2>::Foo: Trait,
    {
        x: T,
    }

    impl<T> Trait2 for Struct2<T> {
        type Foo = Struct1<T>;
    }
}
