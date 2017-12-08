// incorrect visibility restriction
#[macro_use]
extern crate lazy_static;

lazy_static! {
    pub(nonsense) static ref WRONG: () = ();
    //~^ ERROR incorrect visibility restriction
}

fn main() { }
