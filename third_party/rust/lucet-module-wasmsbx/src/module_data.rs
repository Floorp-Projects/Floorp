use crate::{
    functions::{
        ExportFunction, FunctionIndex, FunctionMetadata, ImportFunction, OwnedFunctionMetadata,
    },
    globals::GlobalSpec,
    linear_memory::{HeapSpec, LinearMemorySpec, SparseData},
    types::Signature,
    Error,
};
#[cfg(feature = "signature_checking")]
use minisign::SignatureBones;
use serde::{Deserialize, Serialize};

pub const MODULE_DATA_SYM: &str = "lucet_module_data";

/// The metadata (and some data) for a Lucet module.
///
/// The lifetime parameter exists to support zero-copy deserialization for the `&str` and `&[u8]`
/// fields at the leaves of the structure. For a variant with owned types at the leaves, see
/// [`OwnedModuleData`](owned/struct.OwnedModuleData.html).
///
/// The goal is for this structure to eventually include everything except the code for the guest
/// functions themselves.
#[derive(Debug, Serialize, Deserialize)]
pub struct ModuleData<'a> {
    #[serde(borrow)]
    linear_memory: Option<LinearMemorySpec<'a>>,
    #[serde(borrow)]
    globals_spec: Vec<GlobalSpec<'a>>,
    #[serde(borrow)]
    function_info: Vec<FunctionMetadata<'a>>,
    #[serde(borrow)]
    import_functions: Vec<ImportFunction<'a>>,
    #[serde(borrow)]
    export_functions: Vec<ExportFunction<'a>>,
    signatures: Vec<Signature>,
    module_signature: Vec<u8>,
    features: ModuleFeatures,
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct ModuleFeatures {
    pub sse3: bool,
    pub ssse3: bool,
    pub sse41: bool,
    pub sse42: bool,
    pub avx: bool,
    pub bmi1: bool,
    pub bmi2: bool,
    pub lzcnt: bool,
    pub popcnt: bool,
    _hidden: (),
}

impl ModuleFeatures {
    pub fn none() -> Self {
        Self {
            sse3: false,
            ssse3: false,
            sse41: false,
            sse42: false,
            avx: false,
            bmi1: false,
            bmi2: false,
            lzcnt: false,
            popcnt: false,
            _hidden: (),
        }
    }
}

impl<'a> ModuleData<'a> {
    #[cfg(feature = "signature_checking")]
    pub fn new(
        linear_memory: Option<LinearMemorySpec<'a>>,
        globals_spec: Vec<GlobalSpec<'a>>,
        function_info: Vec<FunctionMetadata<'a>>,
        import_functions: Vec<ImportFunction<'a>>,
        export_functions: Vec<ExportFunction<'a>>,
        signatures: Vec<Signature>,
        features: ModuleFeatures,
    ) -> Self {
        let module_signature = vec![0u8; SignatureBones::BYTES];
        Self {
            linear_memory,
            globals_spec,
            function_info,
            import_functions,
            export_functions,
            signatures,
            module_signature,
            features,
        }
    }

    #[cfg(not(feature = "signature_checking"))]
    pub fn new(
        linear_memory: Option<LinearMemorySpec<'a>>,
        globals_spec: Vec<GlobalSpec<'a>>,
        function_info: Vec<FunctionMetadata<'a>>,
        import_functions: Vec<ImportFunction<'a>>,
        export_functions: Vec<ExportFunction<'a>>,
        signatures: Vec<Signature>,
        features: ModuleFeatures,
    ) -> Self {
        let module_signature = vec![0u8; 0];
        Self {
            linear_memory,
            globals_spec,
            function_info,
            import_functions,
            export_functions,
            signatures,
            module_signature,
            features,
        }
    }

    pub fn heap_spec(&self) -> Option<&HeapSpec> {
        if let Some(ref linear_memory) = self.linear_memory {
            Some(&linear_memory.heap)
        } else {
            None
        }
    }

    pub fn sparse_data(&self) -> Option<&SparseData<'a>> {
        if let Some(ref linear_memory) = self.linear_memory {
            Some(&linear_memory.initializer)
        } else {
            None
        }
    }

    pub fn globals_spec(&self) -> &[GlobalSpec<'a>] {
        &self.globals_spec
    }

    pub fn function_info(&self) -> &[FunctionMetadata<'a>] {
        &self.function_info
    }

    pub fn import_functions(&self) -> &[ImportFunction<'_>] {
        &self.import_functions
    }

    pub fn export_functions(&self) -> &[ExportFunction<'_>] {
        &self.export_functions
    }

    // Function index here is a different index space than `get_func_from_idx`, which
    // uses function index as an index into a table of function elements.
    //
    // This is an index of all functions in the module.
    pub fn get_signature(&self, fn_id: FunctionIndex) -> &Signature {
        let sig_idx = self.function_info[fn_id.as_u32() as usize].signature;
        &self.signatures[sig_idx.as_u32() as usize]
    }

    pub fn get_export_func_id(&self, name: &str) -> Option<FunctionIndex> {
        self.export_functions
            .iter()
            .find(|export| export.names.contains(&name))
            .map(|export| export.fn_idx)
    }

    pub fn signatures(&self) -> &[Signature] {
        &self.signatures
    }

    pub fn get_module_signature(&self) -> &[u8] {
        &self.module_signature
    }

    pub fn features(&self) -> &ModuleFeatures {
        &self.features
    }

    #[cfg(feature = "signature_checking")]
    pub fn patch_module_signature(
        module_data_bin: &'a [u8],
        module_signature: &[u8],
    ) -> Result<Vec<u8>, Error> {
        assert_eq!(module_signature.len(), SignatureBones::BYTES);
        let mut module_data = Self::deserialize(module_data_bin)?;
        module_data
            .module_signature
            .copy_from_slice(module_signature);
        let patched_module_data_bin = module_data.serialize()?;
        assert_eq!(patched_module_data_bin.len(), module_data_bin.len());
        Ok(patched_module_data_bin)
    }

    #[cfg(feature = "signature_checking")]
    pub fn clear_module_signature(module_data_bin: &'a [u8]) -> Result<Vec<u8>, Error> {
        let module_signature = vec![0u8; SignatureBones::BYTES];
        Self::patch_module_signature(module_data_bin, &module_signature)
    }

    /// Serialize to [`bincode`](https://github.com/TyOverby/bincode).
    pub fn serialize(&self) -> Result<Vec<u8>, Error> {
        bincode::serialize(self).map_err(Error::SerializationError)
    }

    /// Deserialize from [`bincode`](https://github.com/TyOverby/bincode).
    pub fn deserialize(buf: &'a [u8]) -> Result<ModuleData<'a>, Error> {
        bincode::deserialize(buf).map_err(Error::DeserializationError)
    }
}

use crate::{
    functions::{OwnedExportFunction, OwnedImportFunction},
    globals::OwnedGlobalSpec,
    linear_memory::{OwnedLinearMemorySpec, OwnedSparseData},
};

/// The metadata (and some data) for a Lucet module.
///
/// This is a version of [`ModuleData`](../struct.ModuleData.html) with owned types throughout,
/// rather than references to support zero-copy deserialization. This type is useful when directly
/// building up a value to be serialized.
pub struct OwnedModuleData {
    linear_memory: Option<OwnedLinearMemorySpec>,
    globals_spec: Vec<OwnedGlobalSpec>,
    function_info: Vec<OwnedFunctionMetadata>,
    imports: Vec<OwnedImportFunction>,
    exports: Vec<OwnedExportFunction>,
    signatures: Vec<Signature>,
    features: ModuleFeatures,
}

impl OwnedModuleData {
    pub fn new(
        linear_memory: Option<OwnedLinearMemorySpec>,
        globals_spec: Vec<OwnedGlobalSpec>,
        function_info: Vec<OwnedFunctionMetadata>,
        imports: Vec<OwnedImportFunction>,
        exports: Vec<OwnedExportFunction>,
        signatures: Vec<Signature>,
        features: ModuleFeatures,
    ) -> Self {
        Self {
            linear_memory,
            globals_spec,
            function_info,
            imports,
            exports,
            signatures,
            features,
        }
    }

    /// Create a [`ModuleData`](../struct.ModuleData.html) backed by the values in this
    /// `OwnedModuleData`.
    pub fn to_ref<'a>(&'a self) -> ModuleData<'a> {
        ModuleData::new(
            if let Some(ref owned_linear_memory) = self.linear_memory {
                Some(owned_linear_memory.to_ref())
            } else {
                None
            },
            self.globals_spec.iter().map(|gs| gs.to_ref()).collect(),
            self.function_info
                .iter()
                .map(|info| info.to_ref())
                .collect(),
            self.imports.iter().map(|imp| imp.to_ref()).collect(),
            self.exports.iter().map(|exp| exp.to_ref()).collect(),
            self.signatures.clone(),
            self.features.clone(),
        )
    }

    pub fn empty() -> Self {
        Self::new(
            None,
            vec![],
            vec![],
            vec![],
            vec![],
            vec![],
            ModuleFeatures::none(),
        )
    }

    pub fn with_heap_spec(mut self, heap_spec: HeapSpec) -> Self {
        if let Some(ref mut linear_memory) = self.linear_memory {
            linear_memory.heap = heap_spec;
        } else {
            self.linear_memory = Some(OwnedLinearMemorySpec {
                heap: heap_spec,
                initializer: OwnedSparseData::new(vec![]).unwrap(),
            });
        }
        self
    }
}

impl Default for OwnedModuleData {
    fn default() -> Self {
        OwnedModuleData::empty()
    }
}
