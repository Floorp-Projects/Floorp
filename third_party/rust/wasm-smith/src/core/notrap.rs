use crate::core::*;
use wasm_encoder::{BlockType, ConstExpr, Instruction, ValType};

const WASM_PAGE_SIZE: u64 = 65_536;

#[derive(Debug)]
pub struct NotSupported<'a> {
    opcode: wasm_encoder::Instruction<'a>,
}

impl std::error::Error for NotSupported<'_> {}
impl std::fmt::Display for NotSupported<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "Opcode not supported for no-trapping mode: {:?}",
            self.opcode
        )
    }
}

impl Module {
    /// Ensure that this generated module will never trap.
    ///
    /// This will take a number of approaches to avoid traps, such as
    ///
    /// * mask loads' and stores' addresses to the associated memory's size
    ///
    /// * mask `table.get`s' and `table.set`'s index to the associated table's size
    ///
    /// * ensure that a divisor is never zero
    ///
    /// * replace `unreachable`s with dummy-value `return`s
    ///
    /// * Masking data and element segments' offsets to be in bounds of their
    /// associated tables or memories
    pub fn no_traps(&mut self) -> std::result::Result<(), NotSupported> {
        self.no_trapping_segments();

        let import_count = self
            .imports
            .iter()
            .filter(|imp| match imp.entity_type {
                EntityType::Func(_, _) => true,
                _ => false,
            })
            .count();
        for (i, code) in self.code.iter_mut().enumerate() {
            let this_func_ty = &self.funcs[i + import_count].1;
            let mut new_insts = vec![];

            let insts = match &mut code.instructions {
                Instructions::Generated(is) => std::mem::replace(is, vec![]),
                Instructions::Arbitrary(_) => unreachable!(),
            };

            for inst in insts {
                match inst {
                    // Replace `unreachable` with an early return of dummy values.
                    //
                    // We *could* instead abstractly interpret all these
                    // instructions and maintain a stack of types (the way the
                    // validation algorithm does) and insert dummy values
                    // whenever an instruction expects a value of type `T` but
                    // there is an `unreachable` on the stack. This would allow
                    // us to keep executing the rest of the code following the
                    // `unreachable`, but also is a ton more work, and it isn't
                    // clear that it would pay for itself.
                    Instruction::Unreachable => {
                        for ty in &this_func_ty.results {
                            new_insts.push(dummy_value_inst(*ty));
                        }
                        new_insts.push(Instruction::Return);
                    }

                    // We have no way to reflect, at run time, on a `funcref` in
                    // the `i`th slot in a table and dynamically avoid trapping
                    // `call_indirect`s. Therefore, we can't emit *any*
                    // `call_indirect` instructions. Instead, we consume the
                    // arguments and generate dummy results.
                    Instruction::CallIndirect { ty, table: _ } => {
                        // When we can, avoid emitting `drop`s to consume the
                        // arguments when possible, since dead code isn't
                        // usually an interesting thing to give to a Wasm
                        // compiler. Instead, prefer writing them to the first
                        // page of the first memory if possible.
                        let callee_func_ty = match &self.types[ty as usize] {
                            Type::Func(f) => f,
                        };
                        let can_store_args_to_memory = callee_func_ty.params.len()
                            < usize::try_from(WASM_PAGE_SIZE).unwrap()
                            && self.memories.get(0).map_or(false, |m| m.minimum > 0);
                        let memory_64 = self.memories.get(0).map_or(false, |m| m.memory64);
                        let address = if memory_64 {
                            Instruction::I64Const(0)
                        } else {
                            Instruction::I32Const(0)
                        };
                        let memarg = wasm_encoder::MemArg {
                            offset: 0,
                            align: 0,
                            memory_index: 0,
                        };

                        // handle table index if we are able, otherwise drop it
                        if can_store_args_to_memory {
                            let val_to_store =
                                u32::try_from(this_func_ty.params.len() + code.locals.len())
                                    .unwrap();
                            code.locals.push(ValType::I32);
                            new_insts.push(Instruction::LocalSet(val_to_store));
                            new_insts.push(address.clone());
                            new_insts.push(Instruction::LocalGet(val_to_store));
                            new_insts.push(Instruction::I32Store(memarg));
                        } else {
                            new_insts.push(Instruction::Drop);
                        }

                        for ty in callee_func_ty.params.iter().rev() {
                            let val_to_store =
                                u32::try_from(this_func_ty.params.len() + code.locals.len())
                                    .unwrap();

                            if let ValType::I32
                            | ValType::I64
                            | ValType::F32
                            | ValType::F64
                            | ValType::V128 = ty
                            {
                                if can_store_args_to_memory {
                                    code.locals.push(*ty);
                                    new_insts.push(Instruction::LocalSet(val_to_store));
                                }
                            }
                            match ty {
                                ValType::I32 if can_store_args_to_memory => {
                                    new_insts.push(address.clone());
                                    new_insts.push(Instruction::LocalGet(val_to_store));
                                    new_insts.push(Instruction::I32Store(memarg));
                                }
                                ValType::I64 if can_store_args_to_memory => {
                                    new_insts.push(address.clone());
                                    new_insts.push(Instruction::LocalGet(val_to_store));
                                    new_insts.push(Instruction::I64Store(memarg));
                                }
                                ValType::F32 if can_store_args_to_memory => {
                                    new_insts.push(address.clone());
                                    new_insts.push(Instruction::LocalGet(val_to_store));
                                    new_insts.push(Instruction::F32Store(memarg));
                                }
                                ValType::F64 if can_store_args_to_memory => {
                                    new_insts.push(address.clone());
                                    new_insts.push(Instruction::LocalGet(val_to_store));
                                    new_insts.push(Instruction::F64Store(memarg));
                                }
                                ValType::V128 if can_store_args_to_memory => {
                                    new_insts.push(address.clone());
                                    new_insts.push(Instruction::LocalGet(val_to_store));
                                    new_insts.push(Instruction::V128Store { memarg });
                                }
                                _ => {
                                    new_insts.push(Instruction::Drop);
                                }
                            }
                        }

                        for ty in &callee_func_ty.results {
                            new_insts.push(dummy_value_inst(*ty));
                        }
                    }

                    // For loads, we dynamically check whether the load will
                    // trap, and if it will then we generate a dummy value to
                    // use instead.
                    Instruction::I32Load(memarg)
                    | Instruction::I64Load(memarg)
                    | Instruction::F32Load(memarg)
                    | Instruction::F64Load(memarg)
                    | Instruction::I32Load8_S(memarg)
                    | Instruction::I32Load8_U(memarg)
                    | Instruction::I32Load16_S(memarg)
                    | Instruction::I32Load16_U(memarg)
                    | Instruction::I64Load8_S(memarg)
                    | Instruction::I64Load8_U(memarg)
                    | Instruction::I64Load16_S(memarg)
                    | Instruction::I64Load16_U(memarg)
                    | Instruction::I64Load32_S(memarg)
                    | Instruction::I64Load32_U(memarg)
                    | Instruction::V128Load { memarg }
                    | Instruction::V128Load8x8S { memarg }
                    | Instruction::V128Load8x8U { memarg }
                    | Instruction::V128Load16x4S { memarg }
                    | Instruction::V128Load16x4U { memarg }
                    | Instruction::V128Load32x2S { memarg }
                    | Instruction::V128Load32x2U { memarg }
                    | Instruction::V128Load8Splat { memarg }
                    | Instruction::V128Load16Splat { memarg }
                    | Instruction::V128Load32Splat { memarg }
                    | Instruction::V128Load64Splat { memarg }
                    | Instruction::V128Load32Zero { memarg }
                    | Instruction::V128Load64Zero { memarg } => {
                        let memory = &self.memories[memarg.memory_index as usize];
                        let address_type = if memory.memory64 {
                            ValType::I64
                        } else {
                            ValType::I32
                        };
                        // Add a temporary local to hold this load's address.
                        let address_local =
                            u32::try_from(this_func_ty.params.len() + code.locals.len()).unwrap();
                        code.locals.push(address_type);

                        // Add a temporary local to hold the result of this load.
                        let load_type = type_of_memory_access(&inst);
                        let result_local =
                            u32::try_from(this_func_ty.params.len() + code.locals.len()).unwrap();
                        code.locals.push(load_type);

                        // [address:address_type]
                        new_insts.push(Instruction::LocalSet(address_local));
                        // []
                        new_insts.push(Instruction::Block(wasm_encoder::BlockType::Empty));
                        {
                            // []
                            new_insts.push(Instruction::Block(wasm_encoder::BlockType::Empty));
                            {
                                // []
                                new_insts.push(Instruction::MemorySize(memarg.memory_index));
                                // [mem_size_in_pages:address_type]
                                new_insts.push(int_const_inst(address_type, 65_536));
                                // [mem_size_in_pages:address_type wasm_page_size:address_type]
                                new_insts.push(int_mul_inst(address_type));
                                // [mem_size_in_bytes:address_type]
                                new_insts.push(int_const_inst(
                                    address_type,
                                    (memarg.offset + size_of_type_in_memory(load_type)) as i64,
                                ));
                                // [mem_size_in_bytes:address_type offset_and_size:address_type]
                                new_insts.push(Instruction::LocalGet(address_local));
                                // [mem_size_in_bytes:address_type offset_and_size:address_type address:address_type]
                                new_insts.push(int_add_inst(address_type));
                                // [mem_size_in_bytes:address_type highest_byte_accessed:address_type]
                                new_insts.push(int_le_u_inst(address_type));
                                // [load_will_trap:i32]
                                new_insts.push(Instruction::BrIf(0));
                                // []
                                new_insts.push(Instruction::LocalGet(address_local));
                                // [address:address_type]
                                new_insts.push(inst);
                                // [result:load_type]
                                new_insts.push(Instruction::LocalSet(result_local));
                                // []
                                new_insts.push(Instruction::Br(1));
                                // <unreachable>
                            }
                            // []
                            new_insts.push(Instruction::End);
                            // []
                            new_insts.push(dummy_value_inst(load_type));
                            // [dummy_value:load_type]
                            new_insts.push(Instruction::LocalSet(result_local));
                            // []
                        }
                        // []
                        new_insts.push(Instruction::End);
                        // []
                        new_insts.push(Instruction::LocalGet(result_local));
                        // [result:load_type]
                    }

                    // Stores are similar to loads: we check whether the store
                    // will trap, and if it will then we just drop the value.
                    Instruction::I32Store(memarg)
                    | Instruction::I64Store(memarg)
                    | Instruction::F32Store(memarg)
                    | Instruction::F64Store(memarg)
                    | Instruction::I32Store8(memarg)
                    | Instruction::I32Store16(memarg)
                    | Instruction::I64Store8(memarg)
                    | Instruction::I64Store16(memarg)
                    | Instruction::I64Store32(memarg)
                    | Instruction::V128Store { memarg } => {
                        let memory = &self.memories[memarg.memory_index as usize];
                        let address_type = if memory.memory64 {
                            ValType::I64
                        } else {
                            ValType::I32
                        };

                        // Add a temporary local to hold this store's address.
                        let address_local =
                            u32::try_from(this_func_ty.params.len() + code.locals.len()).unwrap();
                        code.locals.push(address_type);

                        // Add a temporary local to hold the value to store.
                        let store_type = type_of_memory_access(&inst);
                        let value_local =
                            u32::try_from(this_func_ty.params.len() + code.locals.len()).unwrap();
                        code.locals.push(store_type);

                        // [address:address_type value:store_type]
                        new_insts.push(Instruction::LocalSet(value_local));
                        // [address:address_type]
                        new_insts.push(Instruction::LocalSet(address_local));
                        // []
                        new_insts.push(Instruction::MemorySize(memarg.memory_index));
                        // [mem_size_in_pages:address_type]
                        new_insts.push(int_const_inst(address_type, 65_536));
                        // [mem_size_in_pages:address_type wasm_page_size:address_type]
                        new_insts.push(int_mul_inst(address_type));
                        // [mem_size_in_bytes:address_type]
                        new_insts.push(int_const_inst(
                            address_type,
                            (memarg.offset + size_of_type_in_memory(store_type)) as i64,
                        ));
                        // [mem_size_in_bytes:address_type offset_and_size:address_type]
                        new_insts.push(Instruction::LocalGet(address_local));
                        // [mem_size_in_bytes:address_type offset_and_size:address_type address:address_type]
                        new_insts.push(int_add_inst(address_type));
                        // [mem_size_in_bytes:address_type highest_byte_accessed:address_type]
                        new_insts.push(int_le_u_inst(address_type));
                        // [store_will_trap:i32]
                        new_insts.push(Instruction::If(BlockType::Empty));
                        new_insts.push(Instruction::Else);
                        {
                            // []
                            new_insts.push(Instruction::LocalGet(address_local));
                            // [address:address_type]
                            new_insts.push(Instruction::LocalGet(value_local));
                            // [address:address_type value:store_type]
                            new_insts.push(inst);
                            // []
                        }
                        // []
                        new_insts.push(Instruction::End);
                    }

                    Instruction::V128Load8Lane { memarg: _, lane: _ }
                    | Instruction::V128Load16Lane { memarg: _, lane: _ }
                    | Instruction::V128Load32Lane { memarg: _, lane: _ }
                    | Instruction::V128Load64Lane { memarg: _, lane: _ }
                    | Instruction::V128Store8Lane { memarg: _, lane: _ }
                    | Instruction::V128Store16Lane { memarg: _, lane: _ }
                    | Instruction::V128Store32Lane { memarg: _, lane: _ }
                    | Instruction::V128Store64Lane { memarg: _, lane: _ } => {
                        return Err(NotSupported { opcode: inst })
                    }

                    Instruction::MemoryCopy { src: _, dst: _ } => todo!(),
                    Instruction::MemoryFill(_) => todo!(),
                    Instruction::MemoryInit { mem: _, data: _ } => todo!(),

                    // Unsigned integer division and remainder will trap when
                    // the divisor is 0. To avoid the trap, we will set any 0
                    // divisors to 1 prior to the operation.
                    //
                    // The code below is equivalent to this expression:
                    //
                    //     local.set $temp_divisor
                    //     (select (i32.eqz (local.get $temp_divisor) (i32.const 1) (local.get $temp_divisor))
                    Instruction::I32RemU
                    | Instruction::I64RemU
                    | Instruction::I64DivU
                    | Instruction::I32DivU => {
                        let op_type = type_of_integer_operation(&inst);
                        let temp_divisor =
                            u32::try_from(this_func_ty.params.len() + code.locals.len()).unwrap();
                        code.locals.push(op_type);

                        // [dividend:op_type divisor:op_type]
                        new_insts.push(Instruction::LocalSet(temp_divisor));
                        // [dividend:op_type]
                        new_insts.push(int_const_inst(op_type, 1));
                        // [dividend:op_type 1:op_type]
                        new_insts.push(Instruction::LocalGet(temp_divisor));
                        // [dividend:op_type 1:op_type divisor:op_type]
                        new_insts.push(Instruction::LocalGet(temp_divisor));
                        // [dividend:op_type 1:op_type divisor:op_type divisor:op_type]
                        new_insts.push(eqz_inst(op_type));
                        // [dividend:op_type 1:op_type divisor:op_type is_zero:i32]
                        new_insts.push(Instruction::Select);
                        // [dividend:op_type divisor:op_type]
                        new_insts.push(inst);
                        // [result:op_type]
                    }

                    // Signed division and remainder will trap in the following instances:
                    //     - The divisor is 0
                    //     - The result of the division is 2^(n-1)
                    Instruction::I32DivS
                    | Instruction::I32RemS
                    | Instruction::I64DivS
                    | Instruction::I64RemS => {
                        // If divisor is 0, replace with 1
                        let op_type = type_of_integer_operation(&inst);
                        let temp_divisor =
                            u32::try_from(this_func_ty.params.len() + code.locals.len()).unwrap();
                        code.locals.push(op_type);

                        // [dividend:op_type divisor:op_type]
                        new_insts.push(Instruction::LocalSet(temp_divisor));
                        // [dividend:op_type]
                        new_insts.push(int_const_inst(op_type, 1));
                        // [dividend:op_type 1:op_type]
                        new_insts.push(Instruction::LocalGet(temp_divisor));
                        // [dividend:op_type 1:op_type divisor:op_type]
                        new_insts.push(Instruction::LocalGet(temp_divisor));
                        // [dividend:op_type 1:op_type divisor:op_type divisor:op_type]
                        new_insts.push(eqz_inst(op_type));
                        // [dividend:op_type 1:op_type divisor:op_type is_zero:i32]
                        new_insts.push(Instruction::Select);
                        // [dividend:op_type divisor:op_type]

                        // If dividend and divisor are -int.max and -1, replace
                        // divisor with 1.
                        let temp_dividend =
                            u32::try_from(this_func_ty.params.len() + code.locals.len()).unwrap();
                        code.locals.push(op_type);
                        new_insts.push(Instruction::LocalSet(temp_divisor));
                        // [dividend:op_type]
                        new_insts.push(Instruction::LocalSet(temp_dividend));
                        // []
                        new_insts.push(Instruction::Block(wasm_encoder::BlockType::Empty));
                        {
                            new_insts.push(Instruction::Block(wasm_encoder::BlockType::Empty));
                            {
                                // []
                                new_insts.push(Instruction::LocalGet(temp_dividend));
                                // [dividend:op_type]
                                new_insts.push(Instruction::LocalGet(temp_divisor));
                                // [dividend:op_type divisor:op_type]
                                new_insts.push(Instruction::LocalSet(temp_divisor));
                                // [dividend:op_type]
                                new_insts.push(Instruction::LocalTee(temp_dividend));
                                // [dividend:op_type]
                                new_insts.push(int_min_const_inst(op_type));
                                // [dividend:op_type int_min:op_type]
                                new_insts.push(int_ne_inst(op_type));
                                // [not_int_min:i32]
                                new_insts.push(Instruction::BrIf(0));
                                // []
                                new_insts.push(Instruction::LocalGet(temp_divisor));
                                // [divisor:op_type]
                                new_insts.push(int_const_inst(op_type, -1));
                                // [divisor:op_type -1:op_type]
                                new_insts.push(int_ne_inst(op_type));
                                // [not_neg_one:i32]
                                new_insts.push(Instruction::BrIf(0));
                                // []
                                new_insts.push(int_const_inst(op_type, 1));
                                // [divisor:op_type]
                                new_insts.push(Instruction::LocalSet(temp_divisor));
                                // []
                                new_insts.push(Instruction::Br(1));
                            }
                            // []
                            new_insts.push(Instruction::End);
                        }
                        // []
                        new_insts.push(Instruction::End);
                        // []
                        new_insts.push(Instruction::LocalGet(temp_dividend));
                        // [dividend:op_type]
                        new_insts.push(Instruction::LocalGet(temp_divisor));
                        // [dividend:op_type divisor:op_type]
                        new_insts.push(inst);
                    }

                    Instruction::I32TruncF32S
                    | Instruction::I32TruncF32U
                    | Instruction::I32TruncF64S
                    | Instruction::I32TruncF64U
                    | Instruction::I64TruncF32S
                    | Instruction::I64TruncF32U
                    | Instruction::I64TruncF64S
                    | Instruction::I64TruncF64U => {
                        // If NaN or ±inf, replace with dummy value
                        let conv_type = type_of_float_conversion(&inst);
                        let temp_float =
                            u32::try_from(this_func_ty.params.len() + code.locals.len()).unwrap();
                        code.locals.push(conv_type);

                        // [input:conv_type]
                        new_insts.push(Instruction::LocalTee(temp_float));
                        // [input:conv_type]
                        new_insts.push(flt_nan_const_inst(conv_type));
                        // [input:conv_type NaN:conv_type]
                        new_insts.push(eq_inst(conv_type));
                        // [is_nan:i32]
                        new_insts.push(Instruction::LocalGet(temp_float));
                        // [is_nan:i32 input:conv_type]
                        new_insts.push(flt_inf_const_inst(conv_type));
                        // [is_nan:i32 input:conv_type inf:conv_type]
                        new_insts.push(eq_inst(conv_type));
                        // [is_nan:i32 is_inf:i32]
                        new_insts.push(Instruction::LocalGet(temp_float));
                        // [is_nan:i32 is_inf:i32 input:conv_type]
                        new_insts.push(flt_neg_inf_const_inst(conv_type));
                        // [is_nan:i32 is_inf:i32 input:conv_type neg_inf:conv_type]
                        new_insts.push(eq_inst(conv_type));
                        // [is_nan:i32 is_inf:i32 is_neg_inf:i32]
                        new_insts.push(Instruction::I32Or);
                        // [is_nan:i32 is_±inf:i32]
                        new_insts.push(Instruction::I32Or);
                        // [is_nan_or_inf:i32]
                        new_insts.push(Instruction::If(BlockType::Empty));
                        {
                            // []
                            new_insts.push(dummy_value_inst(conv_type));
                            // [0:conv_type]
                            new_insts.push(Instruction::LocalSet(temp_float));
                            // []
                        }
                        new_insts.push(Instruction::End);
                        // []
                        new_insts.push(Instruction::LocalGet(temp_float));

                        // [input_or_0:conv_type]
                        new_insts.push(inst);
                    }
                    Instruction::TableFill { table: _ } => todo!(),
                    Instruction::TableSet { table: _ } => todo!(),
                    Instruction::TableGet { table: _ } => todo!(),
                    Instruction::TableInit {
                        segment: _,
                        table: _,
                    } => todo!(),
                    Instruction::TableCopy { src: _, dst: _ } => todo!(),

                    // None of the other instructions can trap, so just copy them over.
                    inst => new_insts.push(inst),
                }
            }

            code.instructions = Instructions::Generated(new_insts);
        }
        Ok(())
    }

    /// Mask data and element segments' offsets to be in bounds of their
    /// associated tables and memories.
    fn no_trapping_segments(&mut self) {
        for data in &mut self.data {
            match &mut data.kind {
                DataSegmentKind::Passive => continue,
                DataSegmentKind::Active {
                    memory_index,
                    offset,
                } => {
                    let mem = &mut self.memories[usize::try_from(*memory_index).unwrap()];

                    // Ensure that all memories have at least one
                    // page. Otherwise, if we had a zero-minimum memory, then we
                    // wouldn't be able to mask the initializers to be
                    // definitely in-bounds.
                    mem.minimum = std::cmp::max(1, mem.minimum);
                    mem.maximum = mem.maximum.map(|n| std::cmp::max(mem.minimum, n));

                    // Make sure that the data segment can fit into the memory.
                    data.init
                        .truncate(usize::try_from(mem.minimum * WASM_PAGE_SIZE).unwrap());
                    let data_len = data.init.len() as u64;

                    match offset {
                        Offset::Const64(n) => {
                            let n = *n as u64;
                            let n = n
                                .checked_rem(mem.minimum * WASM_PAGE_SIZE - data_len)
                                .unwrap_or(0);
                            *offset = Offset::Const64(n as i64);
                        }
                        Offset::Const32(n) => {
                            let n = *n as u64;
                            let n = n
                                .checked_rem(mem.minimum * WASM_PAGE_SIZE - data_len)
                                .unwrap_or(0);
                            let n = u32::try_from(n).unwrap();
                            *offset = Offset::Const32(n as i32);
                        }
                        Offset::Global(_) => *offset = Offset::Const32(0),
                        _ => unreachable!("Unexpected instruction: {:?}", offset),
                    }
                }
            }
        }

        for elem in &mut self.elems {
            match &mut elem.kind {
                ElementKind::Passive | ElementKind::Declared => continue,
                ElementKind::Active { table, offset } => {
                    let table = table.unwrap_or(0);
                    let table = usize::try_from(table).unwrap();
                    let table = &mut self.tables[table];

                    // Ensure that we have room for at least one element. See
                    // comment above.
                    table.minimum = std::cmp::max(1, table.minimum);
                    table.maximum = table.maximum.map(|n| std::cmp::max(table.minimum, n));

                    // Make sure that the element segment can fit into the
                    // table.
                    let elem_len = match &mut elem.items {
                        Elements::Functions(fs) => {
                            fs.truncate(usize::try_from(table.minimum).unwrap());
                            u32::try_from(fs.len()).unwrap()
                        }
                        Elements::Expressions(es) => {
                            es.truncate(usize::try_from(table.minimum).unwrap());
                            u32::try_from(es.len()).unwrap()
                        }
                    };

                    match offset {
                        Offset::Const32(n) => {
                            let n = *n as u32;
                            let n = n.checked_rem(table.minimum - elem_len).unwrap_or(0);
                            *offset = Offset::Const32(n as i32);
                        }
                        Offset::Global(_) => {
                            *offset = Offset::Const32(0);
                        }
                        _ => unreachable!(),
                    }
                }
            }
        }
    }
}

fn dummy_value_inst<'a>(ty: ValType) -> Instruction<'a> {
    match ty {
        ValType::I32 => Instruction::I32Const(0),
        ValType::I64 => Instruction::I64Const(0),
        ValType::F32 => Instruction::F32Const(0.0),
        ValType::F64 => Instruction::F64Const(0.0),
        ValType::V128 => Instruction::V128Const(0),
        ValType::FuncRef | ValType::ExternRef => Instruction::RefNull(ty),
    }
}

fn eq_inst<'a>(ty: ValType) -> Instruction<'a> {
    match ty {
        ValType::F32 => Instruction::F32Eq,
        ValType::F64 => Instruction::F64Eq,
        ValType::I32 => Instruction::I32Eq,
        ValType::I64 => Instruction::I64Eq,
        _ => panic!("not a numeric type"),
    }
}

fn eqz_inst<'a>(ty: ValType) -> Instruction<'a> {
    match ty {
        ValType::I32 => Instruction::I32Eqz,
        ValType::I64 => Instruction::I64Eqz,
        _ => panic!("not an integer type"),
    }
}

fn type_of_integer_operation(inst: &Instruction) -> ValType {
    match inst {
        Instruction::I32DivU
        | Instruction::I32DivS
        | Instruction::I32RemU
        | Instruction::I32RemS => ValType::I32,
        Instruction::I64RemU
        | Instruction::I64DivU
        | Instruction::I64DivS
        | Instruction::I64RemS => ValType::I64,
        _ => panic!("not integer division or remainder"),
    }
}

fn type_of_float_conversion(inst: &Instruction) -> ValType {
    match inst {
        Instruction::I32TruncF32S
        | Instruction::I32TruncF32U
        | Instruction::I64TruncF32S
        | Instruction::I64TruncF32U => ValType::F32,
        Instruction::I32TruncF64S
        | Instruction::I32TruncF64U
        | Instruction::I64TruncF64S
        | Instruction::I64TruncF64U => ValType::F64,
        _ => panic!("not a float -> integer conversion"),
    }
}

fn type_of_memory_access(inst: &Instruction) -> ValType {
    match inst {
        Instruction::I32Load(_)
        | Instruction::I32Load8_S(_)
        | Instruction::I32Load8_U(_)
        | Instruction::I32Load16_S(_)
        | Instruction::I32Load16_U(_)
        | Instruction::I32Store(_)
        | Instruction::I32Store8(_)
        | Instruction::I32Store16(_) => ValType::I32,

        Instruction::I64Load(_)
        | Instruction::I64Load8_S(_)
        | Instruction::I64Load8_U(_)
        | Instruction::I64Load16_S(_)
        | Instruction::I64Load16_U(_)
        | Instruction::I64Load32_S(_)
        | Instruction::I64Load32_U(_)
        | Instruction::I64Store(_)
        | Instruction::I64Store8(_)
        | Instruction::I64Store16(_)
        | Instruction::I64Store32(_) => ValType::I64,

        Instruction::F32Load(_) | Instruction::F32Store(_) => ValType::F32,

        Instruction::F64Load(_) | Instruction::F64Store(_) => ValType::F64,

        Instruction::V128Load { .. }
        | Instruction::V128Load8x8S { .. }
        | Instruction::V128Load8x8U { .. }
        | Instruction::V128Load16x4S { .. }
        | Instruction::V128Load16x4U { .. }
        | Instruction::V128Load32x2S { .. }
        | Instruction::V128Load32x2U { .. }
        | Instruction::V128Load8Splat { .. }
        | Instruction::V128Load16Splat { .. }
        | Instruction::V128Load32Splat { .. }
        | Instruction::V128Load64Splat { .. }
        | Instruction::V128Load32Zero { .. }
        | Instruction::V128Load64Zero { .. }
        | Instruction::V128Store { .. } => ValType::V128,

        _ => panic!("not a memory access instruction"),
    }
}

fn int_min_const_inst<'a>(ty: ValType) -> Instruction<'a> {
    match ty {
        ValType::I32 => Instruction::I32Const(i32::MIN),
        ValType::I64 => Instruction::I64Const(i64::MIN),
        _ => panic!("not an int type"),
    }
}

fn int_const_inst<'a>(ty: ValType, x: i64) -> Instruction<'a> {
    match ty {
        ValType::I32 => Instruction::I32Const(x as i32),
        ValType::I64 => Instruction::I64Const(x),
        _ => panic!("not an int type"),
    }
}

fn int_mul_inst<'a>(ty: ValType) -> Instruction<'a> {
    match ty {
        ValType::I32 => Instruction::I32Mul,
        ValType::I64 => Instruction::I64Mul,
        _ => panic!("not an int type"),
    }
}

fn int_add_inst<'a>(ty: ValType) -> Instruction<'a> {
    match ty {
        ValType::I32 => Instruction::I32Add,
        ValType::I64 => Instruction::I64Add,
        _ => panic!("not an int type"),
    }
}

fn int_le_u_inst<'a>(ty: ValType) -> Instruction<'a> {
    match ty {
        ValType::I32 => Instruction::I32LeU,
        ValType::I64 => Instruction::I64LeU,
        _ => panic!("not an int type"),
    }
}

fn int_ne_inst<'a>(ty: ValType) -> Instruction<'a> {
    match ty {
        ValType::I32 => Instruction::I32Ne,
        ValType::I64 => Instruction::I64Ne,
        _ => panic!("not an int type"),
    }
}

fn flt_inf_const_inst<'a>(ty: ValType) -> Instruction<'a> {
    match ty {
        ValType::F32 => Instruction::F32Const(f32::INFINITY),
        ValType::F64 => Instruction::F64Const(f64::INFINITY),
        _ => panic!("not a float type"),
    }
}

fn flt_neg_inf_const_inst<'a>(ty: ValType) -> Instruction<'a> {
    match ty {
        ValType::F32 => Instruction::F32Const(f32::NEG_INFINITY),
        ValType::F64 => Instruction::F64Const(f64::NEG_INFINITY),
        _ => panic!("not a float type"),
    }
}

fn flt_nan_const_inst<'a>(ty: ValType) -> Instruction<'a> {
    match ty {
        ValType::F32 => Instruction::F32Const(f32::NAN),
        ValType::F64 => Instruction::F64Const(f64::NAN),
        _ => panic!("not a float type"),
    }
}

fn size_of_type_in_memory(ty: ValType) -> u64 {
    match ty {
        ValType::I32 => 4,
        ValType::I64 => 8,
        ValType::F32 => 4,
        ValType::F64 => 8,
        ValType::V128 => 16,
        ValType::FuncRef | ValType::ExternRef => panic!("not a memory type"),
    }
}
