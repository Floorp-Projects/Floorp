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

#ifndef __FS_VECTOR_H__
#define __FS_VECTOR_H__

typedef void FishSoundVector;

typedef int (*FishSoundFunc) (void * data);
typedef int (*FishSoundCmpFunc) (void * data1, void * data2);

FishSoundVector *
fs_vector_new (FishSoundCmpFunc cmp);

void
fs_vector_delete (FishSoundVector * vector);

void *
fs_vector_nth (FishSoundVector * vector, int n);

int
fs_vector_find_index (FishSoundVector * vector, const void * data);

void *
fs_vector_find (FishSoundVector * vector, const void * data);

int
fs_vector_foreach (FishSoundVector * vector, FishSoundFunc func);

int
fs_vector_size (FishSoundVector * vector);

/**
 * Add an element to a vector.
 * \param vector An FishSoundVector
 * \param data The new element to add
 * \retval data If the element was successfully added
 * \retval NULL If adding the element failed due to a realloc() error
 */
void *
fs_vector_insert (FishSoundVector * vector, void * data);

/**
 * Remove a (void *) element of a vector
 * \retval \a vector on success
 * \retval NULL on failure (realloc error)
 */
FishSoundVector *
fs_vector_remove (FishSoundVector * vector, void * data);

#endif /* __FS_VECTOR_H__ */
