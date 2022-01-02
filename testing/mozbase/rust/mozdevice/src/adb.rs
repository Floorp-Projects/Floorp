/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[derive(Debug, PartialEq)]
pub enum SyncCommand {
    Data,
    Dent,
    Done,
    Fail,
    List,
    Okay,
    Quit,
    Recv,
    Send,
    Stat,
}

impl SyncCommand {
    // Returns the byte serialisation of the protocol status.
    pub fn code(&self) -> &'static [u8; 4] {
        use self::SyncCommand::*;
        match *self {
            Data => b"DATA",
            Dent => b"DENT",
            Done => b"DONE",
            Fail => b"FAIL",
            List => b"LIST",
            Okay => b"OKAY",
            Quit => b"QUIT",
            Recv => b"RECV",
            Send => b"SEND",
            Stat => b"STAT",
        }
    }
}

pub type DeviceSerial = String;
