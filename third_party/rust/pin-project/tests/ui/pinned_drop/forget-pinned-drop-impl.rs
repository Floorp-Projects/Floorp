use pin_project::pin_project;

#[pin_project(PinnedDrop)] //~ ERROR E0277
struct Struct {
    #[pin]
    field: u8,
}

fn main() {}
