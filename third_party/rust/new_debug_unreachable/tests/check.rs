#[macro_use]
extern crate debug_unreachable;

#[test]
#[should_panic]
fn explodes_in_debug() {
    unsafe { debug_unreachable!() }
}

