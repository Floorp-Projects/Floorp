use pin_project_lite::pin_project;

pin_project! {
    #[project_ref = EnumProjRef]
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
