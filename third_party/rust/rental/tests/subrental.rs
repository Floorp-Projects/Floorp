#[macro_use]
extern crate rental;


pub struct Foo {
	pub i: i32,
}

pub struct Bar<'a> {
	pub foo: &'a Foo,
}

pub struct Qux<'a: 'b, 'b> {
	pub bar: &'b Bar<'a>,
}


impl Foo {
	pub fn borrow(&self) -> Bar { Bar { foo: self } }
	pub fn try_borrow<'a>(&'a self) -> Result<Bar<'a>, ()> { Ok(Bar { foo: self }) }
	pub fn fail_borrow<'a>(&'a self) -> Result<Bar<'a>, ()> { Err(()) }
}

impl<'a> Bar<'a> {
	pub fn borrow<'b>(&'b self) -> Qux<'a, 'b> { Qux { bar: self } }
	pub fn try_borrow<'b>(&'b self) -> Result<Qux<'a, 'b>, ()> { Ok(Qux { bar: self }) }
	pub fn fail_borrow<'b>(&'b self) -> Result<Qux<'a, 'b>, ()> { Err(()) }
}


rental! {
	pub mod rentals {
		use super::*;

		#[rental]
		pub struct Sub {
			foo: Box<Foo>,
			bar: Bar<'foo>,
		}

		#[rental]
		pub struct Rent {
			#[subrental = 2]
			sub: Box<Sub>,
			qux: Qux<'sub_0, 'sub_1>,
		}

		#[rental]
		pub struct BorrowSub<'f> {
			foo: &'f Foo,
			bar: Bar<'foo>,
		}

		#[rental]
		pub struct TailRent {
			foo: Box<Foo>,
			#[subrental = 2]
			sub: Box<BorrowSub<'foo>>,
			iref: &'sub_1 i32,
		}
	}
}


#[test]
fn new() {
	let foo = Foo { i: 5 };
	let sub = rentals::Sub::new(Box::new(foo), |foo| foo.borrow());
	let _ = rentals::Rent::new(Box::new(sub), |sub| sub.bar.borrow());

	let foo = Foo { i: 5 };
	let sub = rentals::Sub::new(Box::new(foo), |foo| foo.borrow());
	let rent = rentals::Rent::try_new(Box::new(sub), |sub| sub.bar.try_borrow());
	assert!(rent.is_ok());

	let foo = Foo { i: 5 };
	let sub = rentals::Sub::new(Box::new(foo), |foo| foo.borrow());
	let rent = rentals::Rent::try_new(Box::new(sub), |sub| sub.bar.fail_borrow());
	assert!(rent.is_err());
}


#[test]
fn read() {
	let foo = Foo { i: 5 };
	let sub = rentals::Sub::new(Box::new(foo), |foo| foo.borrow());
	let _ = rentals::Rent::new(Box::new(sub), |sub| sub.bar.borrow());
}
