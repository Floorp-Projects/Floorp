pub fn get_opcodes_source() -> &'static str {
    include_str!("copy/Opcodes.h")
}

pub fn get_bytecode_util_source() -> &'static str {
    include_str!("copy/BytecodeUtil.h")
}
