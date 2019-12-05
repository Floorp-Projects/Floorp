#[macro_use]
extern crate rental;


rental! {
	pub mod rent_vec_slice {
		#[rental]
		pub struct OwnedSlice {
			buffer: Vec<u8>,
			slice: &'buffer [u8],
		}

		#[rental_mut]
		pub struct OwnedMutSlice {
			buffer: Vec<u8>,
			slice: &'buffer mut [u8],
		}
	}
}


#[test]
fn new() {
	let vec = vec![1, 2, 3];
	let _ = rent_vec_slice::OwnedSlice::new(vec, |slice| slice);
}


#[test]
fn read() {
	let vec = vec![1, 2, 3];
	let rvec = rent_vec_slice::OwnedSlice::new(vec, |slice| slice);

	assert_eq!(rvec.rent(|slice| slice[1]), 2);
	assert_eq!(rvec.rent(|slice| slice[1]), rvec.rent(|slice| slice[1]));
}


#[test]
fn write() {
	let vec = vec![1, 2, 3];
	let mut rvec = rent_vec_slice::OwnedMutSlice::new(vec, |slice| slice);

	rvec.rent_mut(|slice| slice[1] = 4);
	assert_eq!(rvec.rent(|slice| slice[1]), 4);
	assert_eq!(rvec.rent(|slice| slice[1]), rvec.rent(|slice| slice[1]));
}
