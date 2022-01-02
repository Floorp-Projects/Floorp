use pin_project_lite::pin_project;

pin_project! {
    #[project = EnumProj]
    enum Enum<T, U> {
        Struct {
            #[pin]
            pinned: T,
            unpinned: U,
        },
        Unit,
    }
}

fn main() {}
