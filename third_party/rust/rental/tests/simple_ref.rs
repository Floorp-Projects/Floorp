#[macro_use]
extern crate rental;


pub struct Foo {
	i: i32,
}

impl Foo {
	fn try_borrow(&self) -> Result<&i32, ()> { Ok(&self.i) }
	fn fail_borrow(&self) -> Result<&i32, ()> { Err(()) }
}

pub struct FooRef<'i> {
	iref: &'i i32,
	misc: i32,
}


impl<'i> ::std::ops::Deref for FooRef<'i> {
	type Target = i32;

	fn deref(&self) -> &i32 { self.iref }
}


rental! {
	mod rentals {
		use super::*;

		#[rental]
		pub struct SimpleRef {
			foo: Box<Foo>,
			fr: FooRef<'foo>,
		}
	}
}


#[test]
fn new() {
	let foo = Foo { i: 5 };
	let _ = rentals::SimpleRef::new(Box::new(foo), |foo| FooRef{ iref: &foo.i, misc: 12 });

	let foo = Foo { i: 5 };
	let sr: rental::RentalResult<rentals::SimpleRef, (), _> = rentals::SimpleRef::try_new(Box::new(foo), |foo| Ok(FooRef{ iref: foo.try_borrow()?, misc: 12 }));
	assert!(sr.is_ok());

	let foo = Foo { i: 5 };
	let sr: rental::RentalResult<rentals::SimpleRef, (), _> = rentals::SimpleRef::try_new(Box::new(foo), |foo| Ok(FooRef{ iref: foo.fail_borrow()?, misc: 12 }));
	assert!(sr.is_err());
}


#[test]
fn read() {
	let foo = Foo { i: 5 };

	let mut sr = rentals::SimpleRef::new(Box::new(foo), |foo| FooRef{ iref: &foo.i, misc: 12 });

	{
		let i: i32 = sr.rent(|iref| **iref);
		assert_eq!(i, 5);
	}

	{
		let iref: &i32 = sr.ref_rent(|fr| fr.iref);
		assert_eq!(*iref, 5);
		let iref: Option<&i32> = sr.maybe_ref_rent(|fr| Some(fr.iref));
		assert_eq!(iref, Some(&5));
		let iref: Result<&i32, ()> = sr.try_ref_rent(|fr| Ok(fr.iref));
		assert_eq!(iref, Ok(&5));
	}

	{
		assert_eq!(sr.head().i, 5);
		assert_eq!(sr.rent_all(|borrows| borrows.foo.i), 5);
		assert_eq!(sr.rent_all_mut(|borrows| borrows.foo.i), 5);
	}

	{
		let iref: Option<&i32> = sr.maybe_ref_rent_all(|borrows| Some(borrows.fr.iref));
		assert_eq!(iref, Some(&5));
		let iref: Result<&i32, ()> = sr.try_ref_rent_all(|borrows| Ok(borrows.fr.iref));
		assert_eq!(iref, Ok(&5));
	}

	{
		let iref: &mut i32 = sr.ref_rent_all_mut(|borrows| &mut borrows.fr.misc);
		*iref = 57;
		assert_eq!(*iref, 57);
	}

	{
		let iref: Option<&mut i32> = sr.maybe_ref_rent_all_mut(|borrows| Some(&mut borrows.fr.misc));
		assert_eq!(iref, Some(&mut 57));
	}

	{
		let iref: Result<&mut i32, ()> = sr.try_ref_rent_all_mut(|borrows| Ok(&mut borrows.fr.misc));
		assert_eq!(iref, Ok(&mut 57));
	}
}
