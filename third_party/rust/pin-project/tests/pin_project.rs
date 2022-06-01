#![warn(rust_2018_idioms, single_use_lifetimes)]
#![allow(dead_code)]

use std::{
    marker::{PhantomData, PhantomPinned},
    pin::Pin,
};

use pin_project::{pin_project, pinned_drop, UnsafeUnpin};

#[test]
fn projection() {
    #[pin_project(
        project = StructProj,
        project_ref = StructProjRef,
        project_replace = StructProjOwn,
    )]
    struct Struct<T, U> {
        #[pin]
        field1: T,
        field2: U,
    }

    let mut s = Struct { field1: 1, field2: 2 };
    let mut s_orig = Pin::new(&mut s);
    let s = s_orig.as_mut().project();

    let x: Pin<&mut i32> = s.field1;
    assert_eq!(*x, 1);

    let y: &mut i32 = s.field2;
    assert_eq!(*y, 2);

    assert_eq!(s_orig.as_ref().field1, 1);
    assert_eq!(s_orig.as_ref().field2, 2);

    let mut s = Struct { field1: 1, field2: 2 };

    let StructProj { field1, field2 } = Pin::new(&mut s).project();
    let _: Pin<&mut i32> = field1;
    let _: &mut i32 = field2;

    let StructProjRef { field1, field2 } = Pin::new(&s).project_ref();
    let _: Pin<&i32> = field1;
    let _: &i32 = field2;

    let mut s = Pin::new(&mut s);
    let StructProjOwn { field1, field2 } =
        s.as_mut().project_replace(Struct { field1: 3, field2: 4 });
    let _: PhantomData<i32> = field1;
    let _: i32 = field2;
    assert_eq!(field2, 2);
    assert_eq!(s.field1, 3);
    assert_eq!(s.field2, 4);

    #[pin_project(project_replace)]
    struct TupleStruct<T, U>(#[pin] T, U);

    let mut s = TupleStruct(1, 2);
    let s = Pin::new(&mut s).project();

    let x: Pin<&mut i32> = s.0;
    assert_eq!(*x, 1);

    let y: &mut i32 = s.1;
    assert_eq!(*y, 2);

    #[pin_project(project_replace, project = EnumProj)]
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

    let mut e = Enum::Variant1(1, 2);
    let mut e_orig = Pin::new(&mut e);
    let e = e_orig.as_mut().project();

    match e {
        EnumProj::Variant1(x, y) => {
            let x: Pin<&mut i32> = x;
            assert_eq!(*x, 1);

            let y: &mut i32 = y;
            assert_eq!(*y, 2);
        }
        EnumProj::Variant2 { field1, field2 } => {
            let _x: Pin<&mut i32> = field1;
            let _y: &mut i32 = field2;
        }
        EnumProj::None => {}
    }

    assert_eq!(Pin::into_ref(e_orig).get_ref(), &Enum::Variant1(1, 2));

    let mut e = Enum::Variant2 { field1: 3, field2: 4 };
    let mut e = Pin::new(&mut e).project();

    match &mut e {
        EnumProj::Variant1(x, y) => {
            let _x: &mut Pin<&mut i32> = x;
            let _y: &mut &mut i32 = y;
        }
        EnumProj::Variant2 { field1, field2 } => {
            let x: &mut Pin<&mut i32> = field1;
            assert_eq!(**x, 3);

            let y: &mut &mut i32 = field2;
            assert_eq!(**y, 4);
        }
        EnumProj::None => {}
    }

    if let EnumProj::Variant2 { field1, field2 } = e {
        let x: Pin<&mut i32> = field1;
        assert_eq!(*x, 3);

        let y: &mut i32 = field2;
        assert_eq!(*y, 4);
    }
}

#[test]
fn enum_project_set() {
    #[pin_project(project_replace, project = EnumProj)]
    #[derive(Eq, PartialEq, Debug)]
    enum Enum {
        Variant1(#[pin] u8),
        Variant2(bool),
    }

    let mut e = Enum::Variant1(25);
    let mut e_orig = Pin::new(&mut e);
    let e_proj = e_orig.as_mut().project();

    match e_proj {
        EnumProj::Variant1(val) => {
            let new_e = Enum::Variant2(val.as_ref().get_ref() == &25);
            e_orig.set(new_e);
        }
        _ => unreachable!(),
    }

    assert_eq!(e, Enum::Variant2(true));
}

#[test]
fn where_clause() {
    #[pin_project]
    struct Struct<T>
    where
        T: Copy,
    {
        field: T,
    }

    #[pin_project]
    struct TupleStruct<T>(T)
    where
        T: Copy;

    #[pin_project]
    enum EnumWhere<T>
    where
        T: Copy,
    {
        Variant(T),
    }
}

#[test]
fn where_clause_and_associated_type_field() {
    #[pin_project(project_replace)]
    struct Struct1<I>
    where
        I: Iterator,
    {
        #[pin]
        field1: I,
        field2: I::Item,
    }

    #[pin_project(project_replace)]
    struct Struct2<I, J>
    where
        I: Iterator<Item = J>,
    {
        #[pin]
        field1: I,
        field2: J,
    }

    #[pin_project(project_replace)]
    struct Struct3<T>
    where
        T: 'static,
    {
        field: T,
    }

    trait Static: 'static {}

    impl<T> Static for Struct3<T> {}

    #[pin_project(project_replace)]
    struct TupleStruct<I>(#[pin] I, I::Item)
    where
        I: Iterator;

    #[pin_project(project_replace)]
    enum Enum<I>
    where
        I: Iterator,
    {
        Variant1(#[pin] I),
        Variant2(I::Item),
    }
}

#[test]
fn derive_copy() {
    #[pin_project(project_replace)]
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

    #[pin_project(project_replace)]
    struct Struct {
        val: NotCopy,
    }

    let x = Struct { val: NotCopy };
    let _val: NotCopy = x.val;

    #[pin_project(project_replace)]
    enum Enum {
        Variant(NotCopy),
    }

    let x = Enum::Variant(NotCopy);
    #[allow(clippy::infallible_destructuring_match)]
    let _val: NotCopy = match x {
        Enum::Variant(val) => val,
    };
}

#[test]
fn trait_bounds_on_type_generics() {
    #[pin_project(project_replace)]
    pub struct Struct1<'a, T: ?Sized> {
        field: &'a mut T,
    }

    #[pin_project(project_replace)]
    pub struct Struct2<'a, T: ::core::fmt::Debug> {
        field: &'a mut T,
    }

    #[pin_project(project_replace)]
    pub struct Struct3<'a, T: core::fmt::Debug> {
        field: &'a mut T,
    }

    #[pin_project(project_replace)]
    pub struct Struct4<'a, T: core::fmt::Debug + core::fmt::Display> {
        field: &'a mut T,
    }

    #[pin_project(project_replace)]
    pub struct Struct5<'a, T: core::fmt::Debug + ?Sized> {
        field: &'a mut T,
    }

    #[pin_project(project_replace)]
    pub struct Struct6<'a, T: core::fmt::Debug = [u8; 16]> {
        field: &'a mut T,
    }

    let _: Struct6<'_> = Struct6 { field: &mut [0u8; 16] };

    #[pin_project(project_replace)]
    pub struct Struct7<T: 'static> {
        field: T,
    }

    trait Static: 'static {}

    impl<T> Static for Struct7<T> {}

    #[pin_project(project_replace)]
    pub struct Struct8<'a, 'b: 'a> {
        field1: &'a u8,
        field2: &'b u8,
    }

    #[pin_project(project_replace)]
    pub struct TupleStruct<'a, T: ?Sized>(&'a mut T);

    #[pin_project(project_replace)]
    enum Enum<'a, T: ?Sized> {
        Variant(&'a mut T),
    }
}

#[test]
fn overlapping_lifetime_names() {
    #[pin_project(project_replace)]
    pub struct Struct1<'pin, T> {
        #[pin]
        field: &'pin mut T,
    }

    #[pin_project(project_replace)]
    pub struct Struct2<'pin, 'pin_, 'pin__> {
        #[pin]
        field: &'pin &'pin_ &'pin__ (),
    }

    pub trait A<'a> {}

    #[allow(single_use_lifetimes)] // https://github.com/rust-lang/rust/issues/55058
    #[pin_project(project_replace)]
    pub struct HRTB<'pin___, T>
    where
        for<'pin> &'pin T: Unpin,
        T: for<'pin> A<'pin>,
        for<'pin, 'pin_, 'pin__> &'pin &'pin_ &'pin__ T: Unpin,
    {
        #[pin]
        field: &'pin___ mut T,
    }
}

#[test]
fn combine() {
    #[pin_project(PinnedDrop, UnsafeUnpin)]
    pub struct PinnedDropWithUnsafeUnpin<T> {
        #[pin]
        field: T,
    }

    #[pinned_drop]
    impl<T> PinnedDrop for PinnedDropWithUnsafeUnpin<T> {
        fn drop(self: Pin<&mut Self>) {}
    }

    unsafe impl<T: Unpin> UnsafeUnpin for PinnedDropWithUnsafeUnpin<T> {}

    #[pin_project(PinnedDrop, !Unpin)]
    pub struct PinnedDropWithNotUnpin<T> {
        #[pin]
        field: T,
    }

    #[pinned_drop]
    impl<T> PinnedDrop for PinnedDropWithNotUnpin<T> {
        fn drop(self: Pin<&mut Self>) {}
    }

    #[pin_project(UnsafeUnpin, project_replace)]
    pub struct UnsafeUnpinWithReplace<T> {
        #[pin]
        field: T,
    }

    unsafe impl<T: Unpin> UnsafeUnpin for UnsafeUnpinWithReplace<T> {}

    #[pin_project(!Unpin, project_replace)]
    pub struct NotUnpinWithReplace<T> {
        #[pin]
        field: T,
    }
}

#[test]
fn private_type_in_public_type() {
    #[pin_project(project_replace)]
    pub struct PublicStruct<T> {
        #[pin]
        inner: PrivateStruct<T>,
    }

    struct PrivateStruct<T>(T);
}

#[allow(clippy::needless_lifetimes)]
#[test]
fn lifetime_project() {
    #[pin_project(project_replace)]
    struct Struct1<T, U> {
        #[pin]
        pinned: T,
        unpinned: U,
    }

    #[pin_project(project_replace)]
    struct Struct2<'a, T, U> {
        #[pin]
        pinned: &'a mut T,
        unpinned: U,
    }

    #[pin_project(project_replace, project = EnumProj, project_ref = EnumProjRef)]
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
                EnumProjRef::Variant { pinned, .. } => pinned,
            }
        }
        fn get_pin_mut<'a>(self: Pin<&'a mut Self>) -> Pin<&'a mut T> {
            match self.project() {
                EnumProj::Variant { pinned, .. } => pinned,
            }
        }
    }
}

#[rustversion::since(1.36)] // https://github.com/rust-lang/rust/pull/61207
#[test]
fn lifetime_project_elided() {
    #[pin_project(project_replace)]
    struct Struct1<T, U> {
        #[pin]
        pinned: T,
        unpinned: U,
    }

    #[pin_project(project_replace)]
    struct Struct2<'a, T, U> {
        #[pin]
        pinned: &'a mut T,
        unpinned: U,
    }

    #[pin_project(project_replace, project = EnumProj, project_ref = EnumProjRef)]
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
                EnumProjRef::Variant { pinned, .. } => pinned,
            }
        }
        fn get_pin_mut(self: Pin<&mut Self>) -> Pin<&mut T> {
            match self.project() {
                EnumProj::Variant { pinned, .. } => pinned,
            }
        }
    }
}

mod visibility {
    use pin_project::pin_project;

    #[pin_project(project_replace)]
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
    #[pin_project(project_replace)]
    pub struct NoGenerics {
        #[pin]
        field: PhantomPinned,
    }
}

#[test]
fn dst() {
    #[pin_project]
    struct Struct1<T: ?Sized> {
        x: T,
    }

    let mut x = Struct1 { x: 0_u8 };
    let x: Pin<&mut Struct1<dyn core::fmt::Debug>> = Pin::new(&mut x as _);
    let _y: &mut (dyn core::fmt::Debug) = x.project().x;

    #[pin_project]
    struct Struct2<T: ?Sized> {
        #[pin]
        x: T,
    }

    let mut x = Struct2 { x: 0_u8 };
    let x: Pin<&mut Struct2<dyn core::fmt::Debug + Unpin>> = Pin::new(&mut x as _);
    let _y: Pin<&mut (dyn core::fmt::Debug + Unpin)> = x.project().x;

    #[pin_project(UnsafeUnpin)]
    struct Struct5<T: ?Sized> {
        x: T,
    }

    #[pin_project(UnsafeUnpin)]
    struct Struct6<T: ?Sized> {
        #[pin]
        x: T,
    }

    #[pin_project(PinnedDrop)]
    struct Struct7<T: ?Sized> {
        x: T,
    }

    #[pinned_drop]
    impl<T: ?Sized> PinnedDrop for Struct7<T> {
        fn drop(self: Pin<&mut Self>) {}
    }

    #[pin_project(PinnedDrop)]
    struct Struct8<T: ?Sized> {
        #[pin]
        x: T,
    }

    #[pinned_drop]
    impl<T: ?Sized> PinnedDrop for Struct8<T> {
        fn drop(self: Pin<&mut Self>) {}
    }

    #[pin_project(!Unpin)]
    struct Struct9<T: ?Sized> {
        x: T,
    }

    #[pin_project(!Unpin)]
    struct Struct10<T: ?Sized> {
        #[pin]
        x: T,
    }

    #[pin_project]
    struct TupleStruct1<T: ?Sized>(T);

    #[pin_project]
    struct TupleStruct2<T: ?Sized>(#[pin] T);

    #[pin_project(UnsafeUnpin)]
    struct TupleStruct5<T: ?Sized>(T);

    #[pin_project(UnsafeUnpin)]
    struct TupleStruct6<T: ?Sized>(#[pin] T);

    #[pin_project(PinnedDrop)]
    struct TupleStruct7<T: ?Sized>(T);

    #[pinned_drop]
    impl<T: ?Sized> PinnedDrop for TupleStruct7<T> {
        fn drop(self: Pin<&mut Self>) {}
    }

    #[pin_project(PinnedDrop)]
    struct TupleStruct8<T: ?Sized>(#[pin] T);

    #[pinned_drop]
    impl<T: ?Sized> PinnedDrop for TupleStruct8<T> {
        fn drop(self: Pin<&mut Self>) {}
    }

    #[pin_project(!Unpin)]
    struct TupleStruct9<T: ?Sized>(T);

    #[pin_project(!Unpin)]
    struct TupleStruct10<T: ?Sized>(#[pin] T);
}

#[allow(explicit_outlives_requirements)] // https://github.com/rust-lang/rust/issues/60993
#[test]
fn unsized_in_where_clause() {
    #[pin_project]
    struct Struct3<T>
    where
        T: ?Sized,
    {
        x: T,
    }

    #[pin_project]
    struct Struct4<T>
    where
        T: ?Sized,
    {
        #[pin]
        x: T,
    }

    #[pin_project]
    struct TupleStruct3<T>(T)
    where
        T: ?Sized;

    #[pin_project]
    struct TupleStruct4<T>(#[pin] T)
    where
        T: ?Sized;
}

#[test]
fn dyn_type() {
    #[pin_project]
    struct Struct1 {
        f: dyn core::fmt::Debug,
    }

    #[pin_project]
    struct Struct2 {
        #[pin]
        f: dyn core::fmt::Debug,
    }

    #[pin_project]
    struct Struct3 {
        f: dyn core::fmt::Debug + Send,
    }

    #[pin_project]
    struct Struct4 {
        #[pin]
        f: dyn core::fmt::Debug + Send,
    }

    #[pin_project]
    struct TupleStruct1(dyn core::fmt::Debug);

    #[pin_project]
    struct TupleStruct2(#[pin] dyn core::fmt::Debug);

    #[pin_project]
    struct TupleStruct3(dyn core::fmt::Debug + Send);

    #[pin_project]
    struct TupleStruct4(#[pin] dyn core::fmt::Debug + Send);
}

#[test]
fn parse_self() {
    macro_rules! mac {
        ($($tt:tt)*) => {
            $($tt)*
        };
    }

    pub trait Trait {
        type Assoc;
    }

    #[pin_project(project_replace)]
    pub struct Generics<T: Trait<Assoc = Self>>
    where
        Self: Trait<Assoc = Self>,
        <Self as Trait>::Assoc: Sized,
        mac!(Self): Trait<Assoc = mac!(Self)>,
    {
        _f: T,
    }

    impl<T: Trait<Assoc = Self>> Trait for Generics<T> {
        type Assoc = Self;
    }

    #[pin_project(project_replace)]
    pub struct Struct {
        _f1: Box<Self>,
        _f2: Box<<Self as Trait>::Assoc>,
        _f3: Box<mac!(Self)>,
        _f4: [(); Self::ASSOC],
        _f5: [(); Self::assoc()],
        _f6: [(); mac!(Self::assoc())],
    }

    impl Struct {
        const ASSOC: usize = 1;
        const fn assoc() -> usize {
            0
        }
    }

    impl Trait for Struct {
        type Assoc = Self;
    }

    #[pin_project(project_replace)]
    struct Tuple(
        Box<Self>,
        Box<<Self as Trait>::Assoc>,
        Box<mac!(Self)>,
        [(); Self::ASSOC],
        [(); Self::assoc()],
        [(); mac!(Self::assoc())],
    );

    impl Tuple {
        const ASSOC: usize = 1;
        const fn assoc() -> usize {
            0
        }
    }

    impl Trait for Tuple {
        type Assoc = Self;
    }

    #[pin_project(project_replace)]
    enum Enum {
        Struct {
            _f1: Box<Self>,
            _f2: Box<<Self as Trait>::Assoc>,
            _f3: Box<mac!(Self)>,
            _f4: [(); Self::ASSOC],
            _f5: [(); Self::assoc()],
            _f6: [(); mac!(Self::assoc())],
        },
        Tuple(
            Box<Self>,
            Box<<Self as Trait>::Assoc>,
            Box<mac!(Self)>,
            [(); Self::ASSOC],
            [(); Self::assoc()],
            [(); mac!(Self::assoc())],
        ),
    }

    impl Enum {
        const ASSOC: usize = 1;
        const fn assoc() -> usize {
            0
        }
    }

    impl Trait for Enum {
        type Assoc = Self;
    }
}

#[test]
fn no_infer_outlives() {
    trait Bar<X> {
        type Y;
    }

    struct Example<A>(A);

    impl<X, T> Bar<X> for Example<T> {
        type Y = Option<T>;
    }

    #[pin_project(project_replace)]
    struct Foo<A, B> {
        _x: <Example<A> as Bar<B>>::Y,
    }
}

// https://github.com/rust-lang/rust/issues/47949
// https://github.com/taiki-e/pin-project/pull/194#discussion_r419098111
#[allow(clippy::many_single_char_names)]
#[test]
fn project_replace_panic() {
    use std::panic;

    #[pin_project(project_replace)]
    struct S<T, U> {
        #[pin]
        pinned: T,
        unpinned: U,
    }

    struct D<'a>(&'a mut bool, bool);
    impl Drop for D<'_> {
        fn drop(&mut self) {
            *self.0 = true;
            if self.1 {
                panic!()
            }
        }
    }

    let (mut a, mut b, mut c, mut d) = (false, false, false, false);
    let res = panic::catch_unwind(panic::AssertUnwindSafe(|| {
        let mut x = S { pinned: D(&mut a, true), unpinned: D(&mut b, false) };
        let _y = Pin::new(&mut x)
            .project_replace(S { pinned: D(&mut c, false), unpinned: D(&mut d, false) });
        // Previous `x.pinned` was dropped and panicked when `project_replace` is
        // called, so this is unreachable.
        unreachable!();
    }));
    assert!(res.is_err());
    assert!(a);
    assert!(b);
    assert!(c);
    assert!(d);

    let (mut a, mut b, mut c, mut d) = (false, false, false, false);
    let res = panic::catch_unwind(panic::AssertUnwindSafe(|| {
        let mut x = S { pinned: D(&mut a, false), unpinned: D(&mut b, true) };
        {
            let _y = Pin::new(&mut x)
                .project_replace(S { pinned: D(&mut c, false), unpinned: D(&mut d, false) });
            // `_y` (previous `x.unpinned`) live to the end of this scope, so
            // this is not unreachable.
            // unreachable!();
        }
        unreachable!();
    }));
    assert!(res.is_err());
    assert!(a);
    assert!(b);
    assert!(c);
    assert!(d);
}
