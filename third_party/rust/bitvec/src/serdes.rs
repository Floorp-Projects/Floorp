/*! `serde`-powered de/serialization

!*/

#![cfg(all(feature = "serdes"))]

use crate::{
	bits::Bits,
	cursor::Cursor,
	pointer::BitPtr,
	slice::BitSlice,
};

#[cfg(feature = "alloc")]
use crate::{
	boxed::BitBox,
	vec::BitVec,
};

use core::{
	cmp,
	fmt::{
		self,
		Formatter,
	},
	marker::PhantomData,
	mem,
};

use serde::{
	Serialize,
	ser::{
		Serializer,
		SerializeStruct,
	},
};

#[cfg(feature = "alloc")]
use serde::{
	Deserialize,
	de::{
		self,
		Deserializer,
		MapAccess,
		SeqAccess,
		Visitor,
	},
};

/// A Serde visitor to pull `BitBox` data out of a serialized stream
#[cfg(feature = "alloc")]
#[derive(Clone, Copy, Default, Debug)]
pub struct BitBoxVisitor<'de, C, T>
where C: Cursor, T: Bits + Deserialize<'de> {
	_cursor: PhantomData<C>,
	_storage: PhantomData<&'de T>,
}

#[cfg(feature = "alloc")]
impl<'de, C, T> BitBoxVisitor<'de, C, T>
where C: Cursor, T: Bits + Deserialize<'de> {
	fn new() -> Self {
		BitBoxVisitor { _cursor: PhantomData, _storage: PhantomData }
	}
}

#[cfg(feature = "alloc")]
impl<'de, C, T> Visitor<'de> for BitBoxVisitor<'de, C, T>
where C: Cursor, T: Bits + Deserialize<'de> {
	type Value = BitBox<C, T>;

	fn expecting(&self, fmt: &mut Formatter) -> fmt::Result {
		fmt.write_str("A BitSet data series")
	}

	/// Visit a sequence of anonymous data elements. These must be in the order
	/// `usize', `u8`, `u8`, `[T]`.
	fn visit_seq<V>(self, mut seq: V) -> Result<Self::Value, V::Error>
	where V: SeqAccess<'de> {
		let elts: usize = seq.next_element()?
			.ok_or_else(|| de::Error::invalid_length(0, &self))?;
		let head: u8 = seq.next_element()?
			.ok_or_else(|| de::Error::invalid_length(1, &self))?;
		let tail: u8 = seq.next_element()?
			.ok_or_else(|| de::Error::invalid_length(2, &self))?;
		let data: Box<[T]> = seq.next_element()?
			.ok_or_else(|| de::Error::invalid_length(3, &self))?;

		let bitptr = BitPtr::new(data.as_ptr(), cmp::min(elts, data.len()), head, tail);
		mem::forget(data);
		Ok(unsafe { BitBox::from_raw(bitptr) })
	}

	/// Visit a map of named data elements. These may be in any order, and must
	/// be the pairs `elts: usize`, `head: u8`, `tail: u8`, and `data: [T]`.
	fn visit_map<V>(self, mut map: V) -> Result<Self::Value, V::Error>
	where V: MapAccess<'de> {
		let mut elts: Option<usize> = None;
		let mut head: Option<u8> = None;
		let mut tail: Option<u8> = None;
		let mut data: Option<Box<[T]>> = None;

		while let Some(key) = map.next_key()? {
			match key {
				"elts" => if elts.replace(map.next_value()?).is_some() {
					return Err(de::Error::duplicate_field("elts"));
				},
				"head" => if head.replace(map.next_value()?).is_some() {
					return Err(de::Error::duplicate_field("head"));
				},
				"tail" => if tail.replace(map.next_value()?).is_some() {
					return Err(de::Error::duplicate_field("tail"));
				},
				"data" => if data.replace(map.next_value()?).is_some() {
					return Err(de::Error::duplicate_field("data"));
				},
				f => return Err(de::Error::unknown_field(
					f, &["elts", "head", "tail", "data"]
				)),
			}
		}
		let elts = elts.ok_or_else(|| de::Error::missing_field("elts"))?;
		let head = head.ok_or_else(|| de::Error::missing_field("head"))?;
		let tail = tail.ok_or_else(|| de::Error::missing_field("tail"))?;
		let data = data.ok_or_else(|| de::Error::missing_field("data"))?;

		let bitptr = BitPtr::new(
			data.as_ptr(),
			cmp::min(elts as usize, data.len()),
			head,
			tail,
		);
		mem::forget(data);
		Ok(unsafe { BitBox::from_raw(bitptr) })
	}
}

#[cfg(feature = "alloc")]
impl<'de, C, T> Deserialize<'de> for BitBox<C, T>
where C: Cursor, T: 'de + Bits + Deserialize<'de> {
	fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
	where D: Deserializer<'de> {
		deserializer.deserialize_struct("BitSet", &[
			"elts", "head", "tail", "data",
		], BitBoxVisitor::new())
	}
}

#[cfg(feature = "alloc")]
impl<'de, C, T> Deserialize<'de> for BitVec<C, T>
where C: Cursor, T: 'de + Bits + Deserialize<'de> {
	fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
	where D: Deserializer<'de> {
		BitBox::deserialize(deserializer).map(Into::into)
	}
}

impl<C, T> Serialize for BitSlice<C, T>
where C: Cursor, T: Bits + Serialize {
	fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
	where S: Serializer {
		let (e, h, t) = self.bitptr().region_data();
		let mut state = serializer.serialize_struct("BitSet", 4)?;

		state.serialize_field("elts", &(e as u64))?;
		state.serialize_field("head", &*h)?;
		state.serialize_field("tail", &*t)?;
		state.serialize_field("data", self.as_ref())?;

		state.end()
	}
}

#[cfg(feature = "alloc")]
impl<C, T> Serialize for BitBox<C, T>
where C: Cursor, T: Bits + Serialize {
	fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
	where S: Serializer {
		BitSlice::serialize(&*self, serializer)
	}
}

#[cfg(feature = "alloc")]
impl<C, T> Serialize for BitVec<C, T>
where C: Cursor, T: Bits + Serialize {
	fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
	where S: Serializer {
		BitSlice::serialize(&*self, serializer)
	}
}

#[cfg(test)]
mod tests {
	use crate::prelude::*;
	use serde_test::{
		Token,
		assert_de_tokens,
		assert_ser_tokens,
		assert_tokens,
	};

	macro_rules! bvtok {
		( s $elts:expr, $head:expr, $tail:expr, $len:expr, $ty:ident $( , $data:expr )* ) => {
			&[
				Token::Struct { name: "BitSet", len: 4, },
				Token::Str("elts"), Token::U64( $elts ),
				Token::Str("head"), Token::U8( $head ),
				Token::Str("tail"), Token::U8( $tail ),
				Token::Str("data"), Token::Seq { len: Some( $len ) },
				$( Token:: $ty ( $data ), )*
				Token::SeqEnd,
				Token::StructEnd,
			]
		};
		( d $elts:expr, $head:expr, $tail:expr, $len:expr, $ty:ident $( , $data:expr )* ) => {
			&[
				Token::Struct { name: "BitSet", len: 4, },
				Token::BorrowedStr("elts"), Token::U64( $elts ),
				Token::BorrowedStr("head"), Token::U8( $head ),
				Token::BorrowedStr("tail"), Token::U8( $tail ),
				Token::BorrowedStr("data"), Token::Seq { len: Some( $len ) },
				$( Token:: $ty ( $data ), )*
				Token::SeqEnd,
				Token::StructEnd,
			]
		};
	}

	#[test]
	fn empty() {
		let slice = BitSlice::<BigEndian, u8>::empty();
		//  TODO(myrrlyn): Refactor BitPtr impl so the empty slice is 0/0
		assert_ser_tokens(&slice, bvtok![s 0, 0, 8, 0, U8]);
		assert_de_tokens(&bitvec![], bvtok![ d 0, 0, 8, 0, U8 ]);
	}

	#[test]
	fn small() {
		let bv = bitvec![1; 5];
		eprintln!("made vec");
		let bs = &bv[1 ..];
		assert_ser_tokens(&bs, bvtok![s 1, 1, 5, 1, U8, 0b1111_1000]);

		let bv = bitvec![LittleEndian, u16; 1; 12];
		assert_ser_tokens(&bv, bvtok![s 1, 0, 12, 1, U16, 0b00001111_11111111]);

		let bb: BitBox<_, _> = bitvec![LittleEndian, u32; 1; 10].into();
		assert_ser_tokens(&bb, bvtok![s 1, 0, 10, 1, U32, 0x00_00_03_FF]);
	}

	#[test]
	fn wide() {
		let src: &[u8] = &[0, !0];
		let bs: &BitSlice = src.into();
		assert_ser_tokens(&(&bs[1 .. 15]), bvtok![s 2, 1, 7, 2, U8, 0, !0]);
	}

	#[test]
	fn deser() {
		let bv = bitvec![0, 1, 1, 0, 1, 0];
		assert_de_tokens(&bv, bvtok![d 1, 0, 6, 1, U8, 0b0110_1000]);
		//  test that the bits outside the bits domain don't matter in deser
		assert_de_tokens(&bv, bvtok![d 1, 0, 6, 1, U8, 0b0110_1010]);
		assert_de_tokens(&bv, bvtok![d 1, 0, 6, 1, U8, 0b0110_1001]);
	}
}
