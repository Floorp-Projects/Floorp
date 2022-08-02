use extend::ext;

#[ext]
impl &i32 {
    fn foo() {}
}

#[ext]
impl &mut i32 {
    fn bar() {}
}

fn main() {}
