use crate::operators_validator::OperatorValidator;
use crate::{BinaryReader, Result, Type};
use crate::{FunctionBody, Operator, WasmFeatures, WasmModuleResources};

/// Validation context for a WebAssembly function.
///
/// This structure is created by
/// [`Validator::code_section_entry`](crate::Validator::code_section_entry)
/// and is created per-function in a WebAssembly module. This structure is
/// suitable for sending to other threads while the original
/// [`Validator`](crate::Validator) continues processing other functions.
pub struct FuncValidator<T> {
    validator: OperatorValidator,
    resources: T,
}

impl<T: WasmModuleResources> FuncValidator<T> {
    /// Creates a new `FuncValidator`.
    ///
    /// The returned `FuncValidator` can be used to validate a function with
    /// the type `ty` specified. The `resources` indicate what the containing
    /// module has for the function to use, and the `features` configure what
    /// WebAssembly proposals are enabled for this function.
    ///
    /// The returned validator can be used to then parse a [`FunctionBody`], for
    /// example, to read locals and validate operators.
    pub fn new(
        ty: u32,
        offset: usize,
        resources: T,
        features: &WasmFeatures,
    ) -> Result<FuncValidator<T>> {
        Ok(FuncValidator {
            validator: OperatorValidator::new(ty, offset, features, &resources)?,
            resources,
        })
    }

    /// Convenience function to validate an entire function's body.
    ///
    /// You may not end up using this in final implementations because you'll
    /// often want to interleave validation with parsing.
    pub fn validate(&mut self, body: &FunctionBody<'_>) -> Result<()> {
        let mut reader = body.get_binary_reader();
        self.read_locals(&mut reader)?;
        while !reader.eof() {
            let pos = reader.original_position();
            let op = reader.read_operator()?;
            self.op(pos, &op)?;
        }
        self.finish(reader.original_position())
    }

    /// Reads the local defintions from the given `BinaryReader`, often sourced
    /// from a `FunctionBody`.
    ///
    /// This function will automatically advance the `BinaryReader` forward,
    /// leaving reading operators up to the caller afterwards.
    pub fn read_locals(&mut self, reader: &mut BinaryReader<'_>) -> Result<()> {
        for _ in 0..reader.read_var_u32()? {
            let offset = reader.original_position();
            let cnt = reader.read_var_u32()?;
            let ty = reader.read_type()?;
            self.define_locals(offset, cnt, ty)?;
        }
        Ok(())
    }

    /// Defines locals into this validator.
    ///
    /// This should be used if the application is already reading local
    /// definitions and there's no need to re-parse the function again.
    pub fn define_locals(&mut self, offset: usize, count: u32, ty: Type) -> Result<()> {
        self.validator.define_locals(offset, count, ty)
    }

    /// Validates the next operator in a function.
    ///
    /// This functions is expected to be called once-per-operator in a
    /// WebAssembly function. Each operator's offset in the original binary and
    /// the operator itself are passed to this function to provide more useful
    /// error messages.
    pub fn op(&mut self, offset: usize, operator: &Operator<'_>) -> Result<()> {
        self.validator
            .process_operator(operator, &self.resources)
            .map_err(|e| e.set_offset(offset))?;
        Ok(())
    }

    /// Function that must be called after the last opcode has been processed.
    ///
    /// This will validate that the function was properly terminated with the
    /// `end` opcode. If this function is not called then the function will not
    /// be properly validated.
    ///
    /// The `offset` provided to this function will be used as a position for an
    /// error if validation fails.
    pub fn finish(&mut self, offset: usize) -> Result<()> {
        self.validator.finish().map_err(|e| e.set_offset(offset))?;
        Ok(())
    }

    /// Returns the underlying module resources that this validator is using.
    pub fn resources(&self) -> &T {
        &self.resources
    }
}
