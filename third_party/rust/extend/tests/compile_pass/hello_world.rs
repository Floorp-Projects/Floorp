use extend::ext;

#[ext]
impl i32 {
    fn add_one(&self) -> Self {
        self + 1
    }

    fn foo() -> MyType {
        MyType
    }
}

#[derive(Debug, Eq, PartialEq)]
struct MyType;

fn main() {
    assert_eq!(i32::foo(), MyType);
    assert_eq!(1.add_one(), 2);
}
