/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use nserror::{
    nsresult, NS_ERROR_FILE_COPY_OR_MOVE_FAILED, NS_ERROR_ILLEGAL_VALUE, NS_ERROR_INVALID_ARG,
    NS_ERROR_INVALID_POINTER, NS_ERROR_INVALID_SIGNATURE, NS_ERROR_NOT_AVAILABLE,
    NS_ERROR_NOT_INITIALIZED,
};

use goblin::mach;

use super::BuildIdReader;
use crate::reader::MAX_BUFFER_READ;

use log::{error, trace};

impl BuildIdReader {
    pub fn get_build_id_bytes(
        &mut self,
        buffer: &[u8],
        note_name: &str,
    ) -> Result<Vec<u8>, nsresult> {
        trace!("get_build_id_bytes: {}", note_name);
        let buf: [u8; MAX_BUFFER_READ] = buffer.try_into().map_err(|e| {
            error!(
                "get_build_id_bytes: failed to get buffer of {} bytes with error {}",
                MAX_BUFFER_READ, e
            );
            NS_ERROR_NOT_INITIALIZED
        })?;
        let (section, note) = note_name
            .split_once(",")
            .ok_or(NS_ERROR_INVALID_SIGNATURE)?;
        trace!("get_build_id_bytes: {} {}", section, note);

        let macho_head = mach::header::Header64::from_bytes(&buf);
        let mut address = MAX_BUFFER_READ;
        let end_of_commands = address + (macho_head.sizeofcmds as usize);

        while address < end_of_commands {
            let command = unsafe {
                self.copy::<mach::load_command::LoadCommandHeader>(address as usize)
                    .map_err(|e| {
                        error!(
                            "get_build_id_bytes: failed to load MachO command at {} with error {}",
                            address, e
                        );
                        NS_ERROR_INVALID_ARG
                    })?
            };
            trace!("get_build_id_bytes: {:?}", command);

            if command.cmd == mach::load_command::LC_SEGMENT_64 {
                let segment = unsafe {
                    self.copy::<mach::load_command::SegmentCommand64>(address as usize)
                        .map_err(|e| {
                            error!("get_build_id_bytes: failed to load MachO segment at {} with error {}", address, e);
                            NS_ERROR_INVALID_POINTER
                        })?
                };
                trace!("get_build_id_bytes: {:?}", segment);

                let name = segment.name().map_err(|e| {
                    error!(
                        "get_build_id_bytes: failed to get segment name with error {}",
                        e
                    );
                    NS_ERROR_ILLEGAL_VALUE
                })?;
                if name == section {
                    let sections_addr =
                        address + std::mem::size_of::<mach::load_command::SegmentCommand64>();
                    let sections = unsafe {
                        self.copy_array::<mach::load_command::Section64>(
                            sections_addr as usize,
                            segment.nsects as usize,
                        )
                        .map_err(|e| {
                            error!("get_build_id_bytes: failed to get MachO sections at {} with error {}", sections_addr, e);
                            NS_ERROR_FILE_COPY_OR_MOVE_FAILED
                        })?
                    };
                    trace!("get_build_id_bytes: {:?}", sections);

                    for section in &sections {
                        trace!("get_build_id_bytes: {:?}", section);
                        if let Some(sname) = Self::string_from_bytes(&section.sectname) {
                            if (sname.len() == 0) || (sname != note) {
                                continue;
                            }

                            return self
                                .copy_bytes(section.addr as usize, section.size as usize)
                                .map_err(|e| {
                                    error!("get_build_id_bytes: failed to copy section bytes at {} ({} bytes) with error {}", section.addr, section.size, e);
                                    NS_ERROR_FILE_COPY_OR_MOVE_FAILED
                                });
                        }
                    }
                }
            }
            address += command.cmdsize as usize;
        }

        Err(NS_ERROR_NOT_AVAILABLE)
    }
}
