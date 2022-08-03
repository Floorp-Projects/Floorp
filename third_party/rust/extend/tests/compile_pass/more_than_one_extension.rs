use extend::ext;

#[ext]
impl i32 {
    fn foo() {}
}

#[ext]
impl i64 {
    fn bar() {}
}

fn main() {}
