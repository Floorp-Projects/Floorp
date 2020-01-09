// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::{Error, Res};
use neqo_common::{Decoder, Encoder};
use std::ops::Deref;

type SettingsType = u64;

const SETTINGS_MAX_HEADER_LIST_SIZE: SettingsType = 0x6;
const SETTINGS_QPACK_MAX_TABLE_CAPACITY: SettingsType = 0x1;
const SETTINGS_QPACK_BLOCKED_STREAMS: SettingsType = 0x7;

#[derive(Clone, PartialEq, Debug, Copy)]
pub enum HSettingType {
    MaxHeaderListSize,
    MaxTableCapacity,
    BlockedStreams,
}

fn hsetting_default(setting_type: HSettingType) -> u64 {
    match setting_type {
        HSettingType::MaxHeaderListSize => 1 << 62,
        HSettingType::MaxTableCapacity => 0,
        HSettingType::BlockedStreams => 0,
    }
}

#[derive(Debug, Clone, PartialEq)]
pub struct HSetting {
    pub setting_type: HSettingType,
    pub value: u64,
}

impl HSetting {
    pub fn new(setting_type: HSettingType, value: u64) -> Self {
        HSetting {
            setting_type,
            value,
        }
    }
}

#[derive(Clone, Debug, Default, PartialEq)]
pub struct HSettings {
    settings: Vec<HSetting>,
}

impl HSettings {
    pub fn new(settings: &[HSetting]) -> Self {
        HSettings {
            settings: settings.to_vec(),
        }
    }

    pub fn get(&self, setting: HSettingType) -> u64 {
        match self.settings.iter().find(|s| s.setting_type == setting) {
            Some(v) => v.value,
            None => hsetting_default(setting),
        }
    }

    pub fn encode_frame_contents(&self, enc: &mut Encoder) {
        enc.encode_vvec_with(|enc_inner| {
            for iter in self.settings.iter() {
                match iter.setting_type {
                    HSettingType::MaxHeaderListSize => {
                        enc_inner.encode_varint(SETTINGS_MAX_HEADER_LIST_SIZE as u64);
                        enc_inner.encode_varint(iter.value);
                    }
                    HSettingType::MaxTableCapacity => {
                        enc_inner.encode_varint(SETTINGS_QPACK_MAX_TABLE_CAPACITY as u64);
                        enc_inner.encode_varint(iter.value);
                    }
                    HSettingType::BlockedStreams => {
                        enc_inner.encode_varint(SETTINGS_QPACK_BLOCKED_STREAMS as u64);
                        enc_inner.encode_varint(iter.value);
                    }
                }
            }
        });
    }

    pub fn decode_frame_contents(&mut self, dec: &mut Decoder) -> Res<()> {
        while dec.remaining() > 0 {
            let t = dec.decode_varint();
            let v = dec.decode_varint();

            match (t, v) {
                (Some(SETTINGS_MAX_HEADER_LIST_SIZE), Some(value)) => self
                    .settings
                    .push(HSetting::new(HSettingType::MaxHeaderListSize, value)),
                (Some(SETTINGS_QPACK_MAX_TABLE_CAPACITY), Some(value)) => self
                    .settings
                    .push(HSetting::new(HSettingType::MaxTableCapacity, value)),
                (Some(SETTINGS_QPACK_BLOCKED_STREAMS), Some(value)) => self
                    .settings
                    .push(HSetting::new(HSettingType::BlockedStreams, value)),
                // other supported settings here
                (Some(_), Some(_)) => {} // ignore unknown setting, it is fine.
                _ => return Err(Error::NotEnoughData),
            };
        }
        Ok(())
    }
}

impl Deref for HSettings {
    type Target = [HSetting];
    fn deref(&self) -> &Self::Target {
        &self.settings
    }
}
