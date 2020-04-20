#![warn(unsafe_code)]
#![warn(rust_2018_idioms, single_use_lifetimes)]
#![allow(dead_code)]

use pin_project::{pin_project, pinned_drop};
use std::pin::Pin;

#[test]
fn safe_project() {
    #[pin_project(PinnedDrop)]
    pub struct Struct<'a> {
        was_dropped: &'a mut bool,
        #[pin]
        field: u8,
    }

    #[pinned_drop]
    impl PinnedDrop for Struct<'_> {
        fn drop(self: Pin<&mut Self>) {
            **self.project().was_dropped = true;
        }
    }

    let mut was_dropped = false;
    drop(Struct { was_dropped: &mut was_dropped, field: 42 });
    assert!(was_dropped);
}

#[test]
fn mut_self_argument() {
    #[pin_project(PinnedDrop)]
    struct Struct {
        data: usize,
    }

    #[pinned_drop]
    impl PinnedDrop for Struct {
        fn drop(mut self: Pin<&mut Self>) {
            let _: &mut _ = &mut self.data;
        }
    }
}

#[test]
fn self_in_vec() {
    #[pin_project(PinnedDrop)]
    struct Struct {
        data: usize,
    }

    #[pinned_drop]
    impl PinnedDrop for Struct {
        fn drop(self: Pin<&mut Self>) {
            let _: Vec<_> = vec![self.data];
        }
    }
}

#[test]
fn self_in_macro_containing_fn() {
    #[pin_project(PinnedDrop)]
    pub struct Struct {
        data: usize,
    }

    macro_rules! emit {
        ($($tt:tt)*) => {
            $($tt)*
        };
    }

    #[pinned_drop]
    impl PinnedDrop for Struct {
        fn drop(self: Pin<&mut Self>) {
            let _ = emit!({
                impl Struct {
                    pub fn f(self) {}
                }
            });
            self.data;
        }
    }
}

#[test]
fn self_call() {
    #[pin_project(PinnedDrop)]
    pub struct Struct {
        data: usize,
    }

    trait Trait {
        fn self_ref(&self) {}
        fn self_pin_ref(self: Pin<&Self>) {}
        fn self_mut(&mut self) {}
        fn self_pin_mut(self: Pin<&mut Self>) {}
        fn assoc_fn(_this: Pin<&mut Self>) {}
    }

    impl Trait for Struct {}

    #[pinned_drop]
    impl PinnedDrop for Struct {
        fn drop(mut self: Pin<&mut Self>) {
            self.self_ref();
            self.as_ref().self_pin_ref();
            self.self_mut();
            self.as_mut().self_pin_mut();
            Self::assoc_fn(self.as_mut());
            <Self>::assoc_fn(self.as_mut());
        }
    }
}

// See also `ui/pinned_drop/self.rs`.
#[test]
fn self_expr() {
    #[pin_project(PinnedDrop)]
    pub struct Struct {
        x: usize,
    }

    #[pinned_drop]
    impl PinnedDrop for Struct {
        fn drop(mut self: Pin<&mut Self>) {
            let _: Self = Self { x: 0 };
        }
    }

    #[pin_project(PinnedDrop)]
    pub struct TupleStruct(usize);

    #[pinned_drop]
    impl PinnedDrop for TupleStruct {
        fn drop(mut self: Pin<&mut Self>) {
            let _: Self = Self(0);
        }
    }

    #[rustversion::since(1.37)]
    #[pin_project(PinnedDrop)]
    pub enum Enum {
        StructVariant { x: usize },
        TupleVariant(usize),
    }

    #[rustversion::since(1.37)]
    #[pinned_drop]
    impl PinnedDrop for Enum {
        fn drop(mut self: Pin<&mut Self>) {
            // let _: Self = Self::StructVariant { x: 0 }; //~ ERROR can't use generic parameters from outer function [E0401]
            let _: Self = Self::TupleVariant(0);
        }
    }
}

// See also `ui/pinned_drop/self.rs`.
#[test]
fn self_pat() {
    #[pin_project(PinnedDrop)]
    pub struct Struct {
        x: usize,
    }

    #[pinned_drop]
    impl PinnedDrop for Struct {
        fn drop(mut self: Pin<&mut Self>) {
            // match *self {
            //     Self { x: _ } => {} //~ ERROR can't use generic parameters from outer function [E0401]
            // }
            // if let Self { x: _ } = *self {} //~ ERROR can't use generic parameters from outer function [E0401]
            // let Self { x: _ } = *self; //~ ERROR can't use generic parameters from outer function [E0401]
        }
    }

    #[pin_project(PinnedDrop)]
    pub struct TupleStruct(usize);

    #[pinned_drop]
    impl PinnedDrop for TupleStruct {
        #[allow(irrefutable_let_patterns)]
        fn drop(mut self: Pin<&mut Self>) {
            match *self {
                Self(_) => {}
            }
            if let Self(_) = *self {}
            let Self(_) = *self;
        }
    }

    #[rustversion::since(1.37)]
    #[pin_project(PinnedDrop)]
    pub enum Enum {
        StructVariant { x: usize },
        TupleVariant(usize),
    }

    #[rustversion::since(1.37)]
    #[pinned_drop]
    impl PinnedDrop for Enum {
        fn drop(mut self: Pin<&mut Self>) {
            // match *self {
            //     Self::StructVariant { x: _ } => {} //~ ERROR can't use generic parameters from outer function [E0401]
            //     Self::TupleVariant(_) => {} //~ ERROR can't use generic parameters from outer function [E0401]
            // }
            // if let Self::StructVariant { x: _ } = *self {} //~ ERROR can't use generic parameters from outer function [E0401]
            // if let Self::TupleVariant(_) = *self {} //~ ERROR can't use generic parameters from outer function [E0401]
        }
    }
}
