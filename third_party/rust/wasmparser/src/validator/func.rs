use super::ModuleState;
use crate::operators_validator::{FunctionEnd, OperatorValidator};
use crate::{BinaryReaderError, Result, Type};
use crate::{Operator, WasmModuleResources, WasmTypeDef};
use std::sync::Arc;

/// Validation context for a WebAssembly function.
///
/// This structure is created by
/// [`Validator::code_section_entry`](crate::Validator::code_section_entry)
/// and is created per-function in a WebAssembly module. This structure is
/// suitable for sending to other threads while the original
/// [`Validator`](crate::Validator) continues processing other functions.
pub struct FuncValidator {
    pub(super) validator: OperatorValidator,
    pub(super) state: Arc<ModuleState>,
    pub(super) offset: usize,
    pub(super) eof_found: bool,
}

impl FuncValidator {
    /// Validates the next operator in a function.
    ///
    /// This functions is expected to be called once-per-operator in a
    /// WebAssembly function. Each operator's offset in the original binary and
    /// the operator itself are passed to this function.
    pub fn op(&mut self, offset: usize, operator: &Operator<'_>) -> Result<()> {
        self.offset = offset;
        let end = self
            .validator
            .process_operator(operator, &*self.state)
            .map_err(|e| e.set_offset(offset))?;
        match end {
            FunctionEnd::Yes => self.eof_found = true,
            FunctionEnd::No => {}
        }
        Ok(())
    }

    /// Function that must be called after the last opcode has been processed.
    ///
    /// This will validate that the function was properly terminated with the
    /// `end` opcode. If this function is not called then the function will not
    /// be properly validated.
    pub fn finish(&mut self) -> Result<()> {
        if !self.eof_found {
            return Err(BinaryReaderError::new(
                "end of function not found",
                self.offset,
            ));
        }
        Ok(())
    }
}

impl WasmModuleResources for ModuleState {
    type TypeDef = super::TypeDef;
    type TableType = crate::TableType;
    type MemoryType = crate::MemoryType;
    type GlobalType = crate::GlobalType;

    fn type_at(&self, at: u32) -> Option<&Self::TypeDef> {
        self.get_type(self.def(at)).map(|t| t.item)
    }

    fn table_at(&self, at: u32) -> Option<&Self::TableType> {
        self.get_table(self.def(at)).map(|t| &t.item)
    }

    fn memory_at(&self, at: u32) -> Option<&Self::MemoryType> {
        self.get_memory(self.def(at))
    }

    fn global_at(&self, at: u32) -> Option<&Self::GlobalType> {
        self.get_global(self.def(at)).map(|t| &t.item)
    }

    fn func_type_at(&self, at: u32) -> Option<&super::FuncType> {
        let ty = self.get_func_type_index(self.def(at))?;
        match self.get_type(ty)?.item {
            super::TypeDef::Func(f) => Some(f),
            _ => None,
        }
    }

    fn element_type_at(&self, at: u32) -> Option<Type> {
        self.element_types.get(at as usize).cloned()
    }

    fn element_count(&self) -> u32 {
        self.element_types.len() as u32
    }

    fn data_count(&self) -> u32 {
        self.data_count.unwrap_or(0)
    }

    fn is_function_referenced(&self, idx: u32) -> bool {
        self.function_references.contains(&idx)
    }
}

impl WasmTypeDef for super::TypeDef {
    type FuncType = crate::FuncType;

    fn as_func(&self) -> Option<&Self::FuncType> {
        match self {
            super::TypeDef::Func(f) => Some(f),
            _ => None,
        }
    }
}
