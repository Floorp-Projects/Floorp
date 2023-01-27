// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

macro_rules! check_parser_state {
    ( $src:expr ) => {
        if $src.limit() > 0 {
            debug!("bad parser state: {} content bytes left", $src.limit());
            return Err(Error::InvalidData("unread box content or bad parser sync"));
        }
    };
}
