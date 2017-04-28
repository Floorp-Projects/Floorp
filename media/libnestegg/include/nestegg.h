/*
 * Copyright © 2010 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#if !defined(NESTEGG_671cac2a_365d_ed69_d7a3_4491d3538d79)
#define NESTEGG_671cac2a_365d_ed69_d7a3_4491d3538d79

#include <limits.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

/** @mainpage

    @section intro Introduction

    This is the documentation for the <tt>libnestegg</tt> C API.
    <tt>libnestegg</tt> is a demultiplexing library for <a
    href="http://www.webmproject.org/code/specs/container/">WebM</a>
    media files.

    @section example Example code

    @code
    nestegg * demux_ctx;
    nestegg_init(&demux_ctx, io, NULL, -1);

    nestegg_packet * pkt;
    while ((r = nestegg_read_packet(demux_ctx, &pkt)) > 0) {
      unsigned int track;

      nestegg_packet_track(pkt, &track);

      // This example decodes the first track only.
      if (track == 0) {
        unsigned int chunk, chunks;

        nestegg_packet_count(pkt, &chunks);

        // Decode each chunk of data.
        for (chunk = 0; chunk < chunks; ++chunk) {
          unsigned char * data;
          size_t data_size;

          nestegg_packet_data(pkt, chunk, &data, &data_size);

          example_codec_decode(codec_ctx, data, data_size);
        }
      }

      nestegg_free_packet(pkt);
    }

    nestegg_destroy(demux_ctx);
    @endcode
*/


/** @file
    The <tt>libnestegg</tt> C API. */

#define NESTEGG_TRACK_VIDEO   0       /**< Track is of type video. */
#define NESTEGG_TRACK_AUDIO   1       /**< Track is of type audio. */
#define NESTEGG_TRACK_UNKNOWN INT_MAX /**< Track is of type unknown. */

#define NESTEGG_CODEC_VP8     0       /**< Track uses Google On2 VP8 codec. */
#define NESTEGG_CODEC_VORBIS  1       /**< Track uses Xiph Vorbis codec. */
#define NESTEGG_CODEC_VP9     2       /**< Track uses Google On2 VP9 codec. */
#define NESTEGG_CODEC_OPUS    3       /**< Track uses Xiph Opus codec. */
#define NESTEGG_CODEC_AV1     4       /**< Track uses AOMedia AV1 codec. */
#define NESTEGG_CODEC_UNKNOWN INT_MAX /**< Track uses unknown codec. */

#define NESTEGG_VIDEO_MONO              0 /**< Track is mono video. */
#define NESTEGG_VIDEO_STEREO_LEFT_RIGHT 1 /**< Track is side-by-side stereo video.  Left first. */
#define NESTEGG_VIDEO_STEREO_BOTTOM_TOP 2 /**< Track is top-bottom stereo video.  Right first. */
#define NESTEGG_VIDEO_STEREO_TOP_BOTTOM 3 /**< Track is top-bottom stereo video.  Left first. */
#define NESTEGG_VIDEO_STEREO_RIGHT_LEFT 11 /**< Track is side-by-side stereo video.  Right first. */

#define NESTEGG_SEEK_SET 0 /**< Seek offset relative to beginning of stream. */
#define NESTEGG_SEEK_CUR 1 /**< Seek offset relative to current position in stream. */
#define NESTEGG_SEEK_END 2 /**< Seek offset relative to end of stream. */

#define NESTEGG_LOG_DEBUG    1     /**< Debug level log message. */
#define NESTEGG_LOG_INFO     10    /**< Informational level log message. */
#define NESTEGG_LOG_WARNING  100   /**< Warning level log message. */
#define NESTEGG_LOG_ERROR    1000  /**< Error level log message. */
#define NESTEGG_LOG_CRITICAL 10000 /**< Critical level log message. */

#define NESTEGG_ENCODING_COMPRESSION 0 /**< Content encoding type is compression. */
#define NESTEGG_ENCODING_ENCRYPTION  1 /**< Content encoding type is encryption. */

#define NESTEGG_PACKET_HAS_SIGNAL_BYTE_FALSE         0 /**< Packet does not have signal byte */
#define NESTEGG_PACKET_HAS_SIGNAL_BYTE_UNENCRYPTED   1 /**< Packet has signal byte and is unencrypted */
#define NESTEGG_PACKET_HAS_SIGNAL_BYTE_ENCRYPTED     2 /**< Packet has signal byte and is encrypted */
#define NESTEGG_PACKET_HAS_SIGNAL_BYTE_PARTITIONED   4 /**< Packet has signal byte and is partitioned */

#define NESTEGG_PACKET_HAS_KEYFRAME_FALSE   0 /**< Packet contains only keyframes. */
#define NESTEGG_PACKET_HAS_KEYFRAME_TRUE    1 /**< Packet does not contain any keyframes */
#define NESTEGG_PACKET_HAS_KEYFRAME_UNKNOWN 2 /**< Packet may or may not contain keyframes */

typedef struct nestegg nestegg;               /**< Opaque handle referencing the stream state. */
typedef struct nestegg_packet nestegg_packet; /**< Opaque handle referencing a packet of data. */

/** User supplied IO context. */
typedef struct {
  /** User supplied read callback.
      @param buffer   Buffer to read data into.
      @param length   Length of supplied buffer in bytes.
      @param userdata The #userdata supplied by the user.
      @retval  1 Read succeeded.
      @retval  0 End of stream.
      @retval -1 Error. */
  int (* read)(void * buffer, size_t length, void * userdata);

  /** User supplied seek callback.
      @param offset   Offset within the stream to seek to.
      @param whence   Seek direction.  One of #NESTEGG_SEEK_SET,
                      #NESTEGG_SEEK_CUR, or #NESTEGG_SEEK_END.
      @param userdata The #userdata supplied by the user.
      @retval  0 Seek succeeded.
      @retval -1 Error. */
  int (* seek)(int64_t offset, int whence, void * userdata);

  /** User supplied tell callback.
      @param userdata The #userdata supplied by the user.
      @returns Current position within the stream.
      @retval -1 Error. */
  int64_t (* tell)(void * userdata);

  /** User supplied pointer to be passed to the IO callbacks. */
  void * userdata;
} nestegg_io;

/** Parameters specific to a video track. */
typedef struct {
  unsigned int stereo_mode;    /**< Video mode.  One of #NESTEGG_VIDEO_MONO,
                                    #NESTEGG_VIDEO_STEREO_LEFT_RIGHT,
                                    #NESTEGG_VIDEO_STEREO_BOTTOM_TOP, or
                                    #NESTEGG_VIDEO_STEREO_TOP_BOTTOM. */
  unsigned int width;          /**< Width of the video frame in pixels. */
  unsigned int height;         /**< Height of the video frame in pixels. */
  unsigned int display_width;  /**< Display width of the video frame in pixels. */
  unsigned int display_height; /**< Display height of the video frame in pixels. */
  unsigned int crop_bottom;    /**< Pixels to crop from the bottom of the frame. */
  unsigned int crop_top;       /**< Pixels to crop from the top of the frame. */
  unsigned int crop_left;      /**< Pixels to crop from the left of the frame. */
  unsigned int crop_right;     /**< Pixels to crop from the right of the frame. */
  unsigned int alpha_mode;     /**< 1 if an additional opacity stream is available, otherwise 0. */
} nestegg_video_params;

/** Parameters specific to an audio track. */
typedef struct {
  double rate;           /**< Sampling rate in Hz. */
  unsigned int channels; /**< Number of audio channels. */
  unsigned int depth;    /**< Bits per sample. */
  uint64_t  codec_delay; /**< Nanoseconds that must be discarded from the start. */
  uint64_t  seek_preroll;/**< Nanoseconds that must be discarded after a seek. */
} nestegg_audio_params;

/** Logging callback function pointer. */
typedef void (* nestegg_log)(nestegg * context, unsigned int severity, char const * format, ...);

/** Initialize a nestegg context.  During initialization the parser will
    read forward in the stream processing all elements until the first
    block of media is reached.  All track metadata has been processed at this point.
    @param context  Storage for the new nestegg context.  @see nestegg_destroy
    @param io       User supplied IO context.
    @param callback Optional logging callback function pointer.  May be NULL.
    @param max_offset Optional maximum offset to be read. Set -1 to ignore.
    @retval  0 Success.
    @retval -1 Error. */
int nestegg_init(nestegg ** context, nestegg_io io, nestegg_log callback, int64_t max_offset);

/** Destroy a nestegg context and free associated memory.
    @param context #nestegg context to be freed.  @see nestegg_init */
void nestegg_destroy(nestegg * context);

/** Query the duration of the media stream in nanoseconds.
    @param context  Stream context initialized by #nestegg_init.
    @param duration Storage for the queried duration.
    @retval  0 Success.
    @retval -1 Error. */
int nestegg_duration(nestegg * context, uint64_t * duration);

/** Query the tstamp scale of the media stream in nanoseconds.
    @note Timecodes presented by nestegg have been scaled by this value
          before presentation to the caller.
    @param context Stream context initialized by #nestegg_init.
    @param scale   Storage for the queried scale factor.
    @retval  0 Success.
    @retval -1 Error. */
int nestegg_tstamp_scale(nestegg * context, uint64_t * scale);

/** Query the number of tracks in the media stream.
    @param context Stream context initialized by #nestegg_init.
    @param tracks  Storage for the queried track count.
    @retval  0 Success.
    @retval -1 Error. */
int nestegg_track_count(nestegg * context, unsigned int * tracks);

/** Query the start and end offset for a particular cluster.
    @param context     Stream context initialized by #nestegg_init.
    @param cluster_num Zero-based cluster number; order they appear in cues.
    @param max_offset  Optional maximum offset to be read. Set -1 to ignore.
    @param start_pos   Starting offset of the cluster. -1 means non-existant.
    @param end_pos     Starting offset of the cluster. -1 means non-existant or
                       final cluster.
    @param tstamp      Starting timestamp of the cluster.
    @retval  0 Success.
    @retval -1 Error. */
int nestegg_get_cue_point(nestegg * context, unsigned int cluster_num,
                          int64_t max_offset, int64_t * start_pos,
                          int64_t * end_pos, uint64_t * tstamp);

/** Seek to @a offset.  Stream will seek directly to offset.
    Must be used to seek to the start of a cluster; the parser will not be
    able to understand other offsets.
    @param context Stream context initialized by #nestegg_init.
    @param offset  Absolute offset in bytes.
    @retval  0 Success.
    @retval -1 Error. */
int nestegg_offset_seek(nestegg * context, uint64_t offset);

/** Seek @a track to @a tstamp.  Stream seek will terminate at the earliest
    key point in the stream at or before @a tstamp.  Other tracks in the
    stream will output packets with unspecified but nearby timestamps.
    @param context Stream context initialized by #nestegg_init.
    @param track   Zero based track number.
    @param tstamp  Absolute timestamp in nanoseconds.
    @retval  0 Success.
    @retval -1 Error. */
int nestegg_track_seek(nestegg * context, unsigned int track, uint64_t tstamp);

/** Query the type specified by @a track.
    @param context Stream context initialized by #nestegg_init.
    @param track   Zero based track number.
    @retval #NESTEGG_TRACK_VIDEO   Track type is video.
    @retval #NESTEGG_TRACK_AUDIO   Track type is audio.
    @retval #NESTEGG_TRACK_UNKNOWN Track type is unknown.
    @retval -1 Error. */
int nestegg_track_type(nestegg * context, unsigned int track);

/** Query the codec ID specified by @a track.
    @param context Stream context initialized by #nestegg_init.
    @param track   Zero based track number.
    @retval #NESTEGG_CODEC_VP8     Track codec is VP8.
    @retval #NESTEGG_CODEC_VP9     Track codec is VP9.
    @retval #NESTEGG_CODEC_VORBIS  Track codec is Vorbis.
    @retval #NESTEGG_CODEC_OPUS    Track codec is Opus.
    @retval #NESTEGG_CODEC_UNKNOWN Track codec is unknown.
    @retval -1 Error. */
int nestegg_track_codec_id(nestegg * context, unsigned int track);

/** Query the number of codec initialization chunks for @a track.  Each
    chunk of data should be passed to the codec initialization functions in
    the order returned.
    @param context Stream context initialized by #nestegg_init.
    @param track   Zero based track number.
    @param count   Storage for the queried chunk count.
    @retval  0 Success.
    @retval -1 Error. */
int nestegg_track_codec_data_count(nestegg * context, unsigned int track,
                                   unsigned int * count);

/** Get a pointer to chunk number @a item of codec initialization data for
    @a track.
    @param context Stream context initialized by #nestegg_init.
    @param track   Zero based track number.
    @param item    Zero based chunk item number.
    @param data    Storage for the queried data pointer.
                   The data is owned by the #nestegg context.
    @param length  Storage for the queried data size.
    @retval  0 Success.
    @retval -1 Error. */
int nestegg_track_codec_data(nestegg * context, unsigned int track, unsigned int item,
                             unsigned char ** data, size_t * length);

/** Query the video parameters specified by @a track.
    @param context Stream context initialized by #nestegg_init.
    @param track   Zero based track number.
    @param params  Storage for the queried video parameters.
    @retval  0 Success.
    @retval -1 Error. */
int nestegg_track_video_params(nestegg * context, unsigned int track,
                               nestegg_video_params * params);

/** Query the audio parameters specified by @a track.
    @param context Stream context initialized by #nestegg_init.
    @param track   Zero based track number.
    @param params  Storage for the queried audio parameters.
    @retval  0 Success.
    @retval -1 Error. */
int nestegg_track_audio_params(nestegg * context, unsigned int track,
                               nestegg_audio_params * params);

/** Query the encoding status for @a track. If a track has multiple encodings
    the first will be returned.
    @param context Stream context initialized by #nestegg_init.
    @param track   Zero based track number.
    @retval #NESTEGG_ENCODING_COMPRESSION The track is compressed, but not encrypted.
    @retval #NESTEGG_ENCODING_ENCRYPTION The track is encrypted and compressed.
    @retval -1 Error. */
int nestegg_track_encoding(nestegg * context, unsigned int track);

/** Query the ContentEncKeyId for @a track. Will return an error if the track
    in not encrypted, or is not recognized.
    @param context                   Stream context initialized by #nestegg_init.
    @param track                     Zero based track number.
    @param content_enc_key_id        Storage for queried id. The content encryption key used.
                                     Owned by nestegg and will be freed separately.
    @param content_enc_key_id_length Length of the queried ContentEncKeyId in bytes.
    @retval  0 Success.
    @retval -1 Error. */
int nestegg_track_content_enc_key_id(nestegg * context, unsigned int track,
                                     unsigned char const ** content_enc_key_id,
                                     size_t * content_enc_key_id_length);

/** Query the default frame duration for @a track.  For a video track, this
    is typically the inverse of the video frame rate.
    @param context  Stream context initialized by #nestegg_init.
    @param track    Zero based track number.
    @param duration Storage for the default duration in nanoseconds.
    @retval  0 Success.
    @retval -1 Error. */
int nestegg_track_default_duration(nestegg * context, unsigned int track,
                                   uint64_t * duration);

/** Reset parser state to the last valid state before nestegg_read_packet failed.
    @param context Stream context initialized by #nestegg_init.
    @retval  0 Success.
    @retval -1 Error. */
int nestegg_read_reset(nestegg * context);

/** Read a packet of media data.  A packet consists of one or more chunks of
    data associated with a single track.  nestegg_read_packet should be
    called in a loop while the return value is 1 to drive the stream parser
    forward.  @see nestegg_free_packet
    @param context Context returned by #nestegg_init.
    @param packet  Storage for the returned nestegg_packet.
    @retval  1 Additional packets may be read in subsequent calls.
    @retval  0 End of stream.
    @retval -1 Error. */
int nestegg_read_packet(nestegg * context, nestegg_packet ** packet);

/** Destroy a nestegg_packet and free associated memory.
    @param packet #nestegg_packet to be freed. @see nestegg_read_packet */
void nestegg_free_packet(nestegg_packet * packet);

/** Query the keyframe status for a given packet.
    @param packet Packet initialized by #nestegg_read_packet.
    @retval #NESTEGG_PACKET_HAS_KEYFRAME_FALSE   Packet contains no keyframes.
    @retval #NESTEGG_PACKET_HAS_KEYFRAME_TRUE    Packet contains keyframes.
    @retval #NESTEGG_PACKET_HAS_KEYFRAME_UNKNOWN Unknown packet keyframe content.
    @retval -1 Error. */
int nestegg_packet_has_keyframe(nestegg_packet * packet);

/** Query the track number of @a packet.
    @param packet Packet initialized by #nestegg_read_packet.
    @param track  Storage for the queried zero based track index.
    @retval  0 Success.
    @retval -1 Error. */
int nestegg_packet_track(nestegg_packet * packet, unsigned int * track);

/** Query the time stamp in nanoseconds of @a packet.
    @param packet Packet initialized by #nestegg_read_packet.
    @param tstamp Storage for the queried timestamp in nanoseconds.
    @retval  0 Success.
    @retval -1 Error. */
int nestegg_packet_tstamp(nestegg_packet * packet, uint64_t * tstamp);

/** Query the duration in nanoseconds of @a packet.
    @param packet Packet initialized by #nestegg_read_packet.
    @param duration Storage for the queried duration in nanoseconds.
    @retval  0 Success.
    @retval -1 Error. */
int nestegg_packet_duration(nestegg_packet * packet, uint64_t * duration);

/** Query the number of data chunks contained in @a packet.
    @param packet Packet initialized by #nestegg_read_packet.
    @param count  Storage for the queried chunk count.
    @retval  0 Success.
    @retval -1 Error. */
int nestegg_packet_count(nestegg_packet * packet, unsigned int * count);

/** Get a pointer to chunk number @a item of packet data.
    @param packet  Packet initialized by #nestegg_read_packet.
    @param item    Zero based chunk item number.
    @param data    Storage for the queried data pointer.
                   The data is owned by the #nestegg_packet packet.
    @param length  Storage for the queried data size.
    @retval  0 Success.
    @retval -1 Error. */
int nestegg_packet_data(nestegg_packet * packet, unsigned int item,
                        unsigned char ** data, size_t * length);

/** Get a pointer to additional data with identifier @a id of additional packet
    data. If @a id isn't present in the packet, returns -1.
    @param packet  Packet initialized by #nestegg_read_packet.
    @param id      Codec specific identifer. For VP8, use 1 to get a VP8 encoded
                   frame containing an alpha channel in its Y plane.
    @param data    Storage for the queried data pointer.
                   The data is owned by the #nestegg_packet packet.
    @param length  Storage for the queried data size.
    @retval  0 Success.
    @retval -1 Error. */
int nestegg_packet_additional_data(nestegg_packet * packet, unsigned int id,
                                   unsigned char ** data, size_t * length);

/** Returns discard_padding for given packet
    @param packet  Packet initialized by #nestegg_read_packet.
    @param discard_padding pointer to store discard padding in.
    @retval  0 Success.
    @retval -1 Error. */
int nestegg_packet_discard_padding(nestegg_packet * packet,
                                   int64_t * discard_padding);

/** Query if a packet is encrypted.
    @param packet Packet initialized by #nestegg_read_packet.
    @retval  #NESTEGG_PACKET_HAS_SIGNAL_BYTE_FALSE No signal byte, encryption
             information not read from packet.
    @retval  #NESTEGG_PACKET_HAS_SIGNAL_BYTE_UNENCRYPTED Encrypted bit not
             set, encryption information not read from packet.
    @retval  #NESTEGG_PACKET_HAS_SIGNAL_BYTE_ENCRYPTED Encrypted bit set,
             encryption infomation read from packet.
    @retval  #NESTEGG_PACKET_HAS_SIGNAL_BYTE_PARTITIONED Partitioned bit set,
             encryption and parition information read from packet.
    @retval -1 Error.*/
int nestegg_packet_encryption(nestegg_packet * packet);

/** Query the IV for an encrypted packet. Expects a packet from an encrypted
    track, and will return error if given a packet that has no signal btye.
    @param packet Packet initialized by #nestegg_read_packet.
    @param iv     Storage for queried iv.
    @param length Length of returned iv, may be 0.
                  The data is owned by the #nestegg_packet packet.
    @retval  0 Success.
    @retval -1 Error.
  */
int nestegg_packet_iv(nestegg_packet * packet, unsigned char const ** iv,
                      size_t * length);

/** Query the packet for offsets.
@param packet            Packet initialized by #nestegg_read_packet.
@param partition_offsets Storage for queried offsets.
@param num_offsets       Length of returned offsets, may be 0.
The data is owned by the #nestegg_packet packet.
@retval  0 Success.
@retval -1 Error.
*/
int nestegg_packet_offsets(nestegg_packet * packet,
                           uint32_t const ** partition_offsets,
                           uint8_t * num_offsets);

/** Returns reference_block given packet
    @param packet          Packet initialized by #nestegg_read_packet.
    @param reference_block pointer to store reference block in.
    @retval  0 Success.
    @retval -1 Error. */
int nestegg_packet_reference_block(nestegg_packet * packet,
                                   int64_t * reference_block);

/** Query the presence of cues.
    @param context  Stream context initialized by #nestegg_init.
    @retval 0 The media has no cues.
    @retval 1 The media has cues. */
int nestegg_has_cues(nestegg * context);

/** Try to determine if the buffer looks like the beginning of a WebM file.
    @param buffer A buffer containing the beginning of a media file.
    @param length The size of the buffer.
    @retval 0 The file is not a WebM file.
    @retval 1 The file is a WebM file. */
int nestegg_sniff(unsigned char const * buffer, size_t length);

#if defined(__cplusplus)
}
#endif

#endif /* NESTEGG_671cac2a_365d_ed69_d7a3_4491d3538d79 */
