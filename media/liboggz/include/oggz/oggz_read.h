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

#ifndef __OGGZ_READ_H__
#define __OGGZ_READ_H__

/** \file
 * Interfaces for reading Ogg files and streams
 */

/** \defgroup read_api Oggz Read API
 *
 * Oggz parses Ogg bitstreams, forming ogg_packet structures, and calling
 * your OggzReadPacket callback(s).
 *
 * You provide Ogg data to Oggz with oggz_read() or oggz_read_input(), and
 * independently process it in OggzReadPacket callbacks.
 * It is possible to set a different callback per \a serialno (ie. for each
 * logical bitstream in the Ogg bitstream - see the \ref basics section for
 * more detail).
 *
 * When using an OGGZ* opened with the OGGZ_AUTO flag set, Oggz will
 * internally calculate the granulepos for each packet, even though these
 * are not all recorded in the file: only the last packet ending on a page
 * will have its granulepos recorded in the page header.
 * Within a OggzReadPacket callback, calling oggz_tell_granulepos() will
 * retrieve the calculated granulepos.
 *
 * See \ref seek_api for information on seeking on interleaved Ogg data,
 * and for working with calculated granulepos values.
 *
 * \{
 */

/**
 * This is the signature of a callback which you must provide for Oggz
 * to call whenever it finds a new packet in the Ogg stream associated
 * with \a oggz.
 *
 * \param oggz The OGGZ handle
 * \param op The full ogg_packet (see <ogg/ogg.h>)
 * \param serialno Identify the logical bistream in \a oggz that contains
 *                 \a op
 * \param user_data A generic pointer you have provided earlier
 * \returns 0 to continue, non-zero to instruct Oggz to stop.
 *
 * \note It is possible to provide different callbacks per logical
 * bitstream -- see oggz_set_read_callback() for more information.
 */
typedef int (*OggzReadPacket) (OGGZ * oggz, ogg_packet * op, long serialno,
			       void * user_data);

/**
 * Set a callback for Oggz to call when a new Ogg packet is found in the
 * stream.
 *
 * \param oggz An OGGZ handle previously opened for reading
 * \param serialno Identify the logical bitstream in \a oggz to attach
 * this callback to, or -1 to attach this callback to all unattached
 * logical bitstreams in \a oggz.
 * \param read_packet Your callback function
 * \param user_data Arbitrary data you wish to pass to your callback
 * \retval 0 Success
 * \retval OGGZ_ERR_BAD_SERIALNO \a serialno does not identify an existing
 * logical bitstream in \a oggz.
 * \retval OGGZ_ERR_BAD_OGGZ \a oggz does not refer to an existing OGGZ
 * \retval OGGZ_ERR_INVALID Operation not suitable for this OGGZ
 * \retval OGGZ_ERR_OUT_OF_MEMORY Out of memory
 *
 * \note Values of \a serialno other than -1 allows you to specify different
 * callback functions for each logical bitstream.
 *
 * \note It is safe to call this callback from within an OggzReadPacket
 * function, in order to specify that subsequent packets should be handled
 * by a different OggzReadPacket function.
 */
int oggz_set_read_callback (OGGZ * oggz, long serialno,
			    OggzReadPacket read_packet, void * user_data);

/**
 * This is the signature of a callback which you must provide for Oggz
 * to call whenever it finds a new page in the Ogg stream associated
 * with \a oggz.
 *
 * \param oggz The OGGZ handle
 * \param op The full ogg_page (see <ogg/ogg.h>)
 * \param user_data A generic pointer you have provided earlier
 * \returns 0 to continue, non-zero to instruct Oggz to stop.
 */
typedef int (*OggzReadPage) (OGGZ * oggz, const ogg_page * og,
			     long serialno, void * user_data);

/**
 * Set a callback for Oggz to call when a new Ogg page is found in the
 * stream.
 *
 * \param oggz An OGGZ handle previously opened for reading
 * \param serialno Identify the logical bitstream in \a oggz to attach
 * this callback to, or -1 to attach this callback to all unattached
 * logical bitstreams in \a oggz.
 * \param read_page Your OggzReadPage callback function
 * \param user_data Arbitrary data you wish to pass to your callback
 * \retval 0 Success
 * \retval OGGZ_ERR_BAD_OGGZ \a oggz does not refer to an existing OGGZ
 * \retval OGGZ_ERR_INVALID Operation not suitable for this OGGZ
 * \retval OGGZ_ERR_OUT_OF_MEMORY Out of memory
 *
 * \note Values of \a serialno other than -1 allows you to specify different
 * callback functions for each logical bitstream.
 *
 * \note It is safe to call this callback from within an OggzReadPage
 * function, in order to specify that subsequent pages should be handled
 * by a different OggzReadPage function.
 */
int oggz_set_read_page (OGGZ * oggz, long serialno,
			OggzReadPage read_page, void * user_data);


/**
 * Read n bytes into \a oggz, calling any read callbacks on the fly.
 * \param oggz An OGGZ handle previously opened for reading
 * \param n A count of bytes to ingest
 * \retval ">  0" The number of bytes successfully ingested.
 * \retval 0 End of file
 * \retval OGGZ_ERR_BAD_OGGZ \a oggz does not refer to an existing OGGZ
 * \retval OGGZ_ERR_INVALID Operation not suitable for this OGGZ
 * \retval OGGZ_ERR_SYSTEM System error; check errno for details
 * \retval OGGZ_ERR_STOP_OK Reading was stopped by a user callback
 * returning OGGZ_STOP_OK
 * \retval OGGZ_ERR_STOP_ERR Reading was stopped by a user callback
 * returning OGGZ_STOP_ERR
 * \retval OGGZ_ERR_HOLE_IN_DATA Hole (sequence number gap) detected in input data
 * \retval OGGZ_ERR_OUT_OF_MEMORY Out of memory
 */
long oggz_read (OGGZ * oggz, long n);

/**
 * Input data into \a oggz.
 * \param oggz An OGGZ handle previously opened for reading
 * \param buf A memory buffer
 * \param n A count of bytes to input
 * \retval ">  0" The number of bytes successfully ingested.
 * \retval OGGZ_ERR_BAD_OGGZ \a oggz does not refer to an existing OGGZ
 * \retval OGGZ_ERR_INVALID Operation not suitable for this OGGZ
 * \retval OGGZ_ERR_STOP_OK Reading was stopped by a user callback
 * returning OGGZ_STOP_OK
 * \retval OGGZ_ERR_STOP_ERR Reading was stopped by a user callback
 * returning OGGZ_STOP_ERR
 * \retval OGGZ_ERR_HOLE_IN_DATA Hole (sequence number gap) detected in input data
 * \retval OGGZ_ERR_OUT_OF_MEMORY Out of memory
 */
long oggz_read_input (OGGZ * oggz, unsigned char * buf, long n);

/** \}
 */

/**
 * Erase any input buffered in Oggz. This discards any input read from the
 * underlying IO system but not yet delivered as ogg_packets.
 *
 * \param oggz An OGGZ handle
 * \retval 0 Success
 * \retval OGGZ_ERR_SYSTEM Error seeking on underlying IO.
 * \retval OGGZ_ERR_BAD_OGGZ \a oggz does not refer to an existing OGGZ
 * \retval OGGZ_ERR_INVALID Operation not suitable for this OGGZ
 */
int oggz_purge (OGGZ * oggz);

/**
 * Determine the content type of the oggz stream referred to by \a serialno
 *
 * \param oggz An OGGZ handle
 * \param serialno An ogg stream serialno
 * \retval OGGZ_CONTENT_THEORA..OGGZ_CONTENT_UNKNOWN content successfully 
 *          identified
 * \retval OGGZ_ERR_BAD_OGGZ \a oggz does not refer to an existing OGGZ
 * \retval OGGZ_ERR_BAD_SERIALNO \a serialno does not refer to an existing
 *          stream
 */
OggzStreamContent oggz_stream_get_content (OGGZ * oggz, long serialno);

/**
 * Return human-readable string representation of content type of oggz stream
 * referred to by \a serialno
 *
 * \param oggz An OGGZ handle
 * \param serialno An ogg stream serialno
 * \retval string the name of the content type
 * \retval NULL \a oggz or \a serialno invalid
 */
const char * oggz_stream_get_content_type (OGGZ *oggz, long serialno);

/**
 * Determine the number of headers of the oggz stream referred to by
 * \a serialno
 *
 * \param oggz An OGGZ handle
 * \param serialno An ogg stream serialno
 * \retval OGGZ_CONTENT_THEORA..OGGZ_CONTENT_UNKNOWN content successfully 
 *          identified
 * \retval OGGZ_ERR_BAD_OGGZ \a oggz does not refer to an existing OGGZ
 * \retval OGGZ_ERR_BAD_SERIALNO \a serialno does not refer to an existing
 *          stream
 */
int oggz_stream_get_numheaders (OGGZ * oggz, long serialno);

#endif /* __OGGZ_READ_H__ */
