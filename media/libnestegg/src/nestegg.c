/*
 * Copyright © 2010 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "halloc.h"
#include "nestegg/nestegg.h"

/* EBML Elements */
#define ID_EBML                 0x1a45dfa3
#define ID_EBML_VERSION         0x4286
#define ID_EBML_READ_VERSION    0x42f7
#define ID_EBML_MAX_ID_LENGTH   0x42f2
#define ID_EBML_MAX_SIZE_LENGTH 0x42f3
#define ID_DOCTYPE              0x4282
#define ID_DOCTYPE_VERSION      0x4287
#define ID_DOCTYPE_READ_VERSION 0x4285

/* Global Elements */
#define ID_VOID                 0xec
#define ID_CRC32                0xbf

/* WebM Elements */
#define ID_SEGMENT              0x18538067

/* Seek Head Elements */
#define ID_SEEK_HEAD            0x114d9b74
#define ID_SEEK                 0x4dbb
#define ID_SEEK_ID              0x53ab
#define ID_SEEK_POSITION        0x53ac

/* Info Elements */
#define ID_INFO                 0x1549a966
#define ID_TIMECODE_SCALE       0x2ad7b1
#define ID_DURATION             0x4489

/* Cluster Elements */
#define ID_CLUSTER              0x1f43b675
#define ID_TIMECODE             0xe7
#define ID_BLOCK_GROUP          0xa0
#define ID_SIMPLE_BLOCK         0xa3

/* BlockGroup Elements */
#define ID_BLOCK                0xa1
#define ID_BLOCK_ADDITIONS      0x75a1
#define ID_BLOCK_DURATION       0x9b
#define ID_REFERENCE_BLOCK      0xfb
#define ID_DISCARD_PADDING      0x75a2

/* BlockAdditions Elements */
#define ID_BLOCK_MORE           0xa6

/* BlockMore Elements */
#define ID_BLOCK_ADD_ID         0xee
#define ID_BLOCK_ADDITIONAL     0xa5

/* Tracks Elements */
#define ID_TRACKS               0x1654ae6b
#define ID_TRACK_ENTRY          0xae
#define ID_TRACK_NUMBER         0xd7
#define ID_TRACK_UID            0x73c5
#define ID_TRACK_TYPE           0x83
#define ID_FLAG_ENABLED         0xb9
#define ID_FLAG_DEFAULT         0x88
#define ID_FLAG_LACING          0x9c
#define ID_TRACK_TIMECODE_SCALE 0x23314f
#define ID_LANGUAGE             0x22b59c
#define ID_CODEC_ID             0x86
#define ID_CODEC_PRIVATE        0x63a2
#define ID_CODEC_DELAY          0x56aa
#define ID_SEEK_PREROLL         0x56bb
#define ID_DEFAULT_DURATION     0x23e383

/* Video Elements */
#define ID_VIDEO                0xe0
#define ID_STEREO_MODE          0x53b8
#define ID_ALPHA_MODE           0x53c0
#define ID_PIXEL_WIDTH          0xb0
#define ID_PIXEL_HEIGHT         0xba
#define ID_PIXEL_CROP_BOTTOM    0x54aa
#define ID_PIXEL_CROP_TOP       0x54bb
#define ID_PIXEL_CROP_LEFT      0x54cc
#define ID_PIXEL_CROP_RIGHT     0x54dd
#define ID_DISPLAY_WIDTH        0x54b0
#define ID_DISPLAY_HEIGHT       0x54ba

/* Audio Elements */
#define ID_AUDIO                0xe1
#define ID_SAMPLING_FREQUENCY   0xb5
#define ID_CHANNELS             0x9f
#define ID_BIT_DEPTH            0x6264

/* Cues Elements */
#define ID_CUES                 0x1c53bb6b
#define ID_CUE_POINT            0xbb
#define ID_CUE_TIME             0xb3
#define ID_CUE_TRACK_POSITIONS  0xb7
#define ID_CUE_TRACK            0xf7
#define ID_CUE_CLUSTER_POSITION 0xf1
#define ID_CUE_BLOCK_NUMBER     0x5378

/* EBML Types */
enum ebml_type_enum {
  TYPE_UNKNOWN,
  TYPE_MASTER,
  TYPE_UINT,
  TYPE_FLOAT,
  TYPE_INT,
  TYPE_STRING,
  TYPE_BINARY
};

#define LIMIT_STRING            (1 << 20)
#define LIMIT_BINARY            (1 << 24)
#define LIMIT_BLOCK             (1 << 30)
#define LIMIT_FRAME             (1 << 28)

/* Field Flags */
#define DESC_FLAG_NONE          0
#define DESC_FLAG_MULTI         (1 << 0)
#define DESC_FLAG_SUSPEND       (1 << 1)
#define DESC_FLAG_OFFSET        (1 << 2)

/* Block Header Flags */
#define BLOCK_FLAGS_LACING      6

/* Lacing Constants */
#define LACING_NONE             0
#define LACING_XIPH             1
#define LACING_FIXED            2
#define LACING_EBML             3

/* Track Types */
#define TRACK_TYPE_VIDEO        1
#define TRACK_TYPE_AUDIO        2

/* Track IDs */
#define TRACK_ID_VP8            "V_VP8"
#define TRACK_ID_VP9            "V_VP9"
#define TRACK_ID_VORBIS         "A_VORBIS"
#define TRACK_ID_OPUS           "A_OPUS"

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

struct block_more {
  struct ebml_type block_add_id;
  struct ebml_type block_additional;
};

struct block_additions {
  struct ebml_list block_more;
};

struct block_group {
  struct ebml_type block_additions;
  struct ebml_type duration;
  struct ebml_type reference_block;
  struct ebml_type discard_padding;
};

struct cluster {
  struct ebml_type timecode;
  struct ebml_list block_group;
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
};

struct audio {
  struct ebml_type sampling_frequency;
  struct ebml_type channels;
  struct ebml_type bit_depth;
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
  struct ebml_list cluster;
  struct tracks tracks;
  struct cues cues;
};

/* Misc. */
struct pool_ctx {
  char dummy;
};

struct list_node {
  struct list_node * previous;
  struct ebml_element_desc * node;
  unsigned char * data;
};

struct saved_state {
  int64_t stream_offset;
  struct list_node * ancestor;
  uint64_t last_id;
  uint64_t last_size;
  int last_valid;
};

struct frame {
  unsigned char * data;
  size_t length;
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
};

struct nestegg_packet {
  uint64_t track;
  uint64_t timecode;
  uint64_t duration;
  struct frame * frame;
  struct block_additional * block_additional;
  int64_t discard_padding;
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

static struct ebml_element_desc ne_block_more_elements[] = {
  E_FIELD(ID_BLOCK_ADD_ID, TYPE_UINT, struct block_more, block_add_id),
  E_FIELD(ID_BLOCK_ADDITIONAL, TYPE_BINARY, struct block_more, block_additional),
  E_LAST
};

static struct ebml_element_desc ne_block_additions_elements[] = {
  E_MASTER(ID_BLOCK_MORE, TYPE_MASTER, struct block_additions, block_more),
  E_LAST
};

static struct ebml_element_desc ne_block_group_elements[] = {
  E_SUSPEND(ID_BLOCK, TYPE_BINARY),
  E_FIELD(ID_BLOCK_DURATION, TYPE_UINT, struct block_group, duration),
  E_FIELD(ID_REFERENCE_BLOCK, TYPE_INT, struct block_group, reference_block),
  E_FIELD(ID_DISCARD_PADDING, TYPE_INT, struct block_group, discard_padding),
  E_SINGLE_MASTER(ID_BLOCK_ADDITIONS, TYPE_MASTER, struct block_group, block_additions),
  E_LAST
};

static struct ebml_element_desc ne_cluster_elements[] = {
  E_FIELD(ID_TIMECODE, TYPE_UINT, struct cluster, timecode),
  E_MASTER(ID_BLOCK_GROUP, TYPE_MASTER, struct cluster, block_group),
  E_SUSPEND(ID_SIMPLE_BLOCK, TYPE_BINARY),
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
  E_LAST
};

static struct ebml_element_desc ne_audio_elements[] = {
  E_FIELD(ID_SAMPLING_FREQUENCY, TYPE_FLOAT, struct audio, sampling_frequency),
  E_FIELD(ID_CHANNELS, TYPE_UINT, struct audio, channels),
  E_FIELD(ID_BIT_DEPTH, TYPE_UINT, struct audio, bit_depth),
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
  E_MASTER(ID_CLUSTER, TYPE_MASTER, struct segment, cluster),
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
  return h_malloc(sizeof(struct pool_ctx));
}

static void
ne_pool_destroy(struct pool_ctx * pool)
{
  h_free(pool);
}

static void *
ne_pool_alloc(size_t size, struct pool_ctx * pool)
{
  void * p;

  p = h_malloc(size);
  if (!p)
    return NULL;
  hattach(p, pool);
  memset(p, 0, size);
  return p;
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
    float f;
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
    *val = value.f;
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
  s->ancestor = ctx->ancestor;
  s->last_id = ctx->last_id;
  s->last_size = ctx->last_size;
  s->last_valid = ctx->last_valid;
  return 0;
}

static int
ne_ctx_restore(nestegg * ctx, struct saved_state * s)
{
  int r;

  r = ne_io_seek(ctx->io, s->stream_offset, NESTEGG_SEEK_SET);
  if (r != 0)
    return -1;
  ctx->ancestor = s->ancestor;
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
  int r;

  storage = (struct ebml_type *) (ctx->ancestor->data + desc->offset);

  if (storage->read) {
    ctx->log(ctx, NESTEGG_LOG_DEBUG, "element %llx (%s) already read, skipping",
             desc->id, desc->name);
    return 0;
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
  case TYPE_INT:
    r = ne_read_int(ctx->io, &storage->v.i, length);
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
    r = 0;
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

  if (!ctx->ancestor)
    return -1;

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
        assert(element->type == TYPE_BINARY);
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

static uint64_t
ne_xiph_lace_value(unsigned char ** np)
{
  uint64_t lace;
  uint64_t value;
  unsigned char * p = *np;

  lace = *p++;
  value = lace;
  while (lace == 255) {
    lace = *p++;
    value += lace;
  }

  *np = p;

  return value;
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

static int
ne_read_block(nestegg * ctx, uint64_t block_id, uint64_t block_size, nestegg_packet ** data)
{
  int r;
  int64_t timecode, abs_timecode;
  nestegg_packet * pkt;
  struct cluster * cluster;
  struct frame * f, * last;
  struct track_entry * entry;
  double track_scale;
  uint64_t track_number, length, frame_sizes[256], cluster_tc, flags, frames, tc_scale, total;
  unsigned int i, lacing, track;
  size_t consumed = 0;

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

  track_scale = 1.0;

  tc_scale = ne_get_timecode_scale(ctx);

  assert(ctx->segment.cluster.tail->id == ID_CLUSTER);
  cluster = ctx->segment.cluster.tail->data;
  if (ne_get_uint(cluster->timecode, &cluster_tc) != 0)
    return -1;

  abs_timecode = timecode + cluster_tc;
  if (abs_timecode < 0)
    return -1;

  pkt = ne_alloc(sizeof(*pkt));
  if (!pkt)
    return -1;
  pkt->track = track;
  pkt->timecode = abs_timecode * tc_scale * track_scale;

  ctx->log(ctx, NESTEGG_LOG_DEBUG, "%sblock t %lld pts %f f %llx frames: %llu",
           block_id == ID_BLOCK ? "" : "simple", pkt->track, pkt->timecode / 1e9, flags, frames);

  last = NULL;
  for (i = 0; i < frames; ++i) {
    if (frame_sizes[i] > LIMIT_FRAME) {
      nestegg_free_packet(pkt);
      return -1;
    }
    f = ne_alloc(sizeof(*f));
    if (!f) {
      nestegg_free_packet(pkt);
      return -1;
    }
    f->data = ne_alloc(frame_sizes[i]);
    if (!f->data) {
      free(f);
      nestegg_free_packet(pkt);
      return -1;
    }
    f->length = frame_sizes[i];
    r = ne_io_read(ctx->io, f->data, frame_sizes[i]);
    if (r != 1) {
      free(f->data);
      free(f);
      nestegg_free_packet(pkt);
      return -1;
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
ne_read_block_duration(nestegg * ctx, nestegg_packet * pkt)
{
  int r;
  uint64_t id, size;
  struct ebml_element_desc * element;
  struct ebml_type * storage;

  r = ne_peek_element(ctx, &id, &size);
  if (r != 1)
    return r;

  if (id != ID_BLOCK_DURATION)
    return 1;

  element = ne_find_element(id, ctx->ancestor->node);
  if (!element)
    return 1;

  r = ne_read_simple(ctx, element, size);
  if (r != 1)
    return r;

  storage = (struct ebml_type *) (ctx->ancestor->data + element->offset);
  pkt->duration = storage->v.i * ne_get_timecode_scale(ctx);

  return 1;
}

static int
ne_read_discard_padding(nestegg * ctx, nestegg_packet * pkt)
{
  int r;
  uint64_t id, size;
  struct ebml_element_desc * element;
  struct ebml_type * storage;

  r = ne_peek_element(ctx, &id, &size);
  if (r != 1)
    return r;

  if (id != ID_DISCARD_PADDING)
    return 1;

  element = ne_find_element(id, ctx->ancestor->node);
  if (!element)
    return 1;

  r = ne_read_simple(ctx, element, size);
  if (r != 1)
    return r;

  storage = (struct ebml_type *) (ctx->ancestor->data + element->offset);
  pkt->discard_padding = storage->v.i;

  return 1;
}

static int
ne_read_block_additions(nestegg * ctx, nestegg_packet * pkt)
{
  int r;
  uint64_t id, size, data_size;
  int64_t block_additions_end, block_more_end;
  void * data;
  int has_data;
  struct block_additional * block_additional;
  uint64_t add_id;

  assert(pkt != NULL);
  assert(pkt->block_additional == NULL);

  r = ne_peek_element(ctx, &id, &size);
  if (r != 1)
    return r;

  if (id != ID_BLOCK_ADDITIONS)
    return 1;

  /* This makes ne_read_element read the next element instead of returning
     information about the already "peeked" one. */
  ctx->last_valid = 0;

  block_additions_end = ne_io_tell(ctx->io) + size;

  while (ne_io_tell(ctx->io) < block_additions_end) {
    add_id = 1;
    data = NULL;
    has_data = 0;
    r = ne_read_element(ctx, &id, &size);
    if (r != 1)
      return -1;

    if (id != ID_BLOCK_MORE) {
      /* We don't know what this element is, so skip over it */
      if (id != ID_VOID && id != ID_CRC32)
        ctx->log(ctx, NESTEGG_LOG_DEBUG,
                 "unknown element %llx in BlockAdditions", id);
      ne_io_read_skip(ctx->io, size);
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
        if (size != 0) {
          data = ne_alloc(size);
          r = ne_io_read(ctx->io, data, size);
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
        ne_io_read_skip(ctx->io, size);
      }
    }

    if (has_data == 0) {
      ctx->log(ctx, NESTEGG_LOG_ERROR,
               "No BlockAdditional element in a BlockMore");
      return -1;
    }

    block_additional = ne_alloc(sizeof(*block_additional));
    block_additional->next = pkt->block_additional;
    block_additional->id = add_id;
    block_additional->data = data;
    block_additional->length = data_size;
    pkt->block_additional = block_additional;
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

static int
ne_is_suspend_element(uint64_t id)
{
  if (id == ID_SIMPLE_BLOCK || id == ID_BLOCK)
    return 1;
  return 0;
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

    ctx->ancestor = NULL;
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
 * sniff_buffer. */
struct sniff_buffer {
  unsigned char const * buffer;
  size_t length;
  int64_t offset;
};

static int
ne_buffer_read(void * buffer, size_t length, void * userdata)
{
  struct sniff_buffer * sb = userdata;

  int rv = 1;
  size_t available = sb->length - sb->offset;

  if (available < length)
    return 0;

  memcpy(buffer, sb->buffer + sb->offset, length);
  sb->offset += length;

  return rv;
}

static int
ne_buffer_seek(int64_t offset, int whence, void * userdata)
{
  struct sniff_buffer * sb = userdata;
  int64_t o = sb->offset;

  switch(whence) {
  case NESTEGG_SEEK_SET:
    o = offset;
    break;
  case NESTEGG_SEEK_CUR:
    o += offset;
    break;
  case NESTEGG_SEEK_END:
    o = sb->length + offset;
    break;
  }

  if (o < 0 || o > (int64_t) sb->length)
    return -1;

  sb->offset = o;
  return 0;
}

static int64_t
ne_buffer_tell(void * userdata)
{
  struct sniff_buffer * sb = userdata;
  return sb->offset;
}

static int
ne_match_webm(nestegg_io io, int64_t max_offset)
{
  int r;
  uint64_t id;
  char * doctype;
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
  ctx->alloc_pool = ne_pool_init();
  if (!ctx->alloc_pool) {
    nestegg_destroy(ctx);
    return -1;
  }
  ctx->log = ne_null_log_callback;

  r = ne_peek_element(ctx, &id, NULL);
  if (r != 1) {
    nestegg_destroy(ctx);
    return 0;
  }

  if (id != ID_EBML) {
    nestegg_destroy(ctx);
    return 0;
  }

  ne_ctx_push(ctx, ne_top_level_elements, ctx);

  /* we don't check the return value of ne_parse, that might fail because
     max_offset is not on a valid element end point. We only want to check
     the EBML ID and that the doctype is "webm". */
  ne_parse(ctx, NULL, max_offset);

  if (ne_get_string(ctx->ebml.doctype, &doctype) != 0 ||
      strcmp(doctype, "webm") != 0) {
    nestegg_destroy(ctx);
    return 0;
  }

  nestegg_destroy(ctx);

  return 1;
}

int
nestegg_init(nestegg ** context, nestegg_io io, nestegg_log callback, int64_t max_offset)
{
  int r;
  uint64_t id, version, docversion;
  struct ebml_list_node * track;
  char * doctype;
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

  ne_ctx_push(ctx, ne_top_level_elements, ctx);

  r = ne_parse(ctx, NULL, max_offset);

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
  if (strcmp(doctype, "webm") != 0) {
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

  *context = ctx;

  return 0;
}

void
nestegg_destroy(nestegg * ctx)
{
  while (ctx->ancestor)
    ne_ctx_pop(ctx);
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

  *duration = (uint64_t) (unscaled_duration * tc_scale);
  return 0;
}

int
nestegg_tstamp_scale(nestegg * ctx, uint64_t * scale)
{
  *scale = ne_get_timecode_scale(ctx);
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

  /* Seek and set up parser state for segment-level element (Cluster). */
  r = ne_io_seek(ctx->io, offset, NESTEGG_SEEK_SET);
  if (r != 0)
    return -1;
  ctx->last_valid = 0;

  while (ctx->ancestor)
    ne_ctx_pop(ctx);

  ne_ctx_push(ctx, ne_top_level_elements, ctx);
  ne_ctx_push(ctx, ne_segment_elements, &ctx->segment);

  ctx->log(ctx, NESTEGG_LOG_DEBUG, "seek: parsing cluster elements");
  r = ne_parse(ctx, NULL, -1);
  if (r != 1)
    return -1;

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

  cue_point = ne_find_cue_point_for_tstamp(ctx, ctx->segment.cues.cue_point.head,
                                           track, tc_scale, tstamp);
  if (!cue_point)
    return -1;

  pos = ne_find_cue_position_for_track(ctx, cue_point->cue_track_positions.head, track);
  if (pos == NULL)
    return -1;

  if (ne_get_uint(pos->cluster_position, &seek_pos) != 0)
    return -1;

  /* Seek and set up parser state for segment-level element (Cluster). */
  r = nestegg_offset_seek(ctx, ctx->segment_offset + seek_pos);

  if (!ne_is_suspend_element(ctx->last_id))
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

  if (type & TRACK_TYPE_VIDEO)
    return NESTEGG_TRACK_VIDEO;

  if (type & TRACK_TYPE_AUDIO)
    return NESTEGG_TRACK_AUDIO;

  return -1;
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

  if (strcmp(codec_id, TRACK_ID_VORBIS) == 0)
    return NESTEGG_CODEC_VORBIS;

  if (strcmp(codec_id, TRACK_ID_OPUS) == 0)
    return NESTEGG_CODEC_OPUS;

  return -1;
}

int
nestegg_track_codec_data_count(nestegg * ctx, unsigned int track,
                               unsigned int * count)
{
  struct track_entry * entry;
  struct ebml_binary codec_private;
  unsigned char * p;

  *count = 0;

  entry = ne_find_track_entry(ctx, track);
  if (!entry)
    return -1;

  if (nestegg_track_codec_id(ctx, track) != NESTEGG_CODEC_VORBIS)
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
  uint64_t sizes[3], total;
  unsigned char * p;
  unsigned int count, i;

  *data = NULL;
  *length = 0;

  entry = ne_find_track_entry(ctx, track);
  if (!entry)
    return -1;

  if (nestegg_track_codec_id(ctx, track) != NESTEGG_CODEC_VORBIS
      && nestegg_track_codec_id(ctx, track) != NESTEGG_CODEC_OPUS)
    return -1;

  if (ne_get_binary(entry->codec_private, &codec_private) != 0)
    return -1;

  if (nestegg_track_codec_id(ctx, track) == NESTEGG_CODEC_VORBIS) {
    p = codec_private.data;
    count = *p++ + 1;

    if (count > 3)
      return -1;

    i = 0;
    total = 0;
    while (--count) {
      sizes[i] = ne_xiph_lace_value(&p);
      total += sizes[i];
      i += 1;
    }
    sizes[i] = codec_private.length - total - (p - codec_private.data);

    for (i = 0; i < item; ++i) {
      if (sizes[i] > LIMIT_FRAME)
        return -1;
      p += sizes[i];
    }
    *data = p;
    *length = sizes[item];
  } else {
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
nestegg_read_packet(nestegg * ctx, nestegg_packet ** pkt)
{
  int r, read_block = 0;
  uint64_t id, size;

  *pkt = NULL;

  for (;;) {
    r = ne_peek_element(ctx, &id, &size);
    if (r != 1)
      return r;

    /* Any DESC_FLAG_SUSPEND fields must be handled here. */
    if (ne_is_suspend_element(id)) {
      r = ne_read_element(ctx, &id, &size);
      if (r != 1)
        return r;

      /* The only DESC_FLAG_SUSPEND fields are Blocks and SimpleBlocks, which we
         handle directly. */
      r = ne_read_block(ctx, id, size, pkt);
      if (r != 1)
        return r;

      read_block = 1;

      /* These are not valid elements of a SimpleBlock, only a full-blown
         Block. */
      if (id != ID_SIMPLE_BLOCK) {
        r = ne_read_block_duration(ctx, *pkt);
        if (r < 0)
          return r;

        r = ne_read_discard_padding(ctx, *pkt);
        if (r < 0)
          return r;

        r = ne_read_block_additions(ctx, *pkt);
        if (r < 0)
          return r;
      }

      /* If we have read a block and hit EOS when reading optional block
         subelements, don't report EOS until the next call. */
      return read_block;
    }

    r =  ne_parse(ctx, NULL, -1);
    if (r != 1)
      return r;
  }

  return 1;
}

void
nestegg_free_packet(nestegg_packet * pkt)
{
  struct frame * frame;
  struct block_additional * block_additional;

  while (pkt->frame) {
    frame = pkt->frame;
    pkt->frame = frame->next;
    free(frame->data);
    free(frame);
  }

  while (pkt->block_additional) {
    block_additional = pkt->block_additional;
    pkt->block_additional = block_additional->next;
    free(block_additional->data);
    free(block_additional);
  }

  free(pkt);
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
  *duration = pkt->duration;
  return 0;
}

int
nestegg_packet_discard_padding(nestegg_packet * pkt, int64_t * discard_padding)
{
  *discard_padding = pkt->discard_padding;
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
nestegg_has_cues(nestegg * ctx)
{
  return ctx->segment.cues.cue_point.head ||
    ne_find_seek_for_id(ctx->segment.seek_head.head, ID_CUES);
}

int
nestegg_sniff(unsigned char const * buffer, size_t length)
{
  nestegg_io io;
  struct sniff_buffer userdata;

  userdata.buffer = buffer;
  userdata.length = length;
  userdata.offset = 0;

  io.read = ne_buffer_read;
  io.seek = ne_buffer_seek;
  io.tell = ne_buffer_tell;
  io.userdata = &userdata;
  return ne_match_webm(io, length);
}

/* From halloc.c */
int halloc_set_allocator(realloc_t realloc_func);

int
nestegg_set_halloc_func(void * (* realloc_func)(void *, size_t))
{
  return halloc_set_allocator(realloc_func);
}
