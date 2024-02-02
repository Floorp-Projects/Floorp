// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{mem, time::Instant};

use neqo_common::{qdebug, qinfo, Datagram};

use crate::crypto::CryptoSpace;

/// The number of datagrams that are saved during the handshake when
/// keys to decrypt them are not yet available.
const MAX_SAVED_DATAGRAMS: usize = 4;

pub struct SavedDatagram {
    /// The datagram.
    pub d: Datagram,
    /// The time that the datagram was received.
    pub t: Instant,
}

#[derive(Default)]
pub struct SavedDatagrams {
    handshake: Vec<SavedDatagram>,
    application_data: Vec<SavedDatagram>,
    available: Option<CryptoSpace>,
}

impl SavedDatagrams {
    fn store(&mut self, cspace: CryptoSpace) -> &mut Vec<SavedDatagram> {
        match cspace {
            CryptoSpace::Handshake => &mut self.handshake,
            CryptoSpace::ApplicationData => &mut self.application_data,
            _ => panic!("unexpected space"),
        }
    }

    pub fn save(&mut self, cspace: CryptoSpace, d: Datagram, t: Instant) {
        let store = self.store(cspace);

        if store.len() < MAX_SAVED_DATAGRAMS {
            qdebug!("saving datagram of {} bytes", d.len());
            store.push(SavedDatagram { d, t });
        } else {
            qinfo!("not saving datagram of {} bytes", d.len());
        }
    }

    pub fn make_available(&mut self, cspace: CryptoSpace) {
        debug_assert_ne!(cspace, CryptoSpace::ZeroRtt);
        debug_assert_ne!(cspace, CryptoSpace::Initial);
        if !self.store(cspace).is_empty() {
            self.available = Some(cspace);
        }
    }

    pub fn available(&self) -> Option<CryptoSpace> {
        self.available
    }

    pub fn take_saved(&mut self) -> Vec<SavedDatagram> {
        self.available
            .take()
            .map_or_else(Vec::new, |cspace| mem::take(self.store(cspace)))
    }
}
