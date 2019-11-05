use std::fmt::Display;
use std::fmt::Formatter;
use std::fmt::Result as FmtResult;

use core::ColorInterface;
use core::colors::Colorado;
use core::colors::ColorMode;
use core::StrMarker;
use core::symbols::Symbol;
use Style;

// Support multiple style
#[derive(Clone)]
pub struct CString {
    text: String,
    fg_color: Option<Colorado>,
    bg_color: Option<Colorado>,
    styles: Option<Vec<Style>>,
}

impl StrMarker for CString {
    fn to_str(&self) -> String {
        self.text.to_owned()
    }
    fn get_fg_color(&self) -> Option<Colorado> {
        self.fg_color.clone()
    }
    fn get_bg_color(&self) -> Option<Colorado> {
        self.bg_color.clone()
    }
    fn get_style(&self) -> Option<Vec<Style>> {
        self.styles.clone()
    }
}


impl CString {
    pub fn new<S: StrMarker>(cs: S) -> CString {
        CString {
            text: cs.to_str(),
            fg_color: cs.get_fg_color(),
            bg_color: cs.get_bg_color(),
            styles: cs.get_style(),
        }
    }
    pub fn create_by_text<S: StrMarker>(cs: S, t: String) -> CString {
        CString { text: t, ..CString::new(cs) }
    }
    pub fn create_by_fg<S: StrMarker, C: ColorInterface>(cs: S, color: C) -> CString {
        CString { fg_color: Some(Colorado::new(color)), ..CString::new(cs) }
    }
    pub fn create_by_bg<S: StrMarker, C: ColorInterface>(cs: S, color: C) -> CString {
        CString { bg_color: Some(Colorado::new(color)), ..CString::new(cs) }
    }
    pub fn create_by_style<S: StrMarker>(cs: S, style: Style) -> CString {
        CString {
            text: cs.to_str(),
            styles: match cs.get_style() {
                Some(mut v) => {
                    v.push(style);
                    Some(v)
                }
                _ => { Some(vec![style]) }
            },
            fg_color: cs.get_fg_color(),
            bg_color: cs.get_bg_color(),
        }
    }
}

impl Display for CString {
    fn fmt(&self, f: &mut Formatter) -> FmtResult {
        let mut is_colored = false;

        if self.bg_color.is_none() && self.fg_color.is_none() && self.styles.is_none() {
            write!(f, "{}", self.text)?;
            Ok(())
        } else {
            match &self.fg_color {
                Some(v) => {
                    is_colored = true;
                    match v.get_mode() {
                        ColorMode::SIMPLE => {
                            f.write_str(Symbol::Simple256Foreground.to_str())?;
                        }
                        ColorMode::RGB => {
                            f.write_str(Symbol::RgbForeground.to_str())?;
                        }
                        _ => {}
                    }
                    write!(f, "{}", v.get_color())?;
                }
                _ => {}
            }
            match &self.bg_color {
                Some(v) => {
                    if is_colored {
                        f.write_str(Symbol::Mode.to_str())?;
                    } else {
                        is_colored = true;
                    }
                    match v.get_mode() {
                        ColorMode::SIMPLE => {
                            f.write_str(Symbol::Simple256Background.to_str())?;
                        }
                        ColorMode::RGB => {
                            f.write_str(Symbol::RgbBackground.to_str())?;
                        }
                        _ => {}
                    }
                    write!(f, "{}", v.get_color())?;
                }
                _ => {}
            }

            match &self.styles {
                Some(v) => {
                    if !is_colored { // pure style without color
                        write!(f, "{}{}", Symbol::Esc, Symbol::LeftBrackets)?;
                    } else {
                        f.write_str(Symbol::Semicolon.to_str())?;
                    }
                    let t: Vec<String> = v.into_iter().map(|x| x.to_string()).collect();
                    f.write_str(&t.join(";")[..])?;
                }
                _ => {}
            }
            f.write_str(Symbol::Mode.to_str())?;
            write!(f, "{}", self.text)?;
            f.write_str(Symbol::Reset.to_str())?;
            Ok(())
        }
    }
}
