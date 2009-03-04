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

#ifndef __OGGZ_H__
#define __OGGZ_H__

#include <stdio.h>
#include <sys/types.h>

#include <ogg/ogg.h>
#include <oggz/oggz_constants.h>
#include <oggz/oggz_table.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \mainpage
 *
 * \section intro Oggz makes programming with Ogg easy!
 *
 * This is the documentation for the Oggz C API. Oggz provides a simple
 * programming interface for reading and writing Ogg files and streams.
 * Ogg is an interleaving data container developed by Monty
 * at <a href="http://www.xiph.org/">Xiph.Org</a>, originally to
 * support the Ogg Vorbis audio format.
 *
 * liboggz supports the flexibility afforded by the Ogg file format while
 * presenting the following API niceties:
 *
 * - Strict adherence to the formatting requirements of Ogg
 *   \link basics bitstreams \endlink, to ensure that only valid bitstreams
 *   are generated
 * - A simple, callback based open/read/close or open/write/close
 *   \link oggz.h interface \endlink to raw Ogg files
 * - A customisable \link seek_api seeking \endlink abstraction for
 *   seeking on multitrack Ogg data
 * - A packet queue for feeding incoming packets for writing, with callback
 *   based notification when this queue is empty
 * - A means of overriding the \link oggz_io.h IO functions \endlink used by
 *   Oggz, for easier integration with media frameworks and similar systems.
 * - A handy \link oggz_table.h table \endlink structure for storing
 *   information on each logical bitstream
 *
 * \subsection contents Contents
 *
 * - \link basics Basics \endlink:
 * Information about Ogg required to understand liboggz
 *
 * - \link oggz.h oggz.h \endlink:
 * Documentation of the Oggz C API
 *
 * - \link configuration Configuration \endlink:
 * Customizing liboggz to only read or write.
 *
 * - \link building Building \endlink:
 * Information related to building software that uses liboggz.
 *
 * \section Licensing
 *
 * liboggz is provided under the following BSD-style open source license:
 *
 * \include COPYING
 *
 */

/** \defgroup basics Ogg basics
 *
 * \section Scope
 *
 * This section provides a minimal introduction to Ogg concepts, covering
 * only that which is required to use liboggz.
 *
 * For more detailed information, see the
 * <a href="http://www.xiph.org/ogg/">Ogg</a> homepage
 * or IETF <a href="http://www.ietf.org/rfc/rfc3533.txt">RFC 3533</a>
 * <i>The Ogg File Format version 0</i>.
 *
 * \section Terminology
 *
 * The monospace text below is quoted directly from RFC 3533.
 * For each concept introduced, tips related to liboggz are provided
 * in bullet points.
 *
 * \subsection bitstreams Physical and Logical Bitstreams
 *
 * The raw data of an Ogg stream, as read directly from a file or network
 * socket, is called a <i>physical bitstream</i>.
 *
<pre>
   The result of an Ogg encapsulation is called the "Physical (Ogg)
   Bitstream".  It encapsulates one or several encoder-created
   bitstreams, which are called "Logical Bitstreams".  A logical
   bitstream, provided to the Ogg encapsulation process, has a
   structure, i.e., it is split up into a sequence of so-called
   "Packets".  The packets are created by the encoder of that logical
   bitstream and represent meaningful entities for that encoder only
   (e.g., an uncompressed stream may use video frames as packets).
</pre>
 *
 * \subsection pages Packets and Pages
 *
 * Within the Ogg format, packets are written into \a pages. You can think
 * of pages like pages in a book, and packets as items of the actual text.
 * Consider, for example, individual poems or short stories as the packets.
 * Pages are of course all the same size, and a few very short packets could
 * be written into a single page. On the other hand, a very long packet will
 * use many pages.
 *
 * - liboggz handles the details of writing packets into pages, and of
 *   reading packets from pages; your application deals only with
 *   <tt>ogg_packet</tt> structures.
 * - Each <tt>ogg_packet</tt> structure contains a block of data and its
 *   length in bytes, plus other information related to the stream structure
 *   as explained below.
 *
 * \subsection serialno
 *
 * Each logical bitstream is uniquely identified by a serial number or
 * \a serialno.
 *
 * - Packets are always associated with a \a serialno. This is not actually
 *   part of the <tt>ogg_packet</tt> structure, so wherever you see an
 *   <tt>ogg_packet</tt> in the liboggz API, you will see an accompanying
 *   \a serialno.
 *
<pre>
   This unique serial number is created randomly and does not have any
   connection to the content or encoder of the logical bitstream it
   represents.
</pre>
 *
 * - Use oggz_serialno_new() to generate a new serial number for each
 *   logical bitstream you write.
 * - Use an \link oggz_table.h OggzTable \endlink, keyed by \a serialno,
 *   to store and retrieve data related to each logical bitstream.
 *
 * \subsection boseos b_o_s and e_o_s
<pre>
   bos page: The initial page (beginning of stream) of a logical
      bitstream which contains information to identify the codec type
      and other decoding-relevant information.

   eos page: The final page (end of stream) of a logical bitstream.
</pre>
 *
 * - Every <tt>ogg_packet</tt> contains \a b_o_s and \a e_o_s flags.
 *   Of course each of these will be set only once per logical bitstream.
 *   See the Structuring section below for rules on setting \a b_o_s and
 *   \a e_o_s when interleaving logical bitstreams.
 * - This documentation will refer to \a bos and \a eos <i>packets</i>
 *   (not <i>pages</i>) as that is more closely represented by the API.
 *   The \a bos packet is the only packet on the \a bos page, and the
 *   \a eos packet is the last packet on the \a eos page.
 *
 * \subsection granulepos
<pre>
   granule position: An increasing position number for a specific
      logical bitstream stored in the page header.  Its meaning is
      dependent on the codec for that logical bitstream
</pre>
 *
 * - Every <tt>ogg_packet</tt> contains a \a granulepos. The \a granulepos
 *   of each packet is used mostly for \link seek_api seeking. \endlink
 *
 * \section Structuring
 *
 * The general structure of an Ogg stream is governed by various rules.
 *
 * \subsection secondaries Secondary header packets
 *
 * Some data sources require initial setup information such as comments
 * and codebooks to be present near the beginning of the stream (directly
 * following the b_o_s packets.
 *
<pre>
   Ogg also allows but does not require secondary header packets after
   the bos page for logical bitstreams and these must also precede any
   data packets in any logical bitstream.  These subsequent header
   packets are framed into an integral number of pages, which will not
   contain any data packets.  So, a physical bitstream begins with the
   bos pages of all logical bitstreams containing one initial header
   packet per page, followed by the subsidiary header packets of all
   streams, followed by pages containing data packets.
</pre>
 *
 * - liboggz handles the framing of \a packets into low-level \a pages. To
 *   ensure that the pages used by secondary headers contain no data packets,
 *   set the \a flush parameter of oggz_write_feed() to \a OGGZ_FLUSH_AFTER
 *   when queueing the last of the secondary headers.
 * - or, equivalently, set \a flush to \a OGGZ_FLUSH_BEFORE when queueing
 *   the first of the data packets.
 *
 * \subsection boseosseq Sequencing b_o_s and e_o_s packets
 *
 * The following rules apply for sequencing \a bos and \a eos packets in
 * a physical bitstream:
<pre>
   ... All bos pages of all logical bitstreams MUST appear together at
   the beginning of the Ogg bitstream.

   ... eos pages for the logical bitstreams need not all occur
   contiguously.  Eos pages may be 'nil' pages, that is, pages
   containing no content but simply a page header with position
   information and the eos flag set in the page header.
</pre>
 *
 * - oggz_write_feed() will fail with a return value of
 *   \a OGGZ_ERR_BOS if an attempt is made to queue a late \a bos packet
 *
 * \subsection interleaving Interleaving logical bitstreams
<pre>
   It is possible to consecutively chain groups of concurrently
   multiplexed bitstreams.  The groups, when unchained, MUST stand on
   their own as a valid concurrently multiplexed bitstream.  The
   following diagram shows a schematic example of such a physical
   bitstream that obeys all the rules of both grouped and chained
   multiplexed bitstreams.

               physical bitstream with pages of
          different logical bitstreams grouped and chained
      -------------------------------------------------------------
      |*A*|*B*|*C*|A|A|C|B|A|B|#A#|C|...|B|C|#B#|#C#|*D*|D|...|#D#|
      -------------------------------------------------------------
       bos bos bos             eos           eos eos bos       eos

   In this example, there are two chained physical bitstreams, the first
   of which is a grouped stream of three logical bitstreams A, B, and C.
   The second physical bitstream is chained after the end of the grouped
   bitstream, which ends after the last eos page of all its grouped
   logical bitstreams.  As can be seen, grouped bitstreams begin
   together - all of the bos pages MUST appear before any data pages.
   It can also be seen that pages of concurrently multiplexed bitstreams
   need not conform to a regular order.  And it can be seen that a
   grouped bitstream can end long before the other bitstreams in the
   group end.
</pre>
 *
 * - oggz_write_feed() will fail, returning an explicit error value, if
 *   an attempt is made to queue a packet in violation of these rules.
 *
 * \section References
 *
 * This introduction to the Ogg format is derived from
 * IETF <a href="http://www.ietf.org/rfc/rfc3533.txt">RFC 3533</a>
 * <i>The Ogg File Format version 0</i> in accordance with the
 * following copyright statement pertaining to the text of RFC 3533:
 *
<pre>
   Copyright (C) The Internet Society (2003).  All Rights Reserved.

   This document and translations of it may be copied and furnished to
   others, and derivative works that comment on or otherwise explain it
   or assist in its implementation may be prepared, copied, published
   and distributed, in whole or in part, without restriction of any
   kind, provided that the above copyright notice and this paragraph are
   included on all such copies and derivative works.  However, this
   document itself may not be modified in any way, such as by removing
   the copyright notice or references to the Internet Society or other
   Internet organizations, except as needed for the purpose of
   developing Internet standards in which case the procedures for
   copyrights defined in the Internet Standards process must be
   followed, or as required to translate it into languages other than
   English.

   The limited permissions granted above are perpetual and will not be
   revoked by the Internet Society or its successors or assigns.

   This document and the information contained herein is provided on an
   "AS IS" basis and THE INTERNET SOCIETY AND THE INTERNET ENGINEERING
   TASK FORCE DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, INCLUDING
   BUT NOT LIMITED TO ANY WARRANTY THAT THE USE OF THE INFORMATION
   HEREIN WILL NOT INFRINGE ANY RIGHTS OR ANY IMPLIED WARRANTIES OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
</pre>
 *
 */

/** \defgroup configuration Configuration
 * \section configure ./configure
 *
 * It is possible to customize the functionality of liboggz
 * by using various ./configure flags when
 * building it from source. You can build a smaller
 * version of liboggz to only read or write.
 * By default, both reading and writing support is built.
 *
 * For general information about using ./configure, see the file
 * \link install INSTALL \endlink
 *
 * \subsection no_encode Removing writing support
 *
 * Configuring with \a --disable-write will remove all support for writing:
 * - All internal write related functions will not be built
 * - Any attempt to call oggz_new(), oggz_open() or oggz_open_stdio()
 *   with \a flags == OGGZ_WRITE will fail, returning NULL
 * - Any attempt to call oggz_write(), oggz_write_output(), oggz_write_feed(),
 *   oggz_write_set_hungry_callback(), or oggz_write_get_next_page_size()
 *   will return OGGZ_ERR_DISABLED
 *
 * \subsection no_decode Removing reading support
 *
 * Configuring with \a --disable-read will remove all support for reading:
 * - All internal reading related functions will not be built
 * - Any attempt to call oggz_new(), oggz_open() or oggz_open_stdio()
 *    with \a flags == OGGZ_READ will fail, returning NULL
 * - Any attempt to call oggz_read(), oggz_read_input(),
 *   oggz_set_read_callback(), oggz_seek(), or oggz_seek_units() will return
 *   OGGZ_ERR_DISABLED
 *
 */

/** \defgroup install Installation
 * \section install INSTALL
 *
 * \include INSTALL
 */

/** \defgroup building Building against liboggz
 *
 *
 * \section autoconf Using GNU autoconf
 *
 * If you are using GNU autoconf, you do not need to call pkg-config
 * directly. Use the following macro to determine if liboggz is
 * available:
 *
 <pre>
 PKG_CHECK_MODULES(OGGZ, oggz >= 0.6.0,
                   HAVE_OGGZ="yes", HAVE_OGGZ="no")
 if test "x$HAVE_OGGZ" = "xyes" ; then
   AC_SUBST(OGGZ_CFLAGS)
   AC_SUBST(OGGZ_LIBS)
 fi
 </pre>
 *
 * If liboggz is found, HAVE_OGGZ will be set to "yes", and
 * the autoconf variables OGGZ_CFLAGS and OGGZ_LIBS will
 * be set appropriately.
 *
 * \section pkg-config Determining compiler options with pkg-config
 *
 * If you are not using GNU autoconf in your project, you can use the
 * pkg-config tool directly to determine the correct compiler options.
 *
 <pre>
 OGGZ_CFLAGS=`pkg-config --cflags oggz`

 OGGZ_LIBS=`pkg-config --libs oggz`
 </pre>
 *
 */

/** \file
 * The liboggz C API.
 *
 * \section general Generic semantics
 *
 * All access is managed via an OGGZ handle. This can be instantiated
 * in one of three ways:
 *
 * - oggz_open() - Open a full pathname
 * - oggz_open_stdio() - Use an already opened FILE *
 * - oggz_new() - Create an anonymous OGGZ object, which you can later
 *   handle via memory buffers
 *
 * To finish using an OGGZ handle, it should be closed with oggz_close().
 *
 * \section reading Reading Ogg data
 *
 * To read from Ogg files or streams you must instantiate an OGGZ handle
 * with flags set to OGGZ_READ, and provide an OggzReadPacket
 * callback with oggz_set_read_callback().
 * See the \ref read_api section for details.
 *
 * \section writing Writing Ogg data
 *
 * To write to Ogg files or streams you must instantiate an OGGZ handle
 * with flags set to OGGZ_WRITE, and provide an OggzWritePacket
 * callback with oggz_set_write_callback().
 * See the \ref write_api section for details.
 *
 * \section seeking Seeking on Ogg data
 *
 * To seek while reading Ogg files or streams you must instantiate an OGGZ
 * handle for reading, and ensure that an \link metric OggzMetric \endlink
 * function is defined to translate packet positions into units such as time.
 * See the \ref seek_api section for details.
 *
 * \section io Overriding the IO methods
 *
 * When an OGGZ handle is instantiated by oggz_open() or oggz_open_stdio(),
 * Oggz uses stdio functions internally to access the raw data. However for
 * some applications, the raw data cannot be accessed via stdio -- this
 * commonly occurs when integrating with media frameworks. For such
 * applications, you can provide Oggz with custom IO methods that it should
 * use to access the raw data. Oggz will then use these custom methods,
 * rather than using stdio methods, to access the raw data internally.
 *
 * For details, see \link oggz_io.h <oggz/oggz_io.h> \endlink.
 *
 * \section headers Headers
 *
 * oggz.h provides direct access to libogg types such as ogg_packet, defined
 * in <ogg/ogg.h>.
 */

/**
 * An opaque handle to an Ogg file. This is returned by oggz_open() or
 * oggz_new(), and is passed to all other oggz_* functions.
 */
typedef void OGGZ;

/**
 * Create a new OGGZ object
 * \param flags OGGZ_READ or OGGZ_WRITE
 * \returns A new OGGZ object
 * \retval NULL on system error; check errno for details
 */
OGGZ * oggz_new (int flags);

/**
 * Open an Ogg file, creating an OGGZ handle for it
 * \param filename The file to open
 * \param flags OGGZ_READ or OGGZ_WRITE
 * \return A new OGGZ handle
 * \retval NULL System error; check errno for details
 */
OGGZ * oggz_open (const char * filename, int flags);

/**
 * Create an OGGZ handle associated with a stdio stream
 * \param file An open FILE handle
 * \param flags OGGZ_READ or OGGZ_WRITE
 * \returns A new OGGZ handle
 * \retval NULL System error; check errno for details
 */
OGGZ * oggz_open_stdio (FILE * file, int flags);

/**
 * Ensure any associated io streams are flushed.
 * \param oggz An OGGZ handle
 * \retval 0 Success
 * \retval OGGZ_ERR_BAD_OGGZ \a oggz does not refer to an existing OGGZ
 * \retval OGGZ_ERR_INVALID Operation not suitable for this OGGZ
 * \retval OGGZ_ERR_SYSTEM System error; check errno for details
 */
int oggz_flush (OGGZ * oggz);

/**
 * Run an OGGZ until completion, or error.
 * This is a convenience function which repeatedly calls oggz_read() or
 * oggz_write() as appropriate.
 * For an OGGZ opened for reading, an OggzReadPacket or OggzReadPage callback
 * should have been set before calling this function.
 * For an OGGZ opened for writing, either an OggzHungry callback should have
 * been set before calling this function, or you can use this function to
 * write out all unwritten Ogg pages which are pending.
 * \param oggz An OGGZ handle previously opened for either reading or writing
 * \retval 0 Success
 * \retval OGGZ_ERR_BAD_OGGZ \a oggz does not refer to an existing OGGZ
 * \retval OGGZ_ERR_INVALID Operation not suitable for this OGGZ
 * \retval OGGZ_ERR_SYSTEM System error; check errno for details
 * \retval OGGZ_ERR_STOP_OK Operation was stopped by a user callback
 * returning OGGZ_STOP_OK
 * \retval OGGZ_ERR_STOP_ERR Operation was stopped by a user callback
 * returning OGGZ_STOP_ERR
 * \retval OGGZ_ERR_RECURSIVE_WRITE Attempt to initiate writing from
 * within an OggzHungry callback
 */
long oggz_run (OGGZ * oggz);

/**
 * Set the blocksize to use internally for oggz_run()
 * \param oggz An OGGZ handle previously opened for either reading or writing
 * \param blocksize The blocksize to use within oggz_run()
 * \retval 0 Success
 * \retval OGGZ_ERR_BAD_OGGZ \a oggz does not refer to an existing OGGZ
 * \retval OGGZ_ERR_INVALID Invalid blocksize (\a run_blocksize <= 0)
 */
int oggz_run_set_blocksize (OGGZ * oggz, long blocksize);

/**
 * Close an OGGZ handle
 * \param oggz An OGGZ handle
 * \retval 0 Success
 * \retval OGGZ_ERR_BAD_OGGZ \a oggz does not refer to an existing OGGZ
 * \retval OGGZ_ERR_SYSTEM System error; check errno for details
 */
int oggz_close (OGGZ * oggz);

/**
 * Determine if a given logical bitstream is at bos (beginning of stream).
 * \param oggz An OGGZ handle
 * \param serialno Identify a logical bitstream within \a oggz, or -1 to
 * query if all logical bitstreams in \a oggz are at bos
 * \retval 1 The given stream is at bos
 * \retval 0 The given stream is not at bos
 * \retval OGGZ_ERR_BAD_SERIALNO \a serialno does not identify an existing
 * logical bitstream in \a oggz.
 */
int oggz_get_bos (OGGZ * oggz, long serialno);

/**
 * Determine if a given logical bitstream is at eos (end of stream).
 * \param oggz An OGGZ handle
 * \param serialno Identify a logical bitstream within \a oggz, or -1 to
 * query if all logical bitstreams in \a oggz are at eos
 * \retval 1 The given stream is at eos
 * \retval 0 The given stream is not at eos
 * \retval OGGZ_ERR_BAD_SERIALNO \a serialno does not identify an existing
 * logical bitstream in \a oggz.
 */
int oggz_get_eos (OGGZ * oggz, long serialno);

/**
 * Query the number of tracks (logical bitstreams). When reading, this
 * number is incremented every time a new track is found, so the returned
 * value is only correct once the OGGZ is no longer at bos (beginning of
 * stream): see oggz_get_bos() for determining this.
 * \param oggz An OGGZ handle
 * \return The number of tracks in OGGZ
 * \retval OGGZ_ERR_BAD_SERIALNO \a serialno does not identify an existing
 * logical bitstream in \a oggz.
 */
int oggz_get_numtracks (OGGZ * oggz);

/**
 * Request a new serialno, as required for a new stream, ensuring the serialno
 * is not yet used for any other streams managed by this OGGZ.
 * \param oggz An OGGZ handle
 * \returns A new serialno, not already occuring in any logical bitstreams
 * in \a oggz.
 */
long oggz_serialno_new (OGGZ * oggz);

/**
 * Return human-readable string representation of a content type
 *
 * \retval string the name of the content type
 * \retval NULL \a content invalid
 */
const char *
oggz_content_type (OggzStreamContent content);

#include <oggz/oggz_off_t.h>
#include <oggz/oggz_read.h>
#include <oggz/oggz_stream.h>
#include <oggz/oggz_seek.h>
#include <oggz/oggz_write.h>
#include <oggz/oggz_io.h>
#include <oggz/oggz_comments.h>
#include <oggz/oggz_deprecated.h>

#ifdef __cplusplus
}
#endif

#endif /* __OGGZ_H__ */
