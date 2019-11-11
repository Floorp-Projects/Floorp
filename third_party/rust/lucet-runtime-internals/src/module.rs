mod dl;
mod mock;
mod sparse_page_data;

pub use crate::module::dl::DlModule;
pub use crate::module::mock::{MockExportBuilder, MockModuleBuilder};
pub use lucet_module::{
    FunctionHandle, FunctionIndex, FunctionPointer, FunctionSpec, Global, GlobalSpec, GlobalValue,
    HeapSpec, Signature, TableElement, TrapCode, TrapManifest, ValueType,
};

use crate::alloc::Limits;
use crate::error::Error;
use libc::c_void;

/// Details about a program address.
///
/// It is possible to determine whether an address lies within the module code if the module is
/// loaded from a shared object. Statically linked modules are not resolvable. Best effort is made
/// to resolve the symbol the address is found inside, and the file that symbol is found in. See
/// `dladdr(3)` for more details.
#[derive(Clone, Debug)]
pub struct AddrDetails {
    pub in_module_code: bool,
    pub file_name: Option<String>,
    pub sym_name: Option<String>,
}

/// The read-only parts of a Lucet program, including its code and initial heap configuration.
///
/// Types that implement this trait are suitable for use with
/// [`Region::new_instance()`](trait.Region.html#method.new_instance).
pub trait Module: ModuleInternal {
    /// Calculate the initial size in bytes of the module's Wasm globals.
    fn initial_globals_size(&self) -> usize {
        self.globals().len() * std::mem::size_of::<u64>()
    }
}

pub trait ModuleInternal: Send + Sync {
    fn heap_spec(&self) -> Option<&HeapSpec>;

    /// Get the WebAssembly globals of the module.
    ///
    /// The indices into the returned slice correspond to the WebAssembly indices of the globals
    /// (<https://webassembly.github.io/spec/core/syntax/modules.html#syntax-globalidx>)
    fn globals(&self) -> &[GlobalSpec<'_>];

    fn get_sparse_page_data(&self, page: usize) -> Option<&[u8]>;

    /// Get the number of pages in the sparse page data.
    fn sparse_page_data_len(&self) -> usize;

    /// Get the table elements from the module.
    fn table_elements(&self) -> Result<&[TableElement], Error>;

    fn get_export_func(&self, sym: &str) -> Result<FunctionHandle, Error>;

    fn get_func_from_idx(&self, table_id: u32, func_id: u32) -> Result<FunctionHandle, Error>;

    fn get_start_func(&self) -> Result<Option<FunctionHandle>, Error>;

    fn function_manifest(&self) -> &[FunctionSpec];

    fn addr_details(&self, addr: *const c_void) -> Result<Option<AddrDetails>, Error>;

    fn get_signature(&self, fn_id: FunctionIndex) -> &Signature;

    fn get_signatures(&self) -> &[Signature];

    fn function_handle_from_ptr(&self, ptr: FunctionPointer) -> FunctionHandle {
        let id = self
            .function_manifest()
            .iter()
            .enumerate()
            .find(|(_, fn_spec)| fn_spec.ptr() == ptr)
            .map(|(fn_id, _)| FunctionIndex::from_u32(fn_id as u32))
            .expect("valid function pointer");

        FunctionHandle { ptr, id }
    }

    /// Look up an instruction pointer in the trap manifest.
    ///
    /// This function must be signal-safe.
    fn lookup_trapcode(&self, rip: *const c_void) -> Option<TrapCode> {
        for fn_spec in self.function_manifest() {
            if let Some(offset) = fn_spec.relative_addr(rip as u64) {
                // the address falls in this trap manifest's function.
                // `rip` can only lie in one function, so either
                // there's a trap site in this manifest, and that's
                // the one we want, or there's none
                return fn_spec.traps().and_then(|traps| traps.lookup_addr(offset));
            }
        }
        None
    }

    /// Check that the specifications of the WebAssembly module are valid given certain `Limit`s.
    ///
    /// Returns a `Result<(), Error>` rather than a boolean in order to provide a richer accounting
    /// of what may be invalid.
    fn validate_runtime_spec(&self, limits: &Limits) -> Result<(), Error> {
        // Modules without heap specs will not access the heap
        if let Some(heap) = self.heap_spec() {
            // Assure that the total reserved + guard regions fit in the address space.
            // First check makes sure they fit our 32-bit model, and ensures the second
            // check doesn't overflow.
            if heap.reserved_size > std::u32::MAX as u64 + 1
                || heap.guard_size > std::u32::MAX as u64 + 1
            {
                return Err(lucet_incorrect_module!(
                    "heap spec sizes would overflow: {:?}",
                    heap
                ));
            }

            if heap.reserved_size as usize + heap.guard_size as usize
                > limits.heap_address_space_size
            {
                bail_limits_exceeded!("heap spec reserved and guard size: {:?}", heap);
            }

            if heap.initial_size as usize > limits.heap_memory_size {
                bail_limits_exceeded!("heap spec initial size: {:?}", heap);
            }

            if heap.initial_size > heap.reserved_size {
                return Err(lucet_incorrect_module!(
                    "initial heap size greater than reserved size: {:?}",
                    heap
                ));
            }
        }

        if self.globals().len() * std::mem::size_of::<u64>() > limits.globals_size {
            bail_limits_exceeded!("globals exceed limits");
        }

        Ok(())
    }
}
