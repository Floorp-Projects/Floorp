use inherent::inherent;

trait A {
    fn a(&self) {}
}

struct X;

#[inherent]
impl A for X {
    default! {
        const NONSENSE: usize = 1;
    }
}

fn main() {}
