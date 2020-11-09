mod types {
    use inherent::inherent;

    trait Trait {
        fn f<T: ?Sized>(self);
        // A default method
        fn g(&self) {}
    }

    pub struct Struct;

    #[inherent(pub)]
    impl Trait for Struct {
        fn f<T: ?Sized>(self) {}
        default! {
            fn g(&self);
        }
    }
}

#[test]
fn test() {
    // types::Trait is not in scope.
    let s = types::Struct;
    s.g();
    s.f::<str>();
}
