pub mod self_in_macro_def {
    use pin_project::{pin_project, pinned_drop};
    use std::pin::Pin;

    #[pin_project(PinnedDrop)]
    pub struct Struct {
        x: (),
    }

    #[pinned_drop]
    impl PinnedDrop for Struct {
        fn drop(self: Pin<&mut Self>) {
            macro_rules! t {
                () => {{
                    let _ = self; //~ ERROR E0434

                    fn f(self: ()) {} //~ ERROR `self` parameter is only allowed in associated functions
                }};
            }
            t!();
        }
    }
}

pub mod self_span {
    use pin_project::{pin_project, pinned_drop};
    use std::pin::Pin;

    #[pin_project(PinnedDrop)]
    pub struct S {
        x: (),
    }

    #[pinned_drop]
    impl PinnedDrop for S {
        fn drop(self: Pin<&mut Self>) {
            let _: () = self; //~ ERROR E0308
            let _: Self = Self; //~ ERROR E0423
        }
    }

    #[pin_project(PinnedDrop)]
    pub enum E {
        V { x: () },
    }

    #[pinned_drop]
    impl PinnedDrop for E {
        fn drop(self: Pin<&mut Self>) {
            let _: () = self; //~ ERROR E0308
            let _: Self = Self::V; //~ ERROR E0533
        }
    }
}

fn main() {}
