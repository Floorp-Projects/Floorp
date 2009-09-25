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

/** @file
 * oggplay_seek.h
 * 
 * @authors
 * Shane Stephens <shane.stephens@annodex.net>
 */

#ifndef __OGGPLAY_SEEK_H__
#define __OGGPLAY_SEEK_H__

/**
 * Seeks to a requested position.
 * 
 * @param me OggPlay handle associated with the stream
 * @param milliseconds
 * @retval E_OGGPLAY_OK on success
 * @retval E_OGGPLAY_BAD_OGGPLAY the supplied OggPlay handle was invalid
 * @retval E_OGGPLAY_CANT_SEEK error occured while trying to seek
 * @retval E_OGGPLAY_OUT_OF_MEMORY ran out of memory while trying to allocate the new buffer and trash.
 */
OggPlayErrorCode
oggplay_seek(OggPlay *me, ogg_int64_t milliseconds);

/**
 * Seeks to key frame before |milliseconds|.
 */
OggPlayErrorCode
oggplay_seek_to_keyframe(OggPlay *me,
                         ogg_int64_t milliseconds,
                         ogg_int64_t offset_begin,
                         ogg_int64_t offset_end);

#endif
