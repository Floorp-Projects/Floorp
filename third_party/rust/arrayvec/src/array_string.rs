use std::borrow::Borrow;
use std::cmp;
use std::fmt;
use std::hash::{Hash, Hasher};
use std::mem;
use std::ptr;
use std::ops::{Deref, DerefMut};
use std::str;
use std::str::Utf8Error;
use std::slice;

use array::{Array, ArrayExt};
use array::Index;
use CapacityError;
use char::encode_utf8;

#[cfg(feature="serde-1")]
use serde::{Serialize, Deserialize, Serializer, Deserializer};

/// A string with a fixed capacity.
///
/// The `ArrayString` is a string backed by a fixed size array. It keeps track
/// of its length.
///
/// The string is a contiguous value that you can store directly on the stack
/// if needed.
#[derive(Copy)]
pub struct ArrayString<A: Array<Item=u8>> {
    // FIXME: Use Copyable union for xs when we can
    xs: A,
    len: A::Index,
}

impl<A: Array<Item=u8>> Default for ArrayString<A> {
    /// Return an empty `ArrayString`
    fn default() -> ArrayString<A> {
        ArrayString::new()
    }
}

impl<A: Array<Item=u8>> ArrayString<A> {
    /// Create a new empty `ArrayString`.
    ///
    /// Capacity is inferred from the type parameter.
    ///
    /// ```
    /// use arrayvec::ArrayString;
    ///
    /// let mut string = ArrayString::<[_; 16]>::new();
    /// string.push_str("foo");
    /// assert_eq!(&string[..], "foo");
    /// assert_eq!(string.capacity(), 16);
    /// ```
    pub fn new() -> ArrayString<A> {
        unsafe {
            ArrayString {
                // FIXME: Use Copyable union for xs when we can
                xs: mem::zeroed(),
                len: Index::from(0),
            }
        }
    }

    /// Create a new `ArrayString` from a `str`.
    ///
    /// Capacity is inferred from the type parameter.
    ///
    /// **Errors** if the backing array is not large enough to fit the string.
    ///
    /// ```
    /// use arrayvec::ArrayString;
    ///
    /// let mut string = ArrayString::<[_; 3]>::from("foo").unwrap();
    /// assert_eq!(&string[..], "foo");
    /// assert_eq!(string.len(), 3);
    /// assert_eq!(string.capacity(), 3);
    /// ```
    pub fn from(s: &str) -> Result<Self, CapacityError<&str>> {
        let mut arraystr = Self::new();
        arraystr.try_push_str(s)?;
        Ok(arraystr)
    }

    /// Create a new `ArrayString` from a byte string literal.
    ///
    /// **Errors** if the byte string literal is not valid UTF-8.
    ///
    /// ```
    /// use arrayvec::ArrayString;
    ///
    /// let string = ArrayString::from_byte_string(b"hello world").unwrap();
    /// ```
    pub fn from_byte_string(b: &A) -> Result<Self, Utf8Error> {
        let mut arraystr = Self::new();
        let s = try!(str::from_utf8(b.as_slice()));
        let _result = arraystr.try_push_str(s);
        debug_assert!(_result.is_ok());
        Ok(arraystr)
    }

    /// Return the capacity of the `ArrayString`.
    ///
    /// ```
    /// use arrayvec::ArrayString;
    ///
    /// let string = ArrayString::<[_; 3]>::new();
    /// assert_eq!(string.capacity(), 3);
    /// ```
    #[inline]
    pub fn capacity(&self) -> usize { A::capacity() }

    /// Return if the `ArrayString` is completely filled.
    ///
    /// ```
    /// use arrayvec::ArrayString;
    ///
    /// let mut string = ArrayString::<[_; 1]>::new();
    /// assert!(!string.is_full());
    /// string.push_str("A");
    /// assert!(string.is_full());
    /// ```
    pub fn is_full(&self) -> bool { self.len() == self.capacity() }

    /// Adds the given char to the end of the string.
    ///
    /// ***Panics*** if the backing array is not large enough to fit the additional char.
    ///
    /// ```
    /// use arrayvec::ArrayString;
    ///
    /// let mut string = ArrayString::<[_; 2]>::new();
    ///
    /// string.push('a');
    /// string.push('b');
    ///
    /// assert_eq!(&string[..], "ab");
    /// ```
    pub fn push(&mut self, c: char) {
        self.try_push(c).unwrap();
    }

    /// Adds the given char to the end of the string.
    ///
    /// Returns `Ok` if the push succeeds.
    ///
    /// **Errors** if the backing array is not large enough to fit the additional char.
    ///
    /// ```
    /// use arrayvec::ArrayString;
    ///
    /// let mut string = ArrayString::<[_; 2]>::new();
    ///
    /// string.try_push('a').unwrap();
    /// string.try_push('b').unwrap();
    /// let overflow = string.try_push('c');
    ///
    /// assert_eq!(&string[..], "ab");
    /// assert_eq!(overflow.unwrap_err().element(), 'c');
    /// ```
    pub fn try_push(&mut self, c: char) -> Result<(), CapacityError<char>> {
        let len = self.len();
        unsafe {
            match encode_utf8(c, &mut self.raw_mut_bytes()[len..]) {
                Ok(n) => {
                    self.set_len(len + n);
                    Ok(())
                }
                Err(_) => Err(CapacityError::new(c)),
            }
        }
    }

    /// Adds the given string slice to the end of the string.
    ///
    /// ***Panics*** if the backing array is not large enough to fit the string.
    ///
    /// ```
    /// use arrayvec::ArrayString;
    ///
    /// let mut string = ArrayString::<[_; 2]>::new();
    ///
    /// string.push_str("a");
    /// string.push_str("d");
    ///
    /// assert_eq!(&string[..], "ad");
    /// ```
    pub fn push_str(&mut self, s: &str) {
        self.try_push_str(s).unwrap()
    }

    /// Adds the given string slice to the end of the string.
    ///
    /// Returns `Ok` if the push succeeds.
    ///
    /// **Errors** if the backing array is not large enough to fit the string.
    ///
    /// ```
    /// use arrayvec::ArrayString;
    ///
    /// let mut string = ArrayString::<[_; 2]>::new();
    ///
    /// string.try_push_str("a").unwrap();
    /// let overflow1 = string.try_push_str("bc");
    /// string.try_push_str("d").unwrap();
    /// let overflow2 = string.try_push_str("ef");
    ///
    /// assert_eq!(&string[..], "ad");
    /// assert_eq!(overflow1.unwrap_err().element(), "bc");
    /// assert_eq!(overflow2.unwrap_err().element(), "ef");
    /// ```
    pub fn try_push_str<'a>(&mut self, s: &'a str) -> Result<(), CapacityError<&'a str>> {
        if s.len() > self.capacity() - self.len() {
            return Err(CapacityError::new(s));
        }
        unsafe {
            let dst = self.xs.as_mut_ptr().offset(self.len() as isize);
            let src = s.as_ptr();
            ptr::copy_nonoverlapping(src, dst, s.len());
            let newl = self.len() + s.len();
            self.set_len(newl);
        }
        Ok(())
    }

    /// Removes the last character from the string and returns it.
    ///
    /// Returns `None` if this `ArrayString` is empty.
    ///
    /// ```
    /// use arrayvec::ArrayString;
    /// 
    /// let mut s = ArrayString::<[_; 3]>::from("foo").unwrap();
    ///
    /// assert_eq!(s.pop(), Some('o'));
    /// assert_eq!(s.pop(), Some('o'));
    /// assert_eq!(s.pop(), Some('f'));
    ///
    /// assert_eq!(s.pop(), None);
    /// ```
    #[inline]
    pub fn pop(&mut self) -> Option<char> {
        let ch = match self.chars().rev().next() {
            Some(ch) => ch,
            None => return None,
        };
        let new_len = self.len() - ch.len_utf8();
        unsafe {
            self.set_len(new_len);
        }
        Some(ch)
    }

    /// Shortens this `ArrayString` to the specified length.
    ///
    /// If `new_len` is greater than the string’s current length, this has no
    /// effect.
    ///
    /// ***Panics*** if `new_len` does not lie on a `char` boundary.
    ///
    /// ```
    /// use arrayvec::ArrayString;
    ///
    /// let mut string = ArrayString::<[_; 6]>::from("foobar").unwrap();
    /// string.truncate(3);
    /// assert_eq!(&string[..], "foo");
    /// string.truncate(4);
    /// assert_eq!(&string[..], "foo");
    /// ```
    #[inline]
    pub fn truncate(&mut self, new_len: usize) {
        if new_len <= self.len() {
            assert!(self.is_char_boundary(new_len));
            unsafe { 
                // In libstd truncate is called on the underlying vector,
                // which in turns drops each element.
                // As we know we don't have to worry about Drop,
                // we can just set the length (a la clear.)
                self.set_len(new_len);
            }
        }
    }

    /// Removes a `char` from this `ArrayString` at a byte position and returns it.
    ///
    /// This is an `O(n)` operation, as it requires copying every element in the
    /// array.
    ///
    /// ***Panics*** if `idx` is larger than or equal to the `ArrayString`’s length,
    /// or if it does not lie on a `char` boundary.
    ///
    /// ```
    /// use arrayvec::ArrayString;
    /// 
    /// let mut s = ArrayString::<[_; 3]>::from("foo").unwrap();
    ///
    /// assert_eq!(s.remove(0), 'f');
    /// assert_eq!(s.remove(1), 'o');
    /// assert_eq!(s.remove(0), 'o');
    /// ```
    #[inline]
    pub fn remove(&mut self, idx: usize) -> char {
        let ch = match self[idx..].chars().next() {
            Some(ch) => ch,
            None => panic!("cannot remove a char from the end of a string"),
        };

        let next = idx + ch.len_utf8();
        let len = self.len();
        unsafe {
            ptr::copy(self.xs.as_ptr().offset(next as isize),
                      self.xs.as_mut_ptr().offset(idx as isize),
                      len - next);
            self.set_len(len - (next - idx));
        }
        ch
    }

    /// Make the string empty.
    pub fn clear(&mut self) {
        unsafe {
            self.set_len(0);
        }
    }

    /// Set the strings’s length.
    ///
    /// This function is `unsafe` because it changes the notion of the
    /// number of “valid” bytes in the string. Use with care.
    ///
    /// This method uses *debug assertions* to check the validity of `length`
    /// and may use other debug assertions.
    #[inline]
    pub unsafe fn set_len(&mut self, length: usize) {
        debug_assert!(length <= self.capacity());
        self.len = Index::from(length);
    }

    /// Return a string slice of the whole `ArrayString`.
    pub fn as_str(&self) -> &str {
        self
    }

    /// Return a mutable slice of the whole string’s buffer
    unsafe fn raw_mut_bytes(&mut self) -> &mut [u8] {
        slice::from_raw_parts_mut(self.xs.as_mut_ptr(), self.capacity())
    }
}

impl<A: Array<Item=u8>> Deref for ArrayString<A> {
    type Target = str;
    #[inline]
    fn deref(&self) -> &str {
        unsafe {
            let sl = slice::from_raw_parts(self.xs.as_ptr(), self.len.to_usize());
            str::from_utf8_unchecked(sl)
        }
    }
}

impl<A: Array<Item=u8>> DerefMut for ArrayString<A> {
    #[inline]
    fn deref_mut(&mut self) -> &mut str {
        unsafe {
            let sl = slice::from_raw_parts_mut(self.xs.as_mut_ptr(), self.len.to_usize());
            // FIXME: Nothing but transmute to do this right now
            mem::transmute(sl)
        }
    }
}

impl<A: Array<Item=u8>> PartialEq for ArrayString<A> {
    fn eq(&self, rhs: &Self) -> bool {
        **self == **rhs
    }
}

impl<A: Array<Item=u8>> PartialEq<str> for ArrayString<A> {
    fn eq(&self, rhs: &str) -> bool {
        &**self == rhs
    }
}

impl<A: Array<Item=u8>> PartialEq<ArrayString<A>> for str {
    fn eq(&self, rhs: &ArrayString<A>) -> bool {
        self == &**rhs
    }
}

impl<A: Array<Item=u8>> Eq for ArrayString<A> { }

impl<A: Array<Item=u8>> Hash for ArrayString<A> {
    fn hash<H: Hasher>(&self, h: &mut H) {
        (**self).hash(h)
    }
}

impl<A: Array<Item=u8>> Borrow<str> for ArrayString<A> {
    fn borrow(&self) -> &str { self }
}

impl<A: Array<Item=u8>> AsRef<str> for ArrayString<A> {
    fn as_ref(&self) -> &str { self }
}

impl<A: Array<Item=u8>> fmt::Debug for ArrayString<A> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result { (**self).fmt(f) }
}

impl<A: Array<Item=u8>> fmt::Display for ArrayString<A> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result { (**self).fmt(f) }
}

/// `Write` appends written data to the end of the string.
impl<A: Array<Item=u8>> fmt::Write for ArrayString<A> {
    fn write_char(&mut self, c: char) -> fmt::Result {
        self.try_push(c).map_err(|_| fmt::Error)
    }

    fn write_str(&mut self, s: &str) -> fmt::Result {
        self.try_push_str(s).map_err(|_| fmt::Error)
    }
}

impl<A: Array<Item=u8> + Copy> Clone for ArrayString<A> {
    fn clone(&self) -> ArrayString<A> {
        *self
    }
    fn clone_from(&mut self, rhs: &Self) {
        // guaranteed to fit due to types matching.
        self.clear();
        self.try_push_str(rhs).ok();
    }
}

impl<A: Array<Item=u8>> PartialOrd for ArrayString<A> {
    fn partial_cmp(&self, rhs: &Self) -> Option<cmp::Ordering> {
        (**self).partial_cmp(&**rhs)
    }
    fn lt(&self, rhs: &Self) -> bool { **self < **rhs }
    fn le(&self, rhs: &Self) -> bool { **self <= **rhs }
    fn gt(&self, rhs: &Self) -> bool { **self > **rhs }
    fn ge(&self, rhs: &Self) -> bool { **self >= **rhs }
}

impl<A: Array<Item=u8>> PartialOrd<str> for ArrayString<A> {
    fn partial_cmp(&self, rhs: &str) -> Option<cmp::Ordering> {
        (**self).partial_cmp(rhs)
    }
    fn lt(&self, rhs: &str) -> bool { &**self < rhs }
    fn le(&self, rhs: &str) -> bool { &**self <= rhs }
    fn gt(&self, rhs: &str) -> bool { &**self > rhs }
    fn ge(&self, rhs: &str) -> bool { &**self >= rhs }
}

impl<A: Array<Item=u8>> PartialOrd<ArrayString<A>> for str {
    fn partial_cmp(&self, rhs: &ArrayString<A>) -> Option<cmp::Ordering> {
        self.partial_cmp(&**rhs)
    }
    fn lt(&self, rhs: &ArrayString<A>) -> bool { self < &**rhs }
    fn le(&self, rhs: &ArrayString<A>) -> bool { self <= &**rhs }
    fn gt(&self, rhs: &ArrayString<A>) -> bool { self > &**rhs }
    fn ge(&self, rhs: &ArrayString<A>) -> bool { self >= &**rhs }
}

impl<A: Array<Item=u8>> Ord for ArrayString<A> {
    fn cmp(&self, rhs: &Self) -> cmp::Ordering {
        (**self).cmp(&**rhs)
    }
}

#[cfg(feature="serde-1")]
/// Requires crate feature `"serde-1"`
impl<A: Array<Item=u8>> Serialize for ArrayString<A> {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
        where S: Serializer
    {
        serializer.serialize_str(&*self)
    }
}

#[cfg(feature="serde-1")]
/// Requires crate feature `"serde-1"`
impl<'de, A: Array<Item=u8>> Deserialize<'de> for ArrayString<A> {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
        where D: Deserializer<'de>
    {
        use serde::de::{self, Visitor};
        use std::marker::PhantomData;

        struct ArrayStringVisitor<A: Array<Item=u8>>(PhantomData<A>);

        impl<'de, A: Array<Item=u8>> Visitor<'de> for ArrayStringVisitor<A> {
            type Value = ArrayString<A>;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                write!(formatter, "a string no more than {} bytes long", A::capacity())
            }

            fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
                where E: de::Error,
            {
                ArrayString::from(v).map_err(|_| E::invalid_length(v.len(), &self))
            }

            fn visit_bytes<E>(self, v: &[u8]) -> Result<Self::Value, E>
                where E: de::Error,
            {
                let s = try!(str::from_utf8(v).map_err(|_| E::invalid_value(de::Unexpected::Bytes(v), &self)));

                ArrayString::from(s).map_err(|_| E::invalid_length(s.len(), &self))
            }
        }

        deserializer.deserialize_str(ArrayStringVisitor::<A>(PhantomData))
    }
}
