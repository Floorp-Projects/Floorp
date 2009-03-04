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

#ifndef __OGGZ_WRITE_H__
#define __OGGZ_WRITE_H__

/** \file
 * Interfaces for writing Ogg files and streams
 */

/** \defgroup force_feed Writing by force feeding Oggz
 *
 * Force feeding involves synchronously:
 * - Creating an \a ogg_packet structure
 * - Adding it to the packet queue with oggz_write_feed()
 * - Calling oggz_write() or oggz_write_output(), repeatedly as necessary,
 *   to generate the Ogg bitstream.
 *
 * This process is illustrated in the following diagram:
 *
 * \image html forcefeed.png
 * \image latex forcefeed.eps "Force Feeding Oggz" width=10cm
 *
 * The following example code generates a stream of ten packets, each
 * containing a single byte ('A', 'B', ... , 'J'):
 *
 * \include write-feed.c
 */

/** \defgroup hungry Writing with OggzHungry callbacks
 *
 * You can add packets to the Oggz packet queue only when it is "hungry"
 * by providing an OggzHungry callback.
 *
 * An OggzHungry callback will:
 * - Create an \a ogg_packet structure
 * - Add it to the packet queue with oggz_write_feed()
 *
 * Once you have set such a callback with oggz_write_set_hungry_callback(),
 * simply call oggz_write() or oggz_write_output() repeatedly, and Oggz
 * will call your callback to provide packets when it is hungry.
 *
 * This process is illustrated in the following diagram:
 *
 * \image html hungry.png
 * \image latex hungry.eps "Using OggzHungry" width=10cm
 *
 * The following example code generates a stream of ten packets, each
 * containing a single byte ('A', 'B', ... , 'J'):
 *
 * \include write-hungry.c
 */

/** \defgroup write_api Oggz Write API
 *
 * Oggz maintains a packet queue, such that you can independently add
 * packets to the queue and write an Ogg bitstream.
 * There are two complementary methods for adding packets to the
 * packet queue.
 *
 * - by \link force_feed force feeding Oggz \endlink
 * - by using \link hungry OggzHungry \endlink callbacks
 *
 * As each packet is enqueued, its validity is checked against the framing
 * constraints outlined in the \link basics Ogg basics \endlink section.
 * If it does not pass these constraints, oggz_write_feed() will fail with
 * an appropriate error code.
 *
 * \note
 * - When writing, you can ensure that a packet starts on a new page
 *   by setting the \a flush parameter of oggz_write_feed() to
 *   \a OGGZ_FLUSH_BEFORE when enqueuing it.
 *   Similarly you can ensure that the last page a packet is written into
 *   won't contain any following packets by setting the \a flush parameter
 *   of oggz_write_feed() to \a OGGZ_FLUSH_AFTER.
 * - The \a OGGZ_FLUSH_BEFORE and \a OGGZ_FLUSH_AFTER flags can be bitwise
 *   OR'd together to ensure that the packet will not share any pages with
 *   any other packets, either before or after.
 *
 * \{
 */

/**
 * This is the signature of a callback which Oggz will call when \a oggz
 * is \link hungry hungry \endlink.
 *
 * \param oggz The OGGZ handle
 * \param empty A value of 1 indicates that the packet queue is currently
 *        empty. A value of 0 indicates that the packet queue is not empty.
 * \param user_data A generic pointer you have provided earlier
 * \retval 0 Continue
 * \retval non-zero Instruct Oggz to stop.
 */
typedef int (*OggzWriteHungry) (OGGZ * oggz, int empty, void * user_data);

/**
 * Set a callback for Oggz to call when \a oggz
 * is \link hungry hungry \endlink.
 *
 * \param oggz An OGGZ handle previously opened for writing
 * \param hungry Your callback function
 * \param only_when_empty When to call: a value of 0 indicates that
 * Oggz should call \a hungry() after each and every packet is written;
 * a value of 1 indicates that Oggz should call \a hungry() only when
 * its packet queue is empty
 * \param user_data Arbitrary data you wish to pass to your callback
 * \retval 0 Success
 * \retval OGGZ_ERR_BAD_OGGZ \a oggz does not refer to an existing OGGZ
 * \retval OGGZ_ERR_INVALID Operation not suitable for this OGGZ
 * \note Passing a value of 0 for \a only_when_empty allows you to feed
 * new packets into \a oggz's packet queue on the fly.
 */
int oggz_write_set_hungry_callback (OGGZ * oggz,
				    OggzWriteHungry hungry,
				    int only_when_empty,
				    void * user_data);
/**
 * Add a packet to \a oggz's packet queue.
 * \param oggz An OGGZ handle previously opened for writing
 * \param op An ogg_packet with all fields filled in
 * \param serialno Identify the logical bitstream in \a oggz to add the
 * packet to
 * \param flush Bitmask of OGGZ_FLUSH_BEFORE, OGGZ_FLUSH_AFTER
 * \param guard A guard for nocopy, NULL otherwise
 * \retval 0 Success
 * \retval OGGZ_ERR_BAD_GUARD \a guard specified has non-zero initialization
 * \retval OGGZ_ERR_BOS Packet would be bos packet of a new logical bitstream,
 *         but oggz has already written one or more non-bos packets in
 *         other logical bitstreams,
 *         and \a oggz is not flagged OGGZ_NONSTRICT
 * \retval OGGZ_ERR_EOS The logical bitstream identified by \a serialno is
 *         already at eos,
 *         and \a oggz is not flagged OGGZ_NONSTRICT
 * \retval OGGZ_ERR_BAD_BYTES \a op->bytes is invalid,
 *         and \a oggz is not flagged OGGZ_NONSTRICT
 * \retval OGGZ_ERR_BAD_B_O_S \a op->b_o_s is invalid,
 *         and \a oggz is not flagged OGGZ_NONSTRICT
 * \retval OGGZ_ERR_BAD_GRANULEPOS \a op->granulepos is less than that of
 *         an earlier packet within this logical bitstream,
 *         and \a oggz is not flagged OGGZ_NONSTRICT
 * \retval OGGZ_ERR_BAD_PACKETNO \a op->packetno is less than that of an
 *         earlier packet within this logical bitstream,
 *         and \a oggz is not flagged OGGZ_NONSTRICT
 * \retval OGGZ_ERR_BAD_SERIALNO \a serialno does not identify an existing
 *         logical bitstream in \a oggz,
 *         and \a oggz is not flagged OGGZ_NONSTRICT
 *         or \a serialno is equal to -1, or \a serialno does not fit in
 *         32 bits, ie. within the range (-(2^31), (2^31)-1)
 * \retval OGGZ_ERR_BAD_OGGZ \a oggz does not refer to an existing OGGZ
 * \retval OGGZ_ERR_INVALID Operation not suitable for this OGGZ
 * \retval OGGZ_ERR_OUT_OF_MEMORY Unable to allocate memory to queue packet
 *
 * \note If \a op->b_o_s is initialized to \a -1 before calling
 *       oggz_write_feed(), Oggz will fill it in with the appropriate
 *       value; ie. 1 for the first packet of a new stream, and 0 otherwise.
 */
int oggz_write_feed (OGGZ * oggz, ogg_packet * op, long serialno, int flush,
		     int * guard);

/**
 * Output data from an OGGZ handle. Oggz will call your write callback
 * as needed.
 *
 * \param oggz An OGGZ handle previously opened for writing
 * \param buf A memory buffer
 * \param n A count of bytes to output
 * \retval "> 0" The number of bytes successfully output
 * \retval 0 End of stream
 * \retval OGGZ_ERR_RECURSIVE_WRITE Attempt to initiate writing from
 * within an OggzHungry callback
 * \retval OGGZ_ERR_BAD_OGGZ \a oggz does not refer to an existing OGGZ
 * \retval OGGZ_ERR_INVALID Operation not suitable for this OGGZ
 * \retval OGGZ_ERR_STOP_OK Writing was stopped by an OggzHungry callback
 * returning OGGZ_STOP_OK
 * \retval OGGZ_ERR_STOP_ERR Reading was stopped by an OggzHungry callback
 * returning OGGZ_STOP_ERR
 */
long oggz_write_output (OGGZ * oggz, unsigned char * buf, long n);

/**
 * Write n bytes from an OGGZ handle. Oggz will call your write callback
 * as needed.
 *
 * \param oggz An OGGZ handle previously opened for writing
 * \param n A count of bytes to be written
 * \retval "> 0" The number of bytes successfully output
 * \retval 0 End of stream
 * \retval OGGZ_ERR_RECURSIVE_WRITE Attempt to initiate writing from
 * within an OggzHungry callback
 * \retval OGGZ_ERR_BAD_OGGZ \a oggz does not refer to an existing OGGZ
 * \retval OGGZ_ERR_INVALID Operation not suitable for this OGGZ
 * \retval OGGZ_ERR_STOP_OK Writing was stopped by an OggzHungry callback
 * returning OGGZ_STOP_OK
 * \retval OGGZ_ERR_STOP_ERR Reading was stopped by an OggzHungry callback
 * returning OGGZ_STOP_ERR
 */
long oggz_write (OGGZ * oggz, long n);

/**
 * Query the number of bytes in the next page to be written.
 *
 * \param oggz An OGGZ handle previously opened for writing
 * \retval ">= 0" The number of bytes in the next page
 * \retval OGGZ_ERR_BAD_OGGZ \a oggz does not refer to an existing OGGZ
 * \retval OGGZ_ERR_INVALID Operation not suitable for this OGGZ
 */
long oggz_write_get_next_page_size (OGGZ * oggz);

/** \}
 */

#endif /* __OGGZ_WRITE_H__ */
