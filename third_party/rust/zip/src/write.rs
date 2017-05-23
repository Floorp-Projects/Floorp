//! Structs for creating a new zip archive

use compression::CompressionMethod;
use types::{ZipFileData, System, DEFAULT_VERSION};
use spec;
use crc32;
use result::{ZipResult, ZipError};
use std::default::Default;
use std::io;
use std::io::prelude::*;
use std::mem;
use std::ascii::AsciiExt;
use time;
use flate2;
use flate2::FlateWriteExt;
use flate2::write::DeflateEncoder;
use podio::{WritePodExt, LittleEndian};
use msdos_time::TmMsDosExt;

#[cfg(feature = "bzip2")]
use bzip2;
#[cfg(feature = "bzip2")]
use bzip2::write::BzEncoder;

enum GenericZipWriter<W: Write + io::Seek>
{
    Closed,
    Storer(W),
    Deflater(DeflateEncoder<W>),
    #[cfg(feature = "bzip2")]
    Bzip2(BzEncoder<W>),
}

/// Generator for ZIP files.
///
/// ```
/// fn doit() -> zip::result::ZipResult<()>
/// {
///     use std::io::Write;
///
///     // For this example we write to a buffer, but normally you should use a File
///     let mut buf: &mut [u8] = &mut [0u8; 65536];
///     let mut w = std::io::Cursor::new(buf);
///     let mut zip = zip::ZipWriter::new(w);
///
///     try!(zip.start_file("hello_world.txt", zip::CompressionMethod::Stored));
///     try!(zip.write(b"Hello, World!"));
///
///     // Optionally finish the zip. (this is also done on drop)
///     try!(zip.finish());
///
///     Ok(())
/// }
///
/// println!("Result: {:?}", doit());
/// ```
pub struct ZipWriter<W: Write + io::Seek>
{
    inner: GenericZipWriter<W>,
    files: Vec<ZipFileData>,
    stats: ZipWriterStats,
}

#[derive(Default)]
struct ZipWriterStats
{
    crc32: u32,
    start: u64,
    bytes_written: u64,
}

impl<W: Write+io::Seek> Write for ZipWriter<W>
{
    fn write(&mut self, buf: &[u8]) -> io::Result<usize>
    {
        if self.files.len() == 0 { return Err(io::Error::new(io::ErrorKind::Other, "No file has been started")) }
        match self.inner.ref_mut()
        {
            Some(ref mut w) => {
                let write_result = w.write(buf);
                if let Ok(count) = write_result {
                    self.stats.update(&buf[0..count]);
                }
                write_result

            }
            None => Err(io::Error::new(io::ErrorKind::BrokenPipe, "ZipWriter was already closed")),
        }
    }

    fn flush(&mut self) -> io::Result<()>
    {
        match self.inner.ref_mut()
        {
            Some(ref mut w) => w.flush(),
            None => Err(io::Error::new(io::ErrorKind::BrokenPipe, "ZipWriter was already closed")),
        }
    }
}

impl ZipWriterStats
{
    fn update(&mut self, buf: &[u8])
    {
        self.crc32 = crc32::update(self.crc32, buf);
        self.bytes_written += buf.len() as u64;
    }
}

impl<W: Write+io::Seek> ZipWriter<W>
{
    /// Initializes the ZipWriter.
    ///
    /// Before writing to this object, the start_file command should be called.
    pub fn new(inner: W) -> ZipWriter<W>
    {
        ZipWriter
        {
            inner: GenericZipWriter::Storer(inner),
            files: Vec::new(),
            stats: Default::default(),
        }
    }

    /// Start a new file for with the requested compression method.
    pub fn start_file<S>(&mut self, name: S, compression: CompressionMethod) -> ZipResult<()>
        where S: Into<String>
    {
        try!(self.finish_file());

        {
            let writer = self.inner.get_plain();
            let header_start = try!(writer.seek(io::SeekFrom::Current(0)));

            let mut file = ZipFileData
            {
                system: System::Dos,
                version_made_by: DEFAULT_VERSION,
                encrypted: false,
                compression_method: compression,
                last_modified_time: time::now(),
                crc32: 0,
                compressed_size: 0,
                uncompressed_size: 0,
                file_name: name.into(),
                file_comment: String::new(),
                header_start: header_start,
                data_start: 0,
                external_attributes: 0,
            };
            try!(write_local_file_header(writer, &file));

            let header_end = try!(writer.seek(io::SeekFrom::Current(0)));
            self.stats.start = header_end;
            file.data_start = header_end;

            self.stats.bytes_written = 0;
            self.stats.crc32 = 0;

            self.files.push(file);
        }

        try!(self.inner.switch_to(compression));

        Ok(())
    }

    fn finish_file(&mut self) -> ZipResult<()>
    {
        try!(self.inner.switch_to(CompressionMethod::Stored));
        let writer = self.inner.get_plain();

        let file = match self.files.last_mut()
        {
            None => return Ok(()),
            Some(f) => f,
        };
        file.crc32 = self.stats.crc32;
        file.uncompressed_size = self.stats.bytes_written;
        file.compressed_size = try!(writer.seek(io::SeekFrom::Current(0))) - self.stats.start;

        try!(update_local_file_header(writer, file));
        try!(writer.seek(io::SeekFrom::End(0)));
        Ok(())
    }

    /// Finish the last file and write all other zip-structures
    ///
    /// This will return the writer, but one should normally not append any data to the end of the file.  
    /// Note that the zipfile will also be finished on drop.
    pub fn finish(&mut self) -> ZipResult<W>
    {
        try!(self.finalize());
        let inner = mem::replace(&mut self.inner, GenericZipWriter::Closed);
        Ok(inner.unwrap())
    }

    fn finalize(&mut self) -> ZipResult<()>
    {
        try!(self.finish_file());

        {
            let writer = self.inner.get_plain();

            let central_start = try!(writer.seek(io::SeekFrom::Current(0)));
            for file in self.files.iter()
            {
                try!(write_central_directory_header(writer, file));
            }
            let central_size = try!(writer.seek(io::SeekFrom::Current(0))) - central_start;

            let footer = spec::CentralDirectoryEnd
            {
                disk_number: 0,
                disk_with_central_directory: 0,
                number_of_files_on_this_disk: self.files.len() as u16,
                number_of_files: self.files.len() as u16,
                central_directory_size: central_size as u32,
                central_directory_offset: central_start as u32,
                zip_file_comment: b"zip-rs".to_vec(),
            };

            try!(footer.write(writer));
        }

        Ok(())
    }
}

impl<W: Write+io::Seek> Drop for ZipWriter<W>
{
    fn drop(&mut self)
    {
        if !self.inner.is_closed()
        {
            if let Err(e) = self.finalize() {
                let _ = write!(&mut io::stderr(), "ZipWriter drop failed: {:?}", e);
            }
        }
    }
}

impl<W: Write+io::Seek> GenericZipWriter<W>
{
    fn switch_to(&mut self, compression: CompressionMethod) -> ZipResult<()>
    {
        match self.current_compression() {
            Some(method) if method == compression => return Ok(()),
            None => try!(Err(io::Error::new(io::ErrorKind::BrokenPipe, "ZipWriter was already closed"))),
            _ => {},
        }

        let bare = match mem::replace(self, GenericZipWriter::Closed)
        {
            GenericZipWriter::Storer(w) => w,
            GenericZipWriter::Deflater(w) => try!(w.finish()),
            #[cfg(feature = "bzip2")]
            GenericZipWriter::Bzip2(w) => try!(w.finish()),
            GenericZipWriter::Closed => try!(Err(io::Error::new(io::ErrorKind::BrokenPipe, "ZipWriter was already closed"))),
        };

        *self = match compression
        {
            CompressionMethod::Stored => GenericZipWriter::Storer(bare),
            CompressionMethod::Deflated => GenericZipWriter::Deflater(bare.deflate_encode(flate2::Compression::Default)),
            #[cfg(feature = "bzip2")]
            CompressionMethod::Bzip2 => GenericZipWriter::Bzip2(BzEncoder::new(bare, bzip2::Compression::Default)),
            CompressionMethod::Unsupported(..) => return Err(ZipError::UnsupportedArchive("Unsupported compression")),
        };

        Ok(())
    }

    fn ref_mut(&mut self) -> Option<&mut Write> {
        match *self {
            GenericZipWriter::Storer(ref mut w) => Some(w as &mut Write),
            GenericZipWriter::Deflater(ref mut w) => Some(w as &mut Write),
            #[cfg(feature = "bzip2")]
            GenericZipWriter::Bzip2(ref mut w) => Some(w as &mut Write),
            GenericZipWriter::Closed => None,
        }
    }

    fn is_closed(&self) -> bool
    {
        match *self
        {
            GenericZipWriter::Closed => true,
            _ => false,
        }
    }

    fn get_plain(&mut self) -> &mut W
    {
        match *self
        {
            GenericZipWriter::Storer(ref mut w) => w,
            _ => panic!("Should have switched to stored beforehand"),
        }
    }

    fn current_compression(&self) -> Option<CompressionMethod> {
        match *self {
            GenericZipWriter::Storer(..) => Some(CompressionMethod::Stored),
            GenericZipWriter::Deflater(..) => Some(CompressionMethod::Deflated),
            #[cfg(feature = "bzip2")]
            GenericZipWriter::Bzip2(..) => Some(CompressionMethod::Bzip2),
            GenericZipWriter::Closed => None,
        }
    }

    fn unwrap(self) -> W
    {
        match self
        {
            GenericZipWriter::Storer(w) => w,
            _ => panic!("Should have switched to stored beforehand"),
        }
    }
}

fn write_local_file_header<T: Write>(writer: &mut T, file: &ZipFileData) -> ZipResult<()>
{
    try!(writer.write_u32::<LittleEndian>(spec::LOCAL_FILE_HEADER_SIGNATURE));
    let version_made_by = (file.system as u16) << 8 | (file.version_made_by as u16);
    try!(writer.write_u16::<LittleEndian>(version_made_by));
    let flag = if !file.file_name.is_ascii() { 1u16 << 11 } else { 0 };
    try!(writer.write_u16::<LittleEndian>(flag));
    try!(writer.write_u16::<LittleEndian>(file.compression_method.to_u16()));
    let msdos_datetime = try!(file.last_modified_time.to_msdos());
    try!(writer.write_u16::<LittleEndian>(msdos_datetime.timepart));
    try!(writer.write_u16::<LittleEndian>(msdos_datetime.datepart));
    try!(writer.write_u32::<LittleEndian>(file.crc32));
    try!(writer.write_u32::<LittleEndian>(file.compressed_size as u32));
    try!(writer.write_u32::<LittleEndian>(file.uncompressed_size as u32));
    try!(writer.write_u16::<LittleEndian>(file.file_name.as_bytes().len() as u16));
    let extra_field = try!(build_extra_field(file));
    try!(writer.write_u16::<LittleEndian>(extra_field.len() as u16));
    try!(writer.write_all(file.file_name.as_bytes()));
    try!(writer.write_all(&extra_field));

    Ok(())
}

fn update_local_file_header<T: Write+io::Seek>(writer: &mut T, file: &ZipFileData) -> ZipResult<()>
{
    static CRC32_OFFSET : u64 = 14;
    try!(writer.seek(io::SeekFrom::Start(file.header_start + CRC32_OFFSET)));
    try!(writer.write_u32::<LittleEndian>(file.crc32));
    try!(writer.write_u32::<LittleEndian>(file.compressed_size as u32));
    try!(writer.write_u32::<LittleEndian>(file.uncompressed_size as u32));
    Ok(())
}

fn write_central_directory_header<T: Write>(writer: &mut T, file: &ZipFileData) -> ZipResult<()>
{
    try!(writer.write_u32::<LittleEndian>(spec::CENTRAL_DIRECTORY_HEADER_SIGNATURE));
    try!(writer.write_u16::<LittleEndian>(0x14FF));
    try!(writer.write_u16::<LittleEndian>(20));
    let flag = if !file.file_name.is_ascii() { 1u16 << 11 } else { 0 };
    try!(writer.write_u16::<LittleEndian>(flag));
    try!(writer.write_u16::<LittleEndian>(file.compression_method.to_u16()));
    let msdos_datetime = try!(file.last_modified_time.to_msdos());
    try!(writer.write_u16::<LittleEndian>(msdos_datetime.timepart));
    try!(writer.write_u16::<LittleEndian>(msdos_datetime.datepart));
    try!(writer.write_u32::<LittleEndian>(file.crc32));
    try!(writer.write_u32::<LittleEndian>(file.compressed_size as u32));
    try!(writer.write_u32::<LittleEndian>(file.uncompressed_size as u32));
    try!(writer.write_u16::<LittleEndian>(file.file_name.as_bytes().len() as u16));
    let extra_field = try!(build_extra_field(file));
    try!(writer.write_u16::<LittleEndian>(extra_field.len() as u16));
    try!(writer.write_u16::<LittleEndian>(0));
    try!(writer.write_u16::<LittleEndian>(0));
    try!(writer.write_u16::<LittleEndian>(0));
    try!(writer.write_u32::<LittleEndian>(0));
    try!(writer.write_u32::<LittleEndian>(file.header_start as u32));
    try!(writer.write_all(file.file_name.as_bytes()));
    try!(writer.write_all(&extra_field));

    Ok(())
}

fn build_extra_field(_file: &ZipFileData) -> ZipResult<Vec<u8>>
{
    let writer = Vec::new();
    // Future work
    Ok(writer)
}
