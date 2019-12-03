pub trait Invoke<A> {
    type Result;

    fn invoke(self, arg: A) -> Self::Result;
}

#[derive(Copy, Clone)]
pub struct Identity;

impl<A> Invoke<A> for Identity {
    type Result = A;

    fn invoke(self, arg: A) -> A {
        arg
    }
}
