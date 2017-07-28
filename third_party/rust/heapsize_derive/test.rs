#[macro_use] extern crate heapsize_derive;

mod heapsize {
    pub trait HeapSizeOf {
        fn heap_size_of_children(&self) -> usize;
    }

    impl<T> HeapSizeOf for Box<T> {
        fn heap_size_of_children(&self) -> usize {
            ::std::mem::size_of::<T>()
        }
    }
}


#[derive(HeapSizeOf)]
struct Foo([Box<u32>; 2], Box<u8>);

#[test]
fn test() {
    use heapsize::HeapSizeOf;
    assert_eq!(Foo([Box::new(1), Box::new(2)], Box::new(3)).heap_size_of_children(), 9);
}
