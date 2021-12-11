use crate::ir::types::*;
use crate::isa::aarch64::inst::*;
use crate::isa::test_utils;
use crate::isa::CallConv;
use crate::settings;

use alloc::boxed::Box;
use alloc::vec::Vec;

#[test]
fn test_aarch64_binemit() {
    let mut insns = Vec::<(Inst, &str, &str)>::new();

    // N.B.: the architecture is little-endian, so when transcribing the 32-bit
    // hex instructions from e.g. objdump disassembly, one must swap the bytes
    // seen below. (E.g., a `ret` is normally written as the u32 `D65F03C0`,
    // but we write it here as C0035FD6.)

    // Useful helper script to produce the encodings from the text:
    //
    //      #!/bin/sh
    //      tmp=`mktemp /tmp/XXXXXXXX.o`
    //      aarch64-linux-gnu-as /dev/stdin -o $tmp
    //      aarch64-linux-gnu-objdump -d $tmp
    //      rm -f $tmp
    //
    // Then:
    //
    //      $ echo "mov x1, x2" | aarch64inst.sh
    insns.push((Inst::Ret, "C0035FD6", "ret"));
    insns.push((Inst::Nop0, "", "nop-zero-len"));
    insns.push((Inst::Nop4, "1F2003D5", "nop"));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::Add32,
            rd: writable_xreg(1),
            rn: xreg(2),
            rm: xreg(3),
        },
        "4100030B",
        "add w1, w2, w3",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::Add64,
            rd: writable_xreg(4),
            rn: xreg(5),
            rm: xreg(6),
        },
        "A400068B",
        "add x4, x5, x6",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::Sub32,
            rd: writable_xreg(1),
            rn: xreg(2),
            rm: xreg(3),
        },
        "4100034B",
        "sub w1, w2, w3",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::Sub64,
            rd: writable_xreg(4),
            rn: xreg(5),
            rm: xreg(6),
        },
        "A40006CB",
        "sub x4, x5, x6",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::Orr32,
            rd: writable_xreg(1),
            rn: xreg(2),
            rm: xreg(3),
        },
        "4100032A",
        "orr w1, w2, w3",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::Orr64,
            rd: writable_xreg(4),
            rn: xreg(5),
            rm: xreg(6),
        },
        "A40006AA",
        "orr x4, x5, x6",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::And32,
            rd: writable_xreg(1),
            rn: xreg(2),
            rm: xreg(3),
        },
        "4100030A",
        "and w1, w2, w3",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::And64,
            rd: writable_xreg(4),
            rn: xreg(5),
            rm: xreg(6),
        },
        "A400068A",
        "and x4, x5, x6",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::SubS32,
            rd: writable_zero_reg(),
            rn: xreg(2),
            rm: xreg(3),
        },
        "5F00036B",
        // TODO: Display as cmp
        "subs wzr, w2, w3",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::SubS32,
            rd: writable_xreg(1),
            rn: xreg(2),
            rm: xreg(3),
        },
        "4100036B",
        "subs w1, w2, w3",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::SubS64,
            rd: writable_xreg(4),
            rn: xreg(5),
            rm: xreg(6),
        },
        "A40006EB",
        "subs x4, x5, x6",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::AddS32,
            rd: writable_xreg(1),
            rn: xreg(2),
            rm: xreg(3),
        },
        "4100032B",
        "adds w1, w2, w3",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::AddS64,
            rd: writable_xreg(4),
            rn: xreg(5),
            rm: xreg(6),
        },
        "A40006AB",
        "adds x4, x5, x6",
    ));
    insns.push((
        Inst::AluRRImm12 {
            alu_op: ALUOp::AddS64,
            rd: writable_zero_reg(),
            rn: xreg(5),
            imm12: Imm12::maybe_from_u64(1).unwrap(),
        },
        "BF0400B1",
        // TODO: Display as cmn.
        "adds xzr, x5, #1",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::SDiv64,
            rd: writable_xreg(4),
            rn: xreg(5),
            rm: xreg(6),
        },
        "A40CC69A",
        "sdiv x4, x5, x6",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::UDiv64,
            rd: writable_xreg(4),
            rn: xreg(5),
            rm: xreg(6),
        },
        "A408C69A",
        "udiv x4, x5, x6",
    ));

    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::Eor32,
            rd: writable_xreg(4),
            rn: xreg(5),
            rm: xreg(6),
        },
        "A400064A",
        "eor w4, w5, w6",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::Eor64,
            rd: writable_xreg(4),
            rn: xreg(5),
            rm: xreg(6),
        },
        "A40006CA",
        "eor x4, x5, x6",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::AndNot32,
            rd: writable_xreg(4),
            rn: xreg(5),
            rm: xreg(6),
        },
        "A400260A",
        "bic w4, w5, w6",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::AndNot64,
            rd: writable_xreg(4),
            rn: xreg(5),
            rm: xreg(6),
        },
        "A400268A",
        "bic x4, x5, x6",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::OrrNot32,
            rd: writable_xreg(4),
            rn: xreg(5),
            rm: xreg(6),
        },
        "A400262A",
        "orn w4, w5, w6",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::OrrNot64,
            rd: writable_xreg(4),
            rn: xreg(5),
            rm: xreg(6),
        },
        "A40026AA",
        "orn x4, x5, x6",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::EorNot32,
            rd: writable_xreg(4),
            rn: xreg(5),
            rm: xreg(6),
        },
        "A400264A",
        "eon w4, w5, w6",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::EorNot64,
            rd: writable_xreg(4),
            rn: xreg(5),
            rm: xreg(6),
        },
        "A40026CA",
        "eon x4, x5, x6",
    ));

    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::RotR32,
            rd: writable_xreg(4),
            rn: xreg(5),
            rm: xreg(6),
        },
        "A42CC61A",
        "ror w4, w5, w6",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::RotR64,
            rd: writable_xreg(4),
            rn: xreg(5),
            rm: xreg(6),
        },
        "A42CC69A",
        "ror x4, x5, x6",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::Lsr32,
            rd: writable_xreg(4),
            rn: xreg(5),
            rm: xreg(6),
        },
        "A424C61A",
        "lsr w4, w5, w6",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::Lsr64,
            rd: writable_xreg(4),
            rn: xreg(5),
            rm: xreg(6),
        },
        "A424C69A",
        "lsr x4, x5, x6",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::Asr32,
            rd: writable_xreg(4),
            rn: xreg(5),
            rm: xreg(6),
        },
        "A428C61A",
        "asr w4, w5, w6",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::Asr64,
            rd: writable_xreg(4),
            rn: xreg(5),
            rm: xreg(6),
        },
        "A428C69A",
        "asr x4, x5, x6",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::Lsl32,
            rd: writable_xreg(4),
            rn: xreg(5),
            rm: xreg(6),
        },
        "A420C61A",
        "lsl w4, w5, w6",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::Lsl64,
            rd: writable_xreg(4),
            rn: xreg(5),
            rm: xreg(6),
        },
        "A420C69A",
        "lsl x4, x5, x6",
    ));

    insns.push((
        Inst::AluRRImm12 {
            alu_op: ALUOp::Add32,
            rd: writable_xreg(7),
            rn: xreg(8),
            imm12: Imm12 {
                bits: 0x123,
                shift12: false,
            },
        },
        "078D0411",
        "add w7, w8, #291",
    ));
    insns.push((
        Inst::AluRRImm12 {
            alu_op: ALUOp::Add32,
            rd: writable_xreg(7),
            rn: xreg(8),
            imm12: Imm12 {
                bits: 0x123,
                shift12: true,
            },
        },
        "078D4411",
        "add w7, w8, #1191936",
    ));
    insns.push((
        Inst::AluRRImm12 {
            alu_op: ALUOp::Add64,
            rd: writable_xreg(7),
            rn: xreg(8),
            imm12: Imm12 {
                bits: 0x123,
                shift12: false,
            },
        },
        "078D0491",
        "add x7, x8, #291",
    ));
    insns.push((
        Inst::AluRRImm12 {
            alu_op: ALUOp::Sub32,
            rd: writable_xreg(7),
            rn: xreg(8),
            imm12: Imm12 {
                bits: 0x123,
                shift12: false,
            },
        },
        "078D0451",
        "sub w7, w8, #291",
    ));
    insns.push((
        Inst::AluRRImm12 {
            alu_op: ALUOp::Sub64,
            rd: writable_xreg(7),
            rn: xreg(8),
            imm12: Imm12 {
                bits: 0x123,
                shift12: false,
            },
        },
        "078D04D1",
        "sub x7, x8, #291",
    ));
    insns.push((
        Inst::AluRRImm12 {
            alu_op: ALUOp::SubS32,
            rd: writable_xreg(7),
            rn: xreg(8),
            imm12: Imm12 {
                bits: 0x123,
                shift12: false,
            },
        },
        "078D0471",
        "subs w7, w8, #291",
    ));
    insns.push((
        Inst::AluRRImm12 {
            alu_op: ALUOp::SubS64,
            rd: writable_xreg(7),
            rn: xreg(8),
            imm12: Imm12 {
                bits: 0x123,
                shift12: false,
            },
        },
        "078D04F1",
        "subs x7, x8, #291",
    ));

    insns.push((
        Inst::AluRRRExtend {
            alu_op: ALUOp::Add32,
            rd: writable_xreg(7),
            rn: xreg(8),
            rm: xreg(9),
            extendop: ExtendOp::SXTB,
        },
        "0781290B",
        "add w7, w8, w9, SXTB",
    ));

    insns.push((
        Inst::AluRRRExtend {
            alu_op: ALUOp::Add64,
            rd: writable_xreg(15),
            rn: xreg(16),
            rm: xreg(17),
            extendop: ExtendOp::UXTB,
        },
        "0F02318B",
        "add x15, x16, x17, UXTB",
    ));

    insns.push((
        Inst::AluRRRExtend {
            alu_op: ALUOp::Sub32,
            rd: writable_xreg(1),
            rn: xreg(2),
            rm: xreg(3),
            extendop: ExtendOp::SXTH,
        },
        "41A0234B",
        "sub w1, w2, w3, SXTH",
    ));

    insns.push((
        Inst::AluRRRExtend {
            alu_op: ALUOp::Sub64,
            rd: writable_xreg(20),
            rn: xreg(21),
            rm: xreg(22),
            extendop: ExtendOp::UXTW,
        },
        "B44236CB",
        "sub x20, x21, x22, UXTW",
    ));

    insns.push((
        Inst::AluRRRShift {
            alu_op: ALUOp::Add32,
            rd: writable_xreg(10),
            rn: xreg(11),
            rm: xreg(12),
            shiftop: ShiftOpAndAmt::new(
                ShiftOp::LSL,
                ShiftOpShiftImm::maybe_from_shift(20).unwrap(),
            ),
        },
        "6A510C0B",
        "add w10, w11, w12, LSL 20",
    ));
    insns.push((
        Inst::AluRRRShift {
            alu_op: ALUOp::Add64,
            rd: writable_xreg(10),
            rn: xreg(11),
            rm: xreg(12),
            shiftop: ShiftOpAndAmt::new(
                ShiftOp::ASR,
                ShiftOpShiftImm::maybe_from_shift(42).unwrap(),
            ),
        },
        "6AA98C8B",
        "add x10, x11, x12, ASR 42",
    ));
    insns.push((
        Inst::AluRRRShift {
            alu_op: ALUOp::Sub32,
            rd: writable_xreg(10),
            rn: xreg(11),
            rm: xreg(12),
            shiftop: ShiftOpAndAmt::new(
                ShiftOp::LSL,
                ShiftOpShiftImm::maybe_from_shift(23).unwrap(),
            ),
        },
        "6A5D0C4B",
        "sub w10, w11, w12, LSL 23",
    ));
    insns.push((
        Inst::AluRRRShift {
            alu_op: ALUOp::Sub64,
            rd: writable_xreg(10),
            rn: xreg(11),
            rm: xreg(12),
            shiftop: ShiftOpAndAmt::new(
                ShiftOp::LSL,
                ShiftOpShiftImm::maybe_from_shift(23).unwrap(),
            ),
        },
        "6A5D0CCB",
        "sub x10, x11, x12, LSL 23",
    ));
    insns.push((
        Inst::AluRRRShift {
            alu_op: ALUOp::Orr32,
            rd: writable_xreg(10),
            rn: xreg(11),
            rm: xreg(12),
            shiftop: ShiftOpAndAmt::new(
                ShiftOp::LSL,
                ShiftOpShiftImm::maybe_from_shift(23).unwrap(),
            ),
        },
        "6A5D0C2A",
        "orr w10, w11, w12, LSL 23",
    ));
    insns.push((
        Inst::AluRRRShift {
            alu_op: ALUOp::Orr64,
            rd: writable_xreg(10),
            rn: xreg(11),
            rm: xreg(12),
            shiftop: ShiftOpAndAmt::new(
                ShiftOp::LSL,
                ShiftOpShiftImm::maybe_from_shift(23).unwrap(),
            ),
        },
        "6A5D0CAA",
        "orr x10, x11, x12, LSL 23",
    ));
    insns.push((
        Inst::AluRRRShift {
            alu_op: ALUOp::And32,
            rd: writable_xreg(10),
            rn: xreg(11),
            rm: xreg(12),
            shiftop: ShiftOpAndAmt::new(
                ShiftOp::LSL,
                ShiftOpShiftImm::maybe_from_shift(23).unwrap(),
            ),
        },
        "6A5D0C0A",
        "and w10, w11, w12, LSL 23",
    ));
    insns.push((
        Inst::AluRRRShift {
            alu_op: ALUOp::And64,
            rd: writable_xreg(10),
            rn: xreg(11),
            rm: xreg(12),
            shiftop: ShiftOpAndAmt::new(
                ShiftOp::LSL,
                ShiftOpShiftImm::maybe_from_shift(23).unwrap(),
            ),
        },
        "6A5D0C8A",
        "and x10, x11, x12, LSL 23",
    ));
    insns.push((
        Inst::AluRRRShift {
            alu_op: ALUOp::Eor32,
            rd: writable_xreg(10),
            rn: xreg(11),
            rm: xreg(12),
            shiftop: ShiftOpAndAmt::new(
                ShiftOp::LSL,
                ShiftOpShiftImm::maybe_from_shift(23).unwrap(),
            ),
        },
        "6A5D0C4A",
        "eor w10, w11, w12, LSL 23",
    ));
    insns.push((
        Inst::AluRRRShift {
            alu_op: ALUOp::Eor64,
            rd: writable_xreg(10),
            rn: xreg(11),
            rm: xreg(12),
            shiftop: ShiftOpAndAmt::new(
                ShiftOp::LSL,
                ShiftOpShiftImm::maybe_from_shift(23).unwrap(),
            ),
        },
        "6A5D0CCA",
        "eor x10, x11, x12, LSL 23",
    ));
    insns.push((
        Inst::AluRRRShift {
            alu_op: ALUOp::OrrNot32,
            rd: writable_xreg(10),
            rn: xreg(11),
            rm: xreg(12),
            shiftop: ShiftOpAndAmt::new(
                ShiftOp::LSL,
                ShiftOpShiftImm::maybe_from_shift(23).unwrap(),
            ),
        },
        "6A5D2C2A",
        "orn w10, w11, w12, LSL 23",
    ));
    insns.push((
        Inst::AluRRRShift {
            alu_op: ALUOp::OrrNot64,
            rd: writable_xreg(10),
            rn: xreg(11),
            rm: xreg(12),
            shiftop: ShiftOpAndAmt::new(
                ShiftOp::LSL,
                ShiftOpShiftImm::maybe_from_shift(23).unwrap(),
            ),
        },
        "6A5D2CAA",
        "orn x10, x11, x12, LSL 23",
    ));
    insns.push((
        Inst::AluRRRShift {
            alu_op: ALUOp::AndNot32,
            rd: writable_xreg(10),
            rn: xreg(11),
            rm: xreg(12),
            shiftop: ShiftOpAndAmt::new(
                ShiftOp::LSL,
                ShiftOpShiftImm::maybe_from_shift(23).unwrap(),
            ),
        },
        "6A5D2C0A",
        "bic w10, w11, w12, LSL 23",
    ));
    insns.push((
        Inst::AluRRRShift {
            alu_op: ALUOp::AndNot64,
            rd: writable_xreg(10),
            rn: xreg(11),
            rm: xreg(12),
            shiftop: ShiftOpAndAmt::new(
                ShiftOp::LSL,
                ShiftOpShiftImm::maybe_from_shift(23).unwrap(),
            ),
        },
        "6A5D2C8A",
        "bic x10, x11, x12, LSL 23",
    ));
    insns.push((
        Inst::AluRRRShift {
            alu_op: ALUOp::EorNot32,
            rd: writable_xreg(10),
            rn: xreg(11),
            rm: xreg(12),
            shiftop: ShiftOpAndAmt::new(
                ShiftOp::LSL,
                ShiftOpShiftImm::maybe_from_shift(23).unwrap(),
            ),
        },
        "6A5D2C4A",
        "eon w10, w11, w12, LSL 23",
    ));
    insns.push((
        Inst::AluRRRShift {
            alu_op: ALUOp::EorNot64,
            rd: writable_xreg(10),
            rn: xreg(11),
            rm: xreg(12),
            shiftop: ShiftOpAndAmt::new(
                ShiftOp::LSL,
                ShiftOpShiftImm::maybe_from_shift(23).unwrap(),
            ),
        },
        "6A5D2CCA",
        "eon x10, x11, x12, LSL 23",
    ));
    insns.push((
        Inst::AluRRRShift {
            alu_op: ALUOp::AddS32,
            rd: writable_xreg(10),
            rn: xreg(11),
            rm: xreg(12),
            shiftop: ShiftOpAndAmt::new(
                ShiftOp::LSL,
                ShiftOpShiftImm::maybe_from_shift(23).unwrap(),
            ),
        },
        "6A5D0C2B",
        "adds w10, w11, w12, LSL 23",
    ));
    insns.push((
        Inst::AluRRRShift {
            alu_op: ALUOp::AddS64,
            rd: writable_xreg(10),
            rn: xreg(11),
            rm: xreg(12),
            shiftop: ShiftOpAndAmt::new(
                ShiftOp::LSL,
                ShiftOpShiftImm::maybe_from_shift(23).unwrap(),
            ),
        },
        "6A5D0CAB",
        "adds x10, x11, x12, LSL 23",
    ));
    insns.push((
        Inst::AluRRRShift {
            alu_op: ALUOp::SubS32,
            rd: writable_xreg(10),
            rn: xreg(11),
            rm: xreg(12),
            shiftop: ShiftOpAndAmt::new(
                ShiftOp::LSL,
                ShiftOpShiftImm::maybe_from_shift(23).unwrap(),
            ),
        },
        "6A5D0C6B",
        "subs w10, w11, w12, LSL 23",
    ));
    insns.push((
        Inst::AluRRRShift {
            alu_op: ALUOp::SubS64,
            rd: writable_xreg(10),
            rn: xreg(11),
            rm: xreg(12),
            shiftop: ShiftOpAndAmt::new(
                ShiftOp::LSL,
                ShiftOpShiftImm::maybe_from_shift(23).unwrap(),
            ),
        },
        "6A5D0CEB",
        "subs x10, x11, x12, LSL 23",
    ));

    insns.push((
        Inst::AluRRRExtend {
            alu_op: ALUOp::SubS64,
            rd: writable_zero_reg(),
            rn: stack_reg(),
            rm: xreg(12),
            extendop: ExtendOp::UXTX,
        },
        "FF632CEB",
        "subs xzr, sp, x12, UXTX",
    ));

    insns.push((
        Inst::AluRRRR {
            alu_op: ALUOp3::MAdd32,
            rd: writable_xreg(1),
            rn: xreg(2),
            rm: xreg(3),
            ra: xreg(4),
        },
        "4110031B",
        "madd w1, w2, w3, w4",
    ));
    insns.push((
        Inst::AluRRRR {
            alu_op: ALUOp3::MAdd64,
            rd: writable_xreg(1),
            rn: xreg(2),
            rm: xreg(3),
            ra: xreg(4),
        },
        "4110039B",
        "madd x1, x2, x3, x4",
    ));
    insns.push((
        Inst::AluRRRR {
            alu_op: ALUOp3::MSub32,
            rd: writable_xreg(1),
            rn: xreg(2),
            rm: xreg(3),
            ra: xreg(4),
        },
        "4190031B",
        "msub w1, w2, w3, w4",
    ));
    insns.push((
        Inst::AluRRRR {
            alu_op: ALUOp3::MSub64,
            rd: writable_xreg(1),
            rn: xreg(2),
            rm: xreg(3),
            ra: xreg(4),
        },
        "4190039B",
        "msub x1, x2, x3, x4",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::SMulH,
            rd: writable_xreg(1),
            rn: xreg(2),
            rm: xreg(3),
        },
        "417C439B",
        "smulh x1, x2, x3",
    ));
    insns.push((
        Inst::AluRRR {
            alu_op: ALUOp::UMulH,
            rd: writable_xreg(1),
            rn: xreg(2),
            rm: xreg(3),
        },
        "417CC39B",
        "umulh x1, x2, x3",
    ));

    insns.push((
        Inst::AluRRImmShift {
            alu_op: ALUOp::RotR32,
            rd: writable_xreg(20),
            rn: xreg(21),
            immshift: ImmShift::maybe_from_u64(19).unwrap(),
        },
        "B44E9513",
        "ror w20, w21, #19",
    ));
    insns.push((
        Inst::AluRRImmShift {
            alu_op: ALUOp::RotR64,
            rd: writable_xreg(20),
            rn: xreg(21),
            immshift: ImmShift::maybe_from_u64(42).unwrap(),
        },
        "B4AAD593",
        "ror x20, x21, #42",
    ));
    insns.push((
        Inst::AluRRImmShift {
            alu_op: ALUOp::Lsr32,
            rd: writable_xreg(10),
            rn: xreg(11),
            immshift: ImmShift::maybe_from_u64(13).unwrap(),
        },
        "6A7D0D53",
        "lsr w10, w11, #13",
    ));
    insns.push((
        Inst::AluRRImmShift {
            alu_op: ALUOp::Lsr64,
            rd: writable_xreg(10),
            rn: xreg(11),
            immshift: ImmShift::maybe_from_u64(57).unwrap(),
        },
        "6AFD79D3",
        "lsr x10, x11, #57",
    ));
    insns.push((
        Inst::AluRRImmShift {
            alu_op: ALUOp::Asr32,
            rd: writable_xreg(4),
            rn: xreg(5),
            immshift: ImmShift::maybe_from_u64(7).unwrap(),
        },
        "A47C0713",
        "asr w4, w5, #7",
    ));
    insns.push((
        Inst::AluRRImmShift {
            alu_op: ALUOp::Asr64,
            rd: writable_xreg(4),
            rn: xreg(5),
            immshift: ImmShift::maybe_from_u64(35).unwrap(),
        },
        "A4FC6393",
        "asr x4, x5, #35",
    ));
    insns.push((
        Inst::AluRRImmShift {
            alu_op: ALUOp::Lsl32,
            rd: writable_xreg(8),
            rn: xreg(9),
            immshift: ImmShift::maybe_from_u64(24).unwrap(),
        },
        "281D0853",
        "lsl w8, w9, #24",
    ));
    insns.push((
        Inst::AluRRImmShift {
            alu_op: ALUOp::Lsl64,
            rd: writable_xreg(8),
            rn: xreg(9),
            immshift: ImmShift::maybe_from_u64(63).unwrap(),
        },
        "280141D3",
        "lsl x8, x9, #63",
    ));
    insns.push((
        Inst::AluRRImmShift {
            alu_op: ALUOp::Lsl32,
            rd: writable_xreg(10),
            rn: xreg(11),
            immshift: ImmShift::maybe_from_u64(0).unwrap(),
        },
        "6A7D0053",
        "lsl w10, w11, #0",
    ));
    insns.push((
        Inst::AluRRImmShift {
            alu_op: ALUOp::Lsl64,
            rd: writable_xreg(10),
            rn: xreg(11),
            immshift: ImmShift::maybe_from_u64(0).unwrap(),
        },
        "6AFD40D3",
        "lsl x10, x11, #0",
    ));

    insns.push((
        Inst::AluRRImmLogic {
            alu_op: ALUOp::And32,
            rd: writable_xreg(21),
            rn: xreg(27),
            imml: ImmLogic::maybe_from_u64(0x80003fff, I32).unwrap(),
        },
        "753B0112",
        "and w21, w27, #2147500031",
    ));
    insns.push((
        Inst::AluRRImmLogic {
            alu_op: ALUOp::And64,
            rd: writable_xreg(7),
            rn: xreg(6),
            imml: ImmLogic::maybe_from_u64(0x3fff80003fff800, I64).unwrap(),
        },
        "C7381592",
        "and x7, x6, #288221580125796352",
    ));
    insns.push((
        Inst::AluRRImmLogic {
            alu_op: ALUOp::Orr32,
            rd: writable_xreg(1),
            rn: xreg(5),
            imml: ImmLogic::maybe_from_u64(0x100000, I32).unwrap(),
        },
        "A1000C32",
        "orr w1, w5, #1048576",
    ));
    insns.push((
        Inst::AluRRImmLogic {
            alu_op: ALUOp::Orr64,
            rd: writable_xreg(4),
            rn: xreg(5),
            imml: ImmLogic::maybe_from_u64(0x8181818181818181, I64).unwrap(),
        },
        "A4C401B2",
        "orr x4, x5, #9331882296111890817",
    ));
    insns.push((
        Inst::AluRRImmLogic {
            alu_op: ALUOp::Eor32,
            rd: writable_xreg(1),
            rn: xreg(5),
            imml: ImmLogic::maybe_from_u64(0x00007fff, I32).unwrap(),
        },
        "A1380052",
        "eor w1, w5, #32767",
    ));
    insns.push((
        Inst::AluRRImmLogic {
            alu_op: ALUOp::Eor64,
            rd: writable_xreg(10),
            rn: xreg(8),
            imml: ImmLogic::maybe_from_u64(0x8181818181818181, I64).unwrap(),
        },
        "0AC501D2",
        "eor x10, x8, #9331882296111890817",
    ));

    insns.push((
        Inst::BitRR {
            op: BitOp::RBit32,
            rd: writable_xreg(1),
            rn: xreg(10),
        },
        "4101C05A",
        "rbit w1, w10",
    ));

    insns.push((
        Inst::BitRR {
            op: BitOp::RBit64,
            rd: writable_xreg(1),
            rn: xreg(10),
        },
        "4101C0DA",
        "rbit x1, x10",
    ));

    insns.push((
        Inst::BitRR {
            op: BitOp::Clz32,
            rd: writable_xreg(15),
            rn: xreg(3),
        },
        "6F10C05A",
        "clz w15, w3",
    ));

    insns.push((
        Inst::BitRR {
            op: BitOp::Clz64,
            rd: writable_xreg(15),
            rn: xreg(3),
        },
        "6F10C0DA",
        "clz x15, x3",
    ));

    insns.push((
        Inst::BitRR {
            op: BitOp::Cls32,
            rd: writable_xreg(21),
            rn: xreg(16),
        },
        "1516C05A",
        "cls w21, w16",
    ));

    insns.push((
        Inst::BitRR {
            op: BitOp::Cls64,
            rd: writable_xreg(21),
            rn: xreg(16),
        },
        "1516C0DA",
        "cls x21, x16",
    ));

    insns.push((
        Inst::ULoad8 {
            rd: writable_xreg(1),
            mem: AMode::Unscaled(xreg(2), SImm9::zero()),
            flags: MemFlags::trusted(),
        },
        "41004038",
        "ldurb w1, [x2]",
    ));
    insns.push((
        Inst::ULoad8 {
            rd: writable_xreg(1),
            mem: AMode::UnsignedOffset(xreg(2), UImm12Scaled::zero(I8)),
            flags: MemFlags::trusted(),
        },
        "41004039",
        "ldrb w1, [x2]",
    ));
    insns.push((
        Inst::ULoad8 {
            rd: writable_xreg(1),
            mem: AMode::RegReg(xreg(2), xreg(5)),
            flags: MemFlags::trusted(),
        },
        "41686538",
        "ldrb w1, [x2, x5]",
    ));
    insns.push((
        Inst::SLoad8 {
            rd: writable_xreg(1),
            mem: AMode::Unscaled(xreg(2), SImm9::zero()),
            flags: MemFlags::trusted(),
        },
        "41008038",
        "ldursb x1, [x2]",
    ));
    insns.push((
        Inst::SLoad8 {
            rd: writable_xreg(1),
            mem: AMode::UnsignedOffset(xreg(2), UImm12Scaled::maybe_from_i64(63, I8).unwrap()),
            flags: MemFlags::trusted(),
        },
        "41FC8039",
        "ldrsb x1, [x2, #63]",
    ));
    insns.push((
        Inst::SLoad8 {
            rd: writable_xreg(1),
            mem: AMode::RegReg(xreg(2), xreg(5)),
            flags: MemFlags::trusted(),
        },
        "4168A538",
        "ldrsb x1, [x2, x5]",
    ));
    insns.push((
        Inst::ULoad16 {
            rd: writable_xreg(1),
            mem: AMode::Unscaled(xreg(2), SImm9::maybe_from_i64(5).unwrap()),
            flags: MemFlags::trusted(),
        },
        "41504078",
        "ldurh w1, [x2, #5]",
    ));
    insns.push((
        Inst::ULoad16 {
            rd: writable_xreg(1),
            mem: AMode::UnsignedOffset(xreg(2), UImm12Scaled::maybe_from_i64(8, I16).unwrap()),
            flags: MemFlags::trusted(),
        },
        "41104079",
        "ldrh w1, [x2, #8]",
    ));
    insns.push((
        Inst::ULoad16 {
            rd: writable_xreg(1),
            mem: AMode::RegScaled(xreg(2), xreg(3), I16),
            flags: MemFlags::trusted(),
        },
        "41786378",
        "ldrh w1, [x2, x3, LSL #1]",
    ));
    insns.push((
        Inst::SLoad16 {
            rd: writable_xreg(1),
            mem: AMode::Unscaled(xreg(2), SImm9::zero()),
            flags: MemFlags::trusted(),
        },
        "41008078",
        "ldursh x1, [x2]",
    ));
    insns.push((
        Inst::SLoad16 {
            rd: writable_xreg(28),
            mem: AMode::UnsignedOffset(xreg(20), UImm12Scaled::maybe_from_i64(24, I16).unwrap()),
            flags: MemFlags::trusted(),
        },
        "9C328079",
        "ldrsh x28, [x20, #24]",
    ));
    insns.push((
        Inst::SLoad16 {
            rd: writable_xreg(28),
            mem: AMode::RegScaled(xreg(20), xreg(20), I16),
            flags: MemFlags::trusted(),
        },
        "9C7AB478",
        "ldrsh x28, [x20, x20, LSL #1]",
    ));
    insns.push((
        Inst::ULoad32 {
            rd: writable_xreg(1),
            mem: AMode::Unscaled(xreg(2), SImm9::zero()),
            flags: MemFlags::trusted(),
        },
        "410040B8",
        "ldur w1, [x2]",
    ));
    insns.push((
        Inst::ULoad32 {
            rd: writable_xreg(12),
            mem: AMode::UnsignedOffset(xreg(0), UImm12Scaled::maybe_from_i64(204, I32).unwrap()),
            flags: MemFlags::trusted(),
        },
        "0CCC40B9",
        "ldr w12, [x0, #204]",
    ));
    insns.push((
        Inst::ULoad32 {
            rd: writable_xreg(1),
            mem: AMode::RegScaled(xreg(2), xreg(12), I32),
            flags: MemFlags::trusted(),
        },
        "41786CB8",
        "ldr w1, [x2, x12, LSL #2]",
    ));
    insns.push((
        Inst::SLoad32 {
            rd: writable_xreg(1),
            mem: AMode::Unscaled(xreg(2), SImm9::zero()),
            flags: MemFlags::trusted(),
        },
        "410080B8",
        "ldursw x1, [x2]",
    ));
    insns.push((
        Inst::SLoad32 {
            rd: writable_xreg(12),
            mem: AMode::UnsignedOffset(xreg(1), UImm12Scaled::maybe_from_i64(16380, I32).unwrap()),
            flags: MemFlags::trusted(),
        },
        "2CFCBFB9",
        "ldrsw x12, [x1, #16380]",
    ));
    insns.push((
        Inst::SLoad32 {
            rd: writable_xreg(1),
            mem: AMode::RegScaled(xreg(5), xreg(1), I32),
            flags: MemFlags::trusted(),
        },
        "A178A1B8",
        "ldrsw x1, [x5, x1, LSL #2]",
    ));
    insns.push((
        Inst::ULoad64 {
            rd: writable_xreg(1),
            mem: AMode::Unscaled(xreg(2), SImm9::zero()),
            flags: MemFlags::trusted(),
        },
        "410040F8",
        "ldur x1, [x2]",
    ));
    insns.push((
        Inst::ULoad64 {
            rd: writable_xreg(1),
            mem: AMode::Unscaled(xreg(2), SImm9::maybe_from_i64(-256).unwrap()),
            flags: MemFlags::trusted(),
        },
        "410050F8",
        "ldur x1, [x2, #-256]",
    ));
    insns.push((
        Inst::ULoad64 {
            rd: writable_xreg(1),
            mem: AMode::Unscaled(xreg(2), SImm9::maybe_from_i64(255).unwrap()),
            flags: MemFlags::trusted(),
        },
        "41F04FF8",
        "ldur x1, [x2, #255]",
    ));
    insns.push((
        Inst::ULoad64 {
            rd: writable_xreg(1),
            mem: AMode::UnsignedOffset(xreg(2), UImm12Scaled::maybe_from_i64(32760, I64).unwrap()),
            flags: MemFlags::trusted(),
        },
        "41FC7FF9",
        "ldr x1, [x2, #32760]",
    ));
    insns.push((
        Inst::ULoad64 {
            rd: writable_xreg(1),
            mem: AMode::RegReg(xreg(2), xreg(3)),
            flags: MemFlags::trusted(),
        },
        "416863F8",
        "ldr x1, [x2, x3]",
    ));
    insns.push((
        Inst::ULoad64 {
            rd: writable_xreg(1),
            mem: AMode::RegScaled(xreg(2), xreg(3), I64),
            flags: MemFlags::trusted(),
        },
        "417863F8",
        "ldr x1, [x2, x3, LSL #3]",
    ));
    insns.push((
        Inst::ULoad64 {
            rd: writable_xreg(1),
            mem: AMode::RegScaledExtended(xreg(2), xreg(3), I64, ExtendOp::SXTW),
            flags: MemFlags::trusted(),
        },
        "41D863F8",
        "ldr x1, [x2, w3, SXTW #3]",
    ));
    insns.push((
        Inst::ULoad64 {
            rd: writable_xreg(1),
            mem: AMode::RegExtended(xreg(2), xreg(3), ExtendOp::SXTW),
            flags: MemFlags::trusted(),
        },
        "41C863F8",
        "ldr x1, [x2, w3, SXTW]",
    ));
    insns.push((
        Inst::ULoad64 {
            rd: writable_xreg(1),
            mem: AMode::Label(MemLabel::PCRel(64)),
            flags: MemFlags::trusted(),
        },
        "01020058",
        "ldr x1, pc+64",
    ));
    insns.push((
        Inst::ULoad64 {
            rd: writable_xreg(1),
            mem: AMode::PreIndexed(writable_xreg(2), SImm9::maybe_from_i64(16).unwrap()),
            flags: MemFlags::trusted(),
        },
        "410C41F8",
        "ldr x1, [x2, #16]!",
    ));
    insns.push((
        Inst::ULoad64 {
            rd: writable_xreg(1),
            mem: AMode::PostIndexed(writable_xreg(2), SImm9::maybe_from_i64(16).unwrap()),
            flags: MemFlags::trusted(),
        },
        "410441F8",
        "ldr x1, [x2], #16",
    ));
    insns.push((
        Inst::ULoad64 {
            rd: writable_xreg(1),
            mem: AMode::FPOffset(32768, I8),
            flags: MemFlags::trusted(),
        },
        "100090D2B063308B010240F9",
        "movz x16, #32768 ; add x16, fp, x16, UXTX ; ldr x1, [x16]",
    ));
    insns.push((
        Inst::ULoad64 {
            rd: writable_xreg(1),
            mem: AMode::FPOffset(-32768, I8),
            flags: MemFlags::trusted(),
        },
        "F0FF8F92B063308B010240F9",
        "movn x16, #32767 ; add x16, fp, x16, UXTX ; ldr x1, [x16]",
    ));
    insns.push((
        Inst::ULoad64 {
            rd: writable_xreg(1),
            mem: AMode::FPOffset(1048576, I8), // 2^20
            flags: MemFlags::trusted(),
        },
        "1002A0D2B063308B010240F9",
        "movz x16, #16, LSL #16 ; add x16, fp, x16, UXTX ; ldr x1, [x16]",
    ));
    insns.push((
        Inst::ULoad64 {
            rd: writable_xreg(1),
            mem: AMode::FPOffset(1048576 + 1, I8), // 2^20 + 1
            flags: MemFlags::trusted(),
        },
        "300080521002A072B063308B010240F9",
        "movz w16, #1 ; movk w16, #16, LSL #16 ; add x16, fp, x16, UXTX ; ldr x1, [x16]",
    ));

    insns.push((
        Inst::ULoad64 {
            rd: writable_xreg(1),
            mem: AMode::RegOffset(xreg(7), 8, I64),
            flags: MemFlags::trusted(),
        },
        "E18040F8",
        "ldur x1, [x7, #8]",
    ));

    insns.push((
        Inst::ULoad64 {
            rd: writable_xreg(1),
            mem: AMode::RegOffset(xreg(7), 1024, I64),
            flags: MemFlags::trusted(),
        },
        "E10042F9",
        "ldr x1, [x7, #1024]",
    ));

    insns.push((
        Inst::ULoad64 {
            rd: writable_xreg(1),
            mem: AMode::RegOffset(xreg(7), 1048576, I64),
            flags: MemFlags::trusted(),
        },
        "1002A0D2F060308B010240F9",
        "movz x16, #16, LSL #16 ; add x16, x7, x16, UXTX ; ldr x1, [x16]",
    ));

    insns.push((
        Inst::Store8 {
            rd: xreg(1),
            mem: AMode::Unscaled(xreg(2), SImm9::zero()),
            flags: MemFlags::trusted(),
        },
        "41000038",
        "sturb w1, [x2]",
    ));
    insns.push((
        Inst::Store8 {
            rd: xreg(1),
            mem: AMode::UnsignedOffset(xreg(2), UImm12Scaled::maybe_from_i64(4095, I8).unwrap()),
            flags: MemFlags::trusted(),
        },
        "41FC3F39",
        "strb w1, [x2, #4095]",
    ));
    insns.push((
        Inst::Store16 {
            rd: xreg(1),
            mem: AMode::Unscaled(xreg(2), SImm9::zero()),
            flags: MemFlags::trusted(),
        },
        "41000078",
        "sturh w1, [x2]",
    ));
    insns.push((
        Inst::Store16 {
            rd: xreg(1),
            mem: AMode::UnsignedOffset(xreg(2), UImm12Scaled::maybe_from_i64(8190, I16).unwrap()),
            flags: MemFlags::trusted(),
        },
        "41FC3F79",
        "strh w1, [x2, #8190]",
    ));
    insns.push((
        Inst::Store32 {
            rd: xreg(1),
            mem: AMode::Unscaled(xreg(2), SImm9::zero()),
            flags: MemFlags::trusted(),
        },
        "410000B8",
        "stur w1, [x2]",
    ));
    insns.push((
        Inst::Store32 {
            rd: xreg(1),
            mem: AMode::UnsignedOffset(xreg(2), UImm12Scaled::maybe_from_i64(16380, I32).unwrap()),
            flags: MemFlags::trusted(),
        },
        "41FC3FB9",
        "str w1, [x2, #16380]",
    ));
    insns.push((
        Inst::Store64 {
            rd: xreg(1),
            mem: AMode::Unscaled(xreg(2), SImm9::zero()),
            flags: MemFlags::trusted(),
        },
        "410000F8",
        "stur x1, [x2]",
    ));
    insns.push((
        Inst::Store64 {
            rd: xreg(1),
            mem: AMode::UnsignedOffset(xreg(2), UImm12Scaled::maybe_from_i64(32760, I64).unwrap()),
            flags: MemFlags::trusted(),
        },
        "41FC3FF9",
        "str x1, [x2, #32760]",
    ));
    insns.push((
        Inst::Store64 {
            rd: xreg(1),
            mem: AMode::RegReg(xreg(2), xreg(3)),
            flags: MemFlags::trusted(),
        },
        "416823F8",
        "str x1, [x2, x3]",
    ));
    insns.push((
        Inst::Store64 {
            rd: xreg(1),
            mem: AMode::RegScaled(xreg(2), xreg(3), I64),
            flags: MemFlags::trusted(),
        },
        "417823F8",
        "str x1, [x2, x3, LSL #3]",
    ));
    insns.push((
        Inst::Store64 {
            rd: xreg(1),
            mem: AMode::RegScaledExtended(xreg(2), xreg(3), I64, ExtendOp::UXTW),
            flags: MemFlags::trusted(),
        },
        "415823F8",
        "str x1, [x2, w3, UXTW #3]",
    ));
    insns.push((
        Inst::Store64 {
            rd: xreg(1),
            mem: AMode::RegExtended(xreg(2), xreg(3), ExtendOp::UXTW),
            flags: MemFlags::trusted(),
        },
        "414823F8",
        "str x1, [x2, w3, UXTW]",
    ));
    insns.push((
        Inst::Store64 {
            rd: xreg(1),
            mem: AMode::PreIndexed(writable_xreg(2), SImm9::maybe_from_i64(16).unwrap()),
            flags: MemFlags::trusted(),
        },
        "410C01F8",
        "str x1, [x2, #16]!",
    ));
    insns.push((
        Inst::Store64 {
            rd: xreg(1),
            mem: AMode::PostIndexed(writable_xreg(2), SImm9::maybe_from_i64(16).unwrap()),
            flags: MemFlags::trusted(),
        },
        "410401F8",
        "str x1, [x2], #16",
    ));

    insns.push((
        Inst::StoreP64 {
            rt: xreg(8),
            rt2: xreg(9),
            mem: PairAMode::SignedOffset(xreg(10), SImm7Scaled::zero(I64)),
            flags: MemFlags::trusted(),
        },
        "482500A9",
        "stp x8, x9, [x10]",
    ));
    insns.push((
        Inst::StoreP64 {
            rt: xreg(8),
            rt2: xreg(9),
            mem: PairAMode::SignedOffset(xreg(10), SImm7Scaled::maybe_from_i64(504, I64).unwrap()),
            flags: MemFlags::trusted(),
        },
        "48A51FA9",
        "stp x8, x9, [x10, #504]",
    ));
    insns.push((
        Inst::StoreP64 {
            rt: xreg(8),
            rt2: xreg(9),
            mem: PairAMode::SignedOffset(xreg(10), SImm7Scaled::maybe_from_i64(-64, I64).unwrap()),
            flags: MemFlags::trusted(),
        },
        "48253CA9",
        "stp x8, x9, [x10, #-64]",
    ));
    insns.push((
        Inst::StoreP64 {
            rt: xreg(21),
            rt2: xreg(28),
            mem: PairAMode::SignedOffset(xreg(1), SImm7Scaled::maybe_from_i64(-512, I64).unwrap()),
            flags: MemFlags::trusted(),
        },
        "357020A9",
        "stp x21, x28, [x1, #-512]",
    ));
    insns.push((
        Inst::StoreP64 {
            rt: xreg(8),
            rt2: xreg(9),
            mem: PairAMode::PreIndexed(
                writable_xreg(10),
                SImm7Scaled::maybe_from_i64(-64, I64).unwrap(),
            ),
            flags: MemFlags::trusted(),
        },
        "4825BCA9",
        "stp x8, x9, [x10, #-64]!",
    ));
    insns.push((
        Inst::StoreP64 {
            rt: xreg(15),
            rt2: xreg(16),
            mem: PairAMode::PostIndexed(
                writable_xreg(20),
                SImm7Scaled::maybe_from_i64(504, I64).unwrap(),
            ),
            flags: MemFlags::trusted(),
        },
        "8FC29FA8",
        "stp x15, x16, [x20], #504",
    ));

    insns.push((
        Inst::LoadP64 {
            rt: writable_xreg(8),
            rt2: writable_xreg(9),
            mem: PairAMode::SignedOffset(xreg(10), SImm7Scaled::zero(I64)),
            flags: MemFlags::trusted(),
        },
        "482540A9",
        "ldp x8, x9, [x10]",
    ));
    insns.push((
        Inst::LoadP64 {
            rt: writable_xreg(8),
            rt2: writable_xreg(9),
            mem: PairAMode::SignedOffset(xreg(10), SImm7Scaled::maybe_from_i64(504, I64).unwrap()),
            flags: MemFlags::trusted(),
        },
        "48A55FA9",
        "ldp x8, x9, [x10, #504]",
    ));
    insns.push((
        Inst::LoadP64 {
            rt: writable_xreg(8),
            rt2: writable_xreg(9),
            mem: PairAMode::SignedOffset(xreg(10), SImm7Scaled::maybe_from_i64(-64, I64).unwrap()),
            flags: MemFlags::trusted(),
        },
        "48257CA9",
        "ldp x8, x9, [x10, #-64]",
    ));
    insns.push((
        Inst::LoadP64 {
            rt: writable_xreg(8),
            rt2: writable_xreg(9),
            mem: PairAMode::SignedOffset(xreg(10), SImm7Scaled::maybe_from_i64(-512, I64).unwrap()),
            flags: MemFlags::trusted(),
        },
        "482560A9",
        "ldp x8, x9, [x10, #-512]",
    ));
    insns.push((
        Inst::LoadP64 {
            rt: writable_xreg(8),
            rt2: writable_xreg(9),
            mem: PairAMode::PreIndexed(
                writable_xreg(10),
                SImm7Scaled::maybe_from_i64(-64, I64).unwrap(),
            ),
            flags: MemFlags::trusted(),
        },
        "4825FCA9",
        "ldp x8, x9, [x10, #-64]!",
    ));
    insns.push((
        Inst::LoadP64 {
            rt: writable_xreg(8),
            rt2: writable_xreg(25),
            mem: PairAMode::PostIndexed(
                writable_xreg(12),
                SImm7Scaled::maybe_from_i64(504, I64).unwrap(),
            ),
            flags: MemFlags::trusted(),
        },
        "88E5DFA8",
        "ldp x8, x25, [x12], #504",
    ));

    insns.push((
        Inst::Mov64 {
            rd: writable_xreg(8),
            rm: xreg(9),
        },
        "E80309AA",
        "mov x8, x9",
    ));
    insns.push((
        Inst::Mov32 {
            rd: writable_xreg(8),
            rm: xreg(9),
        },
        "E803092A",
        "mov w8, w9",
    ));

    insns.push((
        Inst::MovZ {
            rd: writable_xreg(8),
            imm: MoveWideConst::maybe_from_u64(0x0000_0000_0000_ffff).unwrap(),
            size: OperandSize::Size64,
        },
        "E8FF9FD2",
        "movz x8, #65535",
    ));
    insns.push((
        Inst::MovZ {
            rd: writable_xreg(8),
            imm: MoveWideConst::maybe_from_u64(0x0000_0000_ffff_0000).unwrap(),
            size: OperandSize::Size64,
        },
        "E8FFBFD2",
        "movz x8, #65535, LSL #16",
    ));
    insns.push((
        Inst::MovZ {
            rd: writable_xreg(8),
            imm: MoveWideConst::maybe_from_u64(0x0000_ffff_0000_0000).unwrap(),
            size: OperandSize::Size64,
        },
        "E8FFDFD2",
        "movz x8, #65535, LSL #32",
    ));
    insns.push((
        Inst::MovZ {
            rd: writable_xreg(8),
            imm: MoveWideConst::maybe_from_u64(0xffff_0000_0000_0000).unwrap(),
            size: OperandSize::Size64,
        },
        "E8FFFFD2",
        "movz x8, #65535, LSL #48",
    ));
    insns.push((
        Inst::MovZ {
            rd: writable_xreg(8),
            imm: MoveWideConst::maybe_from_u64(0x0000_0000_ffff_0000).unwrap(),
            size: OperandSize::Size32,
        },
        "E8FFBF52",
        "movz w8, #65535, LSL #16",
    ));

    insns.push((
        Inst::MovN {
            rd: writable_xreg(8),
            imm: MoveWideConst::maybe_from_u64(0x0000_0000_0000_ffff).unwrap(),
            size: OperandSize::Size64,
        },
        "E8FF9F92",
        "movn x8, #65535",
    ));
    insns.push((
        Inst::MovN {
            rd: writable_xreg(8),
            imm: MoveWideConst::maybe_from_u64(0x0000_0000_ffff_0000).unwrap(),
            size: OperandSize::Size64,
        },
        "E8FFBF92",
        "movn x8, #65535, LSL #16",
    ));
    insns.push((
        Inst::MovN {
            rd: writable_xreg(8),
            imm: MoveWideConst::maybe_from_u64(0x0000_ffff_0000_0000).unwrap(),
            size: OperandSize::Size64,
        },
        "E8FFDF92",
        "movn x8, #65535, LSL #32",
    ));
    insns.push((
        Inst::MovN {
            rd: writable_xreg(8),
            imm: MoveWideConst::maybe_from_u64(0xffff_0000_0000_0000).unwrap(),
            size: OperandSize::Size64,
        },
        "E8FFFF92",
        "movn x8, #65535, LSL #48",
    ));
    insns.push((
        Inst::MovN {
            rd: writable_xreg(8),
            imm: MoveWideConst::maybe_from_u64(0x0000_0000_0000_ffff).unwrap(),
            size: OperandSize::Size32,
        },
        "E8FF9F12",
        "movn w8, #65535",
    ));

    insns.push((
        Inst::MovK {
            rd: writable_xreg(12),
            imm: MoveWideConst::maybe_from_u64(0x0000_0000_0000_0000).unwrap(),
            size: OperandSize::Size64,
        },
        "0C0080F2",
        "movk x12, #0",
    ));
    insns.push((
        Inst::MovK {
            rd: writable_xreg(19),
            imm: MoveWideConst::maybe_with_shift(0x0000, 16).unwrap(),
            size: OperandSize::Size64,
        },
        "1300A0F2",
        "movk x19, #0, LSL #16",
    ));
    insns.push((
        Inst::MovK {
            rd: writable_xreg(3),
            imm: MoveWideConst::maybe_from_u64(0x0000_0000_0000_ffff).unwrap(),
            size: OperandSize::Size64,
        },
        "E3FF9FF2",
        "movk x3, #65535",
    ));
    insns.push((
        Inst::MovK {
            rd: writable_xreg(8),
            imm: MoveWideConst::maybe_from_u64(0x0000_0000_ffff_0000).unwrap(),
            size: OperandSize::Size64,
        },
        "E8FFBFF2",
        "movk x8, #65535, LSL #16",
    ));
    insns.push((
        Inst::MovK {
            rd: writable_xreg(8),
            imm: MoveWideConst::maybe_from_u64(0x0000_ffff_0000_0000).unwrap(),
            size: OperandSize::Size64,
        },
        "E8FFDFF2",
        "movk x8, #65535, LSL #32",
    ));
    insns.push((
        Inst::MovK {
            rd: writable_xreg(8),
            imm: MoveWideConst::maybe_from_u64(0xffff_0000_0000_0000).unwrap(),
            size: OperandSize::Size64,
        },
        "E8FFFFF2",
        "movk x8, #65535, LSL #48",
    ));

    insns.push((
        Inst::CSel {
            rd: writable_xreg(10),
            rn: xreg(12),
            rm: xreg(14),
            cond: Cond::Hs,
        },
        "8A218E9A",
        "csel x10, x12, x14, hs",
    ));
    insns.push((
        Inst::CSet {
            rd: writable_xreg(15),
            cond: Cond::Ge,
        },
        "EFB79F9A",
        "cset x15, ge",
    ));
    insns.push((
        Inst::CSetm {
            rd: writable_xreg(0),
            cond: Cond::Eq,
        },
        "E0139FDA",
        "csetm x0, eq",
    ));
    insns.push((
        Inst::CSetm {
            rd: writable_xreg(16),
            cond: Cond::Vs,
        },
        "F0739FDA",
        "csetm x16, vs",
    ));
    insns.push((
        Inst::CCmpImm {
            size: OperandSize::Size64,
            rn: xreg(22),
            imm: UImm5::maybe_from_u8(5).unwrap(),
            nzcv: NZCV::new(false, false, true, true),
            cond: Cond::Eq,
        },
        "C30A45FA",
        "ccmp x22, #5, #nzCV, eq",
    ));
    insns.push((
        Inst::CCmpImm {
            size: OperandSize::Size32,
            rn: xreg(3),
            imm: UImm5::maybe_from_u8(30).unwrap(),
            nzcv: NZCV::new(true, true, true, true),
            cond: Cond::Gt,
        },
        "6FC85E7A",
        "ccmp w3, #30, #NZCV, gt",
    ));
    insns.push((
        Inst::MovToFpu {
            rd: writable_vreg(31),
            rn: xreg(0),
            size: ScalarSize::Size64,
        },
        "1F00679E",
        "fmov d31, x0",
    ));
    insns.push((
        Inst::MovToFpu {
            rd: writable_vreg(1),
            rn: xreg(28),
            size: ScalarSize::Size32,
        },
        "8103271E",
        "fmov s1, w28",
    ));
    insns.push((
        Inst::MovToVec {
            rd: writable_vreg(0),
            rn: xreg(0),
            idx: 7,
            size: VectorSize::Size8x8,
        },
        "001C0F4E",
        "mov v0.b[7], w0",
    ));
    insns.push((
        Inst::MovToVec {
            rd: writable_vreg(20),
            rn: xreg(21),
            idx: 0,
            size: VectorSize::Size64x2,
        },
        "B41E084E",
        "mov v20.d[0], x21",
    ));
    insns.push((
        Inst::MovFromVec {
            rd: writable_xreg(3),
            rn: vreg(27),
            idx: 14,
            size: VectorSize::Size8x16,
        },
        "633F1D0E",
        "umov w3, v27.b[14]",
    ));
    insns.push((
        Inst::MovFromVec {
            rd: writable_xreg(24),
            rn: vreg(5),
            idx: 3,
            size: VectorSize::Size16x8,
        },
        "B83C0E0E",
        "umov w24, v5.h[3]",
    ));
    insns.push((
        Inst::MovFromVec {
            rd: writable_xreg(12),
            rn: vreg(17),
            idx: 1,
            size: VectorSize::Size32x4,
        },
        "2C3E0C0E",
        "mov w12, v17.s[1]",
    ));
    insns.push((
        Inst::MovFromVec {
            rd: writable_xreg(21),
            rn: vreg(20),
            idx: 0,
            size: VectorSize::Size64x2,
        },
        "953E084E",
        "mov x21, v20.d[0]",
    ));
    insns.push((
        Inst::MovFromVecSigned {
            rd: writable_xreg(0),
            rn: vreg(0),
            idx: 15,
            size: VectorSize::Size8x16,
            scalar_size: OperandSize::Size32,
        },
        "002C1F0E",
        "smov w0, v0.b[15]",
    ));
    insns.push((
        Inst::MovFromVecSigned {
            rd: writable_xreg(12),
            rn: vreg(13),
            idx: 7,
            size: VectorSize::Size8x8,
            scalar_size: OperandSize::Size64,
        },
        "AC2D0F4E",
        "smov x12, v13.b[7]",
    ));
    insns.push((
        Inst::MovFromVecSigned {
            rd: writable_xreg(23),
            rn: vreg(31),
            idx: 7,
            size: VectorSize::Size16x8,
            scalar_size: OperandSize::Size32,
        },
        "F72F1E0E",
        "smov w23, v31.h[7]",
    ));
    insns.push((
        Inst::MovFromVecSigned {
            rd: writable_xreg(24),
            rn: vreg(5),
            idx: 1,
            size: VectorSize::Size32x2,
            scalar_size: OperandSize::Size64,
        },
        "B82C0C4E",
        "smov x24, v5.s[1]",
    ));
    insns.push((
        Inst::MovToNZCV { rn: xreg(13) },
        "0D421BD5",
        "msr nzcv, x13",
    ));
    insns.push((
        Inst::MovFromNZCV {
            rd: writable_xreg(27),
        },
        "1B423BD5",
        "mrs x27, nzcv",
    ));
    insns.push((
        Inst::VecDup {
            rd: writable_vreg(25),
            rn: xreg(7),
            size: VectorSize::Size8x16,
        },
        "F90C014E",
        "dup v25.16b, w7",
    ));
    insns.push((
        Inst::VecDup {
            rd: writable_vreg(2),
            rn: xreg(23),
            size: VectorSize::Size16x8,
        },
        "E20E024E",
        "dup v2.8h, w23",
    ));
    insns.push((
        Inst::VecDup {
            rd: writable_vreg(0),
            rn: xreg(28),
            size: VectorSize::Size32x4,
        },
        "800F044E",
        "dup v0.4s, w28",
    ));
    insns.push((
        Inst::VecDup {
            rd: writable_vreg(31),
            rn: xreg(5),
            size: VectorSize::Size64x2,
        },
        "BF0C084E",
        "dup v31.2d, x5",
    ));
    insns.push((
        Inst::VecDupFromFpu {
            rd: writable_vreg(14),
            rn: vreg(19),
            size: VectorSize::Size32x4,
        },
        "6E06044E",
        "dup v14.4s, v19.s[0]",
    ));
    insns.push((
        Inst::VecDupFromFpu {
            rd: writable_vreg(18),
            rn: vreg(10),
            size: VectorSize::Size64x2,
        },
        "5205084E",
        "dup v18.2d, v10.d[0]",
    ));
    insns.push((
        Inst::VecDupFPImm {
            rd: writable_vreg(31),
            imm: ASIMDFPModImm::maybe_from_u64(1_f32.to_bits() as u64, ScalarSize::Size32).unwrap(),
            size: VectorSize::Size32x2,
        },
        "1FF6030F",
        "fmov v31.2s, #1",
    ));
    insns.push((
        Inst::VecDupFPImm {
            rd: writable_vreg(0),
            imm: ASIMDFPModImm::maybe_from_u64(2_f64.to_bits(), ScalarSize::Size64).unwrap(),
            size: VectorSize::Size64x2,
        },
        "00F4006F",
        "fmov v0.2d, #2",
    ));
    insns.push((
        Inst::VecDupImm {
            rd: writable_vreg(31),
            imm: ASIMDMovModImm::maybe_from_u64(255, ScalarSize::Size8).unwrap(),
            invert: false,
            size: VectorSize::Size8x16,
        },
        "FFE7074F",
        "movi v31.16b, #255",
    ));
    insns.push((
        Inst::VecDupImm {
            rd: writable_vreg(30),
            imm: ASIMDMovModImm::maybe_from_u64(0, ScalarSize::Size16).unwrap(),
            invert: false,
            size: VectorSize::Size16x8,
        },
        "1E84004F",
        "movi v30.8h, #0",
    ));
    insns.push((
        Inst::VecDupImm {
            rd: writable_vreg(0),
            imm: ASIMDMovModImm::zero(ScalarSize::Size16),
            invert: true,
            size: VectorSize::Size16x4,
        },
        "0084002F",
        "mvni v0.4h, #0",
    ));
    insns.push((
        Inst::VecDupImm {
            rd: writable_vreg(0),
            imm: ASIMDMovModImm::maybe_from_u64(256, ScalarSize::Size16).unwrap(),
            invert: false,
            size: VectorSize::Size16x8,
        },
        "20A4004F",
        "movi v0.8h, #1, LSL #8",
    ));
    insns.push((
        Inst::VecDupImm {
            rd: writable_vreg(8),
            imm: ASIMDMovModImm::maybe_from_u64(2228223, ScalarSize::Size32).unwrap(),
            invert: false,
            size: VectorSize::Size32x4,
        },
        "28D4014F",
        "movi v8.4s, #33, MSL #16",
    ));
    insns.push((
        Inst::VecDupImm {
            rd: writable_vreg(16),
            imm: ASIMDMovModImm::maybe_from_u64(35071, ScalarSize::Size32).unwrap(),
            invert: true,
            size: VectorSize::Size32x2,
        },
        "10C5042F",
        "mvni v16.2s, #136, MSL #8",
    ));
    insns.push((
        Inst::VecDupImm {
            rd: writable_vreg(1),
            imm: ASIMDMovModImm::maybe_from_u64(0, ScalarSize::Size32).unwrap(),
            invert: false,
            size: VectorSize::Size32x2,
        },
        "0104000F",
        "movi v1.2s, #0",
    ));
    insns.push((
        Inst::VecDupImm {
            rd: writable_vreg(24),
            imm: ASIMDMovModImm::maybe_from_u64(1107296256, ScalarSize::Size32).unwrap(),
            invert: false,
            size: VectorSize::Size32x4,
        },
        "5864024F",
        "movi v24.4s, #66, LSL #24",
    ));
    insns.push((
        Inst::VecDupImm {
            rd: writable_vreg(8),
            imm: ASIMDMovModImm::zero(ScalarSize::Size64),
            invert: false,
            size: VectorSize::Size64x2,
        },
        "08E4006F",
        "movi v8.2d, #0",
    ));
    insns.push((
        Inst::VecDupImm {
            rd: writable_vreg(7),
            imm: ASIMDMovModImm::maybe_from_u64(18374687574904995840, ScalarSize::Size64).unwrap(),
            invert: false,
            size: VectorSize::Size64x2,
        },
        "87E6046F",
        "movi v7.2d, #18374687574904995840",
    ));
    insns.push((
        Inst::VecExtend {
            t: VecExtendOp::Sxtl8,
            rd: writable_vreg(4),
            rn: vreg(27),
            high_half: false,
        },
        "64A7080F",
        "sxtl v4.8h, v27.8b",
    ));
    insns.push((
        Inst::VecExtend {
            t: VecExtendOp::Sxtl16,
            rd: writable_vreg(17),
            rn: vreg(19),
            high_half: true,
        },
        "71A6104F",
        "sxtl2 v17.4s, v19.8h",
    ));
    insns.push((
        Inst::VecExtend {
            t: VecExtendOp::Sxtl32,
            rd: writable_vreg(30),
            rn: vreg(6),
            high_half: false,
        },
        "DEA4200F",
        "sxtl v30.2d, v6.2s",
    ));
    insns.push((
        Inst::VecExtend {
            t: VecExtendOp::Uxtl8,
            rd: writable_vreg(3),
            rn: vreg(29),
            high_half: true,
        },
        "A3A7086F",
        "uxtl2 v3.8h, v29.16b",
    ));
    insns.push((
        Inst::VecExtend {
            t: VecExtendOp::Uxtl16,
            rd: writable_vreg(15),
            rn: vreg(12),
            high_half: false,
        },
        "8FA5102F",
        "uxtl v15.4s, v12.4h",
    ));
    insns.push((
        Inst::VecExtend {
            t: VecExtendOp::Uxtl32,
            rd: writable_vreg(28),
            rn: vreg(2),
            high_half: true,
        },
        "5CA4206F",
        "uxtl2 v28.2d, v2.4s",
    ));

    insns.push((
        Inst::VecMovElement {
            rd: writable_vreg(0),
            rn: vreg(31),
            dest_idx: 7,
            src_idx: 7,
            size: VectorSize::Size16x8,
        },
        "E0771E6E",
        "mov v0.h[7], v31.h[7]",
    ));

    insns.push((
        Inst::VecMovElement {
            rd: writable_vreg(31),
            rn: vreg(16),
            dest_idx: 1,
            src_idx: 0,
            size: VectorSize::Size32x2,
        },
        "1F060C6E",
        "mov v31.s[1], v16.s[0]",
    ));

    insns.push((
        Inst::VecMiscNarrow {
            op: VecMiscNarrowOp::Xtn,
            rd: writable_vreg(22),
            rn: vreg(8),
            size: VectorSize::Size32x2,
            high_half: false,
        },
        "1629A10E",
        "xtn v22.2s, v8.2d",
    ));

    insns.push((
        Inst::VecMiscNarrow {
            op: VecMiscNarrowOp::Sqxtn,
            rd: writable_vreg(31),
            rn: vreg(0),
            size: VectorSize::Size16x8,
            high_half: true,
        },
        "1F48614E",
        "sqxtn2 v31.8h, v0.4s",
    ));

    insns.push((
        Inst::VecMiscNarrow {
            op: VecMiscNarrowOp::Sqxtun,
            rd: writable_vreg(16),
            rn: vreg(23),
            size: VectorSize::Size8x16,
            high_half: false,
        },
        "F02A212E",
        "sqxtun v16.8b, v23.8h",
    ));

    insns.push((
        Inst::VecRRPair {
            op: VecPairOp::Addp,
            rd: writable_vreg(0),
            rn: vreg(30),
        },
        "C0BBF15E",
        "addp d0, v30.2d",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Sqadd,
            rd: writable_vreg(1),
            rn: vreg(2),
            rm: vreg(8),
            size: VectorSize::Size8x16,
        },
        "410C284E",
        "sqadd v1.16b, v2.16b, v8.16b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Sqadd,
            rd: writable_vreg(1),
            rn: vreg(12),
            rm: vreg(28),
            size: VectorSize::Size16x8,
        },
        "810D7C4E",
        "sqadd v1.8h, v12.8h, v28.8h",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Sqadd,
            rd: writable_vreg(12),
            rn: vreg(2),
            rm: vreg(6),
            size: VectorSize::Size32x4,
        },
        "4C0CA64E",
        "sqadd v12.4s, v2.4s, v6.4s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Sqadd,
            rd: writable_vreg(20),
            rn: vreg(7),
            rm: vreg(13),
            size: VectorSize::Size64x2,
        },
        "F40CED4E",
        "sqadd v20.2d, v7.2d, v13.2d",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Sqsub,
            rd: writable_vreg(1),
            rn: vreg(2),
            rm: vreg(8),
            size: VectorSize::Size8x16,
        },
        "412C284E",
        "sqsub v1.16b, v2.16b, v8.16b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Sqsub,
            rd: writable_vreg(1),
            rn: vreg(12),
            rm: vreg(28),
            size: VectorSize::Size16x8,
        },
        "812D7C4E",
        "sqsub v1.8h, v12.8h, v28.8h",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Sqsub,
            rd: writable_vreg(12),
            rn: vreg(2),
            rm: vreg(6),
            size: VectorSize::Size32x4,
        },
        "4C2CA64E",
        "sqsub v12.4s, v2.4s, v6.4s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Sqsub,
            rd: writable_vreg(20),
            rn: vreg(7),
            rm: vreg(13),
            size: VectorSize::Size64x2,
        },
        "F42CED4E",
        "sqsub v20.2d, v7.2d, v13.2d",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Uqadd,
            rd: writable_vreg(1),
            rn: vreg(2),
            rm: vreg(8),
            size: VectorSize::Size8x16,
        },
        "410C286E",
        "uqadd v1.16b, v2.16b, v8.16b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Uqadd,
            rd: writable_vreg(1),
            rn: vreg(12),
            rm: vreg(28),
            size: VectorSize::Size16x8,
        },
        "810D7C6E",
        "uqadd v1.8h, v12.8h, v28.8h",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Uqadd,
            rd: writable_vreg(12),
            rn: vreg(2),
            rm: vreg(6),
            size: VectorSize::Size32x4,
        },
        "4C0CA66E",
        "uqadd v12.4s, v2.4s, v6.4s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Uqadd,
            rd: writable_vreg(20),
            rn: vreg(7),
            rm: vreg(13),
            size: VectorSize::Size64x2,
        },
        "F40CED6E",
        "uqadd v20.2d, v7.2d, v13.2d",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Uqsub,
            rd: writable_vreg(1),
            rn: vreg(2),
            rm: vreg(8),
            size: VectorSize::Size8x16,
        },
        "412C286E",
        "uqsub v1.16b, v2.16b, v8.16b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Uqsub,
            rd: writable_vreg(1),
            rn: vreg(12),
            rm: vreg(28),
            size: VectorSize::Size16x8,
        },
        "812D7C6E",
        "uqsub v1.8h, v12.8h, v28.8h",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Uqsub,
            rd: writable_vreg(12),
            rn: vreg(2),
            rm: vreg(6),
            size: VectorSize::Size32x4,
        },
        "4C2CA66E",
        "uqsub v12.4s, v2.4s, v6.4s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Uqsub,
            rd: writable_vreg(20),
            rn: vreg(7),
            rm: vreg(13),
            size: VectorSize::Size64x2,
        },
        "F42CED6E",
        "uqsub v20.2d, v7.2d, v13.2d",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Cmeq,
            rd: writable_vreg(3),
            rn: vreg(23),
            rm: vreg(24),
            size: VectorSize::Size8x16,
        },
        "E38E386E",
        "cmeq v3.16b, v23.16b, v24.16b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Cmgt,
            rd: writable_vreg(3),
            rn: vreg(23),
            rm: vreg(24),
            size: VectorSize::Size8x16,
        },
        "E336384E",
        "cmgt v3.16b, v23.16b, v24.16b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Cmge,
            rd: writable_vreg(23),
            rn: vreg(9),
            rm: vreg(12),
            size: VectorSize::Size8x16,
        },
        "373D2C4E",
        "cmge v23.16b, v9.16b, v12.16b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Cmhi,
            rd: writable_vreg(5),
            rn: vreg(1),
            rm: vreg(1),
            size: VectorSize::Size8x16,
        },
        "2534216E",
        "cmhi v5.16b, v1.16b, v1.16b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Cmhs,
            rd: writable_vreg(8),
            rn: vreg(2),
            rm: vreg(15),
            size: VectorSize::Size8x16,
        },
        "483C2F6E",
        "cmhs v8.16b, v2.16b, v15.16b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Cmeq,
            rd: writable_vreg(3),
            rn: vreg(23),
            rm: vreg(24),
            size: VectorSize::Size16x8,
        },
        "E38E786E",
        "cmeq v3.8h, v23.8h, v24.8h",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Cmgt,
            rd: writable_vreg(3),
            rn: vreg(23),
            rm: vreg(24),
            size: VectorSize::Size16x8,
        },
        "E336784E",
        "cmgt v3.8h, v23.8h, v24.8h",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Cmge,
            rd: writable_vreg(23),
            rn: vreg(9),
            rm: vreg(12),
            size: VectorSize::Size16x8,
        },
        "373D6C4E",
        "cmge v23.8h, v9.8h, v12.8h",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Cmhi,
            rd: writable_vreg(5),
            rn: vreg(1),
            rm: vreg(1),
            size: VectorSize::Size16x8,
        },
        "2534616E",
        "cmhi v5.8h, v1.8h, v1.8h",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Cmhs,
            rd: writable_vreg(8),
            rn: vreg(2),
            rm: vreg(15),
            size: VectorSize::Size16x8,
        },
        "483C6F6E",
        "cmhs v8.8h, v2.8h, v15.8h",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Cmeq,
            rd: writable_vreg(3),
            rn: vreg(23),
            rm: vreg(24),
            size: VectorSize::Size32x4,
        },
        "E38EB86E",
        "cmeq v3.4s, v23.4s, v24.4s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Cmgt,
            rd: writable_vreg(3),
            rn: vreg(23),
            rm: vreg(24),
            size: VectorSize::Size32x4,
        },
        "E336B84E",
        "cmgt v3.4s, v23.4s, v24.4s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Cmge,
            rd: writable_vreg(23),
            rn: vreg(9),
            rm: vreg(12),
            size: VectorSize::Size32x4,
        },
        "373DAC4E",
        "cmge v23.4s, v9.4s, v12.4s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Cmhi,
            rd: writable_vreg(5),
            rn: vreg(1),
            rm: vreg(1),
            size: VectorSize::Size32x4,
        },
        "2534A16E",
        "cmhi v5.4s, v1.4s, v1.4s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Cmhs,
            rd: writable_vreg(8),
            rn: vreg(2),
            rm: vreg(15),
            size: VectorSize::Size32x4,
        },
        "483CAF6E",
        "cmhs v8.4s, v2.4s, v15.4s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Fcmeq,
            rd: writable_vreg(28),
            rn: vreg(12),
            rm: vreg(4),
            size: VectorSize::Size32x2,
        },
        "9CE5240E",
        "fcmeq v28.2s, v12.2s, v4.2s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Fcmgt,
            rd: writable_vreg(3),
            rn: vreg(16),
            rm: vreg(31),
            size: VectorSize::Size64x2,
        },
        "03E6FF6E",
        "fcmgt v3.2d, v16.2d, v31.2d",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Fcmge,
            rd: writable_vreg(18),
            rn: vreg(23),
            rm: vreg(0),
            size: VectorSize::Size64x2,
        },
        "F2E6606E",
        "fcmge v18.2d, v23.2d, v0.2d",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::And,
            rd: writable_vreg(20),
            rn: vreg(19),
            rm: vreg(18),
            size: VectorSize::Size32x4,
        },
        "741E324E",
        "and v20.16b, v19.16b, v18.16b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Bic,
            rd: writable_vreg(8),
            rn: vreg(11),
            rm: vreg(1),
            size: VectorSize::Size8x16,
        },
        "681D614E",
        "bic v8.16b, v11.16b, v1.16b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Orr,
            rd: writable_vreg(15),
            rn: vreg(2),
            rm: vreg(12),
            size: VectorSize::Size16x8,
        },
        "4F1CAC4E",
        "orr v15.16b, v2.16b, v12.16b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Eor,
            rd: writable_vreg(18),
            rn: vreg(3),
            rm: vreg(22),
            size: VectorSize::Size8x16,
        },
        "721C366E",
        "eor v18.16b, v3.16b, v22.16b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Bsl,
            rd: writable_vreg(8),
            rn: vreg(9),
            rm: vreg(1),
            size: VectorSize::Size8x16,
        },
        "281D616E",
        "bsl v8.16b, v9.16b, v1.16b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Umaxp,
            rd: writable_vreg(8),
            rn: vreg(12),
            rm: vreg(1),
            size: VectorSize::Size8x16,
        },
        "88A5216E",
        "umaxp v8.16b, v12.16b, v1.16b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Umaxp,
            rd: writable_vreg(1),
            rn: vreg(6),
            rm: vreg(1),
            size: VectorSize::Size16x8,
        },
        "C1A4616E",
        "umaxp v1.8h, v6.8h, v1.8h",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Umaxp,
            rd: writable_vreg(1),
            rn: vreg(20),
            rm: vreg(16),
            size: VectorSize::Size32x4,
        },
        "81A6B06E",
        "umaxp v1.4s, v20.4s, v16.4s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Add,
            rd: writable_vreg(5),
            rn: vreg(1),
            rm: vreg(1),
            size: VectorSize::Size8x16,
        },
        "2584214E",
        "add v5.16b, v1.16b, v1.16b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Add,
            rd: writable_vreg(7),
            rn: vreg(13),
            rm: vreg(2),
            size: VectorSize::Size16x8,
        },
        "A785624E",
        "add v7.8h, v13.8h, v2.8h",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Add,
            rd: writable_vreg(18),
            rn: vreg(9),
            rm: vreg(6),
            size: VectorSize::Size32x4,
        },
        "3285A64E",
        "add v18.4s, v9.4s, v6.4s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Add,
            rd: writable_vreg(1),
            rn: vreg(3),
            rm: vreg(2),
            size: VectorSize::Size64x2,
        },
        "6184E24E",
        "add v1.2d, v3.2d, v2.2d",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Sub,
            rd: writable_vreg(5),
            rn: vreg(1),
            rm: vreg(1),
            size: VectorSize::Size8x16,
        },
        "2584216E",
        "sub v5.16b, v1.16b, v1.16b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Sub,
            rd: writable_vreg(7),
            rn: vreg(13),
            rm: vreg(2),
            size: VectorSize::Size16x8,
        },
        "A785626E",
        "sub v7.8h, v13.8h, v2.8h",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Sub,
            rd: writable_vreg(18),
            rn: vreg(9),
            rm: vreg(6),
            size: VectorSize::Size32x4,
        },
        "3285A66E",
        "sub v18.4s, v9.4s, v6.4s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Sub,
            rd: writable_vreg(18),
            rn: vreg(0),
            rm: vreg(8),
            size: VectorSize::Size64x2,
        },
        "1284E86E",
        "sub v18.2d, v0.2d, v8.2d",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Mul,
            rd: writable_vreg(25),
            rn: vreg(9),
            rm: vreg(8),
            size: VectorSize::Size8x16,
        },
        "399D284E",
        "mul v25.16b, v9.16b, v8.16b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Mul,
            rd: writable_vreg(30),
            rn: vreg(30),
            rm: vreg(12),
            size: VectorSize::Size16x8,
        },
        "DE9F6C4E",
        "mul v30.8h, v30.8h, v12.8h",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Mul,
            rd: writable_vreg(18),
            rn: vreg(18),
            rm: vreg(18),
            size: VectorSize::Size32x4,
        },
        "529EB24E",
        "mul v18.4s, v18.4s, v18.4s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Ushl,
            rd: writable_vreg(18),
            rn: vreg(18),
            rm: vreg(18),
            size: VectorSize::Size8x16,
        },
        "5246326E",
        "ushl v18.16b, v18.16b, v18.16b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Ushl,
            rd: writable_vreg(18),
            rn: vreg(18),
            rm: vreg(18),
            size: VectorSize::Size16x8,
        },
        "5246726E",
        "ushl v18.8h, v18.8h, v18.8h",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Ushl,
            rd: writable_vreg(18),
            rn: vreg(1),
            rm: vreg(21),
            size: VectorSize::Size32x4,
        },
        "3244B56E",
        "ushl v18.4s, v1.4s, v21.4s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Ushl,
            rd: writable_vreg(5),
            rn: vreg(7),
            rm: vreg(19),
            size: VectorSize::Size64x2,
        },
        "E544F36E",
        "ushl v5.2d, v7.2d, v19.2d",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Sshl,
            rd: writable_vreg(18),
            rn: vreg(18),
            rm: vreg(18),
            size: VectorSize::Size8x16,
        },
        "5246324E",
        "sshl v18.16b, v18.16b, v18.16b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Sshl,
            rd: writable_vreg(30),
            rn: vreg(1),
            rm: vreg(29),
            size: VectorSize::Size16x8,
        },
        "3E447D4E",
        "sshl v30.8h, v1.8h, v29.8h",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Sshl,
            rd: writable_vreg(8),
            rn: vreg(22),
            rm: vreg(21),
            size: VectorSize::Size32x4,
        },
        "C846B54E",
        "sshl v8.4s, v22.4s, v21.4s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Sshl,
            rd: writable_vreg(8),
            rn: vreg(22),
            rm: vreg(2),
            size: VectorSize::Size64x2,
        },
        "C846E24E",
        "sshl v8.2d, v22.2d, v2.2d",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Umin,
            rd: writable_vreg(1),
            rn: vreg(12),
            rm: vreg(3),
            size: VectorSize::Size8x16,
        },
        "816D236E",
        "umin v1.16b, v12.16b, v3.16b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Umin,
            rd: writable_vreg(30),
            rn: vreg(20),
            rm: vreg(10),
            size: VectorSize::Size16x8,
        },
        "9E6E6A6E",
        "umin v30.8h, v20.8h, v10.8h",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Umin,
            rd: writable_vreg(8),
            rn: vreg(22),
            rm: vreg(21),
            size: VectorSize::Size32x4,
        },
        "C86EB56E",
        "umin v8.4s, v22.4s, v21.4s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Smin,
            rd: writable_vreg(1),
            rn: vreg(12),
            rm: vreg(3),
            size: VectorSize::Size8x16,
        },
        "816D234E",
        "smin v1.16b, v12.16b, v3.16b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Smin,
            rd: writable_vreg(30),
            rn: vreg(20),
            rm: vreg(10),
            size: VectorSize::Size16x8,
        },
        "9E6E6A4E",
        "smin v30.8h, v20.8h, v10.8h",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Smin,
            rd: writable_vreg(8),
            rn: vreg(22),
            rm: vreg(21),
            size: VectorSize::Size32x4,
        },
        "C86EB54E",
        "smin v8.4s, v22.4s, v21.4s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Umax,
            rd: writable_vreg(6),
            rn: vreg(9),
            rm: vreg(8),
            size: VectorSize::Size8x8,
        },
        "2665282E",
        "umax v6.8b, v9.8b, v8.8b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Umax,
            rd: writable_vreg(11),
            rn: vreg(13),
            rm: vreg(2),
            size: VectorSize::Size16x8,
        },
        "AB65626E",
        "umax v11.8h, v13.8h, v2.8h",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Umax,
            rd: writable_vreg(8),
            rn: vreg(12),
            rm: vreg(14),
            size: VectorSize::Size32x4,
        },
        "8865AE6E",
        "umax v8.4s, v12.4s, v14.4s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Smax,
            rd: writable_vreg(6),
            rn: vreg(9),
            rm: vreg(8),
            size: VectorSize::Size8x16,
        },
        "2665284E",
        "smax v6.16b, v9.16b, v8.16b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Smax,
            rd: writable_vreg(11),
            rn: vreg(13),
            rm: vreg(2),
            size: VectorSize::Size16x8,
        },
        "AB65624E",
        "smax v11.8h, v13.8h, v2.8h",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Smax,
            rd: writable_vreg(8),
            rn: vreg(12),
            rm: vreg(14),
            size: VectorSize::Size32x4,
        },
        "8865AE4E",
        "smax v8.4s, v12.4s, v14.4s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Urhadd,
            rd: writable_vreg(8),
            rn: vreg(1),
            rm: vreg(3),
            size: VectorSize::Size8x16,
        },
        "2814236E",
        "urhadd v8.16b, v1.16b, v3.16b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Urhadd,
            rd: writable_vreg(2),
            rn: vreg(13),
            rm: vreg(6),
            size: VectorSize::Size16x8,
        },
        "A215666E",
        "urhadd v2.8h, v13.8h, v6.8h",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Urhadd,
            rd: writable_vreg(8),
            rn: vreg(12),
            rm: vreg(14),
            size: VectorSize::Size32x4,
        },
        "8815AE6E",
        "urhadd v8.4s, v12.4s, v14.4s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Fadd,
            rd: writable_vreg(31),
            rn: vreg(0),
            rm: vreg(16),
            size: VectorSize::Size32x4,
        },
        "1FD4304E",
        "fadd v31.4s, v0.4s, v16.4s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Fsub,
            rd: writable_vreg(8),
            rn: vreg(7),
            rm: vreg(15),
            size: VectorSize::Size64x2,
        },
        "E8D4EF4E",
        "fsub v8.2d, v7.2d, v15.2d",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Fdiv,
            rd: writable_vreg(1),
            rn: vreg(3),
            rm: vreg(4),
            size: VectorSize::Size32x4,
        },
        "61FC246E",
        "fdiv v1.4s, v3.4s, v4.4s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Fmax,
            rd: writable_vreg(31),
            rn: vreg(16),
            rm: vreg(0),
            size: VectorSize::Size64x2,
        },
        "1FF6604E",
        "fmax v31.2d, v16.2d, v0.2d",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Fmin,
            rd: writable_vreg(5),
            rn: vreg(19),
            rm: vreg(26),
            size: VectorSize::Size32x4,
        },
        "65F6BA4E",
        "fmin v5.4s, v19.4s, v26.4s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Fmul,
            rd: writable_vreg(2),
            rn: vreg(0),
            rm: vreg(5),
            size: VectorSize::Size64x2,
        },
        "02DC656E",
        "fmul v2.2d, v0.2d, v5.2d",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Addp,
            rd: writable_vreg(16),
            rn: vreg(12),
            rm: vreg(1),
            size: VectorSize::Size8x16,
        },
        "90BD214E",
        "addp v16.16b, v12.16b, v1.16b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Addp,
            rd: writable_vreg(8),
            rn: vreg(12),
            rm: vreg(14),
            size: VectorSize::Size32x4,
        },
        "88BDAE4E",
        "addp v8.4s, v12.4s, v14.4s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Umlal,
            rd: writable_vreg(9),
            rn: vreg(20),
            rm: vreg(17),
            size: VectorSize::Size32x2,
        },
        "8982B12E",
        "umlal v9.2d, v20.2s, v17.2s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Zip1,
            rd: writable_vreg(16),
            rn: vreg(12),
            rm: vreg(1),
            size: VectorSize::Size8x16,
        },
        "9039014E",
        "zip1 v16.16b, v12.16b, v1.16b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Zip1,
            rd: writable_vreg(2),
            rn: vreg(13),
            rm: vreg(6),
            size: VectorSize::Size16x8,
        },
        "A239464E",
        "zip1 v2.8h, v13.8h, v6.8h",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Zip1,
            rd: writable_vreg(8),
            rn: vreg(12),
            rm: vreg(14),
            size: VectorSize::Size32x4,
        },
        "88398E4E",
        "zip1 v8.4s, v12.4s, v14.4s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Zip1,
            rd: writable_vreg(9),
            rn: vreg(20),
            rm: vreg(17),
            size: VectorSize::Size64x2,
        },
        "893AD14E",
        "zip1 v9.2d, v20.2d, v17.2d",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Smull,
            rd: writable_vreg(16),
            rn: vreg(12),
            rm: vreg(1),
            size: VectorSize::Size8x16,
        },
        "90C1210E",
        "smull v16.8h, v12.8b, v1.8b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Smull,
            rd: writable_vreg(2),
            rn: vreg(13),
            rm: vreg(6),
            size: VectorSize::Size16x8,
        },
        "A2C1660E",
        "smull v2.4s, v13.4h, v6.4h",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Smull,
            rd: writable_vreg(8),
            rn: vreg(12),
            rm: vreg(14),
            size: VectorSize::Size32x4,
        },
        "88C1AE0E",
        "smull v8.2d, v12.2s, v14.2s",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Smull2,
            rd: writable_vreg(16),
            rn: vreg(12),
            rm: vreg(1),
            size: VectorSize::Size8x16,
        },
        "90C1214E",
        "smull2 v16.8h, v12.16b, v1.16b",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Smull2,
            rd: writable_vreg(2),
            rn: vreg(13),
            rm: vreg(6),
            size: VectorSize::Size16x8,
        },
        "A2C1664E",
        "smull2 v2.4s, v13.8h, v6.8h",
    ));

    insns.push((
        Inst::VecRRR {
            alu_op: VecALUOp::Smull2,
            rd: writable_vreg(8),
            rn: vreg(12),
            rm: vreg(14),
            size: VectorSize::Size32x4,
        },
        "88C1AE4E",
        "smull2 v8.2d, v12.4s, v14.4s",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Not,
            rd: writable_vreg(20),
            rn: vreg(17),
            size: VectorSize::Size8x8,
        },
        "345A202E",
        "mvn v20.8b, v17.8b",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Not,
            rd: writable_vreg(2),
            rn: vreg(1),
            size: VectorSize::Size32x4,
        },
        "2258206E",
        "mvn v2.16b, v1.16b",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Neg,
            rd: writable_vreg(3),
            rn: vreg(7),
            size: VectorSize::Size8x8,
        },
        "E3B8202E",
        "neg v3.8b, v7.8b",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Neg,
            rd: writable_vreg(8),
            rn: vreg(12),
            size: VectorSize::Size8x16,
        },
        "88B9206E",
        "neg v8.16b, v12.16b",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Neg,
            rd: writable_vreg(0),
            rn: vreg(31),
            size: VectorSize::Size16x8,
        },
        "E0BB606E",
        "neg v0.8h, v31.8h",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Neg,
            rd: writable_vreg(2),
            rn: vreg(3),
            size: VectorSize::Size32x4,
        },
        "62B8A06E",
        "neg v2.4s, v3.4s",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Neg,
            rd: writable_vreg(10),
            rn: vreg(8),
            size: VectorSize::Size64x2,
        },
        "0AB9E06E",
        "neg v10.2d, v8.2d",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Abs,
            rd: writable_vreg(3),
            rn: vreg(1),
            size: VectorSize::Size8x8,
        },
        "23B8200E",
        "abs v3.8b, v1.8b",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Abs,
            rd: writable_vreg(1),
            rn: vreg(1),
            size: VectorSize::Size8x16,
        },
        "21B8204E",
        "abs v1.16b, v1.16b",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Abs,
            rd: writable_vreg(29),
            rn: vreg(28),
            size: VectorSize::Size16x8,
        },
        "9DBB604E",
        "abs v29.8h, v28.8h",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Abs,
            rd: writable_vreg(7),
            rn: vreg(8),
            size: VectorSize::Size32x4,
        },
        "07B9A04E",
        "abs v7.4s, v8.4s",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Abs,
            rd: writable_vreg(1),
            rn: vreg(10),
            size: VectorSize::Size64x2,
        },
        "41B9E04E",
        "abs v1.2d, v10.2d",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Fabs,
            rd: writable_vreg(15),
            rn: vreg(16),
            size: VectorSize::Size32x4,
        },
        "0FFAA04E",
        "fabs v15.4s, v16.4s",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Fneg,
            rd: writable_vreg(31),
            rn: vreg(0),
            size: VectorSize::Size32x4,
        },
        "1FF8A06E",
        "fneg v31.4s, v0.4s",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Fsqrt,
            rd: writable_vreg(7),
            rn: vreg(18),
            size: VectorSize::Size64x2,
        },
        "47FAE16E",
        "fsqrt v7.2d, v18.2d",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Rev64,
            rd: writable_vreg(1),
            rn: vreg(10),
            size: VectorSize::Size32x4,
        },
        "4109A04E",
        "rev64 v1.4s, v10.4s",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Shll,
            rd: writable_vreg(12),
            rn: vreg(5),
            size: VectorSize::Size8x8,
        },
        "AC38212E",
        "shll v12.8h, v5.8b, #8",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Shll,
            rd: writable_vreg(9),
            rn: vreg(1),
            size: VectorSize::Size16x4,
        },
        "2938612E",
        "shll v9.4s, v1.4h, #16",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Shll,
            rd: writable_vreg(1),
            rn: vreg(10),
            size: VectorSize::Size32x2,
        },
        "4139A12E",
        "shll v1.2d, v10.2s, #32",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Fcvtzs,
            rd: writable_vreg(4),
            rn: vreg(22),
            size: VectorSize::Size32x4,
        },
        "C4BAA14E",
        "fcvtzs v4.4s, v22.4s",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Fcvtzu,
            rd: writable_vreg(29),
            rn: vreg(15),
            size: VectorSize::Size64x2,
        },
        "FDB9E16E",
        "fcvtzu v29.2d, v15.2d",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Scvtf,
            rd: writable_vreg(20),
            rn: vreg(8),
            size: VectorSize::Size32x4,
        },
        "14D9214E",
        "scvtf v20.4s, v8.4s",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Ucvtf,
            rd: writable_vreg(10),
            rn: vreg(19),
            size: VectorSize::Size64x2,
        },
        "6ADA616E",
        "ucvtf v10.2d, v19.2d",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Frintn,
            rd: writable_vreg(11),
            rn: vreg(18),
            size: VectorSize::Size32x4,
        },
        "4B8A214E",
        "frintn v11.4s, v18.4s",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Frintn,
            rd: writable_vreg(12),
            rn: vreg(17),
            size: VectorSize::Size64x2,
        },
        "2C8A614E",
        "frintn v12.2d, v17.2d",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Frintz,
            rd: writable_vreg(11),
            rn: vreg(18),
            size: VectorSize::Size32x4,
        },
        "4B9AA14E",
        "frintz v11.4s, v18.4s",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Frintz,
            rd: writable_vreg(12),
            rn: vreg(17),
            size: VectorSize::Size64x2,
        },
        "2C9AE14E",
        "frintz v12.2d, v17.2d",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Frintm,
            rd: writable_vreg(11),
            rn: vreg(18),
            size: VectorSize::Size32x4,
        },
        "4B9A214E",
        "frintm v11.4s, v18.4s",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Frintm,
            rd: writable_vreg(12),
            rn: vreg(17),
            size: VectorSize::Size64x2,
        },
        "2C9A614E",
        "frintm v12.2d, v17.2d",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Frintp,
            rd: writable_vreg(11),
            rn: vreg(18),
            size: VectorSize::Size32x4,
        },
        "4B8AA14E",
        "frintp v11.4s, v18.4s",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Frintp,
            rd: writable_vreg(12),
            rn: vreg(17),
            size: VectorSize::Size64x2,
        },
        "2C8AE14E",
        "frintp v12.2d, v17.2d",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Cnt,
            rd: writable_vreg(23),
            rn: vreg(5),
            size: VectorSize::Size8x8,
        },
        "B758200E",
        "cnt v23.8b, v5.8b",
    ));

    insns.push((
        Inst::VecMisc {
            op: VecMisc2::Cmeq0,
            rd: writable_vreg(12),
            rn: vreg(27),
            size: VectorSize::Size16x8,
        },
        "6C9B604E",
        "cmeq v12.8h, v27.8h, #0",
    ));

    insns.push((
        Inst::VecLanes {
            op: VecLanesOp::Uminv,
            rd: writable_vreg(0),
            rn: vreg(31),
            size: VectorSize::Size8x8,
        },
        "E0AB312E",
        "uminv b0, v31.8b",
    ));

    insns.push((
        Inst::VecLanes {
            op: VecLanesOp::Uminv,
            rd: writable_vreg(2),
            rn: vreg(1),
            size: VectorSize::Size8x16,
        },
        "22A8316E",
        "uminv b2, v1.16b",
    ));

    insns.push((
        Inst::VecLanes {
            op: VecLanesOp::Uminv,
            rd: writable_vreg(3),
            rn: vreg(11),
            size: VectorSize::Size16x8,
        },
        "63A9716E",
        "uminv h3, v11.8h",
    ));

    insns.push((
        Inst::VecLanes {
            op: VecLanesOp::Uminv,
            rd: writable_vreg(18),
            rn: vreg(4),
            size: VectorSize::Size32x4,
        },
        "92A8B16E",
        "uminv s18, v4.4s",
    ));

    insns.push((
        Inst::VecLanes {
            op: VecLanesOp::Addv,
            rd: writable_vreg(2),
            rn: vreg(29),
            size: VectorSize::Size8x16,
        },
        "A2BB314E",
        "addv b2, v29.16b",
    ));

    insns.push((
        Inst::VecLanes {
            op: VecLanesOp::Addv,
            rd: writable_vreg(15),
            rn: vreg(7),
            size: VectorSize::Size16x4,
        },
        "EFB8710E",
        "addv h15, v7.4h",
    ));

    insns.push((
        Inst::VecLanes {
            op: VecLanesOp::Addv,
            rd: writable_vreg(3),
            rn: vreg(21),
            size: VectorSize::Size16x8,
        },
        "A3BA714E",
        "addv h3, v21.8h",
    ));

    insns.push((
        Inst::VecLanes {
            op: VecLanesOp::Addv,
            rd: writable_vreg(18),
            rn: vreg(5),
            size: VectorSize::Size32x4,
        },
        "B2B8B14E",
        "addv s18, v5.4s",
    ));

    insns.push((
        Inst::VecShiftImm {
            op: VecShiftImmOp::Shl,
            rd: writable_vreg(27),
            rn: vreg(5),
            imm: 7,
            size: VectorSize::Size8x16,
        },
        "BB540F4F",
        "shl v27.16b, v5.16b, #7",
    ));

    insns.push((
        Inst::VecShiftImm {
            op: VecShiftImmOp::Shl,
            rd: writable_vreg(1),
            rn: vreg(30),
            imm: 0,
            size: VectorSize::Size8x16,
        },
        "C157084F",
        "shl v1.16b, v30.16b, #0",
    ));

    insns.push((
        Inst::VecShiftImm {
            op: VecShiftImmOp::Sshr,
            rd: writable_vreg(26),
            rn: vreg(6),
            imm: 16,
            size: VectorSize::Size16x8,
        },
        "DA04104F",
        "sshr v26.8h, v6.8h, #16",
    ));

    insns.push((
        Inst::VecShiftImm {
            op: VecShiftImmOp::Sshr,
            rd: writable_vreg(3),
            rn: vreg(19),
            imm: 1,
            size: VectorSize::Size16x8,
        },
        "63061F4F",
        "sshr v3.8h, v19.8h, #1",
    ));

    insns.push((
        Inst::VecShiftImm {
            op: VecShiftImmOp::Ushr,
            rd: writable_vreg(25),
            rn: vreg(6),
            imm: 32,
            size: VectorSize::Size32x4,
        },
        "D904206F",
        "ushr v25.4s, v6.4s, #32",
    ));

    insns.push((
        Inst::VecShiftImm {
            op: VecShiftImmOp::Ushr,
            rd: writable_vreg(5),
            rn: vreg(21),
            imm: 1,
            size: VectorSize::Size32x4,
        },
        "A5063F6F",
        "ushr v5.4s, v21.4s, #1",
    ));

    insns.push((
        Inst::VecShiftImm {
            op: VecShiftImmOp::Shl,
            rd: writable_vreg(22),
            rn: vreg(13),
            imm: 63,
            size: VectorSize::Size64x2,
        },
        "B6557F4F",
        "shl v22.2d, v13.2d, #63",
    ));

    insns.push((
        Inst::VecShiftImm {
            op: VecShiftImmOp::Shl,
            rd: writable_vreg(23),
            rn: vreg(9),
            imm: 0,
            size: VectorSize::Size64x2,
        },
        "3755404F",
        "shl v23.2d, v9.2d, #0",
    ));

    insns.push((
        Inst::VecExtract {
            rd: writable_vreg(1),
            rn: vreg(30),
            rm: vreg(17),
            imm4: 0,
        },
        "C103116E",
        "ext v1.16b, v30.16b, v17.16b, #0",
    ));

    insns.push((
        Inst::VecExtract {
            rd: writable_vreg(1),
            rn: vreg(30),
            rm: vreg(17),
            imm4: 8,
        },
        "C143116E",
        "ext v1.16b, v30.16b, v17.16b, #8",
    ));

    insns.push((
        Inst::VecExtract {
            rd: writable_vreg(1),
            rn: vreg(30),
            rm: vreg(17),
            imm4: 15,
        },
        "C17B116E",
        "ext v1.16b, v30.16b, v17.16b, #15",
    ));

    insns.push((
        Inst::VecTbl {
            rd: writable_vreg(0),
            rn: vreg(31),
            rm: vreg(16),
            is_extension: false,
        },
        "E003104E",
        "tbl v0.16b, { v31.16b }, v16.16b",
    ));

    insns.push((
        Inst::VecTbl {
            rd: writable_vreg(4),
            rn: vreg(12),
            rm: vreg(23),
            is_extension: true,
        },
        "8411174E",
        "tbx v4.16b, { v12.16b }, v23.16b",
    ));

    insns.push((
        Inst::VecTbl2 {
            rd: writable_vreg(16),
            rn: vreg(31),
            rn2: vreg(0),
            rm: vreg(26),
            is_extension: false,
        },
        "F0231A4E",
        "tbl v16.16b, { v31.16b, v0.16b }, v26.16b",
    ));

    insns.push((
        Inst::VecTbl2 {
            rd: writable_vreg(3),
            rn: vreg(11),
            rn2: vreg(12),
            rm: vreg(19),
            is_extension: true,
        },
        "6331134E",
        "tbx v3.16b, { v11.16b, v12.16b }, v19.16b",
    ));

    insns.push((
        Inst::VecLoadReplicate {
            rd: writable_vreg(31),
            rn: xreg(0),

            size: VectorSize::Size64x2,
        },
        "1FCC404D",
        "ld1r { v31.2d }, [x0]",
    ));

    insns.push((
        Inst::VecLoadReplicate {
            rd: writable_vreg(0),
            rn: xreg(25),

            size: VectorSize::Size8x8,
        },
        "20C3400D",
        "ld1r { v0.8b }, [x25]",
    ));

    insns.push((
        Inst::VecCSel {
            rd: writable_vreg(5),
            rn: vreg(10),
            rm: vreg(19),
            cond: Cond::Gt,
        },
        "6C000054651EB34E02000014451DAA4E",
        "vcsel v5.16b, v10.16b, v19.16b, gt (if-then-else diamond)",
    ));

    insns.push((
        Inst::Extend {
            rd: writable_xreg(3),
            rn: xreg(5),
            signed: false,
            from_bits: 1,
            to_bits: 32,
        },
        "A3000012",
        "and w3, w5, #1",
    ));
    insns.push((
        Inst::Extend {
            rd: writable_xreg(3),
            rn: xreg(5),
            signed: false,
            from_bits: 1,
            to_bits: 64,
        },
        "A3000012",
        "and w3, w5, #1",
    ));
    insns.push((
        Inst::Extend {
            rd: writable_xreg(10),
            rn: xreg(21),
            signed: true,
            from_bits: 1,
            to_bits: 32,
        },
        "AA020013",
        "sbfx w10, w21, #0, #1",
    ));
    insns.push((
        Inst::Extend {
            rd: writable_xreg(1),
            rn: xreg(2),
            signed: true,
            from_bits: 1,
            to_bits: 64,
        },
        "41004093",
        "sbfx x1, x2, #0, #1",
    ));
    insns.push((
        Inst::Extend {
            rd: writable_xreg(1),
            rn: xreg(2),
            signed: false,
            from_bits: 8,
            to_bits: 32,
        },
        "411C0053",
        "uxtb w1, w2",
    ));
    insns.push((
        Inst::Extend {
            rd: writable_xreg(1),
            rn: xreg(2),
            signed: true,
            from_bits: 8,
            to_bits: 32,
        },
        "411C0013",
        "sxtb w1, w2",
    ));
    insns.push((
        Inst::Extend {
            rd: writable_xreg(1),
            rn: xreg(2),
            signed: false,
            from_bits: 16,
            to_bits: 32,
        },
        "413C0053",
        "uxth w1, w2",
    ));
    insns.push((
        Inst::Extend {
            rd: writable_xreg(1),
            rn: xreg(2),
            signed: true,
            from_bits: 16,
            to_bits: 32,
        },
        "413C0013",
        "sxth w1, w2",
    ));
    insns.push((
        Inst::Extend {
            rd: writable_xreg(1),
            rn: xreg(2),
            signed: false,
            from_bits: 8,
            to_bits: 64,
        },
        "411C0053",
        "uxtb w1, w2",
    ));
    insns.push((
        Inst::Extend {
            rd: writable_xreg(1),
            rn: xreg(2),
            signed: true,
            from_bits: 8,
            to_bits: 64,
        },
        "411C4093",
        "sxtb x1, w2",
    ));
    insns.push((
        Inst::Extend {
            rd: writable_xreg(1),
            rn: xreg(2),
            signed: false,
            from_bits: 16,
            to_bits: 64,
        },
        "413C0053",
        "uxth w1, w2",
    ));
    insns.push((
        Inst::Extend {
            rd: writable_xreg(1),
            rn: xreg(2),
            signed: true,
            from_bits: 16,
            to_bits: 64,
        },
        "413C4093",
        "sxth x1, w2",
    ));
    insns.push((
        Inst::Extend {
            rd: writable_xreg(1),
            rn: xreg(2),
            signed: false,
            from_bits: 32,
            to_bits: 64,
        },
        "E103022A",
        "mov w1, w2",
    ));
    insns.push((
        Inst::Extend {
            rd: writable_xreg(1),
            rn: xreg(2),
            signed: true,
            from_bits: 32,
            to_bits: 64,
        },
        "417C4093",
        "sxtw x1, w2",
    ));

    insns.push((
        Inst::Jump {
            dest: BranchTarget::ResolvedOffset(64),
        },
        "10000014",
        "b 64",
    ));

    insns.push((
        Inst::TrapIf {
            trap_code: TrapCode::Interrupt,
            kind: CondBrKind::NotZero(xreg(8)),
        },
        "480000B40000A0D4",
        "cbz x8, 8 ; udf",
    ));
    insns.push((
        Inst::TrapIf {
            trap_code: TrapCode::Interrupt,
            kind: CondBrKind::Zero(xreg(8)),
        },
        "480000B50000A0D4",
        "cbnz x8, 8 ; udf",
    ));
    insns.push((
        Inst::TrapIf {
            trap_code: TrapCode::Interrupt,
            kind: CondBrKind::Cond(Cond::Ne),
        },
        "400000540000A0D4",
        "b.eq 8 ; udf",
    ));
    insns.push((
        Inst::TrapIf {
            trap_code: TrapCode::Interrupt,
            kind: CondBrKind::Cond(Cond::Eq),
        },
        "410000540000A0D4",
        "b.ne 8 ; udf",
    ));
    insns.push((
        Inst::TrapIf {
            trap_code: TrapCode::Interrupt,
            kind: CondBrKind::Cond(Cond::Lo),
        },
        "420000540000A0D4",
        "b.hs 8 ; udf",
    ));
    insns.push((
        Inst::TrapIf {
            trap_code: TrapCode::Interrupt,
            kind: CondBrKind::Cond(Cond::Hs),
        },
        "430000540000A0D4",
        "b.lo 8 ; udf",
    ));
    insns.push((
        Inst::TrapIf {
            trap_code: TrapCode::Interrupt,
            kind: CondBrKind::Cond(Cond::Pl),
        },
        "440000540000A0D4",
        "b.mi 8 ; udf",
    ));
    insns.push((
        Inst::TrapIf {
            trap_code: TrapCode::Interrupt,
            kind: CondBrKind::Cond(Cond::Mi),
        },
        "450000540000A0D4",
        "b.pl 8 ; udf",
    ));
    insns.push((
        Inst::TrapIf {
            trap_code: TrapCode::Interrupt,
            kind: CondBrKind::Cond(Cond::Vc),
        },
        "460000540000A0D4",
        "b.vs 8 ; udf",
    ));
    insns.push((
        Inst::TrapIf {
            trap_code: TrapCode::Interrupt,
            kind: CondBrKind::Cond(Cond::Vs),
        },
        "470000540000A0D4",
        "b.vc 8 ; udf",
    ));
    insns.push((
        Inst::TrapIf {
            trap_code: TrapCode::Interrupt,
            kind: CondBrKind::Cond(Cond::Ls),
        },
        "480000540000A0D4",
        "b.hi 8 ; udf",
    ));
    insns.push((
        Inst::TrapIf {
            trap_code: TrapCode::Interrupt,
            kind: CondBrKind::Cond(Cond::Hi),
        },
        "490000540000A0D4",
        "b.ls 8 ; udf",
    ));
    insns.push((
        Inst::TrapIf {
            trap_code: TrapCode::Interrupt,
            kind: CondBrKind::Cond(Cond::Lt),
        },
        "4A0000540000A0D4",
        "b.ge 8 ; udf",
    ));
    insns.push((
        Inst::TrapIf {
            trap_code: TrapCode::Interrupt,
            kind: CondBrKind::Cond(Cond::Ge),
        },
        "4B0000540000A0D4",
        "b.lt 8 ; udf",
    ));
    insns.push((
        Inst::TrapIf {
            trap_code: TrapCode::Interrupt,
            kind: CondBrKind::Cond(Cond::Le),
        },
        "4C0000540000A0D4",
        "b.gt 8 ; udf",
    ));
    insns.push((
        Inst::TrapIf {
            trap_code: TrapCode::Interrupt,
            kind: CondBrKind::Cond(Cond::Gt),
        },
        "4D0000540000A0D4",
        "b.le 8 ; udf",
    ));
    insns.push((
        Inst::TrapIf {
            trap_code: TrapCode::Interrupt,
            kind: CondBrKind::Cond(Cond::Nv),
        },
        "4E0000540000A0D4",
        "b.al 8 ; udf",
    ));
    insns.push((
        Inst::TrapIf {
            trap_code: TrapCode::Interrupt,
            kind: CondBrKind::Cond(Cond::Al),
        },
        "4F0000540000A0D4",
        "b.nv 8 ; udf",
    ));

    insns.push((
        Inst::CondBr {
            taken: BranchTarget::ResolvedOffset(64),
            not_taken: BranchTarget::ResolvedOffset(128),
            kind: CondBrKind::Cond(Cond::Le),
        },
        "0D02005420000014",
        "b.le 64 ; b 128",
    ));

    insns.push((
        Inst::Call {
            info: Box::new(CallInfo {
                dest: ExternalName::testcase("test0"),
                uses: Vec::new(),
                defs: Vec::new(),
                opcode: Opcode::Call,
                caller_callconv: CallConv::SystemV,
                callee_callconv: CallConv::SystemV,
            }),
        },
        "00000094",
        "bl 0",
    ));

    insns.push((
        Inst::CallInd {
            info: Box::new(CallIndInfo {
                rn: xreg(10),
                uses: Vec::new(),
                defs: Vec::new(),
                opcode: Opcode::CallIndirect,
                caller_callconv: CallConv::SystemV,
                callee_callconv: CallConv::SystemV,
            }),
        },
        "40013FD6",
        "blr x10",
    ));

    insns.push((
        Inst::IndirectBr {
            rn: xreg(3),
            targets: vec![],
        },
        "60001FD6",
        "br x3",
    ));

    insns.push((Inst::Brk, "000020D4", "brk #0"));

    insns.push((
        Inst::Adr {
            rd: writable_xreg(15),
            off: (1 << 20) - 4,
        },
        "EFFF7F10",
        "adr x15, pc+1048572",
    ));

    insns.push((
        Inst::FpuMove64 {
            rd: writable_vreg(8),
            rn: vreg(4),
        },
        "8840601E",
        "fmov d8, d4",
    ));

    insns.push((
        Inst::FpuMove128 {
            rd: writable_vreg(17),
            rn: vreg(26),
        },
        "511FBA4E",
        "mov v17.16b, v26.16b",
    ));

    insns.push((
        Inst::FpuMoveFromVec {
            rd: writable_vreg(1),
            rn: vreg(30),
            idx: 2,
            size: VectorSize::Size32x4,
        },
        "C107145E",
        "mov s1, v30.s[2]",
    ));

    insns.push((
        Inst::FpuMoveFromVec {
            rd: writable_vreg(23),
            rn: vreg(11),
            idx: 0,
            size: VectorSize::Size64x2,
        },
        "7705085E",
        "mov d23, v11.d[0]",
    ));

    insns.push((
        Inst::FpuExtend {
            rd: writable_vreg(31),
            rn: vreg(0),
            size: ScalarSize::Size32,
        },
        "1F40201E",
        "fmov s31, s0",
    ));

    insns.push((
        Inst::FpuRR {
            fpu_op: FPUOp1::Abs32,
            rd: writable_vreg(15),
            rn: vreg(30),
        },
        "CFC3201E",
        "fabs s15, s30",
    ));

    insns.push((
        Inst::FpuRR {
            fpu_op: FPUOp1::Abs64,
            rd: writable_vreg(15),
            rn: vreg(30),
        },
        "CFC3601E",
        "fabs d15, d30",
    ));

    insns.push((
        Inst::FpuRR {
            fpu_op: FPUOp1::Neg32,
            rd: writable_vreg(15),
            rn: vreg(30),
        },
        "CF43211E",
        "fneg s15, s30",
    ));

    insns.push((
        Inst::FpuRR {
            fpu_op: FPUOp1::Neg64,
            rd: writable_vreg(15),
            rn: vreg(30),
        },
        "CF43611E",
        "fneg d15, d30",
    ));

    insns.push((
        Inst::FpuRR {
            fpu_op: FPUOp1::Sqrt32,
            rd: writable_vreg(15),
            rn: vreg(30),
        },
        "CFC3211E",
        "fsqrt s15, s30",
    ));

    insns.push((
        Inst::FpuRR {
            fpu_op: FPUOp1::Sqrt64,
            rd: writable_vreg(15),
            rn: vreg(30),
        },
        "CFC3611E",
        "fsqrt d15, d30",
    ));

    insns.push((
        Inst::FpuRR {
            fpu_op: FPUOp1::Cvt32To64,
            rd: writable_vreg(15),
            rn: vreg(30),
        },
        "CFC3221E",
        "fcvt d15, s30",
    ));

    insns.push((
        Inst::FpuRR {
            fpu_op: FPUOp1::Cvt64To32,
            rd: writable_vreg(15),
            rn: vreg(30),
        },
        "CF43621E",
        "fcvt s15, d30",
    ));

    insns.push((
        Inst::FpuRRR {
            fpu_op: FPUOp2::Add32,
            rd: writable_vreg(15),
            rn: vreg(30),
            rm: vreg(31),
        },
        "CF2B3F1E",
        "fadd s15, s30, s31",
    ));

    insns.push((
        Inst::FpuRRR {
            fpu_op: FPUOp2::Add64,
            rd: writable_vreg(15),
            rn: vreg(30),
            rm: vreg(31),
        },
        "CF2B7F1E",
        "fadd d15, d30, d31",
    ));

    insns.push((
        Inst::FpuRRR {
            fpu_op: FPUOp2::Sub32,
            rd: writable_vreg(15),
            rn: vreg(30),
            rm: vreg(31),
        },
        "CF3B3F1E",
        "fsub s15, s30, s31",
    ));

    insns.push((
        Inst::FpuRRR {
            fpu_op: FPUOp2::Sub64,
            rd: writable_vreg(15),
            rn: vreg(30),
            rm: vreg(31),
        },
        "CF3B7F1E",
        "fsub d15, d30, d31",
    ));

    insns.push((
        Inst::FpuRRR {
            fpu_op: FPUOp2::Mul32,
            rd: writable_vreg(15),
            rn: vreg(30),
            rm: vreg(31),
        },
        "CF0B3F1E",
        "fmul s15, s30, s31",
    ));

    insns.push((
        Inst::FpuRRR {
            fpu_op: FPUOp2::Mul64,
            rd: writable_vreg(15),
            rn: vreg(30),
            rm: vreg(31),
        },
        "CF0B7F1E",
        "fmul d15, d30, d31",
    ));

    insns.push((
        Inst::FpuRRR {
            fpu_op: FPUOp2::Div32,
            rd: writable_vreg(15),
            rn: vreg(30),
            rm: vreg(31),
        },
        "CF1B3F1E",
        "fdiv s15, s30, s31",
    ));

    insns.push((
        Inst::FpuRRR {
            fpu_op: FPUOp2::Div64,
            rd: writable_vreg(15),
            rn: vreg(30),
            rm: vreg(31),
        },
        "CF1B7F1E",
        "fdiv d15, d30, d31",
    ));

    insns.push((
        Inst::FpuRRR {
            fpu_op: FPUOp2::Max32,
            rd: writable_vreg(15),
            rn: vreg(30),
            rm: vreg(31),
        },
        "CF4B3F1E",
        "fmax s15, s30, s31",
    ));

    insns.push((
        Inst::FpuRRR {
            fpu_op: FPUOp2::Max64,
            rd: writable_vreg(15),
            rn: vreg(30),
            rm: vreg(31),
        },
        "CF4B7F1E",
        "fmax d15, d30, d31",
    ));

    insns.push((
        Inst::FpuRRR {
            fpu_op: FPUOp2::Min32,
            rd: writable_vreg(15),
            rn: vreg(30),
            rm: vreg(31),
        },
        "CF5B3F1E",
        "fmin s15, s30, s31",
    ));

    insns.push((
        Inst::FpuRRR {
            fpu_op: FPUOp2::Min64,
            rd: writable_vreg(15),
            rn: vreg(30),
            rm: vreg(31),
        },
        "CF5B7F1E",
        "fmin d15, d30, d31",
    ));

    insns.push((
        Inst::FpuRRR {
            fpu_op: FPUOp2::Uqadd64,
            rd: writable_vreg(21),
            rn: vreg(22),
            rm: vreg(23),
        },
        "D50EF77E",
        "uqadd d21, d22, d23",
    ));

    insns.push((
        Inst::FpuRRR {
            fpu_op: FPUOp2::Sqadd64,
            rd: writable_vreg(21),
            rn: vreg(22),
            rm: vreg(23),
        },
        "D50EF75E",
        "sqadd d21, d22, d23",
    ));

    insns.push((
        Inst::FpuRRR {
            fpu_op: FPUOp2::Uqsub64,
            rd: writable_vreg(21),
            rn: vreg(22),
            rm: vreg(23),
        },
        "D52EF77E",
        "uqsub d21, d22, d23",
    ));

    insns.push((
        Inst::FpuRRR {
            fpu_op: FPUOp2::Sqsub64,
            rd: writable_vreg(21),
            rn: vreg(22),
            rm: vreg(23),
        },
        "D52EF75E",
        "sqsub d21, d22, d23",
    ));

    insns.push((
        Inst::FpuRRRR {
            fpu_op: FPUOp3::MAdd32,
            rd: writable_vreg(15),
            rn: vreg(30),
            rm: vreg(31),
            ra: vreg(1),
        },
        "CF071F1F",
        "fmadd s15, s30, s31, s1",
    ));

    insns.push((
        Inst::FpuRRRR {
            fpu_op: FPUOp3::MAdd64,
            rd: writable_vreg(15),
            rn: vreg(30),
            rm: vreg(31),
            ra: vreg(1),
        },
        "CF075F1F",
        "fmadd d15, d30, d31, d1",
    ));

    insns.push((
        Inst::FpuRRI {
            fpu_op: FPUOpRI::UShr32(FPURightShiftImm::maybe_from_u8(32, 32).unwrap()),
            rd: writable_vreg(2),
            rn: vreg(5),
        },
        "A204202F",
        "ushr v2.2s, v5.2s, #32",
    ));

    insns.push((
        Inst::FpuRRI {
            fpu_op: FPUOpRI::UShr64(FPURightShiftImm::maybe_from_u8(63, 64).unwrap()),
            rd: writable_vreg(2),
            rn: vreg(5),
        },
        "A204417F",
        "ushr d2, d5, #63",
    ));

    insns.push((
        Inst::FpuRRI {
            fpu_op: FPUOpRI::Sli32(FPULeftShiftImm::maybe_from_u8(31, 32).unwrap()),
            rd: writable_vreg(4),
            rn: vreg(10),
        },
        "44553F2F",
        "sli v4.2s, v10.2s, #31",
    ));

    insns.push((
        Inst::FpuRRI {
            fpu_op: FPUOpRI::Sli64(FPULeftShiftImm::maybe_from_u8(63, 64).unwrap()),
            rd: writable_vreg(4),
            rn: vreg(10),
        },
        "44557F7F",
        "sli d4, d10, #63",
    ));

    insns.push((
        Inst::FpuToInt {
            op: FpuToIntOp::F32ToU32,
            rd: writable_xreg(1),
            rn: vreg(4),
        },
        "8100391E",
        "fcvtzu w1, s4",
    ));

    insns.push((
        Inst::FpuToInt {
            op: FpuToIntOp::F32ToU64,
            rd: writable_xreg(1),
            rn: vreg(4),
        },
        "8100399E",
        "fcvtzu x1, s4",
    ));

    insns.push((
        Inst::FpuToInt {
            op: FpuToIntOp::F32ToI32,
            rd: writable_xreg(1),
            rn: vreg(4),
        },
        "8100381E",
        "fcvtzs w1, s4",
    ));

    insns.push((
        Inst::FpuToInt {
            op: FpuToIntOp::F32ToI64,
            rd: writable_xreg(1),
            rn: vreg(4),
        },
        "8100389E",
        "fcvtzs x1, s4",
    ));

    insns.push((
        Inst::FpuToInt {
            op: FpuToIntOp::F64ToU32,
            rd: writable_xreg(1),
            rn: vreg(4),
        },
        "8100791E",
        "fcvtzu w1, d4",
    ));

    insns.push((
        Inst::FpuToInt {
            op: FpuToIntOp::F64ToU64,
            rd: writable_xreg(1),
            rn: vreg(4),
        },
        "8100799E",
        "fcvtzu x1, d4",
    ));

    insns.push((
        Inst::FpuToInt {
            op: FpuToIntOp::F64ToI32,
            rd: writable_xreg(1),
            rn: vreg(4),
        },
        "8100781E",
        "fcvtzs w1, d4",
    ));

    insns.push((
        Inst::FpuToInt {
            op: FpuToIntOp::F64ToI64,
            rd: writable_xreg(1),
            rn: vreg(4),
        },
        "8100789E",
        "fcvtzs x1, d4",
    ));

    insns.push((
        Inst::IntToFpu {
            op: IntToFpuOp::U32ToF32,
            rd: writable_vreg(1),
            rn: xreg(4),
        },
        "8100231E",
        "ucvtf s1, w4",
    ));

    insns.push((
        Inst::IntToFpu {
            op: IntToFpuOp::I32ToF32,
            rd: writable_vreg(1),
            rn: xreg(4),
        },
        "8100221E",
        "scvtf s1, w4",
    ));

    insns.push((
        Inst::IntToFpu {
            op: IntToFpuOp::U32ToF64,
            rd: writable_vreg(1),
            rn: xreg(4),
        },
        "8100631E",
        "ucvtf d1, w4",
    ));

    insns.push((
        Inst::IntToFpu {
            op: IntToFpuOp::I32ToF64,
            rd: writable_vreg(1),
            rn: xreg(4),
        },
        "8100621E",
        "scvtf d1, w4",
    ));

    insns.push((
        Inst::IntToFpu {
            op: IntToFpuOp::U64ToF32,
            rd: writable_vreg(1),
            rn: xreg(4),
        },
        "8100239E",
        "ucvtf s1, x4",
    ));

    insns.push((
        Inst::IntToFpu {
            op: IntToFpuOp::I64ToF32,
            rd: writable_vreg(1),
            rn: xreg(4),
        },
        "8100229E",
        "scvtf s1, x4",
    ));

    insns.push((
        Inst::IntToFpu {
            op: IntToFpuOp::U64ToF64,
            rd: writable_vreg(1),
            rn: xreg(4),
        },
        "8100639E",
        "ucvtf d1, x4",
    ));

    insns.push((
        Inst::IntToFpu {
            op: IntToFpuOp::I64ToF64,
            rd: writable_vreg(1),
            rn: xreg(4),
        },
        "8100629E",
        "scvtf d1, x4",
    ));

    insns.push((
        Inst::FpuCmp32 {
            rn: vreg(23),
            rm: vreg(24),
        },
        "E022381E",
        "fcmp s23, s24",
    ));

    insns.push((
        Inst::FpuCmp64 {
            rn: vreg(23),
            rm: vreg(24),
        },
        "E022781E",
        "fcmp d23, d24",
    ));

    insns.push((
        Inst::FpuLoad32 {
            rd: writable_vreg(16),
            mem: AMode::RegScaled(xreg(8), xreg(9), F32),
            flags: MemFlags::trusted(),
        },
        "107969BC",
        "ldr s16, [x8, x9, LSL #2]",
    ));

    insns.push((
        Inst::FpuLoad64 {
            rd: writable_vreg(16),
            mem: AMode::RegScaled(xreg(8), xreg(9), F64),
            flags: MemFlags::trusted(),
        },
        "107969FC",
        "ldr d16, [x8, x9, LSL #3]",
    ));

    insns.push((
        Inst::FpuLoad128 {
            rd: writable_vreg(16),
            mem: AMode::RegScaled(xreg(8), xreg(9), I128),
            flags: MemFlags::trusted(),
        },
        "1079E93C",
        "ldr q16, [x8, x9, LSL #4]",
    ));

    insns.push((
        Inst::FpuLoad32 {
            rd: writable_vreg(16),
            mem: AMode::Label(MemLabel::PCRel(8)),
            flags: MemFlags::trusted(),
        },
        "5000001C",
        "ldr s16, pc+8",
    ));

    insns.push((
        Inst::FpuLoad64 {
            rd: writable_vreg(16),
            mem: AMode::Label(MemLabel::PCRel(8)),
            flags: MemFlags::trusted(),
        },
        "5000005C",
        "ldr d16, pc+8",
    ));

    insns.push((
        Inst::FpuLoad128 {
            rd: writable_vreg(16),
            mem: AMode::Label(MemLabel::PCRel(8)),
            flags: MemFlags::trusted(),
        },
        "5000009C",
        "ldr q16, pc+8",
    ));

    insns.push((
        Inst::FpuStore32 {
            rd: vreg(16),
            mem: AMode::RegScaled(xreg(8), xreg(9), F32),
            flags: MemFlags::trusted(),
        },
        "107929BC",
        "str s16, [x8, x9, LSL #2]",
    ));

    insns.push((
        Inst::FpuStore64 {
            rd: vreg(16),
            mem: AMode::RegScaled(xreg(8), xreg(9), F64),
            flags: MemFlags::trusted(),
        },
        "107929FC",
        "str d16, [x8, x9, LSL #3]",
    ));

    insns.push((
        Inst::FpuStore128 {
            rd: vreg(16),
            mem: AMode::RegScaled(xreg(8), xreg(9), I128),
            flags: MemFlags::trusted(),
        },
        "1079A93C",
        "str q16, [x8, x9, LSL #4]",
    ));

    insns.push((
        Inst::FpuLoadP64 {
            rt: writable_vreg(0),
            rt2: writable_vreg(31),
            mem: PairAMode::SignedOffset(xreg(0), SImm7Scaled::zero(F64)),
            flags: MemFlags::trusted(),
        },
        "007C406D",
        "ldp d0, d31, [x0]",
    ));

    insns.push((
        Inst::FpuLoadP64 {
            rt: writable_vreg(19),
            rt2: writable_vreg(11),
            mem: PairAMode::PreIndexed(
                writable_xreg(25),
                SImm7Scaled::maybe_from_i64(-512, F64).unwrap(),
            ),
            flags: MemFlags::trusted(),
        },
        "332FE06D",
        "ldp d19, d11, [x25, #-512]!",
    ));

    insns.push((
        Inst::FpuLoadP64 {
            rt: writable_vreg(7),
            rt2: writable_vreg(20),
            mem: PairAMode::PostIndexed(
                writable_stack_reg(),
                SImm7Scaled::maybe_from_i64(64, F64).unwrap(),
            ),
            flags: MemFlags::trusted(),
        },
        "E753C46C",
        "ldp d7, d20, [sp], #64",
    ));

    insns.push((
        Inst::FpuStoreP64 {
            rt: vreg(4),
            rt2: vreg(26),
            mem: PairAMode::SignedOffset(
                stack_reg(),
                SImm7Scaled::maybe_from_i64(504, F64).unwrap(),
            ),
            flags: MemFlags::trusted(),
        },
        "E4EB1F6D",
        "stp d4, d26, [sp, #504]",
    ));

    insns.push((
        Inst::FpuStoreP64 {
            rt: vreg(16),
            rt2: vreg(8),
            mem: PairAMode::PreIndexed(
                writable_xreg(15),
                SImm7Scaled::maybe_from_i64(48, F64).unwrap(),
            ),
            flags: MemFlags::trusted(),
        },
        "F021836D",
        "stp d16, d8, [x15, #48]!",
    ));

    insns.push((
        Inst::FpuStoreP64 {
            rt: vreg(5),
            rt2: vreg(6),
            mem: PairAMode::PostIndexed(
                writable_xreg(28),
                SImm7Scaled::maybe_from_i64(-32, F64).unwrap(),
            ),
            flags: MemFlags::trusted(),
        },
        "851BBE6C",
        "stp d5, d6, [x28], #-32",
    ));

    insns.push((
        Inst::FpuLoadP128 {
            rt: writable_vreg(0),
            rt2: writable_vreg(17),
            mem: PairAMode::SignedOffset(xreg(3), SImm7Scaled::zero(I8X16)),
            flags: MemFlags::trusted(),
        },
        "604440AD",
        "ldp q0, q17, [x3]",
    ));

    insns.push((
        Inst::FpuLoadP128 {
            rt: writable_vreg(29),
            rt2: writable_vreg(9),
            mem: PairAMode::PreIndexed(
                writable_xreg(16),
                SImm7Scaled::maybe_from_i64(-1024, I8X16).unwrap(),
            ),
            flags: MemFlags::trusted(),
        },
        "1D26E0AD",
        "ldp q29, q9, [x16, #-1024]!",
    ));

    insns.push((
        Inst::FpuLoadP128 {
            rt: writable_vreg(10),
            rt2: writable_vreg(20),
            mem: PairAMode::PostIndexed(
                writable_xreg(26),
                SImm7Scaled::maybe_from_i64(256, I8X16).unwrap(),
            ),
            flags: MemFlags::trusted(),
        },
        "4A53C8AC",
        "ldp q10, q20, [x26], #256",
    ));

    insns.push((
        Inst::FpuStoreP128 {
            rt: vreg(9),
            rt2: vreg(31),
            mem: PairAMode::SignedOffset(
                stack_reg(),
                SImm7Scaled::maybe_from_i64(1008, I8X16).unwrap(),
            ),
            flags: MemFlags::trusted(),
        },
        "E9FF1FAD",
        "stp q9, q31, [sp, #1008]",
    ));

    insns.push((
        Inst::FpuStoreP128 {
            rt: vreg(27),
            rt2: vreg(13),
            mem: PairAMode::PreIndexed(
                writable_stack_reg(),
                SImm7Scaled::maybe_from_i64(-192, I8X16).unwrap(),
            ),
            flags: MemFlags::trusted(),
        },
        "FB37BAAD",
        "stp q27, q13, [sp, #-192]!",
    ));

    insns.push((
        Inst::FpuStoreP128 {
            rt: vreg(18),
            rt2: vreg(22),
            mem: PairAMode::PostIndexed(
                writable_xreg(13),
                SImm7Scaled::maybe_from_i64(304, I8X16).unwrap(),
            ),
            flags: MemFlags::trusted(),
        },
        "B2D989AC",
        "stp q18, q22, [x13], #304",
    ));

    insns.push((
        Inst::LoadFpuConst64 {
            rd: writable_vreg(16),
            const_data: 1.0_f64.to_bits(),
        },
        "5000005C03000014000000000000F03F",
        "ldr d16, pc+8 ; b 12 ; data.f64 1",
    ));

    insns.push((
        Inst::LoadFpuConst128 {
            rd: writable_vreg(5),
            const_data: 0x0f0e0d0c0b0a09080706050403020100,
        },
        "4500009C05000014000102030405060708090A0B0C0D0E0F",
        "ldr q5, pc+8 ; b 20 ; data.f128 0x0f0e0d0c0b0a09080706050403020100",
    ));

    insns.push((
        Inst::FpuCSel32 {
            rd: writable_vreg(1),
            rn: vreg(2),
            rm: vreg(3),
            cond: Cond::Hi,
        },
        "418C231E",
        "fcsel s1, s2, s3, hi",
    ));

    insns.push((
        Inst::FpuCSel64 {
            rd: writable_vreg(1),
            rn: vreg(2),
            rm: vreg(3),
            cond: Cond::Eq,
        },
        "410C631E",
        "fcsel d1, d2, d3, eq",
    ));

    insns.push((
        Inst::FpuRound {
            rd: writable_vreg(23),
            rn: vreg(24),
            op: FpuRoundMode::Minus32,
        },
        "1743251E",
        "frintm s23, s24",
    ));
    insns.push((
        Inst::FpuRound {
            rd: writable_vreg(23),
            rn: vreg(24),
            op: FpuRoundMode::Minus64,
        },
        "1743651E",
        "frintm d23, d24",
    ));
    insns.push((
        Inst::FpuRound {
            rd: writable_vreg(23),
            rn: vreg(24),
            op: FpuRoundMode::Plus32,
        },
        "17C3241E",
        "frintp s23, s24",
    ));
    insns.push((
        Inst::FpuRound {
            rd: writable_vreg(23),
            rn: vreg(24),
            op: FpuRoundMode::Plus64,
        },
        "17C3641E",
        "frintp d23, d24",
    ));
    insns.push((
        Inst::FpuRound {
            rd: writable_vreg(23),
            rn: vreg(24),
            op: FpuRoundMode::Zero32,
        },
        "17C3251E",
        "frintz s23, s24",
    ));
    insns.push((
        Inst::FpuRound {
            rd: writable_vreg(23),
            rn: vreg(24),
            op: FpuRoundMode::Zero64,
        },
        "17C3651E",
        "frintz d23, d24",
    ));
    insns.push((
        Inst::FpuRound {
            rd: writable_vreg(23),
            rn: vreg(24),
            op: FpuRoundMode::Nearest32,
        },
        "1743241E",
        "frintn s23, s24",
    ));
    insns.push((
        Inst::FpuRound {
            rd: writable_vreg(23),
            rn: vreg(24),
            op: FpuRoundMode::Nearest64,
        },
        "1743641E",
        "frintn d23, d24",
    ));

    insns.push((
        Inst::AtomicRMW {
            ty: I16,
            op: inst_common::AtomicRmwOp::Xor,
        },
        "BF3B03D53B7F5F487C031ACA3C7F1848B8FFFFB5BF3B03D5",
        "atomically { 16_bits_at_[x25]) Xor= x26 ; x27 = old_value_at_[x25]; x24,x28 = trash }",
    ));

    insns.push((
        Inst::AtomicRMW {
            ty: I32,
            op: inst_common::AtomicRmwOp::Xchg,
        },
        "BF3B03D53B7F5F88FC031AAA3C7F1888B8FFFFB5BF3B03D5",
        "atomically { 32_bits_at_[x25]) Xchg= x26 ; x27 = old_value_at_[x25]; x24,x28 = trash }",
    ));
    insns.push((
        Inst::AtomicCAS {
            rs: writable_xreg(28),
            rt: xreg(20),
            rn: xreg(10),
            ty: I8,
        },
        "54FDFC08",
        "casalb w28, w20, [x10]",
    ));
    insns.push((
        Inst::AtomicCAS {
            rs: writable_xreg(2),
            rt: xreg(19),
            rn: xreg(23),
            ty: I16,
        },
        "F3FEE248",
        "casalh w2, w19, [x23]",
    ));
    insns.push((
        Inst::AtomicCAS {
            rs: writable_xreg(0),
            rt: zero_reg(),
            rn: stack_reg(),
            ty: I32,
        },
        "FFFFE088",
        "casal w0, wzr, [sp]",
    ));
    insns.push((
        Inst::AtomicCAS {
            rs: writable_xreg(7),
            rt: xreg(15),
            rn: xreg(27),
            ty: I64,
        },
        "6FFFE7C8",
        "casal x7, x15, [x27]",
    ));
    insns.push((
        Inst::AtomicCASLoop {
            ty: I8,
        },
        "BF3B03D53B7F5F08581F40927F0318EB610000543C7F180878FFFFB5BF3B03D5",
        "atomically { compare-and-swap(8_bits_at_[x25], x26 -> x28), x27 = old_value_at_[x25]; x24 = trash }"
    ));

    insns.push((
        Inst::AtomicCASLoop {
            ty: I64,
        },
        "BF3B03D53B7F5FC8F8031AAA7F0318EB610000543C7F18C878FFFFB5BF3B03D5",
        "atomically { compare-and-swap(64_bits_at_[x25], x26 -> x28), x27 = old_value_at_[x25]; x24 = trash }"
    ));

    insns.push((
        Inst::AtomicLoad {
            ty: I8,
            r_data: writable_xreg(7),
            r_addr: xreg(28),
        },
        "BF3B03D587034039",
        "atomically { x7 = zero_extend_8_bits_at[x28] }",
    ));

    insns.push((
        Inst::AtomicLoad {
            ty: I64,
            r_data: writable_xreg(28),
            r_addr: xreg(7),
        },
        "BF3B03D5FC0040F9",
        "atomically { x28 = zero_extend_64_bits_at[x7] }",
    ));

    insns.push((
        Inst::AtomicStore {
            ty: I16,
            r_data: xreg(17),
            r_addr: xreg(8),
        },
        "11010079BF3B03D5",
        "atomically { 16_bits_at[x8] = x17 }",
    ));

    insns.push((
        Inst::AtomicStore {
            ty: I32,
            r_data: xreg(18),
            r_addr: xreg(7),
        },
        "F20000B9BF3B03D5",
        "atomically { 32_bits_at[x7] = x18 }",
    ));

    insns.push((Inst::Fence {}, "BF3B03D5", "dmb ish"));

    let flags = settings::Flags::new(settings::builder());
    let rru = create_reg_universe(&flags);
    let emit_info = EmitInfo::new(flags);
    for (insn, expected_encoding, expected_printing) in insns {
        println!(
            "AArch64: {:?}, {}, {}",
            insn, expected_encoding, expected_printing
        );

        // Check the printed text is as expected.
        let actual_printing = insn.show_rru(Some(&rru));
        assert_eq!(expected_printing, actual_printing);

        let mut sink = test_utils::TestCodeSink::new();
        let mut buffer = MachBuffer::new();
        insn.emit(&mut buffer, &emit_info, &mut Default::default());
        let buffer = buffer.finish();
        buffer.emit(&mut sink);
        let actual_encoding = &sink.stringify();
        assert_eq!(expected_encoding, actual_encoding);
    }
}

#[test]
fn test_cond_invert() {
    for cond in vec![
        Cond::Eq,
        Cond::Ne,
        Cond::Hs,
        Cond::Lo,
        Cond::Mi,
        Cond::Pl,
        Cond::Vs,
        Cond::Vc,
        Cond::Hi,
        Cond::Ls,
        Cond::Ge,
        Cond::Lt,
        Cond::Gt,
        Cond::Le,
        Cond::Al,
        Cond::Nv,
    ]
    .into_iter()
    {
        assert_eq!(cond.invert().invert(), cond);
    }
}
