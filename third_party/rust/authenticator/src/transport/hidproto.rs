/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Shared code for platforms that use raw HID access (Linux, FreeBSD, etc.)

#![cfg_attr(
    feature = "cargo-clippy",
    allow(clippy::cast_lossless, clippy::needless_lifetimes)
)]

#[cfg(target_os = "linux")]
use std::io;
use std::mem;

use crate::consts::{FIDO_USAGE_PAGE, FIDO_USAGE_U2FHID};
#[cfg(target_os = "linux")]
use crate::consts::{INIT_HEADER_SIZE, MAX_HID_RPT_SIZE};

// The 4 MSBs (the tag) are set when it's a long item.
const HID_MASK_LONG_ITEM_TAG: u8 = 0b1111_0000;
// The 2 LSBs denote the size of a short item.
const HID_MASK_SHORT_ITEM_SIZE: u8 = 0b0000_0011;
// The 6 MSBs denote the tag (4) and type (2).
const HID_MASK_ITEM_TAGTYPE: u8 = 0b1111_1100;
// tag=0000, type=10 (local)
const HID_ITEM_TAGTYPE_USAGE: u8 = 0b0000_1000;
// tag=0000, type=01 (global)
const HID_ITEM_TAGTYPE_USAGE_PAGE: u8 = 0b0000_0100;
// tag=1000, type=00 (main)
const HID_ITEM_TAGTYPE_INPUT: u8 = 0b1000_0000;
// tag=1001, type=00 (main)
const HID_ITEM_TAGTYPE_OUTPUT: u8 = 0b1001_0000;
// tag=1001, type=01 (global)
const HID_ITEM_TAGTYPE_REPORT_COUNT: u8 = 0b1001_0100;

pub struct ReportDescriptor {
    pub value: Vec<u8>,
}

impl ReportDescriptor {
    fn iter(self) -> ReportDescriptorIterator {
        ReportDescriptorIterator::new(self)
    }
}

#[derive(Debug)]
pub enum Data {
    UsagePage { data: u32 },
    Usage { data: u32 },
    Input,
    Output,
    ReportCount { data: u32 },
}

pub struct ReportDescriptorIterator {
    desc: ReportDescriptor,
    pos: usize,
}

impl ReportDescriptorIterator {
    fn new(desc: ReportDescriptor) -> Self {
        Self { desc, pos: 0 }
    }

    fn next_item(&mut self) -> Option<Data> {
        let item = get_hid_item(&self.desc.value[self.pos..]);
        if item.is_none() {
            self.pos = self.desc.value.len(); // Close, invalid data.
            return None;
        }

        let (tag_type, key_len, data) = item.unwrap();

        // Advance if we have a valid item.
        self.pos += key_len + data.len();

        // We only check short items.
        if key_len > 1 {
            return None; // Check next item.
        }

        // Short items have max. length of 4 bytes.
        assert!(data.len() <= mem::size_of::<u32>());

        // Convert data bytes to a uint.
        let data = read_uint_le(data);
        match tag_type {
            HID_ITEM_TAGTYPE_USAGE_PAGE => Some(Data::UsagePage { data }),
            HID_ITEM_TAGTYPE_USAGE => Some(Data::Usage { data }),
            HID_ITEM_TAGTYPE_INPUT => Some(Data::Input),
            HID_ITEM_TAGTYPE_OUTPUT => Some(Data::Output),
            HID_ITEM_TAGTYPE_REPORT_COUNT => Some(Data::ReportCount { data }),
            _ => None,
        }
    }
}

impl Iterator for ReportDescriptorIterator {
    type Item = Data;

    fn next(&mut self) -> Option<Self::Item> {
        if self.pos >= self.desc.value.len() {
            return None;
        }

        self.next_item().or_else(|| self.next())
    }
}

fn get_hid_item<'a>(buf: &'a [u8]) -> Option<(u8, usize, &'a [u8])> {
    if (buf[0] & HID_MASK_LONG_ITEM_TAG) == HID_MASK_LONG_ITEM_TAG {
        get_hid_long_item(buf)
    } else {
        get_hid_short_item(buf)
    }
}

fn get_hid_long_item<'a>(buf: &'a [u8]) -> Option<(u8, usize, &'a [u8])> {
    // A valid long item has at least three bytes.
    if buf.len() < 3 {
        return None;
    }

    let len = buf[1] as usize;

    // Ensure that there are enough bytes left in the buffer.
    if len > buf.len() - 3 {
        return None;
    }

    Some((buf[2], 3 /* key length */, &buf[3..]))
}

fn get_hid_short_item<'a>(buf: &'a [u8]) -> Option<(u8, usize, &'a [u8])> {
    // This is a short item. The bottom two bits of the key
    // contain the length of the data section (value) for this key.
    let len = match buf[0] & HID_MASK_SHORT_ITEM_SIZE {
        s @ 0..=2 => s as usize,
        _ => 4, /* _ == 3 */
    };

    // Ensure that there are enough bytes left in the buffer.
    if len > buf.len() - 1 {
        return None;
    }

    Some((
        buf[0] & HID_MASK_ITEM_TAGTYPE,
        1, /* key length */
        &buf[1..=len],
    ))
}

fn read_uint_le(buf: &[u8]) -> u32 {
    assert!(buf.len() <= 4);
    // Parse the number in little endian byte order.
    buf.iter()
        .rev()
        .fold(0, |num, b| (num << 8) | (u32::from(*b)))
}

pub fn has_fido_usage(desc: ReportDescriptor) -> bool {
    let mut usage_page = None;
    let mut usage = None;

    for data in desc.iter() {
        match data {
            Data::UsagePage { data } => usage_page = Some(data),
            Data::Usage { data } => usage = Some(data),
            _ => {}
        }

        // Check the values we found.
        if let (Some(usage_page), Some(usage)) = (usage_page, usage) {
            return usage_page == u32::from(FIDO_USAGE_PAGE)
                && usage == u32::from(FIDO_USAGE_U2FHID);
        }
    }

    false
}

#[cfg(target_os = "linux")]
pub fn read_hid_rpt_sizes(desc: ReportDescriptor) -> io::Result<(usize, usize)> {
    let mut in_rpt_count = None;
    let mut out_rpt_count = None;
    let mut last_rpt_count = None;

    for data in desc.iter() {
        match data {
            Data::ReportCount { data } => {
                if last_rpt_count.is_some() {
                    return Err(io::Error::new(
                        io::ErrorKind::InvalidInput,
                        "Duplicate HID_ReportCount",
                    ));
                }
                last_rpt_count = Some(data as usize);
            }
            Data::Input => {
                if last_rpt_count.is_none() {
                    return Err(io::Error::new(
                        io::ErrorKind::InvalidInput,
                        "HID_Input should be preceded by HID_ReportCount",
                    ));
                }
                if in_rpt_count.is_some() {
                    return Err(io::Error::new(
                        io::ErrorKind::InvalidInput,
                        "Duplicate HID_ReportCount",
                    ));
                }
                in_rpt_count = last_rpt_count;
                last_rpt_count = None
            }
            Data::Output => {
                if last_rpt_count.is_none() {
                    return Err(io::Error::new(
                        io::ErrorKind::InvalidInput,
                        "HID_Output should be preceded by HID_ReportCount",
                    ));
                }
                if out_rpt_count.is_some() {
                    return Err(io::Error::new(
                        io::ErrorKind::InvalidInput,
                        "Duplicate HID_ReportCount",
                    ));
                }
                out_rpt_count = last_rpt_count;
                last_rpt_count = None;
            }
            _ => {}
        }
    }

    match (in_rpt_count, out_rpt_count) {
        (Some(in_count), Some(out_count)) => {
            if in_count > INIT_HEADER_SIZE
                && in_count <= MAX_HID_RPT_SIZE
                && out_count > INIT_HEADER_SIZE
                && out_count <= MAX_HID_RPT_SIZE
            {
                Ok((in_count, out_count))
            } else {
                Err(io::Error::new(
                    io::ErrorKind::InvalidData,
                    "Report size is too small or too large",
                ))
            }
        }
        _ => Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "Failed to extract report sizes from report descriptor",
        )),
    }
}
