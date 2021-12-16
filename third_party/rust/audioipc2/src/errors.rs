// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

error_chain! {
    // Maybe replace with chain_err to improve the error info.
    foreign_links {
        Bincode(bincode::Error);
        Io(std::io::Error);
        Cubeb(cubeb::Error);
    }

    // Replace bail!(str) with explicit errors.
    errors {
        Disconnected
    }
}
