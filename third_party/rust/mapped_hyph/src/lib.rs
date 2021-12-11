// Copyright 2019 Mozilla Foundation. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#[macro_use]
extern crate arrayref;
extern crate memmap2;
#[macro_use]
extern crate log;

use std::slice;
use std::str;
use std::cmp::max;
use std::fs::File;
use std::mem;

use memmap2::Mmap;

// Make submodules available publicly.
pub mod builder;
pub mod ffi;

// 4-byte identification expected at beginning of a compiled dictionary file.
// (This will be updated if an incompatible change to the format is made in
// some future revision.)
const MAGIC_NUMBER: [u8; 4] = [b'H', b'y', b'f', b'0'];

const INVALID_STRING_OFFSET: u16 = 0xffff;
const INVALID_STATE_OFFSET: u32 = 0x00ff_ffff;

const FILE_HEADER_SIZE: usize = 8; // 4-byte magic number, 4-byte count of levels
const LEVEL_HEADER_SIZE: usize = 16;

// Transition actually holds a 24-bit new state offset and an 8-bit input byte
// to match. We will be interpreting byte ranges as Transition arrays (in the
// State::transitions() method below), so use repr(C) to ensure we have the
// memory layout we expect.
// Transition records do not depend on any specific alignment.
#[repr(C)]
#[derive(Debug,Copy,Clone)]
struct Transition(u8, u8, u8, u8);

impl Transition {
    fn new_state_offset(&self) -> usize {
        // Read a 24-bit little-endian number from three bytes.
        self.0 as usize + ((self.1 as usize) << 8) + ((self.2 as usize) << 16)
    }
    fn match_byte(&self) -> u8 {
        self.3
    }
}

// State is an area of the Level's data block that begins with a fixed header,
// followed by an array of transitions. The total size of each State's data
// depends on the number of transitions in the state. Only the basic header
// is defined by the struct here; the rest of the state is accessed via
// pointer magic.
// There are two versions of State, a basic version that supports only simple
// hyphenation (no associated spelling change), and an extended version that
// adds the replacement-string fields to support spelling changes at the
// hyphenation point. Check is_extended() to know which version is present.
// State records are NOT necessarily 4-byte aligned, so multi-byte fields
// should be read with care.
#[derive(Debug,Copy,Clone)]
#[repr(C)]
struct State {
    fallback_state: [u8; 4],
    match_string_offset: [u8; 2],
    num_transitions: u8,
    is_extended: u8,
}

#[repr(C)]
struct StateExtended {
    state: State,
    repl_string_offset: [u8; 2],
    repl_index: i8,
    repl_cut: i8,
}

impl State {
    // Accessors for the various State header fields; see file format description.
    fn fallback_state(&self) -> usize {
        u32::from_le_bytes(self.fallback_state) as usize
    }
    fn match_string_offset(&self) -> usize {
        u16::from_le_bytes(self.match_string_offset) as usize
    }
    fn num_transitions(&self) -> u8 {
        self.num_transitions
    }
    fn is_extended(&self) -> bool {
        self.is_extended != 0
    }
    // Accessors that are only valid if is_extended() is true.
    // These use `unsafe` to dereference a pointer to the relevant field;
    // this is OK because Level::get_state always validates the total state size
    // before returning a state reference, so these pointers will be valid for
    // any extended state it returns.
    #[allow(dead_code)]
    fn as_extended(&self) -> &StateExtended {
        debug_assert!(self.is_extended());
        unsafe { mem::transmute(self) }
    }
    #[allow(dead_code)]
    fn repl_string_offset(&self) -> usize {
        u16::from_le_bytes(self.as_extended().repl_string_offset) as usize
    }
    #[allow(dead_code)]
    fn repl_index(&self) -> i8 {
        self.as_extended().repl_index
    }
    #[allow(dead_code)]
    fn repl_cut(&self) -> i8 {
        self.as_extended().repl_cut
    }
    // Return the state's Transitions as a slice reference.
    fn transitions(&self) -> &[Transition] {
        let count = self.num_transitions() as usize;
        if count == 0 {
            return &[];
        }
        let transition_offset = if self.is_extended() { mem::size_of::<StateExtended>() } else { mem::size_of::<State>() } as isize;
        // We know the `offset` here will not look beyond the valid range of memory
        // because Level::get_state() checks the state length (accounting for the
        // number of transitions) before returning a State reference.
        let trans_ptr = unsafe { (self as *const State as *const u8).offset(transition_offset) as *const Transition };
        // Again, because Level::get_state() already checked the state length, we know
        // this slice address and count will be valid.
        unsafe { slice::from_raw_parts(trans_ptr, count) }
    }
    // Look up the Transition for a given input byte, or None.
    fn transition_for(&self, b: u8) -> Option<Transition> {
        // The transitions array is sorted by match_byte() value, but there are
        // usually very few entries; benchmarking showed that using binary_search_by
        // here gave no benefit (possibly slightly slower).
        self.transitions().iter().copied().find(|t| t.match_byte() == b)
    }
    // Just for debugging use...
    #[allow(dead_code)]
    fn deep_show(&self, prefix: &str, dic: &Level) {
        if self.match_string_offset() != INVALID_STRING_OFFSET as usize {
            let match_string = dic.string_at_offset(self.match_string_offset());
            println!("{}match: {}", prefix, str::from_utf8(match_string).unwrap());
        }
        for t in self.transitions() {
            println!("{}{} ->", prefix, t.match_byte() as char);
            let next_prefix = format!("{}  ", prefix);
            dic.get_state(t.new_state_offset()).unwrap().deep_show(&next_prefix, &dic);
        }
    }
}

// We count the presentation-form ligature characters U+FB00..FB06 as multiple
// chars for the purposes of lefthyphenmin/righthyphenmin. In UTF-8, all these
// ligature characters are 3-byte sequences beginning with <0xEF, 0xAC>; this
// helper returns the "decomposed length" of the ligature given its trailing
// byte.
fn lig_length(trail_byte: u8) -> usize {
    // This is only called on valid UTF-8 where we already know trail_byte
    // must be >= 0x80.
    // Ligature lengths:       ff   fi   fl   ffi  ffl  long-st  st
    const LENGTHS: [u8; 7] = [ 2u8, 2u8, 2u8, 3u8, 3u8, 2u8,     2u8 ];
    if trail_byte > 0x86 {
        return 1;
    }
    LENGTHS[trail_byte as usize - 0x80] as usize
}

fn is_utf8_trail_byte(byte: u8) -> bool {
    (byte & 0xC0) == 0x80
}

fn is_ascii_digit(byte: u8) -> bool {
    byte <= b'9' && byte >= b'0'
}

fn is_odd(byte: u8) -> bool {
    (byte & 0x01) == 0x01
}

// A hyphenation Level has a header followed by State records and packed string
// data. The total size of the slice depends on the number and size of the
// States and Strings it contains.
// Note that the data of the Level may not have any specific alignment!
#[derive(Debug,Copy,Clone)]
struct Level<'a> {
    data: &'a [u8],
    // Header fields cached by the constructor for faster access:
    state_data_base_: usize,
    string_data_base_: usize,
}

impl Level<'_> {
    // Constructor that initializes our cache variables.
    fn new(data: &[u8]) -> Level {
        Level {
            data,
            state_data_base_: u32::from_le_bytes(*array_ref!(data, 0, 4)) as usize,
            string_data_base_: u32::from_le_bytes(*array_ref!(data, 4, 4)) as usize,
        }
    }

    // Accessors for Level header fields; see file format description.
    fn state_data_base(&self) -> usize {
        self.state_data_base_ // cached by constructor
    }
    fn string_data_base(&self) -> usize {
        self.string_data_base_ // cached by constructor
    }
    fn nohyphen_string_offset(&self) -> usize {
        u16::from_le_bytes(*array_ref!(self.data, 8, 2)) as usize
    }
    #[allow(dead_code)]
    fn nohyphen_count(&self) -> u16 {
        u16::from_le_bytes(*array_ref!(self.data, 10, 2))
    }
    fn lh_min(&self) -> usize {
        max(1, self.data[12] as usize)
    }
    fn rh_min(&self) -> usize {
        max(1, self.data[13] as usize)
    }
    fn clh_min(&self) -> usize {
        max(1, self.data[14] as usize)
    }
    fn crh_min(&self) -> usize {
        max(1, self.data[15] as usize)
    }
    fn word_boundary_mins(&self) -> (usize, usize, usize, usize) {
        (self.lh_min(), self.rh_min(), self.clh_min(), self.crh_min())
    }
    // Strings are represented as offsets from the Level's string_data_base.
    // This returns a byte slice referencing the string at a given offset,
    // or an empty slice if invalid.
    fn string_at_offset(&self, offset: usize) -> &'_ [u8] {
        if offset == INVALID_STRING_OFFSET as usize {
            return &[];
        }
        let string_base = self.string_data_base() as usize + offset;
        // TODO: move this to the validation function.
        debug_assert!(string_base < self.data.len());
        if string_base + 1 > self.data.len() {
            return &[];
        }
        let len = self.data[string_base] as usize;
        // TODO: move this to the validation function.
        debug_assert!(string_base + 1 + len <= self.data.len());
        if string_base + 1 + len > self.data.len() {
            return &[];
        }
        self.data.get(string_base + 1 .. string_base + 1 + len).unwrap()
    }
    // The nohyphen field actually contains multiple NUL-separated substrings;
    // return them as a vector of individual byte slices.
    fn nohyphen(&self) -> Vec<&[u8]> {
        let string_offset = self.nohyphen_string_offset();
        let nohyph_str = self.string_at_offset(string_offset as usize);
        if nohyph_str.is_empty() {
            return vec![];
        }
        nohyph_str.split(|&b| b == 0).collect()
    }
    // States are represented as an offset from the Level's state_data_base.
    // This returns a reference to the State at a given offset, or None if invalid.
    fn get_state(&self, offset: usize) -> Option<&State> {
        if offset == INVALID_STATE_OFFSET as usize {
            return None;
        }
        debug_assert_eq!(offset & 3, 0);
        let state_base = self.state_data_base() + offset;
        // TODO: move this to the validation function.
        debug_assert!(state_base + mem::size_of::<State>() <= self.string_data_base());
        if state_base + mem::size_of::<State>() > self.string_data_base() {
            return None;
        }
        let state_ptr = &self.data[state_base] as *const u8 as *const State;
        // This is safe because we just checked against self.string_data_base() above.
        let state = unsafe { state_ptr.as_ref().unwrap() };
        let length = if state.is_extended() { mem::size_of::<StateExtended>() } else { mem::size_of::<State>() }
                + mem::size_of::<Transition>() * state.num_transitions() as usize;
        // TODO: move this to the validation function.
        debug_assert!(state_base + length <= self.string_data_base());
        if state_base + length > self.string_data_base() {
            return None;
        }
        // This is safe because we checked the full state length against self.string_data_base().
        unsafe { state_ptr.as_ref() }
    }
    // Sets hyphenation values (odd = potential break, even = no break) in values[],
    // and returns the change in the number of odd values present, so the caller can
    // keep track of the total number of potential breaks in the word.
    fn find_hyphen_values(&self, word: &str, values: &mut [u8], lh_min: usize, rh_min: usize) -> isize {
        // Bail out immediately if the word is too short to hyphenate.
        if word.len() < lh_min + rh_min {
            return 0;
        }
        let start_state = self.get_state(0);
        let mut st = start_state;
        let mut hyph_count = 0;
        for i in 0 .. word.len() + 2 {
            // Loop over the word by bytes, with a virtual '.' added at each end
            // to match word-boundary patterns.
            let b = if i == 0 || i == word.len() + 1 { b'.' } else { word.as_bytes()[i - 1] };
            loop {
                // Loop to repeatedly fall back if we don't find a matching transition.
                // Note that this could infinite-loop if there is a state whose fallback
                // points to itself (or a cycle of fallbacks), but this would represent
                // a table compilation error.
                // (A potential validation function could check for fallback cycles.)
                if st.is_none() {
                    st = start_state;
                    break;
                }
                let state = st.unwrap();
                if let Some(tr) = state.transition_for(b) {
                    // Found a transition for the current byte. Look up the new state;
                    // if it has a match_string, merge its weights into `values`.
                    st = self.get_state(tr.new_state_offset());
                    if let Some(state) = st {
                        let match_offset = state.match_string_offset();
                        if match_offset != INVALID_STRING_OFFSET as usize {
                            if state.is_extended() {
                                debug_assert!(false, "extended hyphenation not supported by this function");
                            } else {
                                let match_str = self.string_at_offset(match_offset);
                                let offset = i + 1 - match_str.len();
                                assert!(offset + match_str.len() <= word.len() + 2);
                                for (j, ch) in match_str.iter().enumerate() {
                                    let index = offset + j;
                                    if index >= lh_min && index <= word.len() - rh_min {
                                        // lh_min and rh_min are guaranteed to be >= 1,
                                        // so this will not try to access outside values[].
                                        let old_value = values[index - 1];
                                        let value = ch - b'0';
                                        if value > old_value {
                                            if is_odd(old_value) != is_odd(value) {
                                                // Adjust hyph_count for the change we're making
                                                hyph_count += if is_odd(value) { 1 } else { -1 };
                                            }
                                            values[index - 1] = value;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    // We have handled the current input byte; leave the fallback loop
                    // and get next input.
                    break;
                }
                // No transition for the current byte; go to fallback state and try again.
                st = self.get_state(state.fallback_state());
            }
        }

        // If the word was not purely ASCII, or if the word begins/ends with
        // digits, the use of lh_min and rh_min above may not have correctly
        // excluded enough positions, so we need to fix things up here.
        let mut index = 0;
        let mut count = 0;
        let word_bytes = word.as_bytes();
        let mut clear_hyphen_at = |i| { if is_odd(values[i]) { hyph_count -= 1; } values[i] = 0; };
        // Handle lh_min.
        while count < lh_min - 1 && index < word_bytes.len() {
            let byte = word_bytes[index];
            clear_hyphen_at(index);
            if byte < 0x80 {
                index += 1;
                if is_ascii_digit(byte) {
                    continue; // ASCII digits don't count
                }
            } else if byte == 0xEF && word_bytes[index + 1] == 0xAC {
                // Unicode presentation-form ligature characters, which we count as
                // multiple chars for the purpose of lh_min/rh_min, all begin with
                // 0xEF, 0xAC in UTF-8.
                count += lig_length(word_bytes[index + 2]);
                clear_hyphen_at(index + 1);
                clear_hyphen_at(index + 2);
                index += 3;
                continue;
            } else {
                index += 1;
                while index < word_bytes.len() && is_utf8_trail_byte(word_bytes[index])  {
                    clear_hyphen_at(index);
                    index += 1;
                }
            }
            count += 1;
        }

        // Handle rh_min.
        count = 0;
        index = word.len();
        while count < rh_min && index > 0 {
            index -= 1;
            let byte = word_bytes[index];
            if index < word.len() - 1 {
                clear_hyphen_at(index);
            }
            if byte < 0x80 {
                // Only count if not an ASCII digit
                if !is_ascii_digit(byte) {
                    count += 1;
                }
                continue;
            }
            if is_utf8_trail_byte(byte) {
                continue;
            }
            if byte == 0xEF && word_bytes[index + 1] == 0xAC {
                // Presentation-form ligatures count as multiple chars.
                count += lig_length(word_bytes[index + 2]);
                continue;
            }
            count += 1;
        }

        hyph_count
    }
}

/// Hyphenation engine encapsulating a language-specific set of patterns (rules)
/// that identify possible break positions within a word.
pub struct Hyphenator<'a>(&'a [u8]);

impl Hyphenator<'_> {
    /// Return a Hyphenator that wraps the given buffer.
    /// This does *not* check that the given buffer is in fact a valid hyphenation table.
    /// Use `is_valid_hyphenator()` to determine whether it is usable.
    /// (Calling hyphenation methods on a Hyphenator that wraps arbitrary,
    /// unvalidated data is not unsafe, but may panic.)
    pub fn new(buffer: &[u8]) -> Hyphenator {
        Hyphenator(buffer)
    }

    // Internal implementation details
    fn magic_number(&self) -> &[u8] {
        &self.0[0 .. 4]
    }
    fn num_levels(&self) -> usize {
        u32::from_le_bytes(*array_ref!(self.0, 4, 4)) as usize
    }
    fn level(&self, i: usize) -> Level {
        let offset = u32::from_le_bytes(*array_ref!(self.0, FILE_HEADER_SIZE + 4 * i, 4)) as usize;
        let limit = if i == self.num_levels() - 1 {
            self.0.len()
        } else {
            u32::from_le_bytes(*array_ref!(self.0, FILE_HEADER_SIZE + 4 * i + 4, 4)) as usize
        };
        debug_assert!(offset + LEVEL_HEADER_SIZE <= limit && limit <= self.0.len());
        debug_assert_eq!(offset & 3, 0);
        debug_assert_eq!(limit & 3, 0);
        Level::new(&self.0[offset .. limit])
    }

    /// Identify acceptable hyphenation positions in the given `word`.
    ///
    /// The caller-supplied `values` must be at least as long as the `word`.
    ///
    /// On return, any elements with an odd value indicate positions in the word
    /// after which a hyphen could be inserted.
    ///
    /// Returns the number of possible hyphenation positions that were found.
    ///
    /// # Panics
    /// If the given `values` slice is too small to hold the results.
    ///
    /// If the block of memory represented by `self.0` is not in fact a valid
    /// hyphenation dictionary, this function may panic with an overflow or
    /// array bounds violation.
    pub fn find_hyphen_values(&self, word: &str, values: &mut [u8]) -> isize {
        assert!(values.len() >= word.len());
        values.iter_mut().for_each(|x| *x = 0);
        let top_level = self.level(0);
        let (lh_min, rh_min, clh_min, crh_min) = top_level.word_boundary_mins();
        if word.len() < lh_min + rh_min {
            return 0;
        }
        let mut hyph_count = top_level.find_hyphen_values(word, values, lh_min, rh_min);
        let compound = hyph_count > 0;
        // Subsequent levels are applied to fragments between potential breaks
        // already found:
        for l in 1 .. self.num_levels() {
            let level = self.level(l);
            if hyph_count > 0 {
                let mut begin = 0;
                let mut lh = lh_min;
                // lh_min and rh_min are both guaranteed to be greater than zero,
                // so this loop will not reach fully to the end of the word.
                for i in lh_min - 1 .. word.len() - rh_min {
                    if is_odd(values[i]) {
                        if i > begin {
                            // We've found a component of a compound;
                            // clear the corresponding values and apply the new level.
                            // (These values must be even, so hyph_count is unchanged.)
                            values[begin .. i].iter_mut().for_each(|x| {
                                *x = 0;
                            });
                            hyph_count += level.find_hyphen_values(&word[begin ..= i],
                                                                   &mut values[begin ..= i],
                                                                   lh, crh_min);
                        }
                        begin = i + 1;
                        lh = clh_min;
                    }
                }
                if begin == 0 {
                    // No compound-word breaks were found, just apply level to the whole word.
                    hyph_count += level.find_hyphen_values(word, values, lh_min, rh_min);
                } else if begin < word.len() {
                    // Handle trailing component of compound.
                    hyph_count += level.find_hyphen_values(&word[begin .. word.len()],
                                                           &mut values[begin .. word.len()],
                                                           clh_min, rh_min);
                }
            } else {
                hyph_count += level.find_hyphen_values(word, values, lh_min, rh_min);
            }
        }

        // Only need to check nohyphen strings if top-level (compound) breaks were found.
        if compound && hyph_count > 0 {
            let nohyph = top_level.nohyphen();
            if !nohyph.is_empty() {
                for i in lh_min ..= word.len() - rh_min {
                    if is_odd(values[i - 1]) {
                        for nh in &nohyph {
                            if i + nh.len() <= word.len() && *nh == &word.as_bytes()[i .. i + nh.len()] {
                                values[i - 1] = 0;
                                hyph_count -= 1;
                                break;
                            }
                            if nh.len() <= i && *nh == &word.as_bytes()[i - nh.len() .. i] {
                                values[i - 1] = 0;
                                hyph_count -= 1;
                                break;
                            }
                        }
                    }
                }
            }
        }

        hyph_count
    }

    /// Generate the hyphenated form of a `word` by inserting the given `hyphen_char`
    /// at each valid break position.
    ///
    /// # Panics
    /// If the block of memory represented by `self` is not in fact a valid
    /// hyphenation dictionary, this function may panic with an overflow or
    /// array bounds violation.
    ///
    /// Also panics if the length of the hyphenated word would overflow `usize`.
    pub fn hyphenate_word(&self, word: &str, hyphchar: char) -> String {
        let mut values = vec![0u8; word.len()];
        let hyph_count = self.find_hyphen_values(word, &mut values);
        if hyph_count <= 0 {
            return word.to_string();
        }
        // We know how long the result will be, so we can preallocate here.
        let result_len = word.len() + hyph_count as usize * hyphchar.len_utf8();
        let mut result = String::with_capacity(result_len);
        let mut n = 0;
        for ch in word.char_indices() {
            if ch.0 > 0 && is_odd(values[ch.0 - 1]) {
                result.push(hyphchar);
                n += 1;
            }
            result.push(ch.1);
        }
        debug_assert_eq!(n, hyph_count);
        debug_assert_eq!(result_len, result.len());
        result
    }

    /// Check if the block of memory looks like it could be a valid hyphenation
    /// table.
    pub fn is_valid_hyphenator(&self) -> bool {
        // Size must be at least 4 bytes for magic_number + 4 bytes num_levels;
        // smaller than this cannot be safely inspected.
        if self.0.len() < FILE_HEADER_SIZE {
            return false;
        }
        if self.magic_number() != MAGIC_NUMBER {
            return false;
        }
        // For each level, there's a 4-byte offset in the header, and the level
        // has its own 16-byte header, so we can check a minimum size again here.
        let num_levels = self.num_levels();
        if self.0.len() < FILE_HEADER_SIZE + LEVEL_HEADER_SIZE * num_levels {
            return false;
        }
        // Check that state_data_base and string_data_base for each hyphenation
        // level are within range.
        for l in 0 .. num_levels {
            let level = self.level(l);
            if level.state_data_base() < LEVEL_HEADER_SIZE ||
                   level.state_data_base() > level.string_data_base() ||
                   level.string_data_base() > level.data.len() {
                return false;
            }
            // TODO: consider doing more extensive validation of states and
            // strings within the level?
        }
        // It's still possible the dic is internally broken, but at least it's
        // worth trying to use it!
        true
    }
}

/// Load the compiled hyphenation file at `dic_path`, if present.
///
/// Returns `None` if the specified file cannot be opened or mapped,
/// otherwise returns a `memmap2::Mmap` mapping the file.
///
/// # Safety
///
/// This is unsafe for the same reason `Mmap::map()` is unsafe:
/// mapped_hyph does not guarantee safety if the mapped file is modified
/// (e.g. by another process) while we're using it.
///
/// This verifies that the file looks superficially like it may be a
/// compiled hyphenation table, but does *not* fully check the validity
/// of the file contents! Calling hyphenation functions with the returned
/// data is not unsafe, but may panic if the data is invalid.
pub unsafe fn load_file(dic_path: &str) -> Option<Mmap> {
    let file = File::open(dic_path).ok()?;
    let dic = Mmap::map(&file).ok()?;
    let hyph = Hyphenator(&*dic);
    if hyph.is_valid_hyphenator() {
        return Some(dic);
    }
    None
}
