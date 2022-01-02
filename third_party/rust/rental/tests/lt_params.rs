#[macro_use]
extern crate rental;


pub struct Foo<'a> {
	i: &'a i32,
}

impl<'a> Foo<'a> {
	fn borrow(&self) -> &i32 { self.i }
	fn try_borrow(&self) -> Result<&i32, ()> { Ok(self.i) }
	fn fail_borrow(&self) -> Result<&i32, ()> { Err(()) }
}


rental! {
	mod rentals {
		use super::*;

		#[rental]
		pub struct LtParam<'a> {
			foo: Box<Foo<'a>>,
			iref: &'foo i32,
		}
	}
}


#[test]
fn new() {
	let i = 5;

	let foo = Foo { i: &i };
	let _ = rentals::LtParam::new(Box::new(foo), |foo| foo.borrow());

	let foo = Foo { i: &i };
	let sr = rentals::LtParam::try_new(Box::new(foo), |foo| foo.try_borrow());
	assert!(sr.is_ok());

	let foo = Foo { i: &i };
	let sr = rentals::LtParam::try_new(Box::new(foo), |foo| foo.fail_borrow());
	assert!(sr.is_err());
}


#[test]
fn read() {
	let i = 5;

	let foo = Foo { i: &i };

	let mut sr = rentals::LtParam::new(Box::new(foo), |foo| foo.borrow());

	{
		let i: i32 = sr.rent(|iref| **iref);
		assert_eq!(i, 5);
	}

	{
		let iref: &i32 = sr.ref_rent(|iref| *iref);
		assert_eq!(*iref, 5);
	}

	assert_eq!(sr.rent_all_mut(|borrows| *borrows.foo.i), 5);
}
