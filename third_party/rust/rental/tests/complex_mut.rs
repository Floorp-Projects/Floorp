#[macro_use]
extern crate rental;


pub struct Foo {
	i: i32,
}

pub struct Bar<'a> {
	foo: &'a mut Foo,
}

pub struct Baz<'a: 'b, 'b> {
	bar: &'b mut Bar<'a>
}

pub struct Qux<'a: 'b, 'b: 'c, 'c> {
	baz: &'c mut Baz<'a, 'b>
}

pub struct Xyzzy<'a: 'b, 'b: 'c, 'c: 'd, 'd> {
	qux: &'d mut Qux<'a, 'b, 'c>
}


impl Foo {
	pub fn borrow_mut<'a>(&'a mut self) -> Bar<'a> { Bar { foo: self } }
	pub fn try_borrow_mut<'a>(&'a mut self) -> Result<Bar<'a>, ()> { Ok(Bar { foo: self }) }
	pub fn fail_borrow_mut<'a>(&'a mut self) -> Result<Bar<'a>, ()> { Err(()) }
}

impl<'a> Bar<'a> {
	pub fn borrow_mut<'b>(&'b mut self) -> Baz<'a, 'b> { Baz { bar: self } }
	pub fn try_borrow_mut<'b>(&'b mut self) -> Result<Baz<'a, 'b>, ()> { Ok(Baz { bar: self }) }
	pub fn fail_borrow_mut<'b>(&'b mut self) -> Result<Baz<'a, 'b>, ()> { Err(()) }
}

impl<'a: 'b, 'b> Baz<'a, 'b> {
	pub fn borrow_mut<'c>(&'c mut self) -> Qux<'a, 'b, 'c> { Qux { baz: self } }
	pub fn try_borrow_mut<'c>(&'c mut self) -> Result<Qux<'a, 'b, 'c>, ()> { Ok(Qux { baz: self }) }
	pub fn fail_borrow_mut<'c>(&'c mut self) -> Result<Qux<'a, 'b, 'c>, ()> { Err(()) }
}

impl<'a: 'b, 'b: 'c, 'c> Qux<'a, 'b, 'c> {
	pub fn borrow_mut<'d>(&'d mut self) -> Xyzzy<'a, 'b, 'c, 'd> { Xyzzy { qux: self } }
	pub fn try_borrow_mut<'d>(&'d mut self) -> Result<Xyzzy<'a, 'b, 'c, 'd>, ()> { Ok(Xyzzy { qux: self }) }
	pub fn fail_borrow_mut<'d>(&'d mut self) -> Result<Xyzzy<'a, 'b, 'c, 'd>, ()> { Err(()) }
}


rental! {
	mod rentals {
		use super::*;

		#[rental_mut]
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
		|foo| Box::new(foo.borrow_mut()),
		|bar| Box::new(bar.borrow_mut()),
		|baz| Box::new(baz.borrow_mut()),
		|qux| qux.borrow_mut()
	);

	let foo = Foo { i: 5 };
	let cm = rentals::ComplexRent::try_new(
		Box::new(foo),
		|foo| foo.try_borrow_mut().map(|bar| Box::new(bar)),
		|bar| bar.try_borrow_mut().map(|baz| Box::new(baz)),
		|baz| baz.try_borrow_mut().map(|qux| Box::new(qux)),
		|qux| qux.try_borrow_mut()
	);
	assert!(cm.is_ok());

	let foo = Foo { i: 5 };
	let cm = rentals::ComplexRent::try_new(
		Box::new(foo),
		|foo| foo.try_borrow_mut().map(|bar| Box::new(bar)),
		|bar| bar.try_borrow_mut().map(|baz| Box::new(baz)),
		|baz| baz.try_borrow_mut().map(|qux| Box::new(qux)),
		|qux| qux.fail_borrow_mut()
	);
	assert!(cm.is_err());
}


#[test]
fn read() {
	let foo = Foo { i: 5 };
	let cm = rentals::ComplexRent::new(
		Box::new(foo),
		|foo| Box::new(foo.borrow_mut()),
		|bar| Box::new(bar.borrow_mut()),
		|baz| Box::new(baz.borrow_mut()),
		|qux| qux.borrow_mut()
	);
	let i = cm.rent(|xyzzy| xyzzy.qux.baz.bar.foo.i);
	assert_eq!(i, 5);

	let iref = cm.ref_rent(|xyzzy| &xyzzy.qux.baz.bar.foo.i);
	assert_eq!(*iref, 5);
}


#[test]
fn write() {
	let foo = Foo { i: 5 };
	let mut cm = rentals::ComplexRent::new(
		Box::new(foo),
		|foo| Box::new(foo.borrow_mut()),
		|bar| Box::new(bar.borrow_mut()),
		|baz| Box::new(baz.borrow_mut()),
		|qux| qux.borrow_mut()
	);

	{
		let iref: &mut i32 = cm.ref_rent_mut(|xyzzy| &mut xyzzy.qux.baz.bar.foo.i);
		*iref = 12;
	}

	let i = cm.rent(|xyzzy| xyzzy.qux.baz.bar.foo.i);
	assert_eq!(i, 12);
}
