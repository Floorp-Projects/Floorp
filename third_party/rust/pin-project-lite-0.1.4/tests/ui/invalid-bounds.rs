use pin_project_lite::pin_project;

pin_project! {
    struct A<T: 'static : ?Sized> { //~ ERROR no rules expected the token `:`
        field: T,
    }
}

pin_project! {
    struct B<T: Sized : 'static> { //~ ERROR expected one of `+`, `,`, `=`, or `>`, found `:`
        field: T,
    }
}

fn main() {}
