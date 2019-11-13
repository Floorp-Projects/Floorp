#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum Style {
    Bold,
    Dim,
    Underlined,
    Blink,
    // invert the foreground and background colors
    Reverse,
    // useful for passwords
    Hidden,
}

impl Style {
    pub fn to_string(&self) -> String {
        match self {
            Style::Bold => String::from("1"),
            Style::Dim => String::from("2"),
            Style::Underlined => String::from("4"),
            Style::Blink => String::from("5"),
            Style::Reverse => String::from("7"),
            Style::Hidden => String::from("8"),
        }
    }
}
