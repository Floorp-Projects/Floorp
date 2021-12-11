#[macro_use]
extern crate rental;


pub struct Foo {
	i: i32,
}

pub struct Bar<'i> {
	iref: &'i i32,
	misc: i32,
}

impl <'i> Clone for Bar<'i> {
	fn clone (&self) -> Self {
		Bar{
			iref: Clone::clone(&self.iref),
			misc: Clone::clone(&self.misc),
		}
	}
}


rental! {
	mod rentals {
		use super::*;
		use std::sync::Arc;

		#[rental(clone)]
		pub struct FooClone {
			foo: Arc<Foo>,
			fr: Bar<'foo>,
		}
	}
}


#[test]
fn clone() {
	use std::sync::Arc;

	let foo = Foo { i: 5 };
	let rf = rentals::FooClone::new(Arc::new(foo), |foo| Bar{ iref: &foo.i, misc: 12 });
	assert_eq!(5, rf.rent(|f| *f.iref));

	let rfc = rf.clone();
	assert_eq!(5, rfc.rent(|f| *f.iref));
}


