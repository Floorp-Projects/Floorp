#![allow(dead_code)]

#[macro_use]
extern crate rental;

use std::cell::RefCell;
use std::rc::Rc;


pub struct Foo {
	i: i32,
	d: Rc<RefCell<String>>,
}

pub struct Bar<'a> {
	foo: &'a Foo,
	d: Rc<RefCell<String>>,
}

pub struct Baz<'a: 'b, 'b> {
	bar: &'b Bar<'a>,
	d: Rc<RefCell<String>>,
}

pub struct Qux<'a: 'b, 'b: 'c, 'c> {
	baz: &'c Baz<'a, 'b>,
	d: Rc<RefCell<String>>,
}

pub struct Xyzzy<'a: 'b, 'b: 'c, 'c: 'd, 'd> {
	qux: &'d Qux<'a, 'b, 'c>,
	d: Rc<RefCell<String>>,
}


impl Foo {
	pub fn borrow<'a>(&'a self) -> Bar<'a> { Bar { foo: self, d: self.d.clone() } }
}

impl Drop for Foo {
	fn drop(&mut self) {
		self.d.borrow_mut().push_str("Foo");
	}
}

impl<'a> Bar<'a> {
	pub fn borrow<'b>(&'b self) -> Baz<'a, 'b> { Baz { bar: self, d: self.d.clone() } }
}

impl<'a> Drop for Bar<'a> {
	fn drop(&mut self) {
		self.d.borrow_mut().push_str("Bar");
	}
}

impl<'a: 'b, 'b> Baz<'a, 'b> {
	pub fn borrow<'c>(&'c self) -> Qux<'a, 'b, 'c> { Qux { baz: self, d: self.d.clone() } }
}

impl<'a: 'b, 'b> Drop for Baz<'a, 'b> {
	fn drop(&mut self) {
		self.d.borrow_mut().push_str("Baz");
	}
}

impl<'a: 'b, 'b: 'c, 'c> Qux<'a, 'b, 'c> {
	pub fn borrow<'d>(&'d self) -> Xyzzy<'a, 'b, 'c, 'd> { Xyzzy { qux: self, d: self.d.clone() } }
}

impl<'a: 'b, 'b: 'c, 'c> Drop for Qux<'a, 'b, 'c> {
	fn drop(&mut self) {
		self.d.borrow_mut().push_str("Qux");
	}
}

impl<'a: 'b, 'b: 'c, 'c: 'd, 'd> Drop for Xyzzy<'a, 'b, 'c, 'd> {
	fn drop(&mut self) {
		self.d.borrow_mut().push_str("Xyzzy");
	}
}


rental! {
	pub mod rentals {
		use super::*;

		#[rental]
		pub struct DropTestRent {
			foo: Box<Foo>,
			bar: Box<Bar<'foo>>,
			baz: Box<Baz<'foo, 'bar>>,
			qux: Box<Qux<'foo, 'bar, 'baz>>,
			xyzzy: Xyzzy<'foo, 'bar, 'baz, 'qux>,
		}
	}
}


#[test]
fn drop_order() {
	let d = Rc::new(RefCell::new(String::new()));
	{
		let foo = Foo { i: 5, d: d.clone() };
		let _ = rentals::DropTestRent::new(
			Box::new(foo),
			|foo| Box::new(foo.borrow()),
			|bar, _| Box::new(bar.borrow()),
			|baz, _, _| Box::new(baz.borrow()),
			|qux, _, _, _| qux.borrow()
		);
	}
	assert_eq!(*d.borrow(), "XyzzyQuxBazBarFoo");

	let d = Rc::new(RefCell::new(String::new()));
	{
		let foo = Foo { i: 5, d: d.clone() };
		let r = rentals::DropTestRent::new(
			Box::new(foo),
			|foo| Box::new(foo.borrow()),
			|bar, _| Box::new(bar.borrow()),
			|baz, _, _| Box::new(baz.borrow()),
			|qux, _, _, _| qux.borrow()
		);

		let _head = r.into_head();
	}
	assert_eq!(*d.borrow(), "XyzzyQuxBazBarFoo");
}

