mod foo {
    use extend::ext;

    #[ext(pub)]
    impl<T1, T2, T3> (T1, T2, T3) {
        fn size(&self) -> usize {
            3
        }
    }
}

fn main() {
    use foo::TupleOfT1T2T3Ext;

    assert_eq!(3, (0, 0, 0).size());
}
