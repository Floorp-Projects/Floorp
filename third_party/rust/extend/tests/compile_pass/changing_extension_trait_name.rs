use extend::ext;

#[ext(name = Foo)]
impl i32 {
    fn foo() {}
}

fn main() {
    <i32 as Foo>::foo();
}
