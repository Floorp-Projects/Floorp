#[macro_use]
extern crate rental;


pub struct Foo {
	i: i32,
}

pub struct Bar<'a> {
	foo: &'a Foo,
}

pub struct Baz<'a: 'b, 'b> {
	bar: &'b Bar<'a>
}

pub struct Qux<'a: 'b, 'b: 'c, 'c> {
	baz: &'c Baz<'a, 'b>
}

pub struct Xyzzy<'a: 'b, 'b: 'c, 'c: 'd, 'd> {
	qux: &'d Qux<'a, 'b, 'c>
}


impl Foo {
	pub fn borrow<'a>(&'a self) -> Bar<'a> { Bar { foo: self } }
	pub fn try_borrow<'a>(&'a self) -> Result<Bar<'a>, ()> { Ok(Bar { foo: self }) }
	pub fn fail_borrow<'a>(&'a self) -> Result<Bar<'a>, ()> { Err(()) }
}

impl<'a> Bar<'a> {
	pub fn borrow<'b>(&'b self) -> Baz<'a, 'b> { Baz { bar: self } }
	pub fn try_borrow<'b>(&'b self) -> Result<Baz<'a, 'b>, ()> { Ok(Baz { bar: self }) }
	pub fn fail_borrow<'b>(&'b self) -> Result<Baz<'a, 'b>, ()> { Err(()) }
}

impl<'a: 'b, 'b> Baz<'a, 'b> {
	pub fn borrow<'c>(&'c self) -> Qux<'a, 'b, 'c> { Qux { baz: self } }
	pub fn try_borrow<'c>(&'c self) -> Result<Qux<'a, 'b, 'c>, ()> { Ok(Qux { baz: self }) }
	pub fn fail_borrow<'c>(&'c self) -> Result<Qux<'a, 'b, 'c>, ()> { Err(()) }
}

impl<'a: 'b, 'b: 'c, 'c> Qux<'a, 'b, 'c> {
	pub fn borrow<'d>(&'d self) -> Xyzzy<'a, 'b, 'c, 'd> { Xyzzy { qux: self } }
	pub fn try_borrow<'d>(&'d self) -> Result<Xyzzy<'a, 'b, 'c, 'd>, ()> { Ok(Xyzzy { qux: self }) }
	pub fn fail_borrow<'d>(&'d self) -> Result<Xyzzy<'a, 'b, 'c, 'd>, ()> { Err(()) }
}


rental! {
	mod rentals {
		use super::*;

		#[rental]
		pub struct ComplexRent {
			foo: Box<Foo>,
			bar: Box<Bar<'foo>>,
			baz: Box<Baz<'foo, 'bar>>,
			qux: Box<Qux<'foo, 'bar, 'baz>>,
			xyzzy: Xyzzy<'foo, 'bar, 'baz, 'qux>,
		}
	}
}


#[test]
fn new() {
	let foo = Foo { i: 5 };
	let _ = rentals::ComplexRent::new(
		Box::new(foo),
		|foo| Box::new(foo.borrow()),
		|bar, _| Box::new(bar.borrow()),
		|baz, _, _| Box::new(baz.borrow()),
		|qux, _, _, _| qux.borrow()
	);

	let foo = Foo { i: 5 };
	let cr = rentals::ComplexRent::try_new(
		Box::new(foo),
		|foo| foo.try_borrow().map(|bar| Box::new(bar)),
		|bar, _| bar.try_borrow().map(|baz| Box::new(baz)),
		|baz, _, _| baz.try_borrow().map(|qux| Box::new(qux)),
		|qux, _, _, _| qux.try_borrow()
	);
	assert!(cr.is_ok());

	let foo = Foo { i: 5 };
	let cr = rentals::ComplexRent::try_new(
		Box::new(foo),
		|foo| foo.try_borrow().map(|bar| Box::new(bar)),
		|bar, _| bar.try_borrow().map(|baz| Box::new(baz)),
		|baz, _, _| baz.try_borrow().map(|qux| Box::new(qux)),
		|qux, _, _, _| qux.fail_borrow()
	);
	assert!(cr.is_err());
}


#[test]
fn read() {
	let foo = Foo { i: 5 };
	let cr = rentals::ComplexRent::new(
		Box::new(foo),
		|foo| Box::new(foo.borrow()),
		|bar, _| Box::new(bar.borrow()),
		|baz, _, _| Box::new(baz.borrow()),
		|qux, _, _, _| qux.borrow()
	);
	let i = cr.rent(|xyzzy| xyzzy.qux.baz.bar.foo.i);
	assert_eq!(i, 5);

	let iref = cr.ref_rent(|xyzzy| &xyzzy.qux.baz.bar.foo.i);
	assert_eq!(*iref, 5);
}
