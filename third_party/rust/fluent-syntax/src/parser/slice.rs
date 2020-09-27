use std::ops::Range;
pub trait Slice<'s>: AsRef<str> + Clone + PartialEq {
    fn slice(&self, range: Range<usize>) -> Self;
    fn trim(&mut self);
}

impl<'s> Slice<'s> for String {
    fn slice(&self, range: Range<usize>) -> Self {
        self[range].to_string()
    }

    fn trim(&mut self) {
        *self = self.trim_end().to_string();
    }
}

impl<'s> Slice<'s> for &'s str {
    fn slice(&self, range: Range<usize>) -> Self {
        &self[range]
    }

    fn trim(&mut self) {
        *self = self.trim_end();
    }
}
