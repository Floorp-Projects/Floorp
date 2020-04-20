use pin_project::{pin_project, project};

trait Bar<X> {
    type Y;
}

struct Example<A>(A);

impl<X, T> Bar<X> for Example<T> {
    type Y = Option<T>;
}

#[pin_project]
struct Foo<A, B> {
    _x: <Example<A> as Bar<B>>::Y,
}

#[project]
impl<A, B> Foo<A, B> {}
