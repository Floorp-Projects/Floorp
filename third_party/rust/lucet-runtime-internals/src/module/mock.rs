use crate::error::Error;
use crate::module::{AddrDetails, GlobalSpec, HeapSpec, Module, ModuleInternal, TableElement};
use libc::c_void;
use lucet_module::owned::{
    OwnedExportFunction, OwnedFunctionMetadata, OwnedGlobalSpec, OwnedImportFunction,
    OwnedLinearMemorySpec, OwnedModuleData, OwnedSparseData,
};
use lucet_module::{
    FunctionHandle, FunctionIndex, FunctionPointer, FunctionSpec, ModuleData, ModuleFeatures,
    Signature, TrapSite, UniqueSignatureIndex,
};
use std::collections::{BTreeMap, HashMap};
use std::sync::Arc;

#[derive(Default)]
pub struct MockModuleBuilder {
    heap_spec: HeapSpec,
    sparse_page_data: Vec<Option<Vec<u8>>>,
    globals: BTreeMap<usize, OwnedGlobalSpec>,
    table_elements: BTreeMap<usize, TableElement>,
    export_funcs: HashMap<&'static str, FunctionPointer>,
    func_table: HashMap<(u32, u32), FunctionPointer>,
    start_func: Option<FunctionPointer>,
    function_manifest: Vec<FunctionSpec>,
    function_info: Vec<OwnedFunctionMetadata>,
    imports: Vec<OwnedImportFunction>,
    exports: Vec<OwnedExportFunction>,
    signatures: Vec<Signature>,
}

impl MockModuleBuilder {
    pub fn new() -> Self {
        const DEFAULT_HEAP_SPEC: HeapSpec = HeapSpec {
            reserved_size: 4 * 1024 * 1024,
            guard_size: 4 * 1024 * 1024,
            initial_size: 64 * 1024,
            max_size: Some(64 * 1024),
        };
        MockModuleBuilder::default().with_heap_spec(DEFAULT_HEAP_SPEC)
    }

    pub fn with_heap_spec(mut self, heap_spec: HeapSpec) -> Self {
        self.heap_spec = heap_spec;
        self
    }

    pub fn with_initial_heap(mut self, heap: &[u8]) -> Self {
        self.sparse_page_data = heap
            .chunks(4096)
            .map(|page| {
                if page.iter().all(|b| *b == 0) {
                    None
                } else {
                    let mut page = page.to_vec();
                    if page.len() < 4096 {
                        page.resize(4096, 0);
                    }
                    Some(page)
                }
            })
            .collect();
        self
    }

    pub fn with_global(mut self, idx: u32, init_val: i64) -> Self {
        self.globals
            .insert(idx as usize, OwnedGlobalSpec::new_def(init_val, vec![]));
        self
    }

    pub fn with_exported_global(mut self, idx: u32, init_val: i64, export_name: &str) -> Self {
        self.globals.insert(
            idx as usize,
            OwnedGlobalSpec::new_def(init_val, vec![export_name.to_string()]),
        );
        self
    }

    pub fn with_import(mut self, idx: u32, import_module: &str, import_field: &str) -> Self {
        self.globals.insert(
            idx as usize,
            OwnedGlobalSpec::new_import(
                import_module.to_string(),
                import_field.to_string(),
                vec![],
            ),
        );
        self
    }

    pub fn with_exported_import(
        mut self,
        idx: u32,
        import_module: &str,
        import_field: &str,
        export_name: &str,
    ) -> Self {
        self.globals.insert(
            idx as usize,
            OwnedGlobalSpec::new_import(
                import_module.to_string(),
                import_field.to_string(),
                vec![export_name.to_string()],
            ),
        );
        self
    }

    pub fn with_table_element(mut self, idx: u32, element: &TableElement) -> Self {
        self.table_elements.insert(idx as usize, element.clone());
        self
    }

    fn record_sig(&mut self, sig: Signature) -> UniqueSignatureIndex {
        let idx = self
            .signatures
            .iter()
            .enumerate()
            .find(|(_, v)| *v == &sig)
            .map(|(key, _)| key)
            .unwrap_or_else(|| {
                self.signatures.push(sig);
                self.signatures.len() - 1
            });
        UniqueSignatureIndex::from_u32(idx as u32)
    }

    pub fn with_export_func(mut self, export: MockExportBuilder) -> Self {
        self.export_funcs.insert(export.sym(), export.func());
        let sig_idx = self.record_sig(export.sig());
        self.function_info.push(OwnedFunctionMetadata {
            signature: sig_idx,
            name: Some(export.sym().to_string()),
        });
        self.exports.push(OwnedExportFunction {
            fn_idx: FunctionIndex::from_u32(self.function_manifest.len() as u32),
            names: vec![export.sym().to_string()],
        });
        self.function_manifest.push(FunctionSpec::new(
            export.func().as_usize() as u64,
            export.func_len() as u32,
            export.traps().as_ptr() as u64,
            export.traps().len() as u64,
        ));
        self
    }

    pub fn with_exported_import_func(
        mut self,
        export_name: &'static str,
        import_fn_ptr: FunctionPointer,
        sig: Signature,
    ) -> Self {
        self.export_funcs.insert(export_name, import_fn_ptr);
        let sig_idx = self.record_sig(sig);
        self.function_info.push(OwnedFunctionMetadata {
            signature: sig_idx,
            name: Some(export_name.to_string()),
        });
        self.exports.push(OwnedExportFunction {
            fn_idx: FunctionIndex::from_u32(self.function_manifest.len() as u32),
            names: vec![export_name.to_string()],
        });
        self.function_manifest.push(FunctionSpec::new(
            import_fn_ptr.as_usize() as u64,
            0u32,
            0u64,
            0u64,
        ));
        self
    }

    pub fn with_table_func(mut self, table_idx: u32, func_idx: u32, func: FunctionPointer) -> Self {
        self.func_table.insert((table_idx, func_idx), func);
        self
    }

    pub fn with_start_func(mut self, func: FunctionPointer) -> Self {
        self.start_func = Some(func);
        self
    }

    pub fn build(self) -> Arc<dyn Module> {
        assert!(
            self.sparse_page_data.len() * 4096 <= self.heap_spec.initial_size as usize,
            "heap must fit in heap spec initial size"
        );

        let table_elements = self
            .table_elements
            .into_iter()
            .enumerate()
            .map(|(expected_idx, (idx, te))| {
                assert_eq!(
                    idx, expected_idx,
                    "table element indices must be contiguous starting from 0"
                );
                te
            })
            .collect();
        let globals_spec = self
            .globals
            .into_iter()
            .enumerate()
            .map(|(expected_idx, (idx, gs))| {
                assert_eq!(
                    idx, expected_idx,
                    "global indices must be contiguous starting from 0"
                );
                gs
            })
            .collect();
        let owned_module_data = OwnedModuleData::new(
            Some(OwnedLinearMemorySpec {
                heap: self.heap_spec,
                initializer: OwnedSparseData::new(self.sparse_page_data)
                    .expect("sparse data pages are valid"),
            }),
            globals_spec,
            self.function_info.clone(),
            self.imports,
            self.exports,
            self.signatures,
            ModuleFeatures::none(),
        );
        let serialized_module_data = owned_module_data
            .to_ref()
            .serialize()
            .expect("serialization of module_data succeeds");
        let module_data = ModuleData::deserialize(&serialized_module_data)
            .map(|md| unsafe { std::mem::transmute(md) })
            .expect("module data can be deserialized");
        let mock = MockModule {
            serialized_module_data,
            module_data,
            table_elements,
            export_funcs: self.export_funcs,
            func_table: self.func_table,
            start_func: self.start_func,
            function_manifest: self.function_manifest,
        };
        Arc::new(mock)
    }
}

pub struct MockModule {
    #[allow(dead_code)]
    serialized_module_data: Vec<u8>,
    module_data: ModuleData<'static>,
    pub table_elements: Vec<TableElement>,
    pub export_funcs: HashMap<&'static str, FunctionPointer>,
    pub func_table: HashMap<(u32, u32), FunctionPointer>,
    pub start_func: Option<FunctionPointer>,
    pub function_manifest: Vec<FunctionSpec>,
}

unsafe impl Send for MockModule {}
unsafe impl Sync for MockModule {}

impl Module for MockModule {}

impl ModuleInternal for MockModule {
    fn heap_spec(&self) -> Option<&HeapSpec> {
        self.module_data.heap_spec()
    }

    fn globals(&self) -> &[GlobalSpec<'_>] {
        self.module_data.globals_spec()
    }

    fn get_sparse_page_data(&self, page: usize) -> Option<&[u8]> {
        if let Some(ref sparse_data) = self.module_data.sparse_data() {
            *sparse_data.get_page(page)
        } else {
            None
        }
    }

    fn sparse_page_data_len(&self) -> usize {
        self.module_data.sparse_data().map(|d| d.len()).unwrap_or(0)
    }

    fn table_elements(&self) -> Result<&[TableElement], Error> {
        Ok(&self.table_elements)
    }

    fn get_export_func(&self, sym: &str) -> Result<FunctionHandle, Error> {
        let ptr = *self
            .export_funcs
            .get(sym)
            .ok_or(Error::SymbolNotFound(sym.to_string()))?;

        Ok(self.function_handle_from_ptr(ptr))
    }

    fn get_func_from_idx(&self, table_id: u32, func_id: u32) -> Result<FunctionHandle, Error> {
        let ptr = self
            .func_table
            .get(&(table_id, func_id))
            .cloned()
            .ok_or(Error::FuncNotFound(table_id, func_id))?;

        Ok(self.function_handle_from_ptr(ptr))
    }

    fn get_start_func(&self) -> Result<Option<FunctionHandle>, Error> {
        Ok(self
            .start_func
            .map(|start| self.function_handle_from_ptr(start)))
    }

    fn function_manifest(&self) -> &[FunctionSpec] {
        &self.function_manifest
    }

    fn addr_details(&self, _addr: *const c_void) -> Result<Option<AddrDetails>, Error> {
        // we can call `dladdr` on Rust code, but unless we inspect the stack I don't think there's
        // a way to determine whether or not we're in "module" code; punt for now
        Ok(None)
    }

    fn get_signature(&self, fn_id: FunctionIndex) -> &Signature {
        self.module_data.get_signature(fn_id)
    }

    fn get_signatures(&self) -> &[Signature] {
        self.module_data.signatures()
    }
}

pub struct MockExportBuilder {
    sym: &'static str,
    func: FunctionPointer,
    func_len: Option<usize>,
    traps: Option<&'static [TrapSite]>,
    sig: Signature,
}

impl MockExportBuilder {
    pub fn new(name: &'static str, func: FunctionPointer) -> MockExportBuilder {
        MockExportBuilder {
            sym: name,
            func: func,
            func_len: None,
            traps: None,
            sig: Signature {
                params: vec![],
                ret_ty: None,
            },
        }
    }

    pub fn with_func_len(mut self, len: usize) -> MockExportBuilder {
        self.func_len = Some(len);
        self
    }

    pub fn with_traps(mut self, traps: &'static [TrapSite]) -> MockExportBuilder {
        self.traps = Some(traps);
        self
    }

    pub fn with_sig(mut self, sig: Signature) -> MockExportBuilder {
        self.sig = sig;
        self
    }

    pub fn sym(&self) -> &'static str {
        self.sym
    }
    pub fn func(&self) -> FunctionPointer {
        self.func
    }
    pub fn func_len(&self) -> usize {
        self.func_len.unwrap_or(1)
    }
    pub fn traps(&self) -> &'static [TrapSite] {
        self.traps.unwrap_or(&[])
    }
    pub fn sig(&self) -> Signature {
        self.sig.clone()
    }
}
