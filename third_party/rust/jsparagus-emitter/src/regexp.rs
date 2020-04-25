use ast::source_slice_list::SourceSliceIndex;

#[derive(Debug)]
pub struct RegExpItem {
    pub pattern: SourceSliceIndex,
    pub global: bool,
    pub ignore_case: bool,
    pub multi_line: bool,
    pub dot_all: bool,
    pub sticky: bool,
    pub unicode: bool,
}

/// Index into RegExpList.atoms.
#[derive(Debug, Clone, Copy)]
pub struct RegExpIndex {
    index: usize,
}

impl RegExpIndex {
    fn new(index: usize) -> Self {
        Self { index }
    }
}

impl From<RegExpIndex> for usize {
    fn from(index: RegExpIndex) -> usize {
        index.index
    }
}

/// List of RegExp.
#[derive(Debug)]
pub struct RegExpList {
    items: Vec<RegExpItem>,
}

impl RegExpList {
    pub fn new() -> Self {
        Self { items: Vec::new() }
    }

    pub fn push(&mut self, item: RegExpItem) -> RegExpIndex {
        let index = self.items.len();
        self.items.push(item);
        RegExpIndex::new(index)
    }
}

impl From<RegExpList> for Vec<RegExpItem> {
    fn from(list: RegExpList) -> Vec<RegExpItem> {
        list.items
    }
}
