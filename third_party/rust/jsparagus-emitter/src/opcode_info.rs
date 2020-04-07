pub fn get_async_function_resolve_kind() -> &'static str {
    include_str!("copy/AsyncFunctionResolveKind.h")
}

pub fn get_bytecode_format_flags() -> &'static str {
    include_str!("copy/BytecodeFormatFlags.h")
}

pub fn get_check_is_callable_kind() -> &'static str {
    include_str!("copy/CheckIsCallableKind.h")
}

pub fn get_check_is_object_kind() -> &'static str {
    include_str!("copy/CheckIsObjectKind.h")
}

pub fn get_function_prefix_kind() -> &'static str {
    include_str!("copy/FunctionPrefixKind.h")
}

pub fn get_generator_resume_kind() -> &'static str {
    include_str!("copy/GeneratorResumeKind.h")
}

pub fn get_opcodes_source() -> &'static str {
    include_str!("copy/Opcodes.h")
}

pub fn get_throw_msg_kind() -> &'static str {
    include_str!("copy/ThrowMsgKind.h")
}

pub fn get_try_note_kind() -> &'static str {
    include_str!("copy/TryNoteKind.h")
}
