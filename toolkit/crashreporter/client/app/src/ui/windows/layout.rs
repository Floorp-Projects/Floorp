/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Helpers for window layout.

use super::{
    model::{self, Alignment, Element, ElementStyle, Margin},
    ElementRef, WideString,
};
use crate::data::Property;
use std::collections::HashMap;
use windows_sys::Win32::{
    Foundation::{HWND, SIZE},
    Graphics::Gdi,
    UI::WindowsAndMessaging as win,
};

pub(super) type ElementMapping = HashMap<ElementRef, HWND>;

/// Handles the layout of windows.
///
/// This is done in two passes. The first pass calculates the sizes for all elements in the tree.
/// Once sizes are known, the second pass can appropriately position the elements (taking alignment
/// into account).
///
/// Currently, the resize/reposition logic is tied into the methods here, which is an inconvenient
/// design because when adding support for a new element type you have to add new information in
/// disparate locations.
pub struct Layout<'a> {
    elements: &'a ElementMapping,
    sizes: HashMap<ElementRef, Size>,
    last_positioned: Option<HWND>,
}

// Unfortunately, there's no good way to get these margins. I just guessed and the first guesses
// seemed to be close enough.
const BUTTON_MARGIN: Margin = Margin {
    start: 5,
    end: 5,
    top: 5,
    bottom: 5,
};
const CHECKBOX_MARGIN: Margin = Margin {
    start: 15,
    end: 0,
    top: 0,
    bottom: 0,
};

impl<'a> Layout<'a> {
    pub(super) fn new(elements: &'a ElementMapping) -> Self {
        Layout {
            elements,
            sizes: Default::default(),
            last_positioned: None,
        }
    }

    /// Perform a layout of the element and all child elements.
    pub fn layout(mut self, element: &Element, max_width: u32, max_height: u32) {
        let max_size = Size {
            width: max_width,
            height: max_height,
        };
        self.resize(element, &max_size);
        self.reposition(element, &Position::default(), &max_size);
    }

    fn resize(&mut self, element: &Element, max_size: &Size) -> Size {
        let style = &element.style;

        let mut inner_size = max_size.inner_size(style);
        let mut content_size = None;

        if !is_visible(style) {
            self.sizes
                .insert(ElementRef::new(element), Default::default());
            return Default::default();
        }

        // Resize inner content.
        //
        // These cases should result in `content_size` being set, if relevant.
        use model::ElementType::*;
        match &element.element_type {
            Button(model::Button {
                content: Some(content),
                ..
            }) => {
                // Special case for buttons with a label.
                if let Label(model::Label {
                    text: Property::Static(text),
                    ..
                }) = &content.element_type
                {
                    let mut size = Size {
                        // We prefer button text to not wrap, so ignore inner size for width to get
                        // an unwrapped content_size.
                        width: max_size.width,
                        height: inner_size.height,
                    };
                    size = size.less_margin(&BUTTON_MARGIN);
                    self.measure_text(text.as_str(), element, &mut size);
                    content_size = Some(size.plus_margin(&BUTTON_MARGIN));
                }
            }
            Checkbox(model::Checkbox {
                label: Some(label), ..
            }) => {
                let mut size = inner_size.less_margin(&CHECKBOX_MARGIN);
                self.measure_text(label.as_str(), element, &mut size);
                content_size = Some(size.plus_margin(&CHECKBOX_MARGIN));
            }
            Label(model::Label { text, bold: _ }) => {
                let mut size = inner_size.clone();
                match text {
                    Property::Static(text) => self.measure_text(text.as_str(), element, &mut size),
                    Property::Binding(b) => {
                        self.measure_text(b.borrow().as_str(), element, &mut size)
                    }
                    Property::ReadOnly(_) => {
                        unimplemented!("Label::text does not support ReadOnly")
                    }
                }
                content_size = Some(size);
            }
            VBox(model::VBox { items, spacing }) => {
                let mut height = 0;
                let mut max_width = 0;
                let mut remaining_size = inner_size.clone();
                let mut resize_child = |c| {
                    let child_size = self.resize(c, &remaining_size);
                    height += child_size.height;
                    max_width = std::cmp::max(child_size.width, max_width);
                    remaining_size.height = remaining_size
                        .height
                        .saturating_sub(child_size.height + spacing);
                };
                // First resize all non-Fill items; Fill items get the remaining space.
                for item in items
                    .iter()
                    .filter(|i| i.style.vertical_alignment != Alignment::Fill)
                {
                    resize_child(item);
                }
                for item in items
                    .iter()
                    .filter(|i| i.style.vertical_alignment == Alignment::Fill)
                {
                    resize_child(item);
                }
                content_size = Some(Size {
                    width: max_width,
                    height: height + spacing * (items.len().saturating_sub(1) as u32),
                });
            }
            HBox(model::HBox {
                items,
                spacing,
                affirmative_order: _,
            }) => {
                let mut width = 0;
                let mut max_height = 0;
                let mut remaining_size = inner_size.clone();
                let mut resize_child = |c| {
                    let child_size = self.resize(c, &remaining_size);
                    width += child_size.width;
                    max_height = std::cmp::max(child_size.height, max_height);
                    remaining_size.width = remaining_size
                        .width
                        .saturating_sub(child_size.width + spacing);
                };
                // First resize all non-Fill items; Fill items get the remaining space.
                for item in items
                    .iter()
                    .filter(|i| i.style.horizontal_alignment != Alignment::Fill)
                {
                    resize_child(item);
                }
                for item in items
                    .iter()
                    .filter(|i| i.style.horizontal_alignment == Alignment::Fill)
                {
                    resize_child(item);
                }
                content_size = Some(Size {
                    width: width + spacing * (items.len().saturating_sub(1) as u32),
                    height: max_height,
                });
            }
            Scroll(model::Scroll {
                content: Some(content),
            }) => {
                content_size = Some(self.resize(content, &inner_size));
            }
            Progress(model::Progress { .. }) => {
                // Min size recommended by windows uxguide
                content_size = Some(Size {
                    width: 160,
                    height: 15,
                });
            }
            // We don't support sizing by textbox content yet (need to read from the HWND due to
            // Property::ReadOnly).
            TextBox(_) => (),
            _ => (),
        }

        // Adjust from content size.
        if let Some(content_size) = content_size {
            inner_size.from_content_size(style, &content_size);
        }

        // Compute/store (outer) size and return.
        let size = inner_size.plus_margin(&style.margin);
        self.sizes.insert(ElementRef::new(element), size);
        size
    }

    fn get_size(&self, element: &Element) -> &Size {
        self.sizes
            .get(&ElementRef::new(element))
            .expect("element not resized")
    }

    fn reposition(&mut self, element: &Element, position: &Position, parent_size: &Size) {
        let style = &element.style;
        if !is_visible(style) {
            return;
        }
        let size = self.get_size(element);

        let start_offset = match style.horizontal_alignment {
            Alignment::Fill | Alignment::Start => 0,
            Alignment::Center => parent_size.width.saturating_sub(size.width) / 2,
            Alignment::End => parent_size.width.saturating_sub(size.width),
        };
        let top_offset = match style.vertical_alignment {
            Alignment::Fill | Alignment::Start => 0,
            Alignment::Center => parent_size.height.saturating_sub(size.height) / 2,
            Alignment::End => parent_size.height.saturating_sub(size.height),
        };

        let inner_position = Position {
            start: position.start + start_offset,
            top: position.top + top_offset,
        }
        .less_margin(&style.margin);
        let inner_size = size.less_margin(&style.margin);

        // Set the window size/position if there is a handle associated with the element.
        if let Some(&hwnd) = self.elements.get(&ElementRef::new(element)) {
            unsafe {
                win::SetWindowPos(
                    hwnd,
                    self.last_positioned.unwrap_or(win::HWND_TOP),
                    inner_position.start.try_into().unwrap(),
                    inner_position.top.try_into().unwrap(),
                    inner_size.width.try_into().unwrap(),
                    inner_size.height.try_into().unwrap(),
                    0,
                );
                Gdi::InvalidateRect(hwnd, std::ptr::null(), 1);
            }
            self.last_positioned = Some(hwnd);
        }

        // Reposition content.
        match &element.element_type {
            model::ElementType::VBox(model::VBox { items, spacing }) => {
                let mut position = inner_position;
                let mut size = inner_size;
                for item in items {
                    self.reposition(item, &position, &size);
                    let consumed = self.get_size(item).height + spacing;
                    if item.style.vertical_alignment != Alignment::End {
                        position.top += consumed;
                    }
                    size.height = size.height.saturating_sub(consumed);
                }
            }
            model::ElementType::HBox(model::HBox {
                items,
                spacing,
                // The default ordering matches the windows platform order
                affirmative_order: _,
            }) => {
                let mut position = inner_position;
                let mut size = inner_size;
                for item in items {
                    self.reposition(item, &position, &inner_size);
                    let consumed = self.get_size(item).width + spacing;
                    if item.style.horizontal_alignment != Alignment::End {
                        position.start += consumed;
                    }
                    size.width = size.width.saturating_sub(consumed);
                }
            }
            model::ElementType::Scroll(model::Scroll {
                content: Some(content),
            }) => {
                self.reposition(content, &inner_position, &inner_size);
            }
            _ => (),
        }
    }

    /// The `size` represents the maximum size permitted for the text (which is used for word
    /// breaking), and it will be set to the precise width and height of the text. The width should
    /// not exceed the input `size` width, but the height may.
    fn measure_text(&mut self, text: &str, element: &Element, size: &mut Size) {
        let Some(&window) = self.elements.get(&ElementRef::new(element)) else {
            return;
        };
        let hdc = unsafe { Gdi::GetDC(window) };
        unsafe { Gdi::SelectObject(hdc, win::SendMessageW(window, win::WM_GETFONT, 0, 0) as _) };
        let mut height: u32 = 0;
        let mut max_width: u32 = 0;
        let mut char_fit = 0i32;
        let mut win_size = unsafe { std::mem::zeroed::<SIZE>() };
        for mut line in text.lines() {
            if line.is_empty() {
                line = " ";
            }
            let text = WideString::new(line);
            let mut text = text.as_slice();
            let mut extents = vec![0i32; text.len()];
            while !text.is_empty() {
                unsafe {
                    Gdi::GetTextExtentExPointW(
                        hdc,
                        text.as_ptr(),
                        text.len() as i32,
                        size.width.try_into().unwrap(),
                        &mut char_fit,
                        extents.as_mut_ptr(),
                        &mut win_size,
                    );
                }
                if char_fit == 0 {
                    return;
                }
                let mut split = char_fit as usize;
                let mut split_end = split.saturating_sub(1);
                if (char_fit as usize) < text.len() {
                    for i in (0..char_fit as usize).rev() {
                        // FIXME safer utf16 handling?
                        if text[i] == b' ' as u16 {
                            split = i + 1;
                            split_end = i.saturating_sub(1);
                            break;
                        }
                    }
                }
                text = &text[split..];
                max_width = std::cmp::max(max_width, extents[split_end].try_into().unwrap());
                let measured_height: u32 = win_size.cy.try_into().unwrap();
                height += measured_height;
            }
        }
        unsafe { Gdi::ReleaseDC(window, hdc) };

        assert!(max_width <= size.width);
        size.width = max_width;
        size.height = height;
    }
}

#[derive(Debug, Default, Clone, Copy)]
struct Size {
    pub width: u32,
    pub height: u32,
}

impl Size {
    pub fn inner_size(&self, style: &ElementStyle) -> Self {
        let mut ret = self.less_margin(&style.margin);
        if let Some(width) = style.horizontal_size_request {
            ret.width = width;
        }
        if let Some(height) = style.vertical_size_request {
            ret.height = height;
        }
        ret
    }

    pub fn from_content_size(&mut self, style: &ElementStyle, content_size: &Self) {
        if style.horizontal_size_request < Some(content_size.width)
            && style.horizontal_alignment != Alignment::Fill
        {
            self.width = content_size.width;
        }
        if style.vertical_size_request < Some(content_size.height)
            && style.vertical_alignment != Alignment::Fill
        {
            self.height = content_size.height;
        }
    }

    pub fn plus_margin(&self, margin: &Margin) -> Self {
        let mut ret = self.clone();
        ret.width += margin.start + margin.end;
        ret.height += margin.top + margin.bottom;
        ret
    }

    pub fn less_margin(&self, margin: &Margin) -> Self {
        let mut ret = self.clone();
        ret.width = ret.width.saturating_sub(margin.start + margin.end);
        ret.height = ret.height.saturating_sub(margin.top + margin.bottom);
        ret
    }
}

#[derive(Debug, Default, Clone, Copy)]
struct Position {
    pub start: u32,
    pub top: u32,
}

impl Position {
    #[allow(dead_code)]
    pub fn plus_margin(&self, margin: &Margin) -> Self {
        let mut ret = self.clone();
        ret.start = ret.start.saturating_sub(margin.start);
        ret.top = ret.top.saturating_sub(margin.top);
        ret
    }

    pub fn less_margin(&self, margin: &Margin) -> Self {
        let mut ret = self.clone();
        ret.start += margin.start;
        ret.top += margin.top;
        ret
    }
}

fn is_visible(style: &ElementStyle) -> bool {
    match &style.visible {
        Property::Static(v) => *v,
        Property::Binding(s) => *s.borrow(),
        _ => true,
    }
}
