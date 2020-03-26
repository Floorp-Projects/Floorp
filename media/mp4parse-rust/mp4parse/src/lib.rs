//! Module for parsing ISO Base Media Format aka video/mp4 streams.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#[macro_use]
extern crate log;

extern crate bitreader;
extern crate byteorder;
extern crate num_traits;
use bitreader::{BitReader, ReadInto};
use byteorder::{ReadBytesExt, WriteBytesExt};
use num_traits::Num;
use std::convert::{TryFrom, TryInto as _};
use std::io::Cursor;
use std::io::{Read, Take};
use std::ops::{Range, RangeFrom};

#[cfg(feature = "mp4parse_fallible")]
extern crate mp4parse_fallible;

#[cfg(feature = "mp4parse_fallible")]
use mp4parse_fallible::FallibleVec;

#[macro_use]
mod macros;

mod boxes;
use boxes::{BoxType, FourCC};

// Unit tests.
#[cfg(test)]
mod tests;

// Arbitrary buffer size limit used for raw read_bufs on a box.
const BUF_SIZE_LIMIT: u64 = 10 * 1024 * 1024;

// Max table length. Calculating in worst case for one week long video, one
// frame per table entry in 30 fps.
const TABLE_SIZE_LIMIT: u32 = 30 * 60 * 60 * 24 * 7;

/// A trait to indicate a type can be infallibly converted to `u64`.
/// This should only be implemented for infallible conversions, so only unsigned types are valid.
trait ToU64 {
    fn to_u64(self) -> u64;
}

/// Statically verify that the platform `usize` can fit within a `u64`.
/// If the size won't fit on the given platform, this will fail at compile time, but if a type
/// which can fail TryInto<usize> is used, it may panic.
impl ToU64 for usize {
    fn to_u64(self) -> u64 {
        static_assertions::const_assert!(
            std::mem::size_of::<usize>() <= std::mem::size_of::<u64>()
        );
        self.try_into().expect("usize -> u64 conversion failed")
    }
}

/// A trait to indicate a type can be infallibly converted to `usize`.
/// This should only be implemented for infallible conversions, so only unsigned types are valid.
trait ToUsize {
    fn to_usize(self) -> usize;
}

/// Statically verify that the given type can fit within a `usize`.
/// If the size won't fit on the given platform, this will fail at compile time, but if a type
/// which can fail TryInto<usize> is used, it may panic.
macro_rules! impl_to_usize_from {
    ( $from_type:ty ) => {
        impl ToUsize for $from_type {
            fn to_usize(self) -> usize {
                static_assertions::const_assert!(
                    std::mem::size_of::<$from_type>() <= std::mem::size_of::<usize>()
                );
                self.try_into().expect(concat!(
                    stringify!($from_type),
                    " -> usize conversion failed"
                ))
            }
        }
    };
}

impl_to_usize_from!(u8);
impl_to_usize_from!(u16);
impl_to_usize_from!(u32);

/// Indicate the current offset (i.e., bytes already read) in a reader
trait Offset {
    fn offset(&self) -> u64;
}

/// Wraps a reader to track the current offset
struct OffsetReader<'a, T: 'a> {
    reader: &'a mut T,
    offset: u64,
}

impl<'a, T> OffsetReader<'a, T> {
    fn new(reader: &'a mut T) -> Self {
        Self { reader, offset: 0 }
    }
}

impl<'a, T> Offset for OffsetReader<'a, T> {
    fn offset(&self) -> u64 {
        self.offset
    }
}

impl<'a, T: Read> Read for OffsetReader<'a, T> {
    fn read(&mut self, buf: &mut [u8]) -> std::io::Result<usize> {
        let bytes_read = self.reader.read(buf)?;
        self.offset = self
            .offset
            .checked_add(bytes_read.to_u64())
            .expect("total bytes read too large for offset type");
        Ok(bytes_read)
    }
}

// TODO: vec_push() needs to be replaced when Rust supports fallible memory
// allocation in raw_vec.
#[allow(unreachable_code)]
pub fn vec_push<T>(vec: &mut Vec<T>, val: T) -> std::result::Result<(), ()> {
    #[cfg(feature = "mp4parse_fallible")]
    {
        return FallibleVec::try_push(vec, val);
    }

    vec.push(val);
    Ok(())
}

fn vec_with_capacity<T>(capacity: usize) -> std::result::Result<Vec<T>, ()> {
    #[cfg(feature = "mp4parse_fallible")]
    {
        let mut v = Vec::new();
        FallibleVec::try_reserve(&mut v, capacity)?;
        Ok(v)
    }
    #[cfg(not(feature = "mp4parse_fallible"))]
    {
        Ok(Vec::with_capacity(capacity))
    }
}

pub fn extend_from_slice<T: Clone>(vec: &mut Vec<T>, other: &[T]) -> std::result::Result<(), ()> {
    #[cfg(feature = "mp4parse_fallible")]
    {
        FallibleVec::try_extend_from_slice(vec, other)
    }
    #[cfg(not(feature = "mp4parse_fallible"))]
    {
        vec.extend_from_slice(other);
        Ok(())
    }
}

/// With the `mp4parse_fallible` feature enabled, this function reserves the
/// upper limit of what `src` can generate before reading all bytes until EOF
/// in this source, placing them into buf. If the allocation is unsuccessful,
/// or reading from the source generates an error before reaching EOF, this
/// will return an error. Otherwise, it will return the number of bytes read.
///
/// Since `src.limit()` may return a value greater than the number of bytes
/// which can be read from the source, it's possible this function may fail
/// in the allocation phase even though allocating the number of bytes available
/// to read would have succeeded. In general, it is assumed that the callers
/// have accurate knowledge of the number of bytes of interest and have created
/// `src` accordingly.
///
/// With the `mp4parse_fallible` feature disabled, this is wrapper around
/// `std::io::Read::read_to_end()`.
fn read_to_end<T: Read>(src: &mut Take<T>, buf: &mut Vec<u8>) -> std::result::Result<usize, ()> {
    #[cfg(feature = "mp4parse_fallible")]
    {
        let limit: usize = src.limit().try_into().map_err(|_| ())?;
        FallibleVec::try_reserve(buf, limit)?;
        let bytes_read = src.read_to_end(buf).map_err(|_| ())?;
        Ok(bytes_read)
    }
    #[cfg(not(feature = "mp4parse_fallible"))]
    {
        src.read_to_end(buf).map_err(|_| ())
    }
}

/// Describes parser failures.
///
/// This enum wraps the standard `io::Error` type, unified with
/// our own parser error states and those of crates we use.
#[derive(Debug)]
pub enum Error {
    /// Parse error caused by corrupt or malformed data.
    InvalidData(&'static str),
    /// Parse error caused by limited parser support rather than invalid data.
    Unsupported(&'static str),
    /// Reflect `std::io::ErrorKind::UnexpectedEof` for short data.
    UnexpectedEOF,
    /// Propagate underlying errors from `std::io`.
    Io(std::io::Error),
    /// read_mp4 terminated without detecting a moov box.
    NoMoov,
    /// Out of memory
    OutOfMemory,
}

impl From<bitreader::BitReaderError> for Error {
    fn from(_: bitreader::BitReaderError) -> Error {
        Error::InvalidData("invalid data")
    }
}

impl From<std::io::Error> for Error {
    fn from(err: std::io::Error) -> Error {
        match err.kind() {
            std::io::ErrorKind::UnexpectedEof => Error::UnexpectedEOF,
            _ => Error::Io(err),
        }
    }
}

impl From<std::string::FromUtf8Error> for Error {
    fn from(_: std::string::FromUtf8Error) -> Error {
        Error::InvalidData("invalid utf8")
    }
}

impl From<std::num::TryFromIntError> for Error {
    fn from(_: std::num::TryFromIntError) -> Error {
        Error::Unsupported("integer conversion failed")
    }
}

impl From<()> for Error {
    fn from(_: ()) -> Error {
        Error::OutOfMemory
    }
}

/// Result shorthand using our Error enum.
pub type Result<T> = std::result::Result<T, Error>;

/// Basic ISO box structure.
///
/// mp4 files are a sequence of possibly-nested 'box' structures.  Each box
/// begins with a header describing the length of the box's data and a
/// four-byte box type which identifies the type of the box. Together these
/// are enough to interpret the contents of that section of the file.
#[derive(Debug, Clone, Copy)]
struct BoxHeader {
    /// Box type.
    name: BoxType,
    /// Size of the box in bytes.
    size: u64,
    /// Offset to the start of the contained data (or header size).
    offset: u64,
    /// Uuid for extended type.
    uuid: Option<[u8; 16]>,
}

/// File type box 'ftyp'.
#[derive(Debug)]
struct FileTypeBox {
    major_brand: FourCC,
    minor_version: u32,
    compatible_brands: Vec<FourCC>,
}

/// Movie header box 'mvhd'.
#[derive(Debug)]
struct MovieHeaderBox {
    pub timescale: u32,
    duration: u64,
}

#[derive(Debug, Clone, Copy)]
pub struct Matrix {
    pub a: i32, // 16.16 fix point
    pub b: i32, // 16.16 fix point
    pub u: i32, // 2.30 fix point
    pub c: i32, // 16.16 fix point
    pub d: i32, // 16.16 fix point
    pub v: i32, // 2.30 fix point
    pub x: i32, // 16.16 fix point
    pub y: i32, // 16.16 fix point
    pub w: i32, // 2.30 fix point
}

/// Track header box 'tkhd'
#[derive(Debug, Clone)]
pub struct TrackHeaderBox {
    track_id: u32,
    pub disabled: bool,
    pub duration: u64,
    pub width: u32,
    pub height: u32,
    pub matrix: Matrix,
}

/// Edit list box 'elst'
#[derive(Debug)]
struct EditListBox {
    edits: Vec<Edit>,
}

#[derive(Debug)]
struct Edit {
    segment_duration: u64,
    media_time: i64,
    media_rate_integer: i16,
    media_rate_fraction: i16,
}

/// Media header box 'mdhd'
#[derive(Debug)]
struct MediaHeaderBox {
    timescale: u32,
    duration: u64,
}

// Chunk offset box 'stco' or 'co64'
#[derive(Debug)]
pub struct ChunkOffsetBox {
    pub offsets: Vec<u64>,
}

// Sync sample box 'stss'
#[derive(Debug)]
pub struct SyncSampleBox {
    pub samples: Vec<u32>,
}

// Sample to chunk box 'stsc'
#[derive(Debug)]
pub struct SampleToChunkBox {
    pub samples: Vec<SampleToChunk>,
}

#[derive(Debug)]
pub struct SampleToChunk {
    pub first_chunk: u32,
    pub samples_per_chunk: u32,
    pub sample_description_index: u32,
}

// Sample size box 'stsz'
#[derive(Debug)]
pub struct SampleSizeBox {
    pub sample_size: u32,
    pub sample_sizes: Vec<u32>,
}

// Time to sample box 'stts'
#[derive(Debug)]
pub struct TimeToSampleBox {
    pub samples: Vec<Sample>,
}

#[repr(C)]
#[derive(Debug)]
pub struct Sample {
    pub sample_count: u32,
    pub sample_delta: u32,
}

#[derive(Debug, Clone, Copy)]
pub enum TimeOffsetVersion {
    Version0(u32),
    Version1(i32),
}

#[derive(Debug, Clone)]
pub struct TimeOffset {
    pub sample_count: u32,
    pub time_offset: TimeOffsetVersion,
}

#[derive(Debug)]
pub struct CompositionOffsetBox {
    pub samples: Vec<TimeOffset>,
}

// Handler reference box 'hdlr'
#[derive(Debug)]
struct HandlerBox {
    handler_type: FourCC,
}

// Sample description box 'stsd'
#[derive(Debug)]
pub struct SampleDescriptionBox {
    pub descriptions: Vec<SampleEntry>,
}

#[derive(Debug, Clone)]
pub enum SampleEntry {
    Audio(AudioSampleEntry),
    Video(VideoSampleEntry),
    Unknown,
}

#[allow(non_camel_case_types)]
#[derive(Debug, Clone, Default)]
pub struct ES_Descriptor {
    pub audio_codec: CodecType,
    pub audio_object_type: Option<u16>,
    pub extended_audio_object_type: Option<u16>,
    pub audio_sample_rate: Option<u32>,
    pub audio_channel_count: Option<u16>,
    pub codec_esds: Vec<u8>,
    pub decoder_specific_data: Vec<u8>, // Data in DECODER_SPECIFIC_TAG
}

#[allow(non_camel_case_types)]
#[derive(Debug, Clone)]
pub enum AudioCodecSpecific {
    ES_Descriptor(ES_Descriptor),
    FLACSpecificBox(FLACSpecificBox),
    OpusSpecificBox(OpusSpecificBox),
    ALACSpecificBox(ALACSpecificBox),
    MP3,
    LPCM,
}

#[derive(Debug, Clone)]
pub struct AudioSampleEntry {
    pub codec_type: CodecType,
    data_reference_index: u16,
    pub channelcount: u32,
    pub samplesize: u16,
    pub samplerate: f64,
    pub codec_specific: AudioCodecSpecific,
    pub protection_info: Vec<ProtectionSchemeInfoBox>,
}

#[derive(Debug, Clone)]
pub enum VideoCodecSpecific {
    AVCConfig(Vec<u8>),
    VPxConfig(VPxConfigBox),
    AV1Config(AV1ConfigBox),
    ESDSConfig(Vec<u8>),
}

#[derive(Debug, Clone)]
pub struct VideoSampleEntry {
    pub codec_type: CodecType,
    data_reference_index: u16,
    pub width: u16,
    pub height: u16,
    pub codec_specific: VideoCodecSpecific,
    pub protection_info: Vec<ProtectionSchemeInfoBox>,
}

/// Represent a Video Partition Codec Configuration 'vpcC' box (aka vp9). The meaning of each
/// field is covered in detail in "VP Codec ISO Media File Format Binding".
#[derive(Debug, Clone)]
pub struct VPxConfigBox {
    /// An integer that specifies the VP codec profile.
    profile: u8,
    /// An integer that specifies a VP codec level all samples conform to the following table.
    /// For a description of the various levels, please refer to the VP9 Bitstream Specification.
    level: u8,
    /// An integer that specifies the bit depth of the luma and color components. Valid values
    /// are 8, 10, and 12.
    pub bit_depth: u8,
    /// Really an enum defined by the "Colour primaries" section of ISO/IEC 23001-8:2016.
    pub colour_primaries: u8,
    /// Really an enum defined by "VP Codec ISO Media File Format Binding".
    pub chroma_subsampling: u8,
    /// Really an enum defined by the "Transfer characteristics" section of ISO/IEC 23001-8:2016.
    transfer_characteristics: u8,
    /// Really an enum defined by the "Matrix coefficients" section of ISO/IEC 23001-8:2016.
    /// Available in 'VP Codec ISO Media File Format' version 1 only.
    matrix_coefficients: Option<u8>,
    /// Indicates the black level and range of the luma and chroma signals. 0 = legal range
    /// (e.g. 16-235 for 8 bit sample depth); 1 = full range (e.g. 0-255 for 8-bit sample depth).
    video_full_range_flag: bool,
    /// This is not used for VP8 and VP9 . Intended for binary codec initialization data.
    pub codec_init: Vec<u8>,
}

#[derive(Debug, Clone)]
pub struct AV1ConfigBox {
    pub profile: u8,
    pub level: u8,
    pub tier: u8,
    pub bit_depth: u8,
    pub monochrome: bool,
    pub chroma_subsampling_x: u8,
    pub chroma_subsampling_y: u8,
    pub chroma_sample_position: u8,
    pub initial_presentation_delay_present: bool,
    pub initial_presentation_delay_minus_one: u8,
    pub config_obus: Vec<u8>,
}

#[derive(Debug, Clone)]
pub struct FLACMetadataBlock {
    pub block_type: u8,
    pub data: Vec<u8>,
}

/// Represents a FLACSpecificBox 'dfLa'
#[derive(Debug, Clone)]
pub struct FLACSpecificBox {
    version: u8,
    pub blocks: Vec<FLACMetadataBlock>,
}

#[derive(Debug, Clone)]
struct ChannelMappingTable {
    stream_count: u8,
    coupled_count: u8,
    channel_mapping: Vec<u8>,
}

/// Represent an OpusSpecificBox 'dOps'
#[derive(Debug, Clone)]
pub struct OpusSpecificBox {
    pub version: u8,
    output_channel_count: u8,
    pre_skip: u16,
    input_sample_rate: u32,
    output_gain: i16,
    channel_mapping_family: u8,
    channel_mapping_table: Option<ChannelMappingTable>,
}

/// Represent an ALACSpecificBox 'alac'
#[derive(Debug, Clone)]
pub struct ALACSpecificBox {
    version: u8,
    pub data: Vec<u8>,
}

#[derive(Debug)]
pub struct MovieExtendsBox {
    pub fragment_duration: Option<MediaScaledTime>,
}

pub type ByteData = Vec<u8>;

#[derive(Debug, Default)]
pub struct ProtectionSystemSpecificHeaderBox {
    pub system_id: ByteData,
    pub kid: Vec<ByteData>,
    pub data: ByteData,

    // The entire pssh box (include header) required by Gecko.
    pub box_content: ByteData,
}

#[derive(Debug, Default, Clone)]
pub struct SchemeTypeBox {
    pub scheme_type: FourCC,
    pub scheme_version: u32,
}

#[derive(Debug, Default, Clone)]
pub struct TrackEncryptionBox {
    pub is_encrypted: u8,
    pub iv_size: u8,
    pub kid: Vec<u8>,
    // Members for pattern encryption schemes
    pub crypt_byte_block_count: Option<u8>,
    pub skip_byte_block_count: Option<u8>,
    pub constant_iv: Option<Vec<u8>>,
    // End pattern encryption scheme members
}

#[derive(Debug, Default, Clone)]
pub struct ProtectionSchemeInfoBox {
    pub code_name: String,
    pub scheme_type: Option<SchemeTypeBox>,
    pub tenc: Option<TrackEncryptionBox>,
}

/// Represents a userdata box 'udta'.
/// Currently, only the metadata atom 'meta'
/// is parsed.
#[derive(Debug, Default, Clone)]
pub struct UserdataBox {
    pub meta: Option<MetadataBox>,
}

/// Represents possible contents of the
/// ©gen or gnre atoms within a metadata box.
/// 'udta.meta.ilst' may only have either a
/// standard genre box 'gnre' or a custom
/// genre box '©gen', but never both at once.
#[derive(Debug, Clone, Eq, PartialEq)]
pub enum Genre {
    /// A standard ID3v1 numbered genre.
    StandardGenre(u8),
    /// Any custom genre string.
    CustomGenre(String),
}

/// Represents the contents of a 'stik'
/// atom that indicates content types within
/// iTunes.
#[derive(Debug, Clone, Eq, PartialEq)]
pub enum MediaType {
    /// Movie is stored as 0 in a 'stik' atom.
    Movie, // 0
    /// Normal is stored as 1 in a 'stik' atom.
    Normal, // 1
    /// AudioBook is stored as 2 in a 'stik' atom.
    AudioBook, // 2
    /// WhackedBookmark is stored as 5 in a 'stik' atom.
    WhackedBookmark, // 5
    /// MusicVideo is stored as 6 in a 'stik' atom.
    MusicVideo, // 6
    /// ShortFilm is stored as 9 in a 'stik' atom.
    ShortFilm, // 9
    /// TVShow is stored as 10 in a 'stik' atom.
    TVShow, // 10
    /// Booklet is stored as 11 in a 'stik' atom.
    Booklet, // 11
    /// An unknown 'stik' value.
    Unknown(u8),
}

/// Represents the parental advisory rating on the track,
/// stored within the 'rtng' atom.
#[derive(Debug, Clone, Eq, PartialEq)]
pub enum AdvisoryRating {
    /// Clean is always stored as 2 in an 'rtng' atom.
    Clean, // 2
    /// A value of 0 in an 'rtng' atom indicates 'Inoffensive'
    Inoffensive, // 0
    /// Any non 2 or 0 value in 'rtng' indicates the track is explicit.
    Explicit(u8),
}

/// Represents the contents of 'ilst' atoms within
/// a metadata box 'meta', parsed as iTunes metadata using
/// the conventional tags.
#[derive(Debug, Default, Clone)]
pub struct MetadataBox {
    /// The album name, '©alb'
    pub album: Option<String>,
    /// The artist name '©art' or '©ART'
    pub artist: Option<String>,
    /// The album artist 'aART'
    pub album_artist: Option<String>,
    /// Track comments '©cmt'
    pub comment: Option<String>,
    /// The date or year field '©day'
    ///
    /// This is stored as an arbitrary string,
    /// and may not necessarily be in a valid date
    /// format.
    pub year: Option<String>,
    /// The track title '©nam'
    pub title: Option<String>,
    /// The track genre '©gen' or 'gnre'.
    pub genre: Option<Genre>,
    /// The track number 'trkn'.
    pub track_number: Option<u8>,
    /// The disc number 'disk'
    pub disc_number: Option<u8>,
    /// The total number of tracks on the disc,
    /// stored in 'trkn'
    pub total_tracks: Option<u8>,
    /// The total number of discs in the album,
    /// stored in 'disk'
    pub total_discs: Option<u8>,
    /// The composer of the track '©wrt'
    pub composer: Option<String>,
    /// The encoder used to create this track '©too'
    pub encoder: Option<String>,
    /// The encoded-by settingo this track '©enc'
    pub encoded_by: Option<String>,
    /// The tempo or BPM of the track 'tmpo'
    pub beats_per_minute: Option<u8>,
    /// Copyright information of the track 'cprt'
    pub copyright: Option<String>,
    /// Whether or not this track is part of a compilation 'cpil'
    pub compilation: Option<bool>,
    /// The advisory rating of this track 'rtng'
    pub advisory: Option<AdvisoryRating>,
    /// The personal rating of this track, 'rate'.
    ///
    /// This is stored in the box as string data, but
    /// the format is an integer percentage from 0 - 100,
    /// where 100 is displayed as 5 stars out of 5.
    pub rating: Option<String>,
    /// The grouping this track belongs to '©grp'
    pub grouping: Option<String>,
    /// The media type of this track 'stik'
    pub media_type: Option<MediaType>, // stik
    /// Whether or not this track is a podcast 'pcst'
    pub podcast: Option<bool>,
    /// The category of ths track 'catg'
    pub category: Option<String>,
    /// The podcast keyword 'keyw'
    pub keyword: Option<String>,
    /// The podcast url 'purl'
    pub podcast_url: Option<String>,
    /// The podcast episode GUID 'egid'
    pub podcast_guid: Option<String>,
    /// The description of the track 'desc'
    pub description: Option<String>,
    /// The long description of the track 'ldes'.
    ///
    /// Unlike other string fields, the long description field
    /// can be longer than 256 characters.
    pub long_description: Option<String>,
    /// The lyrics of the track '©lyr'.
    ///
    /// Unlike other string fields, the lyrics field
    /// can be longer than 256 characters.
    pub lyrics: Option<String>,
    /// The name of the TV network this track aired on 'tvnn'.
    pub tv_network_name: Option<String>,
    /// The name of the TV Show for this track 'tvsh'.
    pub tv_show_name: Option<String>,
    /// The name of the TV Episode for this track 'tven'.
    pub tv_episode_name: Option<String>,
    /// The number of the TV Episode for this track 'tves'.
    pub tv_episode_number: Option<u8>,
    /// The season of the TV Episode of this track 'tvsn'.
    pub tv_season: Option<u8>,
    /// The date this track was purchased 'purd'.
    pub purchase_date: Option<String>,
    /// Whether or not this track supports gapless playback 'pgap'
    pub gapless_playback: Option<bool>,
    /// Any cover artwork attached to this track 'covr'
    ///
    /// 'covr' is unique in that it may contain multiple 'data' sub-entries,
    /// each an image file. Here, each subentry's raw binary data is exposed,
    /// which may contain image data in JPEG or PNG format.
    pub cover_art: Option<Vec<Vec<u8>>>,
    /// The owner of the track 'ownr'
    pub owner: Option<String>,
    /// Whether or not this track is HD Video 'hdvd'
    pub hd_video: Option<bool>,
    /// The name of the track to sort by 'sonm'
    pub sort_name: Option<String>,
    /// The name of the album to sort by 'soal'
    pub sort_album: Option<String>,
    /// The name of the artist to sort by 'soar'
    pub sort_artist: Option<String>,
    /// The name of the album artist to sort by 'soaa'
    pub sort_album_artist: Option<String>,
    /// The name of the composer to sort by 'soco'
    pub sort_composer: Option<String>,
}

/// Internal data structures.
#[derive(Debug, Default)]
pub struct MediaContext {
    pub timescale: Option<MediaTimeScale>,
    /// Tracks found in the file.
    pub tracks: Vec<Track>,
    pub mvex: Option<MovieExtendsBox>,
    pub psshs: Vec<ProtectionSystemSpecificHeaderBox>,
    pub userdata: Option<Result<UserdataBox>>,
}

impl MediaContext {
    pub fn new() -> MediaContext {
        Default::default()
    }
}

#[derive(Debug, Default)]
pub struct AvifContext {
    /// The collected data indicated by the `pitm` box, See ISO 14496-12:2015 § 8.11.4
    pub primary_item: Vec<u8>,
}

impl AvifContext {
    pub fn new() -> Self {
        Default::default()
    }
}

/// A Media Data Box
/// See ISO 14496-12:2015 § 8.1.1
struct MediaDataBox {
    /// Offset of `data` from the beginning of the file. See ConstructionMethod::File
    offset: u64,
    data: Vec<u8>,
}

impl MediaDataBox {
    /// Check whether the beginning of `extent` is within the bounds of the `MediaDataBox`.
    /// We assume extents to not cross box boundaries. If so, this will cause an error
    /// in `read_extent`.
    fn contains_extent(&self, extent: &ExtentRange) -> bool {
        if self.offset <= extent.start() {
            let start_offset = extent.start() - self.offset;
            start_offset < self.data.len().to_u64()
        } else {
            false
        }
    }

    /// Check whether `extent` covers the `MediaDataBox` exactly.
    fn matches_extent(&self, extent: &ExtentRange) -> bool {
        if self.offset == extent.start() {
            match extent {
                ExtentRange::WithLength(range) => {
                    if let Some(end) = self.offset.checked_add(self.data.len().to_u64()) {
                        end == range.end
                    } else {
                        false
                    }
                }
                ExtentRange::ToEnd(_) => true,
            }
        } else {
            false
        }
    }

    /// Copy the range specified by `extent` to the end of `buf` or return an error if the range
    /// is not fully contained within `MediaDataBox`.
    fn read_extent(&mut self, extent: &ExtentRange, buf: &mut Vec<u8>) -> Result<()> {
        let start_offset = extent
            .start()
            .checked_sub(self.offset)
            .expect("mdat does not contain extent");
        let slice = match extent {
            ExtentRange::WithLength(range) => {
                let range_len = range
                    .end
                    .checked_sub(range.start)
                    .expect("range start > end");
                let end = start_offset
                    .checked_add(range_len)
                    .expect("extent end overflow");
                self.data.get(start_offset.try_into()?..end.try_into()?)
            }
            ExtentRange::ToEnd(_) => self.data.get(start_offset.try_into()?..),
        };
        let slice = slice.ok_or(Error::InvalidData("extent crosses box boundary"))?;
        extend_from_slice(buf, slice)?;
        Ok(())
    }
}

/// Used for 'infe' boxes within 'iinf' boxes
/// See ISO 14496-12:2015 § 8.11.6
/// Only versions {2, 3} are supported
#[derive(Debug)]
struct ItemInfoEntry {
    item_id: u32,
    item_type: u32,
}

/// Potential sizes (in bytes) of variable-sized fields of the 'iloc' box
/// See ISO 14496-12:2015 § 8.11.3
enum IlocFieldSize {
    Zero,
    Four,
    Eight,
}

impl IlocFieldSize {
    fn to_bits(&self) -> u8 {
        match self {
            IlocFieldSize::Zero => 0,
            IlocFieldSize::Four => 32,
            IlocFieldSize::Eight => 64,
        }
    }
}

impl TryFrom<u8> for IlocFieldSize {
    type Error = Error;

    fn try_from(value: u8) -> Result<Self> {
        match value {
            0 => Ok(Self::Zero),
            4 => Ok(Self::Four),
            8 => Ok(Self::Eight),
            _ => Err(Error::InvalidData("value must be in the set {0, 4, 8}")),
        }
    }
}

#[derive(PartialEq)]
enum IlocVersion {
    Zero,
    One,
    Two,
}

impl TryFrom<u8> for IlocVersion {
    type Error = Error;

    fn try_from(value: u8) -> Result<Self> {
        match value {
            0 => Ok(Self::Zero),
            1 => Ok(Self::One),
            2 => Ok(Self::Two),
            _ => Err(Error::Unsupported("unsupported version in 'iloc' box")),
        }
    }
}

/// Used for 'iloc' boxes
/// See ISO 14496-12:2015 § 8.11.3
/// `base_offset` is omitted since it is integrated into the ranges in `extents`
/// `data_reference_index` is omitted, since only 0 (i.e., this file) is supported
#[derive(Clone, Debug)]
struct ItemLocationBoxItem {
    item_id: u32,
    construction_method: ConstructionMethod,
    /// Unused for ConstructionMethod::Idat
    extents: Vec<ItemLocationBoxExtent>,
}

#[derive(Clone, Copy, Debug)]
enum ConstructionMethod {
    File,
    Idat,
    #[allow(dead_code)] // TODO: see https://github.com/mozilla/mp4parse-rust/issues/196
    Item,
}

/// `extent_index` is omitted since it's only used for ConstructionMethod::Item which
/// is currently not implemented.
#[derive(Clone, Debug)]
struct ItemLocationBoxExtent {
    extent_range: ExtentRange,
}

#[derive(Clone, Debug)]
enum ExtentRange {
    WithLength(Range<u64>),
    ToEnd(RangeFrom<u64>),
}

impl ExtentRange {
    fn start(&self) -> u64 {
        match self {
            Self::WithLength(r) => r.start,
            Self::ToEnd(r) => r.start,
        }
    }
}

#[derive(Debug, PartialEq)]
pub enum TrackType {
    Audio,
    Video,
    Metadata,
    Unknown,
}

impl Default for TrackType {
    fn default() -> Self {
        TrackType::Unknown
    }
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum CodecType {
    Unknown,
    MP3,
    AAC,
    FLAC,
    Opus,
    H264, // 14496-10
    MP4V, // 14496-2
    AV1,
    VP9,
    VP8,
    EncryptedVideo,
    EncryptedAudio,
    LPCM, // QT
    ALAC,
}

impl Default for CodecType {
    fn default() -> Self {
        CodecType::Unknown
    }
}

/// The media's global (mvhd) timescale in units per second.
#[derive(Debug, Copy, Clone, PartialEq)]
pub struct MediaTimeScale(pub u64);

/// A time to be scaled by the media's global (mvhd) timescale.
#[derive(Debug, Copy, Clone, PartialEq)]
pub struct MediaScaledTime(pub u64);

/// The track's local (mdhd) timescale.
/// Members are timescale units per second and the track id.
#[derive(Debug, Copy, Clone, PartialEq)]
pub struct TrackTimeScale<T: Num>(pub T, pub usize);

/// A time to be scaled by the track's local (mdhd) timescale.
/// Members are time in scale units and the track id.
#[derive(Debug, Copy, Clone, PartialEq)]
pub struct TrackScaledTime<T: Num>(pub T, pub usize);

impl<T> std::ops::Add for TrackScaledTime<T>
where
    T: Num,
{
    type Output = TrackScaledTime<T>;

    fn add(self, other: TrackScaledTime<T>) -> TrackScaledTime<T> {
        TrackScaledTime::<T>(self.0 + other.0, self.1)
    }
}

#[derive(Debug, Default)]
pub struct Track {
    pub id: usize,
    pub track_type: TrackType,
    pub empty_duration: Option<MediaScaledTime>,
    pub media_time: Option<TrackScaledTime<u64>>,
    pub timescale: Option<TrackTimeScale<u64>>,
    pub duration: Option<TrackScaledTime<u64>>,
    pub track_id: Option<u32>,
    pub tkhd: Option<TrackHeaderBox>, // TODO(kinetik): find a nicer way to export this.
    pub stsd: Option<SampleDescriptionBox>,
    pub stts: Option<TimeToSampleBox>,
    pub stsc: Option<SampleToChunkBox>,
    pub stsz: Option<SampleSizeBox>,
    pub stco: Option<ChunkOffsetBox>, // It is for stco or co64.
    pub stss: Option<SyncSampleBox>,
    pub ctts: Option<CompositionOffsetBox>,
}

impl Track {
    fn new(id: usize) -> Track {
        Track {
            id,
            ..Default::default()
        }
    }
}

struct BMFFBox<'a, T: 'a> {
    head: BoxHeader,
    content: Take<&'a mut T>,
}

struct BoxIter<'a, T: 'a> {
    src: &'a mut T,
}

impl<'a, T: Read> BoxIter<'a, T> {
    fn new(src: &mut T) -> BoxIter<T> {
        BoxIter { src }
    }

    fn next_box(&mut self) -> Result<Option<BMFFBox<T>>> {
        let r = read_box_header(self.src);
        match r {
            Ok(h) => Ok(Some(BMFFBox {
                head: h,
                content: self.src.take(h.size - h.offset),
            })),
            Err(Error::UnexpectedEOF) => Ok(None),
            Err(e) => Err(e),
        }
    }
}

impl<'a, T: Read> Read for BMFFBox<'a, T> {
    fn read(&mut self, buf: &mut [u8]) -> std::io::Result<usize> {
        self.content.read(buf)
    }
}

impl<'a, T: Offset> Offset for BMFFBox<'a, T> {
    fn offset(&self) -> u64 {
        self.content.get_ref().offset()
    }
}

impl<'a, T: Read> BMFFBox<'a, T> {
    fn bytes_left(&self) -> u64 {
        self.content.limit()
    }

    fn get_header(&self) -> &BoxHeader {
        &self.head
    }

    fn box_iter<'b>(&'b mut self) -> BoxIter<BMFFBox<'a, T>> {
        BoxIter::new(self)
    }
}

impl<'a, T: Read + Offset> BMFFBox<'a, T> {
    /// Check whether the beginning of `extent` is within the bounds of the `BMFFBox`.
    /// We assume extents to not cross box boundaries. If so, this will cause an error
    /// in `read_extent`.
    fn contains_extent(&self, extent: &ExtentRange) -> bool {
        if self.offset() <= extent.start() {
            let start_offset = extent.start() - self.offset();
            start_offset < self.bytes_left()
        } else {
            false
        }
    }

    /// Read the range specified by `extent` into `buf` or return an error if the range is not
    /// fully contained within the `BMFFBox`.
    fn read_extent(&mut self, extent: &ExtentRange, buf: &mut Vec<u8>) -> Result<()> {
        let start_offset = extent
            .start()
            .checked_sub(self.offset())
            .expect("box does not contain extent");
        skip(self, start_offset)?;
        match extent {
            ExtentRange::WithLength(range) => {
                let len = range
                    .end
                    .checked_sub(range.start)
                    .expect("range start > end");
                if len > self.bytes_left() {
                    return Err(Error::InvalidData("extent crosses box boundary"));
                }
                read_to_end(&mut self.take(len), buf)?;
            }
            ExtentRange::ToEnd(_) => {
                read_to_end(&mut self.take(self.bytes_left()), buf)?;
            }
        }
        Ok(())
    }
}

impl<'a, T> Drop for BMFFBox<'a, T> {
    fn drop(&mut self) {
        if self.content.limit() > 0 {
            let name: FourCC = From::from(self.head.name);
            debug!("Dropping {} bytes in '{}'", self.content.limit(), name);
        }
    }
}

/// Read and parse a box header.
///
/// Call this first to determine the type of a particular mp4 box
/// and its length. Used internally for dispatching to specific
/// parsers for the internal content, or to get the length to
/// skip unknown or uninteresting boxes.
fn read_box_header<T: ReadBytesExt>(src: &mut T) -> Result<BoxHeader> {
    let size32 = be_u32(src)?;
    let name = BoxType::from(be_u32(src)?);
    let size = match size32 {
        // valid only for top-level box and indicates it's the last box in the file.  usually mdat.
        0 => return Err(Error::Unsupported("unknown sized box")),
        1 => {
            let size64 = be_u64(src)?;
            if size64 < 16 {
                return Err(Error::InvalidData("malformed wide size"));
            }
            size64
        }
        2..=7 => return Err(Error::InvalidData("malformed size")),
        _ => u64::from(size32),
    };
    let mut offset = match size32 {
        1 => 4 + 4 + 8,
        _ => 4 + 4,
    };
    let uuid = if name == BoxType::UuidBox {
        if size >= offset + 16 {
            let mut buffer = [0u8; 16];
            let count = src.read(&mut buffer)?;
            offset += count.to_u64();
            if count == 16 {
                Some(buffer)
            } else {
                debug!("malformed uuid (short read), skipping");
                None
            }
        } else {
            debug!("malformed uuid, skipping");
            None
        }
    } else {
        None
    };
    assert!(offset <= size);
    Ok(BoxHeader {
        name,
        size,
        offset,
        uuid,
    })
}

/// Parse the extra header fields for a full box.
fn read_fullbox_extra<T: ReadBytesExt>(src: &mut T) -> Result<(u8, u32)> {
    let version = src.read_u8()?;
    let flags_a = src.read_u8()?;
    let flags_b = src.read_u8()?;
    let flags_c = src.read_u8()?;
    Ok((
        version,
        u32::from(flags_a) << 16 | u32::from(flags_b) << 8 | u32::from(flags_c),
    ))
}

// Parse the extra fields for a full box whose flag fields must be zero.
fn read_fullbox_version_no_flags<T: ReadBytesExt>(src: &mut T) -> Result<u8> {
    let (version, flags) = read_fullbox_extra(src)?;

    if flags != 0 {
        return Err(Error::Unsupported("expected flags to be 0"));
    }

    Ok(version)
}

/// Skip over the entire contents of a box.
fn skip_box_content<T: Read>(src: &mut BMFFBox<T>) -> Result<()> {
    // Skip the contents of unknown chunks.
    let to_skip = {
        let header = src.get_header();
        debug!("{:?} (skipped)", header);
        header
            .size
            .checked_sub(header.offset)
            .expect("header offset > size")
    };
    assert_eq!(to_skip, src.bytes_left());
    skip(src, to_skip)
}

/// Skip over the remain data of a box.
fn skip_box_remain<T: Read>(src: &mut BMFFBox<T>) -> Result<()> {
    let remain = {
        let header = src.get_header();
        let len = src.bytes_left();
        debug!("remain {} (skipped) in {:?}", len, header);
        len
    };
    skip(src, remain)
}

/// Read the contents of an AVIF file
///
/// Metadata is accumulated in the passed-through `AvifContext` struct,
/// which can be examined later.
pub fn read_avif<T: Read>(f: &mut T, context: &mut AvifContext) -> Result<()> {
    let mut f = OffsetReader::new(f);

    let mut iter = BoxIter::new(&mut f);

    // 'ftyp' box must occur first; see ISO 14496-12:2015 § 4.3.1
    if let Some(mut b) = iter.next_box()? {
        if b.head.name == BoxType::FileTypeBox {
            let ftyp = read_ftyp(&mut b)?;
            if !ftyp.compatible_brands.contains(&FourCC::from("mif1")) {
                return Err(Error::InvalidData("compatible_brands must contain 'mif1'"));
            }
        } else {
            return Err(Error::InvalidData("'ftyp' box must occur first"));
        }
    }

    let mut read_meta = false;
    let mut mdats = vec![];
    let mut primary_item_extents = None;
    let mut primary_item_extents_data: Vec<Vec<u8>> = vec![];

    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::MetadataBox => {
                if read_meta {
                    return Err(Error::InvalidData(
                        "There should be zero or one meta boxes per ISO 14496-12:2015 § 8.11.1.1",
                    ));
                }
                read_meta = true;
                let primary_item_loc = read_avif_meta(&mut b)?;
                match primary_item_loc.construction_method {
                    ConstructionMethod::File => {
                        primary_item_extents = Some(primary_item_loc.extents);
                        primary_item_extents_data =
                            primary_item_extents.iter().map(|_| vec![]).collect();
                    }
                    _ => return Err(Error::Unsupported("unsupported construction_method")),
                }
            }
            BoxType::MediaDataBox => {
                // See ISO 14496-12:2015 § 8.1.1
                // If we know our primary item location by this point, try to read it out of this
                // mdat directly and avoid a copy
                if let Some(extents) = &primary_item_extents {
                    for (extent, data) in extents.iter().zip(primary_item_extents_data.iter_mut()) {
                        if b.contains_extent(&extent.extent_range) {
                            b.read_extent(&extent.extent_range, data)?;
                        }
                    }
                }

                // Store any remaining data for potential later extraction
                if b.bytes_left() > 0 {
                    let offset = b.offset();
                    let mut data = vec_with_capacity(b.bytes_left().try_into()?)?;
                    b.read_to_end(&mut data)?;
                    vec_push(&mut mdats, MediaDataBox { offset, data })?;
                }
            }
            _ => skip_box_content(&mut b)?,
        }

        check_parser_state!(b.content);
    }

    // If the `mdat` box came before the `meta` box, we need to fill in our primary item data
    let primary_item_extents =
        primary_item_extents.ok_or(Error::InvalidData("primary item extents missing"))?;
    for (extent, data) in primary_item_extents
        .iter()
        .zip(primary_item_extents_data.iter_mut())
    {
        if data.is_empty() {
            // try to find an overlapping mdat
            for mdat in &mut mdats {
                if mdat.matches_extent(&extent.extent_range) {
                    data.append(&mut mdat.data)
                } else if mdat.contains_extent(&extent.extent_range) {
                    mdat.read_extent(&extent.extent_range, data)?;
                }
            }
        }
    }

    context.primary_item = primary_item_extents_data.concat();

    Ok(())
}

/// Parse a metadata box in the context of an AVIF
/// Currently requires the primary item to be an av01 item type and generates
/// an error otherwise.
/// See ISO 14496-12:2015 § 8.11.1
fn read_avif_meta<T: Read + Offset>(src: &mut BMFFBox<T>) -> Result<ItemLocationBoxItem> {
    let version = read_fullbox_version_no_flags(src)?;

    if version != 0 {
        return Err(Error::Unsupported("unsupported meta version"));
    }

    let mut primary_item_id = None;
    let mut item_infos = None;
    let mut iloc_items = None;

    let mut iter = src.box_iter();
    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::ItemInfoBox => {
                if item_infos.is_some() {
                    return Err(Error::InvalidData(
                        "There should be zero or one iinf boxes per ISO 14496-12:2015 § 8.11.6.1",
                    ));
                }
                item_infos = Some(read_iinf(&mut b)?);
            }
            BoxType::ItemLocationBox => {
                if iloc_items.is_some() {
                    return Err(Error::InvalidData(
                        "There should be zero or one iloc boxes per ISO 14496-12:2015 § 8.11.3.1",
                    ));
                }
                iloc_items = Some(read_iloc(&mut b)?);
            }
            BoxType::PrimaryItemBox => {
                if primary_item_id.is_some() {
                    return Err(Error::InvalidData(
                        "There should be zero or one iloc boxes per ISO 14496-12:2015 § 8.11.4.1",
                    ));
                }
                primary_item_id = Some(read_pitm(&mut b)?);
            }
            _ => skip_box_content(&mut b)?,
        }

        check_parser_state!(b.content);
    }

    let primary_item_id = primary_item_id.ok_or(Error::InvalidData(
        "Required pitm box not present in meta box",
    ))?;

    if let Some(item_info) = item_infos
        .iter()
        .flatten()
        .find(|x| x.item_id == primary_item_id)
    {
        if &item_info.item_type.to_be_bytes() != b"av01" {
            warn!(
                "primary_item_id type: {}",
                be_u32_to_string(item_info.item_type)
            );
            return Err(Error::InvalidData("primary_item_id type is not av01"));
        }
    } else {
        return Err(Error::InvalidData(
            "primary_item_id not present in iinf box",
        ));
    }

    if let Some(loc) = iloc_items
        .iter()
        .flatten()
        .find(|loc| loc.item_id == primary_item_id)
    {
        Ok(loc.clone())
    } else {
        Err(Error::InvalidData(
            "primary_item_id not present in iloc box",
        ))
    }
}

/// Parse a Primary Item Box
/// See ISO 14496-12:2015 § 8.11.4
fn read_pitm<T: Read>(src: &mut BMFFBox<T>) -> Result<u32> {
    let version = read_fullbox_version_no_flags(src)?;

    let item_id = match version {
        0 => be_u16(src)?.into(),
        1 => be_u32(src)?,
        _ => return Err(Error::Unsupported("unsupported pitm version")),
    };

    Ok(item_id)
}

/// Parse an Item Information Box
/// See ISO 14496-12:2015 § 8.11.6
fn read_iinf<T: Read>(src: &mut BMFFBox<T>) -> Result<Vec<ItemInfoEntry>> {
    let version = read_fullbox_version_no_flags(src)?;

    match version {
        0 | 1 => (),
        _ => return Err(Error::Unsupported("unsupported iinf version")),
    }

    let entry_count = if version == 0 {
        be_u16(src)?.to_usize()
    } else {
        be_u32(src)?.to_usize()
    };
    let mut item_infos = vec_with_capacity(entry_count)?;

    let mut iter = src.box_iter();
    while let Some(mut b) = iter.next_box()? {
        if b.head.name != BoxType::ItemInfoEntry {
            return Err(Error::InvalidData(
                "iinf box should contain only infe boxes",
            ));
        }

        vec_push(&mut item_infos, read_infe(&mut b)?)?;

        check_parser_state!(b.content);
    }

    Ok(item_infos)
}

fn be_u32_to_string(src: u32) -> String {
    String::from_utf8(src.to_be_bytes().to_vec()).unwrap_or(format!("{:x?}", src))
}

/// Parse an Item Info Entry
/// See ISO 14496-12:2015 § 8.11.6.2
fn read_infe<T: Read>(src: &mut BMFFBox<T>) -> Result<ItemInfoEntry> {
    // According to the standard, it seems the flags field should be 0, but
    // at least one sample AVIF image has a nonzero value.
    let (version, _) = read_fullbox_extra(src)?;

    // mif1 brand (see ISO 23008-12:2017 § 10.2.1) only requires v2 and 3
    let item_id = match version {
        2 => be_u16(src)?.into(),
        3 => be_u32(src)?,
        _ => return Err(Error::Unsupported("unsupported version in 'infe' box")),
    };

    let item_protection_index = be_u16(src)?;

    if item_protection_index != 0 {
        return Err(Error::Unsupported(
            "protected items (infe.item_protection_index != 0) are not supported",
        ));
    }

    let item_type = be_u32(src)?;
    debug!(
        "infe item_id {} item_type: {}",
        item_id,
        be_u32_to_string(item_type)
    );

    // There are some additional fields here, but they're not of interest to us
    skip_box_remain(src)?;

    Ok(ItemInfoEntry { item_id, item_type })
}

/// Parse an item location box inside a meta box
/// See ISO 14496-12:2015 § 8.11.3
fn read_iloc<T: Read>(src: &mut BMFFBox<T>) -> Result<Vec<ItemLocationBoxItem>> {
    let version: IlocVersion = read_fullbox_version_no_flags(src)?.try_into()?;

    let mut iloc = vec_with_capacity(src.bytes_left().try_into()?)?;
    src.read_to_end(&mut iloc)?;
    let mut iloc = BitReader::new(iloc.as_slice());

    let offset_size: IlocFieldSize = iloc.read_u8(4)?.try_into()?;
    let length_size: IlocFieldSize = iloc.read_u8(4)?.try_into()?;
    let base_offset_size: IlocFieldSize = iloc.read_u8(4)?.try_into()?;

    let index_size: Option<IlocFieldSize> = match version {
        IlocVersion::One | IlocVersion::Two => Some(iloc.read_u8(4)?.try_into()?),
        IlocVersion::Zero => {
            let _reserved = iloc.read_u8(4)?;
            None
        }
    };

    let item_count = match version {
        IlocVersion::Zero | IlocVersion::One => iloc.read_u32(16)?,
        IlocVersion::Two => iloc.read_u32(32)?,
    };

    let mut items = vec_with_capacity(item_count.to_usize())?;

    for _ in 0..item_count {
        let item_id = match version {
            IlocVersion::Zero | IlocVersion::One => iloc.read_u32(16)?,
            IlocVersion::Two => iloc.read_u32(32)?,
        };

        // The spec isn't entirely clear how an `iloc` should be interpreted for version 0,
        // which has no `construction_method` field. It does say:
        // "For maximum compatibility, version 0 of this box should be used in preference to
        //  version 1 with `construction_method==0`, or version 2 when possible."
        // We take this to imply version 0 can be interpreted as using file offsets.
        let construction_method = match version {
            IlocVersion::Zero => ConstructionMethod::File,
            IlocVersion::One | IlocVersion::Two => {
                let _reserved = iloc.read_u16(12)?;
                match iloc.read_u16(4)? {
                    0 => ConstructionMethod::File,
                    1 => ConstructionMethod::Idat,
                    2 => return Err(Error::Unsupported("construction_method 'item_offset' is not supported")),
                    _ => return Err(Error::InvalidData("construction_method is taken from the set 0, 1 or 2 per ISO 14496-12:2015 § 8.11.3.3"))
                }
            }
        };

        let data_reference_index = iloc.read_u16(16)?;

        if data_reference_index != 0 {
            return Err(Error::Unsupported(
                "external file references (iloc.data_reference_index != 0) are not supported",
            ));
        }

        let base_offset = iloc.read_u64(base_offset_size.to_bits())?;
        let extent_count = iloc.read_u16(16)?;

        if extent_count < 1 {
            return Err(Error::InvalidData(
                "extent_count must have a value 1 or greater per ISO 14496-12:2015 § 8.11.3.3",
            ));
        }

        let mut extents = vec_with_capacity(extent_count.to_usize())?;

        for _ in 0..extent_count {
            // Parsed but currently ignored, see `ItemLocationBoxExtent`
            let _extent_index = match &index_size {
                None | Some(IlocFieldSize::Zero) => None,
                Some(index_size) => {
                    debug_assert!(version == IlocVersion::One || version == IlocVersion::Two);
                    Some(iloc.read_u64(index_size.to_bits())?)
                }
            };

            // Per ISO 14496-12:2015 § 8.11.3.1:
            // "If the offset is not identified (the field has a length of zero), then the
            //  beginning of the source (offset 0) is implied"
            // This behavior will follow from BitReader::read_u64(0) -> 0.
            let extent_offset = iloc.read_u64(offset_size.to_bits())?;
            let extent_length = iloc.read_u64(length_size.to_bits())?;

            // "If the length is not specified, or specified as zero, then the entire length of
            //  the source is implied" (ibid)
            let start = base_offset
                .checked_add(extent_offset)
                .ok_or(Error::InvalidData("offset calculation overflow"))?;
            let extent_range = if extent_length == 0 {
                ExtentRange::ToEnd(RangeFrom { start })
            } else {
                let end = start
                    .checked_add(extent_length)
                    .ok_or(Error::InvalidData("end calculation overflow"))?;
                ExtentRange::WithLength(Range { start, end })
            };

            vec_push(&mut extents, ItemLocationBoxExtent { extent_range })?;
        }

        vec_push(
            &mut items,
            ItemLocationBoxItem {
                item_id,
                construction_method,
                extents,
            },
        )?;
    }

    debug_assert_eq!(iloc.remaining(), 0);

    Ok(items)
}

/// Read the contents of a box, including sub boxes.
///
/// Metadata is accumulated in the passed-through `MediaContext` struct,
/// which can be examined later.
pub fn read_mp4<T: Read>(f: &mut T, context: &mut MediaContext) -> Result<()> {
    let mut found_ftyp = false;
    let mut found_moov = false;
    // TODO(kinetik): Top-level parsing should handle zero-sized boxes
    // rather than throwing an error.
    let mut iter = BoxIter::new(f);
    while let Some(mut b) = iter.next_box()? {
        // box ordering: ftyp before any variable length box (inc. moov),
        // but may not be first box in file if file signatures etc. present
        // fragmented mp4 order: ftyp, moov, pairs of moof/mdat (1-multiple), mfra

        // "special": uuid, wide (= 8 bytes)
        // isom: moov, mdat, free, skip, udta, ftyp, moof, mfra
        // iso2: pdin, meta
        // iso3: meco
        // iso5: styp, sidx, ssix, prft
        // unknown, maybe: id32

        // qt: pnot

        // possibly allow anything where all printable and/or all lowercase printable
        // "four printable characters from the ISO 8859-1 character set"
        match b.head.name {
            BoxType::FileTypeBox => {
                let ftyp = read_ftyp(&mut b)?;
                found_ftyp = true;
                debug!("{:?}", ftyp);
            }
            BoxType::MovieBox => {
                read_moov(&mut b, context)?;
                found_moov = true;
            }
            _ => skip_box_content(&mut b)?,
        };
        check_parser_state!(b.content);
        if found_moov {
            debug!(
                "found moov {}, could stop pure 'moov' parser now",
                if found_ftyp {
                    "and ftyp"
                } else {
                    "but no ftyp"
                }
            );
        }
    }

    // XXX(kinetik): This isn't perfect, as a "moov" with no contents is
    // treated as okay but we haven't found anything useful.  Needs more
    // thought for clearer behaviour here.
    if found_moov {
        Ok(())
    } else {
        Err(Error::NoMoov)
    }
}

fn parse_mvhd<T: Read>(f: &mut BMFFBox<T>) -> Result<(MovieHeaderBox, Option<MediaTimeScale>)> {
    let mvhd = read_mvhd(f)?;
    if mvhd.timescale == 0 {
        return Err(Error::InvalidData("zero timescale in mdhd"));
    }
    let timescale = Some(MediaTimeScale(u64::from(mvhd.timescale)));
    Ok((mvhd, timescale))
}

fn read_moov<T: Read>(f: &mut BMFFBox<T>, context: &mut MediaContext) -> Result<()> {
    let mut iter = f.box_iter();
    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::MovieHeaderBox => {
                let (mvhd, timescale) = parse_mvhd(&mut b)?;
                context.timescale = timescale;
                debug!("{:?}", mvhd);
            }
            BoxType::TrackBox => {
                let mut track = Track::new(context.tracks.len());
                read_trak(&mut b, &mut track)?;
                vec_push(&mut context.tracks, track)?;
            }
            BoxType::MovieExtendsBox => {
                let mvex = read_mvex(&mut b)?;
                debug!("{:?}", mvex);
                context.mvex = Some(mvex);
            }
            BoxType::ProtectionSystemSpecificHeaderBox => {
                let pssh = read_pssh(&mut b)?;
                debug!("{:?}", pssh);
                vec_push(&mut context.psshs, pssh)?;
            }
            BoxType::UserdataBox => {
                let udta = read_udta(&mut b);
                debug!("{:?}", udta);
                context.userdata = Some(udta);
            }
            _ => skip_box_content(&mut b)?,
        };
        check_parser_state!(b.content);
    }
    Ok(())
}

fn read_pssh<T: Read>(src: &mut BMFFBox<T>) -> Result<ProtectionSystemSpecificHeaderBox> {
    let len = src.bytes_left();
    let mut box_content = read_buf(src, len)?;
    let (system_id, kid, data) = {
        let pssh = &mut Cursor::new(box_content.as_slice());

        let (version, _) = read_fullbox_extra(pssh)?;

        let system_id = read_buf(pssh, 16)?;

        let mut kid: Vec<ByteData> = Vec::new();
        if version > 0 {
            let count = be_u32_with_limit(pssh)?;
            for _ in 0..count {
                let item = read_buf(pssh, 16)?;
                vec_push(&mut kid, item)?;
            }
        }

        let data_size = be_u32_with_limit(pssh)?;
        let data = read_buf(pssh, data_size.into())?;

        (system_id, kid, data)
    };

    let mut pssh_box = Vec::new();
    write_be_u32(&mut pssh_box, src.head.size.try_into()?)?;
    pssh_box.extend_from_slice(b"pssh");
    pssh_box.append(&mut box_content);

    Ok(ProtectionSystemSpecificHeaderBox {
        system_id,
        kid,
        data,
        box_content: pssh_box,
    })
}

fn read_mvex<T: Read>(src: &mut BMFFBox<T>) -> Result<MovieExtendsBox> {
    let mut iter = src.box_iter();
    let mut fragment_duration = None;
    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::MovieExtendsHeaderBox => {
                let duration = read_mehd(&mut b)?;
                fragment_duration = Some(duration);
            }
            _ => skip_box_content(&mut b)?,
        }
    }
    Ok(MovieExtendsBox { fragment_duration })
}

fn read_mehd<T: Read>(src: &mut BMFFBox<T>) -> Result<MediaScaledTime> {
    let (version, _) = read_fullbox_extra(src)?;
    let fragment_duration = match version {
        1 => be_u64(src)?,
        0 => u64::from(be_u32(src)?),
        _ => return Err(Error::InvalidData("unhandled mehd version")),
    };
    Ok(MediaScaledTime(fragment_duration))
}

fn read_trak<T: Read>(f: &mut BMFFBox<T>, track: &mut Track) -> Result<()> {
    let mut iter = f.box_iter();
    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::TrackHeaderBox => {
                let tkhd = read_tkhd(&mut b)?;
                track.track_id = Some(tkhd.track_id);
                track.tkhd = Some(tkhd.clone());
                debug!("{:?}", tkhd);
            }
            BoxType::EditBox => read_edts(&mut b, track)?,
            BoxType::MediaBox => read_mdia(&mut b, track)?,
            _ => skip_box_content(&mut b)?,
        };
        check_parser_state!(b.content);
    }
    Ok(())
}

fn read_edts<T: Read>(f: &mut BMFFBox<T>, track: &mut Track) -> Result<()> {
    let mut iter = f.box_iter();
    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::EditListBox => {
                let elst = read_elst(&mut b)?;
                if elst.edits.is_empty() {
                    debug!("empty edit list");
                    continue;
                }
                let mut empty_duration = 0;
                let mut idx = 0;
                if elst.edits[idx].media_time == -1 {
                    if elst.edits.len() < 2 {
                        debug!("expected additional edit, ignoring edit list");
                        continue;
                    }
                    empty_duration = elst.edits[idx].segment_duration;
                    idx += 1;
                }
                track.empty_duration = Some(MediaScaledTime(empty_duration));
                let media_time = elst.edits[idx].media_time;
                if media_time < 0 {
                    debug!("unexpected negative media time in edit");
                }
                track.media_time = Some(TrackScaledTime::<u64>(
                    std::cmp::max(0, media_time) as u64,
                    track.id,
                ));
                if elst.edits.len() > 2 {
                    debug!("ignoring edit list with {} entries", elst.edits.len());
                }
                debug!("{:?}", elst);
            }
            _ => skip_box_content(&mut b)?,
        };
        check_parser_state!(b.content);
    }
    Ok(())
}

#[allow(clippy::type_complexity)] // Allow the complex return, maybe rework in future
fn parse_mdhd<T: Read>(
    f: &mut BMFFBox<T>,
    track: &mut Track,
) -> Result<(
    MediaHeaderBox,
    Option<TrackScaledTime<u64>>,
    Option<TrackTimeScale<u64>>,
)> {
    let mdhd = read_mdhd(f)?;
    let duration = match mdhd.duration {
        std::u64::MAX => None,
        duration => Some(TrackScaledTime::<u64>(duration, track.id)),
    };
    if mdhd.timescale == 0 {
        return Err(Error::InvalidData("zero timescale in mdhd"));
    }
    let timescale = Some(TrackTimeScale::<u64>(u64::from(mdhd.timescale), track.id));
    Ok((mdhd, duration, timescale))
}

fn read_mdia<T: Read>(f: &mut BMFFBox<T>, track: &mut Track) -> Result<()> {
    let mut iter = f.box_iter();
    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::MediaHeaderBox => {
                let (mdhd, duration, timescale) = parse_mdhd(&mut b, track)?;
                track.duration = duration;
                track.timescale = timescale;
                debug!("{:?}", mdhd);
            }
            BoxType::HandlerBox => {
                let hdlr = read_hdlr(&mut b)?;

                match hdlr.handler_type.value.as_ref() {
                    "vide" => track.track_type = TrackType::Video,
                    "soun" => track.track_type = TrackType::Audio,
                    "meta" => track.track_type = TrackType::Metadata,
                    _ => (),
                }
                debug!("{:?}", hdlr);
            }
            BoxType::MediaInformationBox => read_minf(&mut b, track)?,
            _ => skip_box_content(&mut b)?,
        };
        check_parser_state!(b.content);
    }
    Ok(())
}

fn read_minf<T: Read>(f: &mut BMFFBox<T>, track: &mut Track) -> Result<()> {
    let mut iter = f.box_iter();
    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::SampleTableBox => read_stbl(&mut b, track)?,
            _ => skip_box_content(&mut b)?,
        };
        check_parser_state!(b.content);
    }
    Ok(())
}

fn read_stbl<T: Read>(f: &mut BMFFBox<T>, track: &mut Track) -> Result<()> {
    let mut iter = f.box_iter();
    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::SampleDescriptionBox => {
                let stsd = read_stsd(&mut b, track)?;
                debug!("{:?}", stsd);
                track.stsd = Some(stsd);
            }
            BoxType::TimeToSampleBox => {
                let stts = read_stts(&mut b)?;
                debug!("{:?}", stts);
                track.stts = Some(stts);
            }
            BoxType::SampleToChunkBox => {
                let stsc = read_stsc(&mut b)?;
                debug!("{:?}", stsc);
                track.stsc = Some(stsc);
            }
            BoxType::SampleSizeBox => {
                let stsz = read_stsz(&mut b)?;
                debug!("{:?}", stsz);
                track.stsz = Some(stsz);
            }
            BoxType::ChunkOffsetBox => {
                let stco = read_stco(&mut b)?;
                debug!("{:?}", stco);
                track.stco = Some(stco);
            }
            BoxType::ChunkLargeOffsetBox => {
                let co64 = read_co64(&mut b)?;
                debug!("{:?}", co64);
                track.stco = Some(co64);
            }
            BoxType::SyncSampleBox => {
                let stss = read_stss(&mut b)?;
                debug!("{:?}", stss);
                track.stss = Some(stss);
            }
            BoxType::CompositionOffsetBox => {
                let ctts = read_ctts(&mut b)?;
                debug!("{:?}", ctts);
                track.ctts = Some(ctts);
            }
            _ => skip_box_content(&mut b)?,
        };
        check_parser_state!(b.content);
    }
    Ok(())
}

/// Parse an ftyp box.
fn read_ftyp<T: Read>(src: &mut BMFFBox<T>) -> Result<FileTypeBox> {
    let major = be_u32(src)?;
    let minor = be_u32(src)?;
    let bytes_left = src.bytes_left();
    if bytes_left % 4 != 0 {
        return Err(Error::InvalidData("invalid ftyp size"));
    }
    // Is a brand_count of zero valid?
    let brand_count = bytes_left / 4;
    let mut brands = Vec::new();
    for _ in 0..brand_count {
        vec_push(&mut brands, From::from(be_u32(src)?))?;
    }
    Ok(FileTypeBox {
        major_brand: From::from(major),
        minor_version: minor,
        compatible_brands: brands,
    })
}

/// Parse an mvhd box.
fn read_mvhd<T: Read>(src: &mut BMFFBox<T>) -> Result<MovieHeaderBox> {
    let (version, _) = read_fullbox_extra(src)?;
    match version {
        // 64 bit creation and modification times.
        1 => {
            skip(src, 16)?;
        }
        // 32 bit creation and modification times.
        0 => {
            skip(src, 8)?;
        }
        _ => return Err(Error::InvalidData("unhandled mvhd version")),
    }
    let timescale = be_u32(src)?;
    let duration = match version {
        1 => be_u64(src)?,
        0 => {
            let d = be_u32(src)?;
            if d == std::u32::MAX {
                std::u64::MAX
            } else {
                u64::from(d)
            }
        }
        _ => return Err(Error::InvalidData("unhandled mvhd version")),
    };
    // Skip remaining fields.
    skip(src, 80)?;
    Ok(MovieHeaderBox {
        timescale,
        duration,
    })
}

/// Parse a tkhd box.
fn read_tkhd<T: Read>(src: &mut BMFFBox<T>) -> Result<TrackHeaderBox> {
    let (version, flags) = read_fullbox_extra(src)?;
    let disabled = flags & 0x1u32 == 0 || flags & 0x2u32 == 0;
    match version {
        // 64 bit creation and modification times.
        1 => {
            skip(src, 16)?;
        }
        // 32 bit creation and modification times.
        0 => {
            skip(src, 8)?;
        }
        _ => return Err(Error::InvalidData("unhandled tkhd version")),
    }
    let track_id = be_u32(src)?;
    skip(src, 4)?;
    let duration = match version {
        1 => be_u64(src)?,
        0 => u64::from(be_u32(src)?),
        _ => return Err(Error::InvalidData("unhandled tkhd version")),
    };
    // Skip uninteresting fields.
    skip(src, 16)?;

    let matrix = Matrix {
        a: be_i32(src)?,
        b: be_i32(src)?,
        u: be_i32(src)?,
        c: be_i32(src)?,
        d: be_i32(src)?,
        v: be_i32(src)?,
        x: be_i32(src)?,
        y: be_i32(src)?,
        w: be_i32(src)?,
    };

    let width = be_u32(src)?;
    let height = be_u32(src)?;
    Ok(TrackHeaderBox {
        track_id,
        disabled,
        duration,
        width,
        height,
        matrix,
    })
}

/// Parse a elst box.
fn read_elst<T: Read>(src: &mut BMFFBox<T>) -> Result<EditListBox> {
    let (version, _) = read_fullbox_extra(src)?;
    let edit_count = be_u32_with_limit(src)?;
    let mut edits = Vec::new();
    for _ in 0..edit_count {
        let (segment_duration, media_time) = match version {
            1 => {
                // 64 bit segment duration and media times.
                (be_u64(src)?, be_i64(src)?)
            }
            0 => {
                // 32 bit segment duration and media times.
                (u64::from(be_u32(src)?), i64::from(be_i32(src)?))
            }
            _ => return Err(Error::InvalidData("unhandled elst version")),
        };
        let media_rate_integer = be_i16(src)?;
        let media_rate_fraction = be_i16(src)?;
        vec_push(
            &mut edits,
            Edit {
                segment_duration,
                media_time,
                media_rate_integer,
                media_rate_fraction,
            },
        )?;
    }

    // Padding could be added in some contents.
    skip_box_remain(src)?;

    Ok(EditListBox { edits })
}

/// Parse a mdhd box.
fn read_mdhd<T: Read>(src: &mut BMFFBox<T>) -> Result<MediaHeaderBox> {
    let (version, _) = read_fullbox_extra(src)?;
    let (timescale, duration) = match version {
        1 => {
            // Skip 64-bit creation and modification times.
            skip(src, 16)?;

            // 64 bit duration.
            (be_u32(src)?, be_u64(src)?)
        }
        0 => {
            // Skip 32-bit creation and modification times.
            skip(src, 8)?;

            // 32 bit duration.
            let timescale = be_u32(src)?;
            let duration = {
                // Since we convert the 32-bit duration to 64-bit by
                // upcasting, we need to preserve the special all-1s
                // ("unknown") case by hand.
                let d = be_u32(src)?;
                if d == std::u32::MAX {
                    std::u64::MAX
                } else {
                    u64::from(d)
                }
            };
            (timescale, duration)
        }
        _ => return Err(Error::InvalidData("unhandled mdhd version")),
    };

    // Skip uninteresting fields.
    skip(src, 4)?;

    Ok(MediaHeaderBox {
        timescale,
        duration,
    })
}

/// Parse a stco box.
fn read_stco<T: Read>(src: &mut BMFFBox<T>) -> Result<ChunkOffsetBox> {
    let (_, _) = read_fullbox_extra(src)?;
    let offset_count = be_u32_with_limit(src)?;
    let mut offsets = Vec::new();
    for _ in 0..offset_count {
        vec_push(&mut offsets, u64::from(be_u32(src)?))?;
    }

    // Padding could be added in some contents.
    skip_box_remain(src)?;

    Ok(ChunkOffsetBox { offsets })
}

/// Parse a co64 box.
fn read_co64<T: Read>(src: &mut BMFFBox<T>) -> Result<ChunkOffsetBox> {
    let (_, _) = read_fullbox_extra(src)?;
    let offset_count = be_u32_with_limit(src)?;
    let mut offsets = Vec::new();
    for _ in 0..offset_count {
        vec_push(&mut offsets, be_u64(src)?)?;
    }

    // Padding could be added in some contents.
    skip_box_remain(src)?;

    Ok(ChunkOffsetBox { offsets })
}

/// Parse a stss box.
fn read_stss<T: Read>(src: &mut BMFFBox<T>) -> Result<SyncSampleBox> {
    let (_, _) = read_fullbox_extra(src)?;
    let sample_count = be_u32_with_limit(src)?;
    let mut samples = Vec::new();
    for _ in 0..sample_count {
        vec_push(&mut samples, be_u32(src)?)?;
    }

    // Padding could be added in some contents.
    skip_box_remain(src)?;

    Ok(SyncSampleBox { samples })
}

/// Parse a stsc box.
fn read_stsc<T: Read>(src: &mut BMFFBox<T>) -> Result<SampleToChunkBox> {
    let (_, _) = read_fullbox_extra(src)?;
    let sample_count = be_u32_with_limit(src)?;
    let mut samples = Vec::new();
    for _ in 0..sample_count {
        let first_chunk = be_u32(src)?;
        let samples_per_chunk = be_u32_with_limit(src)?;
        let sample_description_index = be_u32(src)?;
        vec_push(
            &mut samples,
            SampleToChunk {
                first_chunk,
                samples_per_chunk,
                sample_description_index,
            },
        )?;
    }

    // Padding could be added in some contents.
    skip_box_remain(src)?;

    Ok(SampleToChunkBox { samples })
}

fn read_ctts<T: Read>(src: &mut BMFFBox<T>) -> Result<CompositionOffsetBox> {
    let (version, _) = read_fullbox_extra(src)?;

    let counts = u64::from(be_u32_with_limit(src)?);

    if src.bytes_left() < counts.checked_mul(8).expect("counts -> bytes overflow") {
        return Err(Error::InvalidData("insufficient data in 'ctts' box"));
    }

    let mut offsets = Vec::new();
    for _ in 0..counts {
        let (sample_count, time_offset) = match version {
            // According to spec, Version0 shoule be used when version == 0;
            // however, some buggy contents have negative value when version == 0.
            // So we always use Version1 here.
            0..=1 => {
                let count = be_u32_with_limit(src)?;
                let offset = TimeOffsetVersion::Version1(be_i32(src)?);
                (count, offset)
            }
            _ => {
                return Err(Error::InvalidData("unsupported version in 'ctts' box"));
            }
        };
        vec_push(
            &mut offsets,
            TimeOffset {
                sample_count,
                time_offset,
            },
        )?;
    }

    skip_box_remain(src)?;

    Ok(CompositionOffsetBox { samples: offsets })
}

/// Parse a stsz box.
fn read_stsz<T: Read>(src: &mut BMFFBox<T>) -> Result<SampleSizeBox> {
    let (_, _) = read_fullbox_extra(src)?;
    let sample_size = be_u32(src)?;
    let sample_count = be_u32_with_limit(src)?;
    let mut sample_sizes = Vec::new();
    if sample_size == 0 {
        for _ in 0..sample_count {
            vec_push(&mut sample_sizes, be_u32(src)?)?;
        }
    }

    // Padding could be added in some contents.
    skip_box_remain(src)?;

    Ok(SampleSizeBox {
        sample_size,
        sample_sizes,
    })
}

/// Parse a stts box.
fn read_stts<T: Read>(src: &mut BMFFBox<T>) -> Result<TimeToSampleBox> {
    let (_, _) = read_fullbox_extra(src)?;
    let sample_count = be_u32_with_limit(src)?;
    let mut samples = Vec::new();
    for _ in 0..sample_count {
        let sample_count = be_u32_with_limit(src)?;
        let sample_delta = be_u32(src)?;
        vec_push(
            &mut samples,
            Sample {
                sample_count,
                sample_delta,
            },
        )?;
    }

    // Padding could be added in some contents.
    skip_box_remain(src)?;

    Ok(TimeToSampleBox { samples })
}

/// Parse a VPx Config Box.
fn read_vpcc<T: Read>(src: &mut BMFFBox<T>) -> Result<VPxConfigBox> {
    let (version, _) = read_fullbox_extra(src)?;
    let supported_versions = [0, 1];
    if !supported_versions.contains(&version) {
        return Err(Error::Unsupported("unknown vpcC version"));
    }

    let profile = src.read_u8()?;
    let level = src.read_u8()?;
    let (
        bit_depth,
        colour_primaries,
        chroma_subsampling,
        transfer_characteristics,
        matrix_coefficients,
        video_full_range_flag,
    ) = if version == 0 {
        let (bit_depth, colour_primaries) = {
            let byte = src.read_u8()?;
            ((byte >> 4) & 0x0f, byte & 0x0f)
        };
        // Note, transfer_characteristics was known as transfer_function in v0
        let (chroma_subsampling, transfer_characteristics, video_full_range_flag) = {
            let byte = src.read_u8()?;
            ((byte >> 4) & 0x0f, (byte >> 1) & 0x07, (byte & 1) == 1)
        };
        (
            bit_depth,
            colour_primaries,
            chroma_subsampling,
            transfer_characteristics,
            None,
            video_full_range_flag,
        )
    } else {
        let (bit_depth, chroma_subsampling, video_full_range_flag) = {
            let byte = src.read_u8()?;
            ((byte >> 4) & 0x0f, (byte >> 1) & 0x07, (byte & 1) == 1)
        };
        let colour_primaries = src.read_u8()?;
        let transfer_characteristics = src.read_u8()?;
        let matrix_coefficients = src.read_u8()?;

        (
            bit_depth,
            colour_primaries,
            chroma_subsampling,
            transfer_characteristics,
            Some(matrix_coefficients),
            video_full_range_flag,
        )
    };

    let codec_init_size = be_u16(src)?;
    let codec_init = read_buf(src, codec_init_size.into())?;

    // TODO(rillian): validate field value ranges.
    Ok(VPxConfigBox {
        profile,
        level,
        bit_depth,
        colour_primaries,
        chroma_subsampling,
        transfer_characteristics,
        matrix_coefficients,
        video_full_range_flag,
        codec_init,
    })
}

fn read_av1c<T: Read>(src: &mut BMFFBox<T>) -> Result<AV1ConfigBox> {
    let marker_byte = src.read_u8()?;
    if marker_byte & 0x80 != 0x80 {
        return Err(Error::Unsupported("missing av1C marker bit"));
    }
    if marker_byte & 0x7f != 0x01 {
        return Err(Error::Unsupported("missing av1C marker bit"));
    }
    let profile_byte = src.read_u8()?;
    let profile = (profile_byte & 0xe0) >> 5;
    let level = profile_byte & 0x1f;
    let flags_byte = src.read_u8()?;
    let tier = (flags_byte & 0x80) >> 7;
    let bit_depth = match flags_byte & 0x60 {
        0x60 => 12,
        0x40 => 10,
        _ => 8,
    };
    let monochrome = flags_byte & 0x10 == 0x10;
    let chroma_subsampling_x = (flags_byte & 0x08) >> 3;
    let chroma_subsampling_y = (flags_byte & 0x04) >> 2;
    let chroma_sample_position = flags_byte & 0x03;
    let delay_byte = src.read_u8()?;
    let initial_presentation_delay_present = (delay_byte & 0x10) == 0x10;
    let initial_presentation_delay_minus_one = if initial_presentation_delay_present {
        delay_byte & 0x0f
    } else {
        0
    };

    let config_obus_size = src.bytes_left();
    let config_obus = read_buf(src, config_obus_size)?;

    Ok(AV1ConfigBox {
        profile,
        level,
        tier,
        bit_depth,
        monochrome,
        chroma_subsampling_x,
        chroma_subsampling_y,
        chroma_sample_position,
        initial_presentation_delay_present,
        initial_presentation_delay_minus_one,
        config_obus,
    })
}

fn read_flac_metadata<T: Read>(src: &mut BMFFBox<T>) -> Result<FLACMetadataBlock> {
    let temp = src.read_u8()?;
    let block_type = temp & 0x7f;
    let length = be_u24(src)?.into();
    if length > src.bytes_left() {
        return Err(Error::InvalidData(
            "FLACMetadataBlock larger than parent box",
        ));
    }
    let data = read_buf(src, length)?;
    Ok(FLACMetadataBlock { block_type, data })
}

fn find_descriptor(data: &[u8], esds: &mut ES_Descriptor) -> Result<()> {
    // Tags for elementary stream description
    const ESDESCR_TAG: u8 = 0x03;
    const DECODER_CONFIG_TAG: u8 = 0x04;
    const DECODER_SPECIFIC_TAG: u8 = 0x05;

    let mut remains = data;

    // Descriptor length should be more than 2 bytes.
    while remains.len() > 2 {
        let des = &mut Cursor::new(remains);
        let tag = des.read_u8()?;

        let mut end: u32 = 0; // It's u8 without declaration type that is incorrect.
                              // MSB of extend_or_len indicates more bytes, up to 4 bytes.
        for _ in 0..4 {
            if des.position() == remains.len().to_u64() {
                // There's nothing more to read, the 0x80 was actually part of
                // the content, and not an extension size.
                end = des.position() as u32;
                break;
            }
            let extend_or_len = des.read_u8()?;
            end = (end << 7) + u32::from(extend_or_len & 0x7F);
            if (extend_or_len & 0x80) == 0 {
                end += des.position() as u32;
                break;
            }
        }

        if end.to_usize() > remains.len() || u64::from(end) < des.position() {
            return Err(Error::InvalidData("Invalid descriptor."));
        }

        let descriptor = &remains[des.position().try_into()?..end.to_usize()];

        match tag {
            ESDESCR_TAG => {
                read_es_descriptor(descriptor, esds)?;
            }
            DECODER_CONFIG_TAG => {
                read_dc_descriptor(descriptor, esds)?;
            }
            DECODER_SPECIFIC_TAG => {
                read_ds_descriptor(descriptor, esds)?;
            }
            _ => {
                debug!("Unsupported descriptor, tag {}", tag);
            }
        }

        remains = &remains[end.to_usize()..remains.len()];
    }

    Ok(())
}

fn get_audio_object_type(bit_reader: &mut BitReader) -> Result<u16> {
    let mut audio_object_type: u16 = ReadInto::read(bit_reader, 5)?;

    // Extend audio object type, for example, HE-AAC.
    if audio_object_type == 31 {
        let audio_object_type_ext: u16 = ReadInto::read(bit_reader, 6)?;
        audio_object_type = 32 + audio_object_type_ext;
    }
    Ok(audio_object_type)
}

fn read_ds_descriptor(data: &[u8], esds: &mut ES_Descriptor) -> Result<()> {
    let frequency_table = vec![
        (0x0, 96000),
        (0x1, 88200),
        (0x2, 64000),
        (0x3, 48000),
        (0x4, 44100),
        (0x5, 32000),
        (0x6, 24000),
        (0x7, 22050),
        (0x8, 16000),
        (0x9, 12000),
        (0xa, 11025),
        (0xb, 8000),
        (0xc, 7350),
    ];

    let bit_reader = &mut BitReader::new(data);

    let mut audio_object_type = get_audio_object_type(bit_reader)?;

    let sample_index: u32 = ReadInto::read(bit_reader, 4)?;

    // Sample frequency could be from table, or retrieved from stream directly
    // if index is 0x0f.
    let sample_frequency = match sample_index {
        0x0F => Some(ReadInto::read(bit_reader, 24)?),
        _ => frequency_table
            .iter()
            .find(|item| item.0 == sample_index)
            .map(|x| x.1),
    };

    let channel_configuration: u16 = ReadInto::read(bit_reader, 4)?;

    let extended_audio_object_type = match audio_object_type {
        5 | 29 => Some(5),
        _ => None,
    };

    if audio_object_type == 5 || audio_object_type == 29 {
        // We have an explicit signaling for BSAC extension, should the decoder
        // decode the BSAC extension (all Gecko's AAC decoders do), then this is
        // what the stream will actually look like once decoded.
        let _extended_sample_index = ReadInto::read(bit_reader, 4)?;
        let _extended_sample_frequency: Option<u32> = match _extended_sample_index {
            0x0F => Some(ReadInto::read(bit_reader, 24)?),
            _ => frequency_table
                .iter()
                .find(|item| item.0 == sample_index)
                .map(|x| x.1),
        };
        audio_object_type = get_audio_object_type(bit_reader)?;
        let _extended_channel_configuration = match audio_object_type {
            22 => ReadInto::read(bit_reader, 4)?,
            _ => channel_configuration,
        };
    };

    match audio_object_type {
        1..=4 | 6 | 7 | 17 | 19..=23 => {
            if sample_frequency.is_none() {
                return Err(Error::Unsupported("unknown frequency"));
            }

            // parsing GASpecificConfig

            // If the sampling rate is not one of the rates listed in the right
            // column in Table 4.82, the sampling frequency dependent tables
            // (code tables, scale factor band tables etc.) must be deduced in
            // order for the bitstream payload to be parsed. Since a given
            // sampling frequency is associated with only one sampling frequency
            // table, and since maximum flexibility is desired in the range of
            // possible sampling frequencies, the following table shall be used
            // to associate an implied sampling frequency with the desired
            // sampling frequency dependent tables.
            let sample_frequency_value = match sample_frequency.unwrap() {
                0..=9390 => 8000,
                9391..=11501 => 11025,
                11502..=13855 => 12000,
                13856..=18782 => 16000,
                18783..=23003 => 22050,
                23004..=27712 => 24000,
                27713..=37565 => 32000,
                37566..=46008 => 44100,
                46009..=55425 => 48000,
                55426..=75131 => 64000,
                75132..=92016 => 88200,
                _ => 96000,
            };

            bit_reader.skip(1)?; // frameLengthFlag
            let depend_on_core_order: u8 = ReadInto::read(bit_reader, 1)?;
            if depend_on_core_order > 0 {
                bit_reader.skip(14)?; // codeCoderDelay
            }
            bit_reader.skip(1)?; // extensionFlag

            let channel_counts = match channel_configuration {
                0 => {
                    debug!("Parsing program_config_element for channel counts");

                    bit_reader.skip(4)?; // element_instance_tag
                    bit_reader.skip(2)?; // object_type
                    bit_reader.skip(4)?; // sampling_frequency_index
                    let num_front_channel: u8 = ReadInto::read(bit_reader, 4)?;
                    let num_side_channel: u8 = ReadInto::read(bit_reader, 4)?;
                    let num_back_channel: u8 = ReadInto::read(bit_reader, 4)?;
                    let num_lfe_channel: u8 = ReadInto::read(bit_reader, 2)?;
                    bit_reader.skip(3)?; // num_assoc_data
                    bit_reader.skip(4)?; // num_valid_cc

                    let mono_mixdown_present: bool = ReadInto::read(bit_reader, 1)?;
                    if mono_mixdown_present {
                        bit_reader.skip(4)?; // mono_mixdown_element_number
                    }

                    let stereo_mixdown_present: bool = ReadInto::read(bit_reader, 1)?;
                    if stereo_mixdown_present {
                        bit_reader.skip(4)?; // stereo_mixdown_element_number
                    }

                    let matrix_mixdown_idx_present: bool = ReadInto::read(bit_reader, 1)?;
                    if matrix_mixdown_idx_present {
                        bit_reader.skip(2)?; // matrix_mixdown_idx
                        bit_reader.skip(1)?; // pseudo_surround_enable
                    }
                    let mut _channel_counts = 0;
                    _channel_counts += read_surround_channel_count(bit_reader, num_front_channel)?;
                    _channel_counts += read_surround_channel_count(bit_reader, num_side_channel)?;
                    _channel_counts += read_surround_channel_count(bit_reader, num_back_channel)?;
                    _channel_counts += read_surround_channel_count(bit_reader, num_lfe_channel)?;
                    _channel_counts
                }
                1..=7 => channel_configuration,
                // Amendment 4 of the AAC standard in 2013 below
                11 => 7,      // 6.1 Amendment 4 of the AAC standard in 2013
                12 | 14 => 8, // 7.1 (a/d) of ITU BS.2159
                _ => {
                    return Err(Error::Unsupported("invalid channel configuration"));
                }
            };

            esds.audio_object_type = Some(audio_object_type);
            esds.extended_audio_object_type = extended_audio_object_type;
            esds.audio_sample_rate = Some(sample_frequency_value);
            esds.audio_channel_count = Some(channel_counts);
            assert!(esds.decoder_specific_data.is_empty());
            esds.decoder_specific_data.extend_from_slice(data);

            Ok(())
        }
        _ => Err(Error::Unsupported("unknown aac audio object type")),
    }
}

fn read_surround_channel_count(bit_reader: &mut BitReader, channels: u8) -> Result<u16> {
    let mut count = 0;
    for _ in 0..channels {
        let is_cpe: bool = ReadInto::read(bit_reader, 1)?;
        count += if is_cpe { 2 } else { 1 };
        bit_reader.skip(4)?;
    }
    Ok(count)
}

fn read_dc_descriptor(data: &[u8], esds: &mut ES_Descriptor) -> Result<()> {
    let des = &mut Cursor::new(data);
    let object_profile = des.read_u8()?;

    // Skip uninteresting fields.
    skip(des, 12)?;

    if data.len().to_u64() > des.position() {
        find_descriptor(&data[des.position().try_into()?..data.len()], esds)?;
    }

    esds.audio_codec = match object_profile {
        0x40 | 0x41 => CodecType::AAC,
        0x6B => CodecType::MP3,
        _ => CodecType::Unknown,
    };

    Ok(())
}

fn read_es_descriptor(data: &[u8], esds: &mut ES_Descriptor) -> Result<()> {
    let des = &mut Cursor::new(data);

    skip(des, 2)?;

    let esds_flags = des.read_u8()?;

    // Stream dependency flag, first bit from left most.
    if esds_flags & 0x80 > 0 {
        // Skip uninteresting fields.
        skip(des, 2)?;
    }

    // Url flag, second bit from left most.
    if esds_flags & 0x40 > 0 {
        // Skip uninteresting fields.
        let skip_es_len = u64::from(des.read_u8()?) + 2;
        skip(des, skip_es_len)?;
    }

    if data.len().to_u64() > des.position() {
        find_descriptor(&data[des.position().try_into()?..data.len()], esds)?;
    }

    Ok(())
}

fn read_esds<T: Read>(src: &mut BMFFBox<T>) -> Result<ES_Descriptor> {
    let (_, _) = read_fullbox_extra(src)?;

    // Subtract 4 extra to offset the members of fullbox not accounted for in
    // head.offset
    let esds_size = src
        .head
        .size
        .checked_sub(src.head.offset + 4)
        .expect("offset invalid");
    let esds_array = read_buf(src, esds_size)?;

    let mut es_data = ES_Descriptor::default();
    find_descriptor(&esds_array, &mut es_data)?;

    es_data.codec_esds = esds_array;

    Ok(es_data)
}

/// Parse `FLACSpecificBox`.
fn read_dfla<T: Read>(src: &mut BMFFBox<T>) -> Result<FLACSpecificBox> {
    let (version, flags) = read_fullbox_extra(src)?;
    if version != 0 {
        return Err(Error::Unsupported("unknown dfLa (FLAC) version"));
    }
    if flags != 0 {
        return Err(Error::InvalidData("no-zero dfLa (FLAC) flags"));
    }
    let mut blocks = Vec::new();
    while src.bytes_left() > 0 {
        let block = read_flac_metadata(src)?;
        vec_push(&mut blocks, block)?;
    }
    // The box must have at least one meta block, and the first block
    // must be the METADATA_BLOCK_STREAMINFO
    if blocks.is_empty() {
        return Err(Error::InvalidData("FLACSpecificBox missing metadata"));
    } else if blocks[0].block_type != 0 {
        return Err(Error::InvalidData(
            "FLACSpecificBox must have STREAMINFO metadata first",
        ));
    } else if blocks[0].data.len() != 34 {
        return Err(Error::InvalidData(
            "FLACSpecificBox STREAMINFO block is the wrong size",
        ));
    }
    Ok(FLACSpecificBox { version, blocks })
}

/// Parse `OpusSpecificBox`.
fn read_dops<T: Read>(src: &mut BMFFBox<T>) -> Result<OpusSpecificBox> {
    let version = src.read_u8()?;
    if version != 0 {
        return Err(Error::Unsupported("unknown dOps (Opus) version"));
    }

    let output_channel_count = src.read_u8()?;
    let pre_skip = be_u16(src)?;
    let input_sample_rate = be_u32(src)?;
    let output_gain = be_i16(src)?;
    let channel_mapping_family = src.read_u8()?;

    let channel_mapping_table = if channel_mapping_family == 0 {
        None
    } else {
        let stream_count = src.read_u8()?;
        let coupled_count = src.read_u8()?;
        let channel_mapping = read_buf(src, output_channel_count.into())?;

        Some(ChannelMappingTable {
            stream_count,
            coupled_count,
            channel_mapping,
        })
    };

    // TODO(kinetik): validate field value ranges.
    Ok(OpusSpecificBox {
        version,
        output_channel_count,
        pre_skip,
        input_sample_rate,
        output_gain,
        channel_mapping_family,
        channel_mapping_table,
    })
}

/// Re-serialize the Opus codec-specific config data as an `OpusHead` packet.
///
/// Some decoders expect the initialization data in the format used by the
/// Ogg and WebM encapsulations. To support this we prepend the `OpusHead`
/// tag and byte-swap the data from big- to little-endian relative to the
/// dOps box.
pub fn serialize_opus_header<W: byteorder::WriteBytesExt + std::io::Write>(
    opus: &OpusSpecificBox,
    dst: &mut W,
) -> Result<()> {
    match dst.write(b"OpusHead") {
        Err(e) => return Err(Error::from(e)),
        Ok(bytes) => {
            if bytes != 8 {
                return Err(Error::InvalidData("Couldn't write OpusHead tag."));
            }
        }
    }
    // In mp4 encapsulation, the version field is 0, but in ogg
    // it is 1. While decoders generally accept zero as well, write
    // out the version of the header we're supporting rather than
    // whatever we parsed out of mp4.
    dst.write_u8(1)?;
    dst.write_u8(opus.output_channel_count)?;
    dst.write_u16::<byteorder::LittleEndian>(opus.pre_skip)?;
    dst.write_u32::<byteorder::LittleEndian>(opus.input_sample_rate)?;
    dst.write_i16::<byteorder::LittleEndian>(opus.output_gain)?;
    dst.write_u8(opus.channel_mapping_family)?;
    match opus.channel_mapping_table {
        None => {}
        Some(ref table) => {
            dst.write_u8(table.stream_count)?;
            dst.write_u8(table.coupled_count)?;
            match dst.write(&table.channel_mapping) {
                Err(e) => return Err(Error::from(e)),
                Ok(bytes) => {
                    if bytes != table.channel_mapping.len() {
                        return Err(Error::InvalidData(
                            "Couldn't write channel mapping table data.",
                        ));
                    }
                }
            }
        }
    };
    Ok(())
}

/// Parse `ALACSpecificBox`.
fn read_alac<T: Read>(src: &mut BMFFBox<T>) -> Result<ALACSpecificBox> {
    let (version, flags) = read_fullbox_extra(src)?;
    if version != 0 {
        return Err(Error::Unsupported("unknown alac (ALAC) version"));
    }
    if flags != 0 {
        return Err(Error::InvalidData("no-zero alac (ALAC) flags"));
    }

    let length = match src.bytes_left() {
        x @ 24 | x @ 48 => x,
        _ => {
            return Err(Error::InvalidData(
                "ALACSpecificBox magic cookie is the wrong size",
            ))
        }
    };
    let data = read_buf(src, length)?;

    Ok(ALACSpecificBox { version, data })
}

/// Parse a hdlr box.
fn read_hdlr<T: Read>(src: &mut BMFFBox<T>) -> Result<HandlerBox> {
    let (_, _) = read_fullbox_extra(src)?;

    // Skip uninteresting fields.
    skip(src, 4)?;

    let handler_type = FourCC::from(be_u32(src)?);

    // Skip uninteresting fields.
    skip(src, 12)?;

    // Skip name.
    skip_box_remain(src)?;

    Ok(HandlerBox { handler_type })
}

/// Parse an video description inside an stsd box.
fn read_video_sample_entry<T: Read>(src: &mut BMFFBox<T>) -> Result<SampleEntry> {
    let name = src.get_header().name;
    let codec_type = match name {
        BoxType::AVCSampleEntry | BoxType::AVC3SampleEntry => CodecType::H264,
        BoxType::MP4VideoSampleEntry => CodecType::MP4V,
        BoxType::VP8SampleEntry => CodecType::VP8,
        BoxType::VP9SampleEntry => CodecType::VP9,
        BoxType::AV1SampleEntry => CodecType::AV1,
        BoxType::ProtectedVisualSampleEntry => CodecType::EncryptedVideo,
        _ => {
            debug!("Unsupported video codec, box {:?} found", name);
            CodecType::Unknown
        }
    };

    // Skip uninteresting fields.
    skip(src, 6)?;

    let data_reference_index = be_u16(src)?;

    // Skip uninteresting fields.
    skip(src, 16)?;

    let width = be_u16(src)?;
    let height = be_u16(src)?;

    // Skip uninteresting fields.
    skip(src, 50)?;

    // Skip clap/pasp/etc. for now.
    let mut codec_specific = None;
    let mut protection_info = Vec::new();
    let mut iter = src.box_iter();
    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::AVCConfigurationBox => {
                if (name != BoxType::AVCSampleEntry
                    && name != BoxType::AVC3SampleEntry
                    && name != BoxType::ProtectedVisualSampleEntry)
                    || codec_specific.is_some()
                {
                    return Err(Error::InvalidData("malformed video sample entry"));
                }
                let avcc_size = b
                    .head
                    .size
                    .checked_sub(b.head.offset)
                    .expect("offset invalid");
                let avcc = read_buf(&mut b.content, avcc_size)?;
                debug!("{:?} (avcc)", avcc);
                // TODO(kinetik): Parse avcC box?  For now we just stash the data.
                codec_specific = Some(VideoCodecSpecific::AVCConfig(avcc));
            }
            BoxType::VPCodecConfigurationBox => {
                // vpcC
                if (name != BoxType::VP8SampleEntry
                    && name != BoxType::VP9SampleEntry
                    && name != BoxType::ProtectedVisualSampleEntry)
                    || codec_specific.is_some()
                {
                    return Err(Error::InvalidData("malformed video sample entry"));
                }
                let vpcc = read_vpcc(&mut b)?;
                codec_specific = Some(VideoCodecSpecific::VPxConfig(vpcc));
            }
            BoxType::AV1CodecConfigurationBox => {
                if name != BoxType::AV1SampleEntry {
                    return Err(Error::InvalidData("malformed video sample entry"));
                }
                let av1c = read_av1c(&mut b)?;
                codec_specific = Some(VideoCodecSpecific::AV1Config(av1c));
            }
            BoxType::ESDBox => {
                if name != BoxType::MP4VideoSampleEntry || codec_specific.is_some() {
                    return Err(Error::InvalidData("malformed video sample entry"));
                }
                let (_, _) = read_fullbox_extra(&mut b.content)?;
                // Subtract 4 extra to offset the members of fullbox not
                // accounted for in head.offset
                let esds_size = b
                    .head
                    .size
                    .checked_sub(b.head.offset + 4)
                    .expect("offset invalid");
                let esds = read_buf(&mut b.content, esds_size)?;
                codec_specific = Some(VideoCodecSpecific::ESDSConfig(esds));
            }
            BoxType::ProtectionSchemeInfoBox => {
                if name != BoxType::ProtectedVisualSampleEntry {
                    return Err(Error::InvalidData("malformed video sample entry"));
                }
                let sinf = read_sinf(&mut b)?;
                debug!("{:?} (sinf)", sinf);
                vec_push(&mut protection_info, sinf)?;
            }
            _ => {
                debug!("Unsupported video codec, box {:?} found", b.head.name);
                skip_box_content(&mut b)?;
            }
        }
        check_parser_state!(b.content);
    }

    Ok(
        codec_specific.map_or(SampleEntry::Unknown, |codec_specific| {
            SampleEntry::Video(VideoSampleEntry {
                codec_type,
                data_reference_index,
                width,
                height,
                codec_specific,
                protection_info,
            })
        }),
    )
}

fn read_qt_wave_atom<T: Read>(src: &mut BMFFBox<T>) -> Result<ES_Descriptor> {
    let mut codec_specific = None;
    let mut iter = src.box_iter();
    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::ESDBox => {
                let esds = read_esds(&mut b)?;
                codec_specific = Some(esds);
            }
            _ => skip_box_content(&mut b)?,
        }
    }

    codec_specific.ok_or_else(|| Error::InvalidData("malformed audio sample entry"))
}

/// Parse an audio description inside an stsd box.
fn read_audio_sample_entry<T: Read>(src: &mut BMFFBox<T>) -> Result<SampleEntry> {
    let name = src.get_header().name;

    // Skip uninteresting fields.
    skip(src, 6)?;

    let data_reference_index = be_u16(src)?;

    // XXX(kinetik): This is "reserved" in BMFF, but some old QT MOV variant
    // uses it, need to work out if we have to support it.  Without checking
    // here and reading extra fields after samplerate (or bailing with an
    // error), the parser loses sync completely.
    let version = be_u16(src)?;

    // Skip uninteresting fields.
    skip(src, 6)?;

    let mut channelcount = u32::from(be_u16(src)?);
    let samplesize = be_u16(src)?;

    // Skip uninteresting fields.
    skip(src, 4)?;

    let mut samplerate = f64::from(be_u32(src)? >> 16); // 16.16 fixed point;

    match version {
        0 => (),
        1 => {
            // Quicktime sound sample description version 1.
            // Skip uninteresting fields.
            skip(src, 16)?;
        }
        2 => {
            // Quicktime sound sample description version 2.
            skip(src, 4)?;
            samplerate = f64::from_bits(be_u64(src)?);
            channelcount = be_u32(src)?;
            skip(src, 20)?;
        }
        _ => {
            return Err(Error::Unsupported(
                "unsupported non-isom audio sample entry",
            ))
        }
    }

    let (mut codec_type, mut codec_specific) = match name {
        BoxType::MP3AudioSampleEntry => (CodecType::MP3, Some(AudioCodecSpecific::MP3)),
        BoxType::LPCMAudioSampleEntry => (CodecType::LPCM, Some(AudioCodecSpecific::LPCM)),
        _ => (CodecType::Unknown, None),
    };
    let mut protection_info = Vec::new();
    let mut iter = src.box_iter();
    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::ESDBox => {
                if (name != BoxType::MP4AudioSampleEntry
                    && name != BoxType::ProtectedAudioSampleEntry)
                    || codec_specific.is_some()
                {
                    return Err(Error::InvalidData("malformed audio sample entry"));
                }
                let esds = read_esds(&mut b)?;
                codec_type = esds.audio_codec;
                codec_specific = Some(AudioCodecSpecific::ES_Descriptor(esds));
            }
            BoxType::FLACSpecificBox => {
                if (name != BoxType::FLACSampleEntry && name != BoxType::ProtectedAudioSampleEntry)
                    || codec_specific.is_some()
                {
                    return Err(Error::InvalidData("malformed audio sample entry"));
                }
                let dfla = read_dfla(&mut b)?;
                codec_type = CodecType::FLAC;
                codec_specific = Some(AudioCodecSpecific::FLACSpecificBox(dfla));
            }
            BoxType::OpusSpecificBox => {
                if (name != BoxType::OpusSampleEntry && name != BoxType::ProtectedAudioSampleEntry)
                    || codec_specific.is_some()
                {
                    return Err(Error::InvalidData("malformed audio sample entry"));
                }
                let dops = read_dops(&mut b)?;
                codec_type = CodecType::Opus;
                codec_specific = Some(AudioCodecSpecific::OpusSpecificBox(dops));
            }
            BoxType::ALACSpecificBox => {
                if name != BoxType::ALACSpecificBox || codec_specific.is_some() {
                    return Err(Error::InvalidData("malformed audio sample entry"));
                }
                let alac = read_alac(&mut b)?;
                codec_type = CodecType::ALAC;
                codec_specific = Some(AudioCodecSpecific::ALACSpecificBox(alac));
            }
            BoxType::QTWaveAtom => {
                let qt_esds = read_qt_wave_atom(&mut b)?;
                codec_type = qt_esds.audio_codec;
                codec_specific = Some(AudioCodecSpecific::ES_Descriptor(qt_esds));
            }
            BoxType::ProtectionSchemeInfoBox => {
                if name != BoxType::ProtectedAudioSampleEntry {
                    return Err(Error::InvalidData("malformed audio sample entry"));
                }
                let sinf = read_sinf(&mut b)?;
                debug!("{:?} (sinf)", sinf);
                codec_type = CodecType::EncryptedAudio;
                vec_push(&mut protection_info, sinf)?;
            }
            _ => {
                debug!("Unsupported audio codec, box {:?} found", b.head.name);
                skip_box_content(&mut b)?;
            }
        }
        check_parser_state!(b.content);
    }

    Ok(
        codec_specific.map_or(SampleEntry::Unknown, |codec_specific| {
            SampleEntry::Audio(AudioSampleEntry {
                codec_type,
                data_reference_index,
                channelcount,
                samplesize,
                samplerate,
                codec_specific,
                protection_info,
            })
        }),
    )
}

/// Parse a stsd box.
fn read_stsd<T: Read>(src: &mut BMFFBox<T>, track: &mut Track) -> Result<SampleDescriptionBox> {
    let (_, _) = read_fullbox_extra(src)?;

    let description_count = be_u32(src)?;
    let mut descriptions = Vec::new();

    {
        let mut iter = src.box_iter();
        while let Some(mut b) = iter.next_box()? {
            let description = match track.track_type {
                TrackType::Video => read_video_sample_entry(&mut b),
                TrackType::Audio => read_audio_sample_entry(&mut b),
                TrackType::Metadata => Err(Error::Unsupported("metadata track")),
                TrackType::Unknown => Err(Error::Unsupported("unknown track type")),
            };
            let description = match description {
                Ok(desc) => desc,
                Err(Error::Unsupported(_)) => {
                    // read_{audio,video}_desc may have returned Unsupported
                    // after partially reading the box content, so we can't
                    // simply use skip_box_content here.
                    let to_skip = b.bytes_left();
                    skip(&mut b, to_skip)?;
                    SampleEntry::Unknown
                }
                Err(e) => return Err(e),
            };
            vec_push(&mut descriptions, description)?;
            check_parser_state!(b.content);
            if descriptions.len() == description_count.to_usize() {
                break;
            }
        }
    }

    // Padding could be added in some contents.
    skip_box_remain(src)?;

    Ok(SampleDescriptionBox { descriptions })
}

fn read_sinf<T: Read>(src: &mut BMFFBox<T>) -> Result<ProtectionSchemeInfoBox> {
    let mut sinf = ProtectionSchemeInfoBox::default();

    let mut iter = src.box_iter();
    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::OriginalFormatBox => {
                let frma = read_frma(&mut b)?;
                sinf.code_name = frma;
            }
            BoxType::SchemeTypeBox => {
                sinf.scheme_type = Some(read_schm(&mut b)?);
            }
            BoxType::SchemeInformationBox => {
                // We only need tenc box in schi box so far.
                sinf.tenc = read_schi(&mut b)?;
            }
            _ => skip_box_content(&mut b)?,
        }
        check_parser_state!(b.content);
    }

    Ok(sinf)
}

fn read_schi<T: Read>(src: &mut BMFFBox<T>) -> Result<Option<TrackEncryptionBox>> {
    let mut tenc = None;
    let mut iter = src.box_iter();
    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::TrackEncryptionBox => {
                if tenc.is_some() {
                    return Err(Error::InvalidData(
                        "tenc box should be only one at most in sinf box",
                    ));
                }
                tenc = Some(read_tenc(&mut b)?);
            }
            _ => skip_box_content(&mut b)?,
        }
    }

    Ok(tenc)
}

fn read_tenc<T: Read>(src: &mut BMFFBox<T>) -> Result<TrackEncryptionBox> {
    let (version, _) = read_fullbox_extra(src)?;

    // reserved byte
    skip(src, 1)?;
    // the next byte is used to signal the default pattern in version >= 1
    let (default_crypt_byte_block, default_skip_byte_block) = match version {
        0 => {
            skip(src, 1)?;
            (None, None)
        }
        _ => {
            let pattern_byte = src.read_u8()?;
            let crypt_bytes = pattern_byte >> 4;
            let skip_bytes = pattern_byte & 0x0f;
            (Some(crypt_bytes), Some(skip_bytes))
        }
    };
    let default_is_encrypted = src.read_u8()?;
    let default_iv_size = src.read_u8()?;
    let default_kid = read_buf(src, 16)?;
    // If default_is_encrypted == 1 && default_iv_size == 0 we expect a default_constant_iv
    let default_constant_iv = match (default_is_encrypted, default_iv_size) {
        (1, 0) => {
            let default_constant_iv_size = src.read_u8()?;
            Some(read_buf(src, default_constant_iv_size.into())?)
        }
        _ => None,
    };

    Ok(TrackEncryptionBox {
        is_encrypted: default_is_encrypted,
        iv_size: default_iv_size,
        kid: default_kid,
        crypt_byte_block_count: default_crypt_byte_block,
        skip_byte_block_count: default_skip_byte_block,
        constant_iv: default_constant_iv,
    })
}

fn read_frma<T: Read>(src: &mut BMFFBox<T>) -> Result<String> {
    let code_name = read_buf(src, 4)?;
    String::from_utf8(code_name).map_err(From::from)
}

fn read_schm<T: Read>(src: &mut BMFFBox<T>) -> Result<SchemeTypeBox> {
    // Flags can be used to signal presence of URI in the box, but we don't
    // use the URI so don't bother storing the flags.
    let (_, _) = read_fullbox_extra(src)?;
    let scheme_type = FourCC::from(be_u32(src)?);
    let scheme_version = be_u32(src)?;
    // Null terminated scheme URI may follow, but we don't use it right now.
    skip_box_remain(src)?;
    Ok(SchemeTypeBox {
        scheme_type,
        scheme_version,
    })
}

/// Parse a metadata box inside a moov, trak, or mdia box.
fn read_udta<T: Read>(src: &mut BMFFBox<T>) -> Result<UserdataBox> {
    let mut iter = src.box_iter();
    let mut udta = UserdataBox { meta: None };

    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::MetadataBox => {
                let meta = read_meta(&mut b)?;
                udta.meta = Some(meta);
            }
            _ => skip_box_content(&mut b)?,
        };
        check_parser_state!(b.content);
    }
    Ok(udta)
}

/// Parse a metadata box inside a udta box
fn read_meta<T: Read>(src: &mut BMFFBox<T>) -> Result<MetadataBox> {
    let (_, _) = read_fullbox_extra(src)?;
    let mut iter = src.box_iter();
    let mut meta = MetadataBox::default();
    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::MetadataItemListEntry => read_ilst(&mut b, &mut meta)?,
            _ => skip_box_content(&mut b)?,
        };
        check_parser_state!(b.content);
    }
    Ok(meta)
}

/// Parse a metadata box inside a udta box
fn read_ilst<T: Read>(src: &mut BMFFBox<T>, meta: &mut MetadataBox) -> Result<()> {
    let mut iter = src.box_iter();
    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::AlbumEntry => meta.album = read_ilst_string_data(&mut b)?,
            BoxType::ArtistEntry | BoxType::ArtistLowercaseEntry => {
                meta.artist = read_ilst_string_data(&mut b)?
            }
            BoxType::AlbumArtistEntry => meta.album_artist = read_ilst_string_data(&mut b)?,
            BoxType::CommentEntry => meta.comment = read_ilst_string_data(&mut b)?,
            BoxType::DateEntry => meta.year = read_ilst_string_data(&mut b)?,
            BoxType::TitleEntry => meta.title = read_ilst_string_data(&mut b)?,
            BoxType::CustomGenreEntry => {
                meta.genre = read_ilst_string_data(&mut b)?.map(Genre::CustomGenre)
            }
            BoxType::StandardGenreEntry => {
                meta.genre = read_ilst_u8_data(&mut b)?
                    .and_then(|gnre| Some(Genre::StandardGenre(gnre.get(1).copied()?)))
            }
            BoxType::ComposerEntry => meta.composer = read_ilst_string_data(&mut b)?,
            BoxType::EncoderEntry => meta.encoder = read_ilst_string_data(&mut b)?,
            BoxType::EncodedByEntry => meta.encoded_by = read_ilst_string_data(&mut b)?,
            BoxType::CopyrightEntry => meta.copyright = read_ilst_string_data(&mut b)?,
            BoxType::GroupingEntry => meta.grouping = read_ilst_string_data(&mut b)?,
            BoxType::CategoryEntry => meta.category = read_ilst_string_data(&mut b)?,
            BoxType::KeywordEntry => meta.keyword = read_ilst_string_data(&mut b)?,
            BoxType::PodcastUrlEntry => meta.podcast_url = read_ilst_string_data(&mut b)?,
            BoxType::PodcastGuidEntry => meta.podcast_guid = read_ilst_string_data(&mut b)?,
            BoxType::DescriptionEntry => meta.description = read_ilst_string_data(&mut b)?,
            BoxType::LongDescriptionEntry => meta.long_description = read_ilst_string_data(&mut b)?,
            BoxType::LyricsEntry => meta.lyrics = read_ilst_string_data(&mut b)?,
            BoxType::TVNetworkNameEntry => meta.tv_network_name = read_ilst_string_data(&mut b)?,
            BoxType::TVEpisodeNameEntry => meta.tv_episode_name = read_ilst_string_data(&mut b)?,
            BoxType::TVShowNameEntry => meta.tv_show_name = read_ilst_string_data(&mut b)?,
            BoxType::PurchaseDateEntry => meta.purchase_date = read_ilst_string_data(&mut b)?,
            BoxType::RatingEntry => meta.rating = read_ilst_string_data(&mut b)?,
            BoxType::OwnerEntry => meta.owner = read_ilst_string_data(&mut b)?,
            BoxType::HDVideoEntry => meta.hd_video = read_ilst_bool_data(&mut b)?,
            BoxType::SortNameEntry => meta.sort_name = read_ilst_string_data(&mut b)?,
            BoxType::SortArtistEntry => meta.sort_artist = read_ilst_string_data(&mut b)?,
            BoxType::SortAlbumEntry => meta.sort_album = read_ilst_string_data(&mut b)?,
            BoxType::SortAlbumArtistEntry => {
                meta.sort_album_artist = read_ilst_string_data(&mut b)?
            }
            BoxType::SortComposerEntry => meta.sort_composer = read_ilst_string_data(&mut b)?,
            BoxType::TrackNumberEntry => {
                if let Some(trkn) = read_ilst_u8_data(&mut b)? {
                    meta.track_number = trkn.get(3).copied();
                    meta.total_tracks = trkn.get(5).copied();
                };
            }
            BoxType::DiskNumberEntry => {
                if let Some(disk) = read_ilst_u8_data(&mut b)? {
                    meta.disc_number = disk.get(3).copied();
                    meta.total_discs = disk.get(5).copied();
                };
            }
            BoxType::TempoEntry => {
                meta.beats_per_minute =
                    read_ilst_u8_data(&mut b)?.and_then(|tmpo| tmpo.get(1).copied())
            }
            BoxType::CompilationEntry => meta.compilation = read_ilst_bool_data(&mut b)?,
            BoxType::AdvisoryEntry => {
                meta.advisory = read_ilst_u8_data(&mut b)?.and_then(|rtng| {
                    Some(match rtng.get(0)? {
                        2 => AdvisoryRating::Clean,
                        0 => AdvisoryRating::Inoffensive,
                        r => AdvisoryRating::Explicit(*r),
                    })
                })
            }
            BoxType::MediaTypeEntry => {
                meta.media_type = read_ilst_u8_data(&mut b)?.and_then(|stik| {
                    Some(match stik.get(0)? {
                        0 => MediaType::Movie,
                        1 => MediaType::Normal,
                        2 => MediaType::AudioBook,
                        5 => MediaType::WhackedBookmark,
                        6 => MediaType::MusicVideo,
                        9 => MediaType::ShortFilm,
                        10 => MediaType::TVShow,
                        11 => MediaType::Booklet,
                        s => MediaType::Unknown(*s),
                    })
                })
            }
            BoxType::PodcastEntry => meta.podcast = read_ilst_bool_data(&mut b)?,
            BoxType::TVSeasonNumberEntry => {
                meta.tv_season = read_ilst_u8_data(&mut b)?.and_then(|tvsn| tvsn.get(3).copied())
            }
            BoxType::TVEpisodeNumberEntry => {
                meta.tv_episode_number =
                    read_ilst_u8_data(&mut b)?.and_then(|tves| tves.get(3).copied())
            }
            BoxType::GaplessPlaybackEntry => meta.gapless_playback = read_ilst_bool_data(&mut b)?,
            BoxType::CoverArtEntry => meta.cover_art = read_ilst_multiple_u8_data(&mut b).ok(),
            _ => skip_box_content(&mut b)?,
        };
        check_parser_state!(b.content);
    }
    Ok(())
}

fn read_ilst_bool_data<T: Read>(src: &mut BMFFBox<T>) -> Result<Option<bool>> {
    Ok(read_ilst_u8_data(src)?.and_then(|d| Some(d.get(0)? == &1)))
}

fn read_ilst_string_data<T: Read>(src: &mut BMFFBox<T>) -> Result<Option<String>> {
    read_ilst_u8_data(src)?.map_or(Ok(None), |d| {
        String::from_utf8(d).map_err(From::from).map(Some)
    })
}

fn read_ilst_u8_data<T: Read>(src: &mut BMFFBox<T>) -> Result<Option<Vec<u8>>> {
    // For all non-covr atoms, there must only be one data atom.
    Ok(read_ilst_multiple_u8_data(src)?.pop())
}

fn read_ilst_multiple_u8_data<T: Read>(src: &mut BMFFBox<T>) -> Result<Vec<Vec<u8>>> {
    let mut iter = src.box_iter();
    let mut data = Vec::new();
    while let Some(mut b) = iter.next_box()? {
        match b.head.name {
            BoxType::MetadataItemDataEntry => {
                vec_push(&mut data, read_ilst_data(&mut b)?)?;
            }
            _ => skip_box_content(&mut b)?,
        };
        check_parser_state!(b.content);
    }
    Ok(data)
}

fn read_ilst_data<T: Read>(src: &mut BMFFBox<T>) -> Result<Vec<u8>> {
    // Skip past the padding bytes
    skip(&mut src.content, src.head.offset)?;
    let size = src.content.limit();
    read_buf(&mut src.content, size)
}

/// Skip a number of bytes that we don't care to parse.
fn skip<T: Read>(src: &mut T, bytes: u64) -> Result<()> {
    std::io::copy(&mut src.take(bytes), &mut std::io::sink())?;
    Ok(())
}

/// Read size bytes into a Vector or return error.
fn read_buf<T: Read>(src: &mut T, size: u64) -> Result<Vec<u8>> {
    if size > BUF_SIZE_LIMIT {
        return Err(Error::InvalidData("read_buf size exceeds BUF_SIZE_LIMIT"));
    }

    let mut buf = vec![];
    let r: u64 = read_to_end(&mut src.take(size), &mut buf)?.try_into()?;
    if r != size {
        return Err(Error::InvalidData("failed buffer read"));
    }

    Ok(buf)
}

fn be_i16<T: ReadBytesExt>(src: &mut T) -> Result<i16> {
    src.read_i16::<byteorder::BigEndian>().map_err(From::from)
}

fn be_i32<T: ReadBytesExt>(src: &mut T) -> Result<i32> {
    src.read_i32::<byteorder::BigEndian>().map_err(From::from)
}

fn be_i64<T: ReadBytesExt>(src: &mut T) -> Result<i64> {
    src.read_i64::<byteorder::BigEndian>().map_err(From::from)
}

fn be_u16<T: ReadBytesExt>(src: &mut T) -> Result<u16> {
    src.read_u16::<byteorder::BigEndian>().map_err(From::from)
}

fn be_u24<T: ReadBytesExt>(src: &mut T) -> Result<u32> {
    src.read_u24::<byteorder::BigEndian>().map_err(From::from)
}

fn be_u32<T: ReadBytesExt>(src: &mut T) -> Result<u32> {
    src.read_u32::<byteorder::BigEndian>().map_err(From::from)
}

/// Using in reading table size and return error if it exceeds limitation.
fn be_u32_with_limit<T: ReadBytesExt>(src: &mut T) -> Result<u32> {
    be_u32(src).and_then(|v| {
        if v > TABLE_SIZE_LIMIT {
            return Err(Error::OutOfMemory);
        }
        Ok(v)
    })
}

fn be_u64<T: ReadBytesExt>(src: &mut T) -> Result<u64> {
    src.read_u64::<byteorder::BigEndian>().map_err(From::from)
}

fn write_be_u32<T: WriteBytesExt>(des: &mut T, num: u32) -> Result<()> {
    des.write_u32::<byteorder::BigEndian>(num)
        .map_err(From::from)
}
