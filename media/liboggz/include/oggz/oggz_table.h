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

#ifndef __OGGZ_TABLE_H__
#define __OGGZ_TABLE_H__

/** \file
 * A lookup table.
 *
 * OggzTable is provided for convenience to allow the storage of
 * serialno-specific data.
 */

/**
 * A table of key-value pairs.
 */
typedef void OggzTable;

/**
 * Instantiate a new OggzTable
 * \returns A new OggzTable
 * \retval NULL Could not allocate memory for table
 */
OggzTable *
oggz_table_new (void);

/**
 * Delete an OggzTable
 * \param table An OggzTable
 */
void
oggz_table_delete (OggzTable * table);

/**
 * Insert an element into a table. If a previous value existed for this key,
 * it is overwritten with the new data element.
 * \param table An OggzTable
 * \param key Key to access this data element
 * \param data The new element to add
 * \retval data If the element was successfully added
 * \retval NULL If adding the element failed due to a realloc() error
 */
void *
oggz_table_insert (OggzTable * table, long key, void * data);

/**
 * Remove the element of an OggzTable indexed by a given key
 * \param table An OggzTable
 * \param key a key
 * \retval 0 Success
 * \retval -1 Not found
 */
int
oggz_table_remove (OggzTable * table, long key);

/**
 * Retrieve the element of an OggzTable indexed by a given key
 * \param table An OggzTable
 * \param key a key
 * \returns The element indexed by \a key
 * \retval NULL \a table is undefined, or no element is indexed by \a key
 */
void *
oggz_table_lookup (OggzTable * table, long key);

/**
 * Query the number of elements in an OggzTable
 * \param table An OggzTable
 * \returns the number of elements in \a table
 */
int
oggz_table_size (OggzTable * table);

/**
 * Retrieve the nth element of an OggzTable, and optionally its key
 * \param table An OggzTable
 * \param n An index into the \a table
 * \param key Return pointer for key corresponding to nth data element
 *        of \a table. Ignored if NULL.
 * \returns The nth data element of \a table
 * \retval NULL \a table is undefined, or \a n is out of range
 */
void *
oggz_table_nth (OggzTable * table, int n, long * key);

#endif /* __OGGZ_TABLE_H__ */
