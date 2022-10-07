use std::sync::Arc;

fn main() {}

pub struct Foo;

#[uniffi::export]
fn make_foo() -> Arc<Foo> {
    Arc::new(Foo)
}

mod child {
    use std::sync::Arc;

    enum Foo {}

    #[uniffi::export]
    fn take_foo(foo: Arc<Foo>) {
        match &*foo {}
    }
}

mod uniffi_types {
    pub use super::Foo;
}
