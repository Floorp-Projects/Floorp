mod a {
    use extend::ext;

    #[ext(pub(super))]
    pub impl i32 {
        fn foo() -> Foo {
            Foo
        }
    }

    pub struct Foo;
}

fn main() {}
