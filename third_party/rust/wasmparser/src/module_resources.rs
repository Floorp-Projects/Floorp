/* Copyright 2019 Mozilla Foundation
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

/// Types that qualify as Wasm types for validation purposes.
///
/// Must be comparable with `wasmparser` given Wasm types and
/// must be comparable to themselves.
pub trait WasmType: PartialEq<crate::Type> + PartialEq + Eq {
    /// Converts the custom type into a `wasmparser` known type.
    ///
    /// # Note
    ///
    /// This interface is required as bridge until transitioning is complete.
    fn to_parser_type(&self) -> crate::Type;
}

/// Types that qualify as Wasm function types for validation purposes.
pub trait WasmFuncType {
    /// A type that is comparable with Wasm types.
    type Type: WasmType;

    /// Returns the number of input types.
    fn len_inputs(&self) -> usize;
    /// Returns the number of output types.
    fn len_outputs(&self) -> usize;
    /// Returns the type at given index if any.
    ///
    /// # Note
    ///
    /// The returned type may be wrapped by the user crate and thus
    /// the actually returned type only has to be comparable to a Wasm type.
    fn input_at(&self, at: u32) -> Option<&Self::Type>;
    /// Returns the type at given index if any.
    ///
    /// # Note
    ///
    /// The returned type may be wrapped by the user crate and thus
    /// the actually returned type only has to be comparable to a Wasm type.
    fn output_at(&self, at: u32) -> Option<&Self::Type>;
}

/// Iterator over the inputs of a Wasm function type.
pub(crate) struct WasmFuncTypeInputs<'a, F, T>
where
    F: WasmFuncType<Type = T>,
    T: WasmType + 'a,
{
    /// The iterated-over function type.
    func_type: &'a F,
    /// The current starting index.
    start: u32,
    /// The current ending index.
    end: u32,
}

impl<'a, F, T> WasmFuncTypeInputs<'a, F, T>
where
    F: WasmFuncType<Type = T>,
    T: WasmType + 'a,
{
    fn new(func_type: &'a F) -> Self {
        Self {
            func_type,
            start: 0,
            end: func_type.len_inputs() as u32,
        }
    }
}

impl<'a, F, T> Iterator for WasmFuncTypeInputs<'a, F, T>
where
    F: WasmFuncType<Type = T>,
    T: WasmType + 'a,
{
    type Item = &'a T;

    fn next(&mut self) -> Option<Self::Item> {
        if self.start == self.end {
            return None;
        }
        let ty = self
            .func_type
            .input_at(self.start)
            // Expected since `self.start != self.end`.
            .expect("we expect to receive an input type at this point");
        self.start += 1;
        Some(ty)
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.len(), Some(self.len()))
    }
}

impl<'a, F, T> DoubleEndedIterator for WasmFuncTypeInputs<'a, F, T>
where
    F: WasmFuncType<Type = T>,
    T: WasmType + 'a,
{
    fn next_back(&mut self) -> Option<Self::Item> {
        if self.start == self.end {
            return None;
        }
        let ty = self
            .func_type
            .input_at(self.end)
            // Expected since `self.start != self.end`.
            .expect("we expect to receive an input type at this point");
        self.end -= 1;
        Some(ty)
    }
}

impl<'a, F, T> ExactSizeIterator for WasmFuncTypeInputs<'a, F, T>
where
    F: WasmFuncType<Type = T>,
    T: WasmType + 'a,
{
    fn len(&self) -> usize {
        (self.end as usize) - (self.start as usize)
    }
}

/// Iterator over the outputs of a Wasm function type.
pub(crate) struct WasmFuncTypeOutputs<'a, F, T>
where
    F: WasmFuncType<Type = T>,
    T: WasmType + 'a,
{
    /// The iterated-over function type.
    func_type: &'a F,
    /// The current starting index.
    start: u32,
    /// The current ending index.
    end: u32,
}

impl<'a, F, T> WasmFuncTypeOutputs<'a, F, T>
where
    F: WasmFuncType<Type = T>,
    T: WasmType + 'a,
{
    fn new(func_type: &'a F) -> Self {
        Self {
            func_type,
            start: 0,
            end: func_type.len_outputs() as u32,
        }
    }
}

impl<'a, F, T> Iterator for WasmFuncTypeOutputs<'a, F, T>
where
    F: WasmFuncType<Type = T>,
    T: WasmType + 'a,
{
    type Item = &'a T;

    fn next(&mut self) -> Option<Self::Item> {
        if self.start == self.end {
            return None;
        }
        let ty = self
            .func_type
            .output_at(self.start)
            // Expected since `self.start != self.end`.
            .expect("we expect to receive an input type at this point");
        self.start += 1;
        Some(ty)
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.len(), Some(self.len()))
    }
}

impl<'a, F, T> DoubleEndedIterator for WasmFuncTypeOutputs<'a, F, T>
where
    F: WasmFuncType<Type = T>,
    T: WasmType + 'a,
{
    fn next_back(&mut self) -> Option<Self::Item> {
        if self.start == self.end {
            return None;
        }
        let ty = self
            .func_type
            .output_at(self.end)
            // Expected since `self.start != self.end`.
            .expect("we expect to receive an input type at this point");
        self.end -= 1;
        Some(ty)
    }
}

impl<'a, F, T> ExactSizeIterator for WasmFuncTypeOutputs<'a, F, T>
where
    F: WasmFuncType<Type = T>,
    T: WasmType + 'a,
{
    fn len(&self) -> usize {
        (self.end as usize) - (self.start as usize)
    }
}

/// Returns an iterator over the input types of a Wasm function type.
pub(crate) fn wasm_func_type_inputs<'a, F, T>(func_type: &'a F) -> WasmFuncTypeInputs<'a, F, T>
where
    F: WasmFuncType<Type = T>,
    T: WasmType + 'a,
{
    WasmFuncTypeInputs::new(func_type)
}

/// Returns an iterator over the output types of a Wasm function type.
pub(crate) fn wasm_func_type_outputs<'a, F, T>(func_type: &'a F) -> WasmFuncTypeOutputs<'a, F, T>
where
    F: WasmFuncType<Type = T>,
    T: WasmType + 'a,
{
    WasmFuncTypeOutputs::new(func_type)
}

/// Types that qualify as Wasm table types for validation purposes.
pub trait WasmTableType {
    /// A type that is comparable with Wasm types.
    type Type: WasmType;

    /// Returns the element type of the table.
    fn element_type(&self) -> &Self::Type;
    /// Returns the initial limit of the table.
    fn initial_limit(&self) -> u32;
    /// Returns the maximum limit of the table if any.
    fn maximum_limit(&self) -> Option<u32>;
}

/// Types that qualify as Wasm memory types for validation purposes.
pub trait WasmMemoryType {
    /// Returns `true` if the linear memory is shared.
    fn is_shared(&self) -> bool;
    /// Returns the initial limit of the linear memory.
    fn initial_limit(&self) -> u32;
    /// Returns the maximum limit of the linear memory if any.
    fn maximum_limit(&self) -> Option<u32>;
}

/// Types that qualify as Wasm global types for validation purposes.
pub trait WasmGlobalType {
    /// A type that is comparable with Wasm types.
    type Type: WasmType;

    /// Returns `true` if the global variable is mutable.
    fn is_mutable(&self) -> bool;
    /// Returns the content type of the global variable.
    fn content_type(&self) -> &Self::Type;
}

/// Types  that qualify as Wasm valiation database.
///
/// # Note
///
/// The `wasmparser` crate provides a builtin validation framework but allows
/// users of this crate to also feed the parsed Wasm into their own data
/// structure while parsing and also validate at the same time without
/// the need of an additional parsing or validation step or copying data around.
pub trait WasmModuleResources {
    /// The function type used for validation.
    type FuncType: WasmFuncType;
    /// The table type used for validation.
    type TableType: WasmTableType;
    /// The memory type used for validation.
    type MemoryType: WasmMemoryType;
    /// The global type used for validation.
    type GlobalType: WasmGlobalType;

    /// Returns the type at given index.
    fn type_at(&self, at: u32) -> Option<&Self::FuncType>;
    /// Returns the table at given index if any.
    fn table_at(&self, at: u32) -> Option<&Self::TableType>;
    /// Returns the linear memory at given index.
    fn memory_at(&self, at: u32) -> Option<&Self::MemoryType>;
    /// Returns the global variable at given index.
    fn global_at(&self, at: u32) -> Option<&Self::GlobalType>;
    /// Returns the function signature ID at given index.
    fn func_type_id_at(&self, at: u32) -> Option<u32>;

    /// Returns the number of elements.
    fn element_count(&self) -> u32;
    /// Returns the number of bytes in the Wasm data section.
    fn data_count(&self) -> u32;
}

impl WasmType for crate::Type {
    fn to_parser_type(&self) -> crate::Type {
        *self
    }
}

impl WasmFuncType for crate::FuncType {
    type Type = crate::Type;

    fn len_inputs(&self) -> usize {
        self.params.len()
    }

    fn len_outputs(&self) -> usize {
        self.returns.len()
    }

    fn input_at(&self, at: u32) -> Option<&Self::Type> {
        self.params.get(at as usize)
    }

    fn output_at(&self, at: u32) -> Option<&Self::Type> {
        self.returns.get(at as usize)
    }
}

impl WasmGlobalType for crate::GlobalType {
    type Type = crate::Type;

    fn is_mutable(&self) -> bool {
        self.mutable
    }

    fn content_type(&self) -> &Self::Type {
        &self.content_type
    }
}

impl WasmTableType for crate::TableType {
    type Type = crate::Type;

    fn element_type(&self) -> &Self::Type {
        &self.element_type
    }

    fn initial_limit(&self) -> u32 {
        self.limits.initial
    }

    fn maximum_limit(&self) -> Option<u32> {
        self.limits.maximum
    }
}

impl WasmMemoryType for crate::MemoryType {
    fn is_shared(&self) -> bool {
        self.shared
    }

    fn initial_limit(&self) -> u32 {
        self.limits.initial
    }
    fn maximum_limit(&self) -> Option<u32> {
        self.limits.maximum
    }
}
