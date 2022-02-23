// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::enc_dec_wtframe;
use crate::frames::WebTransportFrame;

#[test]
fn test_wt_close_session() {
    let f = WebTransportFrame::CloseSession {
        error: 5,
        message: "Hello".to_string(),
    };
    enc_dec_wtframe(&f, "6843090000000548656c6c6f", 0);
}
