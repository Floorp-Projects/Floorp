/* Copyright 2017 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

use std::boxed::Box;
use std::cmp::min;
use std::collections::HashSet;
use std::result;
use std::str;
use std::vec::Vec;

use limits::{
    MAX_WASM_FUNCTIONS, MAX_WASM_GLOBALS, MAX_WASM_MEMORIES, MAX_WASM_MEMORY_PAGES,
    MAX_WASM_TABLES, MAX_WASM_TYPES,
};

use binary_reader::BinaryReader;

use primitives::{
    BinaryReaderError, ExternalKind, FuncType, GlobalType, ImportSectionEntryType, MemoryImmediate,
    MemoryType, Operator, ResizableLimits, Result, SIMDLineIndex, SectionCode, TableType, Type,
};

use parser::{Parser, ParserInput, ParserState, WasmDecoder};

type ValidatorResult<'a, T> = result::Result<T, ParserState<'a>>;

/// Test if `subtype` is a subtype of `supertype`.
fn is_subtype_supertype(subtype: Type, supertype: Type) -> bool {
    match supertype {
        Type::AnyRef => subtype == Type::AnyRef || subtype == Type::AnyFunc,
        _ => subtype == supertype,
    }
}

struct BlockState {
    return_types: Vec<Type>,
    stack_starts_at: usize,
    jump_to_top: bool,
    is_else_allowed: bool,
    is_dead_code: bool,
    polymorphic_values: Option<usize>,
}

impl BlockState {
    fn is_stack_polymorphic(&self) -> bool {
        self.polymorphic_values.is_some()
    }
}

struct FuncState {
    local_types: Vec<Type>,
    blocks: Vec<BlockState>,
    stack_types: Vec<Type>,
    end_function: bool,
}

impl FuncState {
    fn block_at(&self, depth: usize) -> &BlockState {
        assert!(depth < self.blocks.len());
        &self.blocks[self.blocks.len() - 1 - depth]
    }
    fn last_block(&self) -> &BlockState {
        self.blocks.last().unwrap()
    }
    fn assert_stack_type_at(&self, index: usize, expected: Type) -> bool {
        let stack_starts_at = self.last_block().stack_starts_at;
        if self.last_block().is_stack_polymorphic()
            && stack_starts_at + index >= self.stack_types.len()
        {
            return true;
        }
        assert!(stack_starts_at + index < self.stack_types.len());
        is_subtype_supertype(
            self.stack_types[self.stack_types.len() - 1 - index],
            expected,
        )
    }
    fn assert_block_stack_len(&self, depth: usize, minimal_len: usize) -> bool {
        assert!(depth < self.blocks.len());
        let blocks_end = self.blocks.len();
        let block_offset = blocks_end - 1 - depth;
        for i in block_offset..blocks_end {
            if self.blocks[i].is_stack_polymorphic() {
                return true;
            }
        }
        let block_starts_at = self.blocks[block_offset].stack_starts_at;
        self.stack_types.len() >= block_starts_at + minimal_len
    }
    fn assert_last_block_stack_len_exact(&self, len: usize) -> bool {
        let block_starts_at = self.last_block().stack_starts_at;
        if self.last_block().is_stack_polymorphic() {
            let polymorphic_values = self.last_block().polymorphic_values.unwrap();
            self.stack_types.len() + polymorphic_values <= block_starts_at + len
        } else {
            self.stack_types.len() == block_starts_at + len
        }
    }
}

struct InitExpressionState {
    ty: Type,
    global_count: usize,
    validated: bool,
}

#[derive(Copy, Clone, PartialOrd, Ord, PartialEq, Eq)]
enum SectionOrderState {
    Initial,
    Type,
    Import,
    Function,
    Table,
    Memory,
    Global,
    Export,
    Start,
    Element,
    DataCount,
    Code,
    Data,
}

impl SectionOrderState {
    pub fn from_section_code(code: &SectionCode) -> Option<SectionOrderState> {
        match *code {
            SectionCode::Type => Some(SectionOrderState::Type),
            SectionCode::Import => Some(SectionOrderState::Import),
            SectionCode::Function => Some(SectionOrderState::Function),
            SectionCode::Table => Some(SectionOrderState::Table),
            SectionCode::Memory => Some(SectionOrderState::Memory),
            SectionCode::Global => Some(SectionOrderState::Global),
            SectionCode::Export => Some(SectionOrderState::Export),
            SectionCode::Start => Some(SectionOrderState::Start),
            SectionCode::Element => Some(SectionOrderState::Element),
            SectionCode::Code => Some(SectionOrderState::Code),
            SectionCode::Data => Some(SectionOrderState::Data),
            SectionCode::DataCount => Some(SectionOrderState::DataCount),
            _ => None,
        }
    }
}

#[derive(Copy, Clone, PartialEq, Eq)]
enum BlockType {
    Block,
    Loop,
    If,
}

enum OperatorAction {
    None,
    ChangeFrame(usize),
    ChangeFrameWithType(usize, Type),
    ChangeFrameWithTypes(usize, Box<[Type]>),
    ChangeFrameToExactTypes(Vec<Type>),
    ChangeFrameAfterSelect(Option<Type>),
    PushBlock(Type, BlockType),
    PopBlock,
    ResetBlock,
    DeadCode,
    EndFunction,
}

impl OperatorAction {
    fn remove_frame_stack_types(
        func_state: &mut FuncState,
        remove_count: usize,
    ) -> OperatorValidatorResult<()> {
        if remove_count == 0 {
            return Ok(());
        }
        let last_block = func_state.blocks.last_mut().unwrap();
        if last_block.is_stack_polymorphic() {
            let len = func_state.stack_types.len();
            let remove_non_polymorphic = len
                .checked_sub(last_block.stack_starts_at)
                .ok_or("invalid block signature")?
                .min(remove_count);
            func_state
                .stack_types
                .truncate(len - remove_non_polymorphic);
            let polymorphic_values = last_block.polymorphic_values.unwrap();
            let remove_polymorphic = min(remove_count - remove_non_polymorphic, polymorphic_values);
            last_block.polymorphic_values = Some(polymorphic_values - remove_polymorphic);
        } else {
            assert!(func_state.stack_types.len() >= last_block.stack_starts_at + remove_count);
            let keep = func_state.stack_types.len() - remove_count;
            func_state.stack_types.truncate(keep);
        }
        Ok(())
    }
    fn update(&self, func_state: &mut FuncState) -> OperatorValidatorResult<()> {
        match *self {
            OperatorAction::None => (),
            OperatorAction::PushBlock(ty, block_type) => {
                let return_types = match ty {
                    Type::EmptyBlockType => Vec::with_capacity(0),
                    _ => vec![ty],
                };
                if block_type == BlockType::If {
                    func_state.stack_types.pop();
                }
                let stack_starts_at = func_state.stack_types.len();
                func_state.blocks.push(BlockState {
                    return_types,
                    stack_starts_at,
                    jump_to_top: block_type == BlockType::Loop,
                    is_else_allowed: block_type == BlockType::If,
                    is_dead_code: false,
                    polymorphic_values: None,
                });
            }
            OperatorAction::PopBlock => {
                assert!(func_state.blocks.len() > 1);
                let last_block = func_state.blocks.pop().unwrap();
                if last_block.is_stack_polymorphic() {
                    assert!(
                        func_state.stack_types.len()
                            <= last_block.return_types.len() + last_block.stack_starts_at
                    );
                } else {
                    assert!(
                        func_state.stack_types.len()
                            == last_block.return_types.len() + last_block.stack_starts_at
                    );
                }
                let keep = last_block.stack_starts_at;
                func_state.stack_types.truncate(keep);
                func_state
                    .stack_types
                    .extend_from_slice(&last_block.return_types);
            }
            OperatorAction::ResetBlock => {
                assert!(func_state.last_block().is_else_allowed);
                let last_block = func_state.blocks.last_mut().unwrap();
                let keep = last_block.stack_starts_at;
                func_state.stack_types.truncate(keep);
                last_block.is_else_allowed = false;
                last_block.polymorphic_values = None;
            }
            OperatorAction::ChangeFrame(remove_count) => {
                OperatorAction::remove_frame_stack_types(func_state, remove_count)?
            }
            OperatorAction::ChangeFrameWithType(remove_count, ty) => {
                OperatorAction::remove_frame_stack_types(func_state, remove_count)?;
                func_state.stack_types.push(ty);
            }
            OperatorAction::ChangeFrameWithTypes(remove_count, ref new_items) => {
                OperatorAction::remove_frame_stack_types(func_state, remove_count)?;
                if new_items.is_empty() {
                    return Ok(());
                }
                func_state.stack_types.extend_from_slice(new_items);
            }
            OperatorAction::ChangeFrameToExactTypes(ref items) => {
                let last_block = func_state.blocks.last_mut().unwrap();
                let keep = last_block.stack_starts_at;
                func_state.stack_types.truncate(keep);
                func_state.stack_types.extend_from_slice(items);
                last_block.polymorphic_values = None;
            }
            OperatorAction::ChangeFrameAfterSelect(ty) => {
                OperatorAction::remove_frame_stack_types(func_state, 3)?;
                if ty.is_none() {
                    let last_block = func_state.blocks.last_mut().unwrap();
                    assert!(last_block.is_stack_polymorphic());
                    last_block.polymorphic_values =
                        Some(last_block.polymorphic_values.unwrap() + 1);
                    return Ok(());
                }
                func_state.stack_types.push(ty.unwrap());
            }
            OperatorAction::DeadCode => {
                let last_block = func_state.blocks.last_mut().unwrap();
                let keep = last_block.stack_starts_at;
                func_state.stack_types.truncate(keep);
                last_block.is_dead_code = true;
                last_block.polymorphic_values = Some(0);
            }
            OperatorAction::EndFunction => {
                func_state.end_function = true;
            }
        }
        Ok(())
    }
}

pub trait WasmModuleResources {
    fn types(&self) -> &[FuncType];
    fn tables(&self) -> &[TableType];
    fn memories(&self) -> &[MemoryType];
    fn globals(&self) -> &[GlobalType];
    fn func_type_indices(&self) -> &[u32];
    fn element_count(&self) -> u32;
    fn data_count(&self) -> u32;
}

type OperatorValidatorResult<T> = result::Result<T, &'static str>;

#[derive(Copy, Clone)]
pub struct OperatorValidatorConfig {
    pub enable_threads: bool,
    pub enable_reference_types: bool,
    pub enable_simd: bool,
    pub enable_bulk_memory: bool,
}

const DEFAULT_OPERATOR_VALIDATOR_CONFIG: OperatorValidatorConfig = OperatorValidatorConfig {
    enable_threads: false,
    enable_reference_types: false,
    enable_simd: false,
    enable_bulk_memory: false,
};

struct OperatorValidator {
    func_state: FuncState,
    config: OperatorValidatorConfig,
}

impl OperatorValidator {
    pub fn new(
        func_type: &FuncType,
        locals: &[(u32, Type)],
        config: OperatorValidatorConfig,
    ) -> OperatorValidator {
        let mut local_types = Vec::new();
        local_types.extend_from_slice(&*func_type.params);
        for local in locals {
            for _ in 0..local.0 {
                local_types.push(local.1);
            }
        }

        let mut blocks = Vec::new();
        let mut last_returns = Vec::new();
        last_returns.extend_from_slice(&*func_type.returns);
        blocks.push(BlockState {
            return_types: last_returns,
            stack_starts_at: 0,
            jump_to_top: false,
            is_else_allowed: false,
            is_dead_code: false,
            polymorphic_values: None,
        });

        OperatorValidator {
            func_state: FuncState {
                local_types,
                blocks,
                stack_types: Vec::new(),
                end_function: false,
            },
            config,
        }
    }

    pub fn is_dead_code(&self) -> bool {
        self.func_state.last_block().is_dead_code
    }

    fn check_frame_size(
        &self,
        func_state: &FuncState,
        require_count: usize,
    ) -> OperatorValidatorResult<()> {
        if !func_state.assert_block_stack_len(0, require_count) {
            Err("not enough operands")
        } else {
            Ok(())
        }
    }

    fn check_operands_1(
        &self,
        func_state: &FuncState,
        operand: Type,
    ) -> OperatorValidatorResult<()> {
        self.check_frame_size(func_state, 1)?;
        if !func_state.assert_stack_type_at(0, operand) {
            return Err("stack operand type mismatch");
        }
        Ok(())
    }

    fn check_operands_2(
        &self,
        func_state: &FuncState,
        operand1: Type,
        operand2: Type,
    ) -> OperatorValidatorResult<()> {
        self.check_frame_size(func_state, 2)?;
        if !func_state.assert_stack_type_at(1, operand1) {
            return Err("stack operand type mismatch");
        }
        if !func_state.assert_stack_type_at(0, operand2) {
            return Err("stack operand type mismatch");
        }
        Ok(())
    }

    fn check_operands(
        &self,
        func_state: &FuncState,
        expected_types: &[Type],
    ) -> OperatorValidatorResult<()> {
        let len = expected_types.len();
        self.check_frame_size(func_state, len)?;
        for i in 0..len {
            if !func_state.assert_stack_type_at(len - 1 - i, expected_types[i]) {
                return Err("stack operand type mismatch");
            }
        }
        Ok(())
    }

    fn check_block_return_types(
        &self,
        func_state: &FuncState,
        block: &BlockState,
        reserve_items: usize,
    ) -> OperatorValidatorResult<()> {
        let len = block.return_types.len();
        for i in 0..len {
            if !func_state.assert_stack_type_at(len - 1 - i + reserve_items, block.return_types[i])
            {
                return Err("stack item type does not match block item type");
            }
        }
        Ok(())
    }

    fn check_block_return(&self, func_state: &FuncState) -> OperatorValidatorResult<()> {
        let len = func_state.last_block().return_types.len();
        if !func_state.assert_last_block_stack_len_exact(len) {
            return Err("stack size does not match block type");
        }
        self.check_block_return_types(func_state, func_state.last_block(), 0)
    }

    fn check_jump_from_block(
        &self,
        func_state: &FuncState,
        relative_depth: u32,
        reserve_items: usize,
    ) -> OperatorValidatorResult<()> {
        if relative_depth as usize >= func_state.blocks.len() {
            return Err("invalid block depth");
        }
        let block = func_state.block_at(relative_depth as usize);
        if block.jump_to_top {
            if !func_state.assert_block_stack_len(0, reserve_items) {
                return Err("stack size does not match target loop type");
            }
            return Ok(());
        }

        let len = block.return_types.len();
        if !func_state.assert_block_stack_len(0, len + reserve_items) {
            return Err("stack size does not match target block type");
        }
        self.check_block_return_types(func_state, block, reserve_items)
    }

    fn match_block_return(
        &self,
        func_state: &FuncState,
        depth1: u32,
        depth2: u32,
    ) -> OperatorValidatorResult<()> {
        if depth1 as usize >= func_state.blocks.len() {
            return Err("invalid block depth");
        }
        if depth2 as usize >= func_state.blocks.len() {
            return Err("invalid block depth");
        }
        let block1 = func_state.block_at(depth1 as usize);
        let block2 = func_state.block_at(depth2 as usize);
        let return_types1 = &block1.return_types;
        let return_types2 = &block2.return_types;
        if block1.jump_to_top || block2.jump_to_top {
            if block1.jump_to_top {
                if !block2.jump_to_top && !return_types2.is_empty() {
                    return Err("block types do not match");
                }
            } else if !return_types1.is_empty() {
                return Err("block types do not match");
            }
        } else if *return_types1 != *return_types2 {
            return Err("block types do not match");
        }
        Ok(())
    }

    fn check_memory_index(
        &self,
        memory_index: u32,
        resources: &WasmModuleResources,
    ) -> OperatorValidatorResult<()> {
        if memory_index as usize >= resources.memories().len() {
            return Err("no liner memories are present");
        }
        Ok(())
    }

    fn check_shared_memory_index(
        &self,
        memory_index: u32,
        resources: &WasmModuleResources,
    ) -> OperatorValidatorResult<()> {
        if memory_index as usize >= resources.memories().len() {
            return Err("no liner memories are present");
        }
        if !resources.memories()[memory_index as usize].shared {
            return Err("atomic accesses require shared memory");
        }
        Ok(())
    }

    fn check_memarg(
        &self,
        memarg: &MemoryImmediate,
        max_align: u32,
        resources: &WasmModuleResources,
    ) -> OperatorValidatorResult<()> {
        self.check_memory_index(0, resources)?;
        let align = memarg.flags;
        if align > max_align {
            return Err("align is required to be at most the number of accessed bytes");
        }
        Ok(())
    }

    fn check_threads_enabled(&self) -> OperatorValidatorResult<()> {
        if !self.config.enable_threads {
            return Err("threads support is not enabled");
        }
        Ok(())
    }

    fn check_reference_types_enabled(&self) -> OperatorValidatorResult<()> {
        if !self.config.enable_reference_types {
            return Err("reference types support is not enabled");
        }
        Ok(())
    }

    fn check_simd_enabled(&self) -> OperatorValidatorResult<()> {
        if !self.config.enable_simd {
            return Err("SIMD support is not enabled");
        }
        Ok(())
    }

    fn check_bulk_memory_enabled(&self) -> OperatorValidatorResult<()> {
        if !self.config.enable_bulk_memory {
            return Err("bulk memory support is not enabled");
        }
        Ok(())
    }

    fn check_shared_memarg_wo_align(
        &self,
        _: &MemoryImmediate,
        resources: &WasmModuleResources,
    ) -> OperatorValidatorResult<()> {
        self.check_shared_memory_index(0, resources)?;
        Ok(())
    }

    fn check_simd_line_index(&self, index: SIMDLineIndex, max: u8) -> OperatorValidatorResult<()> {
        if index >= max {
            return Err("SIMD index out of bounds");
        }
        Ok(())
    }

    fn check_block_type(&self, ty: Type) -> OperatorValidatorResult<()> {
        match ty {
            Type::EmptyBlockType | Type::I32 | Type::I64 | Type::F32 | Type::F64 => Ok(()),
            _ => Err("invalid block return type"),
        }
    }

    fn check_select(&self, func_state: &FuncState) -> OperatorValidatorResult<Option<Type>> {
        self.check_frame_size(func_state, 3)?;
        let last_block = func_state.last_block();
        Ok(if last_block.is_stack_polymorphic() {
            match func_state
                .stack_types
                .len()
                .checked_sub(last_block.stack_starts_at)
                .ok_or("invalid block signature")?
            {
                0 => None,
                1 => {
                    self.check_operands_1(func_state, Type::I32)?;
                    None
                }
                2 => {
                    self.check_operands_1(func_state, Type::I32)?;
                    Some(func_state.stack_types[func_state.stack_types.len() - 2])
                }
                _ => {
                    let ty = func_state.stack_types[func_state.stack_types.len() - 3];
                    self.check_operands_2(func_state, ty, Type::I32)?;
                    Some(ty)
                }
            }
        } else {
            let ty = func_state.stack_types[func_state.stack_types.len() - 3];
            self.check_operands_2(func_state, ty, Type::I32)?;
            Some(ty)
        })
    }

    fn process_operator(
        &self,
        operator: &Operator,
        resources: &WasmModuleResources,
    ) -> OperatorValidatorResult<OperatorAction> {
        let func_state = &self.func_state;
        if func_state.end_function {
            return Err("unexpected operator");
        }
        Ok(match *operator {
            Operator::Unreachable => OperatorAction::DeadCode,
            Operator::Nop => OperatorAction::None,
            Operator::Block { ty } => {
                self.check_block_type(ty)?;
                OperatorAction::PushBlock(ty, BlockType::Block)
            }
            Operator::Loop { ty } => {
                self.check_block_type(ty)?;
                OperatorAction::PushBlock(ty, BlockType::Loop)
            }
            Operator::If { ty } => {
                self.check_block_type(ty)?;
                self.check_operands_1(func_state, Type::I32)?;
                OperatorAction::PushBlock(ty, BlockType::If)
            }
            Operator::Else => {
                if !func_state.last_block().is_else_allowed {
                    return Err("unexpected else: if block is not started");
                }
                self.check_block_return(func_state)?;
                OperatorAction::ResetBlock
            }
            Operator::End => {
                self.check_block_return(func_state)?;
                let last_block = &func_state.last_block();
                if func_state.blocks.len() == 1 {
                    OperatorAction::EndFunction
                } else {
                    if last_block.is_else_allowed && !last_block.return_types.is_empty() {
                        return Err("else is expected: if block has type");
                    }
                    OperatorAction::PopBlock
                }
            }
            Operator::Br { relative_depth } => {
                self.check_jump_from_block(func_state, relative_depth, 0)?;
                OperatorAction::DeadCode
            }
            Operator::BrIf { relative_depth } => {
                self.check_operands_1(func_state, Type::I32)?;
                self.check_jump_from_block(func_state, relative_depth, 1)?;
                if func_state.last_block().is_stack_polymorphic() {
                    let block = func_state.block_at(relative_depth as usize);
                    OperatorAction::ChangeFrameToExactTypes(block.return_types.clone())
                } else {
                    OperatorAction::ChangeFrame(1)
                }
            }
            Operator::BrTable { ref table } => {
                self.check_operands_1(func_state, Type::I32)?;
                let mut depth0: Option<u32> = None;
                for relative_depth in table {
                    if depth0.is_none() {
                        self.check_jump_from_block(func_state, relative_depth, 1)?;
                        depth0 = Some(relative_depth);
                        continue;
                    }
                    self.match_block_return(func_state, relative_depth, depth0.unwrap())?;
                }
                OperatorAction::DeadCode
            }
            Operator::Return => {
                let depth = (func_state.blocks.len() - 1) as u32;
                self.check_jump_from_block(func_state, depth, 0)?;
                OperatorAction::DeadCode
            }
            Operator::Call { function_index } => {
                if function_index as usize >= resources.func_type_indices().len() {
                    return Err("function index out of bounds");
                }
                let type_index = resources.func_type_indices()[function_index as usize];
                let ty = &resources.types()[type_index as usize];
                self.check_operands(func_state, &ty.params)?;
                OperatorAction::ChangeFrameWithTypes(ty.params.len(), ty.returns.clone())
            }
            Operator::CallIndirect { index, table_index } => {
                if table_index as usize >= resources.tables().len() {
                    return Err("table index out of bounds");
                }
                if index as usize >= resources.types().len() {
                    return Err("type index out of bounds");
                }
                let ty = &resources.types()[index as usize];
                let mut types = Vec::with_capacity(ty.params.len() + 1);
                types.extend_from_slice(&ty.params);
                types.push(Type::I32);
                self.check_operands(func_state, &types)?;
                OperatorAction::ChangeFrameWithTypes(ty.params.len() + 1, ty.returns.clone())
            }
            Operator::Drop => {
                self.check_frame_size(func_state, 1)?;
                OperatorAction::ChangeFrame(1)
            }
            Operator::Select => {
                let ty = self.check_select(func_state)?;
                OperatorAction::ChangeFrameAfterSelect(ty)
            }
            Operator::GetLocal { local_index } => {
                if local_index as usize >= func_state.local_types.len() {
                    return Err("local index out of bounds");
                }
                let local_type = func_state.local_types[local_index as usize];
                OperatorAction::ChangeFrameWithType(0, local_type)
            }
            Operator::SetLocal { local_index } => {
                if local_index as usize >= func_state.local_types.len() {
                    return Err("local index out of bounds");
                }
                let local_type = func_state.local_types[local_index as usize];
                self.check_operands_1(func_state, local_type)?;
                OperatorAction::ChangeFrame(1)
            }
            Operator::TeeLocal { local_index } => {
                if local_index as usize >= func_state.local_types.len() {
                    return Err("local index out of bounds");
                }
                let local_type = func_state.local_types[local_index as usize];
                self.check_operands_1(func_state, local_type)?;
                OperatorAction::ChangeFrameWithType(1, local_type)
            }
            Operator::GetGlobal { global_index } => {
                if global_index as usize >= resources.globals().len() {
                    return Err("global index out of bounds");
                }
                let ty = &resources.globals()[global_index as usize];
                OperatorAction::ChangeFrameWithType(0, ty.content_type)
            }
            Operator::SetGlobal { global_index } => {
                if global_index as usize >= resources.globals().len() {
                    return Err("global index out of bounds");
                }
                let ty = &resources.globals()[global_index as usize];
                // FIXME
                //    if !ty.mutable {
                //        return self.create_error("global expected to be mutable");
                //    }
                self.check_operands_1(func_state, ty.content_type)?;
                OperatorAction::ChangeFrame(1)
            }
            Operator::I32Load { ref memarg } => {
                self.check_memarg(memarg, 2, resources)?;
                self.check_operands_1(func_state, Type::I32)?;
                OperatorAction::ChangeFrameWithType(1, Type::I32)
            }
            Operator::I64Load { ref memarg } => {
                self.check_memarg(memarg, 3, resources)?;
                self.check_operands_1(func_state, Type::I32)?;
                OperatorAction::ChangeFrameWithType(1, Type::I64)
            }
            Operator::F32Load { ref memarg } => {
                self.check_memarg(memarg, 2, resources)?;
                self.check_operands_1(func_state, Type::I32)?;
                OperatorAction::ChangeFrameWithType(1, Type::F32)
            }
            Operator::F64Load { ref memarg } => {
                self.check_memarg(memarg, 3, resources)?;
                self.check_operands_1(func_state, Type::I32)?;
                OperatorAction::ChangeFrameWithType(1, Type::F64)
            }
            Operator::I32Load8S { ref memarg } => {
                self.check_memarg(memarg, 0, resources)?;
                self.check_operands_1(func_state, Type::I32)?;
                OperatorAction::ChangeFrameWithType(1, Type::I32)
            }
            Operator::I32Load8U { ref memarg } => {
                self.check_memarg(memarg, 0, resources)?;
                self.check_operands_1(func_state, Type::I32)?;
                OperatorAction::ChangeFrameWithType(1, Type::I32)
            }
            Operator::I32Load16S { ref memarg } => {
                self.check_memarg(memarg, 1, resources)?;
                self.check_operands_1(func_state, Type::I32)?;
                OperatorAction::ChangeFrameWithType(1, Type::I32)
            }
            Operator::I32Load16U { ref memarg } => {
                self.check_memarg(memarg, 1, resources)?;
                self.check_operands_1(func_state, Type::I32)?;
                OperatorAction::ChangeFrameWithType(1, Type::I32)
            }
            Operator::I64Load8S { ref memarg } => {
                self.check_memarg(memarg, 0, resources)?;
                self.check_operands_1(func_state, Type::I32)?;
                OperatorAction::ChangeFrameWithType(1, Type::I64)
            }
            Operator::I64Load8U { ref memarg } => {
                self.check_memarg(memarg, 0, resources)?;
                self.check_operands_1(func_state, Type::I32)?;
                OperatorAction::ChangeFrameWithType(1, Type::I64)
            }
            Operator::I64Load16S { ref memarg } => {
                self.check_memarg(memarg, 1, resources)?;
                self.check_operands_1(func_state, Type::I32)?;
                OperatorAction::ChangeFrameWithType(1, Type::I64)
            }
            Operator::I64Load16U { ref memarg } => {
                self.check_memarg(memarg, 1, resources)?;
                self.check_operands_1(func_state, Type::I32)?;
                OperatorAction::ChangeFrameWithType(1, Type::I64)
            }
            Operator::I64Load32S { ref memarg } => {
                self.check_memarg(memarg, 2, resources)?;
                self.check_operands_1(func_state, Type::I32)?;
                OperatorAction::ChangeFrameWithType(1, Type::I64)
            }
            Operator::I64Load32U { ref memarg } => {
                self.check_memarg(memarg, 2, resources)?;
                self.check_operands_1(func_state, Type::I32)?;
                OperatorAction::ChangeFrameWithType(1, Type::I64)
            }
            Operator::I32Store { ref memarg } => {
                self.check_memarg(memarg, 2, resources)?;
                self.check_operands_2(func_state, Type::I32, Type::I32)?;
                OperatorAction::ChangeFrame(2)
            }
            Operator::I64Store { ref memarg } => {
                self.check_memarg(memarg, 3, resources)?;
                self.check_operands_2(func_state, Type::I32, Type::I64)?;
                OperatorAction::ChangeFrame(2)
            }
            Operator::F32Store { ref memarg } => {
                self.check_memarg(memarg, 2, resources)?;
                self.check_operands_2(func_state, Type::I32, Type::F32)?;
                OperatorAction::ChangeFrame(2)
            }
            Operator::F64Store { ref memarg } => {
                self.check_memarg(memarg, 3, resources)?;
                self.check_operands_2(func_state, Type::I32, Type::F64)?;
                OperatorAction::ChangeFrame(2)
            }
            Operator::I32Store8 { ref memarg } => {
                self.check_memarg(memarg, 0, resources)?;
                self.check_operands_2(func_state, Type::I32, Type::I32)?;
                OperatorAction::ChangeFrame(2)
            }
            Operator::I32Store16 { ref memarg } => {
                self.check_memarg(memarg, 1, resources)?;
                self.check_operands_2(func_state, Type::I32, Type::I32)?;
                OperatorAction::ChangeFrame(2)
            }
            Operator::I64Store8 { ref memarg } => {
                self.check_memarg(memarg, 0, resources)?;
                self.check_operands_2(func_state, Type::I32, Type::I64)?;
                OperatorAction::ChangeFrame(2)
            }
            Operator::I64Store16 { ref memarg } => {
                self.check_memarg(memarg, 1, resources)?;
                self.check_operands_2(func_state, Type::I32, Type::I64)?;
                OperatorAction::ChangeFrame(2)
            }
            Operator::I64Store32 { ref memarg } => {
                self.check_memarg(memarg, 2, resources)?;
                self.check_operands_2(func_state, Type::I32, Type::I64)?;
                OperatorAction::ChangeFrame(2)
            }
            Operator::MemorySize {
                reserved: memory_index,
            } => {
                self.check_memory_index(memory_index, resources)?;
                OperatorAction::ChangeFrameWithType(0, Type::I32)
            }
            Operator::MemoryGrow {
                reserved: memory_index,
            } => {
                self.check_memory_index(memory_index, resources)?;
                self.check_operands_1(func_state, Type::I32)?;
                OperatorAction::ChangeFrameWithType(1, Type::I32)
            }
            Operator::I32Const { .. } => OperatorAction::ChangeFrameWithType(0, Type::I32),
            Operator::I64Const { .. } => OperatorAction::ChangeFrameWithType(0, Type::I64),
            Operator::F32Const { .. } => OperatorAction::ChangeFrameWithType(0, Type::F32),
            Operator::F64Const { .. } => OperatorAction::ChangeFrameWithType(0, Type::F64),
            Operator::I32Eqz => {
                self.check_operands_1(func_state, Type::I32)?;
                OperatorAction::ChangeFrameWithType(1, Type::I32)
            }
            Operator::I32Eq
            | Operator::I32Ne
            | Operator::I32LtS
            | Operator::I32LtU
            | Operator::I32GtS
            | Operator::I32GtU
            | Operator::I32LeS
            | Operator::I32LeU
            | Operator::I32GeS
            | Operator::I32GeU => {
                self.check_operands_2(func_state, Type::I32, Type::I32)?;
                OperatorAction::ChangeFrameWithType(2, Type::I32)
            }
            Operator::I64Eqz => {
                self.check_operands_1(func_state, Type::I64)?;
                OperatorAction::ChangeFrameWithType(1, Type::I32)
            }
            Operator::I64Eq
            | Operator::I64Ne
            | Operator::I64LtS
            | Operator::I64LtU
            | Operator::I64GtS
            | Operator::I64GtU
            | Operator::I64LeS
            | Operator::I64LeU
            | Operator::I64GeS
            | Operator::I64GeU => {
                self.check_operands_2(func_state, Type::I64, Type::I64)?;
                OperatorAction::ChangeFrameWithType(2, Type::I32)
            }
            Operator::F32Eq
            | Operator::F32Ne
            | Operator::F32Lt
            | Operator::F32Gt
            | Operator::F32Le
            | Operator::F32Ge => {
                self.check_operands_2(func_state, Type::F32, Type::F32)?;
                OperatorAction::ChangeFrameWithType(2, Type::I32)
            }
            Operator::F64Eq
            | Operator::F64Ne
            | Operator::F64Lt
            | Operator::F64Gt
            | Operator::F64Le
            | Operator::F64Ge => {
                self.check_operands_2(func_state, Type::F64, Type::F64)?;
                OperatorAction::ChangeFrameWithType(2, Type::I32)
            }
            Operator::I32Clz | Operator::I32Ctz | Operator::I32Popcnt => {
                self.check_operands_1(func_state, Type::I32)?;
                OperatorAction::ChangeFrameWithType(1, Type::I32)
            }
            Operator::I32Add
            | Operator::I32Sub
            | Operator::I32Mul
            | Operator::I32DivS
            | Operator::I32DivU
            | Operator::I32RemS
            | Operator::I32RemU
            | Operator::I32And
            | Operator::I32Or
            | Operator::I32Xor
            | Operator::I32Shl
            | Operator::I32ShrS
            | Operator::I32ShrU
            | Operator::I32Rotl
            | Operator::I32Rotr => {
                self.check_operands_2(func_state, Type::I32, Type::I32)?;
                OperatorAction::ChangeFrameWithType(2, Type::I32)
            }
            Operator::I64Clz | Operator::I64Ctz | Operator::I64Popcnt => {
                self.check_operands_1(func_state, Type::I64)?;
                OperatorAction::ChangeFrameWithType(1, Type::I64)
            }
            Operator::I64Add
            | Operator::I64Sub
            | Operator::I64Mul
            | Operator::I64DivS
            | Operator::I64DivU
            | Operator::I64RemS
            | Operator::I64RemU
            | Operator::I64And
            | Operator::I64Or
            | Operator::I64Xor
            | Operator::I64Shl
            | Operator::I64ShrS
            | Operator::I64ShrU
            | Operator::I64Rotl
            | Operator::I64Rotr => {
                self.check_operands_2(func_state, Type::I64, Type::I64)?;
                OperatorAction::ChangeFrameWithType(2, Type::I64)
            }
            Operator::F32Abs
            | Operator::F32Neg
            | Operator::F32Ceil
            | Operator::F32Floor
            | Operator::F32Trunc
            | Operator::F32Nearest
            | Operator::F32Sqrt => {
                self.check_operands_1(func_state, Type::F32)?;
                OperatorAction::ChangeFrameWithType(1, Type::F32)
            }
            Operator::F32Add
            | Operator::F32Sub
            | Operator::F32Mul
            | Operator::F32Div
            | Operator::F32Min
            | Operator::F32Max
            | Operator::F32Copysign => {
                self.check_operands_2(func_state, Type::F32, Type::F32)?;
                OperatorAction::ChangeFrameWithType(2, Type::F32)
            }
            Operator::F64Abs
            | Operator::F64Neg
            | Operator::F64Ceil
            | Operator::F64Floor
            | Operator::F64Trunc
            | Operator::F64Nearest
            | Operator::F64Sqrt => {
                self.check_operands_1(func_state, Type::F64)?;
                OperatorAction::ChangeFrameWithType(1, Type::F64)
            }
            Operator::F64Add
            | Operator::F64Sub
            | Operator::F64Mul
            | Operator::F64Div
            | Operator::F64Min
            | Operator::F64Max
            | Operator::F64Copysign => {
                self.check_operands_2(func_state, Type::F64, Type::F64)?;
                OperatorAction::ChangeFrameWithType(2, Type::F64)
            }
            Operator::I32WrapI64 => {
                self.check_operands_1(func_state, Type::I64)?;
                OperatorAction::ChangeFrameWithType(1, Type::I32)
            }
            Operator::I32TruncSF32 | Operator::I32TruncUF32 => {
                self.check_operands_1(func_state, Type::F32)?;
                OperatorAction::ChangeFrameWithType(1, Type::I32)
            }
            Operator::I32TruncSF64 | Operator::I32TruncUF64 => {
                self.check_operands_1(func_state, Type::F64)?;
                OperatorAction::ChangeFrameWithType(1, Type::I32)
            }
            Operator::I64ExtendSI32 | Operator::I64ExtendUI32 => {
                self.check_operands_1(func_state, Type::I32)?;
                OperatorAction::ChangeFrameWithType(1, Type::I64)
            }
            Operator::I64TruncSF32 | Operator::I64TruncUF32 => {
                self.check_operands_1(func_state, Type::F32)?;
                OperatorAction::ChangeFrameWithType(1, Type::I64)
            }
            Operator::I64TruncSF64 | Operator::I64TruncUF64 => {
                self.check_operands_1(func_state, Type::F64)?;
                OperatorAction::ChangeFrameWithType(1, Type::I64)
            }
            Operator::F32ConvertSI32 | Operator::F32ConvertUI32 => {
                self.check_operands_1(func_state, Type::I32)?;
                OperatorAction::ChangeFrameWithType(1, Type::F32)
            }
            Operator::F32ConvertSI64 | Operator::F32ConvertUI64 => {
                self.check_operands_1(func_state, Type::I64)?;
                OperatorAction::ChangeFrameWithType(1, Type::F32)
            }
            Operator::F32DemoteF64 => {
                self.check_operands_1(func_state, Type::F64)?;
                OperatorAction::ChangeFrameWithType(1, Type::F32)
            }
            Operator::F64ConvertSI32 | Operator::F64ConvertUI32 => {
                self.check_operands_1(func_state, Type::I32)?;
                OperatorAction::ChangeFrameWithType(1, Type::F64)
            }
            Operator::F64ConvertSI64 | Operator::F64ConvertUI64 => {
                self.check_operands_1(func_state, Type::I64)?;
                OperatorAction::ChangeFrameWithType(1, Type::F64)
            }
            Operator::F64PromoteF32 => {
                self.check_operands_1(func_state, Type::F32)?;
                OperatorAction::ChangeFrameWithType(1, Type::F64)
            }
            Operator::I32ReinterpretF32 => {
                self.check_operands_1(func_state, Type::F32)?;
                OperatorAction::ChangeFrameWithType(1, Type::I32)
            }
            Operator::I64ReinterpretF64 => {
                self.check_operands_1(func_state, Type::F64)?;
                OperatorAction::ChangeFrameWithType(1, Type::I64)
            }
            Operator::F32ReinterpretI32 => {
                self.check_operands_1(func_state, Type::I32)?;
                OperatorAction::ChangeFrameWithType(1, Type::F32)
            }
            Operator::F64ReinterpretI64 => {
                self.check_operands_1(func_state, Type::I64)?;
                OperatorAction::ChangeFrameWithType(1, Type::F64)
            }
            Operator::I32TruncSSatF32 | Operator::I32TruncUSatF32 => {
                self.check_operands_1(func_state, Type::F32)?;
                OperatorAction::ChangeFrameWithType(1, Type::I32)
            }
            Operator::I32TruncSSatF64 | Operator::I32TruncUSatF64 => {
                self.check_operands_1(func_state, Type::F64)?;
                OperatorAction::ChangeFrameWithType(1, Type::I32)
            }
            Operator::I64TruncSSatF32 | Operator::I64TruncUSatF32 => {
                self.check_operands_1(func_state, Type::F32)?;
                OperatorAction::ChangeFrameWithType(1, Type::I64)
            }
            Operator::I64TruncSSatF64 | Operator::I64TruncUSatF64 => {
                self.check_operands_1(func_state, Type::F64)?;
                OperatorAction::ChangeFrameWithType(1, Type::I64)
            }
            Operator::I32Extend16S | Operator::I32Extend8S => {
                self.check_operands_1(func_state, Type::I32)?;
                OperatorAction::ChangeFrameWithType(1, Type::I32)
            }

            Operator::I64Extend32S | Operator::I64Extend16S | Operator::I64Extend8S => {
                self.check_operands_1(func_state, Type::I64)?;
                OperatorAction::ChangeFrameWithType(1, Type::I64)
            }

            Operator::I32AtomicLoad { ref memarg }
            | Operator::I32AtomicLoad16U { ref memarg }
            | Operator::I32AtomicLoad8U { ref memarg } => {
                self.check_threads_enabled()?;
                self.check_shared_memarg_wo_align(memarg, resources)?;
                self.check_operands_1(func_state, Type::I32)?;
                OperatorAction::ChangeFrameWithType(1, Type::I32)
            }
            Operator::I64AtomicLoad { ref memarg }
            | Operator::I64AtomicLoad32U { ref memarg }
            | Operator::I64AtomicLoad16U { ref memarg }
            | Operator::I64AtomicLoad8U { ref memarg } => {
                self.check_threads_enabled()?;
                self.check_shared_memarg_wo_align(memarg, resources)?;
                self.check_operands_1(func_state, Type::I32)?;
                OperatorAction::ChangeFrameWithType(1, Type::I64)
            }
            Operator::I32AtomicStore { ref memarg }
            | Operator::I32AtomicStore16 { ref memarg }
            | Operator::I32AtomicStore8 { ref memarg } => {
                self.check_threads_enabled()?;
                self.check_shared_memarg_wo_align(memarg, resources)?;
                self.check_operands_2(func_state, Type::I32, Type::I32)?;
                OperatorAction::ChangeFrame(2)
            }
            Operator::I64AtomicStore { ref memarg }
            | Operator::I64AtomicStore32 { ref memarg }
            | Operator::I64AtomicStore16 { ref memarg }
            | Operator::I64AtomicStore8 { ref memarg } => {
                self.check_threads_enabled()?;
                self.check_shared_memarg_wo_align(memarg, resources)?;
                self.check_operands_2(func_state, Type::I32, Type::I64)?;
                OperatorAction::ChangeFrame(2)
            }
            Operator::I32AtomicRmwAdd { ref memarg }
            | Operator::I32AtomicRmwSub { ref memarg }
            | Operator::I32AtomicRmwAnd { ref memarg }
            | Operator::I32AtomicRmwOr { ref memarg }
            | Operator::I32AtomicRmwXor { ref memarg }
            | Operator::I32AtomicRmw16UAdd { ref memarg }
            | Operator::I32AtomicRmw16USub { ref memarg }
            | Operator::I32AtomicRmw16UAnd { ref memarg }
            | Operator::I32AtomicRmw16UOr { ref memarg }
            | Operator::I32AtomicRmw16UXor { ref memarg }
            | Operator::I32AtomicRmw8UAdd { ref memarg }
            | Operator::I32AtomicRmw8USub { ref memarg }
            | Operator::I32AtomicRmw8UAnd { ref memarg }
            | Operator::I32AtomicRmw8UOr { ref memarg }
            | Operator::I32AtomicRmw8UXor { ref memarg } => {
                self.check_threads_enabled()?;
                self.check_shared_memarg_wo_align(memarg, resources)?;
                self.check_operands_2(func_state, Type::I32, Type::I32)?;
                OperatorAction::ChangeFrameWithType(2, Type::I32)
            }
            Operator::I64AtomicRmwAdd { ref memarg }
            | Operator::I64AtomicRmwSub { ref memarg }
            | Operator::I64AtomicRmwAnd { ref memarg }
            | Operator::I64AtomicRmwOr { ref memarg }
            | Operator::I64AtomicRmwXor { ref memarg }
            | Operator::I64AtomicRmw32UAdd { ref memarg }
            | Operator::I64AtomicRmw32USub { ref memarg }
            | Operator::I64AtomicRmw32UAnd { ref memarg }
            | Operator::I64AtomicRmw32UOr { ref memarg }
            | Operator::I64AtomicRmw32UXor { ref memarg }
            | Operator::I64AtomicRmw16UAdd { ref memarg }
            | Operator::I64AtomicRmw16USub { ref memarg }
            | Operator::I64AtomicRmw16UAnd { ref memarg }
            | Operator::I64AtomicRmw16UOr { ref memarg }
            | Operator::I64AtomicRmw16UXor { ref memarg }
            | Operator::I64AtomicRmw8UAdd { ref memarg }
            | Operator::I64AtomicRmw8USub { ref memarg }
            | Operator::I64AtomicRmw8UAnd { ref memarg }
            | Operator::I64AtomicRmw8UOr { ref memarg }
            | Operator::I64AtomicRmw8UXor { ref memarg } => {
                self.check_threads_enabled()?;
                self.check_shared_memarg_wo_align(memarg, resources)?;
                self.check_operands_2(func_state, Type::I32, Type::I64)?;
                OperatorAction::ChangeFrameWithType(2, Type::I64)
            }
            Operator::I32AtomicRmwXchg { ref memarg }
            | Operator::I32AtomicRmw16UXchg { ref memarg }
            | Operator::I32AtomicRmw8UXchg { ref memarg } => {
                self.check_threads_enabled()?;
                self.check_shared_memarg_wo_align(memarg, resources)?;
                self.check_operands_2(func_state, Type::I32, Type::I32)?;
                OperatorAction::ChangeFrameWithType(2, Type::I32)
            }
            Operator::I32AtomicRmwCmpxchg { ref memarg }
            | Operator::I32AtomicRmw16UCmpxchg { ref memarg }
            | Operator::I32AtomicRmw8UCmpxchg { ref memarg } => {
                self.check_threads_enabled()?;
                self.check_shared_memarg_wo_align(memarg, resources)?;
                self.check_operands(func_state, &[Type::I32, Type::I32, Type::I32])?;
                OperatorAction::ChangeFrameWithType(3, Type::I32)
            }
            Operator::I64AtomicRmwXchg { ref memarg }
            | Operator::I64AtomicRmw32UXchg { ref memarg }
            | Operator::I64AtomicRmw16UXchg { ref memarg }
            | Operator::I64AtomicRmw8UXchg { ref memarg } => {
                self.check_threads_enabled()?;
                self.check_shared_memarg_wo_align(memarg, resources)?;
                self.check_operands_2(func_state, Type::I32, Type::I64)?;
                OperatorAction::ChangeFrameWithType(2, Type::I64)
            }
            Operator::I64AtomicRmwCmpxchg { ref memarg }
            | Operator::I64AtomicRmw32UCmpxchg { ref memarg }
            | Operator::I64AtomicRmw16UCmpxchg { ref memarg }
            | Operator::I64AtomicRmw8UCmpxchg { ref memarg } => {
                self.check_threads_enabled()?;
                self.check_shared_memarg_wo_align(memarg, resources)?;
                self.check_operands(func_state, &[Type::I32, Type::I64, Type::I64])?;
                OperatorAction::ChangeFrameWithType(3, Type::I64)
            }
            Operator::Wake { ref memarg } => {
                self.check_threads_enabled()?;
                self.check_shared_memarg_wo_align(memarg, resources)?;
                self.check_operands_2(func_state, Type::I32, Type::I32)?;
                OperatorAction::ChangeFrameWithType(2, Type::I32)
            }
            Operator::I32Wait { ref memarg } => {
                self.check_threads_enabled()?;
                self.check_shared_memarg_wo_align(memarg, resources)?;
                self.check_operands(func_state, &[Type::I32, Type::I32, Type::I64])?;
                OperatorAction::ChangeFrameWithType(3, Type::I32)
            }
            Operator::I64Wait { ref memarg } => {
                self.check_threads_enabled()?;
                self.check_shared_memarg_wo_align(memarg, resources)?;
                self.check_operands(func_state, &[Type::I32, Type::I64, Type::I64])?;
                OperatorAction::ChangeFrameWithType(3, Type::I32)
            }
            Operator::RefNull => {
                self.check_reference_types_enabled()?;
                OperatorAction::ChangeFrameWithType(0, Type::AnyRef)
            }
            Operator::RefIsNull => {
                self.check_reference_types_enabled()?;
                self.check_operands(func_state, &[Type::AnyRef])?;
                OperatorAction::ChangeFrameWithType(0, Type::I32)
            }
            Operator::V128Load { ref memarg } => {
                self.check_simd_enabled()?;
                self.check_memarg(memarg, 4, resources)?;
                self.check_operands_1(func_state, Type::I32)?;
                OperatorAction::ChangeFrameWithType(1, Type::V128)
            }
            Operator::V128Store { ref memarg } => {
                self.check_simd_enabled()?;
                self.check_memarg(memarg, 4, resources)?;
                self.check_operands_2(func_state, Type::I32, Type::V128)?;
                OperatorAction::ChangeFrame(2)
            }
            Operator::V128Const { .. } => {
                self.check_simd_enabled()?;
                OperatorAction::ChangeFrameWithType(0, Type::V128)
            }
            Operator::V8x16Shuffle { ref lines } => {
                self.check_simd_enabled()?;
                self.check_operands_2(func_state, Type::V128, Type::V128)?;
                for i in lines {
                    self.check_simd_line_index(*i, 32)?;
                }
                OperatorAction::ChangeFrameWithType(2, Type::V128)
            }
            Operator::I8x16Splat | Operator::I16x8Splat | Operator::I32x4Splat => {
                self.check_simd_enabled()?;
                self.check_operands_1(func_state, Type::I32)?;
                OperatorAction::ChangeFrameWithType(1, Type::V128)
            }
            Operator::I64x2Splat => {
                self.check_simd_enabled()?;
                self.check_operands_1(func_state, Type::I64)?;
                OperatorAction::ChangeFrameWithType(1, Type::V128)
            }
            Operator::F32x4Splat => {
                self.check_simd_enabled()?;
                self.check_operands_1(func_state, Type::F32)?;
                OperatorAction::ChangeFrameWithType(1, Type::V128)
            }
            Operator::F64x2Splat => {
                self.check_simd_enabled()?;
                self.check_operands_1(func_state, Type::F64)?;
                OperatorAction::ChangeFrameWithType(1, Type::V128)
            }
            Operator::I8x16ExtractLaneS { line } | Operator::I8x16ExtractLaneU { line } => {
                self.check_simd_enabled()?;
                self.check_simd_line_index(line, 16)?;
                self.check_operands_1(func_state, Type::V128)?;
                OperatorAction::ChangeFrameWithType(1, Type::I32)
            }
            Operator::I16x8ExtractLaneS { line } | Operator::I16x8ExtractLaneU { line } => {
                self.check_simd_enabled()?;
                self.check_simd_line_index(line, 8)?;
                self.check_operands_1(func_state, Type::V128)?;
                OperatorAction::ChangeFrameWithType(1, Type::I32)
            }
            Operator::I32x4ExtractLane { line } => {
                self.check_simd_enabled()?;
                self.check_simd_line_index(line, 4)?;
                self.check_operands_1(func_state, Type::V128)?;
                OperatorAction::ChangeFrameWithType(1, Type::I32)
            }
            Operator::I8x16ReplaceLane { line } => {
                self.check_simd_enabled()?;
                self.check_simd_line_index(line, 16)?;
                self.check_operands_2(func_state, Type::V128, Type::I32)?;
                OperatorAction::ChangeFrameWithType(2, Type::V128)
            }
            Operator::I16x8ReplaceLane { line } => {
                self.check_simd_enabled()?;
                self.check_simd_line_index(line, 8)?;
                self.check_operands_2(func_state, Type::V128, Type::I32)?;
                OperatorAction::ChangeFrameWithType(2, Type::V128)
            }
            Operator::I32x4ReplaceLane { line } => {
                self.check_simd_enabled()?;
                self.check_simd_line_index(line, 4)?;
                self.check_operands_2(func_state, Type::V128, Type::I32)?;
                OperatorAction::ChangeFrameWithType(2, Type::V128)
            }
            Operator::I64x2ExtractLane { line } => {
                self.check_simd_enabled()?;
                self.check_simd_line_index(line, 2)?;
                self.check_operands_1(func_state, Type::V128)?;
                OperatorAction::ChangeFrameWithType(1, Type::I64)
            }
            Operator::I64x2ReplaceLane { line } => {
                self.check_simd_enabled()?;
                self.check_simd_line_index(line, 2)?;
                self.check_operands_2(func_state, Type::V128, Type::I64)?;
                OperatorAction::ChangeFrameWithType(2, Type::V128)
            }
            Operator::F32x4ExtractLane { line } => {
                self.check_simd_enabled()?;
                self.check_simd_line_index(line, 4)?;
                self.check_operands_1(func_state, Type::V128)?;
                OperatorAction::ChangeFrameWithType(1, Type::F32)
            }
            Operator::F32x4ReplaceLane { line } => {
                self.check_simd_enabled()?;
                self.check_simd_line_index(line, 4)?;
                self.check_operands_2(func_state, Type::V128, Type::F32)?;
                OperatorAction::ChangeFrameWithType(2, Type::V128)
            }
            Operator::F64x2ExtractLane { line } => {
                self.check_simd_enabled()?;
                self.check_simd_line_index(line, 2)?;
                self.check_operands_1(func_state, Type::V128)?;
                OperatorAction::ChangeFrameWithType(1, Type::F64)
            }
            Operator::F64x2ReplaceLane { line } => {
                self.check_simd_enabled()?;
                self.check_simd_line_index(line, 2)?;
                self.check_operands_2(func_state, Type::V128, Type::F64)?;
                OperatorAction::ChangeFrameWithType(2, Type::V128)
            }
            Operator::I8x16Eq
            | Operator::I8x16Ne
            | Operator::I8x16LtS
            | Operator::I8x16LtU
            | Operator::I8x16GtS
            | Operator::I8x16GtU
            | Operator::I8x16LeS
            | Operator::I8x16LeU
            | Operator::I8x16GeS
            | Operator::I8x16GeU
            | Operator::I16x8Eq
            | Operator::I16x8Ne
            | Operator::I16x8LtS
            | Operator::I16x8LtU
            | Operator::I16x8GtS
            | Operator::I16x8GtU
            | Operator::I16x8LeS
            | Operator::I16x8LeU
            | Operator::I16x8GeS
            | Operator::I16x8GeU
            | Operator::I32x4Eq
            | Operator::I32x4Ne
            | Operator::I32x4LtS
            | Operator::I32x4LtU
            | Operator::I32x4GtS
            | Operator::I32x4GtU
            | Operator::I32x4LeS
            | Operator::I32x4LeU
            | Operator::I32x4GeS
            | Operator::I32x4GeU
            | Operator::F32x4Eq
            | Operator::F32x4Ne
            | Operator::F32x4Lt
            | Operator::F32x4Gt
            | Operator::F32x4Le
            | Operator::F32x4Ge
            | Operator::F64x2Eq
            | Operator::F64x2Ne
            | Operator::F64x2Lt
            | Operator::F64x2Gt
            | Operator::F64x2Le
            | Operator::F64x2Ge
            | Operator::V128And
            | Operator::V128Or
            | Operator::V128Xor
            | Operator::I8x16Add
            | Operator::I8x16AddSaturateS
            | Operator::I8x16AddSaturateU
            | Operator::I8x16Sub
            | Operator::I8x16SubSaturateS
            | Operator::I8x16SubSaturateU
            | Operator::I8x16Mul
            | Operator::I16x8Add
            | Operator::I16x8AddSaturateS
            | Operator::I16x8AddSaturateU
            | Operator::I16x8Sub
            | Operator::I16x8SubSaturateS
            | Operator::I16x8SubSaturateU
            | Operator::I16x8Mul
            | Operator::I32x4Add
            | Operator::I32x4Sub
            | Operator::I32x4Mul
            | Operator::I64x2Add
            | Operator::I64x2Sub
            | Operator::F32x4Add
            | Operator::F32x4Sub
            | Operator::F32x4Mul
            | Operator::F32x4Div
            | Operator::F32x4Min
            | Operator::F32x4Max
            | Operator::F64x2Add
            | Operator::F64x2Sub
            | Operator::F64x2Mul
            | Operator::F64x2Div
            | Operator::F64x2Min
            | Operator::F64x2Max => {
                self.check_simd_enabled()?;
                self.check_operands_2(func_state, Type::V128, Type::V128)?;
                OperatorAction::ChangeFrameWithType(2, Type::V128)
            }
            Operator::V128Not
            | Operator::I8x16Neg
            | Operator::I16x8Neg
            | Operator::I32x4Neg
            | Operator::I64x2Neg
            | Operator::F32x4Abs
            | Operator::F32x4Neg
            | Operator::F32x4Sqrt
            | Operator::F64x2Abs
            | Operator::F64x2Neg
            | Operator::F64x2Sqrt
            | Operator::I32x4TruncSF32x4Sat
            | Operator::I32x4TruncUF32x4Sat
            | Operator::I64x2TruncSF64x2Sat
            | Operator::I64x2TruncUF64x2Sat
            | Operator::F32x4ConvertSI32x4
            | Operator::F32x4ConvertUI32x4
            | Operator::F64x2ConvertSI64x2
            | Operator::F64x2ConvertUI64x2 => {
                self.check_simd_enabled()?;
                self.check_operands_1(func_state, Type::V128)?;
                OperatorAction::ChangeFrameWithType(1, Type::V128)
            }
            Operator::V128Bitselect => {
                self.check_simd_enabled()?;
                self.check_operands(func_state, &[Type::V128, Type::V128, Type::V128])?;
                OperatorAction::ChangeFrameWithType(2, Type::V128)
            }
            Operator::I8x16AnyTrue
            | Operator::I8x16AllTrue
            | Operator::I16x8AnyTrue
            | Operator::I16x8AllTrue
            | Operator::I32x4AnyTrue
            | Operator::I32x4AllTrue
            | Operator::I64x2AnyTrue
            | Operator::I64x2AllTrue => {
                self.check_simd_enabled()?;
                self.check_operands_1(func_state, Type::V128)?;
                OperatorAction::ChangeFrameWithType(1, Type::I32)
            }
            Operator::I8x16Shl
            | Operator::I8x16ShrS
            | Operator::I8x16ShrU
            | Operator::I16x8Shl
            | Operator::I16x8ShrS
            | Operator::I16x8ShrU
            | Operator::I32x4Shl
            | Operator::I32x4ShrS
            | Operator::I32x4ShrU
            | Operator::I64x2Shl
            | Operator::I64x2ShrS
            | Operator::I64x2ShrU => {
                self.check_simd_enabled()?;
                self.check_operands_2(func_state, Type::V128, Type::I32)?;
                OperatorAction::ChangeFrameWithType(1, Type::V128)
            }

            Operator::MemoryInit { segment } => {
                self.check_bulk_memory_enabled()?;
                if segment >= resources.data_count() {
                    return Err("segment index out of bounds");
                }
                self.check_memory_index(0, resources)?;
                self.check_operands(func_state, &[Type::I32, Type::I32, Type::I32])?;
                OperatorAction::ChangeFrame(3)
            }
            Operator::DataDrop { segment } => {
                self.check_bulk_memory_enabled()?;
                if segment >= resources.data_count() {
                    return Err("segment index out of bounds");
                }
                OperatorAction::None
            }
            Operator::MemoryCopy | Operator::MemoryFill => {
                self.check_bulk_memory_enabled()?;
                self.check_memory_index(0, resources)?;
                self.check_operands(func_state, &[Type::I32, Type::I32, Type::I32])?;
                OperatorAction::ChangeFrame(3)
            }
            Operator::TableInit { segment } => {
                self.check_bulk_memory_enabled()?;
                if segment >= resources.element_count() {
                    return Err("segment index out of bounds");
                }
                if 0 >= resources.tables().len() {
                    return Err("table index out of bounds");
                }
                self.check_operands(func_state, &[Type::I32, Type::I32, Type::I32])?;
                OperatorAction::ChangeFrame(3)
            }
            Operator::ElemDrop { segment } => {
                self.check_bulk_memory_enabled()?;
                if segment >= resources.element_count() {
                    return Err("segment index out of bounds");
                }
                OperatorAction::None
            }
            Operator::TableCopy => {
                self.check_bulk_memory_enabled()?;
                if 0 >= resources.tables().len() {
                    return Err("table index out of bounds");
                }
                self.check_operands(func_state, &[Type::I32, Type::I32, Type::I32])?;
                OperatorAction::ChangeFrame(3)
            }
            Operator::TableGet { table } => {
                self.check_reference_types_enabled()?;
                if table as usize >= resources.tables().len() {
                    return Err("table index out of bounds");
                }
                self.check_operands(func_state, &[Type::I32])?;
                OperatorAction::ChangeFrameWithType(1, Type::AnyRef)
            }
            Operator::TableSet { table } => {
                self.check_reference_types_enabled()?;
                if table as usize >= resources.tables().len() {
                    return Err("table index out of bounds");
                }
                self.check_operands(func_state, &[Type::I32, Type::AnyRef])?;
                OperatorAction::ChangeFrame(2)
            }
            Operator::TableGrow { table } => {
                self.check_reference_types_enabled()?;
                if table as usize >= resources.tables().len() {
                    return Err("table index out of bounds");
                }
                self.check_operands(func_state, &[Type::I32])?;
                OperatorAction::ChangeFrameWithType(1, Type::I32)
            }
            Operator::TableSize { table } => {
                self.check_reference_types_enabled()?;
                if table as usize >= resources.tables().len() {
                    return Err("table index out of bounds");
                }
                OperatorAction::ChangeFrameWithType(1, Type::I32)
            }
        })
    }

    fn process_end_function(&self) -> OperatorValidatorResult<()> {
        let func_state = &self.func_state;
        if !func_state.end_function {
            return Err("expected end of function");
        }
        Ok(())
    }
}

#[derive(Copy, Clone)]
pub struct ValidatingParserConfig {
    pub operator_config: OperatorValidatorConfig,

    pub mutable_global_imports: bool,
}

const DEFAULT_VALIDATING_PARSER_CONFIG: ValidatingParserConfig = ValidatingParserConfig {
    operator_config: DEFAULT_OPERATOR_VALIDATOR_CONFIG,

    mutable_global_imports: false,
};

pub struct ValidatingParser<'a> {
    parser: Parser<'a>,
    validation_error: Option<ParserState<'a>>,
    read_position: Option<usize>,
    section_order_state: SectionOrderState,
    types: Vec<FuncType>,
    tables: Vec<TableType>,
    memories: Vec<MemoryType>,
    globals: Vec<GlobalType>,
    element_count: u32,
    data_count: Option<u32>,
    data_found: u32,
    func_type_indices: Vec<u32>,
    current_func_index: u32,
    func_imports_count: u32,
    init_expression_state: Option<InitExpressionState>,
    exported_names: HashSet<String>,
    current_operator_validator: Option<OperatorValidator>,
    config: ValidatingParserConfig,
}

impl<'a> WasmModuleResources for ValidatingParser<'a> {
    fn types(&self) -> &[FuncType] {
        &self.types
    }

    fn tables(&self) -> &[TableType] {
        &self.tables
    }

    fn memories(&self) -> &[MemoryType] {
        &self.memories
    }

    fn globals(&self) -> &[GlobalType] {
        &self.globals
    }

    fn func_type_indices(&self) -> &[u32] {
        &self.func_type_indices
    }

    fn element_count(&self) -> u32 {
        self.element_count
    }

    fn data_count(&self) -> u32 {
        self.data_count.unwrap_or(0)
    }
}

impl<'a> ValidatingParser<'a> {
    pub fn new(bytes: &[u8], config: Option<ValidatingParserConfig>) -> ValidatingParser {
        ValidatingParser {
            parser: Parser::new(bytes),
            validation_error: None,
            read_position: None,
            section_order_state: SectionOrderState::Initial,
            types: Vec::new(),
            tables: Vec::new(),
            memories: Vec::new(),
            globals: Vec::new(),
            func_type_indices: Vec::new(),
            current_func_index: 0,
            func_imports_count: 0,
            current_operator_validator: None,
            init_expression_state: None,
            exported_names: HashSet::new(),
            config: config.unwrap_or(DEFAULT_VALIDATING_PARSER_CONFIG),
            element_count: 0,
            data_count: None,
            data_found: 0,
        }
    }

    fn create_validation_error(&self, message: &'static str) -> Option<ParserState<'a>> {
        Some(ParserState::Error(BinaryReaderError {
            message,
            offset: self.read_position.unwrap(),
        }))
    }

    fn create_error<T>(&self, message: &'static str) -> ValidatorResult<'a, T> {
        Err(ParserState::Error(BinaryReaderError {
            message,
            offset: self.read_position.unwrap(),
        }))
    }

    fn check_value_type(&self, ty: Type) -> ValidatorResult<'a, ()> {
        match ty {
            Type::I32 | Type::I64 | Type::F32 | Type::F64 | Type::V128 => Ok(()),
            _ => self.create_error("invalid value type"),
        }
    }

    fn check_value_types(&self, types: &[Type]) -> ValidatorResult<'a, ()> {
        for ty in types {
            self.check_value_type(*ty)?;
        }
        Ok(())
    }

    fn check_limits(&self, limits: &ResizableLimits) -> ValidatorResult<'a, ()> {
        if limits.maximum.is_some() && limits.initial > limits.maximum.unwrap() {
            return self.create_error("maximum limits less than initial");
        }
        Ok(())
    }

    fn check_func_type(&self, func_type: &FuncType) -> ValidatorResult<'a, ()> {
        if let Type::Func = func_type.form {
            self.check_value_types(&*func_type.params)?;
            self.check_value_types(&*func_type.returns)?;
            if func_type.returns.len() > 1 {
                return self.create_error("too many returns, expected 0 or 1");
            }
            Ok(())
        } else {
            self.create_error("type signature is not a func")
        }
    }

    fn check_table_type(&self, table_type: &TableType) -> ValidatorResult<'a, ()> {
        if let Type::AnyFunc = table_type.element_type {
            self.check_limits(&table_type.limits)
        } else {
            self.create_error("element is not anyfunc")
        }
    }

    fn check_memory_type(&self, memory_type: &MemoryType) -> ValidatorResult<'a, ()> {
        self.check_limits(&memory_type.limits)?;
        let initial = memory_type.limits.initial;
        if initial as usize > MAX_WASM_MEMORY_PAGES {
            return self.create_error("memory initial value exceeds limit");
        }
        let maximum = memory_type.limits.maximum;
        if maximum.is_some() && maximum.unwrap() as usize > MAX_WASM_MEMORY_PAGES {
            return self.create_error("memory maximum value exceeds limit");
        }
        Ok(())
    }

    fn check_global_type(&self, global_type: GlobalType) -> ValidatorResult<'a, ()> {
        self.check_value_type(global_type.content_type)
    }

    fn check_import_entry(&self, import_type: &ImportSectionEntryType) -> ValidatorResult<'a, ()> {
        match *import_type {
            ImportSectionEntryType::Function(type_index) => {
                if self.func_type_indices.len() >= MAX_WASM_FUNCTIONS {
                    return self.create_error("functions count out of bounds");
                }
                if type_index as usize >= self.types.len() {
                    return self.create_error("type index out of bounds");
                }
                Ok(())
            }
            ImportSectionEntryType::Table(ref table_type) => {
                if self.tables.len() >= MAX_WASM_TABLES {
                    return self.create_error("tables count must be at most 1");
                }
                self.check_table_type(table_type)
            }
            ImportSectionEntryType::Memory(ref memory_type) => {
                if self.memories.len() >= MAX_WASM_MEMORIES {
                    return self.create_error("memory count must be at most 1");
                }
                self.check_memory_type(memory_type)
            }
            ImportSectionEntryType::Global(global_type) => {
                if self.globals.len() >= MAX_WASM_GLOBALS {
                    return self.create_error("functions count out of bounds");
                }
                if global_type.mutable && !self.config.mutable_global_imports {
                    return self.create_error("global imports are required to be immutable");
                }
                self.check_global_type(global_type)
            }
        }
    }

    fn check_init_expression_operator(&self, operator: &Operator) -> ValidatorResult<'a, ()> {
        let state = self.init_expression_state.as_ref().unwrap();
        if state.validated {
            return self.create_error("only one init_expr operator is expected");
        }
        let ty = match *operator {
            Operator::I32Const { .. } => Type::I32,
            Operator::I64Const { .. } => Type::I64,
            Operator::F32Const { .. } => Type::F32,
            Operator::F64Const { .. } => Type::F64,
            Operator::GetGlobal { global_index } => {
                if global_index as usize >= state.global_count {
                    return self.create_error("init_expr global index out of bounds");
                }
                self.globals[global_index as usize].content_type
            }
            _ => return self.create_error("invalid init_expr operator"),
        };
        if ty != state.ty {
            return self.create_error("invalid init_expr type");
        }
        Ok(())
    }

    fn check_export_entry(
        &self,
        field: &str,
        kind: ExternalKind,
        index: u32,
    ) -> ValidatorResult<'a, ()> {
        if self.exported_names.contains(field) {
            return self.create_error("non-unique export name");
        }
        match kind {
            ExternalKind::Function => {
                if index as usize >= self.func_type_indices.len() {
                    return self.create_error("exported function index out of bounds");
                }
            }
            ExternalKind::Table => {
                if index as usize >= self.tables.len() {
                    return self.create_error("exported table index out of bounds");
                }
            }
            ExternalKind::Memory => {
                if index as usize >= self.memories.len() {
                    return self.create_error("exported memory index out of bounds");
                }
            }
            ExternalKind::Global => {
                if index as usize >= self.globals.len() {
                    return self.create_error("exported global index out of bounds");
                }
                let global = &self.globals[index as usize];
                if global.mutable && !self.config.mutable_global_imports {
                    return self.create_error("exported global must be const");
                }
            }
        };
        Ok(())
    }

    fn check_start(&self, func_index: u32) -> ValidatorResult<'a, ()> {
        if func_index as usize >= self.func_type_indices.len() {
            return self.create_error("start function index out of bounds");
        }
        let type_index = self.func_type_indices[func_index as usize];
        let ty = &self.types[type_index as usize];
        if !ty.params.is_empty() || !ty.returns.is_empty() {
            return self.create_error("invlid start function type");
        }
        Ok(())
    }

    fn process_begin_section(&self, code: &SectionCode) -> ValidatorResult<'a, SectionOrderState> {
        let order_state = SectionOrderState::from_section_code(code);
        Ok(match self.section_order_state {
            SectionOrderState::Initial => {
                if order_state.is_none() {
                    SectionOrderState::Initial
                } else {
                    order_state.unwrap()
                }
            }
            previous => {
                if let Some(order_state_unwraped) = order_state {
                    if previous >= order_state_unwraped {
                        return self.create_error("section out of order");
                    }
                    order_state_unwraped
                } else {
                    previous
                }
            }
        })
    }

    fn process_state(&mut self) {
        match *self.parser.last_state() {
            ParserState::BeginWasm { version } => {
                if version != 1 {
                    self.validation_error = self.create_validation_error("bad wasm file version");
                }
            }
            ParserState::BeginSection { ref code, .. } => {
                let check = self.process_begin_section(code);
                if check.is_err() {
                    self.validation_error = check.err();
                } else {
                    self.section_order_state = check.ok().unwrap();
                }
            }
            ParserState::TypeSectionEntry(ref func_type) => {
                let check = self.check_func_type(func_type);
                if check.is_err() {
                    self.validation_error = check.err();
                } else if self.types.len() > MAX_WASM_TYPES {
                    self.validation_error =
                        self.create_validation_error("types count is out of bounds");
                } else {
                    self.types.push(func_type.clone());
                }
            }
            ParserState::ImportSectionEntry { ref ty, .. } => {
                let check = self.check_import_entry(ty);
                if check.is_err() {
                    self.validation_error = check.err();
                } else {
                    match *ty {
                        ImportSectionEntryType::Function(type_index) => {
                            self.func_imports_count += 1;
                            self.func_type_indices.push(type_index);
                        }
                        ImportSectionEntryType::Table(ref table_type) => {
                            self.tables.push(table_type.clone());
                        }
                        ImportSectionEntryType::Memory(ref memory_type) => {
                            self.memories.push(memory_type.clone());
                        }
                        ImportSectionEntryType::Global(ref global_type) => {
                            self.globals.push(global_type.clone());
                        }
                    }
                }
            }
            ParserState::FunctionSectionEntry(type_index) => {
                if type_index as usize >= self.types.len() {
                    self.validation_error =
                        self.create_validation_error("func type index out of bounds");
                } else if self.func_type_indices.len() >= MAX_WASM_FUNCTIONS {
                    self.validation_error =
                        self.create_validation_error("functions count out of bounds");
                } else {
                    self.func_type_indices.push(type_index);
                }
            }
            ParserState::TableSectionEntry(ref table_type) => {
                if self.tables.len() >= MAX_WASM_TABLES {
                    self.validation_error =
                        self.create_validation_error("tables count must be at most 1");
                } else {
                    self.validation_error = self.check_table_type(table_type).err();
                    self.tables.push(table_type.clone());
                }
            }
            ParserState::MemorySectionEntry(ref memory_type) => {
                if self.memories.len() >= MAX_WASM_MEMORIES {
                    self.validation_error =
                        self.create_validation_error("memories count must be at most 1");
                } else {
                    self.validation_error = self.check_memory_type(memory_type).err();
                    self.memories.push(memory_type.clone());
                }
            }
            ParserState::BeginGlobalSectionEntry(global_type) => {
                if self.globals.len() >= MAX_WASM_GLOBALS {
                    self.validation_error =
                        self.create_validation_error("globals count out of bounds");
                } else {
                    self.validation_error = self.check_global_type(global_type).err();
                    self.init_expression_state = Some(InitExpressionState {
                        ty: global_type.content_type,
                        global_count: self.globals.len(),
                        validated: false,
                    });
                    self.globals.push(global_type);
                }
            }
            ParserState::BeginInitExpressionBody => {
                assert!(self.init_expression_state.is_some());
            }
            ParserState::InitExpressionOperator(ref operator) => {
                self.validation_error = self.check_init_expression_operator(operator).err();
                self.init_expression_state.as_mut().unwrap().validated = true;
            }
            ParserState::EndInitExpressionBody => {
                if !self.init_expression_state.as_ref().unwrap().validated {
                    self.validation_error = self.create_validation_error("init_expr is empty");
                }
                self.init_expression_state = None;
            }
            ParserState::ExportSectionEntry { field, kind, index } => {
                self.validation_error = self.check_export_entry(field, kind, index).err();
                self.exported_names.insert(field.to_string());
            }
            ParserState::StartSectionEntry(func_index) => {
                self.validation_error = self.check_start(func_index).err();
            }
            ParserState::DataCountSectionEntry(count) => {
                self.data_count = Some(count);
            }
            ParserState::BeginPassiveElementSectionEntry(_ty) => {
                self.element_count += 1;
            }
            ParserState::BeginActiveElementSectionEntry(table_index) => {
                self.element_count += 1;
                if table_index as usize >= self.tables.len() {
                    self.validation_error =
                        self.create_validation_error("element section table index out of bounds");
                } else {
                    assert!(self.tables[table_index as usize].element_type == Type::AnyFunc);
                    self.init_expression_state = Some(InitExpressionState {
                        ty: Type::I32,
                        global_count: self.globals.len(),
                        validated: false,
                    });
                }
            }
            ParserState::ElementSectionEntryBody(ref indices) => {
                for func_index in &**indices {
                    if *func_index as usize >= self.func_type_indices.len() {
                        self.validation_error =
                            self.create_validation_error("element func index out of bounds");
                        break;
                    }
                }
            }
            ParserState::BeginFunctionBody { .. } => {
                let index = (self.current_func_index + self.func_imports_count) as usize;
                if index as usize >= self.func_type_indices.len() {
                    self.validation_error =
                        self.create_validation_error("func type is not defined");
                }
            }
            ParserState::FunctionBodyLocals { ref locals } => {
                let index = (self.current_func_index + self.func_imports_count) as usize;
                let func_type = &self.types[self.func_type_indices[index] as usize];
                let operator_config = self.config.operator_config;
                self.current_operator_validator =
                    Some(OperatorValidator::new(func_type, locals, operator_config));
            }
            ParserState::CodeOperator(ref operator) => {
                let check = self
                    .current_operator_validator
                    .as_ref()
                    .unwrap()
                    .process_operator(operator, self);
                match check {
                    Ok(action) => {
                        if let Err(err) = action.update(
                            &mut self.current_operator_validator.as_mut().unwrap().func_state,
                        ) {
                            self.create_validation_error(err);
                        }
                    }
                    Err(err) => {
                        self.validation_error = self.create_validation_error(err);
                    }
                }
            }
            ParserState::EndFunctionBody => {
                let check = self
                    .current_operator_validator
                    .as_ref()
                    .unwrap()
                    .process_end_function();
                if check.is_err() {
                    self.validation_error = self.create_validation_error(check.err().unwrap());
                }
                self.current_func_index += 1;
                self.current_operator_validator = None;
            }
            ParserState::BeginDataSectionEntryBody(_) => {
                self.data_found += 1;
            }
            ParserState::BeginActiveDataSectionEntry(memory_index) => {
                if memory_index as usize >= self.memories.len() {
                    self.validation_error =
                        self.create_validation_error("data section memory index out of bounds");
                } else {
                    self.init_expression_state = Some(InitExpressionState {
                        ty: Type::I32,
                        global_count: self.globals.len(),
                        validated: false,
                    });
                }
            }
            ParserState::EndWasm => {
                if self.func_type_indices.len()
                    != self.current_func_index as usize + self.func_imports_count as usize
                {
                    self.validation_error = self.create_validation_error(
                        "function and code section have inconsistent lengths",
                    );
                }
                if let Some(data_count) = self.data_count {
                    if data_count != self.data_found {
                        self.validation_error = self.create_validation_error(
                            "data count section and passive data mismatch",
                        );
                    }
                }
            }
            _ => (),
        };
    }

    pub fn create_validating_operator_parser<'b>(&mut self) -> ValidatingOperatorParser<'b>
    where
        'a: 'b,
    {
        let func_body_offset = match *self.last_state() {
            ParserState::BeginFunctionBody { .. } => self.parser.current_position(),
            _ => panic!("Invalid reader state"),
        };
        self.read();
        let operator_validator = match *self.last_state() {
            ParserState::FunctionBodyLocals { ref locals } => {
                let index = (self.current_func_index + self.func_imports_count) as usize;
                let func_type = &self.types[self.func_type_indices[index] as usize];
                let operator_config = self.config.operator_config;
                OperatorValidator::new(func_type, locals, operator_config)
            }
            _ => panic!("Invalid reader state"),
        };
        let reader = self.create_binary_reader();
        ValidatingOperatorParser {
            operator_validator,
            reader,
            func_body_offset,
            end_function: false,
        }
    }
}

impl<'a> WasmDecoder<'a> for ValidatingParser<'a> {
    fn read(&mut self) -> &ParserState<'a> {
        if self.validation_error.is_some() {
            panic!("Parser in error state: validation");
        }
        self.read_position = Some(self.parser.current_position());
        self.parser.read();
        self.process_state();
        self.last_state()
    }

    fn push_input(&mut self, input: ParserInput) {
        match input {
            ParserInput::SkipSection => panic!("Not supported"),
            ParserInput::ReadSectionRawData => panic!("Not supported"),
            _ => self.parser.push_input(input),
        }
    }

    fn read_with_input(&mut self, input: ParserInput) -> &ParserState<'a> {
        self.push_input(input);
        self.read()
    }

    fn create_binary_reader<'b>(&mut self) -> BinaryReader<'b>
    where
        'a: 'b,
    {
        if let ParserState::BeginSection { .. } = *self.parser.last_state() {
            panic!("Not supported");
        }
        self.parser.create_binary_reader()
    }

    fn last_state(&self) -> &ParserState<'a> {
        if self.validation_error.is_some() {
            self.validation_error.as_ref().unwrap()
        } else {
            self.parser.last_state()
        }
    }
}

pub struct ValidatingOperatorParser<'b> {
    operator_validator: OperatorValidator,
    reader: BinaryReader<'b>,
    func_body_offset: usize,
    end_function: bool,
}

impl<'b> ValidatingOperatorParser<'b> {
    pub fn eof(&self) -> bool {
        self.end_function
    }

    pub fn current_position(&self) -> usize {
        self.reader.current_position()
    }

    pub fn is_dead_code(&self) -> bool {
        self.operator_validator.is_dead_code()
    }

    /// Creates a BinaryReader when current state is ParserState::BeginSection
    /// or ParserState::BeginFunctionBody.
    ///
    /// # Examples
    /// ```
    /// # let data = &[0x0, 0x61, 0x73, 0x6d, 0x1, 0x0, 0x0, 0x0, 0x1, 0x84,
    /// #              0x80, 0x80, 0x80, 0x0, 0x1, 0x60, 0x0, 0x0, 0x3, 0x83,
    /// #              0x80, 0x80, 0x80, 0x0, 0x2, 0x0, 0x0, 0x6, 0x81, 0x80,
    /// #              0x80, 0x80, 0x0, 0x0, 0xa, 0x91, 0x80, 0x80, 0x80, 0x0,
    /// #              0x2, 0x83, 0x80, 0x80, 0x80, 0x0, 0x0, 0x1, 0xb, 0x83,
    /// #              0x80, 0x80, 0x80, 0x0, 0x0, 0x0, 0xb];
    /// use wasmparser::{WasmDecoder, ParserState, ValidatingParser};
    /// let mut parser = ValidatingParser::new(data, None);
    /// let mut validating_parsers = Vec::new();
    /// loop {
    ///     {
    ///         match *parser.read() {
    ///             ParserState::Error(_) |
    ///             ParserState::EndWasm => break,
    ///             ParserState::BeginFunctionBody {..} => (),
    ///             _ => continue
    ///         }
    ///     }
    ///     let reader = parser.create_validating_operator_parser();
    ///     validating_parsers.push(reader);
    /// }
    /// for (i, reader) in validating_parsers.iter_mut().enumerate() {
    ///     println!("Function {}", i);
    ///     while !reader.eof() {
    ///       let read = reader.next(&parser);
    ///       if let Ok(ref op) = read {
    ///           println!("  {:?}", op);
    ///       } else {
    ///           panic!("  Bad wasm code {:?}", read.err());
    ///       }
    ///     }
    /// }
    /// ```
    pub fn next(&mut self, resources: &WasmModuleResources) -> Result<Operator<'b>> {
        let op = self.reader.read_operator()?;
        let check = self.operator_validator.process_operator(&op, resources);
        if check.is_err() {
            return Err(BinaryReaderError {
                message: check.err().unwrap(),
                offset: self.func_body_offset + self.reader.current_position(),
            });
        }
        if let OperatorAction::EndFunction = check.ok().unwrap() {
            self.end_function = true;
            if !self.reader.eof() {
                return Err(BinaryReaderError {
                    message: "unexpected end of function",
                    offset: self.func_body_offset + self.reader.current_position(),
                });
            }
        }
        Ok(op)
    }
}

/// Test whether the given buffer contains a valid WebAssembly module,
/// analogous to WebAssembly.validate in the JS API.
pub fn validate(bytes: &[u8], config: Option<ValidatingParserConfig>) -> bool {
    let mut parser = ValidatingParser::new(bytes, config);
    loop {
        let state = parser.read();
        match *state {
            ParserState::EndWasm => return true,
            ParserState::Error(_) => return false,
            _ => (),
        }
    }
}

#[test]
fn test_validate() {
    assert!(validate(&[0x0, 0x61, 0x73, 0x6d, 0x1, 0x0, 0x0, 0x0], None));
    assert!(!validate(
        &[0x0, 0x61, 0x73, 0x6d, 0x2, 0x0, 0x0, 0x0],
        None
    ));
}
