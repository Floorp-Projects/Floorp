// basically duplicating the unstable `std::boxed::FnBox`
pub trait FnBox<Arguments, Result> {
	fn call(self: Box<Self>, args: Arguments) -> Result;
}

impl_trait_n_args!();
impl_trait_n_args!(a1: A1);
impl_trait_n_args!(a1: A1, a2: A2);
impl_trait_n_args!(a1: A1, a2: A2, a3: A3);
impl_trait_n_args!(a1: A1, a2: A2, a3: A3, a4: A4);
impl_trait_n_args!(a1: A1, a2: A2, a3: A3, a4: A4, a5: A5);
impl_trait_n_args!(a1: A1, a2: A2, a3: A3, a4: A4, a5: A5, a6: A6);
impl_trait_n_args!(a1: A1, a2: A2, a3: A3, a4: A4, a5: A5, a6: A6, a7: A7);
impl_trait_n_args!(a1: A1, a2: A2, a3: A3, a4: A4, a5: A5, a6: A6, a7: A7, a8: A8);
impl_trait_n_args!(a1: A1, a2: A2, a3: A3, a4: A4, a5: A5, a6: A6, a7: A7, a8: A8, a9: A9);
impl_trait_n_args!(a1: A1, a2: A2, a3: A3, a4: A4, a5: A5, a6: A6, a7: A7, a8: A8, a9: A9, a10: A10);
