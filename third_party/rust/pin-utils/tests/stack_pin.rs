use pin_utils::pin_mut;
use core::pin::Pin;

#[test]
fn stack_pin() {
    struct Foo {}
    let foo = Foo {};
    pin_mut!(foo);
    let _: Pin<&mut Foo> = foo;
}
