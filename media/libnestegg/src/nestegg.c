/*
 * Copyright © 2010 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "nestegg/nestegg.h"

/* EBML Elements */
#define ID_EBML                     0x1a45dfa3
#define ID_EBML_VERSION             0x4286
#define ID_EBML_READ_VERSION        0x42f7
#define ID_EBML_MAX_ID_LENGTH       0x42f2
#define ID_EBML_MAX_SIZE_LENGTH     0x42f3
#define ID_DOCTYPE                  0x4282
#define ID_DOCTYPE_VERSION          0x4287
#define ID_DOCTYPE_READ_VERSION     0x4285

/* Global Elements */
#define ID_VOID                     0xec
#define ID_CRC32                    0xbf

/* WebM Elements */
#define ID_SEGMENT                  0x18538067

/* Seek Head Elements */
#define ID_SEEK_HEAD                0x114d9b74
#define ID_SEEK                     0x4dbb
#define ID_SEEK_ID                  0x53ab
#define ID_SEEK_POSITION            0x53ac

/* Info Elements */
#define ID_INFO                     0x1549a966
#define ID_TIMECODE_SCALE           0x2ad7b1
#define ID_DURATION                 0x4489

/* Cluster Elements */
#define ID_CLUSTER                  0x1f43b675
#define ID_TIMECODE                 0xe7
#define ID_BLOCK_GROUP              0xa0
#define ID_SIMPLE_BLOCK             0xa3

/* BlockGroup Elements */
#define ID_BLOCK                    0xa1
#define ID_BLOCK_ADDITIONS          0x75a1
#define ID_BLOCK_DURATION           0x9b
#define ID_REFERENCE_BLOCK          0xfb
#define ID_DISCARD_PADDING          0x75a2

/* BlockAdditions Elements */
#define ID_BLOCK_MORE               0xa6

/* BlockMore Elements */
#define ID_BLOCK_ADD_ID             0xee
#define ID_BLOCK_ADDITIONAL         0xa5

/* Tracks Elements */
#define ID_TRACKS                   0x1654ae6b
#define ID_TRACK_ENTRY              0xae
#define ID_TRACK_NUMBER             0xd7
#define ID_TRACK_UID                0x73c5
#define ID_TRACK_TYPE               0x83
#define ID_FLAG_ENABLED             0xb9
#define ID_FLAG_DEFAULT             0x88
#define ID_FLAG_LACING              0x9c
#define ID_TRACK_TIMECODE_SCALE     0x23314f
#define ID_LANGUAGE                 0x22b59c
#define ID_CODEC_ID                 0x86
#define ID_CODEC_PRIVATE            0x63a2
#define ID_CODEC_DELAY              0x56aa
#define ID_SEEK_PREROLL             0x56bb
#define ID_DEFAULT_DURATION         0x23e383

/* Video Elements */
#define ID_VIDEO                    0xe0
#define ID_STEREO_MODE              0x53b8
#define ID_ALPHA_MODE               0x53c0
#define ID_PIXEL_WIDTH              0xb0
#define ID_PIXEL_HEIGHT             0xba
#define ID_PIXEL_CROP_BOTTOM        0x54aa
#define ID_PIXEL_CROP_TOP           0x54bb
#define ID_PIXEL_CROP_LEFT          0x54cc
#define ID_PIXEL_CROP_RIGHT         0x54dd
#define ID_DISPLAY_WIDTH            0x54b0
#define ID_DISPLAY_HEIGHT           0x54ba
#define ID_COLOUR                   0x55b0

/* Audio Elements */
#define ID_AUDIO                    0xe1
#define ID_SAMPLING_FREQUENCY       0xb5
#define ID_CHANNELS                 0x9f
#define ID_BIT_DEPTH                0x6264

/* Cues Elements */
#define ID_CUES                     0x1c53bb6b
#define ID_CUE_POINT                0xbb
#define ID_CUE_TIME                 0xb3
#define ID_CUE_TRACK_POSITIONS      0xb7
#define ID_CUE_TRACK                0xf7
#define ID_CUE_CLUSTER_POSITION     0xf1
#define ID_CUE_BLOCK_NUMBER         0x5378

/* Encoding Elements */
#define ID_CONTENT_ENCODINGS        0x6d80
#define ID_CONTENT_ENCODING         0x6240
#define ID_CONTENT_ENCODING_TYPE    0x5033

/* Encryption Elements */
#define ID_CONTENT_ENCRYPTION       0x5035
#define ID_CONTENT_ENC_ALGO         0x47e1
#define ID_CONTENT_ENC_KEY_ID       0x47e2
#define ID_CONTENT_ENC_AES_SETTINGS 0x47e7
#define ID_AES_SETTINGS_CIPHER_MODE 0x47e8

/* Colour Elements */
#define ID_MATRIX_COEFFICIENTS      0x55b1
#define ID_RANGE                    0x55b9
#define ID_TRANSFER_CHARACTERISTICS 0x55ba
#define ID_PRIMARIES                0x55bb
#define ID_MASTERING_METADATA       0x55d0

/* MasteringMetadata Elements */
#define ID_PRIMARY_R_CHROMATICITY_X   0x55d1
#define ID_PRIMARY_R_CHROMATICITY_Y   0x55d2
#define ID_PRIMARY_G_CHROMATICITY_X   0x55d3
#define ID_PRIMARY_G_CHROMATICITY_Y   0x55d4
#define ID_PRIMARY_B_CHROMATICITY_X   0x55d5
#define ID_PRIMARY_B_CHROMATICITY_Y   0x55d6
#define ID_WHITE_POINT_CHROMATICITY_X 0x55d7
#define ID_WHITE_POINT_CHROMATICITY_Y 0x55d8
#define ID_LUMINANCE_MAX              0x55d9
#define ID_LUMINANCE_MIN              0x55da

/* EBML Types */
enum ebml_type_enum {
  TYPE_UNKNOWN,
  TYPE_MASTER,
  TYPE_UINT,
  TYPE_FLOAT,
  TYPE_STRING,
  TYPE_BINARY
};

#define LIMIT_STRING                (1 << 20)
#define LIMIT_BINARY                (1 << 24)
#define LIMIT_BLOCK                 (1 << 30)
#define LIMIT_FRAME                 (1 << 28)

/* Field Flags */
#define DESC_FLAG_NONE              0
#define DESC_FLAG_MULTI             (1 << 0)
#define DESC_FLAG_SUSPEND           (1 << 1)
#define DESC_FLAG_OFFSET            (1 << 2)

/* Block Header Flags */
#define SIMPLE_BLOCK_FLAGS_KEYFRAME (1 << 7)
#define BLOCK_FLAGS_LACING          6

/* Lacing Constants */
#define LACING_NONE                 0
#define LACING_XIPH                 1
#define LACING_FIXED                2
#define LACING_EBML                 3

/* Track Types */
#define TRACK_TYPE_VIDEO            1
#define TRACK_TYPE_AUDIO            2

/* Track IDs */
#define TRACK_ID_VP8                "V_VP8"
#define TRACK_ID_VP9                "V_VP9"
#define TRACK_ID_AV1                "V_AV1"
#define TRACK_ID_VORBIS             "A_VORBIS"
#define TRACK_ID_OPUS               "A_OPUS"

/* Track Encryption */
#define CONTENT_ENC_ALGO_AES        5
#define AES_SETTINGS_CIPHER_CTR     1

/* Packet Encryption */
#define SIGNAL_BYTE_SIZE            1
#define IV_SIZE                     8
#define NUM_PACKETS_SIZE            1
#define PACKET_OFFSET_SIZE          4

/* Signal Byte */
#define PACKET_ENCRYPTED            1
#define ENCRYPTED_BIT_MASK          (1 << 0)

#define PACKET_PARTITIONED          2
#define PARTITIONED_BIT_MASK        (1 << 1)

enum vint_mask {
  MASK_NONE,
  MASK_FIRST_BIT
};

struct ebml_binary {
  unsigned char * data;
  size_t length;
};

struct ebml_list_node {
  struct ebml_list_node * next;
  uint64_t id;
  void * data;
};

struct ebml_list {
  struct ebml_list_node * head;
  struct ebml_list_node * tail;
};

struct ebml_type {
  union ebml_value {
    uint64_t u;
    double f;
    int64_t i;
    char * s;
    struct ebml_binary b;
  } v;
  enum ebml_type_enum type;
  int read;
};

/* EBML Definitions */
struct ebml {
  struct ebml_type ebml_version;
  struct ebml_type ebml_read_version;
  struct ebml_type ebml_max_id_length;
  struct ebml_type ebml_max_size_length;
  struct ebml_type doctype;
  struct ebml_type doctype_version;
  struct ebml_type doctype_read_version;
};

/* Matroksa Definitions */
struct seek {
  struct ebml_type id;
  struct ebml_type position;
};

struct seek_head {
  struct ebml_list seek;
};

struct info {
  struct ebml_type timecode_scale;
  struct ebml_type duration;
};

struct mastering_metadata {
  struct ebml_type primary_r_chromacity_x;
  struct ebml_type primary_r_chromacity_y;
  struct ebml_type primary_g_chromacity_x;
  struct ebml_type primary_g_chromacity_y;
  struct ebml_type primary_b_chromacity_x;
  struct ebml_type primary_b_chromacity_y;
  struct ebml_type white_point_chromaticity_x;
  struct ebml_type white_point_chromaticity_y;
  struct ebml_type luminance_max;
  struct ebml_type luminance_min;
};

struct colour {
  struct ebml_type matrix_coefficients;
  struct ebml_type range;
  struct ebml_type transfer_characteristics;
  struct ebml_type primaries;
  struct mastering_metadata mastering_metadata;
};

struct video {
  struct ebml_type stereo_mode;
  struct ebml_type alpha_mode;
  struct ebml_type pixel_width;
  struct ebml_type pixel_height;
  struct ebml_type pixel_crop_bottom;
  struct ebml_type pixel_crop_top;
  struct ebml_type pixel_crop_left;
  struct ebml_type pixel_crop_right;
  struct ebml_type display_width;
  struct ebml_type display_height;
  struct colour colour;
};

struct audio {
  struct ebml_type sampling_frequency;
  struct ebml_type channels;
  struct ebml_type bit_depth;
};

struct content_enc_aes_settings {
  struct ebml_type aes_settings_cipher_mode;
};

struct content_encryption {
  struct ebml_type content_enc_algo;
  struct ebml_type content_enc_key_id;
  struct ebml_list content_enc_aes_settings;
};

struct content_encoding {
  struct ebml_type content_encoding_type;
  struct ebml_list content_encryption;
};

struct content_encodings {
  struct ebml_list content_encoding;
};

struct track_entry {
  struct ebml_type number;
  struct ebml_type uid;
  struct ebml_type type;
  struct ebml_type flag_enabled;
  struct ebml_type flag_default;
  struct ebml_type flag_lacing;
  struct ebml_type track_timecode_scale;
  struct ebml_type language;
  struct ebml_type codec_id;
  struct ebml_type codec_private;
  struct ebml_type codec_delay;
  struct ebml_type seek_preroll;
  struct ebml_type default_duration;
  struct video video;
  struct audio audio;
  struct content_encodings content_encodings;
};

struct tracks {
  struct ebml_list track_entry;
};

struct cue_track_positions {
  struct ebml_type track;
  struct ebml_type cluster_position;
  struct ebml_type block_number;
};

struct cue_point {
  struct ebml_type time;
  struct ebml_list cue_track_positions;
};

struct cues {
  struct ebml_list cue_point;
};

struct segment {
  struct ebml_list seek_head;
  struct info info;
  struct tracks tracks;
  struct cues cues;
};

/* Misc. */
struct pool_node {
  struct pool_node * next;
  void * data;
};

struct pool_ctx {
  struct pool_node * head;
};

struct list_node {
  struct list_node * previous;
  struct ebml_element_desc * node;
  unsigned char * data;
};

struct saved_state {
  int64_t stream_offset;
  uint64_t last_id;
  uint64_t last_size;
  int last_valid;
};

struct frame_encryption {
  unsigned char * iv;
  size_t length;
  uint8_t signal_byte;
  uint8_t num_partitions;
  uint32_t * partition_offsets;
};

struct frame {
  unsigned char * data;
  size_t length;
  struct frame_encryption * frame_encryption;
  struct frame * next;
};

struct block_additional {
  unsigned int id;
  unsigned char * data;
  size_t length;
  struct block_additional * next;
};

/* Public (opaque) Structures */
struct nestegg {
  nestegg_io * io;
  nestegg_log log;
  struct pool_ctx * alloc_pool;
  uint64_t last_id;
  uint64_t last_size;
  int last_valid;
  struct list_node * ancestor;
  struct ebml ebml;
  struct segment segment;
  int64_t segment_offset;
  unsigned int track_count;
  /* Last read cluster. */
  uint64_t cluster_timecode;
  int read_cluster_timecode;
  struct saved_state saved;
};

struct nestegg_packet {
  uint64_t track;
  uint64_t timecode;
  uint64_t duration;
  int read_duration;
  struct frame * frame;
  struct block_additional * block_additional;
  int64_t discard_padding;
  int read_discard_padding;
  int64_t reference_block;
  int read_reference_block;
  uint8_t keyframe;
};

/* Element Descriptor */
struct ebml_element_desc {
  char const * name;
  uint64_t id;
  enum ebml_type_enum type;
  size_t offset;
  unsigned int flags;
  struct ebml_element_desc * children;
  size_t size;
  size_t data_offset;
};

#define E_FIELD(ID, TYPE, STRUCT, FIELD) \
  { #ID, ID, TYPE, offsetof(STRUCT, FIELD), DESC_FLAG_NONE, NULL, 0, 0 }
#define E_MASTER(ID, TYPE, STRUCT, FIELD) \
  { #ID, ID, TYPE, offsetof(STRUCT, FIELD), DESC_FLAG_MULTI, ne_ ## FIELD ## _elements, \
      sizeof(struct FIELD), 0 }
#define E_SINGLE_MASTER_O(ID, TYPE, STRUCT, FIELD) \
  { #ID, ID, TYPE, offsetof(STRUCT, FIELD), DESC_FLAG_OFFSET, ne_ ## FIELD ## _elements, 0, \
      offsetof(STRUCT, FIELD ## _offset) }
#define E_SINGLE_MASTER(ID, TYPE, STRUCT, FIELD) \
  { #ID, ID, TYPE, offsetof(STRUCT, FIELD), DESC_FLAG_NONE, ne_ ## FIELD ## _elements, 0, 0 }
#define E_SUSPEND(ID, TYPE) \
  { #ID, ID, TYPE, 0, DESC_FLAG_SUSPEND, NULL, 0, 0 }
#define E_LAST \
  { NULL, 0, 0, 0, DESC_FLAG_NONE, NULL, 0, 0 }

/* EBML Element Lists */
static struct ebml_element_desc ne_ebml_elements[] = {
  E_FIELD(ID_EBML_VERSION, TYPE_UINT, struct ebml, ebml_version),
  E_FIELD(ID_EBML_READ_VERSION, TYPE_UINT, struct ebml, ebml_read_version),
  E_FIELD(ID_EBML_MAX_ID_LENGTH, TYPE_UINT, struct ebml, ebml_max_id_length),
  E_FIELD(ID_EBML_MAX_SIZE_LENGTH, TYPE_UINT, struct ebml, ebml_max_size_length),
  E_FIELD(ID_DOCTYPE, TYPE_STRING, struct ebml, doctype),
  E_FIELD(ID_DOCTYPE_VERSION, TYPE_UINT, struct ebml, doctype_version),
  E_FIELD(ID_DOCTYPE_READ_VERSION, TYPE_UINT, struct ebml, doctype_read_version),
  E_LAST
};

/* WebM Element Lists */
static struct ebml_element_desc ne_seek_elements[] = {
  E_FIELD(ID_SEEK_ID, TYPE_BINARY, struct seek, id),
  E_FIELD(ID_SEEK_POSITION, TYPE_UINT, struct seek, position),
  E_LAST
};

static struct ebml_element_desc ne_seek_head_elements[] = {
  E_MASTER(ID_SEEK, TYPE_MASTER, struct seek_head, seek),
  E_LAST
};

static struct ebml_element_desc ne_info_elements[] = {
  E_FIELD(ID_TIMECODE_SCALE, TYPE_UINT, struct info, timecode_scale),
  E_FIELD(ID_DURATION, TYPE_FLOAT, struct info, duration),
  E_LAST
};

static struct ebml_element_desc ne_mastering_metadata_elements[] = {
  E_FIELD(ID_PRIMARY_R_CHROMATICITY_X, TYPE_FLOAT, struct mastering_metadata, primary_r_chromacity_x),
  E_FIELD(ID_PRIMARY_R_CHROMATICITY_Y, TYPE_FLOAT, struct mastering_metadata, primary_r_chromacity_y),
  E_FIELD(ID_PRIMARY_G_CHROMATICITY_X, TYPE_FLOAT, struct mastering_metadata, primary_g_chromacity_x),
  E_FIELD(ID_PRIMARY_G_CHROMATICITY_Y, TYPE_FLOAT, struct mastering_metadata, primary_g_chromacity_y),
  E_FIELD(ID_PRIMARY_B_CHROMATICITY_X, TYPE_FLOAT, struct mastering_metadata, primary_b_chromacity_x),
  E_FIELD(ID_PRIMARY_B_CHROMATICITY_Y, TYPE_FLOAT, struct mastering_metadata, primary_b_chromacity_y),
  E_FIELD(ID_WHITE_POINT_CHROMATICITY_X, TYPE_FLOAT, struct mastering_metadata, white_point_chromaticity_x),
  E_FIELD(ID_WHITE_POINT_CHROMATICITY_Y, TYPE_FLOAT, struct mastering_metadata, white_point_chromaticity_y),
  E_FIELD(ID_LUMINANCE_MAX, TYPE_FLOAT, struct mastering_metadata, luminance_max),
  E_FIELD(ID_LUMINANCE_MIN, TYPE_FLOAT, struct mastering_metadata, luminance_min),
  E_LAST
};

static struct ebml_element_desc ne_colour_elements[] = {
  E_FIELD(ID_MATRIX_COEFFICIENTS, TYPE_UINT, struct colour, matrix_coefficients),
  E_FIELD(ID_RANGE, TYPE_UINT, struct colour, range),
  E_FIELD(ID_TRANSFER_CHARACTERISTICS, TYPE_UINT, struct colour, transfer_characteristics),
  E_FIELD(ID_PRIMARIES, TYPE_UINT, struct colour, primaries),
  E_SINGLE_MASTER(ID_MASTERING_METADATA, TYPE_MASTER, struct colour, mastering_metadata),
  E_LAST
};

static struct ebml_element_desc ne_video_elements[] = {
  E_FIELD(ID_STEREO_MODE, TYPE_UINT, struct video, stereo_mode),
  E_FIELD(ID_ALPHA_MODE, TYPE_UINT, struct video, alpha_mode),
  E_FIELD(ID_PIXEL_WIDTH, TYPE_UINT, struct video, pixel_width),
  E_FIELD(ID_PIXEL_HEIGHT, TYPE_UINT, struct video, pixel_height),
  E_FIELD(ID_PIXEL_CROP_BOTTOM, TYPE_UINT, struct video, pixel_crop_bottom),
  E_FIELD(ID_PIXEL_CROP_TOP, TYPE_UINT, struct video, pixel_crop_top),
  E_FIELD(ID_PIXEL_CROP_LEFT, TYPE_UINT, struct video, pixel_crop_left),
  E_FIELD(ID_PIXEL_CROP_RIGHT, TYPE_UINT, struct video, pixel_crop_right),
  E_FIELD(ID_DISPLAY_WIDTH, TYPE_UINT, struct video, display_width),
  E_FIELD(ID_DISPLAY_HEIGHT, TYPE_UINT, struct video, display_height),
  E_SINGLE_MASTER(ID_COLOUR, TYPE_MASTER, struct video, colour),
  E_LAST
};

static struct ebml_element_desc ne_audio_elements[] = {
  E_FIELD(ID_SAMPLING_FREQUENCY, TYPE_FLOAT, struct audio, sampling_frequency),
  E_FIELD(ID_CHANNELS, TYPE_UINT, struct audio, channels),
  E_FIELD(ID_BIT_DEPTH, TYPE_UINT, struct audio, bit_depth),
  E_LAST
};

static struct ebml_element_desc ne_content_enc_aes_settings_elements[] = {
  E_FIELD(ID_AES_SETTINGS_CIPHER_MODE, TYPE_UINT, struct content_enc_aes_settings, aes_settings_cipher_mode),
  E_LAST
};

static struct ebml_element_desc ne_content_encryption_elements[] = {
  E_FIELD(ID_CONTENT_ENC_ALGO, TYPE_UINT, struct content_encryption, content_enc_algo),
  E_FIELD(ID_CONTENT_ENC_KEY_ID, TYPE_BINARY, struct content_encryption, content_enc_key_id),
  E_MASTER(ID_CONTENT_ENC_AES_SETTINGS, TYPE_MASTER, struct content_encryption, content_enc_aes_settings),
  E_LAST
};

static struct ebml_element_desc ne_content_encoding_elements[] = {
  E_FIELD(ID_CONTENT_ENCODING_TYPE, TYPE_UINT, struct content_encoding, content_encoding_type),
  E_MASTER(ID_CONTENT_ENCRYPTION, TYPE_MASTER, struct content_encoding, content_encryption),
  E_LAST
};

static struct ebml_element_desc ne_content_encodings_elements[] = {
  E_MASTER(ID_CONTENT_ENCODING, TYPE_MASTER, struct content_encodings, content_encoding),
  E_LAST
};

static struct ebml_element_desc ne_track_entry_elements[] = {
  E_FIELD(ID_TRACK_NUMBER, TYPE_UINT, struct track_entry, number),
  E_FIELD(ID_TRACK_UID, TYPE_UINT, struct track_entry, uid),
  E_FIELD(ID_TRACK_TYPE, TYPE_UINT, struct track_entry, type),
  E_FIELD(ID_FLAG_ENABLED, TYPE_UINT, struct track_entry, flag_enabled),
  E_FIELD(ID_FLAG_DEFAULT, TYPE_UINT, struct track_entry, flag_default),
  E_FIELD(ID_FLAG_LACING, TYPE_UINT, struct track_entry, flag_lacing),
  E_FIELD(ID_TRACK_TIMECODE_SCALE, TYPE_FLOAT, struct track_entry, track_timecode_scale),
  E_FIELD(ID_LANGUAGE, TYPE_STRING, struct track_entry, language),
  E_FIELD(ID_CODEC_ID, TYPE_STRING, struct track_entry, codec_id),
  E_FIELD(ID_CODEC_PRIVATE, TYPE_BINARY, struct track_entry, codec_private),
  E_FIELD(ID_CODEC_DELAY, TYPE_UINT, struct track_entry, codec_delay),
  E_FIELD(ID_SEEK_PREROLL, TYPE_UINT, struct track_entry, seek_preroll),
  E_FIELD(ID_DEFAULT_DURATION, TYPE_UINT, struct track_entry, default_duration),
  E_SINGLE_MASTER(ID_VIDEO, TYPE_MASTER, struct track_entry, video),
  E_SINGLE_MASTER(ID_AUDIO, TYPE_MASTER, struct track_entry, audio),
  E_SINGLE_MASTER(ID_CONTENT_ENCODINGS, TYPE_MASTER, struct track_entry, content_encodings),
  E_LAST
};

static struct ebml_element_desc ne_tracks_elements[] = {
  E_MASTER(ID_TRACK_ENTRY, TYPE_MASTER, struct tracks, track_entry),
  E_LAST
};

static struct ebml_element_desc ne_cue_track_positions_elements[] = {
  E_FIELD(ID_CUE_TRACK, TYPE_UINT, struct cue_track_positions, track),
  E_FIELD(ID_CUE_CLUSTER_POSITION, TYPE_UINT, struct cue_track_positions, cluster_position),
  E_FIELD(ID_CUE_BLOCK_NUMBER, TYPE_UINT, struct cue_track_positions, block_number),
  E_LAST
};

static struct ebml_element_desc ne_cue_point_elements[] = {
  E_FIELD(ID_CUE_TIME, TYPE_UINT, struct cue_point, time),
  E_MASTER(ID_CUE_TRACK_POSITIONS, TYPE_MASTER, struct cue_point, cue_track_positions),
  E_LAST
};

static struct ebml_element_desc ne_cues_elements[] = {
  E_MASTER(ID_CUE_POINT, TYPE_MASTER, struct cues, cue_point),
  E_LAST
};

static struct ebml_element_desc ne_segment_elements[] = {
  E_MASTER(ID_SEEK_HEAD, TYPE_MASTER, struct segment, seek_head),
  E_SINGLE_MASTER(ID_INFO, TYPE_MASTER, struct segment, info),
  E_SUSPEND(ID_CLUSTER, TYPE_MASTER),
  E_SINGLE_MASTER(ID_TRACKS, TYPE_MASTER, struct segment, tracks),
  E_SINGLE_MASTER(ID_CUES, TYPE_MASTER, struct segment, cues),
  E_LAST
};

static struct ebml_element_desc ne_top_level_elements[] = {
  E_SINGLE_MASTER(ID_EBML, TYPE_MASTER, nestegg, ebml),
  E_SINGLE_MASTER_O(ID_SEGMENT, TYPE_MASTER, nestegg, segment),
  E_LAST
};

#undef E_FIELD
#undef E_MASTER
#undef E_SINGLE_MASTER_O
#undef E_SINGLE_MASTER
#undef E_SUSPEND
#undef E_LAST

static struct pool_ctx *
ne_pool_init(void)
{
  return calloc(1, sizeof(struct pool_ctx));
}

static void
ne_pool_destroy(struct pool_ctx * pool)
{
  struct pool_node * node = pool->head;
  while (node) {
    struct pool_node * old = node;
    node = node->next;
    free(old->data);
    free(old);
  }
  free(pool);
}

static void *
ne_pool_alloc(size_t size, struct pool_ctx * pool)
{
  struct pool_node * node;

  node = calloc(1, sizeof(*node));
  if (!node)
    return NULL;

  node->data = calloc(1, size);
  if (!node->data) {
    free(node);
    return NULL;
  }

  node->next = pool->head;
  pool->head = node;

  return node->data;
}

static void *
ne_alloc(size_t size)
{
  return calloc(1, size);
}

static int
ne_io_read(nestegg_io * io, void * buffer, size_t length)
{
  return io->read(buffer, length, io->userdata);
}

static int
ne_io_seek(nestegg_io * io, int64_t offset, int whence)
{
  return io->seek(offset, whence, io->userdata);
}

static int
ne_io_read_skip(nestegg_io * io, size_t length)
{
  size_t get;
  unsigned char buf[8192];
  int r = 1;

  while (length > 0) {
    get = length < sizeof(buf) ? length : sizeof(buf);
    r = ne_io_read(io, buf, get);
    if (r != 1)
      break;
    length -= get;
  }

  return r;
}

static int64_t
ne_io_tell(nestegg_io * io)
{
  return io->tell(io->userdata);
}

static int
ne_bare_read_vint(nestegg_io * io, uint64_t * value, uint64_t * length, enum vint_mask maskflag)
{
  int r;
  unsigned char b;
  size_t maxlen = 8;
  unsigned int count = 1, mask = 1 << 7;

  r = ne_io_read(io, &b, 1);
  if (r != 1)
    return r;

  while (count < maxlen) {
    if ((b & mask) != 0)
      break;
    mask >>= 1;
    count += 1;
  }

  if (length)
    *length = count;
  *value = b;

  if (maskflag == MASK_FIRST_BIT)
    *value = b & ~mask;

  while (--count) {
    r = ne_io_read(io, &b, 1);
    if (r != 1)
      return r;
    *value <<= 8;
    *value |= b;
  }

  return 1;
}

static int
ne_read_id(nestegg_io * io, uint64_t * value, uint64_t * length)
{
  return ne_bare_read_vint(io, value, length, MASK_NONE);
}

static int
ne_read_vint(nestegg_io * io, uint64_t * value, uint64_t * length)
{
  return ne_bare_read_vint(io, value, length, MASK_FIRST_BIT);
}

static int
ne_read_svint(nestegg_io * io, int64_t * value, uint64_t * length)
{
  int r;
  uint64_t uvalue;
  uint64_t ulength;
  int64_t svint_subtr[] = {
    0x3f, 0x1fff,
    0xfffff, 0x7ffffff,
    0x3ffffffffLL, 0x1ffffffffffLL,
    0xffffffffffffLL, 0x7fffffffffffffLL
  };

  r = ne_bare_read_vint(io, &uvalue, &ulength, MASK_FIRST_BIT);
  if (r != 1)
    return r;
  *value = uvalue - svint_subtr[ulength - 1];
  if (length)
    *length = ulength;
  return r;
}

static int
ne_read_uint(nestegg_io * io, uint64_t * val, uint64_t length)
{
  unsigned char b;
  int r;

  if (length == 0 || length > 8)
    return -1;
  r = ne_io_read(io, &b, 1);
  if (r != 1)
    return r;
  *val = b;
  while (--length) {
    r = ne_io_read(io, &b, 1);
    if (r != 1)
      return r;
    *val <<= 8;
    *val |= b;
  }
  return 1;
}

static int
ne_read_int(nestegg_io * io, int64_t * val, uint64_t length)
{
  int r;
  uint64_t uval, base;

  r = ne_read_uint(io, &uval, length);
  if (r != 1)
    return r;

  if (length < sizeof(int64_t)) {
    base = 1;
    base <<= length * 8 - 1;
    if (uval >= base) {
      base = 1;
      base <<= length * 8;
    } else {
      base = 0;
    }
    *val = uval - base;
  } else {
    *val = (int64_t) uval;
  }

  return 1;
}

static int
ne_read_float(nestegg_io * io, double * val, uint64_t length)
{
  union {
    uint64_t u;
    struct {
#if defined(__FLOAT_WORD_ORDER__) && __FLOAT_WORD_ORDER__ == __ORDER_BIG_ENDIAN__
      uint32_t _pad;
      float f;
#else
      float f;
      uint32_t _pad;
#endif
    } f;
    double d;
  } value;
  int r;

  /* Length == 10 not implemented. */
  if (length != 4 && length != 8)
    return -1;
  r = ne_read_uint(io, &value.u, length);
  if (r != 1)
    return r;
  if (length == 4)
    *val = value.f.f;
  else
    *val = value.d;
  return 1;
}

static int
ne_read_string(nestegg * ctx, char ** val, uint64_t length)
{
  char * str;
  int r;

  if (length > LIMIT_STRING)
    return -1;
  str = ne_pool_alloc(length + 1, ctx->alloc_pool);
  if (!str)
    return -1;
  if (length) {
    r = ne_io_read(ctx->io, (unsigned char *) str, length);
    if (r != 1)
      return r;
  }
  str[length] = '\0';
  *val = str;
  return 1;
}

static int
ne_read_binary(nestegg * ctx, struct ebml_binary * val, uint64_t length)
{
  if (length == 0 || length > LIMIT_BINARY)
    return -1;
  val->data = ne_pool_alloc(length, ctx->alloc_pool);
  if (!val->data)
    return -1;
  val->length = length;
  return ne_io_read(ctx->io, val->data, length);
}

static int
ne_get_uint(struct ebml_type type, uint64_t * value)
{
  if (!type.read)
    return -1;

  assert(type.type == TYPE_UINT);

  *value = type.v.u;

  return 0;
}

static int
ne_get_float(struct ebml_type type, double * value)
{
  if (!type.read)
    return -1;

  assert(type.type == TYPE_FLOAT);

  *value = type.v.f;

  return 0;
}

static int
ne_get_string(struct ebml_type type, char ** value)
{
  if (!type.read)
    return -1;

  assert(type.type == TYPE_STRING);

  *value = type.v.s;

  return 0;
}

static int
ne_get_binary(struct ebml_type type, struct ebml_binary * value)
{
  if (!type.read)
    return -1;

  assert(type.type == TYPE_BINARY);

  *value = type.v.b;

  return 0;
}

static int
ne_is_ancestor_element(uint64_t id, struct list_node * ancestor)
{
  struct ebml_element_desc * element;

  for (; ancestor; ancestor = ancestor->previous)
    for (element = ancestor->node; element->id; ++element)
      if (element->id == id)
        return 1;

  return 0;
}

static struct ebml_element_desc *
ne_find_element(uint64_t id, struct ebml_element_desc * elements)
{
  struct ebml_element_desc * element;

  for (element = elements; element->id; ++element)
    if (element->id == id)
      return element;

  return NULL;
}

static int
ne_ctx_push(nestegg * ctx, struct ebml_element_desc * ancestor, void * data)
{
  struct list_node * item;

  item = ne_alloc(sizeof(*item));
  if (!item)
    return -1;
  item->previous = ctx->ancestor;
  item->node = ancestor;
  item->data = data;
  ctx->ancestor = item;
  return 0;
}

static void
ne_ctx_pop(nestegg * ctx)
{
  struct list_node * item;

  item = ctx->ancestor;
  ctx->ancestor = item->previous;
  free(item);
}

static int
ne_ctx_save(nestegg * ctx, struct saved_state * s)
{
  s->stream_offset = ne_io_tell(ctx->io);
  if (s->stream_offset < 0)
    return -1;
  s->last_id = ctx->last_id;
  s->last_size = ctx->last_size;
  s->last_valid = ctx->last_valid;
  return 0;
}

static int
ne_ctx_restore(nestegg * ctx, struct saved_state * s)
{
  int r;

  if (s->stream_offset < 0)
    return -1;
  r = ne_io_seek(ctx->io, s->stream_offset, NESTEGG_SEEK_SET);
  if (r != 0)
    return -1;
  ctx->last_id = s->last_id;
  ctx->last_size = s->last_size;
  ctx->last_valid = s->last_valid;
  return 0;
}

static int
ne_peek_element(nestegg * ctx, uint64_t * id, uint64_t * size)
{
  int r;

  if (ctx->last_valid) {
    if (id)
      *id = ctx->last_id;
    if (size)
      *size = ctx->last_size;
    return 1;
  }

  r = ne_read_id(ctx->io, &ctx->last_id, NULL);
  if (r != 1)
    return r;

  r = ne_read_vint(ctx->io, &ctx->last_size, NULL);
  if (r != 1)
    return r;

  if (id)
    *id = ctx->last_id;
  if (size)
    *size = ctx->last_size;

  ctx->last_valid = 1;

  return 1;
}

static int
ne_read_element(nestegg * ctx, uint64_t * id, uint64_t * size)
{
  int r;

  r = ne_peek_element(ctx, id, size);
  if (r != 1)
    return r;

  ctx->last_valid = 0;

  return 1;
}

static int
ne_read_master(nestegg * ctx, struct ebml_element_desc * desc)
{
  struct ebml_list * list;
  struct ebml_list_node * node, * oldtail;

  assert(desc->type == TYPE_MASTER && desc->flags & DESC_FLAG_MULTI);

  ctx->log(ctx, NESTEGG_LOG_DEBUG, "multi master element %llx (%s)",
           desc->id, desc->name);

  list = (struct ebml_list *) (ctx->ancestor->data + desc->offset);

  node = ne_pool_alloc(sizeof(*node), ctx->alloc_pool);
  if (!node)
    return -1;
  node->id = desc->id;
  node->data = ne_pool_alloc(desc->size, ctx->alloc_pool);
  if (!node->data)
    return -1;

  oldtail = list->tail;
  if (oldtail)
    oldtail->next = node;
  list->tail = node;
  if (!list->head)
    list->head = node;

  ctx->log(ctx, NESTEGG_LOG_DEBUG, " -> using data %p", node->data);

  if (ne_ctx_push(ctx, desc->children, node->data) < 0)
    return -1;

  return 0;
}

static int
ne_read_single_master(nestegg * ctx, struct ebml_element_desc * desc)
{
  assert(desc->type == TYPE_MASTER && !(desc->flags & DESC_FLAG_MULTI));

  ctx->log(ctx, NESTEGG_LOG_DEBUG, "single master element %llx (%s)",
           desc->id, desc->name);
  ctx->log(ctx, NESTEGG_LOG_DEBUG, " -> using data %p (%u)",
           ctx->ancestor->data + desc->offset, desc->offset);

  return ne_ctx_push(ctx, desc->children, ctx->ancestor->data + desc->offset);
}

static int
ne_read_simple(nestegg * ctx, struct ebml_element_desc * desc, size_t length)
{
  struct ebml_type * storage;
  int r = -1;

  storage = (struct ebml_type *) (ctx->ancestor->data + desc->offset);

  if (storage->read) {
    ctx->log(ctx, NESTEGG_LOG_DEBUG, "element %llx (%s) already read, skipping %u",
             desc->id, desc->name, length);
    return ne_io_read_skip(ctx->io, length);
  }

  storage->type = desc->type;

  ctx->log(ctx, NESTEGG_LOG_DEBUG, "element %llx (%s) -> %p (%u)",
           desc->id, desc->name, storage, desc->offset);

  switch (desc->type) {
  case TYPE_UINT:
    r = ne_read_uint(ctx->io, &storage->v.u, length);
    break;
  case TYPE_FLOAT:
    r = ne_read_float(ctx->io, &storage->v.f, length);
    break;
  case TYPE_STRING:
    r = ne_read_string(ctx, &storage->v.s, length);
    break;
  case TYPE_BINARY:
    r = ne_read_binary(ctx, &storage->v.b, length);
    break;
  case TYPE_MASTER:
  case TYPE_UNKNOWN:
  default:
    assert(0);
    break;
  }

  if (r == 1)
    storage->read = 1;

  return r;
}

static int
ne_parse(nestegg * ctx, struct ebml_element_desc * top_level, int64_t max_offset)
{
  int r;
  int64_t * data_offset;
  uint64_t id, size, peeked_id;
  struct ebml_element_desc * element;

  assert(ctx->ancestor);

  for (;;) {
    if (max_offset > 0 && ne_io_tell(ctx->io) >= max_offset) {
      /* Reached end of offset allowed for parsing - return gracefully */
      r = 1;
      break;
    }
    r = ne_peek_element(ctx, &id, &size);
    if (r != 1)
      break;
    peeked_id = id;

    element = ne_find_element(id, ctx->ancestor->node);
    if (element) {
      if (element->flags & DESC_FLAG_SUSPEND) {
        assert(element->id == ID_CLUSTER && element->type == TYPE_MASTER);
        ctx->log(ctx, NESTEGG_LOG_DEBUG, "suspend parse at %llx", id);
        r = 1;
        break;
      }

      r = ne_read_element(ctx, &id, &size);
      if (r != 1)
        break;
      assert(id == peeked_id);

      if (element->flags & DESC_FLAG_OFFSET) {
        data_offset = (int64_t *) (ctx->ancestor->data + element->data_offset);
        *data_offset = ne_io_tell(ctx->io);
        if (*data_offset < 0) {
          r = -1;
          break;
        }
      }

      if (element->type == TYPE_MASTER) {
        if (element->flags & DESC_FLAG_MULTI) {
          if (ne_read_master(ctx, element) < 0)
            break;
        } else {
          if (ne_read_single_master(ctx, element) < 0)
            break;
        }
        continue;
      } else {
        r = ne_read_simple(ctx, element, size);
        if (r < 0)
          break;
      }
    } else if (ne_is_ancestor_element(id, ctx->ancestor->previous)) {
      ctx->log(ctx, NESTEGG_LOG_DEBUG, "parent element %llx", id);
      if (top_level && ctx->ancestor->node == top_level) {
        ctx->log(ctx, NESTEGG_LOG_DEBUG, "*** parse about to back up past top_level");
        r = 1;
        break;
      }
      ne_ctx_pop(ctx);
    } else {
      r = ne_read_element(ctx, &id, &size);
      if (r != 1)
        break;

      if (id != ID_VOID && id != ID_CRC32)
        ctx->log(ctx, NESTEGG_LOG_DEBUG, "unknown element %llx", id);
      r = ne_io_read_skip(ctx->io, size);
      if (r != 1)
        break;
    }
  }

  if (r != 1)
    while (ctx->ancestor)
      ne_ctx_pop(ctx);

  return r;
}

static int
ne_read_block_encryption(nestegg * ctx, struct track_entry const * entry,
                         uint64_t * encoding_type, uint64_t * encryption_algo,
                         uint64_t * encryption_mode)
{
  struct content_encoding * encoding;
  struct content_encryption * encryption;
  struct content_enc_aes_settings * aes_settings;

  *encoding_type = 0;
  if (entry->content_encodings.content_encoding.head) {
    encoding = entry->content_encodings.content_encoding.head->data;
    if (ne_get_uint(encoding->content_encoding_type, encoding_type) != 0)
      return -1;

    if (*encoding_type == NESTEGG_ENCODING_ENCRYPTION) {
      /* Metadata states content is encrypted */
      if (!encoding->content_encryption.head)
        return -1;

      encryption = encoding->content_encryption.head->data;
      if (ne_get_uint(encryption->content_enc_algo, encryption_algo) != 0) {
        ctx->log(ctx, NESTEGG_LOG_ERROR, "No ContentEncAlgo element found");
        return -1;
      }

      if (*encryption_algo != CONTENT_ENC_ALGO_AES) {
        ctx->log(ctx, NESTEGG_LOG_ERROR, "Disallowed ContentEncAlgo used");
        return -1;
      }

      if (!encryption->content_enc_aes_settings.head) {
        ctx->log(ctx, NESTEGG_LOG_ERROR, "No ContentEncAESSettings element found");
        return -1;
      }

      aes_settings = encryption->content_enc_aes_settings.head->data;
      *encryption_mode = AES_SETTINGS_CIPHER_CTR;
      ne_get_uint(aes_settings->aes_settings_cipher_mode, encryption_mode);

      if (*encryption_mode != AES_SETTINGS_CIPHER_CTR) {
        ctx->log(ctx, NESTEGG_LOG_ERROR, "Disallowed AESSettingsCipherMode used");
        return -1;
      }
    }
  }
  return 1;
}

static int
ne_read_xiph_lace_value(nestegg_io * io, uint64_t * value, size_t * consumed)
{
  int r;
  uint64_t lace;

  r = ne_read_uint(io, &lace, 1);
  if (r != 1)
    return r;
  *consumed += 1;

  *value = lace;
  while (lace == 255) {
    r = ne_read_uint(io, &lace, 1);
    if (r != 1)
      return r;
    *consumed += 1;
    *value += lace;
  }

  return 1;
}

static int
ne_read_xiph_lacing(nestegg_io * io, size_t block, size_t * read, uint64_t n, uint64_t * sizes)
{
  int r;
  size_t i = 0;
  uint64_t sum = 0;

  while (--n) {
    r = ne_read_xiph_lace_value(io, &sizes[i], read);
    if (r != 1)
      return r;
    sum += sizes[i];
    i += 1;
  }

  if (*read + sum > block)
    return -1;

  /* Last frame is the remainder of the block. */
  sizes[i] = block - *read - sum;
  return 1;
}

static int
ne_read_ebml_lacing(nestegg_io * io, size_t block, size_t * read, uint64_t n, uint64_t * sizes)
{
  int r;
  uint64_t lace, sum, length;
  int64_t slace;
  size_t i = 0;

  r = ne_read_vint(io, &lace, &length);
  if (r != 1)
    return r;
  *read += length;

  sizes[i] = lace;
  sum = sizes[i];

  i += 1;
  n -= 1;

  while (--n) {
    r = ne_read_svint(io, &slace, &length);
    if (r != 1)
      return r;
    *read += length;
    sizes[i] = sizes[i - 1] + slace;
    sum += sizes[i];
    i += 1;
  }

  if (*read + sum > block)
    return -1;

  /* Last frame is the remainder of the block. */
  sizes[i] = block - *read - sum;
  return 1;
}

static uint64_t
ne_get_timecode_scale(nestegg * ctx)
{
  uint64_t scale;

  if (ne_get_uint(ctx->segment.info.timecode_scale, &scale) != 0)
    scale = 1000000;

  return scale;
}

static int
ne_map_track_number_to_index(nestegg * ctx,
                             unsigned int track_number,
                             unsigned int * track_index)
{
  struct ebml_list_node * node;
  struct track_entry * t_entry;
  uint64_t t_number = 0;

  if (!track_index)
    return -1;
  *track_index = 0;

  if (track_number == 0)
    return -1;

  node = ctx->segment.tracks.track_entry.head;
  while (node) {
    assert(node->id == ID_TRACK_ENTRY);
    t_entry = node->data;
    if (ne_get_uint(t_entry->number, &t_number) != 0)
      return -1;
    if (t_number == track_number)
      return 0;
    *track_index += 1;
    node = node->next;
  }

  return -1;
}

static struct track_entry *
ne_find_track_entry(nestegg * ctx, unsigned int track)
{
  struct ebml_list_node * node;
  unsigned int tracks = 0;

  node = ctx->segment.tracks.track_entry.head;
  while (node) {
    assert(node->id == ID_TRACK_ENTRY);
    if (track == tracks)
      return node->data;
    tracks += 1;
    node = node->next;
  }

  return NULL;
}

static struct frame *
ne_alloc_frame(void)
{
  struct frame * f = ne_alloc(sizeof(*f));

  if (!f)
    return NULL;

  f->data = NULL;
  f->length = 0;
  f->frame_encryption = NULL;
  f->next = NULL;

  return f;
}

static struct frame_encryption *
ne_alloc_frame_encryption(void)
{
  struct frame_encryption * f = ne_alloc(sizeof(*f));

  if (!f)
    return NULL;

  f->iv = NULL;
  f->length = 0;
  f->signal_byte = 0;
  f->num_partitions = 0;
  f->partition_offsets = NULL;

  return f;
}

static void
ne_free_frame(struct frame * f)
{
  if (f->frame_encryption) {
    free(f->frame_encryption->iv);
    free(f->frame_encryption->partition_offsets);
  }

  free(f->frame_encryption);
  free(f->data);
  free(f);
}

static int
ne_read_block(nestegg * ctx, uint64_t block_id, uint64_t block_size, nestegg_packet ** data)
{
  int r;
  int64_t timecode, abs_timecode;
  nestegg_packet * pkt;
  struct frame * f, * last;
  struct track_entry * entry;
  double track_scale;
  uint64_t track_number, length, frame_sizes[256], cluster_tc, flags, frames, tc_scale, total,
           encoding_type, encryption_algo, encryption_mode;
  unsigned int i, lacing, track;
  uint8_t signal_byte, keyframe = NESTEGG_PACKET_HAS_KEYFRAME_UNKNOWN, j = 0;
  size_t consumed = 0, data_size, encryption_size;

  *data = NULL;

  if (block_size > LIMIT_BLOCK)
    return -1;

  r = ne_read_vint(ctx->io, &track_number, &length);
  if (r != 1)
    return r;

  if (track_number == 0)
    return -1;

  consumed += length;

  r = ne_read_int(ctx->io, &timecode, 2);
  if (r != 1)
    return r;

  consumed += 2;

  r = ne_read_uint(ctx->io, &flags, 1);
  if (r != 1)
    return r;

  consumed += 1;

  frames = 0;

  /* Simple blocks have an explicit flag for if the contents a keyframes*/
  if (block_id == ID_SIMPLE_BLOCK)
    keyframe = (flags & SIMPLE_BLOCK_FLAGS_KEYFRAME) == SIMPLE_BLOCK_FLAGS_KEYFRAME ?
                                                        NESTEGG_PACKET_HAS_KEYFRAME_TRUE :
                                                        NESTEGG_PACKET_HAS_KEYFRAME_FALSE;

  /* Flags are different between Block and SimpleBlock, but lacing is
     encoded the same way. */
  lacing = (flags & BLOCK_FLAGS_LACING) >> 1;

  switch (lacing) {
  case LACING_NONE:
    frames = 1;
    break;
  case LACING_XIPH:
  case LACING_FIXED:
  case LACING_EBML:
    r = ne_read_uint(ctx->io, &frames, 1);
    if (r != 1)
      return r;
    consumed += 1;
    frames += 1;
    break;
  default:
    assert(0);
    return -1;
  }

  if (frames > 256)
    return -1;

  switch (lacing) {
  case LACING_NONE:
    frame_sizes[0] = block_size - consumed;
    break;
  case LACING_XIPH:
    if (frames == 1)
      return -1;
    r = ne_read_xiph_lacing(ctx->io, block_size, &consumed, frames, frame_sizes);
    if (r != 1)
      return r;
    break;
  case LACING_FIXED:
    if ((block_size - consumed) % frames)
      return -1;
    for (i = 0; i < frames; ++i)
      frame_sizes[i] = (block_size - consumed) / frames;
    break;
  case LACING_EBML:
    if (frames == 1)
      return -1;
    r = ne_read_ebml_lacing(ctx->io, block_size, &consumed, frames, frame_sizes);
    if (r != 1)
      return r;
    break;
  default:
    assert(0);
    return -1;
  }

  /* Sanity check unlaced frame sizes against total block size. */
  total = consumed;
  for (i = 0; i < frames; ++i)
    total += frame_sizes[i];
  if (total > block_size)
    return -1;

  if (ne_map_track_number_to_index(ctx, track_number, &track) != 0)
    return -1;

  entry = ne_find_track_entry(ctx, track);
  if (!entry)
    return -1;

  r = ne_read_block_encryption(ctx, entry, &encoding_type, &encryption_algo, &encryption_mode);
  if (r != 1)
    return r;

  /* Encryption does not support lacing */
  if (lacing != LACING_NONE && encoding_type == NESTEGG_ENCODING_ENCRYPTION) {
    ctx->log(ctx, NESTEGG_LOG_ERROR, "Encrypted blocks may not also be laced");
    return -1;
  }

  track_scale = 1.0;

  tc_scale = ne_get_timecode_scale(ctx);
  if (tc_scale == 0)
    return -1;

  if (!ctx->read_cluster_timecode)
    return -1;
  cluster_tc = ctx->cluster_timecode;

  abs_timecode = timecode + cluster_tc;
  if (abs_timecode < 0) {
      /* Ignore the spec and negative timestamps */
      ctx->log(ctx, NESTEGG_LOG_WARNING, "ignoring negative timecode: %lld", abs_timecode);
      abs_timecode = 0;
  }

  pkt = ne_alloc(sizeof(*pkt));
  if (!pkt)
    return -1;
  pkt->track = track;
  pkt->timecode = abs_timecode * tc_scale * track_scale;
  pkt->keyframe = keyframe;

  ctx->log(ctx, NESTEGG_LOG_DEBUG, "%sblock t %lld pts %f f %llx frames: %llu",
           block_id == ID_BLOCK ? "" : "simple", pkt->track, pkt->timecode / 1e9, flags, frames);

  last = NULL;
  for (i = 0; i < frames; ++i) {
    if (frame_sizes[i] > LIMIT_FRAME) {
      nestegg_free_packet(pkt);
      return -1;
    }
    f = ne_alloc_frame();
    if (!f) {
      nestegg_free_packet(pkt);
      return -1;
    }
    /* Parse encryption */
    if (encoding_type == NESTEGG_ENCODING_ENCRYPTION) {
      r = ne_io_read(ctx->io, &signal_byte, SIGNAL_BYTE_SIZE);
      if (r != 1) {
        ne_free_frame(f);
        nestegg_free_packet(pkt);
        return r;
      }
      f->frame_encryption = ne_alloc_frame_encryption();
      if (!f->frame_encryption) {
        ne_free_frame(f);
        nestegg_free_packet(pkt);
        return -1;
      }
      f->frame_encryption->signal_byte = signal_byte;
      if ((signal_byte & ENCRYPTED_BIT_MASK) == PACKET_ENCRYPTED) {
        f->frame_encryption->iv = ne_alloc(IV_SIZE);
        if (!f->frame_encryption->iv) {
          ne_free_frame(f);
          nestegg_free_packet(pkt);
          return -1;
        }
        r = ne_io_read(ctx->io, f->frame_encryption->iv, IV_SIZE);
        if (r != 1) {
          ne_free_frame(f);
          nestegg_free_packet(pkt);
          return r;
        }
        f->frame_encryption->length = IV_SIZE;
        encryption_size = SIGNAL_BYTE_SIZE + IV_SIZE;

        if ((signal_byte & PARTITIONED_BIT_MASK) == PACKET_PARTITIONED) {
          r = ne_io_read(ctx->io, &f->frame_encryption->num_partitions, NUM_PACKETS_SIZE);
          if (r != 1) {
            ne_free_frame(f);
            nestegg_free_packet(pkt);
            return r;
          }

          encryption_size += NUM_PACKETS_SIZE + f->frame_encryption->num_partitions * PACKET_OFFSET_SIZE;
          f->frame_encryption->partition_offsets = ne_alloc(f->frame_encryption->num_partitions * PACKET_OFFSET_SIZE);

          for (j = 0; j < f->frame_encryption->num_partitions; ++j) {
            uint64_t value = 0;
            r = ne_read_uint(ctx->io, &value, PACKET_OFFSET_SIZE);
            if (r != 1) {
              break;
            }

            f->frame_encryption->partition_offsets[j] = (uint32_t) value;
          }

          /* If any of the partition offsets did not return 1, then fail. */
          if (j != f->frame_encryption->num_partitions) {
            ne_free_frame(f);
            nestegg_free_packet(pkt);
            return r;
          }
        }
      } else {
        encryption_size = SIGNAL_BYTE_SIZE;
      }
    } else {
      encryption_size = 0;
    }
    if (encryption_size > frame_sizes[i]) {
      ne_free_frame(f);
      nestegg_free_packet(pkt);
      return -1;
    }
    data_size = frame_sizes[i] - encryption_size;
    /* Encryption parsed */
    f->data = ne_alloc(data_size);
    if (!f->data) {
      ne_free_frame(f);
      nestegg_free_packet(pkt);
      return -1;
    }
    f->length = data_size;
    r = ne_io_read(ctx->io, f->data, data_size);
    if (r != 1) {
      ne_free_frame(f);
      nestegg_free_packet(pkt);
      return r;
    }

    if (!last)
      pkt->frame = f;
    else
      last->next = f;
    last = f;
  }

  *data = pkt;

  return 1;
}

static int
ne_read_block_additions(nestegg * ctx, uint64_t block_size, struct block_additional ** pkt_block_additional)
{
  int r;
  uint64_t id, size, data_size;
  int64_t block_additions_end, block_more_end;
  void * data;
  int has_data;
  struct block_additional * block_additional;
  uint64_t add_id;

  assert(*pkt_block_additional == NULL);

  block_additions_end = ne_io_tell(ctx->io) + block_size;

  while (ne_io_tell(ctx->io) < block_additions_end) {
    add_id = 1;
    data = NULL;
    has_data = 0;
    data_size = 0;
    r = ne_read_element(ctx, &id, &size);
    if (r != 1)
      return r;

    if (id != ID_BLOCK_MORE) {
      /* We don't know what this element is, so skip over it */
      if (id != ID_VOID && id != ID_CRC32)
        ctx->log(ctx, NESTEGG_LOG_DEBUG,
                 "unknown element %llx in BlockAdditions", id);
      r = ne_io_read_skip(ctx->io, size);
      if (r != 1)
        return r;
      continue;
    }

    block_more_end = ne_io_tell(ctx->io) + size;

    while (ne_io_tell(ctx->io) < block_more_end) {
      r = ne_read_element(ctx, &id, &size);
      if (r != 1) {
        free(data);
        return r;
      }

      if (id == ID_BLOCK_ADD_ID) {
        r = ne_read_uint(ctx->io, &add_id, size);
        if (r != 1) {
          free(data);
          return r;
        }

        if (add_id == 0) {
          ctx->log(ctx, NESTEGG_LOG_ERROR, "Disallowed BlockAddId 0 used");
          free(data);
          return -1;
        }
      } else if (id == ID_BLOCK_ADDITIONAL) {
        if (has_data) {
          /* BlockAdditional is supposed to only occur once in a
             BlockMore. */
          ctx->log(ctx, NESTEGG_LOG_ERROR,
                   "Multiple BlockAdditional elements in a BlockMore");
          free(data);
          return -1;
        }

        has_data = 1;
        data_size = size;
        if (data_size != 0 && data_size < LIMIT_FRAME) {
          data = ne_alloc(data_size);
          if (!data)
            return -1;
          r = ne_io_read(ctx->io, data, data_size);
          if (r != 1) {
            free(data);
            return r;
          }
        }
      } else {
        /* We don't know what this element is, so skip over it */
        if (id != ID_VOID && id != ID_CRC32)
          ctx->log(ctx, NESTEGG_LOG_DEBUG,
                   "unknown element %llx in BlockMore", id);
        r = ne_io_read_skip(ctx->io, size);
        if (r != 1) {
          free(data);
          return r;
        }
      }
    }

    if (has_data == 0) {
      ctx->log(ctx, NESTEGG_LOG_ERROR,
               "No BlockAdditional element in a BlockMore");
      return -1;
    }

    block_additional = ne_alloc(sizeof(*block_additional));
    block_additional->next = *pkt_block_additional;
    block_additional->id = add_id;
    block_additional->data = data;
    block_additional->length = data_size;
    *pkt_block_additional = block_additional;
  }

  return 1;
}

static uint64_t
ne_buf_read_id(unsigned char const * p, size_t length)
{
  uint64_t id = 0;

  while (length--) {
    id <<= 8;
    id |= *p++;
  }

  return id;
}

static struct seek *
ne_find_seek_for_id(struct ebml_list_node * seek_head, uint64_t id)
{
  struct ebml_list * head;
  struct ebml_list_node * seek;
  struct ebml_binary binary_id;
  struct seek * s;

  while (seek_head) {
    assert(seek_head->id == ID_SEEK_HEAD);
    head = seek_head->data;
    seek = head->head;

    while (seek) {
      assert(seek->id == ID_SEEK);
      s = seek->data;

      if (ne_get_binary(s->id, &binary_id) == 0 &&
          ne_buf_read_id(binary_id.data, binary_id.length) == id)
        return s;

      seek = seek->next;
    }

    seek_head = seek_head->next;
  }

  return NULL;
}

static struct cue_track_positions *
ne_find_cue_position_for_track(nestegg * ctx, struct ebml_list_node * node, unsigned int track)
{
  struct cue_track_positions * pos = NULL;
  uint64_t track_number;
  unsigned int t;

  while (node) {
    assert(node->id == ID_CUE_TRACK_POSITIONS);
    pos = node->data;
    if (ne_get_uint(pos->track, &track_number) != 0)
      return NULL;

    if (ne_map_track_number_to_index(ctx, track_number, &t) != 0)
      return NULL;

    if (t == track)
      return pos;

    node = node->next;
  }

  return NULL;
}

static struct cue_point *
ne_find_cue_point_for_tstamp(nestegg * ctx, struct ebml_list_node * cue_point, unsigned int track, uint64_t scale, uint64_t tstamp)
{
  uint64_t time;
  struct cue_point * c, * prev = NULL;

  while (cue_point) {
    assert(cue_point->id == ID_CUE_POINT);
    c = cue_point->data;

    if (!prev)
      prev = c;

    if (ne_get_uint(c->time, &time) == 0 && time * scale > tstamp)
      break;

    if (ne_find_cue_position_for_track(ctx, c->cue_track_positions.head, track) != NULL)
      prev = c;

    cue_point = cue_point->next;
  }

  return prev;
}

static void
ne_null_log_callback(nestegg * ctx, unsigned int severity, char const * fmt, ...)
{
  if (ctx && severity && fmt)
    return;
}

static int
ne_init_cue_points(nestegg * ctx, int64_t max_offset)
{
  int r;
  struct ebml_list_node * node = ctx->segment.cues.cue_point.head;
  struct seek * found;
  uint64_t seek_pos, id;
  struct saved_state state;

  /* If there are no cues loaded, check for cues element in the seek head
     and load it. */
  if (!node) {
    found = ne_find_seek_for_id(ctx->segment.seek_head.head, ID_CUES);
    if (!found)
      return -1;

    if (ne_get_uint(found->position, &seek_pos) != 0)
      return -1;

    /* Save old parser state. */
    r = ne_ctx_save(ctx, &state);
    if (r != 0)
      return -1;

    /* Seek and set up parser state for segment-level element (Cues). */
    r = ne_io_seek(ctx->io, ctx->segment_offset + seek_pos, NESTEGG_SEEK_SET);
    if (r != 0)
      return -1;
    ctx->last_valid = 0;

    r = ne_read_element(ctx, &id, NULL);
    if (r != 1)
      return -1;

    if (id != ID_CUES)
      return -1;

    assert(ctx->ancestor == NULL);
    if (ne_ctx_push(ctx, ne_top_level_elements, ctx) < 0)
      return -1;
    if (ne_ctx_push(ctx, ne_segment_elements, &ctx->segment) < 0)
      return -1;
    if (ne_ctx_push(ctx, ne_cues_elements, &ctx->segment.cues) < 0)
      return -1;
    /* parser will run until end of cues element. */
    ctx->log(ctx, NESTEGG_LOG_DEBUG, "seek: parsing cue elements");
    r = ne_parse(ctx, ne_cues_elements, max_offset);
    while (ctx->ancestor)
      ne_ctx_pop(ctx);

    /* Reset parser state to original state and seek back to old position. */
    if (ne_ctx_restore(ctx, &state) != 0)
      return -1;

    if (r < 0)
      return -1;

    node = ctx->segment.cues.cue_point.head;
    if (!node)
      return -1;
  }

  return 0;
}

/* Three functions that implement the nestegg_io interface, operating on a
   io_buffer. */
struct io_buffer {
  unsigned char const * buffer;
  size_t length;
  int64_t offset;
};

static int
ne_buffer_read(void * buffer, size_t length, void * userdata)
{
  struct io_buffer * iob = userdata;
  size_t available = iob->length - iob->offset;

  if (available == 0)
    return 0;

  if (available < length)
    return -1;

  memcpy(buffer, iob->buffer + iob->offset, length);
  iob->offset += length;

  return 1;
}

static int
ne_buffer_seek(int64_t offset, int whence, void * userdata)
{
  struct io_buffer * iob = userdata;
  int64_t o = iob->offset;

  switch(whence) {
  case NESTEGG_SEEK_SET:
    o = offset;
    break;
  case NESTEGG_SEEK_CUR:
    o += offset;
    break;
  case NESTEGG_SEEK_END:
    o = iob->length + offset;
    break;
  }

  if (o < 0 || o > (int64_t) iob->length)
    return -1;

  iob->offset = o;
  return 0;
}

static int64_t
ne_buffer_tell(void * userdata)
{
  struct io_buffer * iob = userdata;
  return iob->offset;
}

static int
ne_context_new(nestegg ** context, nestegg_io io, nestegg_log callback)
{
  nestegg * ctx;

  if (!(io.read && io.seek && io.tell))
    return -1;

  ctx = ne_alloc(sizeof(*ctx));
  if (!ctx)
    return -1;

  ctx->io = ne_alloc(sizeof(*ctx->io));
  if (!ctx->io) {
    nestegg_destroy(ctx);
    return -1;
  }
  *ctx->io = io;
  ctx->log = callback;
  ctx->alloc_pool = ne_pool_init();
  if (!ctx->alloc_pool) {
    nestegg_destroy(ctx);
    return -1;
  }

  if (!ctx->log)
    ctx->log = ne_null_log_callback;

  *context = ctx;
  return 0;
}

static int
ne_match_webm(nestegg_io io, int64_t max_offset)
{
  int r;
  uint64_t id;
  char * doctype;
  nestegg * ctx;

  if (ne_context_new(&ctx, io, NULL) != 0)
    return -1;

  r = ne_peek_element(ctx, &id, NULL);
  if (r != 1) {
    nestegg_destroy(ctx);
    return 0;
  }

  if (id != ID_EBML) {
    nestegg_destroy(ctx);
    return 0;
  }

  if (ne_ctx_push(ctx, ne_top_level_elements, ctx) < 0) {
    nestegg_destroy(ctx);
    return -1;
  }

  /* we don't check the return value of ne_parse, that might fail because
     max_offset is not on a valid element end point. We only want to check
     the EBML ID and that the doctype is "webm". */
  ne_parse(ctx, NULL, max_offset);
  while (ctx->ancestor)
    ne_ctx_pop(ctx);

  if (ne_get_string(ctx->ebml.doctype, &doctype) != 0 ||
      strcmp(doctype, "webm") != 0) {
    nestegg_destroy(ctx);
    return 0;
  }

  nestegg_destroy(ctx);

  return 1;
}

static void
ne_free_block_additions(struct block_additional * block_additional)
{
  while (block_additional) {
    struct block_additional * tmp = block_additional;
    block_additional = block_additional->next;
    free(tmp->data);
    free(tmp);
  }
}

int
nestegg_init(nestegg ** context, nestegg_io io, nestegg_log callback, int64_t max_offset)
{
  int r;
  uint64_t id, version, docversion;
  struct ebml_list_node * track;
  char * doctype;
  nestegg * ctx;

  if (ne_context_new(&ctx, io, callback) != 0)
    return -1;

  r = ne_peek_element(ctx, &id, NULL);
  if (r != 1) {
    nestegg_destroy(ctx);
    return -1;
  }

  if (id != ID_EBML) {
    nestegg_destroy(ctx);
    return -1;
  }

  ctx->log(ctx, NESTEGG_LOG_DEBUG, "ctx %p", ctx);

  if (ne_ctx_push(ctx, ne_top_level_elements, ctx) < 0) {
    nestegg_destroy(ctx);
    return -1;
  }

  r = ne_parse(ctx, NULL, max_offset);
  while (ctx->ancestor)
    ne_ctx_pop(ctx);

  if (r != 1) {
    nestegg_destroy(ctx);
    return -1;
  }

  if (ne_get_uint(ctx->ebml.ebml_read_version, &version) != 0)
    version = 1;
  if (version != 1) {
    nestegg_destroy(ctx);
    return -1;
  }

  if (ne_get_string(ctx->ebml.doctype, &doctype) != 0)
    doctype = "matroska";
  if (!!strcmp(doctype, "webm") && !!strcmp(doctype, "matroska")) {
    nestegg_destroy(ctx);
    return -1;
  }

  if (ne_get_uint(ctx->ebml.doctype_read_version, &docversion) != 0)
    docversion = 1;
  if (docversion < 1 || docversion > 2) {
    nestegg_destroy(ctx);
    return -1;
  }

  if (!ctx->segment.tracks.track_entry.head) {
    nestegg_destroy(ctx);
    return -1;
  }

  track = ctx->segment.tracks.track_entry.head;
  ctx->track_count = 0;

  while (track) {
    ctx->track_count += 1;
    track = track->next;
  }

  r = ne_ctx_save(ctx, &ctx->saved);
  if (r != 0) {
    nestegg_destroy(ctx);
    return -1;
  }

  *context = ctx;

  return 0;
}

void
nestegg_destroy(nestegg * ctx)
{
  assert(ctx->ancestor == NULL);
  if (ctx->alloc_pool)
    ne_pool_destroy(ctx->alloc_pool);
  free(ctx->io);
  free(ctx);
}

int
nestegg_duration(nestegg * ctx, uint64_t * duration)
{
  uint64_t tc_scale;
  double unscaled_duration;

  if (ne_get_float(ctx->segment.info.duration, &unscaled_duration) != 0)
    return -1;

  tc_scale = ne_get_timecode_scale(ctx);
  if (tc_scale == 0)
    return -1;

  if (unscaled_duration != unscaled_duration ||
      unscaled_duration < 0 || unscaled_duration >= (double) UINT64_MAX ||
      (uint64_t) unscaled_duration > UINT64_MAX / tc_scale)
    return -1;

  *duration = (uint64_t) (unscaled_duration * tc_scale);
  return 0;
}

int
nestegg_tstamp_scale(nestegg * ctx, uint64_t * scale)
{
  *scale = ne_get_timecode_scale(ctx);
  if (*scale == 0)
    return -1;
  return 0;
}

int
nestegg_track_count(nestegg * ctx, unsigned int * tracks)
{
  *tracks = ctx->track_count;
  return 0;
}

int
nestegg_get_cue_point(nestegg * ctx, unsigned int cluster_num, int64_t max_offset,
                      int64_t * start_pos, int64_t * end_pos, uint64_t * tstamp)
{
  int range_obtained = 0;
  unsigned int cluster_count = 0;
  struct cue_point * cue_point;
  struct cue_track_positions * pos;
  uint64_t seek_pos, track_number, tc_scale, time;
  struct ebml_list_node * cues_node = ctx->segment.cues.cue_point.head;
  struct ebml_list_node * cue_pos_node = NULL;
  unsigned int track = 0, track_count = 0, track_index;

  if (!start_pos || !end_pos || !tstamp)
    return -1;

  /* Initialise return values */
  *start_pos = -1;
  *end_pos = -1;
  *tstamp = 0;

  if (!cues_node) {
    ne_init_cue_points(ctx, max_offset);
    cues_node = ctx->segment.cues.cue_point.head;
    /* Verify cues have been added to context. */
    if (!cues_node)
      return -1;
  }

  nestegg_track_count(ctx, &track_count);

  tc_scale = ne_get_timecode_scale(ctx);
  if (tc_scale == 0)
    return -1;

  while (cues_node && !range_obtained) {
    assert(cues_node->id == ID_CUE_POINT);
    cue_point = cues_node->data;
    cue_pos_node = cue_point->cue_track_positions.head;
    while (cue_pos_node) {
      assert(cue_pos_node->id == ID_CUE_TRACK_POSITIONS);
      pos = cue_pos_node->data;
      for (track = 0; track < track_count; ++track) {
        if (ne_get_uint(pos->track, &track_number) != 0)
          return -1;

        if (ne_map_track_number_to_index(ctx, track_number, &track_index) != 0)
          return -1;

        if (track_index == track) {
          if (ne_get_uint(pos->cluster_position, &seek_pos) != 0)
            return -1;
          if (cluster_count == cluster_num) {
            *start_pos = ctx->segment_offset + seek_pos;
            if (ne_get_uint(cue_point->time, &time) != 0)
              return -1;
            *tstamp = time * tc_scale;
          } else if (cluster_count == cluster_num + 1) {
            *end_pos = ctx->segment_offset + seek_pos - 1;
            range_obtained = 1;
            break;
          }
          cluster_count++;
        }
      }
      cue_pos_node = cue_pos_node->next;
    }
    cues_node = cues_node->next;
  }

  return 0;
}

int
nestegg_offset_seek(nestegg * ctx, uint64_t offset)
{
  int r;

  if (offset > INT64_MAX)
    return -1;

  /* Seek and set up parser state for segment-level element (Cluster). */
  r = ne_io_seek(ctx->io, offset, NESTEGG_SEEK_SET);
  if (r != 0)
    return -1;
  ctx->last_valid = 0;

  assert(ctx->ancestor == NULL);

  return 0;
}

int
nestegg_track_seek(nestegg * ctx, unsigned int track, uint64_t tstamp)
{
  int r;
  struct cue_point * cue_point;
  struct cue_track_positions * pos;
  uint64_t seek_pos, tc_scale;

  /* If there are no cues loaded, check for cues element in the seek head
     and load it. */
  if (!ctx->segment.cues.cue_point.head) {
    r = ne_init_cue_points(ctx, -1);
    if (r != 0)
      return -1;
  }

  tc_scale = ne_get_timecode_scale(ctx);
  if (tc_scale == 0)
    return -1;

  cue_point = ne_find_cue_point_for_tstamp(ctx, ctx->segment.cues.cue_point.head,
                                           track, tc_scale, tstamp);
  if (!cue_point)
    return -1;

  pos = ne_find_cue_position_for_track(ctx, cue_point->cue_track_positions.head, track);
  if (pos == NULL)
    return -1;

  if (ne_get_uint(pos->cluster_position, &seek_pos) != 0)
    return -1;

  /* Seek to (we assume) the start of a Cluster element. */
  r = nestegg_offset_seek(ctx, ctx->segment_offset + seek_pos);
  if (r != 0)
    return -1;

  return 0;
}

int
nestegg_track_type(nestegg * ctx, unsigned int track)
{
  struct track_entry * entry;
  uint64_t type;

  entry = ne_find_track_entry(ctx, track);
  if (!entry)
    return -1;

  if (ne_get_uint(entry->type, &type) != 0)
    return -1;

  if (type == TRACK_TYPE_VIDEO)
    return NESTEGG_TRACK_VIDEO;

  if (type == TRACK_TYPE_AUDIO)
    return NESTEGG_TRACK_AUDIO;

  return NESTEGG_TRACK_UNKNOWN;
}

int
nestegg_track_codec_id(nestegg * ctx, unsigned int track)
{
  char * codec_id;
  struct track_entry * entry;

  entry = ne_find_track_entry(ctx, track);
  if (!entry)
    return -1;

  if (ne_get_string(entry->codec_id, &codec_id) != 0)
    return -1;

  if (strcmp(codec_id, TRACK_ID_VP8) == 0)
    return NESTEGG_CODEC_VP8;

  if (strcmp(codec_id, TRACK_ID_VP9) == 0)
    return NESTEGG_CODEC_VP9;

  if (strcmp(codec_id, TRACK_ID_AV1) == 0)
    return NESTEGG_CODEC_AV1;

  if (strcmp(codec_id, TRACK_ID_VORBIS) == 0)
    return NESTEGG_CODEC_VORBIS;

  if (strcmp(codec_id, TRACK_ID_OPUS) == 0)
    return NESTEGG_CODEC_OPUS;

  return NESTEGG_CODEC_UNKNOWN;
}

int
nestegg_track_codec_data_count(nestegg * ctx, unsigned int track,
                               unsigned int * count)
{
  struct track_entry * entry;
  struct ebml_binary codec_private;
  int codec_id;
  unsigned char * p;

  *count = 0;

  entry = ne_find_track_entry(ctx, track);
  if (!entry)
    return -1;

  codec_id = nestegg_track_codec_id(ctx, track);

  if (codec_id == NESTEGG_CODEC_OPUS) {
    *count = 1;
    return 0;
  }

  if (codec_id != NESTEGG_CODEC_VORBIS)
    return -1;

  if (ne_get_binary(entry->codec_private, &codec_private) != 0)
    return -1;

  if (codec_private.length < 1)
    return -1;

  p = codec_private.data;
  *count = *p + 1;

  if (*count > 3)
    return -1;

  return 0;
}

int
nestegg_track_codec_data(nestegg * ctx, unsigned int track, unsigned int item,
                         unsigned char ** data, size_t * length)
{
  struct track_entry * entry;
  struct ebml_binary codec_private;

  *data = NULL;
  *length = 0;

  entry = ne_find_track_entry(ctx, track);
  if (!entry)
    return -1;

  if (nestegg_track_codec_id(ctx, track) != NESTEGG_CODEC_VORBIS &&
      nestegg_track_codec_id(ctx, track) != NESTEGG_CODEC_OPUS)
    return -1;

  if (ne_get_binary(entry->codec_private, &codec_private) != 0)
    return -1;

  if (nestegg_track_codec_id(ctx, track) == NESTEGG_CODEC_VORBIS) {
    uint64_t count;
    uint64_t sizes[3];
    size_t total;
    unsigned char * p;
    unsigned int i;
    int r;

    nestegg_io io;
    struct io_buffer userdata;
    userdata.buffer = codec_private.data;
    userdata.length = codec_private.length;
    userdata.offset = 0;

    io.read = ne_buffer_read;
    io.seek = ne_buffer_seek;
    io.tell = ne_buffer_tell;
    io.userdata = &userdata;

    total = 0;

    r = ne_read_uint(&io, &count, 1);
    if (r != 1)
      return r;
    total += 1;
    count += 1;

    if (count > 3)
      return -1;
    r = ne_read_xiph_lacing(&io, codec_private.length, &total, count, sizes);
    if (r != 1)
      return r;

    if (item >= count)
      return -1;

    p = codec_private.data + total;
    for (i = 0; i < item; ++i) {
      p += sizes[i];
    }
    assert((size_t) (p - codec_private.data) <= codec_private.length &&
           codec_private.length - (p - codec_private.data) >= sizes[item]);
    *data = p;
    *length = sizes[item];
  } else {
    if (item >= 1)
      return -1;

    *data = codec_private.data;
    *length = codec_private.length;
  }

  return 0;
}

int
nestegg_track_video_params(nestegg * ctx, unsigned int track,
                           nestegg_video_params * params)
{
  struct track_entry * entry;
  uint64_t value;
  double fvalue;

  memset(params, 0, sizeof(*params));

  entry = ne_find_track_entry(ctx, track);
  if (!entry)
    return -1;

  if (nestegg_track_type(ctx, track) != NESTEGG_TRACK_VIDEO)
    return -1;

  value = 0;
  ne_get_uint(entry->video.stereo_mode, &value);
  if (value <= NESTEGG_VIDEO_STEREO_TOP_BOTTOM ||
      value == NESTEGG_VIDEO_STEREO_RIGHT_LEFT)
    params->stereo_mode = value;

  value = 0;
  ne_get_uint(entry->video.alpha_mode, &value);
  params->alpha_mode = value;

  if (ne_get_uint(entry->video.pixel_width, &value) != 0)
    return -1;
  params->width = value;

  if (ne_get_uint(entry->video.pixel_height, &value) != 0)
    return -1;
  params->height = value;

  value = 0;
  ne_get_uint(entry->video.pixel_crop_bottom, &value);
  params->crop_bottom = value;

  value = 0;
  ne_get_uint(entry->video.pixel_crop_top, &value);
  params->crop_top = value;

  value = 0;
  ne_get_uint(entry->video.pixel_crop_left, &value);
  params->crop_left = value;

  value = 0;
  ne_get_uint(entry->video.pixel_crop_right, &value);
  params->crop_right = value;

  value = params->width;
  ne_get_uint(entry->video.display_width, &value);
  params->display_width = value;

  value = params->height;
  ne_get_uint(entry->video.display_height, &value);
  params->display_height = value;

  value = 2;
  ne_get_uint(entry->video.colour.matrix_coefficients, &value);
  params->matrix_coefficients = value;

  value = 0;
  ne_get_uint(entry->video.colour.range, &value);
  params->range = value;

  value = 2;
  ne_get_uint(entry->video.colour.transfer_characteristics, &value);
  params->transfer_characteristics = value;

  value = 2;
  ne_get_uint(entry->video.colour.primaries, &value);
  params->primaries = value;

  fvalue = strtod("NaN", NULL);
  ne_get_float(entry->video.colour.mastering_metadata.primary_r_chromacity_x, &fvalue);
  params->primary_r_chromacity_x = fvalue;

  fvalue = strtod("NaN", NULL);
  ne_get_float(entry->video.colour.mastering_metadata.primary_r_chromacity_y, &fvalue);
  params->primary_r_chromacity_y = fvalue;

  fvalue = strtod("NaN", NULL);
  ne_get_float(entry->video.colour.mastering_metadata.primary_g_chromacity_x, &fvalue);
  params->primary_g_chromacity_x = fvalue;

  fvalue = strtod("NaN", NULL);
  ne_get_float(entry->video.colour.mastering_metadata.primary_g_chromacity_y, &fvalue);
  params->primary_g_chromacity_y = fvalue;

  fvalue = strtod("NaN", NULL);
  ne_get_float(entry->video.colour.mastering_metadata.primary_b_chromacity_x, &fvalue);
  params->primary_b_chromacity_x = fvalue;

  fvalue = strtod("NaN", NULL);
  ne_get_float(entry->video.colour.mastering_metadata.primary_b_chromacity_y, &fvalue);
  params->primary_b_chromacity_y = fvalue;

  fvalue = strtod("NaN", NULL);
  ne_get_float(entry->video.colour.mastering_metadata.white_point_chromaticity_x, &fvalue);
  params->white_point_chromaticity_x = fvalue;

  fvalue = strtod("NaN", NULL);
  ne_get_float(entry->video.colour.mastering_metadata.white_point_chromaticity_y, &fvalue);
  params->white_point_chromaticity_y = fvalue;

  fvalue = strtod("NaN", NULL);
  ne_get_float(entry->video.colour.mastering_metadata.luminance_max, &fvalue);
  params->luminance_max = fvalue;

  fvalue = strtod("NaN", NULL);
  ne_get_float(entry->video.colour.mastering_metadata.luminance_min, &fvalue);
  params->luminance_min = fvalue;

  return 0;
}

int
nestegg_track_audio_params(nestegg * ctx, unsigned int track,
                           nestegg_audio_params * params)
{
  struct track_entry * entry;
  uint64_t value;

  memset(params, 0, sizeof(*params));

  entry = ne_find_track_entry(ctx, track);
  if (!entry)
    return -1;

  if (nestegg_track_type(ctx, track) != NESTEGG_TRACK_AUDIO)
    return -1;

  params->rate = 8000;
  ne_get_float(entry->audio.sampling_frequency, &params->rate);

  value = 1;
  ne_get_uint(entry->audio.channels, &value);
  params->channels = value;

  value = 16;
  ne_get_uint(entry->audio.bit_depth, &value);
  params->depth = value;

  value = 0;
  ne_get_uint(entry->codec_delay, &value);
  params->codec_delay = value;

  value = 0;
  ne_get_uint(entry->seek_preroll, &value);
  params->seek_preroll = value;

  return 0;
}

int
nestegg_track_encoding(nestegg * ctx, unsigned int track)
{
  struct track_entry * entry;
  struct content_encoding * encoding;
  uint64_t encoding_value;

  entry = ne_find_track_entry(ctx, track);
  if (!entry) {
    ctx->log(ctx, NESTEGG_LOG_ERROR, "No track entry found");
    return -1;
  }

  if (!entry->content_encodings.content_encoding.head) {
    /* Default encoding is compression */
    return NESTEGG_ENCODING_COMPRESSION;
  }

  encoding = entry->content_encodings.content_encoding.head->data;

  encoding_value = NESTEGG_ENCODING_COMPRESSION;
  ne_get_uint(encoding->content_encoding_type, &encoding_value);
  if (encoding_value != NESTEGG_ENCODING_COMPRESSION && encoding_value != NESTEGG_ENCODING_ENCRYPTION) {
    ctx->log(ctx, NESTEGG_LOG_ERROR, "Invalid ContentEncoding element found");
    return -1;
  }

  return encoding_value;
}

int
nestegg_track_content_enc_key_id(nestegg * ctx, unsigned int track, unsigned char const ** content_enc_key_id,
                                 size_t * content_enc_key_id_length)
{
  struct track_entry * entry;
  struct content_encoding * encoding;
  struct content_encryption * encryption;
  struct content_enc_aes_settings * aes_settings;
  struct nestegg_encryption_params;
  uint64_t value;
  struct ebml_binary enc_key_id;

  entry = ne_find_track_entry(ctx, track);
  if (!entry) {
    ctx->log(ctx, NESTEGG_LOG_ERROR, "No track entry found");
    return -1;
  }

  if (!entry->content_encodings.content_encoding.head) {
    ctx->log(ctx, NESTEGG_LOG_ERROR, "No ContentEncoding element found");
    return -1;
  }

  encoding = entry->content_encodings.content_encoding.head->data;

  value = 0;
  ne_get_uint(encoding->content_encoding_type, &value);
  if (value != NESTEGG_ENCODING_ENCRYPTION) {
    ctx->log(ctx, NESTEGG_LOG_ERROR, "Disallowed ContentEncodingType found");
    return -1;
  }

  if (!encoding->content_encryption.head) {
    ctx->log(ctx, NESTEGG_LOG_ERROR, "No ContentEncryption element found");
    return -1;
  }

  encryption = encoding->content_encryption.head->data;

  value = 0;
  ne_get_uint(encryption->content_enc_algo, &value);

  if (value != CONTENT_ENC_ALGO_AES) {
    ctx->log(ctx, NESTEGG_LOG_ERROR, "Disallowed ContentEncAlgo found");
    return -1;
  }

  if (!encryption->content_enc_aes_settings.head) {
    ctx->log(ctx, NESTEGG_LOG_ERROR, "No ContentEncAesSettings element found");
    return -1;
  }

  aes_settings = encryption->content_enc_aes_settings.head->data;
  value = AES_SETTINGS_CIPHER_CTR;
  ne_get_uint(aes_settings->aes_settings_cipher_mode, &value);

  if (value != AES_SETTINGS_CIPHER_CTR) {
    ctx->log(ctx, NESTEGG_LOG_ERROR, "Disallowed AESSettingCipherMode used");
    return -1;
  }

  if (ne_get_binary(encryption->content_enc_key_id, &enc_key_id) != 0) {
    ctx->log(ctx, NESTEGG_LOG_ERROR, "Could not retrieve track ContentEncKeyId");
    return -1;
  }

  *content_enc_key_id = enc_key_id.data;
  *content_enc_key_id_length = enc_key_id.length;

  return 0;
}

int
nestegg_track_default_duration(nestegg * ctx, unsigned int track,
                               uint64_t * duration)
{
  struct track_entry * entry;
  uint64_t value;

  entry = ne_find_track_entry(ctx, track);
  if (!entry)
    return -1;

  if (ne_get_uint(entry->default_duration, &value) != 0)
    return -1;
  *duration = value;

  return 0;
}

int
nestegg_read_reset(nestegg * ctx)
{
  assert(ctx->ancestor == NULL);
  return ne_ctx_restore(ctx, &ctx->saved);
}

int
nestegg_read_packet(nestegg * ctx, nestegg_packet ** pkt)
{
  int r, read_block = 0;
  uint64_t id, size;

  *pkt = NULL;

  assert(ctx->ancestor == NULL);

  /* Prepare for read_reset to resume parsing from this point upon error. */
  r = ne_ctx_save(ctx, &ctx->saved);
  if (r != 0)
    return -1;

  while (!read_block) {
    r = ne_read_element(ctx, &id, &size);
    if (r != 1)
      return r;

    switch (id) {
    case ID_CLUSTER: {
      r = ne_read_element(ctx, &id, &size);
      if (r != 1)
        return r;

      /* Matroska may place a CRC32 before the Timecode. Skip and continue parsing. */
      if (id == ID_CRC32) {
        r = ne_io_read_skip(ctx->io, size);
        if (r != 1)
          return r;

        r = ne_read_element(ctx, &id, &size);
        if (r != 1)
          return r;
      }

      /* Timecode must be the first element in a Cluster, per WebM spec. */
      if (id != ID_TIMECODE)
        return -1;

      r = ne_read_uint(ctx->io, &ctx->cluster_timecode, size);
      if (r != 1)
        return r;
      ctx->read_cluster_timecode = 1;
      break;
    }
    case ID_SIMPLE_BLOCK:
      r = ne_read_block(ctx, id, size, pkt);
      if (r != 1)
        return r;

      read_block = 1;
      break;
    case ID_BLOCK_GROUP: {
      int64_t block_group_end;
      uint64_t block_duration = 0;
      int read_block_duration = 0;
      int64_t discard_padding = 0;
      int read_discard_padding = 0;
      int64_t reference_block = 0;
      int read_reference_block = 0;
      struct block_additional * block_additional = NULL;
      uint64_t tc_scale;

      block_group_end = ne_io_tell(ctx->io) + size;

      /* Read the entire BlockGroup manually. */
      while (ne_io_tell(ctx->io) < block_group_end) {
        r = ne_read_element(ctx, &id, &size);
        if (r != 1) {
          ne_free_block_additions(block_additional);
          if (*pkt) {
            nestegg_free_packet(*pkt);
            *pkt = NULL;
          }
          return r;
        }

        switch (id) {
        case ID_BLOCK: {
          if (*pkt) {
            ctx->log(ctx, NESTEGG_LOG_DEBUG,
                     "read_packet: multiple Blocks in BlockGroup, dropping previously read Block");
            nestegg_free_packet(*pkt);
          }
          r = ne_read_block(ctx, id, size, pkt);
          if (r != 1) {
            ne_free_block_additions(block_additional);
            if (*pkt) {
              nestegg_free_packet(*pkt);
              *pkt = NULL;
            }
            return r;
          }

          read_block = 1;
          break;
        }
        case ID_BLOCK_DURATION: {
          r = ne_read_uint(ctx->io, &block_duration, size);
          if (r != 1) {
            ne_free_block_additions(block_additional);
            if (*pkt) {
              nestegg_free_packet(*pkt);
              *pkt = NULL;
            }
            return r;
          }
          tc_scale = ne_get_timecode_scale(ctx);
          if (tc_scale == 0) {
            ne_free_block_additions(block_additional);
            if (*pkt) {
              nestegg_free_packet(*pkt);
              *pkt = NULL;
            }
            return -1;
          }
          block_duration *= tc_scale;
          read_block_duration = 1;
          break;
        }
        case ID_DISCARD_PADDING: {
          r = ne_read_int(ctx->io, &discard_padding, size);
          if (r != 1) {
            ne_free_block_additions(block_additional);
            if (*pkt) {
              nestegg_free_packet(*pkt);
              *pkt = NULL;
            }
            return r;
          }
          read_discard_padding = 1;
          break;
        }
        case ID_BLOCK_ADDITIONS: {
          /* There should only be one BlockAdditions; treat multiple as an error. */
          if (block_additional) {
            ne_free_block_additions(block_additional);
            if (*pkt) {
              nestegg_free_packet(*pkt);
              *pkt = NULL;
            }
            return -1;
          }
          r = ne_read_block_additions(ctx, size, &block_additional);
          if (r != 1) {
            ne_free_block_additions(block_additional);
            if (*pkt) {
              nestegg_free_packet(*pkt);
              *pkt = NULL;
            }
            return r;
          }
          break;
        }
        case ID_REFERENCE_BLOCK: {
          r = ne_read_int(ctx->io, &reference_block, size);
          if (r != 1) {
            ne_free_block_additions(block_additional);
            if (*pkt) {
              nestegg_free_packet(*pkt);
              *pkt = NULL;
            }
            return r;
          }
          read_reference_block = 1;
          break;
        }
        default:
          /* We don't know what this element is, so skip over it */
          if (id != ID_VOID && id != ID_CRC32)
            ctx->log(ctx, NESTEGG_LOG_DEBUG,
                     "read_packet: unknown element %llx in BlockGroup", id);
          r = ne_io_read_skip(ctx->io, size);
          if (r != 1) {
            ne_free_block_additions(block_additional);
            if (*pkt) {
              nestegg_free_packet(*pkt);
              *pkt = NULL;
            }
            return r;
          }
        }
      }

      assert(read_block == (*pkt != NULL));
      if (*pkt) {
        (*pkt)->duration = block_duration;
        (*pkt)->read_duration = read_block_duration;
        (*pkt)->discard_padding = discard_padding;
        (*pkt)->read_discard_padding = read_discard_padding;
        (*pkt)->reference_block = reference_block;
        (*pkt)->read_reference_block = read_reference_block;
        (*pkt)->block_additional = block_additional;
        if ((*pkt)->read_reference_block)
          /* If a packet has a reference block it contains
             predictive frames and no keyframes */
          (*pkt)->keyframe = NESTEGG_PACKET_HAS_KEYFRAME_FALSE;
      } else {
        ne_free_block_additions(block_additional);
      }
      break;
    }
    default:
      ctx->log(ctx, NESTEGG_LOG_DEBUG, "read_packet: unknown element %llx", id);
      r = ne_io_read_skip(ctx->io, size);
      if (r != 1)
        return r;
    }
  }

  return 1;
}

void
nestegg_free_packet(nestegg_packet * pkt)
{
  struct frame * frame;

  while (pkt->frame) {
    frame = pkt->frame;
    pkt->frame = frame->next;

    ne_free_frame(frame);
  }

  ne_free_block_additions(pkt->block_additional);

  free(pkt);
}

int
nestegg_packet_has_keyframe(nestegg_packet * pkt)
{
  return pkt->keyframe;
}

int
nestegg_packet_track(nestegg_packet * pkt, unsigned int * track)
{
  *track = pkt->track;
  return 0;
}

int
nestegg_packet_tstamp(nestegg_packet * pkt, uint64_t * tstamp)
{
  *tstamp = pkt->timecode;
  return 0;
}

int
nestegg_packet_duration(nestegg_packet * pkt, uint64_t * duration)
{
  if (!pkt->read_duration)
    return -1;
  *duration = pkt->duration;
  return 0;
}

int
nestegg_packet_discard_padding(nestegg_packet * pkt, int64_t * discard_padding)
{
  if (!pkt->read_discard_padding)
    return -1;
  *discard_padding = pkt->discard_padding;
  return 0;
}

int
nestegg_packet_reference_block(nestegg_packet * pkt, int64_t * reference_block)
{
  if (!pkt->read_reference_block)
    return -1;
  *reference_block = pkt->reference_block;
  return 0;
}

int
nestegg_packet_count(nestegg_packet * pkt, unsigned int * count)
{
  struct frame * f = pkt->frame;

  *count = 0;

  while (f) {
    *count += 1;
    f = f->next;
  }

  return 0;
}

int
nestegg_packet_data(nestegg_packet * pkt, unsigned int item,
                    unsigned char ** data, size_t * length)
{
  struct frame * f = pkt->frame;
  unsigned int count = 0;

  *data = NULL;
  *length = 0;

  while (f) {
    if (count == item) {
      *data = f->data;
      *length = f->length;
      return 0;
    }
    count += 1;
    f = f->next;
  }

  return -1;
}

int
nestegg_packet_additional_data(nestegg_packet * pkt, unsigned int id,
                               unsigned char ** data, size_t * length)
{
  struct block_additional * a = pkt->block_additional;

  *data = NULL;
  *length = 0;

  while (a) {
    if (a->id == id) {
      *data = a->data;
      *length = a->length;
      return 0;
    }
    a = a->next;
  }

  return -1;
}

int
nestegg_packet_encryption(nestegg_packet * pkt)
{
  struct frame * f = pkt->frame;
  unsigned char encrypted_bit;
  unsigned char partitioned_bit;

  if (!f->frame_encryption)
    return NESTEGG_PACKET_HAS_SIGNAL_BYTE_FALSE;

  /* Should never have parsed blocks with both encryption and lacing */
  assert(f->next == NULL);

  encrypted_bit = f->frame_encryption->signal_byte & ENCRYPTED_BIT_MASK;
  partitioned_bit = f->frame_encryption->signal_byte & PARTITIONED_BIT_MASK;

  if (encrypted_bit != PACKET_ENCRYPTED)
    return NESTEGG_PACKET_HAS_SIGNAL_BYTE_UNENCRYPTED;

  if (partitioned_bit == PACKET_PARTITIONED)
    return NESTEGG_PACKET_HAS_SIGNAL_BYTE_PARTITIONED;

  return NESTEGG_PACKET_HAS_SIGNAL_BYTE_ENCRYPTED;
}

int
nestegg_packet_iv(nestegg_packet * pkt, unsigned char const ** iv, size_t * length)
{
  struct frame * f = pkt->frame;
  unsigned char encrypted_bit;

  *iv = NULL;
  *length = 0;
  if (!f->frame_encryption)
    return -1;

  /* Should never have parsed blocks with both encryption and lacing */
  assert(f->next == NULL);

  encrypted_bit = f->frame_encryption->signal_byte & ENCRYPTED_BIT_MASK;

  if (encrypted_bit != PACKET_ENCRYPTED)
    return 0;

  *iv = f->frame_encryption->iv;
  *length = f->frame_encryption->length;
  return 0;
}

int
nestegg_packet_offsets(nestegg_packet * pkt,
                       uint32_t const ** partition_offsets,
                       uint8_t * num_partitions)
{
  struct frame * f = pkt->frame;
  unsigned char encrypted_bit;
  unsigned char partitioned_bit;

  *partition_offsets = NULL;
  *num_partitions = 0;

  if (!f->frame_encryption)
    return -1;

  /* Should never have parsed blocks with both encryption and lacing */
  assert(f->next == NULL);

  encrypted_bit = f->frame_encryption->signal_byte & ENCRYPTED_BIT_MASK;
  partitioned_bit = f->frame_encryption->signal_byte & PARTITIONED_BIT_MASK;

  if (encrypted_bit != PACKET_ENCRYPTED || partitioned_bit != PACKET_PARTITIONED)
    return -1;

  *num_partitions = f->frame_encryption->num_partitions;
  *partition_offsets = f->frame_encryption->partition_offsets;
  return 0;
}

int
nestegg_has_cues(nestegg * ctx)
{
  return ctx->segment.cues.cue_point.head ||
    ne_find_seek_for_id(ctx->segment.seek_head.head, ID_CUES);
}

int
nestegg_sniff(unsigned char const * buffer, size_t length)
{
  nestegg_io io;
  struct io_buffer userdata;

  userdata.buffer = buffer;
  userdata.length = length;
  userdata.offset = 0;

  io.read = ne_buffer_read;
  io.seek = ne_buffer_seek;
  io.tell = ne_buffer_tell;
  io.userdata = &userdata;
  return ne_match_webm(io, length);
}
