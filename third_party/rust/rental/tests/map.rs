#[macro_use]
extern crate rental;


pub struct Foo {
	i: i32,
}


rental! {
	mod rentals {
		use super::*;

		#[rental(map_suffix = "T")]
		pub struct SimpleRef<T: 'static> {
			foo: Box<Foo>,
			fr: &'foo T,
		}

		#[rental_mut(map_suffix = "T")]
		pub struct SimpleMut<T: 'static> {
			foo: Box<Foo>,
			fr: &'foo mut T,
		}
	}
}


#[test]
fn map() {
	let foo = Foo { i: 5 };
	let sr = rentals::SimpleRef::new(Box::new(foo), |foo| foo);
	let sr = sr.map(|fr| &fr.i);
	assert_eq!(sr.rent(|ir| **ir), 5);

	let foo = Foo { i: 12 };
	let sm = rentals::SimpleMut::new(Box::new(foo), |foo| foo);
	let sm = sm.map(|fr| &mut fr.i);
	assert_eq!(sm.rent(|ir| **ir), 12);
}


