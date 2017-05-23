
extern crate itertools;
extern crate odds;

use odds::slice::SliceIterExt;
use itertools::Itertools;

#[test]
fn mend_slices() {
    let text = "α-toco (and) β-toco";
    let full_text = CharSlices::new(text).map(|(_, s)| s).mend_slices().join("");
    assert_eq!(text, full_text);

    // join certain different pieces together again
    let words = CharSlices::new(text).map(|(_, s)| s)
                    .filter(|s| !s.chars().any(char::is_whitespace))
                    .mend_slices().collect::<Vec<_>>();
    assert_eq!(words, vec!["α-toco", "(and)", "β-toco"]);
}

#[test]
fn mend_slices_mut() {
    let mut data = [1, 2, 3];
    let mut copy = data.to_vec();
    {
        let slc = data.chunks_mut(1).mend_slices().next().unwrap();
        assert_eq!(slc, &mut copy[..]);
    }
    {
        let slc = data.chunks_mut(2).mend_slices().next().unwrap();
        assert_eq!(slc, &mut copy[..]);
    }
    {
        let mut iter = data.chunks_mut(1).filter(|c| c[0] != 2).mend_slices();
        assert_eq!(iter.next(), Some(&mut [1][..]));
        assert_eq!(iter.next(), Some(&mut [3][..]));
        assert_eq!(iter.next(), None);
    }
}

/// Like CharIndices iterator, except it yields slices instead
#[derive(Copy, Clone, Debug)]
struct CharSlices<'a> {
    slice: &'a str,
    offset: usize,
}

impl<'a> CharSlices<'a>
{
    pub fn new(s: &'a str) -> Self
    {
        CharSlices {
            slice: s,
            offset: 0,
        }
    }
}

impl<'a> Iterator for CharSlices<'a>
{
    type Item = (usize, &'a str);

    fn next(&mut self) -> Option<Self::Item>
    {
        if self.slice.len() == 0 {
            return None
        }
        // count continuation bytes
        let mut char_len = 1;
        let mut bytes = self.slice.bytes();
        bytes.next();
        for byte in bytes {
            if (byte & 0xC0) != 0x80 {
                break
            }
            char_len += 1;
        }
        let ch_slice;
        unsafe {
            ch_slice = self.slice.slice_unchecked(0, char_len);
            self.slice = self.slice.slice_unchecked(char_len, self.slice.len());
        }
        let off = self.offset;
        self.offset += char_len;
        Some((off, ch_slice))
    }
}

