use pin_project_lite::pin_project;
use std::pin::Pin;

pin_project! {
    #[project = EnumProj]
    #[project_ref = EnumProjRef]
    enum Enum<T, U> {
        Struct {
            #[pin]
            pinned: T,
            unpinned: U,
        },
        Unit,
    }
    impl<T, U> PinnedDrop for Enum<T, U> {
        fn drop(this: Pin<&mut Self>) {
            let _ = this;
        }
    }
}

fn main() {}
