#[derive(derive_more::Debug)]
pub struct Foo {
    #[debug(fmt = "Stuff({}): {}", "bar")]
    bar: String,
}

fn main() {}
