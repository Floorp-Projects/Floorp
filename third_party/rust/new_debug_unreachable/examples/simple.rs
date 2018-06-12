#[macro_use]
extern crate debug_unreachable;

fn main() {
    if 0 > 100 {
        // Can't happen!
        unsafe { debug_unreachable!() }
    } else {
        println!("Good, 0 <= 100.");
    }
}
