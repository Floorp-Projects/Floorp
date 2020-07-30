use crate::error::Error;
use crate::module::DlModule;
use std::env;
use std::path::{Path, PathBuf};
use std::sync::Arc;

fn guest_module_path<P: AsRef<Path>>(path: P) -> PathBuf {
    if let Some(prefix) = env::var_os("GUEST_MODULE_PREFIX") {
        Path::new(&prefix).join(path)
    } else {
        // default to the `devenv` path convention
        Path::new("/lucet").join(path)
    }
}

impl DlModule {
    pub fn load_test<P: AsRef<Path>>(so_path: P) -> Result<Arc<Self>, Error> {
        DlModule::load(guest_module_path(so_path))
    }
}
