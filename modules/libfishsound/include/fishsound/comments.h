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

#ifndef __FISH_SOUND_COMMENT_H__
#define __FISH_SOUND_COMMENT_H__

/** \file
 * Encoding and decoding of comments.
 *
 * Vorbis and Speex bitstreams
 * use a comment format called "Vorbiscomment", defined 
 * <a href="http://www.xiph.org/ogg/vorbis/doc/v-comment.html">here</a>.
 * Many standard comment names (such as TITLE, COPYRIGHT and GENRE) are
 * defined in that document.
 *
 * The following general features of Vorbiscomment are relevant to this API:
 * - Each stream has one comment packet, which occurs before any encoded
 *   audio data in the stream.
 * - When encoding, FishSound will generate the comment block and pass it
 *   to the encoded() callback in sequence, just like any other packet.
 *   Hence, all comments must be set before any call to fish_sound_encode_*().
 * - When decoding, FishSound will decode the comment block before calling
 *   the first decoded() callback. Hence, retrieving comment data is possible
 *   from as soon as the decoded() callback is first called.
 *
 * Each comment block contains one Vendor string, which can be retrieved
 * with fish_sound_comment_get_vendor(). When encoding, this string is
 * effectively fixed by the codec libraries; it cannot be set by the
 * application.
 *
 * The rest of a comment block consists of \a name = \a value pairs, with
 * the following restrictions:
 * - Both the \a name and \a value must be non-empty
 * - The \a name is case-insensitive and must consist of ASCII within the
 *   range 0x20 to 0x7D inclusive, 0x3D ('=') excluded.
 * - The \a name is not unique; multiple entries may exist with equivalent
 *   \a name within a Vorbiscomment block.
 * - The \a value may be any UTF-8 string.
 *
 * \section comments_get Retrieving comments
 *
 * FishSound contains API methods to iterate through all comments associated
 * with a FishSound* handle (fish_sound_comment_first() and
 * fish_sound_comment_next(), and to iterate through comments matching a
 * particular name (fish_sound_comment_first_byname() and
 * fish_sound_comment_next_byname()). Given that multiple comments may exist
 * with the same \a name, you should not use
 * fish_sound_comment_first_byname() as a simple "get" function.
 *
 * \section comments_set Encoding comments
 * 
 * For encoding, FishSound contains API methods for adding comments
 * (fish_sound_comment_add() and fish_sound_comment_add_byname()
 * and for removing comments
 * (fish_sound_comment_remove() and fish_sound_comment_remove_byname()).
 */

#include <fishsound/fishsound.h>

/**
 * A comment.
 */
typedef struct {
  /** The name of the comment, eg. "AUTHOR" */
  char * name;

  /** The value of the comment, as UTF-8 */
  char * value;
} FishSoundComment;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Retrieve the vendor string.
 * \param fsound A FishSound* handle
 * \returns A read-only copy of the vendor string
 * \retval NULL No vendor string is associated with \a fsound,
 *              or \a fsound is NULL.
 */
const char *
fish_sound_comment_get_vendor (FishSound * fsound);


/**
 * Retrieve the first comment.
 * \param fsound A FishSound* handle
 * \returns A read-only copy of the first comment, or NULL if no comments
 * exist for this FishSound* object.
 */
const FishSoundComment *
fish_sound_comment_first (FishSound * fsound);

/**
 * Retrieve the next comment.
 * \param fsound A FishSound* handle
 * \param comment The previous comment.
 * \returns A read-only copy of the comment immediately following the given
 * comment.
 */
const FishSoundComment *
fish_sound_comment_next (FishSound * fsound, const FishSoundComment * comment);

/**
 * Retrieve the first comment with a given name.
 * \param fsound A FishSound* handle
 * \param name the name of the comment to retrieve.
 * \returns A read-only copy of the first comment matching the given \a name.
 * \retval NULL no match was found.
 * \note If \a name is NULL, the behaviour is the same as for
 *   fish_sound_comment_first()
 */
const FishSoundComment *
fish_sound_comment_first_byname (FishSound * fsound, char * name);

/**
 * Retrieve the next comment following and with the same name as a given
 * comment.
 * \param fsound A FishSound* handle
 * \param comment A comment
 * \returns A read-only copy of the next comment with the same name as
 *          \a comment.
 * \retval NULL no further comments with the same name exist for
 *              this FishSound* object.
 */
const FishSoundComment *
fish_sound_comment_next_byname (FishSound * fsound,
				const FishSoundComment * comment);

/**
 * Add a comment
 * \param fsound A FishSound* handle (created with mode FISH_SOUND_ENCODE)
 * \param comment The comment to add
 * \retval 0 Success
 * \retval FISH_SOUND_ERR_BAD \a fsound is not a valid FishSound* handle
 * \retval FISH_SOUND_ERR_INVALID Operation not suitable for this FishSound
 */
int
fish_sound_comment_add (FishSound * fsound, FishSoundComment * comment);

/**
 * Add a comment by name and value.
 * \param fsound A FishSound* handle (created with mode FISH_SOUND_ENCODE)
 * \param name The name of the comment to add
 * \param value The contents of the comment to add
 * \retval 0 Success
 * \retval FISH_SOUND_ERR_BAD \a fsound is not a valid FishSound* handle
 * \retval FISH_SOUND_ERR_INVALID Operation not suitable for this FishSound
 */
int
fish_sound_comment_add_byname (FishSound * fsound, const char * name,
			       const char * value);

/**
 * Remove a comment
 * \param fsound A FishSound* handle (created with FISH_SOUND_ENCODE)
 * \param comment The comment to remove.
 * \retval 1 Success: comment removed
 * \retval 0 No-op: comment not found, nothing to remove
 * \retval FISH_SOUND_ERR_BAD \a fsound is not a valid FishSound* handle
 * \retval FISH_SOUND_ERR_INVALID Operation not suitable for this FishSound
 */
int
fish_sound_comment_remove (FishSound * fsound, FishSoundComment * comment);

/**
 * Remove all comments with a given name.
 * \param fsound A FishSound* handle (created with FISH_SOUND_ENCODE)
 * \param name The name of the comments to remove
 * \retval ">= 0" The number of comments removed
 * \retval FISH_SOUND_ERR_BAD \a fsound is not a valid FishSound* handle
 * \retval FISH_SOUND_ERR_INVALID Operation not suitable for this FishSound
 */
int
fish_sound_comment_remove_byname (FishSound * fsound, char * name);

#ifdef __cplusplus
}
#endif

#endif /* __FISH_SOUND_COMMENTS_H__ */
