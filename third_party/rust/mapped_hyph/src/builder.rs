// Copyright 2019-2020 Mozilla Foundation. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

/// Functions to compile human-readable patterns into a mapped_hyph
/// flattened representation of the hyphenation state machine.

use std::io::{Read,BufRead,BufReader,Write,Error,ErrorKind};
use std::collections::HashMap;
use std::convert::TryInto;
use std::hash::{Hash,Hasher};

// Wrap a HashMap so that we can implement the Hash trait.
#[derive(PartialEq, Eq, Clone)]
struct TransitionMap (HashMap<u8,i32>);

impl TransitionMap {
    fn new() -> TransitionMap {
        TransitionMap(HashMap::<u8,i32>::new())
    }
}

impl Hash for TransitionMap {
    fn hash<H: Hasher>(&self, state: &mut H) {
        // We only look at the values here; that's likely to be enough
        // for a reasonable hash.
        let mut transitions: Vec<&i32> = self.0.values().collect();
        transitions.sort();
        for t in transitions {
            t.hash(state);
        }
    }
}

#[derive(PartialEq, Eq, Hash, Clone)]
struct State {
    match_string: Option<Vec<u8>>,
    #[allow(dead_code)]
    repl_string: Option<Vec<u8>>,
    #[allow(dead_code)]
    repl_index: i32,
    #[allow(dead_code)]
    repl_cut: i32,
    fallback_state: i32,
    transitions: TransitionMap,
}

impl State {
    fn new() -> State {
        State {
            match_string: None,
            repl_string: None,
            repl_index: -1,
            repl_cut: -1,
            fallback_state: -1,
            transitions: TransitionMap::new(),
        }
    }
}

/// Structures returned by the read_dic_file() function;
/// array of these can then be passed to write_hyf_file()
/// to create the flattened output.
struct LevelBuilder {
    states: Vec<State>,
    str_to_state: HashMap<Vec<u8>,i32>,
    encoding: Option<String>,
    nohyphen: Option<String>,
    lh_min: u8,
    rh_min: u8,
    clh_min: u8,
    crh_min: u8,
}

impl LevelBuilder {
    fn new() -> LevelBuilder {
        let mut result = LevelBuilder {
            states: Vec::<State>::new(),
            str_to_state: HashMap::<Vec<u8>,i32>::new(),
            encoding: None,
            nohyphen: None,
            lh_min: 0,
            rh_min: 0,
            clh_min: 0,
            crh_min: 0,
        };
        // Initialize the builder with an empty start state.
        result.str_to_state.insert(vec![], 0);
        result.states.push(State::new());
        result
    }

    fn find_state_number_for(&mut self, text: &[u8]) -> i32 {
        let count = self.states.len() as i32;
        let index = *self.str_to_state.entry(text.to_vec()).or_insert(count);
        if index == count {
            self.states.push(State::new());
        }
        index
    }

    fn add_pattern(&mut self, pattern: &str) {
        let mut bytes = pattern.as_bytes();
        let mut text = Vec::<u8>::with_capacity(bytes.len());
        let mut digits = Vec::<u8>::with_capacity(bytes.len() + 1);
        let mut repl_str = None;
        let mut repl_index = 0;
        let mut repl_cut = 0;

        // Check for replacement rule (non-standard hyphenation spelling change).
        if let Some(slash) = bytes.iter().position(|x| *x == b'/') {
            let parts = bytes.split_at(slash);
            bytes = parts.0;
            let mut it = parts.1[1 ..].split(|x| *x == b',');
            if let Some(repl) = it.next() {
                repl_str = Some(repl.to_vec());
            }
            if let Some(num) = it.next() {
                repl_index = std::str::from_utf8(num).unwrap().parse::<i32>().unwrap() - 1;
            }
            if let Some(num) = it.next() {
                repl_cut = std::str::from_utf8(num).unwrap().parse::<i32>().unwrap();
            }
        }

        // Separate the input pattern into parallel arrays of text (bytes) and digits.
        let mut got_digit = false;
        for byte in bytes {
            if *byte <= b'9' && *byte >= b'0' {
                if got_digit {
                    warn!("invalid pattern \"{}\": consecutive digits", pattern);
                    return;
                }
                digits.push(*byte);
                got_digit = true;
            } else {
                text.push(*byte);
                if got_digit {
                    got_digit = false;
                } else {
                    digits.push(b'0');
                }
            }
        }
        if !got_digit {
            digits.push(b'0');
        }

        if repl_str.is_none() {
            // Optimize away leading zeroes from the digits array.
            while !digits.is_empty() && digits[0] == b'0' {
                digits.remove(0);
            }
        } else {
            // Convert repl_index and repl_cut from Unicode char to byte indexing.
            let start = if text[0] == b'.' { 1 } else { 0 };
            if start == 1 {
                if digits[0] != b'0' {
                    warn!("invalid pattern \"{}\": unexpected digit before start of word", pattern);
                    return;
                }
                digits.remove(0);
            }
            let word = std::str::from_utf8(&text[start..]).unwrap();
            let mut chars: Vec<_> = word.char_indices().collect();
            chars.push((word.len(), '.'));
            repl_cut = chars[(repl_index + repl_cut) as usize].0 as i32 - chars[repl_index as usize].0 as i32;
            repl_index = chars[repl_index as usize].0 as i32;
        }

        // Create the new state, or add pattern into an existing state
        // (which should not already have a match_string).
        let mut state_num = self.find_state_number_for(&text);
        let mut state = &mut self.states[state_num as usize];
        if state.match_string.is_some() {
            warn!("duplicate pattern \"{}\" discarded", pattern);
            return;
        }
        if !digits.is_empty() {
            state.match_string = Some(digits);
        }
        if repl_str.is_some() {
            state.repl_string = repl_str;
            state.repl_index = repl_index;
            state.repl_cut = repl_cut;
        }

        // Set up prefix transitions, inserting additional states as needed.
        while !text.is_empty() {
            let last_state = state_num;
            let ch = *text.last().unwrap();
            text.truncate(text.len() - 1);
            state_num = self.find_state_number_for(&text);
            if let Some(exists) = self.states[state_num as usize].transitions.0.insert(ch, last_state) {
                assert_eq!(exists, last_state, "overwriting existing transition at pattern \"{}\"", pattern);
                break;
            }
        }
    }

    fn merge_duplicate_states(&mut self) {
        // We loop here because when we eliminate a duplicate, and update the transitons
        // that referenced it, we may thereby create new duplicates that another pass
        // will find and compress further.
        loop {
            let orig_len = self.states.len();
            // Used to map State records to the (first) index at which they occur.
            let mut state_to_index = HashMap::<&State,i32>::new();
            // Mapping of old->new state indexes, and whether each old state is
            // a duplicate that should be dropped.
            let mut mappings = Vec::<(i32,bool)>::with_capacity(orig_len);
            let mut next_new_index: i32 = 0;
            for index in 0 .. self.states.len() {
                // Find existing index for this state, or allocate the next new index to it.
                let new_index = *state_to_index.entry(&self.states[index]).or_insert(next_new_index);
                // Record the mapping, and whether the state was a duplicate.
                mappings.push((new_index, new_index != next_new_index));
                // If we used next_new_index for this state, increment it.
                if new_index == next_new_index {
                    next_new_index += 1;
                }
            }
            // If we didn't find any duplicates, next_new_index will have kept pace with
            // index, so we know we're finished.
            if next_new_index as usize == self.states.len() {
                break;
            }
            // Iterate over all the states, either deleting them or updating indexes
            // according to the mapping we created; then repeat the search.
            for index in (0 .. self.states.len()).rev() {
                if mappings[index].1 {
                    self.states.remove(index);
                } else {
                    let state = &mut self.states[index];
                    if state.fallback_state != -1 {
                        state.fallback_state = mappings[state.fallback_state as usize].0;
                    }
                    for t in state.transitions.0.iter_mut() {
                        *t.1 = mappings[*t.1 as usize].0;
                    }
                }
            }
        }
    }

    fn flatten(&self) -> Vec<u8> {
        // Calculate total space needed for state data, and build the state_to_offset table.
        let mut state_data_size = 0;
        let mut state_to_offset = Vec::<usize>::with_capacity(self.states.len());
        for state in &self.states {
            state_to_offset.push(state_data_size);
            state_data_size += if state.repl_string.is_some() { 12 } else { 8 };
            state_data_size += state.transitions.0.len() * 4;
        }

        // Helper to map a state index to its offset in the final data block.
        let get_state_offset_for = |state_index: i32| -> u32 {
            if state_index < 0 {
                return super::INVALID_STATE_OFFSET;
            }
            state_to_offset[state_index as usize] as u32
        };

        // Helper to map a byte string to its offset in the final data block, and
        // store the bytes into string_data unless using an already-existing string.
        let mut string_to_offset = HashMap::<Vec<u8>,usize>::new();
        let mut string_data = Vec::<u8>::new();
        let mut get_string_offset_for = |bytes: &Option<Vec<u8>>| -> u16 {
            if bytes.is_none() {
                return super::INVALID_STRING_OFFSET;
            }
            assert!(bytes.as_ref().unwrap().len() < 256);
            let new_offset = string_data.len();
            let offset = *string_to_offset.entry(bytes.as_ref().unwrap().clone()).or_insert(new_offset);
            if offset == new_offset {
                string_data.push(bytes.as_ref().unwrap().len() as u8);
                string_data.extend_from_slice(bytes.as_ref().unwrap().as_ref());
            }
            offset.try_into().unwrap()
        };

        // Handle nohyphen string list if present, converting comma separators to NULs
        // and trimming any surplus whitespace.
        let mut nohyphen_string_offset: u16 = super::INVALID_STRING_OFFSET;
        let mut nohyphen_count: u16 = 0;
        if self.nohyphen.is_some() {
            let nohyphen_strings: Vec<_> = self.nohyphen.as_ref().unwrap().split(',').map(|x| x.trim()).collect();
            nohyphen_count = nohyphen_strings.len().try_into().unwrap();
            nohyphen_string_offset = get_string_offset_for(&Some(nohyphen_strings.join("\0").as_bytes().to_vec()));
        }

        let mut state_data = Vec::<u8>::with_capacity(state_data_size);
        for state in &self.states {
            state_data.extend(&get_state_offset_for(state.fallback_state).to_le_bytes());
            state_data.extend(&get_string_offset_for(&state.match_string).to_le_bytes());
            state_data.push(state.transitions.0.len() as u8);
            // Determine whether to use an extended state record, and if so add the
            // replacement string and index fields.
            if state.repl_string.is_none() {
                state_data.push(0);
            } else {
                state_data.push(1);
                state_data.extend(&get_string_offset_for(&state.repl_string).to_le_bytes());
                state_data.push(state.repl_index as u8);
                state_data.push(state.repl_cut as u8);
            }
            // Collect transitions into an array so we can sort them.
            let mut transitions = vec![];
            for (key, value) in state.transitions.0.iter() {
                transitions.push((*key, get_state_offset_for(*value)))
            }
            transitions.sort();
            for t in transitions {
                // New state offset is stored as a 24-bit value, so we do this manually.
                state_data.push((t.1 & 0xff) as u8);
                state_data.push(((t.1 >> 8) & 0xff) as u8);
                state_data.push(((t.1 >> 16) & 0xff) as u8);
                state_data.push(t.0);
            }
        }
        assert_eq!(state_data.len(), state_data_size);

        // Pad string data to a 4-byte boundary
        while string_data.len() & 3 != 0 {
            string_data.push(0);
        }

        let total_size = super::LEVEL_HEADER_SIZE as usize + state_data_size + string_data.len();
        let mut result = Vec::<u8>::with_capacity(total_size);

        let state_data_base: u32 = super::LEVEL_HEADER_SIZE as u32;
        let string_data_base: u32 = state_data_base + state_data_size as u32;

        result.extend(&state_data_base.to_le_bytes());
        result.extend(&string_data_base.to_le_bytes());
        result.extend(&nohyphen_string_offset.to_le_bytes());
        result.extend(&nohyphen_count.to_le_bytes());
        result.push(self.lh_min);
        result.push(self.rh_min);
        result.push(self.clh_min);
        result.push(self.crh_min);

        result.extend(state_data.iter());
        result.extend(string_data.iter());

        assert_eq!(result.len(), total_size);

        result
    }
}

/// Read a libhyphen-style pattern file and create the corresponding state
/// machine transitions, etc.
/// The returned Vec can be passed to write_hyf_file() to generate a flattened
/// representation of the state machine in mapped_hyph's binary format.
fn read_dic_file<T: Read>(dic_file: T, compress: bool) -> Result<Vec<LevelBuilder>, &'static str> {
    let reader = BufReader::new(dic_file);

    let mut builders = Vec::<LevelBuilder>::new();
    builders.push(LevelBuilder::new());
    let mut builder = &mut builders[0];

    for (index, line) in reader.lines().enumerate() {
        let mut trimmed = line.unwrap().trim().to_string();
        // Strip comments.
        if let Some(i) = trimmed.find('%') {
            trimmed = trimmed[..i].trim().to_string();
        }
        // Ignore empty lines.
        if trimmed.is_empty() {
            continue;
        }
        // Uppercase indicates keyword rather than pattern.
        if trimmed.as_bytes()[0] >= b'A' && trimmed.as_bytes()[0] <= b'Z' {
            // First line is encoding; we only support UTF-8.
            if builder.encoding.is_none() {
                if trimmed != "UTF-8" {
                    return Err("Only UTF-8 patterns are accepted!");
                };
                builder.encoding = Some(trimmed);
                continue;
            }
            // Check for valid keyword-value pairs.
            if trimmed.contains(' ') {
                let parts: Vec<&str> = trimmed.split(' ').collect();
                if parts.len() != 2 {
                    warn!("unrecognized keyword/values: {}", trimmed);
                    continue;
                }
                let keyword = parts[0];
                let value = parts[1];
                match keyword {
                    "LEFTHYPHENMIN" => builder.lh_min = value.parse::<u8>().unwrap(),
                    "RIGHTHYPHENMIN" => builder.rh_min = value.parse::<u8>().unwrap(),
                    "COMPOUNDLEFTHYPHENMIN" => builder.clh_min = value.parse::<u8>().unwrap(),
                    "COMPOUNDRIGHTHYPHENMIN" => builder.crh_min = value.parse::<u8>().unwrap(),
                    "NOHYPHEN" => builder.nohyphen = Some(trimmed),
                    _ => warn!("unknown keyword: {}", trimmed),
                }
                continue;
            }
            // Start a new hyphenation level?
            if trimmed == "NEXTLEVEL" {
                builders.push(LevelBuilder::new());
                builder = builders.last_mut().unwrap();
                continue;
            }
            warn!("unknown keyword: {}", trimmed);
            continue;
        }
        // Patterns should always be provided in lowercase; complain if not, and discard
        // the bad pattern.
        if trimmed != trimmed.to_lowercase() {
            warn!("pattern \"{}\" not lowercased at line {}", trimmed, index);
            continue;
        }
        builder.add_pattern(&trimmed);
    }

    // Create default first (compound-word) level if only one level was provided.
    // (Maybe this should be optional? Currently just copying libhyphen behavior.)
    if builders.len() == 1 {
        let (lh_min, rh_min, clh_min, crh_min) =
            (builders[0].lh_min, builders[0].rh_min, builders[0].clh_min, builders[0].crh_min);
        builders.insert(0, LevelBuilder::new());
        builder = builders.first_mut().unwrap();
        builder.add_pattern("1-1");
        builder.add_pattern("1'1");
        builder.add_pattern("1\u{2013}1"); // en-dash
        builder.add_pattern("1\u{2019}1"); // curly apostrophe
        builder.nohyphen = Some("',\u{2013},\u{2019},-".to_string());
        builder.lh_min = lh_min;
        builder.rh_min = rh_min;
        builder.clh_min = if clh_min > 0 { clh_min } else if lh_min > 0 { lh_min } else { 3 };
        builder.crh_min = if crh_min > 0 { crh_min } else if rh_min > 0 { rh_min } else { 3 };
    }

    // Put in fallback states in each builder.
    for builder in &mut builders {
        for (key, state_index) in builder.str_to_state.iter() {
            if key.is_empty() {
                continue;
            }
            let mut fallback_key = key.clone();
            while !fallback_key.is_empty() {
                fallback_key.remove(0);
                if builder.str_to_state.contains_key(&fallback_key) {
                    break;
                }
            }
            builder.states[*state_index as usize].fallback_state = builder.str_to_state[&fallback_key];
        }
    }

    if compress {
        // Merge duplicate states to reduce size.
        for builder in &mut builders {
            builder.merge_duplicate_states();
        }
    }

    Ok(builders)
}

/// Write out the state machines representing a set of hyphenation rules
/// to the given output stream.
fn write_hyf_file<T: Write>(hyf_file: &mut T, levels: Vec<LevelBuilder>) -> std::io::Result<()> {
    if levels.is_empty() {
        return Err(Error::from(ErrorKind::InvalidData));
    }
    let mut flattened = vec![];
    for level in levels {
        flattened.push(level.flatten());
    }
    // Write file header: magic number, count of levels.
    hyf_file.write_all(&[b'H', b'y', b'f', b'0'])?;
    let level_count: u32 = flattened.len() as u32;
    hyf_file.write_all(&level_count.to_le_bytes())?;
    // Write array of offsets to each level. First level will begin immediately
    // after the array of offsets.
    let mut offset: u32 = super::FILE_HEADER_SIZE as u32 + 4 * level_count;
    for flat in &flattened {
        hyf_file.write_all(&offset.to_le_bytes())?;
        offset += flat.len() as u32;
    }
    // Write the flattened data for each level.
    for flat in &flattened {
        hyf_file.write_all(&flat)?;
    }
    Ok(())
}

/// The public API to the compilation process: reads `dic_file` and writes compiled tables
/// to `hyf_file`. The `compress` param determines whether extra processing to reduce the
/// size of the output is performed.
pub fn compile<T1: Read, T2: Write>(dic_file: T1, hyf_file: &mut T2, compress: bool) -> std::io::Result<()> {
    match read_dic_file(dic_file, compress) {
        Ok(dic) => write_hyf_file(hyf_file, dic),
        Err(e) => {
            warn!("parse error: {}", e);
            return Err(Error::from(ErrorKind::InvalidData))
        }
    }
}
