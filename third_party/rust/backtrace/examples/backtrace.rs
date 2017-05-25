extern crate backtrace;

use backtrace::Backtrace;

fn main() {
    println!("{:?}", Backtrace::new());
}
