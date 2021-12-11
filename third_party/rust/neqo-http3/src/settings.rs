// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(clippy::module_name_repetitions)]

use crate::{Error, Res};
use neqo_common::{Decoder, Encoder};
use neqo_crypto::{ZeroRttCheckResult, ZeroRttChecker};
use neqo_qpack::QpackSettings;
use std::ops::Deref;

type SettingsType = u64;

/// Increment this version number if a new setting is added and that might
/// cause 0-RTT to be accepted where shouldn't be.
const SETTINGS_ZERO_RTT_VERSION: u64 = 1;

const SETTINGS_MAX_HEADER_LIST_SIZE: SettingsType = 0x6;
const SETTINGS_QPACK_MAX_TABLE_CAPACITY: SettingsType = 0x1;
const SETTINGS_QPACK_BLOCKED_STREAMS: SettingsType = 0x7;

pub const H3_RESERVED_SETTINGS: &[SettingsType] = &[0x2, 0x3, 0x4, 0x5];

#[derive(Clone, PartialEq, Debug, Copy)]
pub enum HSettingType {
    MaxHeaderListSize,
    MaxTableCapacity,
    BlockedStreams,
}

fn hsetting_default(setting_type: HSettingType) -> u64 {
    match setting_type {
        HSettingType::MaxHeaderListSize => 1 << 62,
        HSettingType::MaxTableCapacity | HSettingType::BlockedStreams => 0,
    }
}

#[derive(Debug, Clone, PartialEq)]
pub struct HSetting {
    pub setting_type: HSettingType,
    pub value: u64,
}

impl HSetting {
    pub fn new(setting_type: HSettingType, value: u64) -> Self {
        Self {
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
        Self {
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
            for iter in &self.settings {
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

            if let Some(settings_type) = t {
                if H3_RESERVED_SETTINGS.contains(&settings_type) {
                    return Err(Error::HttpSettings);
                }
            }
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

#[derive(Debug)]
pub struct HttpZeroRttChecker {
    settings: QpackSettings,
}

impl HttpZeroRttChecker {
    /// Right now we only have QPACK settings, so that is all this takes.
    #[must_use]
    pub fn new(settings: QpackSettings) -> Self {
        Self { settings }
    }

    /// Save the settings that matter for 0-RTT.
    #[must_use]
    pub fn save(settings: QpackSettings) -> Vec<u8> {
        let mut enc = Encoder::new();
        enc.encode_varint(SETTINGS_ZERO_RTT_VERSION)
            .encode_varint(SETTINGS_QPACK_MAX_TABLE_CAPACITY)
            .encode_varint(settings.max_table_size_decoder)
            .encode_varint(SETTINGS_QPACK_BLOCKED_STREAMS)
            .encode_varint(settings.max_blocked_streams);
        enc.into()
    }
}

impl ZeroRttChecker for HttpZeroRttChecker {
    fn check(&self, token: &[u8]) -> ZeroRttCheckResult {
        let mut dec = Decoder::from(token);

        // Read and check the version.
        if let Some(version) = dec.decode_varint() {
            if version != SETTINGS_ZERO_RTT_VERSION {
                return ZeroRttCheckResult::Reject;
            }
        } else {
            return ZeroRttCheckResult::Fail;
        }

        // Now treat the rest as a settings frame.
        let mut settings = HSettings::new(&[]);
        if settings.decode_frame_contents(&mut dec).is_err() {
            return ZeroRttCheckResult::Fail;
        }
        if settings.iter().all(|setting| match setting.setting_type {
            HSettingType::BlockedStreams => {
                u64::from(self.settings.max_blocked_streams) >= setting.value
            }
            HSettingType::MaxTableCapacity => self.settings.max_table_size_decoder >= setting.value,
            HSettingType::MaxHeaderListSize => true,
        }) {
            ZeroRttCheckResult::Accept
        } else {
            ZeroRttCheckResult::Reject
        }
    }
}
