use std::boxed::Box;
use std::ops::Deref;

#[cfg(feature = "rustc-serialize")]
use rustc_serialize_crate::{Encodable, Encoder, Decodable, Decoder};

#[cfg(feature = "serde")]
use serde_crate as serde;

/// A struct for encoding nested reference types.
///
/// Encoding large objects by reference is really handy.  For example,
/// `encode(&large_hashmap, ...)` encodes the large structure without having to
/// own the hashmap.  However, it is impossible to serialize a reference if that
/// reference is inside of a struct.
///
/// ```ignore rust
/// // Not possible, rustc can not decode the reference.
/// #[derive(RustcEncoding, RustcDecoding)]
/// struct Message<'a>  {
///   big_map: &'a HashMap<u32, u32>,
///   message_type: String,
/// }
/// ```
///
/// This is because on the decoding side, you can't create the Message struct
/// because it needs to have a reference to a HashMap, which is impossible because
/// during deserialization, all members need to be owned by the deserialized
/// object.
///
/// This is where RefBox comes in.  During serialization, it serializs a reference,
/// but during deserialization, it puts that sub-object into a box!
///
/// ```ignore rust
/// // This works!
/// #[derive(RustcEncoding, RustcDecoding)]
/// struct Message<'a> {
///     big_map: RefBox<'a, HashMap<u32, u32>>,
///     message_type: String
/// }
/// ```
///
/// Now we can write
///
/// ```ignore rust
/// let my_map = HashMap::new();
/// let my_msg = Message {
///     big_map: RefBox::new(&my_map),
///     message_type: "foo".to_string()
/// };
///
/// let encoded = encode(&my_msg, ...).unwrap();
/// let decoded: Message<'static> = decode(&encoded[]).unwrap();
/// ```
///
/// Notice that we managed to encode and decode a struct with a nested reference
/// and that the decoded message has the lifetime `'static` which shows us
/// that the message owns everything inside it completely.
///
/// Please don't stick RefBox inside deep data structures.  It is much better
/// suited in the outermost layer of whatever it is that you are encoding.
#[derive(Debug, PartialEq, PartialOrd, Eq, Ord, Hash, Clone)]
pub struct RefBox<'a, T: 'a> {
    inner:  RefBoxInner<'a, T, Box<T>>
}

/// Like a RefBox, but encoding from a `str` and decoedes to a `String`.
#[derive(Debug, PartialEq, PartialOrd, Eq, Ord, Hash, Clone)]
pub struct StrBox<'a> {
    inner: RefBoxInner<'a, str, String>
}

/// Like a RefBox, but encodes from a `[T]` and encodes to a `Vec<T>`.
#[derive(Debug, PartialEq, PartialOrd, Eq, Ord, Hash, Clone)]
pub struct SliceBox<'a, T: 'a> {
    inner: RefBoxInner<'a, [T], Vec<T>>
}

#[derive(Debug, PartialEq, PartialOrd, Eq, Ord, Hash)]
enum RefBoxInner<'a, A: 'a + ?Sized, B> {
    Ref(&'a A),
    Box(B)
}

impl<'a, T> Clone for RefBoxInner<'a, T, Box<T>> where T: Clone {
    fn clone(&self) -> RefBoxInner<'a, T, Box<T>> {
        match *self {
            RefBoxInner::Ref(reff) => RefBoxInner::Box(Box::new(reff.clone())),
            RefBoxInner::Box(ref boxed) => RefBoxInner::Box(boxed.clone())
        }
    }
}

impl<'a> Clone for RefBoxInner<'a, str, String> {
    fn clone(&self) -> RefBoxInner<'a, str, String> {
        match *self {
            RefBoxInner::Ref(reff) => RefBoxInner::Box(String::from(reff)),
            RefBoxInner::Box(ref boxed) => RefBoxInner::Box(boxed.clone())
        }
    }
}

impl<'a, T> Clone for RefBoxInner<'a, [T], Vec<T>> where T: Clone {
    fn clone(&self) -> RefBoxInner<'a, [T], Vec<T>> {
        match *self {
            RefBoxInner::Ref(reff) => RefBoxInner::Box(Vec::from(reff)),
            RefBoxInner::Box(ref boxed) => RefBoxInner::Box(boxed.clone())
        }
    }
}

impl <'a, T> RefBox<'a, T> {
    /// Creates a new RefBox that looks at a borrowed value.
    pub fn new(v: &'a T) -> RefBox<'a, T> {
        RefBox {
            inner: RefBoxInner::Ref(v)
        }
    }
}

impl <T> RefBox<'static, T>  {
    /// Takes the value out of this refbox.
    ///
    /// Fails if this refbox was not created out of a deserialization.
    ///
    /// Unless you are doing some really weird things with static references,
    /// this function will never fail.
    pub fn take(self) -> Box<T> {
        match self.inner {
            RefBoxInner::Box(b) => b,
            _ => unreachable!()
        }
    }

    /// Tries to take the value out of this refbox.
    pub fn try_take(self) -> Result<Box<T>, RefBox<'static, T>> {
        match self.inner {
            RefBoxInner::Box(b) => Ok(b),
            o => Err(RefBox{ inner: o})
        }
    }
}

#[cfg(feature = "rustc-serialize")]
impl <'a, T: Encodable> Encodable for RefBox<'a, T> {
    fn encode<S: Encoder>(&self, s: &mut S) -> Result<(), S::Error> {
        self.inner.encode(s)
    }
}

#[cfg(feature = "rustc-serialize")]
impl <T: Decodable> Decodable for RefBox<'static, T> {
    fn decode<D: Decoder>(d: &mut D) -> Result<RefBox<'static, T>, D::Error> {
        let inner = try!(Decodable::decode(d));
        Ok(RefBox{inner: inner})
    }
}

#[cfg(feature = "serde")]
impl<'a, T> serde::Serialize for RefBox<'a, T>
    where T: serde::Serialize,
{
    fn serialize<S>(&self, serializer: &mut S) -> Result<(), S::Error>
        where S: serde::Serializer
    {
        serde::Serialize::serialize(&self.inner, serializer)
    }
}

#[cfg(feature = "serde")]
impl<'a, T: serde::Deserialize> serde::Deserialize for RefBox<'a, T> {
    fn deserialize<D>(deserializer: &mut D) -> Result<Self, D::Error>
        where D: serde::Deserializer
    {
        let inner = try!(serde::Deserialize::deserialize(deserializer));
        Ok(RefBox{ inner: inner })
    }
}

impl<'a> StrBox<'a> {
    /// Creates a new StrBox that looks at a borrowed value.
    pub fn new(s: &'a str) -> StrBox<'a> {
        StrBox {
            inner: RefBoxInner::Ref(s)
        }
    }

    /// Extract a String from a StrBox.
    pub fn into_string(self) -> String {
        match self.inner {
            RefBoxInner::Ref(s) => String::from(s),
            RefBoxInner::Box(s) => s
        }
    }

    /// Convert to an Owned `SliceBox`.
    pub fn to_owned(self) -> StrBox<'static> {
        match self.inner {
            RefBoxInner::Ref(s) => StrBox::boxed(String::from(s)),
            RefBoxInner::Box(s) => StrBox::boxed(s)
        }
    }
}

impl<'a> AsRef<str> for StrBox<'a> {
    fn as_ref(&self) -> &str {
        match self.inner {
            RefBoxInner::Ref(ref s) => s,
            RefBoxInner::Box(ref s) => s
        }
    }
}

impl StrBox<'static>  {
    /// Creates a new StrBox made from an allocated String.
    pub fn boxed(s: String) -> StrBox<'static> {
        StrBox { inner: RefBoxInner::Box(s) }
    }

    /// Takes the value out of this refbox.
    ///
    /// Fails if this refbox was not created out of a deserialization.
    ///
    /// Unless you are doing some really weird things with static references,
    /// this function will never fail.
    pub fn take(self) -> String {
        match self.inner {
            RefBoxInner::Box(b) => b,
            RefBoxInner::Ref(b) => String::from(b)
        }
    }

    /// Tries to take the value out of this refbox.
    pub fn try_take(self) -> Result<String, StrBox<'static>> {
        match self.inner {
            RefBoxInner::Box(b) => Ok(b),
            o => Err(StrBox{ inner: o})
        }
    }
}

#[cfg(feature = "rustc-serialize")]
impl <'a> Encodable for StrBox<'a> {
    fn encode<S: Encoder>(&self, s: &mut S) -> Result<(), S::Error> {
        self.inner.encode(s)
    }
}

#[cfg(feature = "rustc-serialize")]
impl Decodable for StrBox<'static> {
    fn decode<D: Decoder>(d: &mut D) -> Result<StrBox<'static>, D::Error> {
        let inner: RefBoxInner<'static, str, String> = try!(Decodable::decode(d));
        Ok(StrBox{inner: inner})
    }
}

#[cfg(feature = "serde")]
impl<'a> serde::Serialize for StrBox<'a> {
    fn serialize<S>(&self, serializer: &mut S) -> Result<(), S::Error>
        where S: serde::Serializer
    {
        serde::Serialize::serialize(&self.inner, serializer)
    }
}

#[cfg(feature = "serde")]
impl serde::Deserialize for StrBox<'static> {
    fn deserialize<D>(deserializer: &mut D) -> Result<Self, D::Error>
        where D: serde::Deserializer
    {
        let inner = try!(serde::Deserialize::deserialize(deserializer));
        Ok(StrBox{ inner: inner })
    }
}

//
// SliceBox
//

impl <'a, T> SliceBox<'a, T> {
    /// Creates a new RefBox that looks at a borrowed value.
    pub fn new(v: &'a [T]) -> SliceBox<'a, T> {
        SliceBox {
            inner: RefBoxInner::Ref(v)
        }
    }

    /// Extract a `Vec<T>` from a `SliceBox`.
    pub fn into_vec(self) -> Vec<T> where T: Clone {
        match self.inner {
            RefBoxInner::Ref(s) => s.to_vec(),
            RefBoxInner::Box(s) => s
        }
    }

    /// Convert to an Owned `SliceBox`.
    pub fn to_owned(self) -> SliceBox<'static, T> where T: Clone {
        match self.inner {
            RefBoxInner::Ref(s) => SliceBox::boxed(s.to_vec()),
            RefBoxInner::Box(s) => SliceBox::boxed(s)
        }
    }
}

impl <T> SliceBox<'static, T>  {
    /// Creates a new SliceBox made from an allocated `Vec<T>`.
    pub fn boxed(s: Vec<T>) -> SliceBox<'static, T> {
        SliceBox { inner: RefBoxInner::Box(s) }
    }

    /// Takes the value out of this refbox.
    ///
    /// Fails if this refbox was not created out of a deserialization.
    ///
    /// Unless you are doing some really weird things with static references,
    /// this function will never fail.
    pub fn take(self) -> Vec<T> {
        match self.inner {
            RefBoxInner::Box(b) => b,
            _ => unreachable!()
        }
    }

    /// Tries to take the value out of this refbox.
    pub fn try_take(self) -> Result<Vec<T>, SliceBox<'static, T>> {
        match self.inner {
            RefBoxInner::Box(b) => Ok(b),
            o => Err(SliceBox{ inner: o})
        }
    }
}

#[cfg(feature = "rustc-serialize")]
impl <'a, T: Encodable> Encodable for SliceBox<'a, T> {
    fn encode<S: Encoder>(&self, s: &mut S) -> Result<(), S::Error> {
        self.inner.encode(s)
    }
}

#[cfg(feature = "rustc-serialize")]
impl <T: Decodable> Decodable for SliceBox<'static, T> {
    fn decode<D: Decoder>(d: &mut D) -> Result<SliceBox<'static, T>, D::Error> {
        let inner: RefBoxInner<'static, [T], Vec<T>> = try!(Decodable::decode(d));
        Ok(SliceBox{inner: inner})
    }
}

#[cfg(feature = "serde")]
impl<'a, T> serde::Serialize for SliceBox<'a, T>
    where T: serde::Serialize,
{
    fn serialize<S>(&self, serializer: &mut S) -> Result<(), S::Error>
        where S: serde::Serializer
    {
        serde::Serialize::serialize(&self.inner, serializer)
    }
}

#[cfg(feature = "serde")]
impl<'a, T: serde::Deserialize> serde::Deserialize for SliceBox<'a, T> {
    fn deserialize<D>(deserializer: &mut D) -> Result<Self, D::Error>
        where D: serde::Deserializer
    {
        let inner = try!(serde::Deserialize::deserialize(deserializer));
        Ok(SliceBox{ inner: inner })
    }
}

#[cfg(feature = "rustc-serialize")]
impl <'a, A: Encodable + ?Sized, B: Encodable> Encodable for RefBoxInner<'a, A, B> {
    fn encode<S: Encoder>(&self, s: &mut S) -> Result<(), S::Error> {
        match self {
            &RefBoxInner::Ref(ref r) => r.encode(s),
            &RefBoxInner::Box(ref b) => b.encode(s)
        }
    }
}

#[cfg(feature = "serde")]
impl<'a, A: ?Sized, B> serde::Serialize for RefBoxInner<'a, A, B>
    where A: serde::Serialize,
          B: serde::Serialize,
{
    fn serialize<S>(&self, serializer: &mut S) -> Result<(), S::Error>
        where S: serde::Serializer
    {
        match self {
            &RefBoxInner::Ref(ref r) => serde::Serialize::serialize(r, serializer),
            &RefBoxInner::Box(ref b) => serde::Serialize::serialize(b, serializer),
        }
    }
}

#[cfg(feature = "rustc-serialize")]
impl <A: ?Sized, B: Decodable> Decodable for RefBoxInner<'static, A, B> {
    fn decode<D: Decoder>(d: &mut D) -> Result<RefBoxInner<'static, A, B>, D::Error> {
        let decoded = try!(Decodable::decode(d));
        Ok(RefBoxInner::Box(decoded))
    }
}

#[cfg(feature = "serde")]
impl<'a, A: ?Sized, B> serde::Deserialize for RefBoxInner<'a, A, B>
    where B: serde::Deserialize,
{
    fn deserialize<D>(deserializer: &mut D) -> Result<Self, D::Error>
        where D: serde::Deserializer
    {
        let deserialized = try!(serde::Deserialize::deserialize(deserializer));
        Ok(RefBoxInner::Box(deserialized))
    }
}

impl <'a, T> Deref for RefBox<'a, T> {
    type Target = T;

    fn deref(&self) -> &T {
        match &self.inner {
            &RefBoxInner::Ref(ref t) => t,
            &RefBoxInner::Box(ref b) => b.deref()
        }
    }
}

impl <'a, T> Deref for SliceBox<'a, T> {
    type Target = [T];

    fn deref(&self) -> &[T] {
        match &self.inner {
            &RefBoxInner::Ref(ref t) => t,
            &RefBoxInner::Box(ref b) => b.deref()
        }
    }
}
