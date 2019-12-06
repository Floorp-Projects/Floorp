use alloc::vec::Vec;
use super::invoke::{Invoke, Identity};
use crate::elements;

pub struct ValueTypeBuilder<F=Identity> {
	callback: F,
}

impl<F> ValueTypeBuilder<F> where F: Invoke<elements::ValueType> {
	pub fn with_callback(callback: F) -> Self {
		ValueTypeBuilder { callback: callback }
	}

	pub fn i32(self) -> F::Result {
		self.callback.invoke(elements::ValueType::I32)
	}

	pub fn i64(self) -> F::Result {
		self.callback.invoke(elements::ValueType::I64)
	}

	pub fn f32(self) -> F::Result {
		self.callback.invoke(elements::ValueType::F32)
	}

	pub fn f64(self) -> F::Result {
		self.callback.invoke(elements::ValueType::F64)
	}
}

pub struct OptionalValueTypeBuilder<F=Identity> {
	callback: F,
}

impl<F> OptionalValueTypeBuilder<F> where F: Invoke<Option<elements::ValueType>> {
	pub fn with_callback(callback: F) -> Self {
		OptionalValueTypeBuilder { callback: callback }
	}

	pub fn i32(self) -> F::Result {
		self.callback.invoke(Some(elements::ValueType::I32))
	}

	pub fn i64(self) -> F::Result {
		self.callback.invoke(Some(elements::ValueType::I64))
	}

	pub fn f32(self) -> F::Result {
		self.callback.invoke(Some(elements::ValueType::F32))
	}

	pub fn f64(self) -> F::Result {
		self.callback.invoke(Some(elements::ValueType::F64))
	}
}

pub struct ValueTypesBuilder<F=Identity> {
	callback: F,
	value_types: Vec<elements::ValueType>,
}

impl<F> ValueTypesBuilder<F> where F: Invoke<Vec<elements::ValueType>> {
	pub fn with_callback(callback: F) -> Self {
		ValueTypesBuilder {
			callback: callback,
			value_types: Vec::new(),
		}
	}

	pub fn i32(mut self) -> Self {
		self.value_types.push(elements::ValueType::I32);
		self
	}

	pub fn i64(mut self) -> Self {
		self.value_types.push(elements::ValueType::I64);
		self
	}

	pub fn f32(mut self) -> Self {
		self.value_types.push(elements::ValueType::F32);
		self
	}

	pub fn f64(mut self) -> Self {
		self.value_types.push(elements::ValueType::F64);
		self
	}

	pub fn build(self) -> F::Result {
		self.callback.invoke(self.value_types)
	}
}
