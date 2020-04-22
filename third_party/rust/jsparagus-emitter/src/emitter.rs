//! Low-level bytecode emitter, used by ast_builder.
//!
//! This API makes it easy to emit correct individual bytecode instructions.

// Most of this functionality isn't used yet.
#![allow(dead_code)]

use crate::bytecode_offset::{BytecodeOffset, BytecodeOffsetDiff};
use crate::gcthings::{GCThingIndex, GCThingList};
use crate::opcode::Opcode;
use crate::regexp::{RegExpItem, RegExpList};
use crate::scope_notes::{ScopeNoteIndex, ScopeNoteList};
use crate::script_atom_set::{ScriptAtomSet, ScriptAtomSetIndex};
use crate::stencil::ScriptStencil;
use ast::source_atom_set::SourceAtomSetIndex;
use byteorder::{ByteOrder, LittleEndian};
use scope::data::ScopeIndex;
use scope::frame_slot::FrameSlot;
use std::cmp;
use std::convert::TryFrom;
use std::convert::TryInto;
use std::fmt;

// @@@@ BEGIN TYPES @@@@
pub enum AsyncFunctionResolveKind {
    Fulfill = 0,
    Reject = 1,
}

pub enum CheckIsCallableKind {
    IteratorReturn = 0,
}

pub enum CheckIsObjectKind {
    IteratorNext = 0,
    IteratorReturn = 1,
    IteratorThrow = 2,
    GetIterator = 3,
    GetAsyncIterator = 4,
}

pub enum FunctionPrefixKind {
    None = 0,
    Get = 1,
    Set = 2,
}

pub enum GeneratorResumeKind {
    Next = 0,
    Throw = 1,
    Return = 2,
}

pub enum ThrowMsgKind {
    AssignToCall = 0,
    IteratorNoThrow = 1,
    CantDeleteSuper = 2,
}

pub enum TryNoteKind {
    Catch = 0,
    Finally = 1,
    ForIn = 2,
    Destructuring = 3,
    ForOf = 4,
    ForOfIterClose = 5,
    Loop = 6,
}

pub enum SymbolCode {
    IsConcatSpreadable = 0,
    Iterator = 1,
    Match = 2,
    Replace = 3,
    Search = 4,
    Species = 5,
    HasInstance = 6,
    Split = 7,
    ToPrimitive = 8,
    ToStringTag = 9,
    Unscopables = 10,
    AsyncIterator = 11,
    MatchAll = 12,
}

pub enum SrcNoteType {
    Null = 0,
    AssignOp = 1,
    ColSpan = 2,
    NewLine = 3,
    SetLine = 4,
    Breakpoint = 5,
    StepSep = 6,
    Unused7 = 7,
    XDelta = 8,
}

// @@@@ END TYPES @@@@

#[allow(non_camel_case_types)]
pub type u24 = u32;

/// Low-level bytecode emitter.
pub struct InstructionWriter {
    bytecode: Vec<u8>,
    atoms: ScriptAtomSet,

    gcthings: GCThingList,
    scope_notes: ScopeNoteList,

    regexps: RegExpList,

    last_jump_target_offset: Option<BytecodeOffset>,

    main_offset: BytecodeOffset,

    /// The maximum number of fixed frame slots.
    max_fixed_slots: FrameSlot,

    /// Stack depth after the instructions emitted so far.
    stack_depth: usize,

    /// Maximum stack_depth at any point in the instructions emitted so far.
    maximum_stack_depth: usize,

    body_scope_index: Option<GCThingIndex>,

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
            atoms: ScriptAtomSet::new(),
            gcthings: GCThingList::new(),
            scope_notes: ScopeNoteList::new(),
            regexps: RegExpList::new(),
            last_jump_target_offset: None,
            main_offset: BytecodeOffset::from(0usize),
            max_fixed_slots: FrameSlot::new(0),
            stack_depth: 0,
            maximum_stack_depth: 0,
            body_scope_index: None,
            num_ic_entries: 0,
            num_type_sets: 0,
        }
    }

    fn write_i8(&mut self, value: i8) {
        self.write_u8(value as u8);
    }

    fn write_u8(&mut self, value: u8) {
        self.bytecode.push(value);
    }

    fn write_u16(&mut self, value: u16) {
        self.bytecode.extend_from_slice(&value.to_le_bytes());
    }

    fn write_u24(&mut self, value: u24) {
        let slice = value.to_le_bytes();
        assert!(slice.len() == 4 && slice[3] == 0);
        self.bytecode.extend_from_slice(&slice[0..3]);
    }

    fn write_i32(&mut self, value: i32) {
        self.bytecode.extend_from_slice(&value.to_le_bytes());
    }

    fn write_u32(&mut self, value: u32) {
        self.bytecode.extend_from_slice(&value.to_le_bytes());
    }

    fn write_g_c_thing_index(&mut self, value: GCThingIndex) {
        self.write_u32(usize::from(value) as u32);
    }

    fn write_script_atom_set_index(&mut self, atom_index: ScriptAtomSetIndex) {
        self.write_u32(atom_index.into());
    }

    fn write_offset(&mut self, offset: i32) {
        self.write_i32(offset);
    }

    fn write_bytecode_offset_diff(&mut self, offset: BytecodeOffsetDiff) {
        self.write_i32(i32::from(offset));
    }

    fn write_f64(&mut self, val: f64) {
        self.bytecode
            .extend_from_slice(&val.to_bits().to_le_bytes());
    }

    fn write_ic_index(&mut self) {
        self.write_u32(self.num_ic_entries.try_into().unwrap());
    }

    fn emit_op(&mut self, opcode: Opcode) {
        let nuses: isize = opcode.nuses();
        assert!(nuses >= 0);
        self.emit_op_common(opcode, nuses as usize);
    }

    fn emit_argc_op(&mut self, opcode: Opcode, argc: u16) {
        assert!(opcode.has_argc());
        assert_eq!(opcode.nuses(), -1);
        let nuses = match opcode {
            Opcode::Call
            | Opcode::CallIgnoresRv
            | Opcode::Eval
            | Opcode::CallIter
            | Opcode::StrictEval
            | Opcode::FunCall
            | Opcode::FunApply => {
                // callee, this, arguments...
                2 + (argc as usize)
            }

            Opcode::New | Opcode::SuperCall => {
                // callee, isConstructing, arguments..., newtarget
                2 + (argc as usize) + 1
            }

            _ => panic!("Unsupported opcode"),
        };
        self.emit_op_common(opcode, nuses);
    }

    fn emit_pop_n_op(&mut self, opcode: Opcode, n: u16) {
        assert_eq!(opcode.nuses(), -1);
        debug_assert_eq!(opcode, Opcode::PopN);
        self.emit_op_common(opcode, n as usize);
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

        if opcode.has_typeset() {
            self.num_type_sets += 1;
        }
    }

    fn set_last_jump_target_offset(&mut self, target: BytecodeOffset) {
        self.last_jump_target_offset = Some(target);
    }

    fn get_end_of_bytecode(&mut self, offset: BytecodeOffset) -> usize {
        // find the offset after the end of bytecode associated with this offset.
        let target_opcode = Opcode::try_from(self.bytecode[offset.offset]).unwrap();
        offset.offset + target_opcode.instruction_length()
    }

    pub fn get_atom_index(&mut self, value: SourceAtomSetIndex) -> ScriptAtomSetIndex {
        self.atoms.insert(value)
    }

    pub fn emit_jump_target_and_patch(&mut self, jumplist: Vec<BytecodeOffset>) {
        let mut target = self.bytecode_offset();
        let last_jump = self.last_jump_target_offset;
        match last_jump {
            Some(offset) => {
                if self.get_end_of_bytecode(offset) != target.offset {
                    self.jump_target();
                    self.set_last_jump_target_offset(target);
                } else {
                    target = offset;
                }
            }
            None => {
                self.jump_target();
                self.set_last_jump_target_offset(target);
            }
        }

        for jump in jumplist {
            self.patch_jump_to_target(target, jump);
        }
    }

    pub fn patch_jump_to_target(&mut self, target: BytecodeOffset, jump: BytecodeOffset) {
        let diff = target.diff_from(jump).into();
        let index = jump.offset + 1;
        // FIXME: Use native endian instead of little endian
        LittleEndian::write_i32(&mut self.bytecode[index..index + 4], diff);
    }

    pub fn bytecode_offset(&mut self) -> BytecodeOffset {
        BytecodeOffset::from(self.bytecode.len())
    }

    pub fn stack_depth(&self) -> usize {
        self.stack_depth
    }

    pub fn set_stack_depth(&mut self, depth: usize) {
        self.stack_depth = depth;
    }

    // Public methods to emit each instruction.

    pub fn emit_boolean(&mut self, value: bool) {
        self.emit_op(if value { Opcode::True } else { Opcode::False });
    }

    pub fn emit_unary_op(&mut self, opcode: Opcode) {
        assert!(opcode.is_simple_unary_operator());
        self.emit_op(opcode);
    }

    pub fn emit_binary_op(&mut self, opcode: Opcode) {
        assert!(opcode.is_simple_binary_operator());
        debug_assert_eq!(opcode.nuses(), 2);
        debug_assert_eq!(opcode.ndefs(), 1);
        self.emit_op(opcode);
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

    pub fn numeric(&mut self, value: f64) {
        if value.is_finite() && value.fract() == 0.0 {
            if i8::min_value() as f64 <= value && value <= i8::max_value() as f64 {
                match value as i8 {
                    0 => self.zero(),
                    1 => self.one(),
                    i => self.int8(i),
                }
                return;
            }
            if 0.0 <= value {
                if value <= u16::max_value() as f64 {
                    self.uint16(value as u16);
                    return;
                }
                if value <= 0x00ffffff as f64 {
                    self.uint24(value as u24);
                    return;
                }
            }
            if i32::min_value() as f64 <= value && value <= i32::max_value() as f64 {
                self.int32(value as i32);
                return;
            }
        }
        self.double_(value);
    }

    // WARNING
    // The following section is generated by
    // crates/emitter/scripts/update_opcodes.py.
    // Do mot modify manually.
    //
    // @@@@ BEGIN METHODS @@@@
    pub fn undefined(&mut self) {
        self.emit_op(Opcode::Undefined);
    }

    pub fn null(&mut self) {
        self.emit_op(Opcode::Null);
    }

    pub fn int32(&mut self, val: i32) {
        self.emit_op(Opcode::Int32);
        self.write_i32(val);
    }

    pub fn zero(&mut self) {
        self.emit_op(Opcode::Zero);
    }

    pub fn one(&mut self) {
        self.emit_op(Opcode::One);
    }

    pub fn int8(&mut self, val: i8) {
        self.emit_op(Opcode::Int8);
        self.write_i8(val);
    }

    pub fn uint16(&mut self, val: u16) {
        self.emit_op(Opcode::Uint16);
        self.write_u16(val);
    }

    pub fn uint24(&mut self, val: u24) {
        self.emit_op(Opcode::Uint24);
        self.write_u24(val);
    }

    pub fn double_(&mut self, val: f64) {
        self.emit_op(Opcode::Double);
        self.write_f64(val);
    }

    pub fn big_int(&mut self, big_int_index: u32) {
        self.emit_op(Opcode::BigInt);
        self.write_u32(big_int_index);
    }

    pub fn string(&mut self, atom_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::String);
        self.write_script_atom_set_index(atom_index);
    }

    pub fn symbol(&mut self, symbol: u8) {
        self.emit_op(Opcode::Symbol);
        self.write_u8(symbol);
    }

    pub fn typeof_(&mut self) {
        self.emit_op(Opcode::Typeof);
    }

    pub fn typeof_expr(&mut self) {
        self.emit_op(Opcode::TypeofExpr);
    }

    pub fn inc(&mut self) {
        self.emit_op(Opcode::Inc);
    }

    pub fn dec(&mut self) {
        self.emit_op(Opcode::Dec);
    }

    pub fn to_id(&mut self) {
        self.emit_op(Opcode::ToId);
    }

    pub fn to_numeric(&mut self) {
        self.emit_op(Opcode::ToNumeric);
    }

    pub fn to_string(&mut self) {
        self.emit_op(Opcode::ToString);
    }

    pub fn global_this(&mut self) {
        self.emit_op(Opcode::GlobalThis);
    }

    pub fn new_target(&mut self) {
        self.emit_op(Opcode::NewTarget);
    }

    pub fn dynamic_import(&mut self) {
        self.emit_op(Opcode::DynamicImport);
    }

    pub fn import_meta(&mut self) {
        self.emit_op(Opcode::ImportMeta);
    }

    pub fn new_init(&mut self) {
        self.emit_op(Opcode::NewInit);
    }

    pub fn new_object(&mut self, baseobj_index: u32) {
        self.emit_op(Opcode::NewObject);
        self.write_u32(baseobj_index);
    }

    pub fn new_object_with_group(&mut self, baseobj_index: u32) {
        self.emit_op(Opcode::NewObjectWithGroup);
        self.write_u32(baseobj_index);
    }

    pub fn object(&mut self, object_index: u32) {
        self.emit_op(Opcode::Object);
        self.write_u32(object_index);
    }

    pub fn obj_with_proto(&mut self) {
        self.emit_op(Opcode::ObjWithProto);
    }

    pub fn init_prop(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::InitProp);
        self.write_script_atom_set_index(name_index);
    }

    pub fn init_hidden_prop(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::InitHiddenProp);
        self.write_script_atom_set_index(name_index);
    }

    pub fn init_locked_prop(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::InitLockedProp);
        self.write_script_atom_set_index(name_index);
    }

    pub fn init_elem(&mut self) {
        self.emit_op(Opcode::InitElem);
    }

    pub fn init_hidden_elem(&mut self) {
        self.emit_op(Opcode::InitHiddenElem);
    }

    pub fn init_prop_getter(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::InitPropGetter);
        self.write_script_atom_set_index(name_index);
    }

    pub fn init_hidden_prop_getter(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::InitHiddenPropGetter);
        self.write_script_atom_set_index(name_index);
    }

    pub fn init_elem_getter(&mut self) {
        self.emit_op(Opcode::InitElemGetter);
    }

    pub fn init_hidden_elem_getter(&mut self) {
        self.emit_op(Opcode::InitHiddenElemGetter);
    }

    pub fn init_prop_setter(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::InitPropSetter);
        self.write_script_atom_set_index(name_index);
    }

    pub fn init_hidden_prop_setter(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::InitHiddenPropSetter);
        self.write_script_atom_set_index(name_index);
    }

    pub fn init_elem_setter(&mut self) {
        self.emit_op(Opcode::InitElemSetter);
    }

    pub fn init_hidden_elem_setter(&mut self) {
        self.emit_op(Opcode::InitHiddenElemSetter);
    }

    pub fn get_prop(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::GetProp);
        self.write_script_atom_set_index(name_index);
    }

    pub fn call_prop(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::CallProp);
        self.write_script_atom_set_index(name_index);
    }

    pub fn get_elem(&mut self) {
        self.emit_op(Opcode::GetElem);
    }

    pub fn call_elem(&mut self) {
        self.emit_op(Opcode::CallElem);
    }

    pub fn length(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::Length);
        self.write_script_atom_set_index(name_index);
    }

    pub fn set_prop(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::SetProp);
        self.write_script_atom_set_index(name_index);
    }

    pub fn strict_set_prop(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::StrictSetProp);
        self.write_script_atom_set_index(name_index);
    }

    pub fn set_elem(&mut self) {
        self.emit_op(Opcode::SetElem);
    }

    pub fn strict_set_elem(&mut self) {
        self.emit_op(Opcode::StrictSetElem);
    }

    pub fn del_prop(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::DelProp);
        self.write_script_atom_set_index(name_index);
    }

    pub fn strict_del_prop(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::StrictDelProp);
        self.write_script_atom_set_index(name_index);
    }

    pub fn del_elem(&mut self) {
        self.emit_op(Opcode::DelElem);
    }

    pub fn strict_del_elem(&mut self) {
        self.emit_op(Opcode::StrictDelElem);
    }

    pub fn has_own(&mut self) {
        self.emit_op(Opcode::HasOwn);
    }

    pub fn super_base(&mut self) {
        self.emit_op(Opcode::SuperBase);
    }

    pub fn get_prop_super(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::GetPropSuper);
        self.write_script_atom_set_index(name_index);
    }

    pub fn get_elem_super(&mut self) {
        self.emit_op(Opcode::GetElemSuper);
    }

    pub fn set_prop_super(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::SetPropSuper);
        self.write_script_atom_set_index(name_index);
    }

    pub fn strict_set_prop_super(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::StrictSetPropSuper);
        self.write_script_atom_set_index(name_index);
    }

    pub fn set_elem_super(&mut self) {
        self.emit_op(Opcode::SetElemSuper);
    }

    pub fn strict_set_elem_super(&mut self) {
        self.emit_op(Opcode::StrictSetElemSuper);
    }

    pub fn iter(&mut self) {
        self.emit_op(Opcode::Iter);
    }

    pub fn more_iter(&mut self) {
        self.emit_op(Opcode::MoreIter);
    }

    pub fn is_no_iter(&mut self) {
        self.emit_op(Opcode::IsNoIter);
    }

    pub fn iter_next(&mut self) {
        self.emit_op(Opcode::IterNext);
    }

    pub fn end_iter(&mut self) {
        self.emit_op(Opcode::EndIter);
    }

    pub fn check_is_obj(&mut self, kind: CheckIsObjectKind) {
        self.emit_op(Opcode::CheckIsObj);
        self.write_u8(kind as u8);
    }

    pub fn check_is_callable(&mut self, kind: CheckIsCallableKind) {
        self.emit_op(Opcode::CheckIsCallable);
        self.write_u8(kind as u8);
    }

    pub fn check_obj_coercible(&mut self) {
        self.emit_op(Opcode::CheckObjCoercible);
    }

    pub fn to_async_iter(&mut self) {
        self.emit_op(Opcode::ToAsyncIter);
    }

    pub fn mutate_proto(&mut self) {
        self.emit_op(Opcode::MutateProto);
    }

    pub fn new_array(&mut self, length: u32) {
        self.emit_op(Opcode::NewArray);
        self.write_u32(length);
    }

    pub fn init_elem_array(&mut self, index: u32) {
        self.emit_op(Opcode::InitElemArray);
        self.write_u32(index);
    }

    pub fn init_elem_inc(&mut self) {
        self.emit_op(Opcode::InitElemInc);
    }

    pub fn hole(&mut self) {
        self.emit_op(Opcode::Hole);
    }

    pub fn new_array_copy_on_write(&mut self, object_index: u32) {
        self.emit_op(Opcode::NewArrayCopyOnWrite);
        self.write_u32(object_index);
    }

    pub fn reg_exp(&mut self, regexp_index: GCThingIndex) {
        self.emit_op(Opcode::RegExp);
        self.write_g_c_thing_index(regexp_index);
    }

    pub fn lambda(&mut self, func_index: u32) {
        self.emit_op(Opcode::Lambda);
        self.write_u32(func_index);
    }

    pub fn lambda_arrow(&mut self, func_index: u32) {
        self.emit_op(Opcode::LambdaArrow);
        self.write_u32(func_index);
    }

    pub fn set_fun_name(&mut self, prefix_kind: FunctionPrefixKind) {
        self.emit_op(Opcode::SetFunName);
        self.write_u8(prefix_kind as u8);
    }

    pub fn init_home_object(&mut self) {
        self.emit_op(Opcode::InitHomeObject);
    }

    pub fn check_class_heritage(&mut self) {
        self.emit_op(Opcode::CheckClassHeritage);
    }

    pub fn fun_with_proto(&mut self, func_index: u32) {
        self.emit_op(Opcode::FunWithProto);
        self.write_u32(func_index);
    }

    pub fn class_constructor(&mut self, name_index: u32, source_start: u32, source_end: u32) {
        self.emit_op(Opcode::ClassConstructor);
        self.write_u32(name_index);
        self.write_u32(source_start);
        self.write_u32(source_end);
    }

    pub fn derived_constructor(&mut self, name_index: u32, source_start: u32, source_end: u32) {
        self.emit_op(Opcode::DerivedConstructor);
        self.write_u32(name_index);
        self.write_u32(source_start);
        self.write_u32(source_end);
    }

    pub fn function_proto(&mut self) {
        self.emit_op(Opcode::FunctionProto);
    }

    pub fn call(&mut self, argc: u16) {
        self.emit_argc_op(Opcode::Call, argc);
        self.write_u16(argc);
    }

    pub fn call_iter(&mut self, argc: u16) {
        self.emit_argc_op(Opcode::CallIter, argc);
        self.write_u16(argc);
    }

    pub fn fun_apply(&mut self, argc: u16) {
        self.emit_argc_op(Opcode::FunApply, argc);
        self.write_u16(argc);
    }

    pub fn fun_call(&mut self, argc: u16) {
        self.emit_argc_op(Opcode::FunCall, argc);
        self.write_u16(argc);
    }

    pub fn call_ignores_rv(&mut self, argc: u16) {
        self.emit_argc_op(Opcode::CallIgnoresRv, argc);
        self.write_u16(argc);
    }

    pub fn spread_call(&mut self) {
        self.emit_op(Opcode::SpreadCall);
    }

    pub fn optimize_spread_call(&mut self) {
        self.emit_op(Opcode::OptimizeSpreadCall);
    }

    pub fn eval(&mut self, argc: u16) {
        self.emit_argc_op(Opcode::Eval, argc);
        self.write_u16(argc);
    }

    pub fn spread_eval(&mut self) {
        self.emit_op(Opcode::SpreadEval);
    }

    pub fn strict_eval(&mut self, argc: u16) {
        self.emit_argc_op(Opcode::StrictEval, argc);
        self.write_u16(argc);
    }

    pub fn strict_spread_eval(&mut self) {
        self.emit_op(Opcode::StrictSpreadEval);
    }

    pub fn implicit_this(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::ImplicitThis);
        self.write_script_atom_set_index(name_index);
    }

    pub fn g_implicit_this(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::GImplicitThis);
        self.write_script_atom_set_index(name_index);
    }

    pub fn call_site_obj(&mut self, object_index: u32) {
        self.emit_op(Opcode::CallSiteObj);
        self.write_u32(object_index);
    }

    pub fn is_constructing(&mut self) {
        self.emit_op(Opcode::IsConstructing);
    }

    pub fn new_(&mut self, argc: u16) {
        self.emit_argc_op(Opcode::New, argc);
        self.write_u16(argc);
    }

    pub fn super_call(&mut self, argc: u16) {
        self.emit_argc_op(Opcode::SuperCall, argc);
        self.write_u16(argc);
    }

    pub fn spread_new(&mut self) {
        self.emit_op(Opcode::SpreadNew);
    }

    pub fn spread_super_call(&mut self) {
        self.emit_op(Opcode::SpreadSuperCall);
    }

    pub fn super_fun(&mut self) {
        self.emit_op(Opcode::SuperFun);
    }

    pub fn check_this_reinit(&mut self) {
        self.emit_op(Opcode::CheckThisReinit);
    }

    pub fn generator(&mut self) {
        self.emit_op(Opcode::Generator);
    }

    pub fn initial_yield(&mut self, resume_index: u24) {
        self.emit_op(Opcode::InitialYield);
        self.write_u24(resume_index);
    }

    pub fn after_yield(&mut self) {
        self.emit_op(Opcode::AfterYield);
        self.write_ic_index();
    }

    pub fn final_yield_rval(&mut self) {
        self.emit_op(Opcode::FinalYieldRval);
    }

    pub fn yield_(&mut self, resume_index: u24) {
        self.emit_op(Opcode::Yield);
        self.write_u24(resume_index);
    }

    pub fn is_gen_closing(&mut self) {
        self.emit_op(Opcode::IsGenClosing);
    }

    pub fn async_await(&mut self) {
        self.emit_op(Opcode::AsyncAwait);
    }

    pub fn async_resolve(&mut self, fulfill_or_reject: AsyncFunctionResolveKind) {
        self.emit_op(Opcode::AsyncResolve);
        self.write_u8(fulfill_or_reject as u8);
    }

    pub fn await_(&mut self, resume_index: u24) {
        self.emit_op(Opcode::Await);
        self.write_u24(resume_index);
    }

    pub fn try_skip_await(&mut self) {
        self.emit_op(Opcode::TrySkipAwait);
    }

    pub fn resume_kind(&mut self, resume_kind: GeneratorResumeKind) {
        self.emit_op(Opcode::ResumeKind);
        self.write_u8(resume_kind as u8);
    }

    pub fn check_resume_kind(&mut self) {
        self.emit_op(Opcode::CheckResumeKind);
    }

    pub fn resume(&mut self) {
        self.emit_op(Opcode::Resume);
    }

    pub fn jump_target(&mut self) {
        self.emit_op(Opcode::JumpTarget);
        self.write_ic_index();
    }

    pub fn loop_head(&mut self, depth_hint: u8) {
        self.emit_op(Opcode::LoopHead);
        self.write_ic_index();
        self.write_u8(depth_hint);
    }

    pub fn goto_(&mut self, offset: BytecodeOffsetDiff) {
        self.emit_op(Opcode::Goto);
        self.write_bytecode_offset_diff(offset);
    }

    pub fn if_eq(&mut self, forward_offset: BytecodeOffsetDiff) {
        self.emit_op(Opcode::IfEq);
        self.write_bytecode_offset_diff(forward_offset);
    }

    pub fn if_ne(&mut self, offset: BytecodeOffsetDiff) {
        self.emit_op(Opcode::IfNe);
        self.write_bytecode_offset_diff(offset);
    }

    pub fn and_(&mut self, forward_offset: BytecodeOffsetDiff) {
        self.emit_op(Opcode::And);
        self.write_bytecode_offset_diff(forward_offset);
    }

    pub fn or_(&mut self, forward_offset: BytecodeOffsetDiff) {
        self.emit_op(Opcode::Or);
        self.write_bytecode_offset_diff(forward_offset);
    }

    pub fn coalesce(&mut self, forward_offset: BytecodeOffsetDiff) {
        self.emit_op(Opcode::Coalesce);
        self.write_bytecode_offset_diff(forward_offset);
    }

    pub fn case_(&mut self, forward_offset: BytecodeOffsetDiff) {
        self.emit_op(Opcode::Case);
        self.write_bytecode_offset_diff(forward_offset);
    }

    pub fn default_(&mut self, forward_offset: BytecodeOffsetDiff) {
        self.emit_op(Opcode::Default);
        self.write_bytecode_offset_diff(forward_offset);
    }

    pub fn return_(&mut self) {
        self.emit_op(Opcode::Return);
    }

    pub fn get_rval(&mut self) {
        self.emit_op(Opcode::GetRval);
    }

    pub fn set_rval(&mut self) {
        self.emit_op(Opcode::SetRval);
    }

    pub fn ret_rval(&mut self) {
        self.emit_op(Opcode::RetRval);
    }

    pub fn check_return(&mut self) {
        self.emit_op(Opcode::CheckReturn);
    }

    pub fn throw_(&mut self) {
        self.emit_op(Opcode::Throw);
    }

    pub fn throw_msg(&mut self, msg_number: ThrowMsgKind) {
        self.emit_op(Opcode::ThrowMsg);
        self.write_u8(msg_number as u8);
    }

    pub fn throw_set_const(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::ThrowSetConst);
        self.write_script_atom_set_index(name_index);
    }

    pub fn try_(&mut self, jump_at_end_offset: i32) {
        self.emit_op(Opcode::Try);
        self.write_i32(jump_at_end_offset);
    }

    pub fn try_destructuring(&mut self) {
        self.emit_op(Opcode::TryDestructuring);
    }

    pub fn exception(&mut self) {
        self.emit_op(Opcode::Exception);
    }

    pub fn resume_index(&mut self, resume_index: u24) {
        self.emit_op(Opcode::ResumeIndex);
        self.write_u24(resume_index);
    }

    pub fn gosub(&mut self, forward_offset: BytecodeOffsetDiff) {
        self.emit_op(Opcode::Gosub);
        self.write_bytecode_offset_diff(forward_offset);
    }

    pub fn finally(&mut self) {
        self.emit_op(Opcode::Finally);
    }

    pub fn retsub(&mut self) {
        self.emit_op(Opcode::Retsub);
    }

    pub fn uninitialized(&mut self) {
        self.emit_op(Opcode::Uninitialized);
    }

    pub fn init_lexical(&mut self, localno: u24) {
        self.emit_op(Opcode::InitLexical);
        self.write_u24(localno);
    }

    pub fn init_g_lexical(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::InitGLexical);
        self.write_script_atom_set_index(name_index);
    }

    pub fn init_aliased_lexical(&mut self, hops: u8, slot: u24) {
        self.emit_op(Opcode::InitAliasedLexical);
        self.write_u8(hops);
        self.write_u24(slot);
    }

    pub fn check_lexical(&mut self, localno: u24) {
        self.emit_op(Opcode::CheckLexical);
        self.write_u24(localno);
    }

    pub fn check_aliased_lexical(&mut self, hops: u8, slot: u24) {
        self.emit_op(Opcode::CheckAliasedLexical);
        self.write_u8(hops);
        self.write_u24(slot);
    }

    pub fn check_this(&mut self) {
        self.emit_op(Opcode::CheckThis);
    }

    pub fn bind_g_name(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::BindGName);
        self.write_script_atom_set_index(name_index);
    }

    pub fn bind_name(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::BindName);
        self.write_script_atom_set_index(name_index);
    }

    pub fn get_name(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::GetName);
        self.write_script_atom_set_index(name_index);
    }

    pub fn get_g_name(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::GetGName);
        self.write_script_atom_set_index(name_index);
    }

    pub fn get_arg(&mut self, argno: u16) {
        self.emit_op(Opcode::GetArg);
        self.write_u16(argno);
    }

    pub fn get_local(&mut self, localno: u24) {
        self.emit_op(Opcode::GetLocal);
        self.write_u24(localno);
    }

    pub fn get_aliased_var(&mut self, hops: u8, slot: u24) {
        self.emit_op(Opcode::GetAliasedVar);
        self.write_u8(hops);
        self.write_u24(slot);
    }

    pub fn get_import(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::GetImport);
        self.write_script_atom_set_index(name_index);
    }

    pub fn get_bound_name(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::GetBoundName);
        self.write_script_atom_set_index(name_index);
    }

    pub fn get_intrinsic(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::GetIntrinsic);
        self.write_script_atom_set_index(name_index);
    }

    pub fn callee(&mut self) {
        self.emit_op(Opcode::Callee);
    }

    pub fn env_callee(&mut self, num_hops: u8) {
        self.emit_op(Opcode::EnvCallee);
        self.write_u8(num_hops);
    }

    pub fn set_name(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::SetName);
        self.write_script_atom_set_index(name_index);
    }

    pub fn strict_set_name(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::StrictSetName);
        self.write_script_atom_set_index(name_index);
    }

    pub fn set_g_name(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::SetGName);
        self.write_script_atom_set_index(name_index);
    }

    pub fn strict_set_g_name(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::StrictSetGName);
        self.write_script_atom_set_index(name_index);
    }

    pub fn set_arg(&mut self, argno: u16) {
        self.emit_op(Opcode::SetArg);
        self.write_u16(argno);
    }

    pub fn set_local(&mut self, localno: u24) {
        self.emit_op(Opcode::SetLocal);
        self.write_u24(localno);
    }

    pub fn set_aliased_var(&mut self, hops: u8, slot: u24) {
        self.emit_op(Opcode::SetAliasedVar);
        self.write_u8(hops);
        self.write_u24(slot);
    }

    pub fn set_intrinsic(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::SetIntrinsic);
        self.write_script_atom_set_index(name_index);
    }

    pub fn push_lexical_env(&mut self, lexical_scope_index: u32) {
        self.emit_op(Opcode::PushLexicalEnv);
        self.write_u32(lexical_scope_index);
    }

    pub fn pop_lexical_env(&mut self) {
        self.emit_op(Opcode::PopLexicalEnv);
    }

    pub fn debug_leave_lexical_env(&mut self) {
        self.emit_op(Opcode::DebugLeaveLexicalEnv);
    }

    pub fn recreate_lexical_env(&mut self) {
        self.emit_op(Opcode::RecreateLexicalEnv);
    }

    pub fn freshen_lexical_env(&mut self) {
        self.emit_op(Opcode::FreshenLexicalEnv);
    }

    pub fn push_var_env(&mut self, scope_index: u32) {
        self.emit_op(Opcode::PushVarEnv);
        self.write_u32(scope_index);
    }

    pub fn enter_with(&mut self, static_with_index: u32) {
        self.emit_op(Opcode::EnterWith);
        self.write_u32(static_with_index);
    }

    pub fn leave_with(&mut self) {
        self.emit_op(Opcode::LeaveWith);
    }

    pub fn bind_var(&mut self) {
        self.emit_op(Opcode::BindVar);
    }

    pub fn def_var(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::DefVar);
        self.write_script_atom_set_index(name_index);
    }

    pub fn def_fun(&mut self) {
        self.emit_op(Opcode::DefFun);
    }

    pub fn def_let(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::DefLet);
        self.write_script_atom_set_index(name_index);
    }

    pub fn def_const(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::DefConst);
        self.write_script_atom_set_index(name_index);
    }

    pub fn check_global_or_eval_decl(&mut self) {
        self.emit_op(Opcode::CheckGlobalOrEvalDecl);
    }

    pub fn del_name(&mut self, name_index: ScriptAtomSetIndex) {
        self.emit_op(Opcode::DelName);
        self.write_script_atom_set_index(name_index);
    }

    pub fn arguments(&mut self) {
        self.emit_op(Opcode::Arguments);
    }

    pub fn rest(&mut self) {
        self.emit_op(Opcode::Rest);
    }

    pub fn function_this(&mut self) {
        self.emit_op(Opcode::FunctionThis);
    }

    pub fn pop(&mut self) {
        self.emit_op(Opcode::Pop);
    }

    pub fn pop_n(&mut self, n: u16) {
        self.emit_pop_n_op(Opcode::PopN, n);
        self.write_u16(n);
    }

    pub fn dup(&mut self) {
        self.emit_op(Opcode::Dup);
    }

    pub fn dup2(&mut self) {
        self.emit_op(Opcode::Dup2);
    }

    pub fn dup_at(&mut self, n: u24) {
        self.emit_op(Opcode::DupAt);
        self.write_u24(n);
    }

    pub fn swap(&mut self) {
        self.emit_op(Opcode::Swap);
    }

    pub fn pick(&mut self, n: u8) {
        self.emit_op(Opcode::Pick);
        self.write_u8(n);
    }

    pub fn unpick(&mut self, n: u8) {
        self.emit_op(Opcode::Unpick);
        self.write_u8(n);
    }

    pub fn nop(&mut self) {
        self.emit_op(Opcode::Nop);
    }

    pub fn lineno(&mut self, lineno: u32) {
        self.emit_op(Opcode::Lineno);
        self.write_u32(lineno);
    }

    pub fn nop_destructuring(&mut self) {
        self.emit_op(Opcode::NopDestructuring);
    }

    pub fn force_interpreter(&mut self) {
        self.emit_op(Opcode::ForceInterpreter);
    }

    pub fn debug_check_self_hosted(&mut self) {
        self.emit_op(Opcode::DebugCheckSelfHosted);
    }

    pub fn instrumentation_active(&mut self) {
        self.emit_op(Opcode::InstrumentationActive);
    }

    pub fn instrumentation_callback(&mut self) {
        self.emit_op(Opcode::InstrumentationCallback);
    }

    pub fn instrumentation_script_id(&mut self) {
        self.emit_op(Opcode::InstrumentationScriptId);
    }

    pub fn debugger(&mut self) {
        self.emit_op(Opcode::Debugger);
    }

    // @@@@ END METHODS @@@@

    pub fn get_regexp_gcthing_index(&mut self, regexp: RegExpItem) -> GCThingIndex {
        let regexp_index = self.regexps.append(regexp);
        self.gcthings.append_regexp(regexp_index)
    }

    fn update_max_frame_slots(&mut self, max_frame_slots: FrameSlot) {
        self.max_fixed_slots = cmp::max(self.max_fixed_slots, max_frame_slots);
    }

    pub fn enter_global_scope(&mut self, scope_index: ScopeIndex) {
        let index = self.gcthings.append_scope(scope_index);
        self.body_scope_index = Some(index);
    }

    pub fn leave_global_scope(&self) {}

    pub fn enter_lexical_scope(
        &mut self,
        scope_index: ScopeIndex,
        parent_scope_note_index: Option<ScopeNoteIndex>,
        next_frame_slot: FrameSlot,
    ) -> ScopeNoteIndex {
        self.update_max_frame_slots(next_frame_slot);

        let gcthing_index = self.gcthings.append_scope(scope_index);
        let offset = self.bytecode_offset();
        let note_index =
            self.scope_notes
                .enter_scope(gcthing_index, offset, parent_scope_note_index);
        note_index
    }

    pub fn leave_lexical_scope(&mut self, index: ScopeNoteIndex) {
        self.debug_leave_lexical_env();
        let offset = self.bytecode_offset();
        self.scope_notes.leave_scope(index, offset);
    }

    pub fn switch_to_main(&mut self) {
        self.main_offset = self.bytecode_offset();
    }
}

impl From<InstructionWriter> for ScriptStencil {
    fn from(emit: InstructionWriter) -> Self {
        Self {
            bytecode: emit.bytecode,
            atoms: emit.atoms.into(),
            regexps: emit.regexps.into(),
            gcthings: emit.gcthings.into(),
            scope_notes: emit.scope_notes.into(),

            lineno: 1,
            column: 0,

            main_offset: emit.main_offset.into(),
            max_fixed_slots: emit.max_fixed_slots,

            // These values probably can't be out of range for u32, as we would
            // have hit other limits first. Release-assert anyway.
            maximum_stack_depth: emit.maximum_stack_depth.try_into().unwrap(),
            body_scope_index: usize::from(emit.body_scope_index.expect("body scope should be set"))
                .try_into()
                .unwrap(),
            num_ic_entries: emit.num_ic_entries.try_into().unwrap(),
            num_type_sets: emit.num_type_sets.try_into().unwrap(),

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
}
