#[macro_use]
extern crate rental;


#[derive(Debug)]
pub struct Foo {
	i: i32,
}


rental! {
	mod rentals {
		use super::*;

		#[rental(debug, deref_suffix)]
		pub struct SimpleRef {
			foo: Box<Foo>,
			iref: &'foo i32,
		}

		#[rental_mut(debug, deref_suffix)]
		pub struct SimpleMut {
			foo: Box<Foo>,
			iref: &'foo mut i32,
		}
	}
}


#[test]
fn print() {
	let foo = Foo { i: 5 };
	let sr = rentals::SimpleRef::new(Box::new(foo), |foo| &foo.i);
	println!("{:?}", sr);

	let foo = Foo { i: 5 };
	let sm = rentals::SimpleMut::new(Box::new(foo), |foo| &mut foo.i);
	println!("{:?}", sm);
}
