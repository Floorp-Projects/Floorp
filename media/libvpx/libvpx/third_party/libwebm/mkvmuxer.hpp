// Copyright (c) 2012 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef MKVMUXER_HPP
#define MKVMUXER_HPP

#include "mkvmuxertypes.hpp"

// For a description of the WebM elements see
// http://www.webmproject.org/code/specs/container/.

namespace mkvparser {
class IMkvReader;
}  // end namespace

namespace mkvmuxer {

class MkvWriter;
class Segment;

///////////////////////////////////////////////////////////////
// Interface used by the mkvmuxer to write out the Mkv data.
class IMkvWriter {
 public:
  // Writes out |len| bytes of |buf|. Returns 0 on success.
  virtual int32 Write(const void* buf, uint32 len) = 0;

  // Returns the offset of the output position from the beginning of the
  // output.
  virtual int64 Position() const = 0;

  // Set the current File position. Returns 0 on success.
  virtual int32 Position(int64 position) = 0;

  // Returns true if the writer is seekable.
  virtual bool Seekable() const = 0;

  // Element start notification. Called whenever an element identifier is about
  // to be written to the stream. |element_id| is the element identifier, and
  // |position| is the location in the WebM stream where the first octet of the
  // element identifier will be written.
  // Note: the |MkvId| enumeration in webmids.hpp defines element values.
  virtual void ElementStartNotify(uint64 element_id, int64 position) = 0;

 protected:
  IMkvWriter();
  virtual ~IMkvWriter();

 private:
  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(IMkvWriter);
};

// Writes out the EBML header for a WebM file. This function must be called
// before any other libwebm writing functions are called.
bool WriteEbmlHeader(IMkvWriter* writer);

// Copies in Chunk from source to destination between the given byte positions
bool ChunkedCopy(mkvparser::IMkvReader* source, IMkvWriter* dst, int64 start,
                 int64 size);

///////////////////////////////////////////////////////////////
// Class to hold data the will be written to a block.
class Frame {
 public:
  Frame();
  ~Frame();

  // Copies |frame| data into |frame_|. Returns true on success.
  bool Init(const uint8* frame, uint64 length);

  // Copies |additional| data into |additional_|. Returns true on success.
  bool AddAdditionalData(const uint8* additional, uint64 length, uint64 add_id);

  uint64 add_id() const { return add_id_; }
  const uint8* additional() const { return additional_; }
  uint64 additional_length() const { return additional_length_; }
  void set_duration(uint64 duration) { duration_ = duration; }
  uint64 duration() const { return duration_; }
  const uint8* frame() const { return frame_; }
  void set_is_key(bool key) { is_key_ = key; }
  bool is_key() const { return is_key_; }
  uint64 length() const { return length_; }
  void set_track_number(uint64 track_number) { track_number_ = track_number; }
  uint64 track_number() const { return track_number_; }
  void set_timestamp(uint64 timestamp) { timestamp_ = timestamp; }
  uint64 timestamp() const { return timestamp_; }
  void set_discard_padding(uint64 discard_padding) {
    discard_padding_ = discard_padding;
  }
  uint64 discard_padding() const { return discard_padding_; }

 private:
  // Id of the Additional data.
  uint64 add_id_;

  // Pointer to additional data. Owned by this class.
  uint8* additional_;

  // Length of the additional data.
  uint64 additional_length_;

  // Duration of the frame in nanoseconds.
  uint64 duration_;

  // Pointer to the data. Owned by this class.
  uint8* frame_;

  // Flag telling if the data should set the key flag of a block.
  bool is_key_;

  // Length of the data.
  uint64 length_;

  // Mkv track number the data is associated with.
  uint64 track_number_;

  // Timestamp of the data in nanoseconds.
  uint64 timestamp_;

  // Discard padding for the frame.
  int64 discard_padding_;
};

///////////////////////////////////////////////////////////////
// Class to hold one cue point in a Cues element.
class CuePoint {
 public:
  CuePoint();
  ~CuePoint();

  // Returns the size in bytes for the entire CuePoint element.
  uint64 Size() const;

  // Output the CuePoint element to the writer. Returns true on success.
  bool Write(IMkvWriter* writer) const;

  void set_time(uint64 time) { time_ = time; }
  uint64 time() const { return time_; }
  void set_track(uint64 track) { track_ = track; }
  uint64 track() const { return track_; }
  void set_cluster_pos(uint64 cluster_pos) { cluster_pos_ = cluster_pos; }
  uint64 cluster_pos() const { return cluster_pos_; }
  void set_block_number(uint64 block_number) { block_number_ = block_number; }
  uint64 block_number() const { return block_number_; }
  void set_output_block_number(bool output_block_number) {
    output_block_number_ = output_block_number;
  }
  bool output_block_number() const { return output_block_number_; }

 private:
  // Returns the size in bytes for the payload of the CuePoint element.
  uint64 PayloadSize() const;

  // Absolute timecode according to the segment time base.
  uint64 time_;

  // The Track element associated with the CuePoint.
  uint64 track_;

  // The position of the Cluster containing the Block.
  uint64 cluster_pos_;

  // Number of the Block within the Cluster, starting from 1.
  uint64 block_number_;

  // If true the muxer will write out the block number for the cue if the
  // block number is different than the default of 1. Default is set to true.
  bool output_block_number_;

  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(CuePoint);
};

///////////////////////////////////////////////////////////////
// Cues element.
class Cues {
 public:
  Cues();
  ~Cues();

  // Adds a cue point to the Cues element. Returns true on success.
  bool AddCue(CuePoint* cue);

  // Returns the cue point by index. Returns NULL if there is no cue point
  // match.
  CuePoint* GetCueByIndex(int32 index) const;

  // Returns the total size of the Cues element
  uint64 Size();

  // Output the Cues element to the writer. Returns true on success.
  bool Write(IMkvWriter* writer) const;

  int32 cue_entries_size() const { return cue_entries_size_; }
  void set_output_block_number(bool output_block_number) {
    output_block_number_ = output_block_number;
  }
  bool output_block_number() const { return output_block_number_; }

 private:
  // Number of allocated elements in |cue_entries_|.
  int32 cue_entries_capacity_;

  // Number of CuePoints in |cue_entries_|.
  int32 cue_entries_size_;

  // CuePoint list.
  CuePoint** cue_entries_;

  // If true the muxer will write out the block number for the cue if the
  // block number is different than the default of 1. Default is set to true.
  bool output_block_number_;

  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(Cues);
};

///////////////////////////////////////////////////////////////
// ContentEncAESSettings element
class ContentEncAESSettings {
 public:
  enum { kCTR = 1 };

  ContentEncAESSettings();
  ~ContentEncAESSettings() {}

  // Returns the size in bytes for the ContentEncAESSettings element.
  uint64 Size() const;

  // Writes out the ContentEncAESSettings element to |writer|. Returns true on
  // success.
  bool Write(IMkvWriter* writer) const;

  uint64 cipher_mode() const { return cipher_mode_; }

 private:
  // Returns the size in bytes for the payload of the ContentEncAESSettings
  // element.
  uint64 PayloadSize() const;

  // Sub elements
  uint64 cipher_mode_;

  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(ContentEncAESSettings);
};

///////////////////////////////////////////////////////////////
// ContentEncoding element
// Elements used to describe if the track data has been encrypted or
// compressed with zlib or header stripping.
// Currently only whole frames can be encrypted with AES. This dictates that
// ContentEncodingOrder will be 0, ContentEncodingScope will be 1,
// ContentEncodingType will be 1, and ContentEncAlgo will be 5.
class ContentEncoding {
 public:
  ContentEncoding();
  ~ContentEncoding();

  // Sets the content encryption id. Copies |length| bytes from |id| to
  // |enc_key_id_|. Returns true on success.
  bool SetEncryptionID(const uint8* id, uint64 length);

  // Returns the size in bytes for the ContentEncoding element.
  uint64 Size() const;

  // Writes out the ContentEncoding element to |writer|. Returns true on
  // success.
  bool Write(IMkvWriter* writer) const;

  uint64 enc_algo() const { return enc_algo_; }
  uint64 encoding_order() const { return encoding_order_; }
  uint64 encoding_scope() const { return encoding_scope_; }
  uint64 encoding_type() const { return encoding_type_; }
  ContentEncAESSettings* enc_aes_settings() { return &enc_aes_settings_; }

 private:
  // Returns the size in bytes for the encoding elements.
  uint64 EncodingSize(uint64 compresion_size, uint64 encryption_size) const;

  // Returns the size in bytes for the encryption elements.
  uint64 EncryptionSize() const;

  // Track element names
  uint64 enc_algo_;
  uint8* enc_key_id_;
  uint64 encoding_order_;
  uint64 encoding_scope_;
  uint64 encoding_type_;

  // ContentEncAESSettings element.
  ContentEncAESSettings enc_aes_settings_;

  // Size of the ContentEncKeyID data in bytes.
  uint64 enc_key_id_length_;

  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(ContentEncoding);
};

///////////////////////////////////////////////////////////////
// Track element.
class Track {
 public:
  // The |seed| parameter is used to synthesize a UID for the track.
  explicit Track(unsigned int* seed);
  virtual ~Track();

  // Adds a ContentEncoding element to the Track. Returns true on success.
  virtual bool AddContentEncoding();

  // Returns the ContentEncoding by index. Returns NULL if there is no
  // ContentEncoding match.
  ContentEncoding* GetContentEncodingByIndex(uint32 index) const;

  // Returns the size in bytes for the payload of the Track element.
  virtual uint64 PayloadSize() const;

  // Returns the size in bytes of the Track element.
  virtual uint64 Size() const;

  // Output the Track element to the writer. Returns true on success.
  virtual bool Write(IMkvWriter* writer) const;

  // Sets the CodecPrivate element of the Track element. Copies |length|
  // bytes from |codec_private| to |codec_private_|. Returns true on success.
  bool SetCodecPrivate(const uint8* codec_private, uint64 length);

  void set_codec_id(const char* codec_id);
  const char* codec_id() const { return codec_id_; }
  const uint8* codec_private() const { return codec_private_; }
  void set_language(const char* language);
  const char* language() const { return language_; }
  void set_max_block_additional_id(uint64 max_block_additional_id) {
    max_block_additional_id_ = max_block_additional_id;
  }
  uint64 max_block_additional_id() const { return max_block_additional_id_; }
  void set_name(const char* name);
  const char* name() const { return name_; }
  void set_number(uint64 number) { number_ = number; }
  uint64 number() const { return number_; }
  void set_type(uint64 type) { type_ = type; }
  uint64 type() const { return type_; }
  void set_uid(uint64 uid) { uid_ = uid; }
  uint64 uid() const { return uid_; }
  void set_codec_delay(uint64 codec_delay) { codec_delay_ = codec_delay; }
  uint64 codec_delay() const { return codec_delay_; }
  void set_seek_pre_roll(uint64 seek_pre_roll) {
    seek_pre_roll_ = seek_pre_roll;
  }
  uint64 seek_pre_roll() const { return seek_pre_roll_; }
  void set_default_duration(uint64 default_duration) {
    default_duration_ = default_duration;
  }
  uint64 default_duration() const { return default_duration_; }

  uint64 codec_private_length() const { return codec_private_length_; }
  uint32 content_encoding_entries_size() const {
    return content_encoding_entries_size_;
  }

 private:
  // Track element names.
  char* codec_id_;
  uint8* codec_private_;
  char* language_;
  uint64 max_block_additional_id_;
  char* name_;
  uint64 number_;
  uint64 type_;
  uint64 uid_;
  uint64 codec_delay_;
  uint64 seek_pre_roll_;
  uint64 default_duration_;

  // Size of the CodecPrivate data in bytes.
  uint64 codec_private_length_;

  // ContentEncoding element list.
  ContentEncoding** content_encoding_entries_;

  // Number of ContentEncoding elements added.
  uint32 content_encoding_entries_size_;

  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(Track);
};

///////////////////////////////////////////////////////////////
// Track that has video specific elements.
class VideoTrack : public Track {
 public:
  // Supported modes for stereo 3D.
  enum StereoMode {
    kMono = 0,
    kSideBySideLeftIsFirst = 1,
    kTopBottomRightIsFirst = 2,
    kTopBottomLeftIsFirst = 3,
    kSideBySideRightIsFirst = 11
  };

  enum AlphaMode { kNoAlpha = 0, kAlpha = 1 };

  // The |seed| parameter is used to synthesize a UID for the track.
  explicit VideoTrack(unsigned int* seed);
  virtual ~VideoTrack();

  // Returns the size in bytes for the payload of the Track element plus the
  // video specific elements.
  virtual uint64 PayloadSize() const;

  // Output the VideoTrack element to the writer. Returns true on success.
  virtual bool Write(IMkvWriter* writer) const;

  // Sets the video's stereo mode. Returns true on success.
  bool SetStereoMode(uint64 stereo_mode);

  // Sets the video's alpha mode. Returns true on success.
  bool SetAlphaMode(uint64 alpha_mode);

  void set_display_height(uint64 height) { display_height_ = height; }
  uint64 display_height() const { return display_height_; }
  void set_display_width(uint64 width) { display_width_ = width; }
  uint64 display_width() const { return display_width_; }
  void set_frame_rate(double frame_rate) { frame_rate_ = frame_rate; }
  double frame_rate() const { return frame_rate_; }
  void set_height(uint64 height) { height_ = height; }
  uint64 height() const { return height_; }
  uint64 stereo_mode() { return stereo_mode_; }
  uint64 alpha_mode() { return alpha_mode_; }
  void set_width(uint64 width) { width_ = width; }
  uint64 width() const { return width_; }

 private:
  // Returns the size in bytes of the Video element.
  uint64 VideoPayloadSize() const;

  // Video track element names.
  uint64 display_height_;
  uint64 display_width_;
  double frame_rate_;
  uint64 height_;
  uint64 stereo_mode_;
  uint64 alpha_mode_;
  uint64 width_;

  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(VideoTrack);
};

///////////////////////////////////////////////////////////////
// Track that has audio specific elements.
class AudioTrack : public Track {
 public:
  // The |seed| parameter is used to synthesize a UID for the track.
  explicit AudioTrack(unsigned int* seed);
  virtual ~AudioTrack();

  // Returns the size in bytes for the payload of the Track element plus the
  // audio specific elements.
  virtual uint64 PayloadSize() const;

  // Output the AudioTrack element to the writer. Returns true on success.
  virtual bool Write(IMkvWriter* writer) const;

  void set_bit_depth(uint64 bit_depth) { bit_depth_ = bit_depth; }
  uint64 bit_depth() const { return bit_depth_; }
  void set_channels(uint64 channels) { channels_ = channels; }
  uint64 channels() const { return channels_; }
  void set_sample_rate(double sample_rate) { sample_rate_ = sample_rate; }
  double sample_rate() const { return sample_rate_; }

 private:
  // Audio track element names.
  uint64 bit_depth_;
  uint64 channels_;
  double sample_rate_;

  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(AudioTrack);
};

///////////////////////////////////////////////////////////////
// Tracks element
class Tracks {
 public:
  // Audio and video type defined by the Matroska specs.
  enum { kVideo = 0x1, kAudio = 0x2 };
  // Opus, Vorbis, VP8, and VP9 codec ids defined by the Matroska specs.
  static const char kOpusCodecId[];
  static const char kVorbisCodecId[];
  static const char kVp8CodecId[];
  static const char kVp9CodecId[];

  Tracks();
  ~Tracks();

  // Adds a Track element to the Tracks object. |track| will be owned and
  // deleted by the Tracks object. Returns true on success. |number| is the
  // number to use for the track. |number| must be >= 0. If |number| == 0
  // then the muxer will decide on the track number.
  bool AddTrack(Track* track, int32 number);

  // Returns the track by index. Returns NULL if there is no track match.
  const Track* GetTrackByIndex(uint32 idx) const;

  // Search the Tracks and return the track that matches |tn|. Returns NULL
  // if there is no track match.
  Track* GetTrackByNumber(uint64 track_number) const;

  // Returns true if the track number is an audio track.
  bool TrackIsAudio(uint64 track_number) const;

  // Returns true if the track number is a video track.
  bool TrackIsVideo(uint64 track_number) const;

  // Output the Tracks element to the writer. Returns true on success.
  bool Write(IMkvWriter* writer) const;

  uint32 track_entries_size() const { return track_entries_size_; }

 private:
  // Track element list.
  Track** track_entries_;

  // Number of Track elements added.
  uint32 track_entries_size_;

  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(Tracks);
};

///////////////////////////////////////////////////////////////
// Chapter element
//
class Chapter {
 public:
  // Set the identifier for this chapter.  (This corresponds to the
  // Cue Identifier line in WebVTT.)
  // TODO(matthewjheaney): the actual serialization of this item in
  // MKV is pending.
  bool set_id(const char* id);

  // Converts the nanosecond start and stop times of this chapter to
  // their corresponding timecode values, and stores them that way.
  void set_time(const Segment& segment, uint64 start_time_ns,
                uint64 end_time_ns);

  // Sets the uid for this chapter. Primarily used to enable
  // deterministic output from the muxer.
  void set_uid(const uint64 uid) { uid_ = uid; }

  // Add a title string to this chapter, per the semantics described
  // here:
  //  http://www.matroska.org/technical/specs/index.html
  //
  // The title ("chapter string") is a UTF-8 string.
  //
  // The language has ISO 639-2 representation, described here:
  //  http://www.loc.gov/standards/iso639-2/englangn.html
  //  http://www.loc.gov/standards/iso639-2/php/English_list.php
  // If you specify NULL as the language value, this implies
  // English ("eng").
  //
  // The country value corresponds to the codes listed here:
  //  http://www.iana.org/domains/root/db/
  //
  // The function returns false if the string could not be allocated.
  bool add_string(const char* title, const char* language, const char* country);

 private:
  friend class Chapters;

  // For storage of chapter titles that differ by language.
  class Display {
   public:
    // Establish representation invariant for new Display object.
    void Init();

    // Reclaim resources, in anticipation of destruction.
    void Clear();

    // Copies the title to the |title_| member.  Returns false on
    // error.
    bool set_title(const char* title);

    // Copies the language to the |language_| member.  Returns false
    // on error.
    bool set_language(const char* language);

    // Copies the country to the |country_| member.  Returns false on
    // error.
    bool set_country(const char* country);

    // If |writer| is non-NULL, serialize the Display sub-element of
    // the Atom into the stream.  Returns the Display element size on
    // success, 0 if error.
    uint64 WriteDisplay(IMkvWriter* writer) const;

   private:
    char* title_;
    char* language_;
    char* country_;
  };

  Chapter();
  ~Chapter();

  // Establish the representation invariant for a newly-created
  // Chapter object.  The |seed| parameter is used to create the UID
  // for this chapter atom.
  void Init(unsigned int* seed);

  // Copies this Chapter object to a different one.  This is used when
  // expanding a plain array of Chapter objects (see Chapters).
  void ShallowCopy(Chapter* dst) const;

  // Reclaim resources used by this Chapter object, pending its
  // destruction.
  void Clear();

  // If there is no storage remaining on the |displays_| array for a
  // new display object, creates a new, longer array and copies the
  // existing Display objects to the new array.  Returns false if the
  // array cannot be expanded.
  bool ExpandDisplaysArray();

  // If |writer| is non-NULL, serialize the Atom sub-element into the
  // stream.  Returns the total size of the element on success, 0 if
  // error.
  uint64 WriteAtom(IMkvWriter* writer) const;

  // The string identifier for this chapter (corresponds to WebVTT cue
  // identifier).
  char* id_;

  // Start timecode of the chapter.
  uint64 start_timecode_;

  // Stop timecode of the chapter.
  uint64 end_timecode_;

  // The binary identifier for this chapter.
  uint64 uid_;

  // The Atom element can contain multiple Display sub-elements, as
  // the same logical title can be rendered in different languages.
  Display* displays_;

  // The physical length (total size) of the |displays_| array.
  int displays_size_;

  // The logical length (number of active elements) on the |displays_|
  // array.
  int displays_count_;

  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(Chapter);
};

///////////////////////////////////////////////////////////////
// Chapters element
//
class Chapters {
 public:
  Chapters();
  ~Chapters();

  Chapter* AddChapter(unsigned int* seed);

  // Returns the number of chapters that have been added.
  int Count() const;

  // Output the Chapters element to the writer. Returns true on success.
  bool Write(IMkvWriter* writer) const;

 private:
  // Expands the chapters_ array if there is not enough space to contain
  // another chapter object.  Returns true on success.
  bool ExpandChaptersArray();

  // If |writer| is non-NULL, serialize the Edition sub-element of the
  // Chapters element into the stream.  Returns the Edition element
  // size on success, 0 if error.
  uint64 WriteEdition(IMkvWriter* writer) const;

  // Total length of the chapters_ array.
  int chapters_size_;

  // Number of active chapters on the chapters_ array.
  int chapters_count_;

  // Array for storage of chapter objects.
  Chapter* chapters_;

  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(Chapters);
};

///////////////////////////////////////////////////////////////
// Cluster element
//
// Notes:
//  |Init| must be called before any other method in this class.
class Cluster {
 public:
  Cluster(uint64 timecode, int64 cues_pos);
  ~Cluster();

  // |timecode| is the absolute timecode of the cluster. |cues_pos| is the
  // position for the cluster within the segment that should be written in
  // the cues element.
  bool Init(IMkvWriter* ptr_writer);

  // Adds a frame to be output in the file. The frame is written out through
  // |writer_| if successful. Returns true on success.
  // Inputs:
  //   frame: Pointer to the data
  //   length: Length of the data
  //   track_number: Track to add the data to. Value returned by Add track
  //                 functions.  The range of allowed values is [1, 126].
  //   timecode:     Absolute (not relative to cluster) timestamp of the
  //                 frame, expressed in timecode units.
  //   is_key:       Flag telling whether or not this frame is a key frame.
  bool AddFrame(const uint8* frame, uint64 length, uint64 track_number,
                uint64 timecode,  // timecode units (absolute)
                bool is_key);

  // Adds a frame to be output in the file. The frame is written out through
  // |writer_| if successful. Returns true on success.
  // Inputs:
  //   frame: Pointer to the data
  //   length: Length of the data
  //   additional: Pointer to the additional data
  //   additional_length: Length of the additional data
  //   add_id: Value of BlockAddID element
  //   track_number: Track to add the data to. Value returned by Add track
  //                 functions.  The range of allowed values is [1, 126].
  //   abs_timecode: Absolute (not relative to cluster) timestamp of the
  //                 frame, expressed in timecode units.
  //   is_key:       Flag telling whether or not this frame is a key frame.
  bool AddFrameWithAdditional(const uint8* frame, uint64 length,
                              const uint8* additional, uint64 additional_length,
                              uint64 add_id, uint64 track_number,
                              uint64 abs_timecode, bool is_key);

  // Adds a frame to be output in the file. The frame is written out through
  // |writer_| if successful. Returns true on success.
  // Inputs:
  //   frame: Pointer to the data.
  //   length: Length of the data.
  //   discard_padding: DiscardPadding element value.
  //   track_number: Track to add the data to. Value returned by Add track
  //                 functions.  The range of allowed values is [1, 126].
  //   abs_timecode: Absolute (not relative to cluster) timestamp of the
  //                 frame, expressed in timecode units.
  //   is_key:       Flag telling whether or not this frame is a key frame.
  bool AddFrameWithDiscardPadding(const uint8* frame, uint64 length,
                                  int64 discard_padding, uint64 track_number,
                                  uint64 abs_timecode, bool is_key);

  // Writes a frame of metadata to the output medium; returns true on
  // success.
  // Inputs:
  //   frame: Pointer to the data
  //   length: Length of the data
  //   track_number: Track to add the data to. Value returned by Add track
  //                 functions.  The range of allowed values is [1, 126].
  //   timecode:     Absolute (not relative to cluster) timestamp of the
  //                 metadata frame, expressed in timecode units.
  //   duration:     Duration of metadata frame, in timecode units.
  //
  // The metadata frame is written as a block group, with a duration
  // sub-element but no reference time sub-elements (indicating that
  // it is considered a keyframe, per Matroska semantics).
  bool AddMetadata(const uint8* frame, uint64 length, uint64 track_number,
                   uint64 timecode, uint64 duration);

  // Increments the size of the cluster's data in bytes.
  void AddPayloadSize(uint64 size);

  // Closes the cluster so no more data can be written to it. Will update the
  // cluster's size if |writer_| is seekable. Returns true on success.
  bool Finalize();

  // Returns the size in bytes for the entire Cluster element.
  uint64 Size() const;

  int64 size_position() const { return size_position_; }
  int32 blocks_added() const { return blocks_added_; }
  uint64 payload_size() const { return payload_size_; }
  int64 position_for_cues() const { return position_for_cues_; }
  uint64 timecode() const { return timecode_; }

 private:
  //  Signature that matches either of WriteSimpleBlock or WriteMetadataBlock
  //  in the muxer utilities package.
  typedef uint64 (*WriteBlock)(IMkvWriter* writer, const uint8* data,
                               uint64 length, uint64 track_number,
                               int64 timecode, uint64 generic_arg);

  //  Signature that matches WriteBlockWithAdditional
  //  in the muxer utilities package.
  typedef uint64 (*WriteBlockAdditional)(IMkvWriter* writer, const uint8* data,
                                         uint64 length, const uint8* additional,
                                         uint64 add_id,
                                         uint64 additional_length,
                                         uint64 track_number, int64 timecode,
                                         uint64 is_key);

  //  Signature that matches WriteBlockWithDiscardPadding
  //  in the muxer utilities package.
  typedef uint64 (*WriteBlockDiscardPadding)(IMkvWriter* writer,
                                             const uint8* data, uint64 length,
                                             int64 discard_padding,
                                             uint64 track_number,
                                             int64 timecode, uint64 is_key);

  // Utility method that confirms that blocks can still be added, and that the
  // cluster header has been written. Used by |DoWriteBlock*|. Returns true
  // when successful.
  template <typename Type>
  bool PreWriteBlock(Type* write_function);

  // Utility method used by the |DoWriteBlock*| methods that handles the book
  // keeping required after each block is written.
  void PostWriteBlock(uint64 element_size);

  // To simplify things, we require that there be fewer than 127
  // tracks -- this allows us to serialize the track number value for
  // a stream using a single byte, per the Matroska encoding.
  bool IsValidTrackNumber(uint64 track_number) const;

  // Given |abs_timecode|, calculates timecode relative to most recent timecode.
  // Returns -1 on failure, or a relative timecode.
  int64 GetRelativeTimecode(int64 abs_timecode) const;

  //  Used to implement AddFrame and AddMetadata.
  bool DoWriteBlock(const uint8* frame, uint64 length, uint64 track_number,
                    uint64 absolute_timecode, uint64 generic_arg,
                    WriteBlock write_block);

  // Used to implement AddFrameWithAdditional
  bool DoWriteBlockWithAdditional(const uint8* frame, uint64 length,
                                  const uint8* additional,
                                  uint64 additional_length, uint64 add_id,
                                  uint64 track_number, uint64 absolute_timecode,
                                  uint64 generic_arg,
                                  WriteBlockAdditional write_block);

  // Used to implement AddFrameWithDiscardPadding
  bool DoWriteBlockWithDiscardPadding(const uint8* frame, uint64 length,
                                      int64 discard_padding,
                                      uint64 track_number,
                                      uint64 absolute_timecode,
                                      uint64 generic_arg,
                                      WriteBlockDiscardPadding write_block);

  // Outputs the Cluster header to |writer_|. Returns true on success.
  bool WriteClusterHeader();

  // Number of blocks added to the cluster.
  int32 blocks_added_;

  // Flag telling if the cluster has been closed.
  bool finalized_;

  // Flag telling if the cluster's header has been written.
  bool header_written_;

  // The size of the cluster elements in bytes.
  uint64 payload_size_;

  // The file position used for cue points.
  const int64 position_for_cues_;

  // The file position of the cluster's size element.
  int64 size_position_;

  // The absolute timecode of the cluster.
  const uint64 timecode_;

  // Pointer to the writer object. Not owned by this class.
  IMkvWriter* writer_;

  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(Cluster);
};

///////////////////////////////////////////////////////////////
// SeekHead element
class SeekHead {
 public:
  SeekHead();
  ~SeekHead();

  // TODO(fgalligan): Change this to reserve a certain size. Then check how
  // big the seek entry to be added is as not every seek entry will be the
  // maximum size it could be.
  // Adds a seek entry to be written out when the element is finalized. |id|
  // must be the coded mkv element id. |pos| is the file position of the
  // element. Returns true on success.
  bool AddSeekEntry(uint32 id, uint64 pos);

  // Writes out SeekHead and SeekEntry elements. Returns true on success.
  bool Finalize(IMkvWriter* writer) const;

  // Returns the id of the Seek Entry at the given index. Returns -1 if index is
  // out of range.
  uint32 GetId(int index) const;

  // Returns the position of the Seek Entry at the given index. Returns -1 if
  // index is out of range.
  uint64 GetPosition(int index) const;

  // Sets the Seek Entry id and position at given index.
  // Returns true on success.
  bool SetSeekEntry(int index, uint32 id, uint64 position);

  // Reserves space by writing out a Void element which will be updated with
  // a SeekHead element later. Returns true on success.
  bool Write(IMkvWriter* writer);

  // We are going to put a cap on the number of Seek Entries.
  const static int32 kSeekEntryCount = 5;

 private:
  // Returns the maximum size in bytes of one seek entry.
  uint64 MaxEntrySize() const;

  // Seek entry id element list.
  uint32 seek_entry_id_[kSeekEntryCount];

  // Seek entry pos element list.
  uint64 seek_entry_pos_[kSeekEntryCount];

  // The file position of SeekHead element.
  int64 start_pos_;

  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(SeekHead);
};

///////////////////////////////////////////////////////////////
// Segment Information element
class SegmentInfo {
 public:
  SegmentInfo();
  ~SegmentInfo();

  // Will update the duration if |duration_| is > 0.0. Returns true on success.
  bool Finalize(IMkvWriter* writer) const;

  // Sets |muxing_app_| and |writing_app_|.
  bool Init();

  // Output the Segment Information element to the writer. Returns true on
  // success.
  bool Write(IMkvWriter* writer);

  void set_duration(double duration) { duration_ = duration; }
  double duration() const { return duration_; }
  void set_muxing_app(const char* app);
  const char* muxing_app() const { return muxing_app_; }
  void set_timecode_scale(uint64 scale) { timecode_scale_ = scale; }
  uint64 timecode_scale() const { return timecode_scale_; }
  void set_writing_app(const char* app);
  const char* writing_app() const { return writing_app_; }
  void set_date_utc(int64 date_utc) { date_utc_ = date_utc; }
  int64 date_utc() const { return date_utc_; }

 private:
  // Segment Information element names.
  // Initially set to -1 to signify that a duration has not been set and should
  // not be written out.
  double duration_;
  // Set to libwebm-%d.%d.%d.%d, major, minor, build, revision.
  char* muxing_app_;
  uint64 timecode_scale_;
  // Initially set to libwebm-%d.%d.%d.%d, major, minor, build, revision.
  char* writing_app_;
  // LLONG_MIN when DateUTC is not set.
  int64 date_utc_;

  // The file position of the duration element.
  int64 duration_pos_;

  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(SegmentInfo);
};

///////////////////////////////////////////////////////////////
// This class represents the main segment in a WebM file. Currently only
// supports one Segment element.
//
// Notes:
//  |Init| must be called before any other method in this class.
class Segment {
 public:
  enum Mode { kLive = 0x1, kFile = 0x2 };

  enum CuesPosition {
    kAfterClusters = 0x0,  // Position Cues after Clusters - Default
    kBeforeClusters = 0x1  // Position Cues before Clusters
  };

  const static uint64 kDefaultMaxClusterDuration = 30000000000ULL;

  Segment();
  ~Segment();

  // Initializes |SegmentInfo| and returns result. Always returns false when
  // |ptr_writer| is NULL.
  bool Init(IMkvWriter* ptr_writer);

  // Adds a generic track to the segment.  Returns the newly-allocated
  // track object (which is owned by the segment) on success, NULL on
  // error. |number| is the number to use for the track.  |number|
  // must be >= 0. If |number| == 0 then the muxer will decide on the
  // track number.
  Track* AddTrack(int32 number);

  // Adds a Vorbis audio track to the segment. Returns the number of the track
  // on success, 0 on error. |number| is the number to use for the audio track.
  // |number| must be >= 0. If |number| == 0 then the muxer will decide on
  // the track number.
  uint64 AddAudioTrack(int32 sample_rate, int32 channels, int32 number);

  // Adds an empty chapter to the chapters of this segment.  Returns
  // non-NULL on success.  After adding the chapter, the caller should
  // populate its fields via the Chapter member functions.
  Chapter* AddChapter();

  // Adds a cue point to the Cues element. |timestamp| is the time in
  // nanoseconds of the cue's time. |track| is the Track of the Cue. This
  // function must be called after AddFrame to calculate the correct
  // BlockNumber for the CuePoint. Returns true on success.
  bool AddCuePoint(uint64 timestamp, uint64 track);

  // Adds a frame to be output in the file. Returns true on success.
  // Inputs:
  //   frame: Pointer to the data
  //   length: Length of the data
  //   track_number: Track to add the data to. Value returned by Add track
  //                 functions.
  //   timestamp:    Timestamp of the frame in nanoseconds from 0.
  //   is_key:       Flag telling whether or not this frame is a key frame.
  bool AddFrame(const uint8* frame, uint64 length, uint64 track_number,
                uint64 timestamp_ns, bool is_key);

  // Writes a frame of metadata to the output medium; returns true on
  // success.
  // Inputs:
  //   frame: Pointer to the data
  //   length: Length of the data
  //   track_number: Track to add the data to. Value returned by Add track
  //                 functions.
  //   timecode:     Absolute timestamp of the metadata frame, expressed
  //                 in nanosecond units.
  //   duration:     Duration of metadata frame, in nanosecond units.
  //
  // The metadata frame is written as a block group, with a duration
  // sub-element but no reference time sub-elements (indicating that
  // it is considered a keyframe, per Matroska semantics).
  bool AddMetadata(const uint8* frame, uint64 length, uint64 track_number,
                   uint64 timestamp_ns, uint64 duration_ns);

  // Writes a frame with additional data to the output medium; returns true on
  // success.
  // Inputs:
  //   frame: Pointer to the data.
  //   length: Length of the data.
  //   additional: Pointer to additional data.
  //   additional_length: Length of additional data.
  //   add_id: Additional ID which identifies the type of additional data.
  //   track_number: Track to add the data to. Value returned by Add track
  //                 functions.
  //   timestamp:    Absolute timestamp of the frame, expressed in nanosecond
  //                 units.
  //   is_key:       Flag telling whether or not this frame is a key frame.
  bool AddFrameWithAdditional(const uint8* frame, uint64 length,
                              const uint8* additional, uint64 additional_length,
                              uint64 add_id, uint64 track_number,
                              uint64 timestamp, bool is_key);

  // Writes a frame with DiscardPadding to the output medium; returns true on
  // success.
  // Inputs:
  //   frame: Pointer to the data.
  //   length: Length of the data.
  //   discard_padding: DiscardPadding element value.
  //   track_number: Track to add the data to. Value returned by Add track
  //                 functions.
  //   timestamp:    Absolute timestamp of the frame, expressed in nanosecond
  //                 units.
  //   is_key:       Flag telling whether or not this frame is a key frame.
  bool AddFrameWithDiscardPadding(const uint8* frame, uint64 length,
                                  int64 discard_padding, uint64 track_number,
                                  uint64 timestamp, bool is_key);

  // Writes a Frame to the output medium. Chooses the correct way of writing
  // the frame (Block vs SimpleBlock) based on the parameters passed.
  // Inputs:
  //   frame: frame object
  bool AddGenericFrame(const Frame* frame);

  // Adds a VP8 video track to the segment. Returns the number of the track on
  // success, 0 on error. |number| is the number to use for the video track.
  // |number| must be >= 0. If |number| == 0 then the muxer will decide on
  // the track number.
  uint64 AddVideoTrack(int32 width, int32 height, int32 number);

  // This function must be called after Finalize() if you need a copy of the
  // output with Cues written before the Clusters. It will return false if the
  // writer is not seekable of if chunking is set to true.
  // Input parameters:
  // reader - an IMkvReader object created with the same underlying file of the
  //          current writer object. Make sure to close the existing writer
  //          object before creating this so that all the data is properly
  //          flushed and available for reading.
  // writer - an IMkvWriter object pointing to a *different* file than the one
  //          pointed by the current writer object. This file will contain the
  //          Cues element before the Clusters.
  bool CopyAndMoveCuesBeforeClusters(mkvparser::IMkvReader* reader,
                                     IMkvWriter* writer);

  // Sets which track to use for the Cues element. Must have added the track
  // before calling this function. Returns true on success. |track_number| is
  // returned by the Add track functions.
  bool CuesTrack(uint64 track_number);

  // This will force the muxer to create a new Cluster when the next frame is
  // added.
  void ForceNewClusterOnNextFrame();

  // Writes out any frames that have not been written out. Finalizes the last
  // cluster. May update the size and duration of the segment. May output the
  // Cues element. May finalize the SeekHead element. Returns true on success.
  bool Finalize();

  // Returns the Cues object.
  Cues* GetCues() { return &cues_; }

  // Returns the Segment Information object.
  const SegmentInfo* GetSegmentInfo() const { return &segment_info_; }
  SegmentInfo* GetSegmentInfo() { return &segment_info_; }

  // Search the Tracks and return the track that matches |track_number|.
  // Returns NULL if there is no track match.
  Track* GetTrackByNumber(uint64 track_number) const;

  // Toggles whether to output a cues element.
  void OutputCues(bool output_cues);

  // Sets if the muxer will output files in chunks or not. |chunking| is a
  // flag telling whether or not to turn on chunking. |filename| is the base
  // filename for the chunk files. The header chunk file will be named
  // |filename|.hdr and the data chunks will be named
  // |filename|_XXXXXX.chk. Chunking implies that the muxer will be writing
  // to files so the muxer will use the default MkvWriter class to control
  // what data is written to what files. Returns true on success.
  // TODO: Should we change the IMkvWriter Interface to add Open and Close?
  // That will force the interface to be dependent on files.
  bool SetChunking(bool chunking, const char* filename);

  bool chunking() const { return chunking_; }
  uint64 cues_track() const { return cues_track_; }
  void set_max_cluster_duration(uint64 max_cluster_duration) {
    max_cluster_duration_ = max_cluster_duration;
  }
  uint64 max_cluster_duration() const { return max_cluster_duration_; }
  void set_max_cluster_size(uint64 max_cluster_size) {
    max_cluster_size_ = max_cluster_size;
  }
  uint64 max_cluster_size() const { return max_cluster_size_; }
  void set_mode(Mode mode) { mode_ = mode; }
  Mode mode() const { return mode_; }
  CuesPosition cues_position() const { return cues_position_; }
  bool output_cues() const { return output_cues_; }
  const SegmentInfo* segment_info() const { return &segment_info_; }

 private:
  // Checks if header information has been output and initialized. If not it
  // will output the Segment element and initialize the SeekHead elment and
  // Cues elements.
  bool CheckHeaderInfo();

  // Sets |name| according to how many chunks have been written. |ext| is the
  // file extension. |name| must be deleted by the calling app. Returns true
  // on success.
  bool UpdateChunkName(const char* ext, char** name) const;

  // Returns the maximum offset within the segment's payload. When chunking
  // this function is needed to determine offsets of elements within the
  // chunked files. Returns -1 on error.
  int64 MaxOffset();

  // Adds the frame to our frame array.
  bool QueueFrame(Frame* frame);

  // Output all frames that are queued. Returns -1 on error, otherwise
  // it returns the number of frames written.
  int WriteFramesAll();

  // Output all frames that are queued that have an end time that is less
  // then |timestamp|. Returns true on success and if there are no frames
  // queued.
  bool WriteFramesLessThan(uint64 timestamp);

  // Outputs the segment header, Segment Information element, SeekHead element,
  // and Tracks element to |writer_|.
  bool WriteSegmentHeader();

  // Given a frame with the specified timestamp (nanosecond units) and
  // keyframe status, determine whether a new cluster should be
  // created, before writing enqueued frames and the frame itself. The
  // function returns one of the following values:
  //  -1 = error: an out-of-order frame was detected
  //  0 = do not create a new cluster, and write frame to the existing cluster
  //  1 = create a new cluster, and write frame to that new cluster
  //  2 = create a new cluster, and re-run test
  int TestFrame(uint64 track_num, uint64 timestamp_ns, bool key) const;

  // Create a new cluster, using the earlier of the first enqueued
  // frame, or the indicated time. Returns true on success.
  bool MakeNewCluster(uint64 timestamp_ns);

  // Checks whether a new cluster needs to be created, and if so
  // creates a new cluster. Returns false if creation of a new cluster
  // was necessary but creation was not successful.
  bool DoNewClusterProcessing(uint64 track_num, uint64 timestamp_ns, bool key);

  // Adjusts Cue Point values (to place Cues before Clusters) so that they
  // reflect the correct offsets.
  void MoveCuesBeforeClusters();

  // This function recursively computes the correct cluster offsets (this is
  // done to move the Cues before Clusters). It recursively updates the change
  // in size (which indicates a change in cluster offset) until no sizes change.
  // Parameters:
  // diff - indicates the difference in size of the Cues element that needs to
  //        accounted for.
  // index - index in the list of Cues which is currently being adjusted.
  // cue_size - size of the Cues element.
  void MoveCuesBeforeClustersHelper(uint64 diff, int index, uint64* cue_size);

  // Seeds the random number generator used to make UIDs.
  unsigned int seed_;

  // WebM elements
  Cues cues_;
  SeekHead seek_head_;
  SegmentInfo segment_info_;
  Tracks tracks_;
  Chapters chapters_;

  // Number of chunks written.
  int chunk_count_;

  // Current chunk filename.
  char* chunk_name_;

  // Default MkvWriter object created by this class used for writing clusters
  // out in separate files.
  MkvWriter* chunk_writer_cluster_;

  // Default MkvWriter object created by this class used for writing Cues
  // element out to a file.
  MkvWriter* chunk_writer_cues_;

  // Default MkvWriter object created by this class used for writing the
  // Matroska header out to a file.
  MkvWriter* chunk_writer_header_;

  // Flag telling whether or not the muxer is chunking output to multiple
  // files.
  bool chunking_;

  // Base filename for the chunked files.
  char* chunking_base_name_;

  // File position offset where the Clusters end.
  int64 cluster_end_offset_;

  // List of clusters.
  Cluster** cluster_list_;

  // Number of cluster pointers allocated in the cluster list.
  int32 cluster_list_capacity_;

  // Number of clusters in the cluster list.
  int32 cluster_list_size_;

  // Indicates whether Cues should be written before or after Clusters
  CuesPosition cues_position_;

  // Track number that is associated with the cues element for this segment.
  uint64 cues_track_;

  // Tells the muxer to force a new cluster on the next Block.
  bool force_new_cluster_;

  // List of stored audio frames. These variables are used to store frames so
  // the muxer can follow the guideline "Audio blocks that contain the video
  // key frame's timecode should be in the same cluster as the video key frame
  // block."
  Frame** frames_;

  // Number of frame pointers allocated in the frame list.
  int32 frames_capacity_;

  // Number of frames in the frame list.
  int32 frames_size_;

  // Flag telling if a video track has been added to the segment.
  bool has_video_;

  // Flag telling if the segment's header has been written.
  bool header_written_;

  // Duration of the last block in nanoseconds.
  uint64 last_block_duration_;

  // Last timestamp in nanoseconds added to a cluster.
  uint64 last_timestamp_;

  // Maximum time in nanoseconds for a cluster duration. This variable is a
  // guideline and some clusters may have a longer duration. Default is 30
  // seconds.
  uint64 max_cluster_duration_;

  // Maximum size in bytes for a cluster. This variable is a guideline and
  // some clusters may have a larger size. Default is 0 which signifies that
  // the muxer will decide the size.
  uint64 max_cluster_size_;

  // The mode that segment is in. If set to |kLive| the writer must not
  // seek backwards.
  Mode mode_;

  // Flag telling the muxer that a new cue point should be added.
  bool new_cuepoint_;

  // TODO(fgalligan): Should we add support for more than one Cues element?
  // Flag whether or not the muxer should output a Cues element.
  bool output_cues_;

  // The file position of the segment's payload.
  int64 payload_pos_;

  // The file position of the element's size.
  int64 size_position_;

  // Pointer to the writer objects. Not owned by this class.
  IMkvWriter* writer_cluster_;
  IMkvWriter* writer_cues_;
  IMkvWriter* writer_header_;

  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(Segment);
};

}  // end namespace mkvmuxer

#endif  // MKVMUXER_HPP
