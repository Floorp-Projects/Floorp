#[macro_use]
extern crate rental;


pub struct Foo<T: 'static> {
	t: T,
}

impl<T: 'static> Foo<T> {
	fn try_borrow(&self) -> Result<&T, ()> { Ok(&self.t) }
	fn fail_borrow(&self) -> Result<&T, ()> { Err(()) }
}


rental! {
	mod rentals {
		type FooAlias<T> = super::Foo<T>;

		#[rental]
		pub struct SimpleRef<T: 'static> {
			foo: Box<FooAlias<T>>,
			tref: &'foo T,
		}
	}
}


#[test]
fn new() {
	let foo = Foo { t: 5 };
	let _ = rentals::SimpleRef::new(Box::new(foo), |foo| &foo.t);

	let foo = Foo { t: 5 };
	let sr = rentals::SimpleRef::try_new(Box::new(foo), |foo| foo.try_borrow());
	assert!(sr.is_ok());

	let foo = Foo { t: 5 };
	let sr = rentals::SimpleRef::try_new(Box::new(foo), |foo| foo.fail_borrow());
	assert!(sr.is_err());
}


#[test]
fn read() {
	let foo = Foo { t: 5 };

	let sr = rentals::SimpleRef::new(Box::new(foo), |foo| &foo.t);
	let t: i32 = sr.rent(|tref| **tref);
	assert_eq!(t, 5);

	let tref: &i32 = sr.ref_rent(|tref| *tref);
	assert_eq!(*tref, 5);
}
