use std::ops::*;

pub struct NextBits;
pub trait BitsRange {
    fn get_bits_range(&self, packed_bit_width: usize, prev_range: &Option<Range<usize>>) -> Range<usize>; 
}

impl BitsRange for usize {
    fn get_bits_range(&self, packed_bit_width: usize, _prev_range: &Option<Range<usize>>) -> Range<usize> {
        *self..(*self + packed_bit_width as usize - 1)
    }
}

impl BitsRange for Range<usize> {
    fn get_bits_range(&self, _packed_bit_width: usize, _prev_range: &Option<Range<usize>>) -> Range<usize> {
        self.start..self.end
    }
}

impl BitsRange for NextBits {    
    fn get_bits_range(&self, packed_bit_width: usize, prev_range: &Option<Range<usize>>) -> Range<usize> {
        if let &Some(ref prev_range) = prev_range {
            (prev_range.end + 1)..((prev_range.end + 1) + (packed_bit_width as usize) - 1)
        } else {
            0..((packed_bit_width as usize) - 1)
        }
    }
}


pub fn ones_u8(n: u8) -> u8 {    
    match n {
        0 => 0b00000000,
        1 => 0b00000001,
        2 => 0b00000011,
        3 => 0b00000111,
        4 => 0b00001111,
        5 => 0b00011111,
        6 => 0b00111111,
        7 => 0b01111111,
        _ => 0b11111111
    }
}


// From rustc
pub fn to_snake_case(mut str: &str) -> String {
    let mut words = vec![];
    // Preserve leading underscores
    str = str.trim_left_matches(|c: char| {
        if c == '_' {
            words.push(String::new());
            true
        } else {
            false
        }
    });
    for s in str.split('_') {
        let mut last_upper = false;
        let mut buf = String::new();
        if s.is_empty() {
            continue;
        }
        for ch in s.chars() {
            if !buf.is_empty() && buf != "'" && ch.is_uppercase() && !last_upper {
                words.push(buf);
                buf = String::new();
            }
            last_upper = ch.is_uppercase();
            buf.extend(ch.to_lowercase());
        }
        words.push(buf);
    }
    words.join("_")
}
