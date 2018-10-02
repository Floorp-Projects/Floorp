use error;
use scroll::{self, Pread};

#[repr(C)]
#[derive(Debug, PartialEq, Copy, Clone, Default)]
#[derive(Pread, Pwrite, SizeWith)]
pub struct DataDirectory {
    pub virtual_address: u32,
    pub size: u32,
}

pub const SIZEOF_DATA_DIRECTORY: usize = 8;
const NUM_DATA_DIRECTORIES: usize = 16;

impl DataDirectory {
    pub fn parse(bytes: &[u8], offset: &mut usize) -> error::Result<Self> {
        let dd = bytes.gread_with(offset, scroll::LE)?;
        Ok (dd)
    }
}

#[derive(Debug, PartialEq, Copy, Clone, Default)]
pub struct DataDirectories {
    pub data_directories: [Option<DataDirectory>; NUM_DATA_DIRECTORIES],
}

impl DataDirectories {
    pub fn parse(bytes: &[u8], count: usize, offset: &mut usize) -> error::Result<Self> {
        let mut data_directories = [None; NUM_DATA_DIRECTORIES];
        if count > NUM_DATA_DIRECTORIES { return Err (error::Error::Malformed(format!("data directory count ({}) is greater than maximum number of data directories ({})", count, NUM_DATA_DIRECTORIES))) }
        for i in 0..count {
            let dd = DataDirectory::parse(bytes, offset)?;
            let dd = if dd.virtual_address == 0 && dd.size == 0 { None } else { Some (dd) };
            data_directories[i] = dd;
        }
        Ok (DataDirectories { data_directories: data_directories })
    }
    pub fn get_export_table(&self) -> &Option<DataDirectory> {
        let idx = 0;
        unsafe { self.data_directories.get_unchecked(idx) }
    }
    pub fn get_import_table(&self) -> &Option<DataDirectory> {
        let idx = 1;
        unsafe { self.data_directories.get_unchecked(idx) }
    }
    pub fn get_resource_table(&self) ->          &Option<DataDirectory> {
        let idx = 2;
        unsafe { self.data_directories.get_unchecked(idx) }
    }
    pub fn get_exception_table(&self) ->         &Option<DataDirectory> {
        let idx = 3;
        unsafe { self.data_directories.get_unchecked(idx) }
    }
    pub fn get_certificate_table(&self) ->       &Option<DataDirectory> {
        let idx = 4;
        unsafe { self.data_directories.get_unchecked(idx) }
    }
    pub fn get_base_relocation_table(&self) ->   &Option<DataDirectory> {
        let idx = 5;
        unsafe { self.data_directories.get_unchecked(idx) }
    }
    pub fn get_debug_table(&self) ->             &Option<DataDirectory> {
        let idx = 6;
        unsafe { self.data_directories.get_unchecked(idx) }
    }
    pub fn get_architecture(&self) ->            &Option<DataDirectory> {
        let idx = 7;
        unsafe { self.data_directories.get_unchecked(idx) }
    }
    pub fn get_global_ptr(&self) ->              &Option<DataDirectory> {
        let idx = 8;
        unsafe { self.data_directories.get_unchecked(idx) }
    }
    pub fn get_tls_table(&self) ->               &Option<DataDirectory> {
        let idx = 9;
        unsafe { self.data_directories.get_unchecked(idx) }
    }
    pub fn get_load_config_table(&self) ->       &Option<DataDirectory> {
        let idx = 10;
        unsafe { self.data_directories.get_unchecked(idx) }
    }
    pub fn get_bound_import_table(&self) ->      &Option<DataDirectory> {
        let idx = 11;
        unsafe { self.data_directories.get_unchecked(idx) }
    }
    pub fn get_import_address_table(&self) ->    &Option<DataDirectory> {
        let idx = 12;
        unsafe { self.data_directories.get_unchecked(idx) }
    }
    pub fn get_delay_import_descriptor(&self) -> &Option<DataDirectory> {
        let idx = 13;
        unsafe { self.data_directories.get_unchecked(idx) }
    }
    pub fn get_clr_runtime_header(&self) ->      &Option<DataDirectory> {
        let idx = 14;
        unsafe { self.data_directories.get_unchecked(idx) }
    }
}
