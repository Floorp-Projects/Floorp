use inherent::inherent;

trait A {
    fn a(&self) {}
}

struct X;

#[inherent]
impl A for X {
    default! {
        fn a(&self) {
            println!("Default with a body >:-P");
        }
    }
}

fn main() {}
