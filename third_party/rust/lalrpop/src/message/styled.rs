use ascii_canvas::AsciiView;
use std::fmt::{Debug, Error, Formatter};
use style::Style;
use super::*;

pub struct Styled {
    style: Style,
    content: Box<Content>,
}

impl Styled {
    pub fn new(style: Style, content: Box<Content>) -> Self {
        Styled {
            style: style,
            content: content,
        }
    }
}

impl Content for Styled {
    fn min_width(&self) -> usize {
        self.content.min_width()
    }

    fn emit(&self, view: &mut AsciiView) {
        self.content.emit(&mut view.styled(self.style))
    }

    fn into_wrap_items(self: Box<Self>, wrap_items: &mut Vec<Box<Content>>) {
        let style = self.style;
        super::into_wrap_items_map(self.content, wrap_items, |item| Styled::new(style, item))
    }
}

impl Debug for Styled {
    fn fmt(&self, fmt: &mut Formatter) -> Result<(), Error> {
        fmt.debug_struct("Styled")
            .field("content", &self.content)
            .finish()
    }
}
