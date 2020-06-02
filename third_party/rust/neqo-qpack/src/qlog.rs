// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Functions that handle capturing QLOG traces.

use crate::Res;
use neqo_common::hex;
use neqo_common::qlog::NeqoQlog;
use qlog::{event::Event, QPackInstruction, QpackInstructionTypeName};

pub fn qpack_read_insert_count_increment_instruction(
    qlog: &mut Option<NeqoQlog>,
    increment: u64,
    data: &[u8],
) -> Res<()> {
    if let Some(qlog) = qlog {
        let event = Event::qpack_instruction_received(
            QPackInstruction::InsertCountIncrementInstruction {
                instruction_type: QpackInstructionTypeName::InsertCountIncrementInstruction,
                increment,
            },
            Some(8.to_string()),
            Some(hex(data)),
        );

        qlog.stream().add_event(event)?;
    }
    Ok(())
}
