// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::{Error, Res};
use neqo_common::qdebug;
use std::convert::TryFrom;

pub type WireVersion = u32;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum Version {
    Version2,
    Version1,
    Draft29,
    Draft30,
    Draft31,
    Draft32,
}

impl Version {
    pub const fn wire_version(self) -> WireVersion {
        match self {
            Self::Version2 => 0x709a50c4,
            Self::Version1 => 1,
            Self::Draft29 => 0xff00_0000 + 29,
            Self::Draft30 => 0xff00_0000 + 30,
            Self::Draft31 => 0xff00_0000 + 31,
            Self::Draft32 => 0xff00_0000 + 32,
        }
    }

    pub(crate) fn initial_salt(self) -> &'static [u8] {
        const INITIAL_SALT_V2: &[u8] = &[
            0xa7, 0x07, 0xc2, 0x03, 0xa5, 0x9b, 0x47, 0x18, 0x4a, 0x1d, 0x62, 0xca, 0x57, 0x04,
            0x06, 0xea, 0x7a, 0xe3, 0xe5, 0xd3,
        ];
        const INITIAL_SALT_V1: &[u8] = &[
            0x38, 0x76, 0x2c, 0xf7, 0xf5, 0x59, 0x34, 0xb3, 0x4d, 0x17, 0x9a, 0xe6, 0xa4, 0xc8,
            0x0c, 0xad, 0xcc, 0xbb, 0x7f, 0x0a,
        ];
        const INITIAL_SALT_29_32: &[u8] = &[
            0xaf, 0xbf, 0xec, 0x28, 0x99, 0x93, 0xd2, 0x4c, 0x9e, 0x97, 0x86, 0xf1, 0x9c, 0x61,
            0x11, 0xe0, 0x43, 0x90, 0xa8, 0x99,
        ];
        match self {
            Self::Version2 => INITIAL_SALT_V2,
            Self::Version1 => INITIAL_SALT_V1,
            Self::Draft29 | Self::Draft30 | Self::Draft31 | Self::Draft32 => INITIAL_SALT_29_32,
        }
    }

    pub(crate) fn label_prefix(self) -> &'static str {
        match self {
            Self::Version2 => "quicv2 ",
            Self::Version1 | Self::Draft29 | Self::Draft30 | Self::Draft31 | Self::Draft32 => {
                "quic "
            }
        }
    }

    pub(crate) fn retry_secret(self) -> &'static [u8] {
        const RETRY_SECRET_29: &[u8] = &[
            0x8b, 0x0d, 0x37, 0xeb, 0x85, 0x35, 0x02, 0x2e, 0xbc, 0x8d, 0x76, 0xa2, 0x07, 0xd8,
            0x0d, 0xf2, 0x26, 0x46, 0xec, 0x06, 0xdc, 0x80, 0x96, 0x42, 0xc3, 0x0a, 0x8b, 0xaa,
            0x2b, 0xaa, 0xff, 0x4c,
        ];
        const RETRY_SECRET_V1: &[u8] = &[
            0xd9, 0xc9, 0x94, 0x3e, 0x61, 0x01, 0xfd, 0x20, 0x00, 0x21, 0x50, 0x6b, 0xcc, 0x02,
            0x81, 0x4c, 0x73, 0x03, 0x0f, 0x25, 0xc7, 0x9d, 0x71, 0xce, 0x87, 0x6e, 0xca, 0x87,
            0x6e, 0x6f, 0xca, 0x8e,
        ];
        const RETRY_SECRET_V2: &[u8] = &[
            0x34, 0x25, 0xc2, 0x0c, 0xf8, 0x87, 0x79, 0xdf, 0x2f, 0xf7, 0x1e, 0x8a, 0xbf, 0xa7,
            0x82, 0x49, 0x89, 0x1e, 0x76, 0x3b, 0xbe, 0xd2, 0xf1, 0x3c, 0x04, 0x83, 0x43, 0xd3,
            0x48, 0xc0, 0x60, 0xe2,
        ];
        match self {
            Self::Version2 => RETRY_SECRET_V2,
            Self::Version1 => RETRY_SECRET_V1,
            Self::Draft29 | Self::Draft30 | Self::Draft31 | Self::Draft32 => RETRY_SECRET_29,
        }
    }

    pub(crate) fn is_draft(self) -> bool {
        matches!(
            self,
            Self::Draft29 | Self::Draft30 | Self::Draft31 | Self::Draft32,
        )
    }

    /// Determine if `self` can be upgraded to `other` compatibly.
    pub fn is_compatible(self, other: Self) -> bool {
        self == other
            || matches!(
                (self, other),
                (Self::Version1, Self::Version2) | (Self::Version2, Self::Version1)
            )
    }

    pub fn all() -> Vec<Self> {
        vec![
            Self::Version2,
            Self::Version1,
            Self::Draft32,
            Self::Draft31,
            Self::Draft30,
            Self::Draft29,
        ]
    }

    pub fn compatible<'a>(
        self,
        all: impl IntoIterator<Item = &'a Self>,
    ) -> impl Iterator<Item = &'a Self> {
        all.into_iter().filter(move |&v| self.is_compatible(*v))
    }
}

impl Default for Version {
    fn default() -> Self {
        Self::Version1
    }
}

impl TryFrom<WireVersion> for Version {
    type Error = Error;

    fn try_from(wire: WireVersion) -> Res<Self> {
        if wire == 1 {
            Ok(Self::Version1)
        } else if wire == 0x709a50c4 {
            Ok(Self::Version2)
        } else if wire == 0xff00_0000 + 29 {
            Ok(Self::Draft29)
        } else if wire == 0xff00_0000 + 30 {
            Ok(Self::Draft30)
        } else if wire == 0xff00_0000 + 31 {
            Ok(Self::Draft31)
        } else if wire == 0xff00_0000 + 32 {
            Ok(Self::Draft32)
        } else {
            Err(Error::VersionNegotiation)
        }
    }
}

#[derive(Debug, Clone)]
pub struct VersionConfig {
    /// The version that a client uses to establish a connection.
    ///
    /// For a client, this is the version that is sent out in an Initial packet.
    /// A client that resumes will set this to the version from the original
    /// connection.
    /// A client that handles a Version Negotiation packet will be initialized with
    /// a version chosen from the packet, but it will then have this value overridden
    /// to match the original configuration so that the version negotiation can be
    /// authenticated.
    ///
    /// For a server `Connection`, this is the only type of Initial packet that
    /// can be accepted; the correct value is set by `Server`, see below.
    ///
    /// For a `Server`, this value is not used; if an Initial packet is received
    /// in a supported version (as listed in `versions`), new instances of
    /// `Connection` will be created with this value set to match what was received.
    ///
    /// An invariant here is that this version is always listed in `all`.
    initial: Version,
    /// The set of versions that are enabled, in preference order.  For a server,
    /// only the relative order of compatible versions matters.
    all: Vec<Version>,
}

impl VersionConfig {
    pub fn new(initial: Version, all: Vec<Version>) -> Self {
        assert!(all.contains(&initial));
        Self { initial, all }
    }

    pub fn initial(&self) -> Version {
        self.initial
    }

    pub fn all(&self) -> &[Version] {
        &self.all
    }

    /// Overwrite the initial value; used by the `Server` when handling new connections
    /// and by the client on resumption.
    pub(crate) fn set_initial(&mut self, initial: Version) {
        qdebug!(
            "Overwrite initial version {:?} ==> {:?}",
            self.initial,
            initial
        );
        assert!(self.all.contains(&initial));
        self.initial = initial;
    }

    pub fn compatible(&self) -> impl Iterator<Item = &Version> {
        self.initial.compatible(&self.all)
    }

    fn find_preferred<'a>(
        preferences: impl IntoIterator<Item = &'a Version>,
        vn: &[WireVersion],
    ) -> Option<Version> {
        for v in preferences {
            if vn.contains(&v.wire_version()) {
                return Some(*v);
            }
        }
        None
    }

    /// Determine the preferred version based on a version negotiation packet.
    pub(crate) fn preferred(&self, vn: &[WireVersion]) -> Option<Version> {
        Self::find_preferred(&self.all, vn)
    }

    /// Determine the preferred version based on a set of compatible versions.
    pub(crate) fn preferred_compatible(&self, vn: &[WireVersion]) -> Option<Version> {
        Self::find_preferred(self.compatible(), vn)
    }
}

impl Default for VersionConfig {
    fn default() -> Self {
        Self::new(Version::default(), Version::all())
    }
}
