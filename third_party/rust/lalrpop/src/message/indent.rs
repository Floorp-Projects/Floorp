use super::*;
use ascii_canvas::AsciiView;

#[derive(Debug)]
pub struct Indent {
    amount: usize,
    content: Box<Content>,
}

impl Indent {
    pub fn new(amount: usize, content: Box<Content>) -> Self {
        Indent {
            amount: amount,
            content: content,
        }
    }
}

impl Content for Indent {
    fn min_width(&self) -> usize {
        self.content.min_width() + self.amount
    }

    fn emit(&self, view: &mut AsciiView) {
        let mut subview = view.shift(0, self.amount);
        self.content.emit(&mut subview);
    }

    fn into_wrap_items(self: Box<Self>, wrap_items: &mut Vec<Box<Content>>) {
        wrap_items.push(self);
    }
}
