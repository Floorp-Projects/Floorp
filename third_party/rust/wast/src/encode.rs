pub(crate) trait Encode {
    fn encode(&self, e: &mut Vec<u8>);
}

impl<T: Encode + ?Sized> Encode for &'_ T {
    fn encode(&self, e: &mut Vec<u8>) {
        T::encode(self, e)
    }
}

impl<T: Encode + ?Sized> Encode for Box<T> {
    fn encode(&self, e: &mut Vec<u8>) {
        T::encode(self, e)
    }
}

impl<T: Encode> Encode for [T] {
    fn encode(&self, e: &mut Vec<u8>) {
        self.len().encode(e);
        for item in self {
            item.encode(e);
        }
    }
}

impl<T: Encode> Encode for Vec<T> {
    fn encode(&self, e: &mut Vec<u8>) {
        <[T]>::encode(self, e)
    }
}

impl Encode for str {
    fn encode(&self, e: &mut Vec<u8>) {
        self.len().encode(e);
        e.extend_from_slice(self.as_bytes());
    }
}

impl Encode for usize {
    fn encode(&self, e: &mut Vec<u8>) {
        assert!(*self <= u32::max_value() as usize);
        (*self as u32).encode(e)
    }
}

impl Encode for u8 {
    fn encode(&self, e: &mut Vec<u8>) {
        e.push(*self);
    }
}

impl Encode for u32 {
    fn encode(&self, e: &mut Vec<u8>) {
        leb128::write::unsigned(e, (*self).into()).unwrap();
    }
}

impl Encode for i32 {
    fn encode(&self, e: &mut Vec<u8>) {
        leb128::write::signed(e, (*self).into()).unwrap();
    }
}

impl Encode for u64 {
    fn encode(&self, e: &mut Vec<u8>) {
        leb128::write::unsigned(e, *self).unwrap();
    }
}

impl Encode for i64 {
    fn encode(&self, e: &mut Vec<u8>) {
        leb128::write::signed(e, *self).unwrap();
    }
}

impl<T: Encode, U: Encode> Encode for (T, U) {
    fn encode(&self, e: &mut Vec<u8>) {
        self.0.encode(e);
        self.1.encode(e);
    }
}

impl<T: Encode, U: Encode, V: Encode> Encode for (T, U, V) {
    fn encode(&self, e: &mut Vec<u8>) {
        self.0.encode(e);
        self.1.encode(e);
        self.2.encode(e);
    }
}
