// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Functions that handle capturing QLOG traces.

use neqo_common::hex;
use neqo_common::qlog::NeqoQlog;
use qlog::events::{
    qpack::QpackInstructionTypeName,
    qpack::{QPackInstruction, QpackInstructionParsed},
    EventData, RawInfo,
};

pub fn qpack_read_insert_count_increment_instruction(
    qlog: &mut NeqoQlog,
    increment: u64,
    data: &[u8],
) {
    qlog.add_event_data(|| {
        let raw = RawInfo {
            length: Some(8),
            payload_length: None,
            data: Some(hex(data)),
        };
        let ev_data = EventData::QpackInstructionParsed(QpackInstructionParsed {
            instruction: QPackInstruction::InsertCountIncrementInstruction {
                instruction_type: QpackInstructionTypeName::InsertCountIncrementInstruction,
                increment,
            },
            raw: Some(raw),
        });

        Some(ev_data)
    });
}
