/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Daniel Veditz <dveditz@netscape.com>
 */

#ifndef _zipfile_h
#define _zipfile_h
/* 
 * This module implements a simple archive extractor for the PKZIP format.
 *
 * All functions return a status/error code, and have an opaque hZip argument 
 * that represents an open archive.
 *
 * Currently only compression mode 8 (or none) is supported.
 */


#define ZIP_OK                 0
#define ZIP_ERR_GENERAL       -1
#define ZIP_ERR_MEMORY        -2
#define ZIP_ERR_DISK          -3
#define ZIP_ERR_CORRUPT       -4
#define ZIP_ERR_PARAM         -5
#define ZIP_ERR_FNF           -6
#define ZIP_ERR_UNSUPPORTED   -7
#define ZIP_ERR_SMALLBUF      -8

#ifdef STANDALONE

PR_BEGIN_EXTERN_C

/* Open and close the archive 
 *
 * If successful OpenArchive returns a handle in the hZip parameter
 * that must be passed to all subsequent operations on the archive
 */
PR_EXTERN(PRInt32) ZIP_OpenArchive( const char * zipname, void** hZip );
PR_EXTERN(PRInt32) ZIP_CloseArchive( void** hZip );


/* Test the integrity of every item in this open archive
 * by verifying each item's checksum against the stored
 * CRC32 value.
 */
PR_EXTERN(PRInt32) ZIP_TestArchive( void* hZip );

/* Extract the named file in the archive to disk. 
 * This function will happily overwrite an existing Outfile if it can.
 * It's up to the caller to detect or move it out of the way if it's important.
 */
PR_EXTERN(PRInt32) ZIP_ExtractFile( void* hZip, const char * filename, const char * outname );


/* Functions to list the files contained in the archive
 *
 * FindInit() initializes the search with the pattern and returns a find token, 
 * or NULL on an error.  Then FindNext() is called with the token to get the 
 * matching filenames if any. When done you must call FindFree() with the 
 * token to release memory.
 * 
 * a NULL pattern will find all the files in the archive, otherwise the 
 * pattern must be a shell regexp type pattern.
 *
 * if a matching filename is too small for the passed buffer FindNext()
 * will return ZIP_ERR_SMALLBUF. When no more matches can be found in
 * the archive it will return ZIP_ERR_FNF
 */
PR_EXTERN(void*) ZIP_FindInit( void* hZip, const char * pattern );
PR_EXTERN(PRInt32) ZIP_FindNext( void* hFind, char * outbuf, PRUint16 bufsize );
PR_EXTERN(PRInt32) ZIP_FindFree( void* hFind );


PR_END_EXTERN_C

#endif /* STANDALONE */


#endif /* _zipfile_h */
