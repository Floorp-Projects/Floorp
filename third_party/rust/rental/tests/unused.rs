use std::rc::Rc;

#[macro_use]
extern crate rental;

pub struct Sample {
    field: i32,
}

rental! {
    mod sample_rental {
        use super::*;

        #[rental]
        pub struct SampleRental {
            sample: Rc<Sample>,
            sref: &'sample i32,
        }
    }
}
use self::sample_rental::SampleRental;

#[test]
fn unused() {
    let sample = Rc::new(Sample { field: 42 });
    let rental = SampleRental::new(sample, |sample_rc| &sample_rc.field);
    rental.rent(|this| println!("{}", this));
}
