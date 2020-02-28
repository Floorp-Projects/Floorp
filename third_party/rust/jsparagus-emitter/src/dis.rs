use crate::opcode::Opcode;
use std::convert::TryFrom;
use std::fmt::Write;

/// Return a string form of the given bytecode.
pub fn dis(bc: &[u8]) -> String {
    let mut result = String::new();
    let mut iter = bc.iter();
    loop {
        let len = match iter.next() {
            Some(byte) => match Opcode::try_from(*byte) {
                Ok(op) => {
                    write!(&mut result, "{:?}", op).unwrap();
                    op.instruction_length()
                }
                Err(()) => {
                    write!(&mut result, "{}", byte).unwrap();
                    1
                }
            },
            None => break,
        };

        for _ in 1..len {
            write!(&mut result, " {}", iter.next().unwrap()).unwrap();
        }

        writeln!(&mut result).unwrap();
    }

    result
}
