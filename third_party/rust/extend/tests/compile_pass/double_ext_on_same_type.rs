use extend::ext;

#[ext]
impl Option<usize> {
    fn foo() -> usize {
        1
    }
}

#[ext]
impl Option<i32> {
    fn bar() -> i32 {
        1
    }
}

fn main() {}
