// default #[pin_project], PinnedDrop, project_replace, !Unpin, and UnsafeUnpin without UnsafeUnpin impl are completely safe.

#[::pin_project::pin_project]
#[derive(Debug)]
pub struct DefaultStruct<T, U> {
    #[pin]
    pub pinned: T,
    pub unpinned: U,
}

#[::pin_project::pin_project]
#[derive(Debug)]
pub struct DefaultTupleStruct<T, U>(#[pin] pub T, pub U);

#[::pin_project::pin_project]
#[derive(Debug)]
pub enum DefaultEnum<T, U> {
    Struct {
        #[pin]
        pinned: T,
        unpinned: U,
    },
    Tuple(#[pin] T, U),
    Unit,
}

#[::pin_project::pin_project(PinnedDrop)]
#[derive(Debug)]
pub struct PinnedDropStruct<T, U> {
    #[pin]
    pub pinned: T,
    pub unpinned: U,
}

#[::pin_project::pinned_drop]
impl<T, U> PinnedDrop for PinnedDropStruct<T, U> {
    fn drop(self: ::pin_project::__private::Pin<&mut Self>) {}
}

#[::pin_project::pin_project(PinnedDrop)]
#[derive(Debug)]
pub struct PinnedDropTupleStruct<T, U>(#[pin] pub T, pub U);

#[::pin_project::pinned_drop]
impl<T, U> PinnedDrop for PinnedDropTupleStruct<T, U> {
    fn drop(self: ::pin_project::__private::Pin<&mut Self>) {}
}

#[::pin_project::pin_project(PinnedDrop)]
#[derive(Debug)]
pub enum PinnedDropEnum<T, U> {
    Struct {
        #[pin]
        pinned: T,
        unpinned: U,
    },
    Tuple(#[pin] T, U),
    Unit,
}

#[::pin_project::pinned_drop]
impl<T, U> PinnedDrop for PinnedDropEnum<T, U> {
    fn drop(self: ::pin_project::__private::Pin<&mut Self>) {}
}

#[::pin_project::pin_project(project_replace)]
#[derive(Debug)]
pub struct ReplaceStruct<T, U> {
    #[pin]
    pub pinned: T,
    pub unpinned: U,
}

#[::pin_project::pin_project(project_replace)]
#[derive(Debug)]
pub struct ReplaceTupleStruct<T, U>(#[pin] pub T, pub U);

#[::pin_project::pin_project(project_replace)]
#[derive(Debug)]
pub enum ReplaceEnum<T, U> {
    Struct {
        #[pin]
        pinned: T,
        unpinned: U,
    },
    Tuple(#[pin] T, U),
    Unit,
}

#[::pin_project::pin_project(UnsafeUnpin)]
#[derive(Debug)]
pub struct UnsafeUnpinStruct<T, U> {
    #[pin]
    pub pinned: T,
    pub unpinned: U,
}

#[::pin_project::pin_project(UnsafeUnpin)]
#[derive(Debug)]
pub struct UnsafeUnpinTupleStruct<T, U>(#[pin] pub T, pub U);

#[::pin_project::pin_project(UnsafeUnpin)]
#[derive(Debug)]
pub enum UnsafeUnpinEnum<T, U> {
    Struct {
        #[pin]
        pinned: T,
        unpinned: U,
    },
    Tuple(#[pin] T, U),
    Unit,
}

#[::pin_project::pin_project(!Unpin)]
#[derive(Debug)]
pub struct NotUnpinStruct<T, U> {
    #[pin]
    pub pinned: T,
    pub unpinned: U,
}

#[::pin_project::pin_project(!Unpin)]
#[derive(Debug)]
pub struct NotUnpinTupleStruct<T, U>(#[pin] pub T, pub U);

#[::pin_project::pin_project(!Unpin)]
#[derive(Debug)]
pub enum NotUnpinEnum<T, U> {
    Struct {
        #[pin]
        pinned: T,
        unpinned: U,
    },
    Tuple(#[pin] T, U),
    Unit,
}
