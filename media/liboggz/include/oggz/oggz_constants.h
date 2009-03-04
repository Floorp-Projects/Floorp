/*
   Copyright (C) 2003 Commonwealth Scientific and Industrial Research
   Organisation (CSIRO) Australia

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of CSIRO Australia nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ORGANISATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __OGGZ_CONSTANTS_H__
#define __OGGZ_CONSTANTS_H__

/** \file
 * General constants used by liboggz.
 */

/**
 * Flags to oggz_new(), oggz_open(), and oggz_openfd().
 * Can be or'ed together in the following combinations:
 * - OGGZ_READ | OGGZ_AUTO
 * - OGGZ_WRITE | OGGZ_NONSTRICT | OGGZ_PREFIX | OGGZ_SUFFIX
 */
enum OggzFlags {
  /** Read only */
  OGGZ_READ         = 0x00,

  /** Write only */
  OGGZ_WRITE        = 0x01,

  /** Disable strict adherence to mapping constraints, eg for
   * handling an incomplete stream */
  OGGZ_NONSTRICT    = 0x10,

  /**
   * Scan for known headers while reading, and automatically set
   * metrics appropriately. Opening a file for reading with
   * \a flags = OGGZ_READ | OGGZ_AUTO will allow seeking on Speex,
   * Vorbis, FLAC, Theora, CMML and all Annodex streams in units of
   * milliseconds, once all bos pages have been delivered. */
  OGGZ_AUTO         = 0x20,

  /**
   * Write Prefix: Assume that we are only writing the prefix of an
   * Ogg stream, ie. disable checking for conformance with end-of-stream
   * constraints.
   */
  OGGZ_PREFIX       = 0x40,

  /**
   * Write Suffix: Assume that we are only writing the suffix of an
   * Ogg stream, ie. disable checking for conformance with
   * beginning-of-stream constraints.
   */
  OGGZ_SUFFIX       = 0x80

};

enum OggzStopCtl {
  /** Continue calling read callbacks */
  OGGZ_CONTINUE     = 0,

  /** Stop calling callbacks, but retain buffered packet data */
  OGGZ_STOP_OK      = 1,

  /** Stop calling callbacks, and purge buffered packet data */
  OGGZ_STOP_ERR     = -1
};

/**
 * Flush options for oggz_write_feed; can be or'ed together
 */
enum OggzFlushOpts {
  /** Flush all streams before beginning this packet */
  OGGZ_FLUSH_BEFORE = 0x01,

  /** Flush after this packet */
  OGGZ_FLUSH_AFTER  = 0x02
};

/**
 * Definition of stream content types
 */
typedef enum OggzStreamContent {
  OGGZ_CONTENT_THEORA = 0,
  OGGZ_CONTENT_VORBIS,
  OGGZ_CONTENT_SPEEX,
  OGGZ_CONTENT_PCM,
  OGGZ_CONTENT_CMML,
  OGGZ_CONTENT_ANX2,
  OGGZ_CONTENT_SKELETON,
  OGGZ_CONTENT_FLAC0,
  OGGZ_CONTENT_FLAC,
  OGGZ_CONTENT_ANXDATA,
  OGGZ_CONTENT_CELT,
  OGGZ_CONTENT_KATE,
  OGGZ_CONTENT_DIRAC,
  OGGZ_CONTENT_UNKNOWN
} OggzStreamContent;

/**
 * Definitions of error return values
 */
enum OggzError {
  /** No error */
  OGGZ_ERR_OK                           = 0,

  /** generic error */
  OGGZ_ERR_GENERIC                      = -1,

  /** oggz is not a valid OGGZ */
  OGGZ_ERR_BAD_OGGZ                     = -2,

  /** The requested operation is not suitable for this OGGZ */
  OGGZ_ERR_INVALID                      = -3,

  /** oggz contains no logical bitstreams */
  OGGZ_ERR_NO_STREAMS                   = -4,

  /** Operation is inappropriate for oggz in current bos state */
  OGGZ_ERR_BOS                          = -5,

  /** Operation is inappropriate for oggz in current eos state */
  OGGZ_ERR_EOS                          = -6,

  /** Operation requires a valid metric, but none has been set */
  OGGZ_ERR_BAD_METRIC                   = -7,

  /** System specific error; check errno for details */
  OGGZ_ERR_SYSTEM                       = -10,

  /** Functionality disabled at build time */
  OGGZ_ERR_DISABLED                     = -11,

  /** Seeking operation is not possible for this OGGZ */
  OGGZ_ERR_NOSEEK                       = -13,

  /** Reading was stopped by an OggzReadCallback returning OGGZ_STOP_OK
   * or writing was stopped by an  OggzWriteHungry callback returning
   * OGGZ_STOP_OK */
  OGGZ_ERR_STOP_OK                      = -14,

  /** Reading was stopped by an OggzReadCallback returning OGGZ_STOP_ERR
   * or writing was stopped by an OggzWriteHungry callback returning
   * OGGZ_STOP_ERR */
  OGGZ_ERR_STOP_ERR                     = -15,

  /** no data available from IO, try again */
  OGGZ_ERR_IO_AGAIN                     = -16,

  /** Hole (sequence number gap) detected in input data */
  OGGZ_ERR_HOLE_IN_DATA                 = -17,

  /** Out of memory */
  OGGZ_ERR_OUT_OF_MEMORY                = -18,

  /** The requested serialno does not exist in this OGGZ */
  OGGZ_ERR_BAD_SERIALNO                 = -20,

  /** Packet disallowed due to invalid byte length */
  OGGZ_ERR_BAD_BYTES                    = -21,

  /** Packet disallowed due to invalid b_o_s (beginning of stream) flag */
  OGGZ_ERR_BAD_B_O_S                    = -22,

  /** Packet disallowed due to invalid e_o_s (end of stream) flag */
  OGGZ_ERR_BAD_E_O_S                    = -23,

  /** Packet disallowed due to invalid granulepos */
  OGGZ_ERR_BAD_GRANULEPOS               = -24,

  /** Packet disallowed due to invalid packetno */
  OGGZ_ERR_BAD_PACKETNO                 = -25,

  /** Comment violates VorbisComment restrictions */
  /* 129 == 0x81 is the frame marker for Theora's comments page ;-) */
  OGGZ_ERR_COMMENT_INVALID              = -129,

  /** Guard provided by user has non-zero value */
  OGGZ_ERR_BAD_GUARD                    = -210,

  /** Attempt to call oggz_write() or oggz_write_output() from within
   * a hungry() callback */
  OGGZ_ERR_RECURSIVE_WRITE              = -266
};

#endif /* __OGGZ_CONSTANTS_H__ */
