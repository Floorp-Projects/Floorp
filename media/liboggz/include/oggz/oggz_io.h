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

#ifndef __OGGZ_IO_H__
#define __OGGZ_IO_H__

/** \file
 * Overriding the functions used for input and output of raw data.
 *
 * OggzIO provides a way of overriding the functions Oggz uses to access
 * its raw input or output data. This is required in many situations where
 * the raw stream cannot be accessed via stdio, but can be accessed by
 * other means. This is typically useful within media frameworks, where
 * accessing and moving around in the data is possible only using methods
 * provided by the framework.
 *
 * The functions you provide for overriding IO will be used by Oggz
 * whenever you call oggz_read() or oggz_write(). They will also be
 * used repeatedly by Oggz when you call oggz_seek().
 *
 * \note Opening a file with oggz_open() or oggz_open_stdio() is equivalent
 * to calling oggz_new() and setting stdio based functions for data IO.
 */

/**
 * This is the signature of a function which you provide for Oggz
 * to call when it needs to acquire raw input data.
 *
 * \param user_handle A generic pointer you have provided earlier
 * \param n The length in bytes that Oggz wants to read
 * \param buf The buffer that you read data into
 * \retval ">  0" The number of bytes successfully read into the buffer
 * \retval 0 to indicate that there is no more data to read (End of file)
 * \retval "<  0" An error condition
 */
typedef size_t (*OggzIORead) (void * user_handle, void * buf, size_t n);

/**
 * This is the signature of a function which you provide for Oggz
 * to call when it needs to output raw data.
 *
 * \param user_handle A generic pointer you have provided earlier
 * \param n The length in bytes of the data
 * \param buf A buffer containing data to write
 * \retval ">= 0" The number of bytes successfully written (may be less than
 * \a n if a write error has occurred)
 * \retval "<  0" An error condition
 */
typedef size_t (*OggzIOWrite) (void * user_handle, void * buf, size_t n);

/**
 * This is the signature of a function which you provide for Oggz
 * to call when it needs to seek on the raw input or output data.
 *
 * \param user_handle A generic pointer you have provided earlier
 * \param offset The offset in bytes to seek to
 * \param whence SEEK_SET, SEEK_CUR or SEEK_END (as for stdio.h)
 * \retval ">= 0" The offset seeked to
 * \retval "<  0" An error condition
 *
 * \note If you provide an OggzIOSeek function, you MUST also provide
 * an OggzIOTell function, or else all your seeks will fail.
 */
typedef int (*OggzIOSeek) (void * user_handle, long offset, int whence);

/**
 * This is the signature of a function which you provide for Oggz
 * to call when it needs to determine the current offset of the raw
 * input or output data.
 *
 * \param user_handle A generic pointer you have provided earlier
 * \retval ">= 0" The offset
 * \retval "<  0" An error condition
 */
typedef long (*OggzIOTell) (void * user_handle);

/**
 * This is the signature of a function which you provide for Oggz
 * to call when it needs to flush the output data. The behaviour
 * of this function is similar to that of fflush() in stdio.
 *
 * \param user_handle A generic pointer you have provided earlier
 * \retval 0 Success
 * \retval "<  0" An error condition
 */
typedef int (*OggzIOFlush) (void * user_handle);


/**
 * Set a function for Oggz to call when it needs to read input data.
 *
 * \param oggz An OGGZ handle
 * \param read Your reading function
 * \param user_handle Any arbitrary data you wish to pass to the function
 * \retval 0 Success
 * \retval OGGZ_ERR_BAD_OGGZ \a oggz does not refer to an existing OGGZ
 * \retval OGGZ_ERR_INVALID Operation not suitable for this OGGZ; \a oggz not
 * open for reading.
 * \retval OGGZ_ERR_OUT_OF_MEMORY Out of memory
 */
int oggz_io_set_read (OGGZ * oggz, OggzIORead read, void * user_handle);

/**
 * Retrieve the user_handle associated with the function you have provided
 * for reading input data.
 *
 * \param oggz An OGGZ handle
 * \returns the associated user_handle
 */
void * oggz_io_get_read_user_handle (OGGZ * oggz);

/**
 * Set a function for Oggz to call when it needs to write output data.
 *
 * \param oggz An OGGZ handle
 * \param write Your writing function
 * \param user_handle Any arbitrary data you wish to pass to the function
 * \retval 0 Success
 * \retval OGGZ_ERR_BAD_OGGZ \a oggz does not refer to an existing OGGZ
 * \retval OGGZ_ERR_INVALID Operation not suitable for this OGGZ; \a oggz not
 * open for writing.
 * \retval OGGZ_ERR_OUT_OF_MEMORY Out of memory
 */
int oggz_io_set_write (OGGZ * oggz, OggzIOWrite write, void * user_handle);

/**
 * Retrieve the user_handle associated with the function you have provided
 * for writing output data.
 *
 * \param oggz An OGGZ handle
 * \returns the associated user_handle
 */
void * oggz_io_get_write_user_handle (OGGZ * oggz);

/**
 * Set a function for Oggz to call when it needs to seek on its raw data.
 *
 * \param oggz An OGGZ handle
 * \param seek Your seeking function
 * \param user_handle Any arbitrary data you wish to pass to the function
 * \retval 0 Success
 * \retval OGGZ_ERR_BAD_OGGZ \a oggz does not refer to an existing OGGZ
 * \retval OGGZ_ERR_INVALID Operation not suitable for this OGGZ
 * \retval OGGZ_ERR_OUT_OF_MEMORY Out of memory
 *
 * \note If you provide an OggzIOSeek function, you MUST also provide
 * an OggzIOTell function, or else all your seeks will fail.
 */
int oggz_io_set_seek (OGGZ * oggz, OggzIOSeek seek, void * user_handle);

/**
 * Retrieve the user_handle associated with the function you have provided
 * for seeking on input or output data.
 *
 * \param oggz An OGGZ handle
 * \returns the associated user_handle
 */
void * oggz_io_get_seek_user_handle (OGGZ * oggz);

/**
 * Set a function for Oggz to call when it needs to determine the offset
 * within its input data (if OGGZ_READ) or output data (if OGGZ_WRITE).
 *
 * \param oggz An OGGZ handle
 * \param tell Your tell function
 * \param user_handle Any arbitrary data you wish to pass to the function
 * \retval 0 Success
 * \retval OGGZ_ERR_BAD_OGGZ \a oggz does not refer to an existing OGGZ
 * \retval OGGZ_ERR_INVALID Operation not suitable for this OGGZ
 * \retval OGGZ_ERR_OUT_OF_MEMORY Out of memory
 */
int oggz_io_set_tell (OGGZ * oggz, OggzIOTell tell, void * user_handle);

/**
 * Retrieve the user_handle associated with the function you have provided
 * for determining the current offset in input or output data.
 *
 * \param oggz An OGGZ handle
 * \returns the associated user_handle
 */
void * oggz_io_get_tell_user_handle (OGGZ * oggz);

/**
 * Set a function for Oggz to call when it needs to flush its output. The
 * meaning of this is similar to that of fflush() in stdio.
 *
 * \param oggz An OGGZ handle
 * \param flush Your flushing function
 * \param user_handle Any arbitrary data you wish to pass to the function
 * \retval 0 Success
 * \retval OGGZ_ERR_BAD_OGGZ \a oggz does not refer to an existing OGGZ
 * \retval OGGZ_ERR_INVALID Operation not suitable for this OGGZ; \a oggz not
 * open for writing.
 * \retval OGGZ_ERR_OUT_OF_MEMORY Out of memory
 */
int oggz_io_set_flush (OGGZ * oggz, OggzIOFlush flush, void * user_handle);

/**
 * Retrieve the user_handle associated with the function you have provided
 * for flushing output.
 *
 * \param oggz An OGGZ handle
 * \returns the associated user_handle
 */
void * oggz_io_get_flush_user_handle (OGGZ * oggz);

#endif /* __OGGZ_IO_H__ */
