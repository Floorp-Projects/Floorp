#[macro_use]
extern crate rental;


rental! {
	pub mod rent_string {
		#[rental(deref_suffix)]
		pub struct OwnedStr {
			buffer: String,
			slice: &'buffer str,
			slice_2: &'slice str,
		}
	}
}


#[test]
fn new() {
	let buf = "Hello, World!".to_string();
	let _ = rent_string::OwnedStr::new(buf, |slice| slice, |slice, _| slice);
}


#[test]
fn read() {
	let buf = "Hello, World!".to_string();
	let rbuf = rent_string::OwnedStr::new(buf, |slice| slice, |slice, _| slice);

	assert_eq!(&*rbuf, "Hello, World!");
}
