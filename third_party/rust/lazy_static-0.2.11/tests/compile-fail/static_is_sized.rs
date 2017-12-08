// error-pattern:the trait bound `str: std::marker::Sized` is not satisfied
#[macro_use]
extern crate lazy_static;

lazy_static! {
    pub static ref FOO: str = panic!();
}


fn main() {
}
