use extend::ext;

#[ext(pub(crate), name = Foo)]
impl i32 {
    fn foo() {}
}

#[ext(pub, name = Bar)]
impl i64 {
    fn foo() {}
}

fn main() {}
