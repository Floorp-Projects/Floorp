#[derive(derive_more::Display)]
#[display(fmt = "Stuff({}): {}", "bar")]
pub struct Foo {
    bar: String,
}

fn main() {}
