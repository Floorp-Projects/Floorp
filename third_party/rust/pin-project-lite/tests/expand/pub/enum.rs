use pin_project_lite::pin_project;

pin_project! {
    #[project = EnumProj]
    #[project_ref = EnumProjRef]
    pub enum Enum<T, U> {
        Struct {
            #[pin]
            pinned: T,
            unpinned: U,
        },
        Unit,
    }
}

fn main() {}
