mod a {
    use extend::ext;

    #[ext(pub)]
    impl i32 {
        fn foo() -> Foo { Foo }
    }

    pub struct Foo;
}

fn main() {
    use a::i32Ext;
    i32::foo();
}
