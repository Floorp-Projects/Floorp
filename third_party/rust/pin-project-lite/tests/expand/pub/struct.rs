use pin_project_lite::pin_project;

pin_project! {
    pub struct Struct<T, U> {
        #[pin]
        pub pinned: T,
        pub unpinned: U,
    }
}

fn main() {}
