use extend::ext;
use std::marker::PhantomData;

struct Foo<T>(PhantomData<T>);

#[ext]
impl<T, K> T {
    fn some_method(&self, _: Foo<K>) {}
}

fn main() {}
