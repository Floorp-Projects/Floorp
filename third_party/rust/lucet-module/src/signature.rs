#[cfg(feature = "signature_checking")]
use crate::error::Error::{self, IOError, ModuleSignatureError};
use crate::module::LUCET_MODULE_SYM;
use crate::module_data::MODULE_DATA_SYM;
#[cfg(feature = "signature_checking")]
use crate::ModuleData;
use byteorder::{ByteOrder, LittleEndian};
#[cfg(feature = "signature_checking")]
pub use minisign::{PublicKey, SecretKey};
#[cfg(feature = "signature_checking")]
use minisign::{SignatureBones, SignatureBox};
use object::*;
use std::fs::{File, OpenOptions};
#[cfg(feature = "signature_checking")]
use std::io::Cursor;
use std::io::{self, Read, Seek, SeekFrom, Write};
use std::path::Path;

pub struct ModuleSignature;

#[cfg(feature = "signature_checking")]
impl ModuleSignature {
    pub fn verify<P: AsRef<Path>>(
        so_path: P,
        pk: &PublicKey,
        module_data: &ModuleData,
    ) -> Result<(), Error> {
        let signature_box: SignatureBox =
            SignatureBones::from_bytes(&module_data.get_module_signature())
                .map_err(|e| ModuleSignatureError(e))?
                .into();

        let mut raw_module_and_data =
            RawModuleAndData::from_file(&so_path).map_err(|e| IOError(e))?;
        let cleared_module_data_bin =
            ModuleData::clear_module_signature(raw_module_and_data.module_data_bin())?;
        raw_module_and_data.patch_module_data(&cleared_module_data_bin);

        minisign::verify(
            &pk,
            &signature_box,
            Cursor::new(&raw_module_and_data.obj_bin),
            true,
            false,
        )
        .map_err(|e| ModuleSignatureError(e))
    }

    pub fn sign<P: AsRef<Path>>(path: P, sk: &SecretKey) -> Result<(), Error> {
        let raw_module_and_data = RawModuleAndData::from_file(&path).map_err(|e| IOError(e))?;
        let signature_box = minisign::sign(
            None,
            sk,
            Cursor::new(&raw_module_and_data.obj_bin),
            true,
            None,
            None,
        )
        .map_err(|e| ModuleSignatureError(e))?;
        let signature_bones: SignatureBones = signature_box.into();
        let patched_module_data_bin = ModuleData::patch_module_signature(
            raw_module_and_data.module_data_bin(),
            &signature_bones.to_bytes(),
        )?;
        raw_module_and_data
            .write_patched_module_data(&path, &patched_module_data_bin)
            .map_err(|e| IOError(e))?;
        Ok(())
    }
}

#[allow(dead_code)]
struct SymbolData {
    offset: usize,
    len: usize,
}

#[allow(dead_code)]
struct RawModuleAndData {
    pub obj_bin: Vec<u8>,
    pub module_data_offset: usize,
    pub module_data_len: usize,
}

#[allow(dead_code)]
impl RawModuleAndData {
    pub fn from_file<P: AsRef<Path>>(path: P) -> Result<Self, io::Error> {
        let mut obj_bin: Vec<u8> = Vec::new();
        File::open(&path)?.read_to_end(&mut obj_bin)?;

        let native_data_symbol_data =
            Self::symbol_data(&obj_bin, LUCET_MODULE_SYM, true)?.ok_or(io::Error::new(
                io::ErrorKind::InvalidInput,
                format!("`{}` symbol not present", LUCET_MODULE_SYM),
            ))?;

        // While `module_data` is the first field of the `SerializedModule` that `lucet_module` points
        // to, it is a virtual address, not a file offset. The translation is somewhat tricky at
        // the moment, so just look at the corresponding `lucet_module_data` symbol for now.
        let module_data_symbol_data =
            Self::symbol_data(&obj_bin, MODULE_DATA_SYM, true)?.ok_or(io::Error::new(
                io::ErrorKind::InvalidInput,
                format!("`{}` symbol not present", MODULE_DATA_SYM),
            ))?;

        let module_data_len =
            LittleEndian::read_u64(&obj_bin[(native_data_symbol_data.offset + 8)..]) as usize;

        Ok(RawModuleAndData {
            obj_bin,
            module_data_offset: module_data_symbol_data.offset,
            module_data_len: module_data_len,
        })
    }

    pub fn module_data_bin(&self) -> &[u8] {
        &self.obj_bin[self.module_data_offset as usize
            ..self.module_data_offset as usize + self.module_data_len]
    }

    pub fn module_data_bin_mut(&mut self) -> &mut [u8] {
        &mut self.obj_bin[self.module_data_offset as usize
            ..self.module_data_offset as usize + self.module_data_len]
    }

    pub fn patch_module_data(&mut self, module_data_bin: &[u8]) {
        self.module_data_bin_mut().copy_from_slice(&module_data_bin);
    }

    pub fn write_patched_module_data<P: AsRef<Path>>(
        &self,
        path: P,
        patched_module_data_bin: &[u8],
    ) -> Result<(), io::Error> {
        let mut fp = OpenOptions::new()
            .write(true)
            .create_new(false)
            .open(&path)?;
        fp.seek(SeekFrom::Start(self.module_data_offset as u64))?;
        fp.write_all(&patched_module_data_bin)?;
        Ok(())
    }

    // Retrieving the offset of a symbol is not supported by the object crate.
    // In Mach-O, actual file offsets are encoded, whereas Elf encodes virtual
    // addresses, requiring extra steps to retrieve the section, its base
    // address as well as the section offset.

    // Elf
    #[cfg(all(target_family = "unix", not(target_os = "macos")))]
    fn symbol_data(
        obj_bin: &[u8],
        symbol_name: &str,
        _mangle: bool,
    ) -> Result<Option<SymbolData>, io::Error> {
        let obj = object::ElfFile::parse(obj_bin)
            .map_err(|e| io::Error::new(io::ErrorKind::InvalidInput, e))?;
        let symbol_map = obj.symbol_map();
        for symbol in symbol_map.symbols() {
            let kind = symbol.kind();
            if kind != SymbolKind::Data {
                continue;
            }
            if symbol.name() != Some(symbol_name) {
                continue;
            }
            let section_index = match symbol.section_index() {
                Some(section_index) => section_index,
                None => continue,
            };
            let section = &obj.elf().section_headers[section_index.0];
            let offset = (symbol.address() - section.sh_addr + section.sh_offset) as usize;
            let len = symbol.size() as usize;
            return Ok(Some(SymbolData { offset, len }));
        }
        Ok(None)
    }

    // Mach-O
    #[cfg(target_os = "macos")]
    fn symbol_data(
        obj_bin: &[u8],
        symbol_name: &str,
        mangle: bool,
    ) -> Result<Option<SymbolData>, io::Error> {
        let obj = object::File::parse(obj_bin)
            .map_err(|e| io::Error::new(io::ErrorKind::InvalidInput, e))?;
        let symbol_map = obj.symbol_map();
        let mangled_symbol_name = format!("_{}", symbol_name);
        let symbol_name = if mangle {
            &mangled_symbol_name
        } else {
            symbol_name
        };
        for symbol in symbol_map.symbols() {
            let kind = symbol.kind();
            if kind != SymbolKind::Data && kind != SymbolKind::Unknown {
                continue;
            }
            if symbol.name() != Some(symbol_name) {
                continue;
            }
            let offset = symbol.address() as usize;
            let len = symbol.size() as usize;
            return Ok(Some(SymbolData { offset, len }));
        }
        Ok(None)
    }
}
