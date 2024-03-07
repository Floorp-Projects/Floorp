use std::env;
use std::fs::{self, File};
use std::io::{self, stdout, BufWriter, Cursor, Write};
use std::path::{Path, PathBuf};

use crate::manifest::ManifestBuilder;

use self::coff::{resource_data_entry, resource_directory_id_entry, resource_directory_table, CoffWriter, MachineType};
use self::error::Error;

mod coff;
pub mod error;

#[cfg(test)]
mod test;

/// Embeds the manifest described by `manifest` by converting it to XML,
/// then saving it to a file and passing the correct options to the linker
/// on MSVC targets, or by building a static library and instructing Cargo
/// to link the executable against it on GNU targets.
pub fn embed_manifest(manifest: ManifestBuilder) -> Result<(), Error> {
    let out_dir = get_out_dir()?;
    let target = get_target()?;
    if matches!(target.os, TargetOs::WindowsMsvc) {
        let manifest_file = out_dir.join("manifest.xml");
        write!(BufWriter::new(File::create(&manifest_file)?), "{}", manifest)?;
        link_manifest_msvc(&manifest_file, &mut stdout().lock())
    } else {
        let manifest_data = manifest.to_string();
        link_manifest_gnu(manifest_data.as_bytes(), &out_dir, target.arch, &mut stdout().lock())
    }
}

/// Directly embeds the manifest in the provided `file` by passing the correct
/// options to the linker on MSVC targets, or by building a static library
/// and instructing Cargo to link the executable against it on GNU targets.
pub fn embed_manifest_file<P: AsRef<Path>>(file: P) -> Result<(), io::Error> {
    let out_dir = get_out_dir()?;
    let target = get_target()?;
    if matches!(target.os, TargetOs::WindowsMsvc) {
        Ok(link_manifest_msvc(file.as_ref(), &mut stdout().lock())?)
    } else {
        let manifest = fs::read(file.as_ref())?;
        Ok(link_manifest_gnu(&manifest, &out_dir, target.arch, &mut stdout().lock())?)
    }
}

fn get_out_dir() -> Result<PathBuf, io::Error> {
    match env::var_os("OUT_DIR") {
        Some(dir) => Ok(PathBuf::from(dir)),
        None => env::current_dir(),
    }
}

enum TargetOs {
    WindowsGnu,
    WindowsMsvc,
}

struct Target {
    arch: MachineType,
    os: TargetOs,
}

fn get_target() -> Result<Target, Error> {
    match env::var("TARGET") {
        Ok(target) => parse_target(&target),
        _ => Err(Error::unknown_target()),
    }
}

fn parse_target(target: &str) -> Result<Target, Error> {
    let mut iter = target.splitn(3, '-');
    let arch = match iter.next() {
        Some("i686") => MachineType::I386,
        Some("aarch64") => MachineType::Aarch64,
        Some("x86_64") => MachineType::X86_64,
        _ => return Err(Error::unknown_target()),
    };
    if iter.next() != Some("pc") {
        return Err(Error::unknown_target());
    }
    let os = match iter.next() {
        Some("windows-gnu") => TargetOs::WindowsGnu,
        Some("windows-gnullvm") => TargetOs::WindowsGnu,
        Some("windows-msvc") => TargetOs::WindowsMsvc,
        _ => return Err(Error::unknown_target()),
    };
    Ok(Target { arch, os })
}

fn link_manifest_msvc<W: Write>(manifest_path: &Path, out: &mut W) -> Result<(), Error> {
    writeln!(out, "cargo:rustc-link-arg-bins=/MANIFEST:EMBED")?;
    writeln!(
        out,
        "cargo:rustc-link-arg-bins=/MANIFESTINPUT:{}",
        manifest_path.canonicalize()?.display()
    )?;
    writeln!(out, "cargo:rustc-link-arg-bins=/MANIFESTUAC:NO")?;
    Ok(())
}

fn link_manifest_gnu<W: Write>(manifest: &[u8], out_dir: &Path, arch: MachineType, out: &mut W) -> Result<(), Error> {
    // Generate a COFF object file containing the manifest in a .rsrc section.
    let object_data = create_object_file(manifest, arch)?;
    let path = out_dir.join("embed-manifest.o");
    fs::create_dir_all(out_dir)?;
    fs::write(&path, object_data)?;

    // Link the manifest with the executable.
    writeln!(out, "cargo:rustc-link-arg-bins={}", path.display())?;
    Ok(())
}

fn create_object_file(manifest: &[u8], arch: MachineType) -> Result<Vec<u8>, io::Error> {
    // Start object file with .rsrc section.
    let mut obj = CoffWriter::new(Cursor::new(Vec::with_capacity(4096)), arch)?;

    // Create resource directories for type ID 24, name ID 1, language ID 1033.
    obj.add_data(&resource_directory_table(1))?;
    obj.add_data(&resource_directory_id_entry(24, 24, true))?;
    obj.add_data(&resource_directory_table(1))?;
    obj.add_data(&resource_directory_id_entry(1, 48, true))?;
    obj.add_data(&resource_directory_table(1))?;
    obj.add_data(&resource_directory_id_entry(1033, 72, false))?;

    // Add resource data entry with relocated address.
    let address = obj.add_data(&resource_data_entry(88, manifest.len() as u32))?;

    // Add the manifest data.
    obj.add_data(manifest)?;
    obj.align_to(8)?;

    // Write manifest data relocation at the end of the section.
    obj.add_relocation(address)?;

    // Finish writing file and return the populated object data.
    Ok(obj.finish()?.into_inner())
}
