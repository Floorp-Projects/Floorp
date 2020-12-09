use crate::error::Error;
use crate::module::{AddrDetails, GlobalSpec, HeapSpec, Module, ModuleInternal, TableElement};
use libc::c_void;
use libloading::Library;
use lucet_module::{
    FunctionHandle, FunctionIndex, FunctionPointer, FunctionSpec, ModuleData, ModuleFeatures,
    SerializedModule, Signature, LUCET_MODULE_SYM,
};
#[cfg(feature = "signature_checking")]
use lucet_module::{ModuleSignature, PublicKey};
use std::ffi::CStr;
use std::mem::MaybeUninit;
use std::path::Path;
use std::slice;
use std::slice::from_raw_parts;
use std::sync::Arc;

use raw_cpuid::CpuId;

fn check_feature_support(module_features: &ModuleFeatures) -> Result<(), Error> {
    let cpuid = CpuId::new();

    fn missing_feature(feature: &str) -> Error {
        Error::Unsupported(format!(
            "Module requires feature host does not support: {}",
            feature
        ))
    }

    let info = cpuid
        .get_feature_info()
        .ok_or_else(|| Error::Unsupported("Unable to obtain host CPU feature info!".to_string()))?;

    if module_features.sse3 && !info.has_sse3() {
        return Err(missing_feature("SSE3"));
    }
    if module_features.ssse3 && !info.has_ssse3() {
        return Err(missing_feature("SSS3"));
    }
    if module_features.sse41 && !info.has_sse41() {
        return Err(missing_feature("SSE4.1"));
    }
    if module_features.sse42 && !info.has_sse42() {
        return Err(missing_feature("SSE4.2"));
    }
    if module_features.avx && !info.has_avx() {
        return Err(missing_feature("AVX"));
    }
    if module_features.popcnt && !info.has_popcnt() {
        return Err(missing_feature("POPCNT"));
    }

    if module_features.bmi1 || module_features.bmi2 {
        let info = cpuid.get_extended_feature_info().ok_or_else(|| {
            Error::Unsupported("Unable to obtain host CPU extended feature info!".to_string())
        })?;

        if module_features.bmi1 && !info.has_bmi1() {
            return Err(missing_feature("BMI1"));
        }

        if module_features.bmi2 && !info.has_bmi2() {
            return Err(missing_feature("BMI2"));
        }
    }

    if module_features.lzcnt {
        let info = cpuid.get_extended_function_info().ok_or_else(|| {
            Error::Unsupported("Unable to obtain host CPU extended function info!".to_string())
        })?;

        if module_features.lzcnt && !info.has_lzcnt() {
            return Err(missing_feature("LZCNT"));
        }
    }

    // Features are fine, we're compatible!
    Ok(())
}

/// A Lucet module backed by a dynamically-loaded shared object.
pub struct DlModule {
    lib: Library,

    /// Base address of the dynamically-loaded module
    fbase: *const c_void,

    /// Metadata decoded from inside the module
    module: lucet_module::Module<'static>,
}

// for the one raw pointer only
unsafe impl Send for DlModule {}
unsafe impl Sync for DlModule {}

impl DlModule {
    /// Create a module, loading code from a shared object on the filesystem.
    pub fn load<P: AsRef<Path>>(so_path: P) -> Result<Arc<Self>, Error> {
        Self::load_and_maybe_verify(so_path, |_module_data| Ok(()))
    }

    /// Create a module, loading code from a shared object on the filesystem
    /// and verifying it using a public key if one has been supplied.
    #[cfg(feature = "signature_checking")]
    pub fn load_and_verify<P: AsRef<Path>>(so_path: P, pk: PublicKey) -> Result<Arc<Self>, Error> {
        Self::load_and_maybe_verify(so_path, |module_data| {
            // Public key has been provided, verify the module signature
            // The TOCTOU issue is unavoidable without reimplenting `dlopen(3)`
            ModuleSignature::verify(so_path, &pk, &module_data)
        })
    }

    fn load_and_maybe_verify<P: AsRef<Path>>(
        so_path: P,
        verifier: fn(&ModuleData) -> Result<(), Error>,
        // pk: Option<PublicKey>,
    ) -> Result<Arc<Self>, Error> {
        // Load the dynamic library. The undefined symbols corresponding to the lucet_syscall_
        // functions will be provided by the current executable.  We trust our wasm->dylib compiler
        // to make sure these function calls are the way the dylib can touch memory outside of its
        // stack and heap.
        // let abs_so_path = so_path.as_ref().canonicalize().map_err(Error::DlError)?;
        let lib = Library::new(so_path.as_ref().as_os_str()).map_err(Error::DlError)?;

        let serialized_module_ptr = unsafe {
            lib.get::<*const SerializedModule>(LUCET_MODULE_SYM.as_bytes())
                .map_err(|e| {
                    lucet_incorrect_module!("error loading required symbol `lucet_module`: {}", e)
                })?
        };

        let serialized_module: &SerializedModule =
            unsafe { serialized_module_ptr.as_ref().unwrap() };

        // Deserialize the slice into ModuleData, which will hold refs into the loaded
        // shared object file in `module_data_slice`. Both of these get a 'static lifetime because
        // Rust doesn't have a safe way to describe that their lifetime matches the containing
        // struct (and the dll).
        //
        // The exposed lifetime of ModuleData will be the same as the lifetime of the
        // dynamically loaded library. This makes the interface safe.
        let module_data_slice: &'static [u8] = unsafe {
            slice::from_raw_parts(
                serialized_module.module_data_ptr as *const u8,
                serialized_module.module_data_len as usize,
            )
        };
        let module_data = ModuleData::deserialize(module_data_slice)?;

        check_feature_support(module_data.features())?;

        verifier(&module_data)?;

        let fbase = if let Some(dli) =
            dladdr(serialized_module as *const SerializedModule as *const c_void)
        {
            dli.dli_fbase
        } else {
            std::ptr::null()
        };

        if serialized_module.tables_len > std::u32::MAX as u64 {
            lucet_incorrect_module!("table segment too long: {}", serialized_module.tables_len);
        }
        let tables: &'static [&'static [TableElement]] = unsafe {
            from_raw_parts(
                serialized_module.tables_ptr as *const &[TableElement],
                serialized_module.tables_len as usize,
            )
        };

        let function_manifest = if serialized_module.function_manifest_ptr != 0 {
            unsafe {
                from_raw_parts(
                    serialized_module.function_manifest_ptr as *const FunctionSpec,
                    serialized_module.function_manifest_len as usize,
                )
            }
        } else {
            &[]
        };

        Ok(Arc::new(DlModule {
            lib,
            fbase,
            module: lucet_module::Module {
                module_data,
                tables,
                function_manifest,
            },
        }))
    }
}

impl Module for DlModule {}

impl ModuleInternal for DlModule {
    fn heap_spec(&self) -> Option<&HeapSpec> {
        self.module.module_data.heap_spec()
    }

    fn globals(&self) -> &[GlobalSpec<'_>] {
        self.module.module_data.globals_spec()
    }

    fn get_sparse_page_data(&self, page: usize) -> Option<&[u8]> {
        if let Some(ref sparse_data) = self.module.module_data.sparse_data() {
            *sparse_data.get_page(page)
        } else {
            None
        }
    }

    fn sparse_page_data_len(&self) -> usize {
        self.module
            .module_data
            .sparse_data()
            .map(|d| d.len())
            .unwrap_or(0)
    }

    fn table_elements(&self) -> Result<&[TableElement], Error> {
        match self.module.tables.get(0) {
            Some(table) => Ok(table),
            None => Err(lucet_incorrect_module!("table 0 is not present")),
        }
    }

    fn get_export_func(&self, sym: &str) -> Result<FunctionHandle, Error> {
        self.module
            .module_data
            .get_export_func_id(sym)
            .ok_or_else(|| Error::SymbolNotFound(sym.to_string()))
            .map(|id| {
                let ptr = self.function_manifest()[id.as_u32() as usize].ptr();
                FunctionHandle { ptr, id }
            })
    }

    fn get_func_from_idx(&self, table_id: u32, func_id: u32) -> Result<FunctionHandle, Error> {
        if table_id != 0 {
            return Err(Error::FuncNotFound(table_id, func_id));
        }
        let table = self.table_elements()?;
        let func = table
            .get(func_id as usize)
            .map(|element| element.function_pointer())
            .ok_or(Error::FuncNotFound(table_id, func_id))?;

        Ok(self.function_handle_from_ptr(func))
    }

    fn get_start_func(&self) -> Result<Option<FunctionHandle>, Error> {
        // `guest_start` is a pointer to the function the module designates as the start function,
        // since we can't have multiple symbols pointing to the same function and guest code might
        // call it in the normal course of execution
        if let Ok(start_func) = unsafe { self.lib.get::<*const extern "C" fn()>(b"guest_start") } {
            if start_func.is_null() {
                lucet_incorrect_module!("`guest_start` is defined but null");
            }
            Ok(Some(self.function_handle_from_ptr(
                FunctionPointer::from_usize(unsafe { **start_func } as usize),
            )))
        } else {
            Ok(None)
        }
    }

    fn function_manifest(&self) -> &[FunctionSpec] {
        self.module.function_manifest
    }

    fn addr_details(&self, addr: *const c_void) -> Result<Option<AddrDetails>, Error> {
        if let Some(dli) = dladdr(addr) {
            let file_name = if dli.dli_fname.is_null() {
                None
            } else {
                Some(unsafe { CStr::from_ptr(dli.dli_fname).to_owned().into_string()? })
            };
            let sym_name = if dli.dli_sname.is_null() {
                None
            } else {
                Some(unsafe { CStr::from_ptr(dli.dli_sname).to_owned().into_string()? })
            };
            Ok(Some(AddrDetails {
                in_module_code: dli.dli_fbase as *const c_void == self.fbase,
                file_name,
                sym_name,
            }))
        } else {
            Ok(None)
        }
    }

    fn get_signature(&self, fn_id: FunctionIndex) -> &Signature {
        self.module.module_data.get_signature(fn_id)
    }

    fn get_signatures(&self) -> &[Signature] {
        self.module.module_data.signatures()
    }
}

// TODO: PR to nix or libloading?
// TODO: possibly not safe to use without grabbing the mutex within libloading::Library?
fn dladdr(addr: *const c_void) -> Option<libc::Dl_info> {
    let mut info = MaybeUninit::<libc::Dl_info>::uninit();
    let res = unsafe { libc::dladdr(addr, info.as_mut_ptr()) };
    if res != 0 {
        Some(unsafe { info.assume_init() })
    } else {
        None
    }
}
