/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use anyhow::{bail, Context};
use camino::Utf8Path;
use fs_err as fs;
use goblin::{
    archive::Archive,
    elf::Elf,
    mach::{segment::Section, symbols, Mach, MachO, SingleArch},
    pe::PE,
    Object,
};
use std::collections::HashSet;
use uniffi_meta::Metadata;

/// Extract metadata written by the `uniffi::export` macro from a library file
///
/// In addition to generating the scaffolding, that macro and also encodes the
/// `uniffi_meta::Metadata` for the components which can be used to generate the bindings side of
/// the interface.
pub fn extract_from_library(path: &Utf8Path) -> anyhow::Result<Vec<Metadata>> {
    extract_from_bytes(&fs::read(path)?)
}

fn extract_from_bytes(file_data: &[u8]) -> anyhow::Result<Vec<Metadata>> {
    match Object::parse(file_data)? {
        Object::Elf(elf) => extract_from_elf(elf, file_data),
        Object::PE(pe) => extract_from_pe(pe, file_data),
        Object::Mach(mach) => extract_from_mach(mach, file_data),
        Object::Archive(archive) => extract_from_archive(archive, file_data),
        Object::Unknown(_) => bail!("Unknown library format"),
    }
}

pub fn extract_from_elf(elf: Elf<'_>, file_data: &[u8]) -> anyhow::Result<Vec<Metadata>> {
    let mut extracted = ExtractedItems::new();
    let iter = elf
        .syms
        .iter()
        .filter_map(|sym| elf.section_headers.get(sym.st_shndx).map(|sh| (sym, sh)));

    for (sym, sh) in iter {
        let name = elf
            .strtab
            .get_at(sym.st_name)
            .context("Error getting symbol name")?;
        if is_metadata_symbol(name) {
            // Offset relative to the start of the section.
            let section_offset = sym.st_value - sh.sh_addr;
            // Offset relative to the start of the file contents
            extracted.extract_item(name, file_data, (sh.sh_offset + section_offset) as usize)?;
        }
    }
    Ok(extracted.into_metadata())
}

pub fn extract_from_pe(pe: PE<'_>, file_data: &[u8]) -> anyhow::Result<Vec<Metadata>> {
    let mut extracted = ExtractedItems::new();
    for export in pe.exports {
        if let Some(name) = export.name {
            if is_metadata_symbol(name) {
                extracted.extract_item(
                    name,
                    file_data,
                    export.offset.context("Error getting symbol offset")?,
                )?;
            }
        }
    }
    Ok(extracted.into_metadata())
}

pub fn extract_from_mach(mach: Mach<'_>, file_data: &[u8]) -> anyhow::Result<Vec<Metadata>> {
    match mach {
        Mach::Binary(macho) => extract_from_macho(macho, file_data),
        // Multi-binary library, just extract the first one
        Mach::Fat(multi_arch) => match multi_arch.get(0)? {
            SingleArch::MachO(macho) => extract_from_macho(macho, file_data),
            SingleArch::Archive(archive) => extract_from_archive(archive, file_data),
        },
    }
}

pub fn extract_from_macho(macho: MachO<'_>, file_data: &[u8]) -> anyhow::Result<Vec<Metadata>> {
    let mut sections: Vec<Section> = Vec::new();
    for sects in macho.segments.sections() {
        sections.extend(sects.map(|r| r.expect("section").0));
    }
    let mut extracted = ExtractedItems::new();
    sections.sort_by_key(|s| s.addr);

    // Iterate through the symbols.  This picks up symbols from the .o files embedded in a Darwin
    // archive.
    for (name, nlist) in macho.symbols().flatten() {
        // Check that the symbol:
        //   - Is global (exported)
        //   - Has type=N_SECT (it's regular data as opposed to something like
        //     "undefined" or "indirect")
        //   - Has a metadata symbol name
        if nlist.is_global() && nlist.get_type() == symbols::N_SECT && is_metadata_symbol(name) {
            let section = &sections[nlist.n_sect];
            // `nlist.n_value` is an address, so we can calculating the offset inside the section
            // using the difference between that and `section.addr`
            let offset = section.offset as usize + nlist.n_value as usize - section.addr as usize;
            extracted.extract_item(name, file_data, offset)?;
        }
    }

    // Iterate through the exports.  This picks up symbols from .dylib files.
    for export in macho.exports()? {
        let name = &export.name;
        if is_metadata_symbol(name) {
            extracted.extract_item(name, file_data, export.offset as usize)?;
        }
    }
    Ok(extracted.into_metadata())
}

pub fn extract_from_archive(
    archive: Archive<'_>,
    file_data: &[u8],
) -> anyhow::Result<Vec<Metadata>> {
    // Store the names of archive members that have metadata symbols in them
    let mut members_to_check: HashSet<&str> = HashSet::new();
    for (member_name, _, symbols) in archive.summarize() {
        for name in symbols {
            if is_metadata_symbol(name) {
                members_to_check.insert(member_name);
            }
        }
    }

    let mut items = vec![];
    for member_name in members_to_check {
        items.append(
            &mut extract_from_bytes(
                archive
                    .extract(member_name, file_data)
                    .with_context(|| format!("Failed to extract archive member `{member_name}`"))?,
            )
            .with_context(|| {
                format!("Failed to extract data from archive member `{member_name}`")
            })?,
        );
    }
    Ok(items)
}

/// Container for extracted metadata items
#[derive(Default)]
struct ExtractedItems {
    items: Vec<Metadata>,
    /// symbol names for the extracted items, we use this to ensure that we don't extract the same
    /// symbol twice
    names: HashSet<String>,
}

impl ExtractedItems {
    fn new() -> Self {
        Self::default()
    }

    fn extract_item(&mut self, name: &str, file_data: &[u8], offset: usize) -> anyhow::Result<()> {
        if self.names.contains(name) {
            // Already extracted this item
            return Ok(());
        }

        // Use the file data starting from offset, without specifying the end position.  We don't
        // always know the end position, because goblin reports the symbol size as 0 for PE and
        // MachO files.
        //
        // This works fine, because `MetadataReader` knows when the serialized data is terminated
        // and will just ignore the trailing data.
        let data = &file_data[offset..];
        self.items.push(Metadata::read(data)?);
        self.names.insert(name.to_string());
        Ok(())
    }

    fn into_metadata(self) -> Vec<Metadata> {
        self.items
    }
}

fn is_metadata_symbol(name: &str) -> bool {
    // Skip the "_" char that Darwin prepends, if present
    let name = name.strip_prefix('_').unwrap_or(name);
    name.starts_with("UNIFFI_META")
}
