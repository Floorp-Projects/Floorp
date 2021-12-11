//! Structs for reading a ZIP archive

use crc32::Crc32Reader;
use compression::CompressionMethod;
use spec;
use result::{ZipResult, ZipError};
use std::io;
use std::io::prelude::*;
use std::collections::HashMap;
use std::borrow::Cow;

use podio::{ReadPodExt, LittleEndian};
use types::{ZipFileData, System};
use cp437::FromCp437;
use msdos_time::{TmMsDosExt, MsDosDateTime};

#[cfg(feature = "flate2")]
use flate2;
#[cfg(feature = "flate2")]
use flate2::read::DeflateDecoder;

#[cfg(feature = "bzip2")]
use bzip2::read::BzDecoder;

mod ffi {
    pub const S_IFDIR: u32 = 0o0040000;
    pub const S_IFREG: u32 = 0o0100000;
}

const TM_1980_01_01 : ::time::Tm = ::time::Tm {
	tm_sec: 0,
	tm_min: 0,
	tm_hour: 0,
	tm_mday: 1,
	tm_mon: 0,
	tm_year: 80,
	tm_wday: 2,
	tm_yday: 0,
	tm_isdst: -1,
	tm_utcoff: 0,
	tm_nsec: 0
};

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
    offset: u64,
    comment: Vec<u8>,
}

enum ZipFileReader<'a> {
    NoReader,
    Stored(Crc32Reader<io::Take<&'a mut Read>>),
    #[cfg(feature = "flate2")]
    Deflated(Crc32Reader<flate2::read::DeflateDecoder<io::Take<&'a mut Read>>>),
    #[cfg(feature = "bzip2")]
    Bzip2(Crc32Reader<BzDecoder<io::Take<&'a mut Read>>>),
}

/// A struct for reading a zip file
pub struct ZipFile<'a> {
    data: Cow<'a, ZipFileData>,
    reader: ZipFileReader<'a>,
}

fn unsupported_zip_error<T>(detail: &'static str) -> ZipResult<T>
{
    Err(ZipError::UnsupportedArchive(detail))
}


fn make_reader<'a>(
    compression_method: ::compression::CompressionMethod,
    crc32: u32,
    reader: io::Take<&'a mut io::Read>)
        -> ZipResult<ZipFileReader<'a>> {

    match compression_method {
        CompressionMethod::Stored =>
        {
            Ok(ZipFileReader::Stored(Crc32Reader::new(
                reader,
                crc32)))
        },
        #[cfg(feature = "flate2")]
        CompressionMethod::Deflated =>
        {
            let deflate_reader = DeflateDecoder::new(reader);
            Ok(ZipFileReader::Deflated(Crc32Reader::new(
                deflate_reader,
                crc32)))
        },
        #[cfg(feature = "bzip2")]
        CompressionMethod::Bzip2 =>
        {
            let bzip2_reader = BzDecoder::new(reader);
            Ok(ZipFileReader::Bzip2(Crc32Reader::new(
                bzip2_reader,
                crc32)))
        },
        _ => unsupported_zip_error("Compression method not supported"),
    }
}

impl<R: Read+io::Seek> ZipArchive<R>
{
    /// Get the directory start offset and number of files. This is done in a
    /// separate function to ease the control flow design.
    fn get_directory_counts(reader: &mut R,
                            footer: &spec::CentralDirectoryEnd,
                            cde_start_pos: u64) -> ZipResult<(u64, u64, usize)> {
        // See if there's a ZIP64 footer. The ZIP64 locator if present will
        // have its signature 20 bytes in front of the standard footer. The
        // standard footer, in turn, is 22+N bytes large, where N is the
        // comment length. Therefore:
        let zip64locator = if reader.seek(io::SeekFrom::End(-(20 + 22 + footer.zip_file_comment.len() as i64))).is_ok() {
            match spec::Zip64CentralDirectoryEndLocator::parse(reader) {
                Ok(loc) => Some(loc),
                Err(ZipError::InvalidArchive(_)) => {
                    // No ZIP64 header; that's actually fine. We're done here.
                    None
                },
                Err(e) => {
                    // Yikes, a real problem
                    return Err(e);
                }
            }
        }
        else {
            // Empty Zip files will have nothing else so this error might be fine. If
            // not, we'll find out soon.
            None
        };

        match zip64locator {
            None => {
                // Some zip files have data prepended to them, resulting in the
                // offsets all being too small. Get the amount of error by comparing
                // the actual file position we found the CDE at with the offset
                // recorded in the CDE.
                let archive_offset = cde_start_pos.checked_sub(footer.central_directory_size as u64)
                    .and_then(|x| x.checked_sub(footer.central_directory_offset as u64))
                    .ok_or(ZipError::InvalidArchive("Invalid central directory size or offset"))?;

                let directory_start = footer.central_directory_offset as u64 + archive_offset;
                let number_of_files = footer.number_of_files_on_this_disk as usize;
                return Ok((archive_offset, directory_start, number_of_files));
            },
            Some(locator64) => {
                // If we got here, this is indeed a ZIP64 file.

                if footer.disk_number as u32 != locator64.disk_with_central_directory {
                    return unsupported_zip_error("Support for multi-disk files is not implemented")
                }

                // We need to reassess `archive_offset`. We know where the ZIP64
                // central-directory-end structure *should* be, but unfortunately we
                // don't know how to precisely relate that location to our current
                // actual offset in the file, since there may be junk at its
                // beginning. Therefore we need to perform another search, as in
                // read::CentralDirectoryEnd::find_and_parse, except now we search
                // forward.

                let search_upper_bound = cde_start_pos
                    .checked_sub(60) // minimum size of Zip64CentralDirectoryEnd + Zip64CentralDirectoryEndLocator
                    .ok_or(ZipError::InvalidArchive("File cannot contain ZIP64 central directory end"))?;
                let (footer, archive_offset) = spec::Zip64CentralDirectoryEnd::find_and_parse(
                    reader,
                    locator64.end_of_central_directory_offset,
                    search_upper_bound)?;

                if footer.disk_number != footer.disk_with_central_directory {
                    return unsupported_zip_error("Support for multi-disk files is not implemented")
                }

                let directory_start = footer.central_directory_offset + archive_offset;
                Ok((archive_offset, directory_start, footer.number_of_files as usize))
            },
        }
    }

    /// Opens a Zip archive and parses the central directory
    pub fn new(mut reader: R) -> ZipResult<ZipArchive<R>> {
        let (footer, cde_start_pos) = try!(spec::CentralDirectoryEnd::find_and_parse(&mut reader));

        if footer.disk_number != footer.disk_with_central_directory
        {
            return unsupported_zip_error("Support for multi-disk files is not implemented")
        }

        let (archive_offset, directory_start, number_of_files) =
            try!(Self::get_directory_counts(&mut reader, &footer, cde_start_pos));

        let mut files = Vec::new();
        let mut names_map = HashMap::new();

        if let Err(_) = reader.seek(io::SeekFrom::Start(directory_start)) {
            return Err(ZipError::InvalidArchive("Could not seek to start of central directory"));
        }

        for _ in 0 .. number_of_files
        {
            let file = try!(central_header_to_zip_file(&mut reader, archive_offset));
            names_map.insert(file.file_name.clone(), files.len());
            files.push(file);
        }

        Ok(ZipArchive {
            reader: reader,
            files: files,
            names_map: names_map,
            offset: archive_offset,
            comment: footer.zip_file_comment,
        })
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

    /// Get the offset from the beginning of the underlying reader that this zip begins at, in bytes.
    ///
    /// Normally this value is zero, but if the zip has arbitrary data prepended to it, then this value will be the size
    /// of that prepended data.
    pub fn offset(&self) -> u64 {
        self.offset
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

        Ok(ZipFile { reader: try!(make_reader(data.compression_method, data.crc32, limit_reader)), data: Cow::Borrowed(data) })
    }

    /// Unwrap and return the inner reader object
    ///
    /// The position of the reader is undefined.
    pub fn into_inner(self) -> R
    {
        self.reader
    }
}

fn central_header_to_zip_file<R: Read+io::Seek>(reader: &mut R, archive_offset: u64) -> ZipResult<ZipFileData>
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
    let mut offset = try!(reader.read_u32::<LittleEndian>()) as u64;
    let file_name_raw = try!(ReadPodExt::read_exact(reader, file_name_length));
    let extra_field = try!(ReadPodExt::read_exact(reader, extra_field_length));
    let file_comment_raw  = try!(ReadPodExt::read_exact(reader, file_comment_length));

    // Account for shifted zip offsets.
    offset += archive_offset;

    let file_name = match is_utf8
    {
        true => String::from_utf8_lossy(&*file_name_raw).into_owned(),
        false => file_name_raw.clone().from_cp437(),
    };
    let file_comment = match is_utf8
    {
        true => String::from_utf8_lossy(&*file_comment_raw).into_owned(),
        false => file_comment_raw.from_cp437(),
    };

    // Construct the result
    let mut result = ZipFileData
    {
        system: System::from_u8((version_made_by >> 8) as u8),
        version_made_by: version_made_by as u8,
        encrypted: encrypted,
        compression_method: CompressionMethod::from_u16(compression_method),
        last_modified_time: ::time::Tm::from_msdos(MsDosDateTime::new(last_mod_time, last_mod_date)).unwrap_or(TM_1980_01_01),
        crc32: crc32,
        compressed_size: compressed_size as u64,
        uncompressed_size: uncompressed_size as u64,
        file_name: file_name,
        file_name_raw: file_name_raw,
        file_comment: file_comment,
        header_start: offset,
        data_start: 0,
        external_attributes: external_file_attributes,
    };

    match parse_extra_field(&mut result, &*extra_field) {
        Ok(..) | Err(ZipError::Io(..)) => {},
        Err(e) => try!(Err(e)),
    }

    // Remember end of central header
    let return_position = try!(reader.seek(io::SeekFrom::Current(0)));

    // Parse local header
    try!(reader.seek(io::SeekFrom::Start(result.header_start)));
    let signature = try!(reader.read_u32::<LittleEndian>());
    if signature != spec::LOCAL_FILE_HEADER_SIGNATURE
    {
        return Err(ZipError::InvalidArchive("Invalid local file header"))
    }

    try!(reader.seek(io::SeekFrom::Current(22)));
    let file_name_length = try!(reader.read_u16::<LittleEndian>()) as u64;
    let extra_field_length = try!(reader.read_u16::<LittleEndian>()) as u64;
    let magic_and_header = 4 + 22 + 2 + 2;
    result.data_start = result.header_start + magic_and_header + file_name_length + extra_field_length;

    // Go back after the central header
    try!(reader.seek(io::SeekFrom::Start(return_position)));

    Ok(result)
}

fn parse_extra_field(file: &mut ZipFileData, data: &[u8]) -> ZipResult<()>
{
    let mut reader = io::Cursor::new(data);

    while (reader.position() as usize) < data.len()
    {
        let kind = try!(reader.read_u16::<LittleEndian>());
        let len = try!(reader.read_u16::<LittleEndian>());
        match kind
        {
            // Zip64 extended information extra field
            0x0001 => {
                file.uncompressed_size = try!(reader.read_u64::<LittleEndian>());
                file.compressed_size = try!(reader.read_u64::<LittleEndian>());
                try!(reader.read_u64::<LittleEndian>());  // relative header offset
                try!(reader.read_u32::<LittleEndian>());  // disk start number
            },
            _ => { try!(reader.seek(io::SeekFrom::Current(len as i64))); },
        };
    }
    Ok(())
}

fn get_reader<'a>(reader: &'a mut ZipFileReader) -> &'a mut Read {
    match *reader {
        ZipFileReader::NoReader => panic!("ZipFileReader was in an invalid state"),
        ZipFileReader::Stored(ref mut r) => r as &mut Read,
        #[cfg(feature = "flate2")]
        ZipFileReader::Deflated(ref mut r) => r as &mut Read,
        #[cfg(feature = "bzip2")]
        ZipFileReader::Bzip2(ref mut r) => r as &mut Read,
    }
}

/// Methods for retreiving information on zip files
impl<'a> ZipFile<'a> {
    fn get_reader(&mut self) -> &mut Read {
        get_reader(&mut self.reader)
    }
    /// Get the version of the file
    pub fn version_made_by(&self) -> (u8, u8) {
        (self.data.version_made_by / 10, self.data.version_made_by % 10)
    }
    /// Get the name of the file
    pub fn name(&self) -> &str {
        &*self.data.file_name
    }
    /// Get the name of the file, in the raw (internal) byte representation.
    pub fn name_raw(&self) -> &[u8] {
        &*self.data.file_name_raw
    }
    /// Get the name of the file in a sanitized form. It truncates the name to the first NULL byte,
    /// removes a leading '/' and removes '..' parts.
    pub fn sanitized_name(&self) -> ::std::path::PathBuf {
        self.data.file_name_sanitized()
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
        if self.data.external_attributes == 0 {
            return None;
        }

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

    /// Get the starting offset of the data of the compressed file
    pub fn data_start(&self) -> u64 {
        self.data.data_start
    }
}

impl<'a> Read for ZipFile<'a> {
     fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
         self.get_reader().read(buf)
     }
}

impl<'a> Drop for ZipFile<'a> {
    fn drop(&mut self) {
        // self.data is Owned, this reader is constructed by a streaming reader.
        // In this case, we want to exhaust the reader so that the next file is accessible.
        if let Cow::Owned(_) = self.data {
            let mut buffer = [0; 1<<16];

            // Get the inner `Take` reader so all decompression and CRC calculation is skipped.
            let innerreader = ::std::mem::replace(&mut self.reader, ZipFileReader::NoReader);
            let mut reader = match innerreader {
                ZipFileReader::NoReader => panic!("ZipFileReader was in an invalid state"),
                ZipFileReader::Stored(crcreader) => crcreader.into_inner(),
                #[cfg(feature = "flate2")]
                ZipFileReader::Deflated(crcreader) => crcreader.into_inner().into_inner(),
                #[cfg(feature = "bzip2")]
                ZipFileReader::Bzip2(crcreader) => crcreader.into_inner().into_inner(),
            };

            loop {
                match reader.read(&mut buffer) {
                    Ok(0) => break,
                    Ok(_) => (),
                    Err(e) => panic!("Could not consume all of the output of the current ZipFile: {:?}", e),
                }
            }
        }
    }
}

/// Read ZipFile structures from a non-seekable reader.
///
/// This is an alternative method to read a zip file. If possible, use the ZipArchive functions
/// as some information will be missing when reading this manner.
///
/// Reads a file header from the start of the stream. Will return `Ok(Some(..))` if a file is
/// present at the start of the stream. Returns `Ok(None)` if the start of the central directory
/// is encountered. No more files should be read after this.
///
/// The Drop implementation of ZipFile ensures that the reader will be correctly positioned after
/// the structure is done.
///
/// Missing fields are:
/// * `comment`: set to an empty string
/// * `data_start`: set to 0
/// * `external_attributes`: `unix_mode()`: will return None
pub fn read_zipfile_from_stream<'a, R: io::Read>(reader: &'a mut R) -> ZipResult<Option<ZipFile>> {
    let signature = try!(reader.read_u32::<LittleEndian>());

    match signature {
        spec::LOCAL_FILE_HEADER_SIGNATURE => (),
        spec::CENTRAL_DIRECTORY_HEADER_SIGNATURE => return Ok(None),
        _ => return Err(ZipError::InvalidArchive("Invalid local file header")),
    }

    let version_made_by = try!(reader.read_u16::<LittleEndian>());
    let flags = try!(reader.read_u16::<LittleEndian>());
    let encrypted = flags & 1 == 1;
    let is_utf8 = flags & (1 << 11) != 0;
    let using_data_descriptor = flags & (1 << 3) != 0;
    let compression_method = CompressionMethod::from_u16(try!(reader.read_u16::<LittleEndian>()));
    let last_mod_time = try!(reader.read_u16::<LittleEndian>());
    let last_mod_date = try!(reader.read_u16::<LittleEndian>());
    let crc32 = try!(reader.read_u32::<LittleEndian>());
    let compressed_size = try!(reader.read_u32::<LittleEndian>());
    let uncompressed_size = try!(reader.read_u32::<LittleEndian>());
    let file_name_length = try!(reader.read_u16::<LittleEndian>()) as usize;
    let extra_field_length = try!(reader.read_u16::<LittleEndian>()) as usize;

    let file_name_raw = try!(ReadPodExt::read_exact(reader, file_name_length));
    let extra_field = try!(ReadPodExt::read_exact(reader, extra_field_length));

    let file_name = match is_utf8
    {
        true => String::from_utf8_lossy(&*file_name_raw).into_owned(),
        false => file_name_raw.clone().from_cp437(),
    };

    let mut result = ZipFileData
    {
        system: System::from_u8((version_made_by >> 8) as u8),
        version_made_by: version_made_by as u8,
        encrypted: encrypted,
        compression_method: compression_method,
        last_modified_time: ::time::Tm::from_msdos(MsDosDateTime::new(last_mod_time, last_mod_date)).unwrap_or(TM_1980_01_01),
        crc32: crc32,
        compressed_size: compressed_size as u64,
        uncompressed_size: uncompressed_size as u64,
        file_name: file_name,
        file_name_raw: file_name_raw,
        file_comment: String::new(),  // file comment is only available in the central directory
        // header_start and data start are not available, but also don't matter, since seeking is
        // not available.
        header_start: 0,
        data_start: 0,
        // The external_attributes field is only available in the central directory.
        // We set this to zero, which should be valid as the docs state 'If input came
        // from standard input, this field is set to zero.'
        external_attributes: 0,
    };

    match parse_extra_field(&mut result, &extra_field) {
        Ok(..) | Err(ZipError::Io(..)) => {},
        Err(e) => try!(Err(e)),
    }

    if encrypted {
        return unsupported_zip_error("Encrypted files are not supported")
    }
    if using_data_descriptor {
        return unsupported_zip_error("The file length is not available in the local header");
    }

    let limit_reader = (reader as &'a mut io::Read).take(result.compressed_size as u64);

    let result_crc32 = result.crc32;
    let result_compression_method = result.compression_method;
    Ok(Some(ZipFile {
        data: Cow::Owned(result),
        reader: try!(make_reader(result_compression_method, result_crc32, limit_reader))
    }))
}

#[cfg(test)]
mod test {
    #[test]
    fn invalid_offset() {
        use std::io;
        use super::ZipArchive;

        let mut v = Vec::new();
        v.extend_from_slice(include_bytes!("../tests/data/invalid_offset.zip"));
        let reader = ZipArchive::new(io::Cursor::new(v));
        assert!(reader.is_err());
    }

    #[test]
    fn zip64_with_leading_junk() {
        use std::io;
        use super::ZipArchive;

        let mut v = Vec::new();
        v.extend_from_slice(include_bytes!("../tests/data/zip64_demo.zip"));
        let reader = ZipArchive::new(io::Cursor::new(v)).unwrap();
        assert!(reader.len() == 1);
    }

    #[test]
    fn zip_comment() {
        use std::io;
        use super::ZipArchive;

        let mut v = Vec::new();
        v.extend_from_slice(include_bytes!("../tests/data/mimetype.zip"));
        let reader = ZipArchive::new(io::Cursor::new(v)).unwrap();
        assert!(reader.comment == b"zip-rs");
    }

    #[test]
    fn zip_read_streaming() {
        use std::io;
        use super::read_zipfile_from_stream;

        let mut v = Vec::new();
        v.extend_from_slice(include_bytes!("../tests/data/mimetype.zip"));
        let mut reader = io::Cursor::new(v);
        loop {
            match read_zipfile_from_stream(&mut reader).unwrap() {
                None => break,
                _ => (),
            }
        }
    }
}
