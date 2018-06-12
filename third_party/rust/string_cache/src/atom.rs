// Copyright 2014 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(non_upper_case_globals)]

use phf_shared;
use serde::{Deserialize, Deserializer, Serialize, Serializer};

use std::borrow::Cow;
use std::cmp::Ordering::{self, Equal};
use std::fmt;
use std::hash::{Hash, Hasher};
use std::marker::PhantomData;
use std::mem;
use std::ops;
use std::slice;
use std::str;
use std::sync::Mutex;
use std::sync::atomic::AtomicIsize;
use std::sync::atomic::Ordering::SeqCst;

use shared::{STATIC_TAG, INLINE_TAG, DYNAMIC_TAG, TAG_MASK, MAX_INLINE_LEN, STATIC_SHIFT_BITS,
             ENTRY_ALIGNMENT, pack_static};
use self::UnpackedAtom::{Dynamic, Inline, Static};

#[cfg(feature = "log-events")]
use event::Event;

#[cfg(not(feature = "log-events"))]
macro_rules! log (($e:expr) => (()));

const NB_BUCKETS: usize = 1 << 12;  // 4096
const BUCKET_MASK: u64 = (1 << 12) - 1;

struct StringCache {
    buckets: [Option<Box<StringCacheEntry>>; NB_BUCKETS],
}

lazy_static! {
    static ref STRING_CACHE: Mutex<StringCache> = Mutex::new(StringCache::new());
}

struct StringCacheEntry {
    next_in_bucket: Option<Box<StringCacheEntry>>,
    hash: u64,
    ref_count: AtomicIsize,
    string: Box<str>,
}

impl StringCacheEntry {
    fn new(next: Option<Box<StringCacheEntry>>, hash: u64, string: String)
           -> StringCacheEntry {
        StringCacheEntry {
            next_in_bucket: next,
            hash: hash,
            ref_count: AtomicIsize::new(1),
            string: string.into_boxed_str(),
        }
    }
}

impl StringCache {
    fn new() -> StringCache {
        StringCache {
            buckets: unsafe { mem::zeroed() },
        }
    }

    fn add(&mut self, string: Cow<str>, hash: u64) -> *mut StringCacheEntry {
        let bucket_index = (hash & BUCKET_MASK) as usize;
        {
            let mut ptr: Option<&mut Box<StringCacheEntry>> =
                self.buckets[bucket_index].as_mut();

            while let Some(entry) = ptr.take() {
                if entry.hash == hash && &*entry.string == &*string {
                    if entry.ref_count.fetch_add(1, SeqCst) > 0 {
                        return &mut **entry;
                    }
                    // Uh-oh. The pointer's reference count was zero, which means someone may try
                    // to free it. (Naive attempts to defend against this, for example having the
                    // destructor check to see whether the reference count is indeed zero, don't
                    // work due to ABA.) Thus we need to temporarily add a duplicate string to the
                    // list.
                    entry.ref_count.fetch_sub(1, SeqCst);
                    break;
                }
                ptr = entry.next_in_bucket.as_mut();
            }
        }
        debug_assert!(mem::align_of::<StringCacheEntry>() >= ENTRY_ALIGNMENT);
        let string = string.into_owned();
        let _string_clone = if cfg!(feature = "log-events") {
            string.clone()
        } else {
            "".to_owned()
        };
        let mut entry = Box::new(StringCacheEntry::new(
            self.buckets[bucket_index].take(), hash, string));
        let ptr: *mut StringCacheEntry = &mut *entry;
        self.buckets[bucket_index] = Some(entry);
        log!(Event::Insert(ptr as u64, _string_clone));

        ptr
    }

    fn remove(&mut self, key: u64) {
        let ptr = key as *mut StringCacheEntry;
        let bucket_index = {
            let value: &StringCacheEntry = unsafe { &*ptr };
            debug_assert!(value.ref_count.load(SeqCst) == 0);
            (value.hash & BUCKET_MASK) as usize
        };


        let mut current: &mut Option<Box<StringCacheEntry>> = &mut self.buckets[bucket_index];

        loop {
            let entry_ptr: *mut StringCacheEntry = match current.as_mut() {
                Some(entry) => &mut **entry,
                None => break,
            };
            if entry_ptr == ptr {
                mem::drop(mem::replace(current, unsafe { (*entry_ptr).next_in_bucket.take() }));
                break;
            }
            current = unsafe { &mut (*entry_ptr).next_in_bucket };
        }

        log!(Event::Remove(key));
    }
}

/// A static `PhfStrSet`
///
/// This trait is implemented by static sets of interned strings generated using
/// `string_cache_codegen`, and `EmptyStaticAtomSet` for when strings will be added dynamically.
///
/// It is used by the methods of [`Atom`] to check if a string is present in the static set.
///
/// [`Atom`]: struct.Atom.html
pub trait StaticAtomSet {
    /// Get the location of the static string set in the binary.
    fn get() -> &'static PhfStrSet;
    /// Get the index of the empty string, which is in every set and is used for `Atom::default`.
    fn empty_string_index() -> u32;
}

/// A string set created using a [perfect hash function], specifically
/// [Hash, Displace and Compress].
///
/// See the CHD document for the meaning of the struct fields.
///
/// [perfect hash function]: https://en.wikipedia.org/wiki/Perfect_hash_function
/// [Hash, Displace and Compress]: http://cmph.sourceforge.net/papers/esa09.pdf
pub struct PhfStrSet {
    pub key: u64,
    pub disps: &'static [(u32, u32)],
    pub atoms: &'static [&'static str],
    pub hashes: &'static [u32],
}

/// An empty static atom set for when only dynamic strings will be added
pub struct EmptyStaticAtomSet;

impl StaticAtomSet for EmptyStaticAtomSet {
    fn get() -> &'static PhfStrSet {
        // The name is a lie: this set is not empty (it contains the empty string)
        // but that’s only to avoid divisions by zero in rust-phf.
        static SET: PhfStrSet = PhfStrSet {
            key: 0,
            disps: &[(0, 0)],
            atoms: &[""],
            // "" SipHash'd, and xored with u64_hash_to_u32.
            hashes: &[0x3ddddef3],
        };
        &SET
    }

    fn empty_string_index() -> u32 {
        0
    }
}

/// Use this if you don’t care about static atoms.
pub type DefaultAtom = Atom<EmptyStaticAtomSet>;

/// Represents a string that has been interned.
///
/// In reality this contains a complex packed datastructure and the methods to extract information
/// from it, along with type information to tell the compiler which static set it corresponds to.
pub struct Atom<Static: StaticAtomSet> {
    /// This field is public so that the `atom!()` macros can use it.
    /// You should not otherwise access this field.
    #[doc(hidden)]
    pub unsafe_data: u64,

    #[doc(hidden)]
    pub phantom: PhantomData<Static>,
}

impl<Static: StaticAtomSet> ::precomputed_hash::PrecomputedHash for Atom<Static> {
    fn precomputed_hash(&self) -> u32 {
        self.get_hash()
    }
}

impl<'a, Static: StaticAtomSet> From<&'a Atom<Static>> for Atom<Static> {
    fn from(atom: &'a Self) -> Self {
        atom.clone()
    }
}

fn u64_hash_as_u32(h: u64) -> u32 {
    // This may or may not be great...
    ((h >> 32) ^ h) as u32
}

impl<Static: StaticAtomSet> Atom<Static> {
    #[inline(always)]
    unsafe fn unpack(&self) -> UnpackedAtom {
        UnpackedAtom::from_packed(self.unsafe_data)
    }

    /// Get the hash of the string as it is stored in the set.
    pub fn get_hash(&self) -> u32 {
        match unsafe { self.unpack() } {
            Static(index) => {
                let static_set = Static::get();
                static_set.hashes[index as usize]
            }
            Dynamic(entry) => {
                let entry = entry as *mut StringCacheEntry;
                u64_hash_as_u32(unsafe { (*entry).hash })
            }
            Inline(..) => {
                u64_hash_as_u32(self.unsafe_data)
            }
        }
    }
}

impl<Static: StaticAtomSet> Default for Atom<Static> {
    #[inline]
    fn default() -> Self {
        Atom {
            unsafe_data: pack_static(Static::empty_string_index()),
            phantom: PhantomData
        }
    }
}

impl<Static: StaticAtomSet> Hash for Atom<Static> {
    #[inline]
    fn hash<H>(&self, state: &mut H) where H: Hasher {
        state.write_u32(self.get_hash())
    }
}

impl<Static: StaticAtomSet> Eq for Atom<Static> {}

// NOTE: This impl requires that a given string must always be interned the same way.
impl<Static: StaticAtomSet> PartialEq for Atom<Static> {
    #[inline]
    fn eq(&self, other: &Self) -> bool {
        self.unsafe_data == other.unsafe_data
    }
}

impl<Static: StaticAtomSet> PartialEq<str> for Atom<Static> {
    fn eq(&self, other: &str) -> bool {
        &self[..] == other
    }
}

impl<Static: StaticAtomSet> PartialEq<Atom<Static>> for str {
    fn eq(&self, other: &Atom<Static>) -> bool {
        self == &other[..]
    }
}

impl<Static: StaticAtomSet> PartialEq<String> for Atom<Static> {
    fn eq(&self, other: &String) -> bool {
        &self[..] == &other[..]
    }
}

impl<'a, Static: StaticAtomSet> From<Cow<'a, str>> for Atom<Static> {
    #[inline]
    fn from(string_to_add: Cow<'a, str>) -> Self {
        let static_set = Static::get();
        let hash = phf_shared::hash(&*string_to_add, static_set.key);
        let index = phf_shared::get_index(hash, static_set.disps, static_set.atoms.len());

        let unpacked = if static_set.atoms[index as usize] == string_to_add {
            Static(index)
        } else {
            let len = string_to_add.len();
            if len <= MAX_INLINE_LEN {
                let mut buf: [u8; 7] = [0; 7];
                buf[..len].copy_from_slice(string_to_add.as_bytes());
                Inline(len as u8, buf)
            } else {
                Dynamic(STRING_CACHE.lock().unwrap().add(string_to_add, hash) as *mut ())
            }
        };

        let data = unsafe { unpacked.pack() };
        log!(Event::Intern(data));
        Atom { unsafe_data: data, phantom: PhantomData }
    }
}

impl<'a, Static: StaticAtomSet> From<&'a str> for Atom<Static> {
    #[inline]
    fn from(string_to_add: &str) -> Self {
        Atom::from(Cow::Borrowed(string_to_add))
    }
}

impl<Static: StaticAtomSet> From<String> for Atom<Static> {
    #[inline]
    fn from(string_to_add: String) -> Self {
        Atom::from(Cow::Owned(string_to_add))
    }
}

impl<Static: StaticAtomSet> Clone for Atom<Static> {
    #[inline(always)]
    fn clone(&self) -> Self {
        unsafe {
            match from_packed_dynamic(self.unsafe_data) {
                Some(entry) => {
                    let entry = entry as *mut StringCacheEntry;
                    (*entry).ref_count.fetch_add(1, SeqCst);
                },
                None => (),
            }
        }
        Atom {
            unsafe_data: self.unsafe_data,
            phantom: PhantomData,
        }
    }
}

impl<Static: StaticAtomSet> Drop for Atom<Static> {
    #[inline]
    fn drop(&mut self) {
        // Out of line to guide inlining.
        fn drop_slow<Static: StaticAtomSet>(this: &mut Atom<Static>) {
            STRING_CACHE.lock().unwrap().remove(this.unsafe_data);
        }

        unsafe {
            match from_packed_dynamic(self.unsafe_data) {
                Some(entry) => {
                    let entry = entry as *mut StringCacheEntry;
                    if (*entry).ref_count.fetch_sub(1, SeqCst) == 1 {
                        drop_slow(self);
                    }
                }
                _ => (),
            }
        }
    }
}

impl<Static: StaticAtomSet> ops::Deref for Atom<Static> {
    type Target = str;

    #[inline]
    fn deref(&self) -> &str {
        unsafe {
            match self.unpack() {
                Inline(..) => {
                    let buf = inline_orig_bytes(&self.unsafe_data);
                    str::from_utf8_unchecked(buf)
                },
                Static(idx) => Static::get().atoms.get(idx as usize).expect("bad static atom"),
                Dynamic(entry) => {
                    let entry = entry as *mut StringCacheEntry;
                    &(*entry).string
                }
            }
        }
    }
}

impl<Static: StaticAtomSet> fmt::Display for Atom<Static> {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        <str as fmt::Display>::fmt(self, f)
    }
}

impl<Static: StaticAtomSet> fmt::Debug for Atom<Static> {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let ty_str = unsafe {
            match self.unpack() {
                Dynamic(..) => "dynamic",
                Inline(..) => "inline",
                Static(..) => "static",
            }
        };

        write!(f, "Atom('{}' type={})", &*self, ty_str)
    }
}

impl<Static: StaticAtomSet> PartialOrd for Atom<Static> {
    #[inline]
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        if self.unsafe_data == other.unsafe_data {
            return Some(Equal);
        }
        self.as_ref().partial_cmp(other.as_ref())
    }
}

impl<Static: StaticAtomSet> Ord for Atom<Static> {
    #[inline]
    fn cmp(&self, other: &Self) -> Ordering {
        if self.unsafe_data == other.unsafe_data {
            return Equal;
        }
        self.as_ref().cmp(other.as_ref())
    }
}

impl<Static: StaticAtomSet> AsRef<str> for Atom<Static> {
    fn as_ref(&self) -> &str {
        &self
    }
}

impl<Static: StaticAtomSet> Serialize for Atom<Static> {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: Serializer {
        let string: &str = self.as_ref();
        string.serialize(serializer)
    }
}

impl<'a, Static: StaticAtomSet> Deserialize<'a> for Atom<Static> {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: Deserializer<'a> {
        let string: String = try!(Deserialize::deserialize(deserializer));
        Ok(Atom::from(string))
    }
}

// AsciiExt requires mutating methods, so we just implement the non-mutating ones.
// We don't need to implement is_ascii because there's no performance improvement
// over the one from &str.
impl<Static: StaticAtomSet> Atom<Static> {
    fn from_mutated_str<F: FnOnce(&mut str)>(s: &str, f: F) -> Self {
        let mut buffer: [u8; 64] = unsafe { mem::uninitialized() };
        if let Some(buffer_prefix) = buffer.get_mut(..s.len()) {
            buffer_prefix.copy_from_slice(s.as_bytes());
            let as_str = unsafe { ::std::str::from_utf8_unchecked_mut(buffer_prefix) };
            f(as_str);
            Atom::from(&*as_str)
        } else {
            let mut string = s.to_owned();
            f(&mut string);
            Atom::from(string)
        }
    }

    /// Like [`to_ascii_uppercase`].
    ///
    /// [`to_ascii_uppercase`]: https://doc.rust-lang.org/std/ascii/trait.AsciiExt.html#tymethod.to_ascii_uppercase
    pub fn to_ascii_uppercase(&self) -> Self {
        for (i, b) in self.bytes().enumerate() {
            if let b'a' ... b'z' = b {
                return Atom::from_mutated_str(self, |s| s[i..].make_ascii_uppercase())
            }
        }
        self.clone()
    }

    /// Like [`to_ascii_lowercase`].
    ///
    /// [`to_ascii_lowercase`]: https://doc.rust-lang.org/std/ascii/trait.AsciiExt.html#tymethod.to_ascii_lowercase
    pub fn to_ascii_lowercase(&self) -> Self {
        for (i, b) in self.bytes().enumerate() {
            if let b'A' ... b'Z' = b {
                return Atom::from_mutated_str(self, |s| s[i..].make_ascii_lowercase())
            }
        }
        self.clone()
    }

    /// Like [`eq_ignore_ascii_case`].
    ///
    /// [`eq_ignore_ascii_case`]: https://doc.rust-lang.org/std/ascii/trait.AsciiExt.html#tymethod.eq_ignore_ascii_case
    pub fn eq_ignore_ascii_case(&self, other: &Self) -> bool {
        (self == other) || self.eq_str_ignore_ascii_case(&**other)
    }

    /// Like [`eq_ignore_ascii_case`], but takes an unhashed string as `other`.
    ///
    /// [`eq_ignore_ascii_case`]: https://doc.rust-lang.org/std/ascii/trait.AsciiExt.html#tymethod.eq_ignore_ascii_case
    pub fn eq_str_ignore_ascii_case(&self, other: &str) -> bool {
        (&**self).eq_ignore_ascii_case(other)
    }
}

// Atoms use a compact representation which fits this enum in a single u64.
// Inlining avoids actually constructing the unpacked representation in memory.
#[allow(missing_copy_implementations)]
enum UnpackedAtom {
    /// Pointer to a dynamic table entry.  Must be 16-byte aligned!
    Dynamic(*mut ()),

    /// Length + bytes of string.
    Inline(u8, [u8; 7]),

    /// Index in static interning table.
    Static(u32),
}

#[inline(always)]
fn inline_atom_slice(x: &u64) -> &[u8] {
    unsafe {
        let x: *const u64 = x;
        let mut data = x as *const u8;
        // All except the lowest byte, which is first in little-endian, last in big-endian.
        if cfg!(target_endian = "little") {
            data = data.offset(1);
        }
        let len = 7;
        slice::from_raw_parts(data, len)
    }
}

#[inline(always)]
fn inline_atom_slice_mut(x: &mut u64) -> &mut [u8] {
    unsafe {
        let x: *mut u64 = x;
        let mut data = x as *mut u8;
        // All except the lowest byte, which is first in little-endian, last in big-endian.
        if cfg!(target_endian = "little") {
            data = data.offset(1);
        }
        let len = 7;
        slice::from_raw_parts_mut(data, len)
    }
}

impl UnpackedAtom {
    /// Pack a key, fitting it into a u64 with flags and data. See `string_cache_shared` for
    /// hints for the layout.
    #[inline(always)]
    unsafe fn pack(self) -> u64 {
        match self {
            Static(n) => pack_static(n),
            Dynamic(p) => {
                let n = p as u64;
                debug_assert!(0 == n & TAG_MASK);
                n
            }
            Inline(len, buf) => {
                debug_assert!((len as usize) <= MAX_INLINE_LEN);
                let mut data: u64 = (INLINE_TAG as u64) | ((len as u64) << 4);
                {
                    let dest = inline_atom_slice_mut(&mut data);
                    dest.copy_from_slice(&buf)
                }
                data
            }
        }
    }

    /// Unpack a key, extracting information from a single u64 into useable structs.
    #[inline(always)]
    unsafe fn from_packed(data: u64) -> UnpackedAtom {
        debug_assert!(DYNAMIC_TAG == 0); // Dynamic is untagged

        match (data & TAG_MASK) as u8 {
            DYNAMIC_TAG => Dynamic(data as *mut ()),
            STATIC_TAG => Static((data >> STATIC_SHIFT_BITS) as u32),
            INLINE_TAG => {
                let len = ((data & 0xf0) >> 4) as usize;
                debug_assert!(len <= MAX_INLINE_LEN);
                let mut buf: [u8; 7] = [0; 7];
                let src = inline_atom_slice(&data);
                buf.copy_from_slice(src);
                Inline(len as u8, buf)
            },
            _ => debug_unreachable!(),
        }
    }
}

/// Used for a fast path in Clone and Drop.
#[inline(always)]
unsafe fn from_packed_dynamic(data: u64) -> Option<*mut ()> {
    if (DYNAMIC_TAG as u64) == (data & TAG_MASK) {
        Some(data as *mut ())
    } else {
        None
    }
}

/// For as_slice on inline atoms, we need a pointer into the original
/// string contents.
///
/// It's undefined behavior to call this on a non-inline atom!!
#[inline(always)]
unsafe fn inline_orig_bytes<'a>(data: &'a u64) -> &'a [u8] {
    match UnpackedAtom::from_packed(*data) {
        Inline(len, _) => {
            let src = inline_atom_slice(&data);
            &src[..(len as usize)]
        }
        _ => debug_unreachable!(),
    }
}

#[cfg(test)]
#[macro_use]
mod tests {
    use std::mem;
    use std::thread;
    use super::{StaticAtomSet, StringCacheEntry};
    use super::UnpackedAtom::{Dynamic, Inline, Static};
    use shared::ENTRY_ALIGNMENT;

    include!(concat!(env!("OUT_DIR"), "/test_atom.rs"));
    pub type Atom = TestAtom;

    #[test]
    fn test_as_slice() {
        let s0 = Atom::from("");
        assert!(s0.as_ref() == "");

        let s1 = Atom::from("class");
        assert!(s1.as_ref() == "class");

        let i0 = Atom::from("blah");
        assert!(i0.as_ref() == "blah");

        let s0 = Atom::from("BLAH");
        assert!(s0.as_ref() == "BLAH");

        let d0 = Atom::from("zzzzzzzzzz");
        assert!(d0.as_ref() == "zzzzzzzzzz");

        let d1 = Atom::from("ZZZZZZZZZZ");
        assert!(d1.as_ref() == "ZZZZZZZZZZ");
    }

    macro_rules! unpacks_to (($e:expr, $t:pat) => (
        match unsafe { Atom::from($e).unpack() } {
            $t => (),
            _ => panic!("atom has wrong type"),
        }
    ));

    #[test]
    fn test_types() {
        unpacks_to!("", Static(..));
        unpacks_to!("id", Static(..));
        unpacks_to!("body", Static(..));
        unpacks_to!("c", Inline(..)); // "z" is a static atom
        unpacks_to!("zz", Inline(..));
        unpacks_to!("zzz", Inline(..));
        unpacks_to!("zzzz", Inline(..));
        unpacks_to!("zzzzz", Inline(..));
        unpacks_to!("zzzzzz", Inline(..));
        unpacks_to!("zzzzzzz", Inline(..));
        unpacks_to!("zzzzzzzz", Dynamic(..));
        unpacks_to!("zzzzzzzzzzzzz", Dynamic(..));
    }

    #[test]
    fn test_equality() {
        let s0 = Atom::from("fn");
        let s1 = Atom::from("fn");
        let s2 = Atom::from("loop");

        let i0 = Atom::from("blah");
        let i1 = Atom::from("blah");
        let i2 = Atom::from("blah2");

        let d0 = Atom::from("zzzzzzzz");
        let d1 = Atom::from("zzzzzzzz");
        let d2 = Atom::from("zzzzzzzzz");

        assert!(s0 == s1);
        assert!(s0 != s2);

        assert!(i0 == i1);
        assert!(i0 != i2);

        assert!(d0 == d1);
        assert!(d0 != d2);

        assert!(s0 != i0);
        assert!(s0 != d0);
        assert!(i0 != d0);
    }

    #[test]
    fn default() {
        assert_eq!(TestAtom::default(), test_atom!(""));
        assert_eq!(&*TestAtom::default(), "");
    }

    #[test]
    fn ord() {
        fn check(x: &str, y: &str) {
            assert_eq!(x < y, Atom::from(x) < Atom::from(y));
            assert_eq!(x.cmp(y), Atom::from(x).cmp(&Atom::from(y)));
            assert_eq!(x.partial_cmp(y), Atom::from(x).partial_cmp(&Atom::from(y)));
        }

        check("a", "body");
        check("asdf", "body");
        check("zasdf", "body");
        check("z", "body");

        check("a", "bbbbb");
        check("asdf", "bbbbb");
        check("zasdf", "bbbbb");
        check("z", "bbbbb");
    }

    #[test]
    fn clone() {
        let s0 = Atom::from("fn");
        let s1 = s0.clone();
        let s2 = Atom::from("loop");

        let i0 = Atom::from("blah");
        let i1 = i0.clone();
        let i2 = Atom::from("blah2");

        let d0 = Atom::from("zzzzzzzz");
        let d1 = d0.clone();
        let d2 = Atom::from("zzzzzzzzz");

        assert!(s0 == s1);
        assert!(s0 != s2);

        assert!(i0 == i1);
        assert!(i0 != i2);

        assert!(d0 == d1);
        assert!(d0 != d2);

        assert!(s0 != i0);
        assert!(s0 != d0);
        assert!(i0 != d0);
    }

    macro_rules! assert_eq_fmt (($fmt:expr, $x:expr, $y:expr) => ({
        let x = $x;
        let y = $y;
        if x != y {
            panic!("assertion failed: {} != {}",
                format_args!($fmt, x),
                format_args!($fmt, y));
        }
    }));

    #[test]
    fn repr() {
        fn check(s: &str, data: u64) {
            assert_eq_fmt!("0x{:016X}", Atom::from(s).unsafe_data, data);
        }

        fn check_static(s: &str, x: Atom) {
            assert_eq_fmt!("0x{:016X}", x.unsafe_data, Atom::from(s).unsafe_data);
            assert_eq!(0x2, x.unsafe_data & 0xFFFF_FFFF);
            // The index is unspecified by phf.
            assert!((x.unsafe_data >> 32) <= TestAtomStaticSet::get().atoms.len() as u64);
        }

        // This test is here to make sure we don't change atom representation
        // by accident.  It may need adjusting if there are changes to the
        // static atom table, the tag values, etc.

        // Static atoms
        check_static("a",       test_atom!("a"));
        check_static("address", test_atom!("address"));
        check_static("area",    test_atom!("area"));

        // Inline atoms
        check("e",       0x0000_0000_0000_6511);
        check("xyzzy",   0x0000_797A_7A79_7851);
        check("xyzzy01", 0x3130_797A_7A79_7871);

        // Dynamic atoms. This is a pointer so we can't verify every bit.
        assert_eq!(0x00, Atom::from("a dynamic string").unsafe_data & 0xf);
    }

    #[test]
    fn assert_sizes() {
        use std::mem;
        struct EmptyWithDrop;
        impl Drop for EmptyWithDrop {
            fn drop(&mut self) {}
        }
        let compiler_uses_inline_drop_flags = mem::size_of::<EmptyWithDrop>() > 0;

        // Guard against accidental changes to the sizes of things.
        assert_eq!(mem::size_of::<Atom>(),
                   if compiler_uses_inline_drop_flags { 16 } else { 8 });
        assert_eq!(mem::size_of::<super::StringCacheEntry>(),
                   8 + 4 * mem::size_of::<usize>());
    }

    #[test]
    fn test_threads() {
        for _ in 0_u32..100 {
            thread::spawn(move || {
                let _ = Atom::from("a dynamic string");
                let _ = Atom::from("another string");
            });
        }
    }

    #[test]
    fn atom_macro() {
        assert_eq!(test_atom!("body"), Atom::from("body"));
        assert_eq!(test_atom!("font-weight"), Atom::from("font-weight"));
    }

    #[test]
    fn match_atom() {
        assert_eq!(2, match Atom::from("head") {
            test_atom!("br") => 1,
            test_atom!("html") | test_atom!("head") => 2,
            _ => 3,
        });

        assert_eq!(3, match Atom::from("body") {
            test_atom!("br") => 1,
            test_atom!("html") | test_atom!("head") => 2,
            _ => 3,
        });

        assert_eq!(3, match Atom::from("zzzzzz") {
            test_atom!("br") => 1,
            test_atom!("html") | test_atom!("head") => 2,
            _ => 3,
        });
    }

    #[test]
    fn ensure_deref() {
        // Ensure we can Deref to a &str
        let atom = Atom::from("foobar");
        let _: &str = &atom;
    }

    #[test]
    fn ensure_as_ref() {
        // Ensure we can as_ref to a &str
        let atom = Atom::from("foobar");
        let _: &str = atom.as_ref();
    }

    #[test]
    fn string_cache_entry_alignment_is_sufficient() {
        assert!(mem::align_of::<StringCacheEntry>() >= ENTRY_ALIGNMENT);
    }

    #[test]
    fn test_ascii_lowercase() {
        assert_eq!(Atom::from("").to_ascii_lowercase(), Atom::from(""));
        assert_eq!(Atom::from("aZ9").to_ascii_lowercase(), Atom::from("az9"));
        assert_eq!(Atom::from("The Quick Brown Fox!").to_ascii_lowercase(), Atom::from("the quick brown fox!"));
        assert_eq!(Atom::from("JE VAIS À PARIS").to_ascii_lowercase(), Atom::from("je vais À paris"));
    }

    #[test]
    fn test_ascii_uppercase() {
        assert_eq!(Atom::from("").to_ascii_uppercase(), Atom::from(""));
        assert_eq!(Atom::from("aZ9").to_ascii_uppercase(), Atom::from("AZ9"));
        assert_eq!(Atom::from("The Quick Brown Fox!").to_ascii_uppercase(), Atom::from("THE QUICK BROWN FOX!"));
        assert_eq!(Atom::from("Je vais à Paris").to_ascii_uppercase(), Atom::from("JE VAIS à PARIS"));
    }

    #[test]
    fn test_eq_ignore_ascii_case() {
        assert!(Atom::from("").eq_ignore_ascii_case(&Atom::from("")));
        assert!(Atom::from("aZ9").eq_ignore_ascii_case(&Atom::from("aZ9")));
        assert!(Atom::from("aZ9").eq_ignore_ascii_case(&Atom::from("Az9")));
        assert!(Atom::from("The Quick Brown Fox!").eq_ignore_ascii_case(&Atom::from("THE quick BROWN fox!")));
        assert!(Atom::from("Je vais à Paris").eq_ignore_ascii_case(&Atom::from("je VAIS à PARIS")));
        assert!(!Atom::from("").eq_ignore_ascii_case(&Atom::from("az9")));
        assert!(!Atom::from("aZ9").eq_ignore_ascii_case(&Atom::from("")));
        assert!(!Atom::from("aZ9").eq_ignore_ascii_case(&Atom::from("9Za")));
        assert!(!Atom::from("The Quick Brown Fox!").eq_ignore_ascii_case(&Atom::from("THE quick BROWN fox!!")));
        assert!(!Atom::from("Je vais à Paris").eq_ignore_ascii_case(&Atom::from("JE vais À paris")));
    }

    #[test]
    fn test_from_string() {
        assert!(Atom::from("camembert".to_owned()) == Atom::from("camembert"));
    }
}

#[cfg(all(test, feature = "unstable"))]
#[path = "bench.rs"]
mod bench;
