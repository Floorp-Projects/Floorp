#[macro_use]
extern crate rental;


//use std::marker::PhantomData;


pub struct Foo {
	i: i32,
}


//pub struct Invariant<'a> {
//	iref: &'a i32,
//	inv: PhantomData<&'a mut &'a ()>,
//}


rental! {
	mod rentals {
		use super::*;

		#[rental(covariant)]
		pub struct SimpleRef {
			foo: Box<Foo>,
			iref: &'foo i32,
		}

		#[rental_mut(covariant)]
		pub struct SimpleMut {
			foo: Box<Foo>,
			iref: &'foo mut i32,
		}

//		#[rental(covariant)]
//		pub struct ShouldBreak {
//			foo: Box<Foo>,
//			inv: Invariant<'foo>,
//		}
	}
}


#[test]
fn borrow() {
	let foo = Foo { i: 5 };
	let fr = rentals::SimpleRef::new(Box::new(foo), |foo| &foo.i);
	let b = fr.all();
	assert_eq!(**b.iref, 5);
}


