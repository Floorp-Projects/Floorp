/// Index into SourceSliceList.items.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct SourceSliceIndex {
    index: usize,
}
impl SourceSliceIndex {
    fn new(index: usize) -> Self {
        Self { index }
    }
}

impl From<SourceSliceIndex> for usize {
    fn from(index: SourceSliceIndex) -> usize {
        index.index
    }
}

/// Set of slices inside source, including the following:
///
///   * RegExp source
///   * BigInt literal
///
/// Compared to `SourceAtomSet`, this is not atomized.
#[derive(Debug)]
pub struct SourceSliceList<'alloc> {
    items: Vec<&'alloc str>,
}

impl<'alloc> SourceSliceList<'alloc> {
    pub fn new() -> Self {
        Self { items: Vec::new() }
    }

    pub fn push(&mut self, s: &'alloc str) -> SourceSliceIndex {
        let index = self.items.len();
        self.items.push(s);
        SourceSliceIndex::new(index)
    }

    pub fn get(&self, index: SourceSliceIndex) -> &'alloc str {
        self.items.get(usize::from(index)).unwrap()
    }
}

impl<'alloc> From<SourceSliceList<'alloc>> for Vec<&'alloc str> {
    fn from(set: SourceSliceList<'alloc>) -> Vec<&'alloc str> {
        set.items.into_iter().collect()
    }
}
