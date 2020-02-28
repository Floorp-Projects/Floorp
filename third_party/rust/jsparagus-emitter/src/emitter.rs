//! Low-level bytecode emitter, used by ast_builder.
//!
//! This API makes it easy to emit correct individual bytecode instructions.

// Most of this functionality isn't used yet.
#![allow(dead_code)]

use super::opcode::Opcode;
use byteorder::{ByteOrder, LittleEndian};
use std::convert::TryInto;
use std::fmt;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum ResumeKind {
    Normal = 0,
    Throw = 1,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum AsyncFunctionResolveKind {
    Fulfill = 0,
    Reject = 1,
}

#[allow(non_camel_case_types)]
pub type u24 = u32;

/// For tracking bytecode offsets in jumps
#[derive(Clone, Copy, PartialEq, Debug)]
#[must_use]
pub struct BytecodeOffset {
    pub offset: usize,
}

// Struct to hold atom index in `EmitResult.strings`.
// Opaque from outside of this module to avoid creating invalid atom index.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct AtomIndex {
    index: u32,
}
impl AtomIndex {
    fn new(index: u32) -> Self {
        Self { index }
    }
}

/// Low-level bytecode emitter.
pub struct InstructionWriter {
    bytecode: Vec<u8>,
    strings: Vec<String>,

    /// Stack depth after the instructions emitted so far.
    stack_depth: usize,

    /// Maximum stack_depth at any point in the instructions emitted so far.
    maximum_stack_depth: usize,

    /// Number of JOF_IC instructions emitted so far.
    num_ic_entries: usize,

    /// Number of instructions in this script that have JOF_TYPESET.
    num_type_sets: usize,
}

#[derive(Debug)]
pub struct EmitOptions {
    pub no_script_rval: bool,
}
impl EmitOptions {
    pub fn new() -> Self {
        Self {
            no_script_rval: false,
        }
    }
}

/// The output of bytecode-compiling a script or module.
#[derive(Debug)]
pub struct EmitResult {
    pub bytecode: Vec<u8>,
    pub strings: Vec<String>,

    // Line and column numbers for the first character of source.
    pub lineno: usize,
    pub column: usize,

    pub main_offset: usize,
    pub max_fixed_slots: u32,
    pub maximum_stack_depth: u32,
    pub body_scope_index: u32,
    pub num_ic_entries: u32,
    pub num_type_sets: u32,

    pub strict: bool,
    pub bindings_accessed_dynamically: bool,
    pub has_call_site_obj: bool,
    pub is_for_eval: bool,
    pub is_module: bool,
    pub is_function: bool,
    pub has_non_syntactic_scope: bool,
    pub needs_function_environment_objects: bool,
    pub has_module_goal: bool,
}

/// The error of bytecode-compilation.
#[derive(Clone, Debug)]
pub enum EmitError {
    NotImplemented(&'static str),
}

impl fmt::Display for EmitError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            EmitError::NotImplemented(message) => write!(f, "not implemented: {}", message),
        }
    }
}

impl InstructionWriter {
    pub fn new() -> Self {
        Self {
            bytecode: Vec::new(),
            strings: Vec::new(),
            stack_depth: 0,
            maximum_stack_depth: 0,
            num_ic_entries: 0,
            num_type_sets: 0,
        }
    }

    pub fn into_emit_result(self) -> EmitResult {
        EmitResult {
            bytecode: self.bytecode,
            strings: self.strings,

            lineno: 1,
            column: 0,

            main_offset: 0,
            max_fixed_slots: 0,

            // These values probably can't be out of range for u32, as we would
            // have hit other limits first. Release-assert anyway.
            maximum_stack_depth: self.maximum_stack_depth.try_into().unwrap(),
            body_scope_index: 0,
            num_ic_entries: self.num_ic_entries.try_into().unwrap(),
            num_type_sets: self.num_type_sets.try_into().unwrap(),

            strict: false,
            bindings_accessed_dynamically: false,
            has_call_site_obj: false,
            is_for_eval: false,
            is_module: false,
            is_function: false,
            has_non_syntactic_scope: false,
            needs_function_environment_objects: false,
            has_module_goal: false,
        }
    }

    fn write_u16(&mut self, value: u16) {
        self.bytecode.extend_from_slice(&value.to_le_bytes());
    }

    fn write_u32(&mut self, value: u32) {
        self.bytecode.extend_from_slice(&value.to_le_bytes());
    }

    fn emit_op(&mut self, opcode: Opcode) {
        let nuses: isize = opcode.nuses();
        assert!(nuses >= 0);
        self.emit_op_common(opcode, nuses as usize);
    }

    fn emit_argc_op(&mut self, opcode: Opcode, argc: u16) {
        assert!(opcode.has_argc());
        assert_eq!(opcode.nuses(), -1);
        self.emit_op_common(opcode, argc as usize);
        self.write_u16(argc);
    }

    fn emit_op_common(&mut self, opcode: Opcode, nuses: usize) {
        assert!(
            self.stack_depth >= nuses as usize,
            "InstructionWriter misuse! Not enough arguments on the stack."
        );
        self.stack_depth -= nuses as usize;

        let ndefs = opcode.ndefs();
        if ndefs > 0 {
            self.stack_depth += ndefs;
            if self.stack_depth > self.maximum_stack_depth {
                self.maximum_stack_depth = self.stack_depth;
            }
        }

        if opcode.has_ic_entry() {
            self.num_ic_entries += 1;
        }

        self.bytecode.push(opcode.to_byte());

        if opcode.has_ic_index() {
            self.write_u32(self.num_ic_entries.try_into().unwrap());
        }

        if opcode.has_typeset() {
            self.num_type_sets += 1;
        }
    }

    fn emit1(&mut self, opcode: Opcode) {
        self.emit_op(opcode);
    }

    fn emit_i8(&mut self, opcode: Opcode, value: i8) {
        self.emit_u8(opcode, value as u8);
    }

    fn emit_u8(&mut self, opcode: Opcode, value: u8) {
        self.emit_op(opcode);
        self.bytecode.push(value);
    }

    fn emit_u16(&mut self, opcode: Opcode, value: u16) {
        self.emit_op(opcode);
        self.write_u16(value);
    }

    fn emit_u24(&mut self, opcode: Opcode, value: u24) {
        self.emit_op(opcode);
        let slice = value.to_le_bytes();
        assert!(slice.len() == 4 && slice[3] == 0);
        self.bytecode.extend_from_slice(&slice[0..3]);
    }

    fn emit_i32(&mut self, opcode: Opcode, value: i32) {
        self.emit_op(opcode);
        self.bytecode.extend_from_slice(&value.to_le_bytes());
    }

    fn emit_u32(&mut self, opcode: Opcode, value: u32) {
        self.emit_op(opcode);
        self.write_u32(value);
    }

    fn emit_with_atom(&mut self, opcode: Opcode, atom_index: AtomIndex) {
        self.emit_op(opcode);
        self.emit_atom_index(atom_index);
    }

    fn emit_aliased(&mut self, opcode: Opcode, hops: u8, slot: u24) {
        self.emit_op(opcode);
        self.bytecode.push(hops);
        let slice = slot.to_le_bytes();
        assert!(slice.len() == 4 && slice[3] == 0);
        self.bytecode.extend_from_slice(&slice[0..3]);
    }

    fn emit_with_offset(&mut self, opcode: Opcode, offset: i32) {
        self.emit_i32(opcode, offset);
    }

    pub fn get_atom_index(&mut self, value: &str) -> AtomIndex {
        // Eventually we should be fancy and make this O(1)
        for (i, string) in self.strings.iter().enumerate() {
            if string == value {
                return AtomIndex::new(i as u32);
            }
        }

        let index = self.strings.len();
        self.strings.push(value.to_string());
        AtomIndex::new(index as u32)
    }

    fn emit_atom_index(&mut self, atom_index: AtomIndex) {
        self.bytecode
            .extend_from_slice(&atom_index.index.to_ne_bytes());
    }

    // Public methods to emit each instruction.

    pub fn undefined(&mut self) {
        self.emit1(Opcode::Undefined);
    }

    pub fn null(&mut self) {
        self.emit1(Opcode::Null);
    }

    pub fn emit_boolean(&mut self, value: bool) {
        self.emit1(if value { Opcode::True } else { Opcode::False });
    }

    pub fn int32(&mut self, value: i32) {
        self.emit_i32(Opcode::Int32, value);
    }

    pub fn zero(&mut self) {
        self.emit1(Opcode::Zero);
    }

    pub fn one(&mut self) {
        self.emit1(Opcode::One);
    }

    pub fn int8(&mut self, value: i8) {
        self.emit_i8(Opcode::Int8, value);
    }

    pub fn uint16(&mut self, value: u16) {
        self.emit_u16(Opcode::Uint16, value);
    }

    pub fn uint24(&mut self, value: u24) {
        self.emit_u24(Opcode::Uint24, value);
    }

    pub fn double(&mut self, value: f64) {
        self.emit_op(Opcode::Double);
        self.bytecode
            .extend_from_slice(&value.to_bits().to_le_bytes());
    }

    pub fn big_int(&mut self, const_index: u32) {
        self.emit_u32(Opcode::BigInt, const_index);
    }

    pub fn string(&mut self, str_index: AtomIndex) {
        self.emit_op(Opcode::String);
        self.emit_atom_index(str_index);
    }

    pub fn symbol(&mut self, symbol: u8) {
        self.emit_u8(Opcode::Symbol, symbol);
    }

    pub fn emit_unary_op(&mut self, opcode: Opcode) {
        assert!(opcode.is_simple_unary_operator());
        self.emit1(opcode);
    }

    pub fn typeof_(&mut self) {
        self.emit1(Opcode::Typeof);
    }

    pub fn typeof_expr(&mut self) {
        self.emit1(Opcode::TypeofExpr);
    }

    pub fn emit_binary_op(&mut self, opcode: Opcode) {
        assert!(opcode.is_simple_binary_operator());
        debug_assert_eq!(opcode.nuses(), 2);
        debug_assert_eq!(opcode.ndefs(), 1);
        self.emit1(opcode);
    }

    pub fn inc(&mut self) {
        self.emit1(Opcode::Inc);
    }

    pub fn dec(&mut self) {
        self.emit1(Opcode::Dec);
    }

    pub fn to_id(&mut self) {
        self.emit1(Opcode::ToId);
    }

    pub fn to_numeric(&mut self) {
        self.emit1(Opcode::ToNumeric);
    }

    pub fn to_string(&mut self) {
        self.emit1(Opcode::ToString);
    }

    pub fn global_this(&mut self) {
        self.emit1(Opcode::GlobalThis);
    }

    pub fn new_target(&mut self) {
        self.emit1(Opcode::NewTarget);
    }

    pub fn dynamic_import(&mut self) {
        self.emit1(Opcode::DynamicImport);
    }

    pub fn import_meta(&mut self) {
        self.emit1(Opcode::ImportMeta);
    }

    pub fn new_init(&mut self, extra: u32) {
        self.emit_u32(Opcode::NewInit, extra);
    }

    pub fn new_object(&mut self, base_obj_index: u32) {
        self.emit_u32(Opcode::NewObject, base_obj_index);
    }

    pub fn new_object_with_group(&mut self, base_obj_index: u32) {
        self.emit_u32(Opcode::NewObjectWithGroup, base_obj_index);
    }

    pub fn object(&mut self, object_index: u32) {
        self.emit_u32(Opcode::Object, object_index);
    }

    pub fn obj_with_proto(&mut self) {
        self.emit1(Opcode::ObjWithProto);
    }

    pub fn init_prop(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::InitProp, name);
    }

    pub fn init_hidden_prop(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::InitHiddenProp, name);
    }

    pub fn init_locked_prop(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::InitLockedProp, name);
    }

    pub fn init_elem(&mut self) {
        self.emit1(Opcode::InitElem);
    }

    pub fn init_hidden_elem(&mut self) {
        self.emit1(Opcode::InitHiddenElem);
    }

    pub fn init_prop_getter(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::InitPropGetter, name);
    }

    pub fn init_hidden_prop_getter(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::InitHiddenPropGetter, name);
    }

    pub fn init_elem_getter(&mut self) {
        self.emit1(Opcode::InitElemGetter);
    }

    pub fn init_hidden_elem_getter(&mut self) {
        self.emit1(Opcode::InitHiddenElemGetter);
    }

    pub fn init_prop_setter(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::InitPropSetter, name);
    }

    pub fn init_hidden_prop_setter(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::InitHiddenPropSetter, name);
    }

    pub fn init_elem_setter(&mut self) {
        self.emit1(Opcode::InitElemSetter);
    }

    pub fn init_hidden_elem_setter(&mut self) {
        self.emit1(Opcode::InitHiddenElemSetter);
    }

    pub fn get_prop(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::GetProp, name);
    }

    pub fn call_prop(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::CallProp, name);
    }

    pub fn get_elem(&mut self) {
        self.emit1(Opcode::GetElem);
    }

    pub fn call_elem(&mut self) {
        self.emit1(Opcode::CallElem);
    }

    pub fn length(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::Length, name);
    }

    pub fn set_prop(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::SetProp, name);
    }

    pub fn strict_set_prop(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::StrictSetProp, name);
    }

    pub fn set_elem(&mut self) {
        self.emit1(Opcode::SetElem);
    }

    pub fn strict_set_elem(&mut self) {
        self.emit1(Opcode::StrictSetElem);
    }

    pub fn del_prop(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::DelProp, name);
    }

    pub fn strict_del_prop(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::StrictDelProp, name);
    }

    pub fn del_elem(&mut self) {
        self.emit1(Opcode::DelElem);
    }

    pub fn strict_del_elem(&mut self) {
        self.emit1(Opcode::StrictDelElem);
    }

    pub fn has_own(&mut self) {
        self.emit1(Opcode::HasOwn);
    }

    pub fn super_base(&mut self) {
        self.emit1(Opcode::SuperBase);
    }

    pub fn get_prop_super(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::GetPropSuper, name);
    }

    pub fn get_elem_super(&mut self) {
        self.emit1(Opcode::GetElemSuper);
    }

    pub fn set_prop_super(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::SetPropSuper, name);
    }

    pub fn strict_set_prop_super(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::StrictSetPropSuper, name);
    }

    pub fn set_elem_super(&mut self) {
        self.emit1(Opcode::SetElemSuper);
    }

    pub fn strict_set_elem_super(&mut self) {
        self.emit1(Opcode::StrictSetElemSuper);
    }

    pub fn iter(&mut self) {
        self.emit1(Opcode::Iter);
    }

    pub fn more_iter(&mut self) {
        self.emit1(Opcode::MoreIter);
    }

    pub fn is_no_iter(&mut self) {
        self.emit1(Opcode::IsNoIter);
    }

    pub fn iter_next(&mut self) {
        self.emit1(Opcode::IterNext);
    }

    pub fn end_iter(&mut self) {
        self.emit1(Opcode::EndIter);
    }

    // TODO - stronger typing of this parameter
    pub fn check_is_obj(&mut self, kind: u8) {
        self.emit_u8(Opcode::CheckIsObj, kind);
    }

    pub fn check_is_callable(&mut self, kind: u8) {
        self.emit_u8(Opcode::CheckIsCallable, kind);
    }

    pub fn check_obj_coercible(&mut self) {
        self.emit1(Opcode::CheckObjCoercible);
    }

    pub fn to_async_iter(&mut self) {
        self.emit1(Opcode::ToAsyncIter);
    }

    pub fn mutate_proto(&mut self) {
        self.emit1(Opcode::MutateProto);
    }

    pub fn new_array(&mut self, length: u32) {
        self.emit_u32(Opcode::NewArray, length);
    }

    pub fn init_elem_array(&mut self, index: u32) {
        self.emit_u32(Opcode::InitElemArray, index);
    }

    pub fn init_elem_inc(&mut self) {
        self.emit1(Opcode::InitElemInc);
    }

    pub fn hole(&mut self) {
        self.emit1(Opcode::Hole);
    }

    pub fn new_array_copy_on_write(&mut self, object_index: u32) {
        self.emit_u32(Opcode::NewArrayCopyOnWrite, object_index);
    }

    pub fn reg_exp(&mut self, regexp_index: u32) {
        self.emit_u32(Opcode::RegExp, regexp_index);
    }

    pub fn lambda(&mut self, func_index: u32) {
        self.emit_u32(Opcode::Lambda, func_index);
    }

    pub fn lambda_arrow(&mut self, func_index: u32) {
        self.emit_u32(Opcode::LambdaArrow, func_index);
    }

    pub fn set_fun_name(&mut self, prefix_kind: u8) {
        self.emit_u8(Opcode::SetFunName, prefix_kind);
    }

    pub fn init_home_object(&mut self) {
        self.emit1(Opcode::InitHomeObject);
    }

    pub fn check_class_heritage(&mut self) {
        self.emit1(Opcode::CheckClassHeritage);
    }

    pub fn fun_with_proto(&mut self, func_index: u32) {
        self.emit_u32(Opcode::FunWithProto, func_index);
    }

    pub fn class_constructor(&mut self, atom_index: u32) {
        self.emit_u32(Opcode::ClassConstructor, atom_index);
    }

    pub fn derived_constructor(&mut self, atom_index: u32) {
        self.emit_u32(Opcode::DerivedConstructor, atom_index);
    }

    pub fn builtin_proto(&mut self, kind: u8) {
        self.emit_u8(Opcode::BuiltinProto, kind);
    }

    pub fn call(&mut self, argc: u16) {
        self.emit_argc_op(Opcode::Call, argc);
    }

    pub fn call_iter(&mut self) {
        // JSOP_CALLITER has an operand in bytecode, for consistency with other
        // call opcodes, but it must be 0.
        self.emit_argc_op(Opcode::CallIter, 0);
    }

    pub fn fun_apply(&mut self, argc: u16) {
        self.emit_argc_op(Opcode::FunApply, argc);
    }

    pub fn fun_call(&mut self, argc: u16) {
        self.emit_argc_op(Opcode::FunCall, argc);
    }

    pub fn call_ignores_rv(&mut self, argc: u16) {
        self.emit_argc_op(Opcode::CallIgnoresRv, argc);
    }

    pub fn spread_call(&mut self) {
        self.emit1(Opcode::SpreadCall);
    }

    pub fn optimize_spread_call(&mut self) {
        self.emit1(Opcode::OptimizeSpreadCall);
    }

    pub fn eval(&mut self, argc: u16) {
        self.emit_argc_op(Opcode::Eval, argc);
    }

    pub fn spread_eval(&mut self) {
        self.emit1(Opcode::SpreadEval);
    }

    pub fn strict_eval(&mut self, argc: u16) {
        self.emit_argc_op(Opcode::StrictEval, argc);
    }

    pub fn strict_spread_eval(&mut self) {
        self.emit1(Opcode::StrictSpreadEval);
    }

    pub fn implicit_this(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::ImplicitThis, name);
    }

    pub fn g_implicit_this(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::GImplicitThis, name);
    }

    pub fn call_site_obj(&mut self, object_index: u32) {
        self.emit_u32(Opcode::CallSiteObj, object_index);
    }

    pub fn is_constructing(&mut self) {
        self.emit1(Opcode::IsConstructing);
    }

    pub fn new_(&mut self, argc: u16) {
        self.emit_argc_op(Opcode::New, argc);
    }

    pub fn spread_new(&mut self) {
        self.emit1(Opcode::SpreadNew);
    }

    pub fn super_fun(&mut self) {
        self.emit1(Opcode::SuperFun);
    }

    pub fn super_call(&mut self, argc: u16) {
        self.emit_argc_op(Opcode::SuperCall, argc);
    }

    pub fn spread_super_call(&mut self) {
        self.emit1(Opcode::SpreadSuperCall);
    }

    pub fn check_this_reinit(&mut self) {
        self.emit1(Opcode::CheckThisReinit);
    }

    pub fn generator(&mut self) {
        self.emit1(Opcode::Generator);
    }

    pub fn initial_yield(&mut self, resume_index: u24) {
        self.emit_u24(Opcode::InitialYield, resume_index);
    }

    pub fn after_yield(&mut self) {
        self.emit_op(Opcode::AfterYield);
    }

    pub fn final_yield_rval(&mut self) {
        self.emit1(Opcode::FinalYieldRval);
    }

    pub fn yield_(&mut self, resume_index: u24) {
        self.emit_u24(Opcode::Yield, resume_index);
    }

    pub fn is_gen_closing(&mut self) {
        self.emit1(Opcode::IsGenClosing);
    }

    pub fn async_await(&mut self) {
        self.emit1(Opcode::AsyncAwait);
    }

    pub fn async_resolve(&mut self, fulfill_or_reject: AsyncFunctionResolveKind) {
        self.emit_u8(Opcode::AsyncResolve, fulfill_or_reject as u8);
    }

    pub fn await_(&mut self, resume_index: u24) {
        self.emit_u24(Opcode::Await, resume_index);
    }

    pub fn try_skip_await(&mut self) {
        self.emit1(Opcode::TrySkipAwait);
    }

    pub fn resume(&mut self, kind: ResumeKind) {
        self.emit_u8(Opcode::Resume, kind as u8);
    }

    pub fn jump_target(&mut self) {
        self.emit_op(Opcode::JumpTarget);
    }

    pub fn bytecode_offset(&mut self) -> BytecodeOffset {
        BytecodeOffset {
            offset: self.bytecode.len(),
        }
    }

    pub fn patch_jump_target(&mut self, jumplist: Vec<BytecodeOffset>) {
        let target = self.bytecode_offset();
        for jump in jumplist {
            let new_target = (target.offset - jump.offset) as i32;
            let index = jump.offset + 1;
            LittleEndian::write_i32(&mut self.bytecode[index..index + 4], new_target);
        }
    }

    pub fn loop_head(&mut self) {
        self.emit_op(Opcode::LoopHead);
    }

    pub fn goto(&mut self, offset: i32) {
        self.emit_with_offset(Opcode::Goto, offset);
    }

    pub fn if_eq(&mut self, offset: i32) {
        self.emit_with_offset(Opcode::IfEq, offset);
    }

    pub fn if_ne(&mut self, offset: i32) {
        self.emit_with_offset(Opcode::IfNe, offset);
    }

    pub fn and(&mut self, offset: i32) {
        self.emit_with_offset(Opcode::And, offset);
    }

    pub fn or(&mut self, offset: i32) {
        self.emit_with_offset(Opcode::Or, offset);
    }

    pub fn coalesce(&mut self, offset: i32) {
        self.emit_with_offset(Opcode::Coalesce, offset);
    }

    pub fn case(&mut self, offset: i32) {
        self.emit_with_offset(Opcode::Case, offset);
    }

    pub fn default(&mut self, offset: i32) {
        self.emit_with_offset(Opcode::Default, offset);
    }

    pub fn table_switch(
        &mut self,
        _len: i32,
        _low: i32,
        _high: i32,
        _first_resume_index: u24,
    ) -> Result<(), EmitError> {
        Err(EmitError::NotImplemented("TODO: table_switch"))
    }

    pub fn return_(&mut self) {
        self.emit1(Opcode::Return);
    }

    pub fn get_rval(&mut self) {
        self.emit1(Opcode::GetRval);
    }

    pub fn set_rval(&mut self) {
        self.emit1(Opcode::SetRval);
    }

    pub fn ret_rval(&mut self) {
        self.emit1(Opcode::RetRval);
    }

    pub fn check_return(&mut self) {
        self.emit1(Opcode::CheckReturn);
    }

    pub fn throw(&mut self) {
        self.emit1(Opcode::Throw);
    }

    pub fn throw_msg(&mut self, message_number: u16) {
        self.emit_u16(Opcode::ThrowMsg, message_number);
    }

    pub fn throw_set_aliased_const(&mut self, hops: u8, slot: u24) {
        self.emit_aliased(Opcode::ThrowSetAliasedConst, hops, slot);
    }

    pub fn throw_set_callee(&mut self) {
        self.emit1(Opcode::ThrowSetCallee);
    }

    pub fn throw_set_const(&mut self, local_no: u24) {
        self.emit_u24(Opcode::ThrowSetConst, local_no);
    }

    pub fn try_(&mut self) {
        self.emit1(Opcode::Try);
    }

    pub fn try_destructuring(&mut self) {
        self.emit1(Opcode::TryDestructuring);
    }

    pub fn exception(&mut self) {
        self.emit1(Opcode::Exception);
    }

    pub fn resume_index(&mut self, resume_index: u24) {
        self.emit_u24(Opcode::ResumeIndex, resume_index);
    }

    pub fn gosub(&mut self, offset: i32) {
        self.emit_with_offset(Opcode::Gosub, offset);
    }

    pub fn finally(&mut self) {
        self.emit1(Opcode::Finally);
    }

    pub fn retsub(&mut self) {
        self.emit1(Opcode::Retsub);
    }

    pub fn uninitialized(&mut self) {
        self.emit1(Opcode::Uninitialized);
    }

    pub fn init_lexical(&mut self, local_no: u24) {
        self.emit_u24(Opcode::InitLexical, local_no);
    }

    pub fn init_g_lexical(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::InitGLexical, name);
    }

    pub fn init_aliased_lexical(&mut self, hops: u8, slot: u24) {
        self.emit_aliased(Opcode::InitAliasedLexical, hops, slot);
    }

    pub fn check_lexical(&mut self, local_no: u24) {
        self.emit_u24(Opcode::CheckLexical, local_no);
    }

    pub fn check_aliased_lexical(&mut self, hops: u8, slot: u24) {
        self.emit_aliased(Opcode::CheckAliasedLexical, hops, slot);
    }

    pub fn check_this(&mut self) {
        self.emit1(Opcode::CheckThis);
    }

    pub fn bind_g_name(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::BindGName, name);
    }

    pub fn bind_name(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::BindName, name);
    }

    pub fn get_name(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::GetName, name);
    }

    pub fn get_g_name(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::GetGName, name);
    }

    pub fn get_arg(&mut self, arg_no: u16) {
        self.emit_u16(Opcode::GetArg, arg_no);
    }

    pub fn get_local(&mut self, local_no: u24) {
        self.emit_u24(Opcode::GetLocal, local_no);
    }

    pub fn get_aliased_var(&mut self, hops: u8, slot: u24) {
        self.emit_aliased(Opcode::GetAliasedVar, hops, slot);
    }

    pub fn get_import(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::GetImport, name);
    }

    pub fn get_bound_name(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::GetBoundName, name);
    }

    pub fn get_intrinsic(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::GetIntrinsic, name);
    }

    pub fn callee(&mut self) {
        self.emit1(Opcode::Callee);
    }

    pub fn env_callee(&mut self, hops: u8) {
        self.emit_u8(Opcode::EnvCallee, hops);
    }

    pub fn set_name(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::SetName, name);
    }

    pub fn strict_set_name(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::StrictSetName, name);
    }

    pub fn set_g_name(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::SetGName, name);
    }

    pub fn strict_set_g_name(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::StrictSetGName, name);
    }

    pub fn set_arg(&mut self, arg_no: u16) {
        self.emit_u16(Opcode::SetArg, arg_no);
    }

    pub fn set_local(&mut self, local_no: u24) {
        self.emit_u24(Opcode::SetLocal, local_no);
    }

    pub fn set_aliased_var(&mut self, hops: u8, slot: u24) {
        self.emit_aliased(Opcode::SetAliasedVar, hops, slot);
    }

    pub fn set_intrinsic(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::SetIntrinsic, name);
    }

    pub fn push_lexical_env(&mut self, scope_index: u32) {
        self.emit_u32(Opcode::PushLexicalEnv, scope_index);
    }

    pub fn pop_lexical_env(&mut self) {
        self.emit1(Opcode::PopLexicalEnv);
    }

    pub fn debug_leave_lexical_env(&mut self) {
        self.emit1(Opcode::DebugLeaveLexicalEnv);
    }

    pub fn recreate_lexical_env(&mut self) {
        self.emit1(Opcode::RecreateLexicalEnv);
    }

    pub fn freshen_lexical_env(&mut self) {
        self.emit1(Opcode::FreshenLexicalEnv);
    }

    pub fn push_var_env(&mut self, scope_index: u32) {
        self.emit_u32(Opcode::PushVarEnv, scope_index);
    }

    pub fn pop_var_env(&mut self) {
        self.emit1(Opcode::PopVarEnv);
    }

    pub fn enter_with(&mut self, static_with_index: u32) {
        self.emit_u32(Opcode::EnterWith, static_with_index);
    }

    pub fn leave_with(&mut self) {
        self.emit1(Opcode::LeaveWith);
    }

    pub fn bind_var(&mut self) {
        self.emit1(Opcode::BindVar);
    }

    pub fn def_var(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::DefVar, name);
    }

    pub fn def_fun(&mut self) {
        self.emit1(Opcode::DefFun);
    }

    pub fn def_let(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::DefLet, name);
    }

    pub fn def_const(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::DefConst, name);
    }

    pub fn del_name(&mut self, name: AtomIndex) {
        self.emit_with_atom(Opcode::DelName, name);
    }

    pub fn arguments(&mut self) {
        self.emit1(Opcode::Arguments);
    }

    pub fn rest(&mut self) {
        self.emit1(Opcode::Rest);
    }

    pub fn function_this(&mut self) {
        self.emit1(Opcode::FunctionThis);
    }

    pub fn pop(&mut self) {
        self.emit1(Opcode::Pop);
    }

    pub fn pop_n(&mut self, n: u16) {
        self.emit_op_common(Opcode::PopN, n as usize);
        self.write_u16(n);
    }

    pub fn dup(&mut self) {
        self.emit1(Opcode::Dup);
    }

    pub fn dup2(&mut self) {
        self.emit1(Opcode::Dup2);
    }

    pub fn dup_at(&mut self, n: u32) {
        self.emit_u24(Opcode::DupAt, n);
    }

    pub fn swap(&mut self) {
        self.emit1(Opcode::Swap);
    }

    pub fn pick(&mut self, n: u8) {
        self.emit_u8(Opcode::Pick, n);
    }

    pub fn unpick(&mut self, n: u8) {
        self.emit_u8(Opcode::Unpick, n);
    }

    pub fn nop(&mut self) {
        self.emit1(Opcode::Nop);
    }

    pub fn lineno(&mut self, lineno: u32) {
        self.emit_u32(Opcode::Lineno, lineno);
    }

    pub fn nop_destructuring(&mut self) {
        self.emit1(Opcode::NopDestructuring);
    }

    pub fn force_interpreter(&mut self) {
        self.emit1(Opcode::ForceInterpreter);
    }

    pub fn debug_check_self_hosted(&mut self) {
        self.emit1(Opcode::DebugCheckSelfHosted);
    }

    pub fn instrumentation_active(&mut self) {
        self.emit1(Opcode::InstrumentationActive);
    }

    pub fn instrumentation_callback(&mut self) {
        self.emit1(Opcode::InstrumentationCallback);
    }

    pub fn instrumentation_script_id(&mut self) {
        self.emit1(Opcode::InstrumentationScriptId);
    }

    pub fn debugger(&mut self) {
        self.emit1(Opcode::Debugger);
    }
}
