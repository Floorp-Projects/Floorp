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

use log::{error, trace};

const HEADER_SIZE: usize = std::mem::size_of::<goblin::mach::header::Header64>();

impl BuildIdReader {
    pub fn get_build_id_bytes(
        &mut self,
        buffer: &[u8],
        note_name: &str,
    ) -> Result<Vec<u8>, nsresult> {
        trace!("get_build_id_bytes: {}", note_name);
        let (section, note) = note_name
            .split_once(",")
            .ok_or(NS_ERROR_INVALID_SIGNATURE)?;
        trace!("get_build_id_bytes: {} {}", section, note);

        let fat_header = mach::fat::FatHeader::parse(buffer).map_err(|e| {
            error!("Failed FatHeader::parse(): {}", e);
            NS_ERROR_NOT_INITIALIZED
        })?;
        trace!("get_build_id_bytes: fat header: {:?}", fat_header);

        /* First we attempt to parse if there's a Fat header there
         *
         * If we have one, then we have a universal binary so we are going to
         * search the architectures to find the one we want, extract the correct
         * MachO buffer as well as the offset at which the MachO binary is.
         * Testing Universal binaries will require running gtest against a
         * Shippable build.
         *
         * If not we have a normal MachO and we directly parse the buffer we
         * have read earlier, and use a 0 offset.
         */
        let (buf /*: [u8; HEADER_SIZE] */, main_offset /*: usize */) = if fat_header.magic
            == mach::fat::FAT_CIGAM
            || fat_header.magic == mach::fat::FAT_MAGIC
        {
            let total = std::mem::size_of::<mach::fat::FatHeader>() as usize
                + (std::mem::size_of::<mach::fat::FatArch>() * fat_header.nfat_arch as usize);

            let mach_buffer = self.copy_bytes(0, total).map_err(|e| {
                error!(
                    "get_build_id_bytes: failed to copy Mach ({}) bytes with error {}",
                    total, e
                );
                NS_ERROR_FILE_COPY_OR_MOVE_FAILED
            })?;

            if let mach::Mach::Fat(multi_arch) =
                mach::Mach::parse_lossy(&mach_buffer).map_err(|e| {
                    error!("Failed Mach::parse_lossy(): {}", e);
                    NS_ERROR_NOT_INITIALIZED
                })?
            {
                let arches = multi_arch.arches().map_err(|e| {
                    error!("Error getting arches(): {}", e);
                    NS_ERROR_FILE_COPY_OR_MOVE_FAILED
                })?;

                #[cfg(target_arch = "x86_64")]
                let that_arch = mach::constants::cputype::CPU_TYPE_X86_64;
                #[cfg(target_arch = "aarch64")]
                let that_arch = mach::constants::cputype::CPU_TYPE_ARM64;

                let arch_index = arches
                    .iter()
                    .position(|&x| x.cputype == that_arch)
                    .ok_or(NS_ERROR_FILE_COPY_OR_MOVE_FAILED)?;
                trace!("get_build_id_bytes: arches[]: {:?}", arches[arch_index]);

                let offset = arches[arch_index].offset as usize;

                if let Ok(b) = self
                    .copy_bytes(offset, HEADER_SIZE)
                    .map_err(|e| {
                        error!("get_build_id_bytes: failed to copy Mach-O header bytes at {} ({} bytes) with error {}", offset, HEADER_SIZE, e);
                        NS_ERROR_FILE_COPY_OR_MOVE_FAILED
                    })?.try_into() {
                    (b, offset)
                } else {
                    return Err(NS_ERROR_FILE_COPY_OR_MOVE_FAILED);
                }
            } else {
                return Err(NS_ERROR_INVALID_ARG);
            }
        } else {
            (
                buffer.try_into().map_err(|e| {
                    error!(
                        "get_build_id_bytes: failed to get buffer of {} bytes with error {}",
                        HEADER_SIZE, e
                    );
                    NS_ERROR_NOT_INITIALIZED
                })?,
                0,
            )
        };
        trace!("get_build_id_bytes: {} {}", section, note);

        let macho_head = mach::header::Header64::from_bytes(&buf);
        let mut address = main_offset + HEADER_SIZE;
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
            trace!("get_build_id_bytes: command {:?}", command);

            if command.cmd == mach::load_command::LC_SEGMENT_64 {
                let segment = unsafe {
                    self.copy::<mach::load_command::SegmentCommand64>(address as usize)
                        .map_err(|e| {
                            error!("get_build_id_bytes: failed to load MachO segment at {} with error {}", address, e);
                            NS_ERROR_INVALID_POINTER
                        })?
                };
                trace!("get_build_id_bytes: segment {:?}", segment);

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
                    trace!("get_build_id_bytes: sections {:?}", sections);

                    for section in &sections {
                        trace!("get_build_id_bytes: section {:?}", section);
                        if let Some(sname) = Self::string_from_bytes(&section.sectname) {
                            trace!("get_build_id_bytes: sname {:?}", sname);
                            if (sname.len() == 0) || (sname != note) {
                                continue;
                            }

                            return self
                                .copy_bytes(main_offset + section.addr as usize, section.size as usize)
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
