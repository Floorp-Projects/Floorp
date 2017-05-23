//! Structs for reading a ZIP archive

use crc32::Crc32Reader;
use compression::CompressionMethod;
use spec;
use result::{ZipResult, ZipError};
use std::io;
use std::io::prelude::*;
use std::collections::HashMap;
use flate2;
use flate2::FlateReadExt;
use podio::{ReadPodExt, LittleEndian};
use types::{ZipFileData, System};
use cp437::FromCp437;
use msdos_time::{TmMsDosExt, MsDosDateTime};

#[cfg(feature = "bzip2")]
use bzip2::read::BzDecoder;

mod ffi {
    pub const S_IFDIR: u32 = 0o0040000;
    pub const S_IFREG: u32 = 0o0100000;
}

/// Wrapper for reading the contents of a ZIP file.
///
/// ```
/// fn doit() -> zip::result::ZipResult<()>
/// {
///     use std::io::prelude::*;
///
///     // For demonstration purposes we read from an empty buffer.
///     // Normally a File object would be used.
///     let buf: &[u8] = &[0u8; 128];
///     let mut reader = std::io::Cursor::new(buf);
///
///     let mut zip = try!(zip::ZipArchive::new(reader));
///
///     for i in 0..zip.len()
///     {
///         let mut file = zip.by_index(i).unwrap();
///         println!("Filename: {}", file.name());
///         let first_byte = try!(file.bytes().next().unwrap());
///         println!("{}", first_byte);
///     }
///     Ok(())
/// }
///
/// println!("Result: {:?}", doit());
/// ```
#[derive(Debug)]
pub struct ZipArchive<R: Read + io::Seek>
{
    reader: R,
    files: Vec<ZipFileData>,
    names_map: HashMap<String, usize>,
}

enum ZipFileReader<'a> {
    Stored(Crc32Reader<io::Take<&'a mut Read>>),
    Deflated(Crc32Reader<flate2::read::DeflateDecoder<io::Take<&'a mut Read>>>),
    #[cfg(feature = "bzip2")]
    Bzip2(Crc32Reader<BzDecoder<io::Take<&'a mut Read>>>),
}

/// A struct for reading a zip file
pub struct ZipFile<'a> {
    data: &'a ZipFileData,
    reader: ZipFileReader<'a>,
}

fn unsupported_zip_error<T>(detail: &'static str) -> ZipResult<T>
{
    Err(ZipError::UnsupportedArchive(detail))
}

impl<R: Read+io::Seek> ZipArchive<R>
{
    /// Opens a Zip archive and parses the central directory
    pub fn new(mut reader: R) -> ZipResult<ZipArchive<R>> {
        let footer = try!(spec::CentralDirectoryEnd::find_and_parse(&mut reader));

        if footer.disk_number != footer.disk_with_central_directory { return unsupported_zip_error("Support for multi-disk files is not implemented") }

        let directory_start = footer.central_directory_offset as u64;
        let number_of_files = footer.number_of_files_on_this_disk as usize;

        let mut files = Vec::with_capacity(number_of_files);
        let mut names_map = HashMap::new();

        try!(reader.seek(io::SeekFrom::Start(directory_start)));
        for _ in 0 .. number_of_files
        {
            let file = try!(central_header_to_zip_file(&mut reader));
            names_map.insert(file.file_name.clone(), files.len());
            files.push(file);
        }

        Ok(ZipArchive { reader: reader, files: files, names_map: names_map })
    }

    /// Number of files contained in this zip.
    ///
    /// ```
    /// fn iter() {
    ///     let mut zip = zip::ZipArchive::new(std::io::Cursor::new(vec![])).unwrap();
    ///
    ///     for i in 0..zip.len() {
    ///         let mut file = zip.by_index(i).unwrap();
    ///         // Do something with file i
    ///     }
    /// }
    /// ```
    pub fn len(&self) -> usize
    {
        self.files.len()
    }

    /// Search for a file entry by name
    pub fn by_name<'a>(&'a mut self, name: &str) -> ZipResult<ZipFile<'a>>
    {
        let index = match self.names_map.get(name) {
            Some(index) => *index,
            None => { return Err(ZipError::FileNotFound); },
        };
        self.by_index(index)
    }

    /// Get a contained file by index
    pub fn by_index<'a>(&'a mut self, file_number: usize) -> ZipResult<ZipFile<'a>>
    {
        if file_number >= self.files.len() { return Err(ZipError::FileNotFound); }
        let ref data = self.files[file_number];
        let pos = data.data_start;

        if data.encrypted
        {
            return unsupported_zip_error("Encrypted files are not supported")
        }

        try!(self.reader.seek(io::SeekFrom::Start(pos)));
        let limit_reader = (self.reader.by_ref() as &mut Read).take(data.compressed_size);

        let reader = match data.compression_method
        {
            CompressionMethod::Stored =>
            {
                ZipFileReader::Stored(Crc32Reader::new(
                    limit_reader,
                    data.crc32))
            },
            CompressionMethod::Deflated =>
            {
                let deflate_reader = limit_reader.deflate_decode();
                ZipFileReader::Deflated(Crc32Reader::new(
                    deflate_reader,
                    data.crc32))
            },
            #[cfg(feature = "bzip2")]
            CompressionMethod::Bzip2 =>
            {
                let bzip2_reader = BzDecoder::new(limit_reader);
                ZipFileReader::Bzip2(Crc32Reader::new(
                    bzip2_reader,
                    data.crc32))
            },
            _ => return unsupported_zip_error("Compression method not supported"),
        };
        Ok(ZipFile { reader: reader, data: data })
    }

    /// Unwrap and return the inner reader object
    ///
    /// The position of the reader is undefined.
    pub fn into_inner(self) -> R
    {
        self.reader
    }
}

fn central_header_to_zip_file<R: Read+io::Seek>(reader: &mut R) -> ZipResult<ZipFileData>
{
    // Parse central header
    let signature = try!(reader.read_u32::<LittleEndian>());
    if signature != spec::CENTRAL_DIRECTORY_HEADER_SIGNATURE
    {
        return Err(ZipError::InvalidArchive("Invalid Central Directory header"))
    }

    let version_made_by = try!(reader.read_u16::<LittleEndian>());
    let _version_to_extract = try!(reader.read_u16::<LittleEndian>());
    let flags = try!(reader.read_u16::<LittleEndian>());
    let encrypted = flags & 1 == 1;
    let is_utf8 = flags & (1 << 11) != 0;
    let compression_method = try!(reader.read_u16::<LittleEndian>());
    let last_mod_time = try!(reader.read_u16::<LittleEndian>());
    let last_mod_date = try!(reader.read_u16::<LittleEndian>());
    let crc32 = try!(reader.read_u32::<LittleEndian>());
    let compressed_size = try!(reader.read_u32::<LittleEndian>());
    let uncompressed_size = try!(reader.read_u32::<LittleEndian>());
    let file_name_length = try!(reader.read_u16::<LittleEndian>()) as usize;
    let extra_field_length = try!(reader.read_u16::<LittleEndian>()) as usize;
    let file_comment_length = try!(reader.read_u16::<LittleEndian>()) as usize;
    let _disk_number = try!(reader.read_u16::<LittleEndian>());
    let _internal_file_attributes = try!(reader.read_u16::<LittleEndian>());
    let external_file_attributes = try!(reader.read_u32::<LittleEndian>());
    let offset = try!(reader.read_u32::<LittleEndian>()) as u64;
    let file_name_raw = try!(ReadPodExt::read_exact(reader, file_name_length));
    let extra_field = try!(ReadPodExt::read_exact(reader, extra_field_length));
    let file_comment_raw  = try!(ReadPodExt::read_exact(reader, file_comment_length));

    let file_name = match is_utf8
    {
        true => String::from_utf8_lossy(&*file_name_raw).into_owned(),
        false => file_name_raw.from_cp437(),
    };
    let file_comment = match is_utf8
    {
        true => String::from_utf8_lossy(&*file_comment_raw).into_owned(),
        false => file_comment_raw.from_cp437(),
    };

    // Remember end of central header
    let return_position = try!(reader.seek(io::SeekFrom::Current(0)));

    // Parse local header
    try!(reader.seek(io::SeekFrom::Start(offset)));
    let signature = try!(reader.read_u32::<LittleEndian>());
    if signature != spec::LOCAL_FILE_HEADER_SIGNATURE
    {
        return Err(ZipError::InvalidArchive("Invalid local file header"))
    }

    try!(reader.seek(io::SeekFrom::Current(22)));
    let file_name_length = try!(reader.read_u16::<LittleEndian>()) as u64;
    let extra_field_length = try!(reader.read_u16::<LittleEndian>()) as u64;
    let magic_and_header = 4 + 22 + 2 + 2;
    let data_start = offset + magic_and_header + file_name_length + extra_field_length;

    // Construct the result
    let mut result = ZipFileData
    {
        system: System::from_u8((version_made_by >> 8) as u8),
        version_made_by: version_made_by as u8,
        encrypted: encrypted,
        compression_method: CompressionMethod::from_u16(compression_method),
        last_modified_time: try!(::time::Tm::from_msdos(MsDosDateTime::new(last_mod_time, last_mod_date))),
        crc32: crc32,
        compressed_size: compressed_size as u64,
        uncompressed_size: uncompressed_size as u64,
        file_name: file_name,
        file_comment: file_comment,
        header_start: offset,
        data_start: data_start,
        external_attributes: external_file_attributes,
    };

    try!(parse_extra_field(&mut result, &*extra_field));

    // Go back after the central header
    try!(reader.seek(io::SeekFrom::Start(return_position)));

    Ok(result)
}

fn parse_extra_field(_file: &mut ZipFileData, data: &[u8]) -> ZipResult<()>
{
    let mut reader = io::Cursor::new(data);

    while (reader.position() as usize) < data.len()
    {
        let kind = try!(reader.read_u16::<LittleEndian>());
        let len = try!(reader.read_u16::<LittleEndian>());
        match kind
        {
            _ => try!(reader.seek(io::SeekFrom::Current(len as i64))),
        };
    }
    Ok(())
}

/// Methods for retreiving information on zip files
impl<'a> ZipFile<'a> {
    fn get_reader(&mut self) -> &mut Read {
        match self.reader {
           ZipFileReader::Stored(ref mut r) => r as &mut Read,
           ZipFileReader::Deflated(ref mut r) => r as &mut Read,
           #[cfg(feature = "bzip2")]
           ZipFileReader::Bzip2(ref mut r) => r as &mut Read,
        }
    }
    /// Get the version of the file
    pub fn version_made_by(&self) -> (u8, u8) {
        (self.data.version_made_by / 10, self.data.version_made_by % 10)
    }
    /// Get the name of the file
    pub fn name(&self) -> &str {
        &*self.data.file_name
    }
    /// Get the comment of the file
    pub fn comment(&self) -> &str {
        &*self.data.file_comment
    }
    /// Get the compression method used to store the file
    pub fn compression(&self) -> CompressionMethod {
        self.data.compression_method
    }
    /// Get the size of the file in the archive
    pub fn compressed_size(&self) -> u64 {
        self.data.compressed_size
    }
    /// Get the size of the file when uncompressed
    pub fn size(&self) -> u64 {
        self.data.uncompressed_size
    }
    /// Get the time the file was last modified
    pub fn last_modified(&self) -> ::time::Tm {
        self.data.last_modified_time
    }
    /// Get unix mode for the file
    pub fn unix_mode(&self) -> Option<u32> {
        match self.data.system {
            System::Unix => {
                Some(self.data.external_attributes >> 16)
            },
            System::Dos => {
                // Interpret MSDOS directory bit
                let mut mode = if 0x10 == (self.data.external_attributes & 0x10) {
                    ffi::S_IFDIR | 0o0775
                } else {
                    ffi::S_IFREG | 0o0664
                };
                if 0x01 == (self.data.external_attributes & 0x01) {
                    // Read-only bit; strip write permissions
                    mode &= 0o0555;
                }
                Some(mode)
            },
            _ => None,
        }
    }
    /// Get the CRC32 hash of the original file
    pub fn crc32(&self) -> u32 {
        self.data.crc32
    }
}

impl<'a> Read for ZipFile<'a> {
     fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
         self.get_reader().read(buf)
     }
}
