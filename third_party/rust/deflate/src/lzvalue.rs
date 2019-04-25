use huffman_table::{MAX_DISTANCE, MIN_MATCH};
#[cfg(test)]
use huffman_table::MAX_MATCH;

#[derive(Copy, Clone, Eq, PartialEq, Debug)]
pub struct StoredLength {
    length: u8,
}

impl StoredLength {
    #[cfg(test)]
    pub fn from_actual_length(length: u16) -> StoredLength {
        assert!(length <= MAX_MATCH && length >= MIN_MATCH);
        StoredLength {
            length: (length - MIN_MATCH) as u8,
        }
    }

    pub fn new(stored_length: u8) -> StoredLength {
        StoredLength {
            length: stored_length,
        }
    }

    pub fn stored_length(&self) -> u8 {
        self.length
    }

    #[cfg(test)]
    pub fn actual_length(&self) -> u16 {
        u16::from(self.length) + MIN_MATCH
    }
}

#[derive(Copy, Clone, Eq, PartialEq, Debug)]
pub enum LZType {
    Literal(u8),
    StoredLengthDistance(StoredLength, u16),
}

#[derive(Copy, Clone, Eq, PartialEq, Debug)]
pub struct LZValue {
    litlen: u8,
    distance: u16,
}

impl LZValue {
    #[inline]
    pub fn literal(value: u8) -> LZValue {
        LZValue {
            litlen: value,
            distance: 0,
        }
    }

    #[inline]
    pub fn length_distance(length: u16, distance: u16) -> LZValue {
        debug_assert!(distance > 0 && distance <= MAX_DISTANCE);
        let stored_length = (length - MIN_MATCH) as u8;
        LZValue {
            litlen: stored_length,
            distance: distance,
        }
    }

    #[inline]
    pub fn value(&self) -> LZType {
        if self.distance != 0 {
            LZType::StoredLengthDistance(StoredLength::new(self.litlen), self.distance)
        } else {
            LZType::Literal(self.litlen)
        }
    }
}

#[cfg(test)]
pub fn lit(l: u8) -> LZValue {
    LZValue::literal(l)
}

#[cfg(test)]
pub fn ld(l: u16, d: u16) -> LZValue {
    LZValue::length_distance(l, d)
}

#[cfg(test)]
mod test {
    use super::*;
    use huffman_table::{MIN_MATCH, MIN_DISTANCE, MAX_MATCH, MAX_DISTANCE};
    #[test]
    fn lzvalue() {
        for i in 0..255 as usize + 1 {
            let v = LZValue::literal(i as u8);
            if let LZType::Literal(n) = v.value() {
                assert_eq!(n as usize, i);
            } else {
                panic!();
            }
        }

        for i in MIN_MATCH..MAX_MATCH + 1 {
            let v = LZValue::length_distance(i, 5);
            if let LZType::StoredLengthDistance(l, _) = v.value() {
                assert_eq!(l.actual_length(), i);
            } else {
                panic!();
            }
        }

        for i in MIN_DISTANCE..MAX_DISTANCE + 1 {
            let v = LZValue::length_distance(5, i);

            if let LZType::StoredLengthDistance(_, d) = v.value() {
                assert_eq!(d, i);
            } else {
                panic!("Failed to get distance {}", i);
            }
        }

    }
}
