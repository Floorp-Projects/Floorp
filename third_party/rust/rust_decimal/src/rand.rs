#![cfg(feature = "rand")]

use crate::Decimal;
use rand::{
    distributions::{Distribution, Standard},
    Rng,
};

impl Distribution<Decimal> for Standard {
    fn sample<R>(&self, rng: &mut R) -> Decimal
    where
        R: Rng + ?Sized,
    {
        Decimal::from_parts(
            rng.next_u32(),
            rng.next_u32(),
            rng.next_u32(),
            rng.gen(),
            rng.next_u32(),
        )
    }
}

#[test]
fn has_random_decimal_instances() {
    let mut rng = rand::rngs::OsRng;
    let random: [Decimal; 32] = rng.gen();
    assert!(random.windows(2).any(|slice| { slice[0] != slice[1] }));
}
