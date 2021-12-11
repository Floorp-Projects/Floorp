#[macro_use]
extern crate rental;


pub trait MyTrait { }


pub struct MyStruct { }


impl MyTrait for MyStruct { }


rental! {
    pub mod rentals {
		use ::MyTrait;

		#[rental]
		pub struct RentTrait {
			my_trait: Box<MyTrait + 'static>,
			my_suffix: &'my_trait (MyTrait + 'static),
		}
	}
}


#[test]
fn new() {
	let _tr = rentals::RentTrait::new(
		Box::new(MyStruct{}),
		|t| &*t,
	);
}
