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

#ifndef __OGGZ_DEPRECATED_H__
#define __OGGZ_DEPRECATED_H__

/** \file
 * Deprecated interfaces
 */

/**
 * DEPRECATED CONSTANT.
 * OGGZ_ERR_USER_STOPPED was introduced during development (post 0.8.3),
 * and is similar in functionality to and numerically equal to (ie. ABI
 * compatible with) OGGZ_ERR_STOP_OK in <oggz/oggz_constants.h>.
 * It was badly named, as the preferred functionality distinguishes between
 * a user's OggzReadCallback returning OGGZ_STOP_OK or OGGZ_STOP_ERR; your
 * code should distinguish between these two too :-) Hence, don't use this
 * (unreleased) name in new code.
 */
#define OGGZ_ERR_USER_STOPPED OGGZ_ERR_STOP_OK

/**
 * DEPRECATED CONSTANT.
 * OGGZ_ERR_READ_STOP_OK, OGGZ_ERR_READ_STOP_ERR were introduced to allow
 * the user to differentiate between a cancelled oggz_read_*() returning
 * due to error or an ok condition.
 * From 0.9.4 similar functionality was added for oggz_write_*(), hence this
 * constant was renamed.
 */
#define OGGZ_ERR_READ_STOP_OK  OGGZ_ERR_STOP_OK

/**
 * DEPRECATED CONSTANT.
 * OGGZ_ERR_READ_STOP_OK, OGGZ_ERR_READ_STOP_ERR were introduced to allow
 * the user to differentiate between a cancelled oggz_read_*() returning
 * due to error or an ok condition.
 * From 0.9.4 similar functionality was added for oggz_write_*(), hence this
 * constant was renamed.
 */
#define OGGZ_ERR_READ_STOP_ERR OGGZ_ERR_STOP_ERR

/**
 * DEPRECATED FUNCTION
 * This function has been replaced with the more clearly named
 * oggz_set_granulerate().
 * Specify that a logical bitstream has a linear metric
 * \param oggz An OGGZ handle
 * \param serialno Identify the logical bitstream in \a oggz to attach
 * this linear metric to. A value of -1 indicates that the metric should
 * be attached to all unattached logical bitstreams in \a oggz.
 * \param granule_rate_numerator The numerator of the granule rate
 * \param granule_rate_denominator The denominator of the granule rate
 * \returns 0 Success
 * \retval OGGZ_ERR_BAD_SERIALNO \a serialno does not identify an existing
 * logical bitstream in \a oggz.
 * \retval OGGZ_ERR_BAD_OGGZ \a oggz does not refer to an existing OGGZ
 */
int oggz_set_metric_linear (OGGZ * oggz, long serialno,
			    ogg_int64_t granule_rate_numerator,
			    ogg_int64_t granule_rate_denominator);

/**
 * DEPRECATED FUNCTION
 * This function has been replaced with oggz_comments_generate(), which
 * does not require the packet_type argument. Instead, the packet type is
 * determined by the content type of the stream, which was discovered when
 * the bos packet was passed to oggz_write_feed.
 *
 * Output a comment packet for the specified stream.
 * \param oggz A OGGZ* handle (created with OGGZ_WRITE)
 * \param serialno Identify a logical bitstream within \a oggz
 * \param packet_type Type of comment packet to generate,
 * FLAC, OggPCM, Speex, Theora and Vorbis are supported
 * \param FLAC_final_metadata_block Set this to zero unless the packet_type is
 * FLAC, and there are no further metadata blocks to follow. See note below
 * for details.
 * \returns A comment packet for the stream. When no longer needed it
 * should be freed with oggz_packet_destroy().
 * \retval NULL content type does not support comments, not enough memory
 * or comment was too long for FLAC
 * \note FLAC streams may contain multiple metadata blocks of different types.
 * When encapsulated in Ogg the first of these must be a Vorbis comment packet
 * but PADDING, APPLICATION, SEEKTABLE, CUESHEET and PICTURE may follow.
 * The last metadata block must have its first bit set to 1. Since liboggz does
 * not know whether you will supply more metadata blocks you must tell it if
 * this is the last (or only) metadata block by setting
 * FLAC_final_metadata_block to 1.
 * \n As FLAC metadata blocks are limited in size to 16MB minus 1 byte, this
 * function will refuse to produce longer comment packets for FLAC.
 * \n See http://flac.sourceforge.net/format.html for more details.
 */
ogg_packet *
oggz_comment_generate(OGGZ * oggz, long serialno,
		      OggzStreamContent packet_type,
		      int FLAC_final_metadata_block);

#endif /* __OGGZ_DEPRECATED_H__ */
