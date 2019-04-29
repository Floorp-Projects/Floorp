/*! Sieve of Eratosthenes

The `bit_vec` crate had this as an example, so I do too, I guess.

Run with

```sh
$ cargo run --release --example sieve -- [max] [count]
```

where max is an optional maximum number below which all primes will be found,
and count is an optional number whose square will be used to display the bottom
primes.

For example,

```sh
$ cargo run --release --example sieve -- 10000000 25
```

will find all primes less than ten million, and print the primes below 625 in a
square 25x25.
!*/

#[cfg(feature = "alloc")]
extern crate bitvec;

#[cfg(feature = "alloc")]
use bitvec::{
	BitVec,
	BigEndian,
};
#[cfg(feature = "alloc")]
use std::env;

#[cfg(feature = "alloc")]
fn main() {
	let max_prime: usize = env::args()
		.nth(1)
		.unwrap_or("1000000".into())
		.parse()
		.unwrap_or(1_000_000);

	let primes = {
		let mut bv = BitVec::<BigEndian, u64>::with_capacity(max_prime);
		bv.set_elements(!0u64);

		//  Consider the vector fully populated
		unsafe { bv.set_len(max_prime); }

		//  0 and 1 are not primes
		bv.set(0, false);
		bv.set(1, false);

		for n in 2 .. (1 + (max_prime as f64).sqrt() as usize) {
			//  Adjust the frequency of log statements vaguely logarithmically.
			if n <  20_000 && n %  1_000 == 0
			|| n <  50_000 && n %  5_000 == 0
			|| n < 100_000 && n % 10_000 == 0 {
				println!("Calculating {}…", n);
			}
			//  If n is prime, mark all multiples as non-prime
			if bv[n] {
				if n < 50 {
					println!("Calculating {}…", n);
				}
				'inner:
				for i in n .. {
					let j = n * i;
					if j >= max_prime {
						break 'inner;
					}
					bv.set(j, false);
				}
			}
		}
		println!("Calculation complete!");

		bv
	};

	//  Count primes and non-primes.
	let (mut one, mut zero) = (0u64, 0u64);
	for n in primes.iter() {
		if n {
			one += 1;
		}
		else {
			zero += 1;
		}
	}
	println!("Counting complete!");
	println!("There are {} primes and {} non-primes below {}", one, zero, max_prime);

	let dim: usize = env::args()
		.nth(2)
		.unwrap_or("10".into())
		.parse()
		.unwrap_or(10);

	println!("The primes smaller than {} are:", dim * dim);
	let len = primes.len();
	'outer:
	for i in 0 .. dim {
		for j in 0 .. dim {
			let k = i * dim + j;
			if k >= len {
				println!();
				break 'outer;
			}
			if primes[k] {
				print!("{:>4} ", k);
			}
			else {
				print!("     ");
			}
		}
		println!();
	}
}

#[cfg(not(feature = "alloc"))]
fn main() {
	println!("This example only runs when an allocator is present");
}
