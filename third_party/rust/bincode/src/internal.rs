use std::io::{Read, Write};
use serde;

use config::{Options, OptionsExt};
use de::read::BincodeRead;
use {ErrorKind, Result};

#[derive(Clone)]
struct CountSize<L: SizeLimit> {
    total: u64,
    other_limit: L,
}

pub(crate) fn serialize_into<W, T: ?Sized, O>(writer: W, value: &T, mut options: O) -> Result<()>
where
    W: Write,
    T: serde::Serialize,
    O: Options,
{
    if options.limit().limit().is_some() {
        // "compute" the size for the side-effect
        // of returning Err if the bound was reached.
        serialized_size(value, &mut options)?;
    }

    let mut serializer = ::ser::Serializer::<_, O>::new(writer, options);
    serde::Serialize::serialize(value, &mut serializer)
}

pub(crate) fn serialize<T: ?Sized, O>(value: &T, mut options: O) -> Result<Vec<u8>>
where
    T: serde::Serialize,
    O: Options,
{
    let mut writer = {
        let actual_size = serialized_size(value, &mut options)?;
        Vec::with_capacity(actual_size as usize)
    };

    serialize_into(&mut writer, value, options.with_no_limit())?;
    Ok(writer)
}

impl<L: SizeLimit> SizeLimit for CountSize<L> {
    fn add(&mut self, c: u64) -> Result<()> {
        self.other_limit.add(c)?;
        self.total += c;
        Ok(())
    }

    fn limit(&self) -> Option<u64> {
        unreachable!();
    }
}

pub(crate) fn serialized_size<T: ?Sized, O: Options>(value: &T, mut options: O) -> Result<u64>
where
    T: serde::Serialize,
{
    let old_limiter = options.limit().clone();
    let mut size_counter = ::ser::SizeChecker {
        options: ::config::WithOtherLimit::new(
            options,
            CountSize {
                total: 0,
                other_limit: old_limiter,
            },
        ),
    };

    let result = value.serialize(&mut size_counter);
    result.map(|_| size_counter.options.new_limit.total)
}

pub(crate) fn deserialize_from<R, T, O>(reader: R, options: O) -> Result<T>
where
    R: Read,
    T: serde::de::DeserializeOwned,
    O: Options,
{
    let reader = ::de::read::IoReader::new(reader);
    let mut deserializer = ::de::Deserializer::<_, O>::new(reader, options);
    serde::Deserialize::deserialize(&mut deserializer)
}

pub(crate) fn deserialize_from_custom<'a, R, T, O>(reader: R, options: O) -> Result<T>
where
    R: BincodeRead<'a>,
    T: serde::de::DeserializeOwned,
    O: Options,
{
    let mut deserializer = ::de::Deserializer::<_, O>::new(reader, options);
    serde::Deserialize::deserialize(&mut deserializer)
}

pub(crate) fn deserialize_in_place<'a, R, T, O>(reader: R, options: O, place: &mut T) -> Result<()>
where
    R: BincodeRead<'a>,
    T: serde::de::Deserialize<'a>,
    O: Options,
{
    let mut deserializer = ::de::Deserializer::<_, _>::new(reader, options);
    serde::Deserialize::deserialize_in_place(&mut deserializer, place)
}

pub(crate) fn deserialize<'a, T, O>(bytes: &'a [u8], options: O) -> Result<T>
where
    T: serde::de::Deserialize<'a>,
    O: Options,
{
    let reader = ::de::read::SliceReader::new(bytes);
    let options = ::config::WithOtherLimit::new(options, Infinite);
    let mut deserializer = ::de::Deserializer::new(reader, options);
    serde::Deserialize::deserialize(&mut deserializer)
}


pub(crate) trait SizeLimit: Clone {
    /// Tells the SizeLimit that a certain number of bytes has been
    /// read or written.  Returns Err if the limit has been exceeded.
    fn add(&mut self, n: u64) -> Result<()>;
    /// Returns the hard limit (if one exists)
    fn limit(&self) -> Option<u64>;
}


/// A SizeLimit that restricts serialized or deserialized messages from
/// exceeding a certain byte length.
#[derive(Copy, Clone)]
pub struct Bounded(pub u64);

/// A SizeLimit without a limit!
/// Use this if you don't care about the size of encoded or decoded messages.
#[derive(Copy, Clone)]
pub struct Infinite;

impl SizeLimit for Bounded {
    #[inline(always)]
    fn add(&mut self, n: u64) -> Result<()> {
        if self.0 >= n {
            self.0 -= n;
            Ok(())
        } else {
            Err(Box::new(ErrorKind::SizeLimit))
        }
    }

    #[inline(always)]
    fn limit(&self) -> Option<u64> {
        Some(self.0)
    }
}

impl SizeLimit for Infinite {
    #[inline(always)]
    fn add(&mut self, _: u64) -> Result<()> {
        Ok(())
    }

    #[inline(always)]
    fn limit(&self) -> Option<u64> {
        None
    }
}
