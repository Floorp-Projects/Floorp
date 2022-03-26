use std::cell::RefCell;
use std::collections::HashMap;
use std::path::PathBuf;

#[derive(Default)]
pub struct MockFileSystem {
    files: RefCell<HashMap<String, std::io::Result<String>>>,
}

impl MockFileSystem {
    pub fn clear(&self) {
        self.files.borrow_mut().clear();
    }

    fn get_test_file_path() -> PathBuf {
        PathBuf::from(std::env!("CARGO_MANIFEST_DIR")).join("resources")
    }

    fn get_file(&self, path: &str) -> std::io::Result<String> {
        let mut tmp = self.files.borrow_mut();
        let result = tmp.entry(path.to_string()).or_insert_with(|| {
            let root_path = Self::get_test_file_path();
            let full_path = root_path.join(path);
            std::fs::read_to_string(full_path)
        });
        match result {
            Ok(s) => Ok(s.to_string()),
            Err(e) => Err(std::io::Error::new(e.kind(), "Error")),
        }
    }

    #[cfg(feature = "sync")]
    pub fn get_test_file_sync(&self, path: &str) -> std::io::Result<String> {
        self.get_file(path)
    }

    #[cfg(feature = "async")]
    pub async fn get_test_file_async(&self, path: &str) -> std::io::Result<String> {
        self.get_file(path)
    }
}
