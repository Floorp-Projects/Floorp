#[derive(Debug, PartialEq, Eq, Clone, Copy, Hash)]
pub struct SourceLocation {
    pub start: usize,
    pub end: usize,
}

impl SourceLocation {
    pub fn new(start: usize, end: usize) -> Self {
        debug_assert!(start <= end);

        Self { start, end }
    }

    pub fn from_parts(start: SourceLocation, end: SourceLocation) -> Self {
        debug_assert!(start.start <= end.end);

        Self {
            start: start.start,
            end: end.end,
        }
    }

    pub fn set_range(&mut self, start: SourceLocation, end: SourceLocation) {
        debug_assert!(start.start <= end.end);
        self.start = start.start;
        self.end = end.end;
    }

    pub fn default() -> Self {
        Self { start: 0, end: 0 }
    }
}
