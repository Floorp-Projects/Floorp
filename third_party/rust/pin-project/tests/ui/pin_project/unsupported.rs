use pin_project::pin_project;

#[pin_project]
struct Struct1 {} //~ ERROR may not be used on structs with zero fields

#[pin_project]
struct Struct2(); //~ ERROR may not be used on structs with zero fields

#[pin_project]
struct Struct3; //~ ERROR may not be used on structs with units

#[pin_project]
enum Enum1 {} //~ ERROR may not be used on enums without variants

#[pin_project]
enum Enum2 {
    A = 2, //~ ERROR may not be used on enums with discriminants
}

#[pin_project]
enum Enum3 {
    A, //~ ERROR may not be used on enums that have no field
    B,
}

#[pin_project]
union Union {
    //~^ ERROR may only be used on structs or enums
    x: u8,
}

fn main() {}
