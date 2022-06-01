#![deny(warnings)]

macro_rules! m {
    ($i:ident) => {
        paste::item! {
            pub fn [<foo $i>]() {}
        }
    };
}

m!(Bar);

fn main() {}
