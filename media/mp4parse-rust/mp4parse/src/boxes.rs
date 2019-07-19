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
    pub value: String
}

impl From<u32> for FourCC {
    fn from(number: u32) -> FourCC {
        let mut box_chars = Vec::new();
        for x in 0..4 {
            let c = (number >> (x * 8) & 0x000000FF) as u8;
            box_chars.push(c);
        }
        box_chars.reverse();

        let box_string = match String::from_utf8(box_chars) {
            Ok(t) => t,
            _ => String::from("null"), // error to retrieve fourcc
        };

        FourCC {
            value: box_string
        }
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
            value: v.to_owned()
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
    FileTypeBox                       0x66747970, // "ftyp"
    MovieBox                          0x6d6f6f76, // "moov"
    MovieHeaderBox                    0x6d766864, // "mvhd"
    TrackBox                          0x7472616b, // "trak"
    TrackHeaderBox                    0x746b6864, // "tkhd"
    EditBox                           0x65647473, // "edts"
    MediaBox                          0x6d646961, // "mdia"
    EditListBox                       0x656c7374, // "elst"
    MediaHeaderBox                    0x6d646864, // "mdhd"
    HandlerBox                        0x68646c72, // "hdlr"
    MediaInformationBox               0x6d696e66, // "minf"
    SampleTableBox                    0x7374626c, // "stbl"
    SampleDescriptionBox              0x73747364, // "stsd"
    TimeToSampleBox                   0x73747473, // "stts"
    SampleToChunkBox                  0x73747363, // "stsc"
    SampleSizeBox                     0x7374737a, // "stsz"
    ChunkOffsetBox                    0x7374636f, // "stco"
    ChunkLargeOffsetBox               0x636f3634, // "co64"
    SyncSampleBox                     0x73747373, // "stss"
    AVCSampleEntry                    0x61766331, // "avc1"
    AVC3SampleEntry                   0x61766333, // "avc3" - Need to check official name in spec.
    AVCConfigurationBox               0x61766343, // "avcC"
    MP4AudioSampleEntry               0x6d703461, // "mp4a"
    MP4VideoSampleEntry               0x6d703476, // "mp4v"
    ESDBox                            0x65736473, // "esds"
    VP8SampleEntry                    0x76703038, // "vp08"
    VP9SampleEntry                    0x76703039, // "vp09"
    VPCodecConfigurationBox           0x76706343, // "vpcC"
    AV1SampleEntry                    0x61763031, // "av01"
    AV1CodecConfigurationBox          0x61763143, // "av1C"
    FLACSampleEntry                   0x664c6143, // "fLaC"
    FLACSpecificBox                   0x64664c61, // "dfLa"
    OpusSampleEntry                   0x4f707573, // "Opus"
    OpusSpecificBox                   0x644f7073, // "dOps"
    ProtectedVisualSampleEntry        0x656e6376, // "encv" - Need to check official name in spec.
    ProtectedAudioSampleEntry         0x656e6361, // "enca" - Need to check official name in spec.
    MovieExtendsBox                   0x6d766578, // "mvex"
    MovieExtendsHeaderBox             0x6d656864, // "mehd"
    QTWaveAtom                        0x77617665, // "wave" - quicktime atom
    ProtectionSystemSpecificHeaderBox 0x70737368, // "pssh"
    SchemeInformationBox              0x73636869, // "schi"
    TrackEncryptionBox                0x74656e63, // "tenc"
    ProtectionSchemeInformationBox    0x73696e66, // "sinf"
    OriginalFormatBox                 0x66726d61, // "frma"
    SchemeTypeBox                     0x7363686d, // "schm"
    MP3AudioSampleEntry               0x2e6d7033, // ".mp3" - from F4V.
    CompositionOffsetBox              0x63747473, // "ctts"
    LPCMAudioSampleEntry              0x6C70636D, // "lpcm" - quicktime atom
    ALACSpecificBox                   0x616C6163, // "alac" - Also used by ALACSampleEntry
    UuidBox                           0x75756964, // "uuid"
    MetadataBox                       0x6d657461, // "meta"
    MetadataHeaderBox                 0x6d686472, // "mhdr"
    MetadataItemKeysBox               0x6b657973, // "keys"
    MetadataItemListEntry             0x696c7374, // "ilst"
    MetadataItemDataEntry             0x64617461, // "data"
    MetadataItemNameBox               0x6e616d65, // "name"
    MetadataItemInformationBox        0x69746966, // "itif"
    UserdataBox                       0x75647461, // "udta"
    AlbumEntry                        0xa9616c62, // "©alb"
    ArtistEntry                       0xa9415254, // "©ART"
    ArtistLowercaseEntry              0xa9617274, // "©art"
    AlbumArtistEntry                  0x61415254, // "aART"
    CommentEntry                      0xa9636d74, // "©cmt"
    DateEntry                         0xa9646179, // "©day"
    TitleEntry                        0xa96e616d, // "©nam"
    CustomGenreEntry                  0xa967656e, // "©gen"
    StandardGenreEntry                0x676e7265, // "gnre"
    TrackNumberEntry                  0x74726b6e, // "trkn"
    DiskNumberEntry                   0x6469736b, // "disk"
    ComposerEntry                     0xa9777274, // "©wrt"
    EncoderEntry                      0xa9746f6f, // "©too"
    EncodedByEntry                    0xa9656e63, // "©enc"
    TempoEntry                        0x746d706f, // "tmpo"
    CopyrightEntry                    0x63707274, // "cprt"
    CompilationEntry                  0x6370696c, // "cpil"
    CoverArtEntry                     0x636f7672, // "covr"
    AdvisoryEntry                     0x72746e67, // "rtng"
    RatingEntry                       0x72617465, // "rate"
    GroupingEntry                     0xa9677270, // "©grp"
    MediaTypeEntry                    0x7374696b, // "stik"
    PodcastEntry                      0x70637374, // "pcst"
    CategoryEntry                     0x63617467, // "catg"
    KeywordEntry                      0x6b657977, // "keyw"
    PodcastUrlEntry                   0x7075726c, // "purl"
    PodcastGuidEntry                  0x65676964, // "egid"
    DescriptionEntry                  0x64657363, // "desc"
    LongDescriptionEntry              0x6c646573, // "ldes"
    LyricsEntry                       0xa96c7972, // "©lyr"
    TVNetworkNameEntry                0x74766e6e, // "tvnn"
    TVShowNameEntry                   0x74767368, // "tvsh"
    TVEpisodeNameEntry                0x7476656e, // "tven"
    TVSeasonNumberEntry               0x7476736e, // "tvsn"
    TVEpisodeNumberEntry              0x74766573, // "tves"
    PurchaseDateEntry                 0x70757264, // "purd"
    GaplessPlaybackEntry              0x70676170, // "pgap"
    OwnerEntry                        0x6f776e72, // "ownr"
    HDVideoEntry                      0x68647664, // "hdvd"
    SortNameEntry                     0x736f6e6d, // "sonm"
    SortAlbumEntry                    0x736f616c, // "soal"
    SortArtistEntry                   0x736f6172, // "soar"
    SortAlbumArtistEntry              0x736f6161, // "soaa"
    SortComposerEntry                 0x736f636f, // "soco"
);
