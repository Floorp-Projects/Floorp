use byteorder::{BigEndian, ReadBytesExt};
use std::io::{Read, Seek, SeekFrom};
use std::mem::size_of;
use std::string::{FromUtf16Error, FromUtf8Error};

use stream::Event;
use {u64_to_usize, Date, Error};

impl From<FromUtf8Error> for Error {
    fn from(_: FromUtf8Error) -> Error {
        Error::InvalidData
    }
}

impl From<FromUtf16Error> for Error {
    fn from(_: FromUtf16Error) -> Error {
        Error::InvalidData
    }
}

struct StackItem {
    object_ref: u64,
    child_object_refs: Vec<u64>,
    ty: StackType,
}

enum StackType {
    Array,
    Dict,
}

// https://opensource.apple.com/source/CF/CF-550/CFBinaryPList.c
// https://hg.python.org/cpython/file/3.4/Lib/plistlib.py
pub struct BinaryReader<R> {
    stack: Vec<StackItem>,
    object_offsets: Vec<u64>,
    object_on_stack: Vec<bool>,
    reader: R,
    ref_size: u8,
    root_object: u64,
    // The largest single allocation allowed for this plist.
    // Equal to the number of bytes in the plist minus the magic number and trailer.
    max_allocation_bytes: usize,
}

impl<R: Read + Seek> BinaryReader<R> {
    pub fn new(reader: R) -> BinaryReader<R> {
        BinaryReader {
            stack: Vec::new(),
            object_offsets: Vec::new(),
            object_on_stack: Vec::new(),
            reader,
            ref_size: 0,
            root_object: 0,
            max_allocation_bytes: 0,
        }
    }

    fn can_allocate(&self, len: u64, size: usize) -> bool {
        let byte_len = len.saturating_mul(size as u64);
        byte_len <= self.max_allocation_bytes as u64
    }

    fn allocate_vec<T>(&self, len: u64, size: usize) -> Result<Vec<T>, Error> {
        if self.can_allocate(len, size) {
            Ok(Vec::with_capacity(len as usize))
        } else {
            Err(Error::InvalidData)
        }
    }

    fn read_trailer(&mut self) -> Result<(), Error> {
        self.reader.seek(SeekFrom::Start(0))?;
        let mut magic = [0; 8];
        self.reader.read_exact(&mut magic)?;
        if &magic != b"bplist00" {
            return Err(Error::InvalidData);
        }

        // Trailer starts with 6 bytes of padding
        let trailer_start = self.reader.seek(SeekFrom::End(-32 + 6))?;

        let offset_size = self.reader.read_u8()?;
        match offset_size {
            1 | 2 | 4 | 8 => (),
            _ => return Err(Error::InvalidData),
        }

        self.ref_size = self.reader.read_u8()?;
        match self.ref_size {
            1 | 2 | 4 | 8 => (),
            _ => return Err(Error::InvalidData),
        }

        let num_objects = self.reader.read_u64::<BigEndian>()?;
        self.root_object = self.reader.read_u64::<BigEndian>()?;
        let offset_table_offset = self.reader.read_u64::<BigEndian>()?;

        // File size minus trailer and header
        // Truncated to max(usize)
        self.max_allocation_bytes = trailer_start.saturating_sub(8) as usize;

        // Read offset table
        self.reader.seek(SeekFrom::Start(offset_table_offset))?;
        self.object_offsets = self.read_ints(num_objects, offset_size)?;
        self.object_on_stack = vec![false; self.object_offsets.len()];

        Ok(())
    }

    fn read_ints(&mut self, len: u64, size: u8) -> Result<Vec<u64>, Error> {
        let mut ints = self.allocate_vec(len, size as usize)?;
        for _ in 0..len {
            match size {
                1 => ints.push(self.reader.read_u8()?.into()),
                2 => ints.push(self.reader.read_u16::<BigEndian>()?.into()),
                4 => ints.push(self.reader.read_u32::<BigEndian>()?.into()),
                8 => ints.push(self.reader.read_u64::<BigEndian>()?),
                _ => return Err(Error::InvalidData),
            }
        }
        Ok(ints)
    }

    fn read_refs(&mut self, len: u64) -> Result<Vec<u64>, Error> {
        let ref_size = self.ref_size;
        self.read_ints(len, ref_size)
    }

    fn read_object_len(&mut self, len: u8) -> Result<u64, Error> {
        if (len & 0x0f) == 0x0f {
            let len_power_of_two = self.reader.read_u8()? & 0x03;
            Ok(match len_power_of_two {
                0 => self.reader.read_u8()?.into(),
                1 => self.reader.read_u16::<BigEndian>()?.into(),
                2 => self.reader.read_u32::<BigEndian>()?.into(),
                3 => self.reader.read_u64::<BigEndian>()?,
                _ => return Err(Error::InvalidData),
            })
        } else {
            Ok(len.into())
        }
    }

    fn read_data(&mut self, len: u64) -> Result<Vec<u8>, Error> {
        let mut data = self.allocate_vec(len, size_of::<u8>())?;
        data.resize(len as usize, 0);
        self.reader.read_exact(&mut data)?;
        Ok(data)
    }

    fn seek_to_object(&mut self, object_ref: u64) -> Result<u64, Error> {
        let object_ref = u64_to_usize(object_ref).ok_or(Error::InvalidData)?;
        let offset = *self
            .object_offsets
            .get(object_ref)
            .ok_or(Error::InvalidData)?;
        Ok(self.reader.seek(SeekFrom::Start(offset))?)
    }

    fn push_stack_item_and_check_for_recursion(&mut self, item: StackItem) -> Result<(), Error> {
        let object_ref = u64_to_usize(item.object_ref).expect("internal consistency error");
        let is_on_stack = &mut self.object_on_stack[object_ref];
        if *is_on_stack {
            return Err(Error::InvalidData);
        }
        *is_on_stack = true;
        self.stack.push(item);
        Ok(())
    }

    fn pop_stack_item(&mut self) -> StackItem {
        let item = self.stack.pop().expect("internal consistency error");
        let object_ref = u64_to_usize(item.object_ref).expect("internal consistency error");
        self.object_on_stack[object_ref] = false;
        item
    }

    fn read_next(&mut self) -> Result<Option<Event>, Error> {
        let object_ref = if self.ref_size == 0 {
            // Initialise here rather than in new
            self.read_trailer()?;
            self.root_object
        } else {
            let maybe_object_ref = if let Some(stack_item) = self.stack.last_mut() {
                stack_item.child_object_refs.pop()
            } else {
                // Finished reading the plist
                return Ok(None);
            };

            if let Some(object_ref) = maybe_object_ref {
                object_ref
            } else {
                // We're at the end of an array or dict. Pop the top stack item and return.
                let stack_item = self.pop_stack_item();
                match stack_item.ty {
                    StackType::Array => return Ok(Some(Event::EndArray)),
                    StackType::Dict => return Ok(Some(Event::EndDictionary)),
                }
            }
        };

        self.seek_to_object(object_ref)?;

        let token = self.reader.read_u8()?;
        let ty = (token & 0xf0) >> 4;
        let size = token & 0x0f;

        let result = match (ty, size) {
            (0x0, 0x00) => return Err(Error::InvalidData), // null
            (0x0, 0x08) => Some(Event::BooleanValue(false)),
            (0x0, 0x09) => Some(Event::BooleanValue(true)),
            (0x0, 0x0f) => return Err(Error::InvalidData), // fill
            (0x1, 0) => Some(Event::IntegerValue(self.reader.read_u8()?.into())),
            (0x1, 1) => Some(Event::IntegerValue(
                self.reader.read_u16::<BigEndian>()?.into(),
            )),
            (0x1, 2) => Some(Event::IntegerValue(
                self.reader.read_u32::<BigEndian>()?.into(),
            )),
            (0x1, 3) => Some(Event::IntegerValue(self.reader.read_i64::<BigEndian>()?)),
            (0x1, 4) => return Err(Error::InvalidData), // 128 bit int
            (0x1, _) => return Err(Error::InvalidData), // variable length int
            (0x2, 2) => Some(Event::RealValue(
                self.reader.read_f32::<BigEndian>()?.into(),
            )),
            (0x2, 3) => Some(Event::RealValue(self.reader.read_f64::<BigEndian>()?)),
            (0x2, _) => return Err(Error::InvalidData), // odd length float
            (0x3, 3) => {
                // Date. Seconds since 1/1/2001 00:00:00.
                let secs = self.reader.read_f64::<BigEndian>()?;
                Some(Event::DateValue(
                    Date::from_seconds_since_plist_epoch(secs).map_err(|()| Error::InvalidData)?,
                ))
            }
            (0x4, n) => {
                // Data
                let len = self.read_object_len(n)?;
                Some(Event::DataValue(self.read_data(len)?))
            }
            (0x5, n) => {
                // ASCII string
                let len = self.read_object_len(n)?;
                let raw = self.read_data(len)?;
                let string = String::from_utf8(raw)?;
                Some(Event::StringValue(string))
            }
            (0x6, n) => {
                // UTF-16 string
                let len_utf16_codepoints = self.read_object_len(n)?;
                let mut raw_utf16 = self.allocate_vec(len_utf16_codepoints, size_of::<u16>())?;

                for _ in 0..len_utf16_codepoints {
                    raw_utf16.push(self.reader.read_u16::<BigEndian>()?);
                }

                let string = String::from_utf16(&raw_utf16)?;
                Some(Event::StringValue(string))
            }
            (0xa, n) => {
                // Array
                let len = self.read_object_len(n)?;
                let mut child_object_refs = self.read_refs(len)?;
                // Reverse so we can pop off the end of the stack in order
                child_object_refs.reverse();

                self.push_stack_item_and_check_for_recursion(StackItem {
                    object_ref,
                    ty: StackType::Array,
                    child_object_refs,
                })?;

                Some(Event::StartArray(Some(len)))
            }
            (0xd, n) => {
                // Dict
                let len = self.read_object_len(n)?;
                let key_refs = self.read_refs(len)?;
                let value_refs = self.read_refs(len)?;

                let mut child_object_refs = self.allocate_vec(len * 2, self.ref_size as usize)?;
                let len = key_refs.len();
                for i in 1..len + 1 {
                    // Reverse so we can pop off the end of the stack in order
                    child_object_refs.push(value_refs[len - i]);
                    child_object_refs.push(key_refs[len - i]);
                }

                self.push_stack_item_and_check_for_recursion(StackItem {
                    object_ref,
                    ty: StackType::Dict,
                    child_object_refs,
                })?;

                Some(Event::StartDictionary(Some(len as u64)))
            }
            (_, _) => return Err(Error::InvalidData),
        };

        Ok(result)
    }
}

impl<R: Read + Seek> Iterator for BinaryReader<R> {
    type Item = Result<Event, Error>;

    fn next(&mut self) -> Option<Result<Event, Error>> {
        match self.read_next() {
            Ok(Some(event)) => Some(Ok(event)),
            Err(err) => {
                // Mark the plist as finished
                self.stack.clear();
                Some(Err(err))
            }
            Ok(None) => None,
        }
    }
}

#[cfg(test)]
mod tests {
    use humantime::parse_rfc3339_weak;
    use std::fs::File;
    use std::path::Path;

    use super::*;
    use stream::Event;
    use stream::Event::*;

    #[test]
    fn streaming_parser() {
        let reader = File::open(&Path::new("./tests/data/binary.plist")).unwrap();
        let streaming_parser = BinaryReader::new(reader);
        let events: Vec<Event> = streaming_parser.map(|e| e.unwrap()).collect();

        let comparison = &[
            StartDictionary(Some(6)),
            StringValue("Lines".to_owned()),
            StartArray(Some(2)),
            StringValue("It is a tale told by an idiot,".to_owned()),
            StringValue("Full of sound and fury, signifying nothing.".to_owned()),
            EndArray,
            StringValue("Death".to_owned()),
            IntegerValue(1564),
            StringValue("Height".to_owned()),
            RealValue(1.60),
            StringValue("Birthdate".to_owned()),
            DateValue(parse_rfc3339_weak("1981-05-16 11:32:06").unwrap().into()),
            StringValue("Author".to_owned()),
            StringValue("William Shakespeare".to_owned()),
            StringValue("Data".to_owned()),
            DataValue(vec![0, 0, 0, 190, 0, 0, 0, 3, 0, 0, 0, 30, 0, 0, 0]),
            EndDictionary,
        ];

        assert_eq!(events, comparison);
    }

    #[test]
    fn utf16_plist() {
        let reader = File::open(&Path::new("./tests/data/utf16_bplist.plist")).unwrap();
        let streaming_parser = BinaryReader::new(reader);
        let mut events: Vec<Event> = streaming_parser.map(|e| e.unwrap()).collect();

        assert_eq!(events[2], StringValue("\u{2605} or better".to_owned()));

        let poem = if let StringValue(ref mut poem) = events[4] {
            poem
        } else {
            panic!("not a string")
        };
        assert_eq!(poem.len(), 643);
        assert_eq!(poem.pop().unwrap(), '\u{2605}');
    }
}
