use pin_project::pin_project;

#[pin_project(UnsafeUnpin, UnsafeUnpin)] //~ ERROR duplicate `UnsafeUnpin` argument
struct UnsafeUnpin<T> {
    #[pin]
    pinned: T,
}

#[pin_project(PinnedDrop, PinnedDrop)] //~ ERROR duplicate `PinnedDrop` argument
struct PinnedDrop<T> {
    #[pin]
    pinned: T,
}

#[pin_project(PinnedDrop, UnsafeUnpin, UnsafeUnpin)] //~ ERROR duplicate `UnsafeUnpin` argument
struct Duplicate3<T> {
    #[pin]
    pinned: T,
}

#[pin_project(PinnedDrop, UnsafeUnpin, PinnedDrop, PinnedDrop)] //~ ERROR duplicate `PinnedDrop` argument
struct Duplicate4<T> {
    #[pin]
    pinned: T,
}

fn main() {}
