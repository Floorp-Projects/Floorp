// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
use std::fmt;

macro_rules! box_database {
    ($($boxenum:ident $boxtype:expr),*,) => {
        #[derive(Clone, Copy, PartialEq)]
        pub enum BoxType {
            $($boxenum),*,
            UnknownBox(u32),
        }

        impl From<u32> for BoxType {
            fn from(t: u32) -> BoxType {
                use self::BoxType::*;
                match t {
                    $($boxtype => $boxenum),*,
                    _ => UnknownBox(t),
                }
            }
        }

        impl Into<u32> for BoxType {
            fn into(self) -> u32 {
                use self::BoxType::*;
                match self {
                    $($boxenum => $boxtype),*,
                    UnknownBox(t) => t,
                }
            }
        }

        impl fmt::Debug for BoxType {
            fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
                let fourcc: FourCC = From::from(self.clone());
                write!(f, "{}", fourcc)
            }
        }
    }
}

#[derive(Default, PartialEq, Clone)]
pub struct FourCC {
    pub value: String,
}

impl From<u32> for FourCC {
    fn from(number: u32) -> FourCC {
        let mut box_chars = Vec::new();
        for x in 0..4 {
            let c = (number >> (x * 8) & 0x0000_00FF) as u8;
            box_chars.push(c);
        }
        box_chars.reverse();

        let box_string = match String::from_utf8(box_chars) {
            Ok(t) => t,
            _ => String::from("null"), // error to retrieve fourcc
        };

        FourCC { value: box_string }
    }
}

impl From<BoxType> for FourCC {
    fn from(t: BoxType) -> FourCC {
        let box_num: u32 = Into::into(t);
        From::from(box_num)
    }
}

impl<'a> From<&'a str> for FourCC {
    fn from(v: &'a str) -> FourCC {
        FourCC {
            value: v.to_owned(),
        }
    }
}

impl fmt::Debug for FourCC {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.value)
    }
}

impl fmt::Display for FourCC {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.value)
    }
}

box_database!(
    FileTypeBox                       0x6674_7970, // "ftyp"
    MediaDataBox                      0x6d64_6174, // "mdat"
    PrimaryItemBox                    0x7069_746d, // "pitm"
    ItemInfoBox                       0x6969_6e66, // "iinf"
    ItemInfoEntry                     0x696e_6665, // "infe"
    ItemLocationBox                   0x696c_6f63, // "iloc"
    MovieBox                          0x6d6f_6f76, // "moov"
    MovieHeaderBox                    0x6d76_6864, // "mvhd"
    TrackBox                          0x7472_616b, // "trak"
    TrackHeaderBox                    0x746b_6864, // "tkhd"
    EditBox                           0x6564_7473, // "edts"
    MediaBox                          0x6d64_6961, // "mdia"
    EditListBox                       0x656c_7374, // "elst"
    MediaHeaderBox                    0x6d64_6864, // "mdhd"
    HandlerBox                        0x6864_6c72, // "hdlr"
    MediaInformationBox               0x6d69_6e66, // "minf"
    SampleTableBox                    0x7374_626c, // "stbl"
    SampleDescriptionBox              0x7374_7364, // "stsd"
    TimeToSampleBox                   0x7374_7473, // "stts"
    SampleToChunkBox                  0x7374_7363, // "stsc"
    SampleSizeBox                     0x7374_737a, // "stsz"
    ChunkOffsetBox                    0x7374_636f, // "stco"
    ChunkLargeOffsetBox               0x636f_3634, // "co64"
    SyncSampleBox                     0x7374_7373, // "stss"
    AVCSampleEntry                    0x6176_6331, // "avc1"
    AVC3SampleEntry                   0x6176_6333, // "avc3" - Need to check official name in spec.
    AVCConfigurationBox               0x6176_6343, // "avcC"
    MP4AudioSampleEntry               0x6d70_3461, // "mp4a"
    MP4VideoSampleEntry               0x6d70_3476, // "mp4v"
    ESDBox                            0x6573_6473, // "esds"
    VP8SampleEntry                    0x7670_3038, // "vp08"
    VP9SampleEntry                    0x7670_3039, // "vp09"
    VPCodecConfigurationBox           0x7670_6343, // "vpcC"
    AV1SampleEntry                    0x6176_3031, // "av01"
    AV1CodecConfigurationBox          0x6176_3143, // "av1C"
    FLACSampleEntry                   0x664c_6143, // "fLaC"
    FLACSpecificBox                   0x6466_4c61, // "dfLa"
    OpusSampleEntry                   0x4f70_7573, // "Opus"
    OpusSpecificBox                   0x644f_7073, // "dOps"
    ProtectedVisualSampleEntry        0x656e_6376, // "encv" - Need to check official name in spec.
    ProtectedAudioSampleEntry         0x656e_6361, // "enca" - Need to check official name in spec.
    MovieExtendsBox                   0x6d76_6578, // "mvex"
    MovieExtendsHeaderBox             0x6d65_6864, // "mehd"
    QTWaveAtom                        0x7761_7665, // "wave" - quicktime atom
    ProtectionSystemSpecificHeaderBox 0x7073_7368, // "pssh"
    SchemeInformationBox              0x7363_6869, // "schi"
    TrackEncryptionBox                0x7465_6e63, // "tenc"
    ProtectionSchemeInfoBox           0x7369_6e66, // "sinf"
    OriginalFormatBox                 0x6672_6d61, // "frma"
    SchemeTypeBox                     0x7363_686d, // "schm"
    MP3AudioSampleEntry               0x2e6d_7033, // ".mp3" - from F4V.
    CompositionOffsetBox              0x6374_7473, // "ctts"
    LPCMAudioSampleEntry              0x6C70_636D, // "lpcm" - quicktime atom
    ALACSpecificBox                   0x616C_6163, // "alac" - Also used by ALACSampleEntry
    UuidBox                           0x7575_6964, // "uuid"
    MetadataBox                       0x6d65_7461, // "meta"
    MetadataHeaderBox                 0x6d68_6472, // "mhdr"
    MetadataItemKeysBox               0x6b65_7973, // "keys"
    MetadataItemListEntry             0x696c_7374, // "ilst"
    MetadataItemDataEntry             0x6461_7461, // "data"
    MetadataItemNameBox               0x6e61_6d65, // "name"
    UserdataBox                       0x7564_7461, // "udta"
    AlbumEntry                        0xa961_6c62, // "©alb"
    ArtistEntry                       0xa941_5254, // "©ART"
    ArtistLowercaseEntry              0xa961_7274, // "©art"
    AlbumArtistEntry                  0x6141_5254, // "aART"
    CommentEntry                      0xa963_6d74, // "©cmt"
    DateEntry                         0xa964_6179, // "©day"
    TitleEntry                        0xa96e_616d, // "©nam"
    CustomGenreEntry                  0xa967_656e, // "©gen"
    StandardGenreEntry                0x676e_7265, // "gnre"
    TrackNumberEntry                  0x7472_6b6e, // "trkn"
    DiskNumberEntry                   0x6469_736b, // "disk"
    ComposerEntry                     0xa977_7274, // "©wrt"
    EncoderEntry                      0xa974_6f6f, // "©too"
    EncodedByEntry                    0xa965_6e63, // "©enc"
    TempoEntry                        0x746d_706f, // "tmpo"
    CopyrightEntry                    0x6370_7274, // "cprt"
    CompilationEntry                  0x6370_696c, // "cpil"
    CoverArtEntry                     0x636f_7672, // "covr"
    AdvisoryEntry                     0x7274_6e67, // "rtng"
    RatingEntry                       0x7261_7465, // "rate"
    GroupingEntry                     0xa967_7270, // "©grp"
    MediaTypeEntry                    0x7374_696b, // "stik"
    PodcastEntry                      0x7063_7374, // "pcst"
    CategoryEntry                     0x6361_7467, // "catg"
    KeywordEntry                      0x6b65_7977, // "keyw"
    PodcastUrlEntry                   0x7075_726c, // "purl"
    PodcastGuidEntry                  0x6567_6964, // "egid"
    DescriptionEntry                  0x6465_7363, // "desc"
    LongDescriptionEntry              0x6c64_6573, // "ldes"
    LyricsEntry                       0xa96c_7972, // "©lyr"
    TVNetworkNameEntry                0x7476_6e6e, // "tvnn"
    TVShowNameEntry                   0x7476_7368, // "tvsh"
    TVEpisodeNameEntry                0x7476_656e, // "tven"
    TVSeasonNumberEntry               0x7476_736e, // "tvsn"
    TVEpisodeNumberEntry              0x7476_6573, // "tves"
    PurchaseDateEntry                 0x7075_7264, // "purd"
    GaplessPlaybackEntry              0x7067_6170, // "pgap"
    OwnerEntry                        0x6f77_6e72, // "ownr"
    HDVideoEntry                      0x6864_7664, // "hdvd"
    SortNameEntry                     0x736f_6e6d, // "sonm"
    SortAlbumEntry                    0x736f_616c, // "soal"
    SortArtistEntry                   0x736f_6172, // "soar"
    SortAlbumArtistEntry              0x736f_6161, // "soaa"
    SortComposerEntry                 0x736f_636f, // "soco"
);
