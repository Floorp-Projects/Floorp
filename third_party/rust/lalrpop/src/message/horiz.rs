use super::*;
use ascii_canvas::AsciiView;
use itertools::Itertools;

#[derive(Debug)]
pub struct Horiz {
    items: Vec<Box<Content>>,
    separate: usize, // 0 => overlapping, 1 => each on its own line, 2 => paragraphs
}

impl Horiz {
    pub fn new(items: Vec<Box<Content>>, separate: usize) -> Self {
        Horiz {
            items: items,
            separate: separate,
        }
    }
}

impl Content for Horiz {
    fn min_width(&self) -> usize {
        self.items
            .iter()
            .map(|c| c.min_width())
            .intersperse(self.separate)
            .fold(0, |a, b| a + b)
    }

    fn emit(&self, view: &mut AsciiView) {
        emit_horiz(view, &self.items, self.separate);
    }

    fn into_wrap_items(self: Box<Self>, wrap_items: &mut Vec<Box<Content>>) {
        wrap_items.push(self);
    }
}

pub fn emit_horiz(view: &mut AsciiView, items: &[Box<Content>], separate: usize) {
    let mut column = 0;
    for item in items {
        let (_, end_column) = item.emit_at(view, 0, column);
        column = end_column + separate;
    }
}
