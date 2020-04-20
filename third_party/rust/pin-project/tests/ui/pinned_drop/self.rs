use pin_project::{pin_project, pinned_drop};
use std::pin::Pin;

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

    #[pin_project(PinnedDrop)]
    pub enum Enum {
        StructVariant { x: usize },
        TupleVariant(usize),
    }

    #[pinned_drop]
    impl PinnedDrop for Enum {
        fn drop(mut self: Pin<&mut Self>) {
            let _: Self = Self::StructVariant { x: 0 }; //~ ERROR can't use generic parameters from outer function [E0401]
            let _: Self = Self::TupleVariant(0);
        }
    }
}

fn self_pat() {
    #[pin_project(PinnedDrop)]
    pub struct Struct {
        x: usize,
    }

    #[pinned_drop]
    impl PinnedDrop for Struct {
        fn drop(mut self: Pin<&mut Self>) {
            match *self {
                Self { x: _ } => {} //~ ERROR can't use generic parameters from outer function [E0401]
            }
            if let Self { x: _ } = *self {} //~ ERROR can't use generic parameters from outer function [E0401]
            let Self { x: _ } = *self; //~ ERROR can't use generic parameters from outer function [E0401]
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

    #[pin_project(PinnedDrop)]
    pub enum Enum {
        StructVariant { x: usize },
        TupleVariant(usize),
    }

    #[pinned_drop]
    impl PinnedDrop for Enum {
        fn drop(mut self: Pin<&mut Self>) {
            match *self {
                Self::StructVariant { x: _ } => {} //~ ERROR can't use generic parameters from outer function [E0401]
                Self::TupleVariant(_) => {} //~ ERROR can't use generic parameters from outer function [E0401]
            }
            if let Self::StructVariant { x: _ } = *self {} //~ ERROR can't use generic parameters from outer function [E0401]
            if let Self::TupleVariant(_) = *self {} //~ ERROR can't use generic parameters from outer function [E0401]
        }
    }
}

fn main() {}
