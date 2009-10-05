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
 * oggplay_query.h
 * 
 * @authors
 * Shane Stephens <shane.stephens@annodex.net>
 * Viktor Gal
 */
 
#ifndef __OGGPLAY_QUERY_H__
#define __OGGPLAY_QUERY_H__

#include <oggz/oggz.h>

/**
 * Get the number of tracks in the Ogg container.
 *
 * @param me OggPlay handle
 * @retval "> 0" number of tracks
 * @retval E_OGGPLAY_BAD_OGGPLAY the supplied OggPlay 
 * @retval E_OGGPLAY_BAD_READER 
 * @retval E_OGGPLAY_UNINITIALISED the is not initialised.
 */
int
oggplay_get_num_tracks (OggPlay * me);

/**
 * Retrieve the type of a track.
 * 
 * @param me OggPlay handle
 * @param track_num the desired track's number
 * @retval "> 0" the track's type (see OggzStreamContent)
 * @retval "< 0" error occured
 */
OggzStreamContent
oggplay_get_track_type (OggPlay * me, int track_num);

/**
 * Get a track's type name.
 * 
 * @param me OggPlay handle
 * @param track_num the desired track's number
 * @retval typa name of the track
 * @retval NULL in case of error.
 */
const char *
oggplay_get_track_typename (OggPlay * me, int track_num);

/**
 * Set a track active.
 *
 * @param me OggPlay handle
 * @param track_num the desired track's number for activation
 * @retval E_OGGPLAY_OK on success
 * @retval E_OGGPLAY_BAD_OGGPLAY the supplied OggPlay is invalid
 * @retval E_OGGPLAY_BAD_READER the OggPlayReader associated with the Ogg 
 * container is invalid
 * @retval E_OGGPLAY_UNINITIALISED the tracks are not initialised
 * @retval E_OGGPLAY_BAD_TRACK invalid track number
 * @retval E_OGGPLAY_TRACK_IS_SKELETON the chosen track is a Skeleton track
 * @retval E_OGGPLAY_TRACK_IS_UNKNOWN the chosen track's content type is unknown
 * @retval E_OGGPLAY_TRACK_UNINITIALISED the chosen track was not initialised
 * @retval E_OGGPLAY_TRACK_IS_OVER the track is over.
 */
OggPlayErrorCode
oggplay_set_track_active(OggPlay *me, int track_num);

/**
 * Inactivate a given track.
 *
 * @param me OggPlay handle
 * @param track_num the desired track's number for inactivation
 * @retval E_OGGPLAY_OK on success
 * @retval E_OGGPLAY_BAD_OGGPLAY the supplied OggPlay is invalid
 * @retval E_OGGPLAY_BAD_READER the OggPlayReader associated with the Ogg 
 * container is invalid
 * @retval E_OGGPLAY_UNINITIALISED the tracks are not initialised
 * @retval E_OGGPLAY_BAD_TRACK invalid track number
 * @retval E_OGGPLAY_TRACK_IS_SKELETON the chosen track is a Skeleton track
 * @retval E_OGGPLAY_TRACK_IS_UNKNOWN the chosen track's content type is unknown
 */
OggPlayErrorCode
oggplay_set_track_inactive(OggPlay *me, int track_num);

#endif
