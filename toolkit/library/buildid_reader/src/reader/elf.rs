/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use nserror::{
    nsresult, NS_ERROR_CANNOT_CONVERT_DATA, NS_ERROR_FAILURE, NS_ERROR_FILE_COPY_OR_MOVE_FAILED,
    NS_ERROR_ILLEGAL_VALUE, NS_ERROR_INVALID_ARG, NS_ERROR_INVALID_POINTER, NS_ERROR_NOT_AVAILABLE,
    NS_ERROR_NOT_INITIALIZED,
};

use super::BuildIdReader;
use goblin::{elf, elf::note::Note, elf::section_header::SectionHeader};
use scroll::ctx::TryFromCtx;

use log::{error, trace};

impl BuildIdReader {
    pub fn get_build_id_bytes(
        &mut self,
        buffer: &[u8],
        note_name: &str,
    ) -> Result<Vec<u8>, nsresult> {
        trace!("get_build_id_bytes: {}", note_name);
        let elf_head = elf::Elf::parse_header(buffer).map_err(|e| {
            error!(
                "get_build_id_bytes: failed to parse Elf header with error {}",
                e
            );
            NS_ERROR_FAILURE
        })?;
        let mut elf = elf::Elf::lazy_parse(elf_head).map_err(|e| {
            error!(
                "get_build_id_bytes: failed to lazy parse Elf with error {}",
                e
            );
            NS_ERROR_NOT_INITIALIZED
        })?;

        trace!("get_build_id_bytes: {:?}", elf);
        let context = goblin::container::Ctx {
            container: elf.header.container().map_err(|e| {
                error!(
                    "get_build_id_bytes: failed to get Elf container with error {}",
                    e
                );
                NS_ERROR_INVALID_ARG
            })?,
            le: elf.header.endianness().map_err(|e| {
                error!(
                    "get_build_id_bytes: failed to get Elf endianness with error {}",
                    e
                );
                NS_ERROR_INVALID_ARG
            })?,
        };

        trace!("get_build_id_bytes: {:?}", context);
        let section_header_bytes = self
            .copy_bytes(
                elf_head.e_shoff as usize,
                (elf_head.e_shnum as usize) * (elf_head.e_shentsize as usize),
            )
            .map_err(|e| {
                error!(
                    "get_build_id_bytes: failed to copy section header bytes with error {}",
                    e
                );
                NS_ERROR_FILE_COPY_OR_MOVE_FAILED
            })?;

        trace!("get_build_id_bytes: {:?}", section_header_bytes);
        elf.section_headers =
            SectionHeader::parse_from(&section_header_bytes, 0, elf_head.e_shnum as usize, context)
                .map_err(|e| {
                    error!(
                        "get_build_id_bytes: failed to parse sectiion headers with error {}",
                        e
                    );
                    NS_ERROR_INVALID_POINTER
                })?;

        trace!("get_build_id_bytes: {:?}", elf.section_headers);
        let shdr_strtab = &elf.section_headers[elf_head.e_shstrndx as usize];
        let shdr_strtab_bytes = self
            .copy_bytes(shdr_strtab.sh_offset as usize, shdr_strtab.sh_size as usize)
            .map_err(|e| {
                error!("get_build_id_bytes: failed to get section header string tab bytes with error {}", e);
                NS_ERROR_FILE_COPY_OR_MOVE_FAILED
            })?;

        trace!("get_build_id_bytes: {:?}", shdr_strtab_bytes);
        elf.shdr_strtab =
            goblin::strtab::Strtab::parse(&shdr_strtab_bytes, 0, shdr_strtab.sh_size as usize, 0x0)
                .map_err(|e| {
                    error!(
                "get_build_id_bytes: failed to parse section header string tab with error {}",
                e
            );
                    NS_ERROR_ILLEGAL_VALUE
                })?;

        trace!("get_build_id_bytes: {:?}", elf.shdr_strtab);
        let tk_note = elf
            .section_headers
            .iter()
            .find(|s| elf.shdr_strtab.get_at(s.sh_name) == Some(note_name))
            .ok_or(NS_ERROR_NOT_AVAILABLE)?;

        trace!("get_build_id_bytes: {:?}", tk_note);
        let note_bytes = self
            .copy_bytes(tk_note.sh_offset as usize, tk_note.sh_size as usize)
            .map_err(|e| {
                error!(
                    "get_build_id_bytes: failed to copy bytes for note with error {}",
                    e
                );
                NS_ERROR_FILE_COPY_OR_MOVE_FAILED
            })?;

        trace!("get_build_id_bytes: {:?}", note_bytes);
        let (note, _size) = Note::try_from_ctx(&note_bytes, (4, context)).map_err(|e| {
            error!("get_build_id_bytes: failed to parse note with error {}", e);
            NS_ERROR_CANNOT_CONVERT_DATA
        })?;

        trace!("get_build_id_bytes: {:?}", note);
        Ok(note.desc.to_vec())
    }
}
