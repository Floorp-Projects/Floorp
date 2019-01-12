use super::*;
use ascii_canvas::AsciiView;

#[derive(Debug)]
pub struct Vert {
    items: Vec<Box<Content>>,
    separate: usize, // 0 => overlapping, 1 => each on its own line, 2 => paragraphs
}

impl Vert {
    pub fn new(items: Vec<Box<Content>>, separate: usize) -> Self {
        Vert {
            items: items,
            separate: separate,
        }
    }
}

impl Content for Vert {
    fn min_width(&self) -> usize {
        self.items.iter().map(|c| c.min_width()).max().unwrap()
    }

    fn emit(&self, view: &mut AsciiView) {
        emit_vert(view, &self.items, self.separate);
    }

    fn into_wrap_items(self: Box<Self>, wrap_items: &mut Vec<Box<Content>>) {
        wrap_items.push(self);
    }
}

pub fn emit_vert(view: &mut AsciiView, items: &[Box<Content>], separate: usize) {
    let mut row = 0;
    for item in items {
        let (end_row, _) = item.emit_at(view, row, 0);
        row = end_row + separate;
    }
}
