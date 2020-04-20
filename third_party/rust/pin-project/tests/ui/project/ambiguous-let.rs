use pin_project::{pin_project, project};

#[pin_project]
enum Enum<A, B> {
    A(#[pin] A),
    B(B),
}

struct Struct<T>(T);

#[project]
fn foo() {
    let mut foo: Enum<bool, bool> = Enum::A(true);

    #[project]
    let Struct(x) = match Pin::new(&mut foo).project() {
        //~^ ERROR Both initializer expression and pattern are replaceable, you need to split the initializer expression into separate let bindings to avoid ambiguity
        Enum::A(_) => Struct(true),
        Enum::B(_) => unreachable!(),
    };
    assert!(x);
}

fn main() {}
