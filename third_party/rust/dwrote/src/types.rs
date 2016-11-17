/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* this is include!()'d in lib.rs */
use std::mem;

// mirrors DWRITE_FONT_WEIGHT
#[repr(u32)]
#[derive(Deserialize, Serialize, PartialEq, Debug, Clone, Copy)]
pub enum FontWeight {
    Thin = 100,
    ExtraLight = 200,
    Light = 300,
    SemiLight = 350,
    Regular = 400,
    Medium = 500,
    SemiBold = 600,
    Bold = 700,
    ExtraBold = 800,
    Black = 900,
    ExtraBlack = 950,
}

impl FontWeight {
    fn t(&self) -> winapi::DWRITE_FONT_WEIGHT { unsafe { mem::transmute(*self) } }
    pub fn to_u32(&self) -> u32 { unsafe { mem::transmute(*self) } }
    pub fn from_u32(v: u32) -> FontStretch { unsafe { mem::transmute(v) } }
}

// mirrors DWRITE_FONT_STRETCH
#[repr(u32)]
#[derive(Deserialize, Serialize, PartialEq, Debug, Clone, Copy)]
pub enum FontStretch {
    Undefined = 0,
    UltraCondensed = 1,
    ExtraCondensed = 2,
    Condensed = 3,
    SemiCondensed = 4,
    Normal = 5,
    SemiExpanded = 6,
    Expanded = 7,
    ExtraExpanded = 8,
    UltraExpanded = 9,
}

impl FontStretch {
    fn t(&self) -> winapi::DWRITE_FONT_STRETCH { unsafe { mem::transmute(*self) } }
    pub fn to_u32(&self) -> u32 { unsafe { mem::transmute(*self) } }
    pub fn from_u32(v: u32) -> FontStretch { unsafe { mem::transmute(v) } }
}

// mirrors DWRITE_FONT_STYLE
#[repr(u32)]
#[derive(Deserialize, Serialize, PartialEq, Debug, Clone, Copy)]
pub enum FontStyle {
    Normal = 0,
    Oblique = 1,
    Italic = 2,
}

impl FontStyle {
    fn t(&self) -> winapi::DWRITE_FONT_STYLE { unsafe { mem::transmute(*self) } }
    pub fn to_u32(&self) -> u32 { unsafe { mem::transmute(*self) } }
    pub fn from_u32(v: u32) -> FontStretch { unsafe { mem::transmute(v) } }
}

#[derive(Deserialize, Serialize, PartialEq, Debug, Clone)]
pub struct FontDescriptor {
    pub family_name: String,
    pub weight: FontWeight,
    pub stretch: FontStretch,
    pub style: FontStyle,
}
