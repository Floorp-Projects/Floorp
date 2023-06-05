#![allow(warnings)]

use extend::ext;

#[ext]
impl i32 {
    fn foo(self, (a, b): (i32, i32)) {}

    fn bar(self, [a, b]: [i32; 2]) {}
}

fn main() {}
