#![warn(rust_2018_idioms, single_use_lifetimes)]

use std::pin::Pin;

use pin_project::{pin_project, pinned_drop};

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
fn self_argument_in_macro() {
    use std::pin::Pin;

    use pin_project::{pin_project, pinned_drop};

    #[pin_project(PinnedDrop)]
    struct Struct {
        x: (),
    }

    #[pinned_drop]
    impl PinnedDrop for Struct {
        fn drop(self: Pin<&mut Self>) {
            let _: Vec<_> = vec![self.x];
        }
    }
}

#[test]
fn self_in_macro_containing_fn() {
    use std::pin::Pin;

    use pin_project::{pin_project, pinned_drop};

    macro_rules! mac {
        ($($tt:tt)*) => {
            $($tt)*
        };
    }

    #[pin_project(PinnedDrop)]
    pub struct Struct {
        _x: (),
    }

    #[pinned_drop]
    impl PinnedDrop for Struct {
        fn drop(self: Pin<&mut Self>) {
            let _ = mac!({
                impl Struct {
                    pub fn _f(self) -> Self {
                        self
                    }
                }
            });
        }
    }
}

#[test]
fn self_call() {
    use std::pin::Pin;

    use pin_project::{pin_project, pinned_drop};

    #[pin_project(PinnedDrop)]
    pub struct Struct<T> {
        _x: T,
    }

    trait Trait {
        fn self_ref(&self) {}
        fn self_pin_ref(self: Pin<&Self>) {}
        fn self_mut(&mut self) {}
        fn self_pin_mut(self: Pin<&mut Self>) {}
        fn assoc_fn(_this: Pin<&mut Self>) {}
    }

    impl<T> Trait for Struct<T> {}

    #[pinned_drop]
    impl<T> PinnedDrop for Struct<T> {
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

#[test]
fn self_struct() {
    use std::pin::Pin;

    use pin_project::{pin_project, pinned_drop};

    #[pin_project(PinnedDrop)]
    pub struct Struct {
        pub x: (),
    }

    #[pinned_drop]
    impl PinnedDrop for Struct {
        #[allow(irrefutable_let_patterns)]
        #[allow(clippy::match_single_binding)]
        fn drop(mut self: Pin<&mut Self>) {
            // expr
            let _: Self = Self { x: () };

            // pat
            match *self {
                Self { x: _ } => {}
            }
            if let Self { x: _ } = *self {}
            let Self { x: _ } = *self;
        }
    }

    #[pin_project(PinnedDrop)]
    pub struct TupleStruct(());

    #[pinned_drop]
    impl PinnedDrop for TupleStruct {
        #[allow(irrefutable_let_patterns)]
        fn drop(mut self: Pin<&mut Self>) {
            // expr
            let _: Self = Self(());

            // pat
            match *self {
                Self(_) => {}
            }
            if let Self(_) = *self {}
            let Self(_) = *self;
        }
    }
}

#[rustversion::since(1.37)] // type_alias_enum_variants requires Rust 1.37
#[test]
fn self_enum() {
    use std::pin::Pin;

    use pin_project::{pin_project, pinned_drop};

    #[pin_project(PinnedDrop)]
    pub enum Enum {
        Struct { x: () },
        Tuple(()),
    }

    #[pinned_drop]
    impl PinnedDrop for Enum {
        fn drop(mut self: Pin<&mut Self>) {
            // expr
            let _: Self = Self::Struct { x: () };
            let _: Self = Self::Tuple(());

            // pat
            match *self {
                Self::Struct { x: _ } => {}
                Self::Tuple(_) => {}
            }
            if let Self::Struct { x: _ } = *self {}
            if let Self::Tuple(_) = *self {}
        }
    }
}

// See also `ui/pinned_drop/self.rs`.
#[rustversion::since(1.40)] // https://github.com/rust-lang/rust/pull/64690
#[test]
fn self_in_macro_def() {
    use std::pin::Pin;

    use pin_project::{pin_project, pinned_drop};

    #[pin_project(PinnedDrop)]
    pub struct Struct {
        _x: (),
    }

    #[pinned_drop]
    impl PinnedDrop for Struct {
        fn drop(self: Pin<&mut Self>) {
            macro_rules! mac {
                () => {{
                    let _ = self;
                }};
            }
            mac!();
        }
    }
}

#[test]
fn self_inside_macro() {
    use std::pin::Pin;

    use pin_project::{pin_project, pinned_drop};

    macro_rules! mac {
        ($($tt:tt)*) => {
            $($tt)*
        };
    }

    #[pin_project(PinnedDrop)]
    pub struct Struct<T: Send>
    where
        mac!(Self): Send,
    {
        _x: T,
    }

    impl<T: Send> Struct<T> {
        const ASSOCIATED1: &'static str = "1";
        fn associated1() {}
    }

    trait Trait {
        type Associated2;
        const ASSOCIATED2: &'static str;
        fn associated2();
    }

    impl<T: Send> Trait for Struct<T> {
        type Associated2 = ();
        const ASSOCIATED2: &'static str = "2";
        fn associated2() {}
    }

    #[pinned_drop]
    impl<T: Send> PinnedDrop for Struct<T>
    where
        mac!(Self): Send,
    {
        #[allow(path_statements)]
        #[allow(clippy::no_effect)]
        fn drop(self: Pin<&mut Self>) {
            // inherent items
            mac!(Self::ASSOCIATED1;);
            mac!(<Self>::ASSOCIATED1;);
            mac!(Self::associated1(););
            mac!(<Self>::associated1(););

            // trait items
            mac!(let _: <Self as Trait>::Associated2;);
            mac!(Self::ASSOCIATED2;);
            mac!(<Self>::ASSOCIATED2;);
            mac!(<Self as Trait>::ASSOCIATED2;);
            mac!(Self::associated2(););
            mac!(<Self>::associated2(););
            mac!(<Self as Trait>::associated2(););
        }
    }
}

#[test]
fn inside_macro() {
    use std::pin::Pin;

    use pin_project::{pin_project, pinned_drop};

    #[pin_project(PinnedDrop)]
    struct Struct(());

    macro_rules! mac {
        ($expr:expr) => {
            #[pinned_drop]
            impl PinnedDrop for Struct {
                #[allow(clippy::no_effect)]
                fn drop(self: Pin<&mut Self>) {
                    $expr;
                }
            }
        };
    }

    mac!(1);
}
