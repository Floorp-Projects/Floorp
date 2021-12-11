// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Some access to internal connection stuff for testing purposes.

use crate::packet::PacketBuilder;

pub trait FrameWriter {
    fn write_frames(&mut self, builder: &mut PacketBuilder);
}
