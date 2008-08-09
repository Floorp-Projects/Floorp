/*
   Copyright (C) 2007 Annodex Association

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of the Annodex Association nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ASSOCIATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __OGGZ_STREAM_H__
#define __OGGZ_STREAM_H__

/** \file
 * Interfaces for querying Ogg streams
 */

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

#endif /* __OGGZ_STREAM_H__ */
