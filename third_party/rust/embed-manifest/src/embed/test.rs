use std::fs;
use std::path::{Path, PathBuf};

use object::coff::CoffFile;
use object::pe::{ImageResourceDataEntry, ImageResourceDirectory, ImageResourceDirectoryEntry, IMAGE_RESOURCE_DATA_IS_DIRECTORY};
use object::{
    pod, Architecture, LittleEndian, Object, ObjectSection, ObjectSymbol, RelocationEncoding, RelocationKind, SectionKind,
};
use tempfile::{tempdir, TempDir};

use crate::embed::coff::MachineType;
use crate::embed::{create_object_file, link_manifest_gnu, link_manifest_msvc, TargetOs};
use crate::new_manifest;

#[test]
fn create_obj() {
    let res = do_embed_file(MachineType::X86_64, TargetOs::WindowsGnu);
    let data = fs::read(&res.object_file()).unwrap();
    let obj = CoffFile::parse(&data[..]).unwrap();
    assert_eq!(obj.architecture(), Architecture::X86_64);
    let expected_manifest = fs::read(&sample_manifest_path()).unwrap();
    check_object_file(obj, &expected_manifest);
}

#[test]
fn link_lib_gnu() {
    let res = do_embed_file(MachineType::X86_64, TargetOs::WindowsGnu);
    assert!(res.object_file().exists());
    let object_option = format!("cargo:rustc-link-arg-bins={}", res.object_file().display());
    assert_eq!(res.lines(), &[object_option.as_str()]);
}

#[test]
fn link_file_msvc() {
    let res = do_embed_file(MachineType::X86_64, TargetOs::WindowsMsvc);
    assert!(!res.object_file().exists());
    let mut input_option = String::from("cargo:rustc-link-arg-bins=/MANIFESTINPUT:");
    input_option.push_str(res.manifest_path.canonicalize().unwrap().to_str().unwrap());
    assert_eq!(
        res.lines(),
        &[
            "cargo:rustc-link-arg-bins=/MANIFEST:EMBED",
            input_option.as_str(),
            "cargo:rustc-link-arg-bins=/MANIFESTUAC:NO"
        ]
    );
}

struct EmbedResult {
    manifest_path: PathBuf,
    out_dir: TempDir,
    output: String,
}

impl EmbedResult {
    fn object_file(&self) -> PathBuf {
        self.out_dir.path().join("embed-manifest.o")
    }

    fn lines(&self) -> Vec<&str> {
        self.output.lines().collect()
    }
}

fn sample_manifest_path() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("testdata/sample.exe.manifest")
}

fn do_embed_file(arch: MachineType, os: TargetOs) -> EmbedResult {
    let manifest_path = sample_manifest_path();
    let out_dir = tempdir().unwrap();
    let mut buf: Vec<u8> = Vec::new();
    if matches!(os, TargetOs::WindowsMsvc) {
        link_manifest_msvc(&manifest_path, &mut buf).unwrap();
    } else {
        link_manifest_gnu(&fs::read(&manifest_path).unwrap(), out_dir.path(), arch, &mut buf).unwrap();
    }
    EmbedResult {
        manifest_path,
        out_dir,
        output: String::from_utf8(buf).unwrap(),
    }
}

#[test]
fn object_file_x86() {
    let manifest = new_manifest("Test.X86").to_string().into_bytes();
    let file = create_object_file(&manifest, MachineType::I386).unwrap();
    let obj = CoffFile::parse(&file[..]).unwrap();
    assert_eq!(obj.architecture(), Architecture::I386);
    check_object_file(obj, &manifest);
}

#[test]
fn object_file_x86_64() {
    let manifest = new_manifest("Test.X86_64").to_string().into_bytes();
    let file = create_object_file(&manifest, MachineType::X86_64).unwrap();
    let obj = CoffFile::parse(&file[..]).unwrap();
    assert_eq!(obj.architecture(), Architecture::X86_64);
    check_object_file(obj, &manifest);
}

#[test]
fn object_file_aarch64() {
    let manifest = new_manifest("Test.AARCH64").to_string().into_bytes();
    let file = create_object_file(&manifest, MachineType::Aarch64).unwrap();
    let obj = CoffFile::parse(&file[..]).unwrap();
    assert_eq!(obj.architecture(), Architecture::Aarch64);
    check_object_file(obj, &manifest);
}

fn check_object_file(obj: CoffFile, expected_manifest: &[u8]) {
    // There should be one sections `.rsrc`.
    assert_eq!(
        obj.sections().map(|s| s.name().unwrap().to_string()).collect::<Vec<_>>(),
        &[".rsrc"]
    );

    // There should be one section symbol.
    assert_eq!(
        obj.symbols().map(|s| s.name().unwrap().to_string()).collect::<Vec<_>>(),
        &[".rsrc"]
    );

    // The resource section must be a data section.
    let rsrc = obj.section_by_name(".rsrc").unwrap();
    assert_eq!(rsrc.address(), 0);
    assert_eq!(rsrc.kind(), SectionKind::Data);

    // The data RVA in the resource data entry must be relocatable.
    let (addr, reloc) = rsrc.relocations().next().unwrap();
    assert_eq!(reloc.kind(), RelocationKind::ImageOffset);
    assert_eq!(reloc.encoding(), RelocationEncoding::Generic);
    assert_eq!(addr, 0x48); // size of the directory table, three directories, and no strings
    assert_eq!(reloc.addend(), 0);

    // The resource directory contains one manifest resource type subdirectory.
    let data = rsrc.data().unwrap();
    let (dir, rest) = pod::from_bytes::<ImageResourceDirectory>(data).unwrap();
    assert_eq!(0, dir.number_of_named_entries.get(LittleEndian));
    assert_eq!(1, dir.number_of_id_entries.get(LittleEndian));
    let (entries, _) = pod::slice_from_bytes::<ImageResourceDirectoryEntry>(rest, 1).unwrap();
    assert_eq!(24, entries[0].name_or_id.get(LittleEndian));
    let offset = entries[0].offset_to_data_or_directory.get(LittleEndian);
    assert_eq!(IMAGE_RESOURCE_DATA_IS_DIRECTORY, offset & IMAGE_RESOURCE_DATA_IS_DIRECTORY);
    let offset = (offset & !IMAGE_RESOURCE_DATA_IS_DIRECTORY) as usize;

    // The manifest subdirectory contains one image (not DLL) manifest subdirectory.
    let (dir, rest) = pod::from_bytes::<ImageResourceDirectory>(&data[offset..]).unwrap();
    assert_eq!(0, dir.number_of_named_entries.get(LittleEndian));
    assert_eq!(1, dir.number_of_id_entries.get(LittleEndian));
    let (entries, _) = pod::slice_from_bytes::<ImageResourceDirectoryEntry>(rest, 1).unwrap();
    assert_eq!(1, entries[0].name_or_id.get(LittleEndian));
    let offset = entries[0].offset_to_data_or_directory.get(LittleEndian);
    assert_eq!(IMAGE_RESOURCE_DATA_IS_DIRECTORY, offset & IMAGE_RESOURCE_DATA_IS_DIRECTORY);
    let offset = (offset & !IMAGE_RESOURCE_DATA_IS_DIRECTORY) as usize;

    // The image manifest subdirectory contains one US English manifest data entry.
    let (dir, rest) = pod::from_bytes::<ImageResourceDirectory>(&data[offset..]).unwrap();
    assert_eq!(0, dir.number_of_named_entries.get(LittleEndian));
    assert_eq!(1, dir.number_of_id_entries.get(LittleEndian));
    let (entries, _) = pod::slice_from_bytes::<ImageResourceDirectoryEntry>(rest, 1).unwrap();
    assert_eq!(0x0409, entries[0].name_or_id.get(LittleEndian));
    let offset = entries[0].offset_to_data_or_directory.get(LittleEndian);
    assert_eq!(0, offset & IMAGE_RESOURCE_DATA_IS_DIRECTORY);
    let offset = offset as usize;

    // The manifest data matches what was added.
    let (entry, resource_data) = pod::from_bytes::<ImageResourceDataEntry>(&data[offset..]).unwrap();
    let end = entry.size.get(LittleEndian) as usize;
    let manifest = &resource_data[..end];
    assert_eq!(manifest, expected_manifest);
}
