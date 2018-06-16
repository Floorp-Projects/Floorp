//! Types that specify what is contained in a ZIP.

use time;

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum System
{
    Dos = 0,
    Unix = 3,
    Unknown,
    #[doc(hidden)]
    __Nonexhaustive,
}

impl System {
    pub fn from_u8(system: u8) -> System
    {
        use self::System::*;

        match system {
            0 => Dos,
            3 => Unix,
            _ => Unknown,
        }
    }
}

pub const DEFAULT_VERSION: u8 = 46;

/// Structure representing a ZIP file.
#[derive(Debug)]
pub struct ZipFileData
{
    /// Compatibility of the file attribute information
    pub system: System,
    /// Specification version
    pub version_made_by: u8,
    /// True if the file is encrypted.
    pub encrypted: bool,
    /// Compression method used to store the file
    pub compression_method: ::compression::CompressionMethod,
    /// Last modified time. This will only have a 2 second precision.
    pub last_modified_time: time::Tm,
    /// CRC32 checksum
    pub crc32: u32,
    /// Size of the file in the ZIP
    pub compressed_size: u64,
    /// Size of the file when extracted
    pub uncompressed_size: u64,
    /// Name of the file
    pub file_name: String,
    /// Raw file name. To be used when file_name was incorrectly decoded.
    pub file_name_raw: Vec<u8>,
    /// File comment
    pub file_comment: String,
    /// Specifies where the local header of the file starts
    pub header_start: u64,
    /// Specifies where the compressed data of the file starts
    pub data_start: u64,
    /// External file attributes
    pub external_attributes: u32,
}

impl ZipFileData {
    pub fn file_name_sanitized(&self) -> ::std::path::PathBuf {
        let no_null_filename = match self.file_name.find('\0') {
            Some(index) => &self.file_name[0..index],
            None => &self.file_name,
        };

        ::std::path::Path::new(no_null_filename)
            .components()
            .filter(|component| match *component {
                ::std::path::Component::Normal(..) => true,
                _ => false,
            })
            .fold(::std::path::PathBuf::new(), |mut path, ref cur| {
                path.push(cur.as_os_str());
                path
            })
    }

    pub fn version_needed(&self) -> u16 {
        match self.compression_method {
            #[cfg(feature = "bzip2")]
            ::compression::CompressionMethod::Bzip2 => 46,
            _ => 20,
        }
    }
}

#[cfg(test)]
mod test {
    #[test]
    fn system() {
        use super::System;
        assert_eq!(System::Dos as u16, 0u16);
        assert_eq!(System::Unix as u16, 3u16);
        assert_eq!(System::from_u8(0), System::Dos);
        assert_eq!(System::from_u8(3), System::Unix);
    }

    #[test]
    fn sanitize() {
        use super::*;
        let file_name = "/path/../../../../etc/./passwd\0/etc/shadow".to_string();
        let data = ZipFileData {
            system: System::Dos,
            version_made_by: 0,
            encrypted: false,
            compression_method: ::compression::CompressionMethod::Stored,
            last_modified_time: time::empty_tm(),
            crc32: 0,
            compressed_size: 0,
            uncompressed_size: 0,
            file_name: file_name.clone(),
            file_name_raw: file_name.into_bytes(),
            file_comment: String::new(),
            header_start: 0,
            data_start: 0,
            external_attributes: 0,
        };
        assert_eq!(data.file_name_sanitized(), ::std::path::PathBuf::from("path/etc/passwd"));
    }
}
