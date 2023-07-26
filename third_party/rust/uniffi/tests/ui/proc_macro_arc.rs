use std::sync::Arc;

fn main() {}

// Normally this is defined by the scaffolding code, manually define it for the UI test
pub struct UniFfiTag;

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
