/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/* zip.h
 * Structures and functions for creating ZIP archives.
 */
#ifndef ZIP_H
#define ZIP_H

/* For general information on ZIP formats, you can look at jarfile.h
 * in ns/security/lib/jar.  Or look it up on the web...
 */

/* One entry in a ZIPfile.  This corresponds to one file in the archive.
 * We have to save this information because first all the files go into
 * the archive with local headers; then at the end of the file we have to
 * put the central directory entries for each file.
 */
typedef struct ZIPentry_s {
	struct ZipLocal local;		/* local header info */
	struct ZipCentral central;	/* central directory info */
	char *filename;				/* name of file */
	char *comment;				/* comment for this file -- optional */

	struct ZIPentry_s *next;
} ZIPentry;

/* This structure contains the necessary data for putting a ZIP file
 * together.  Has some overall information and a list of ZIPentrys.
 */
typedef struct ZIPfile_s {
	char *filename;	/* ZIP file name */
	char *comment;	/* ZIP file comment -- may be NULL */
	PRFileDesc *fp;	/* ZIP file pointer */
	ZIPentry *list;	/* one entry for each file in the archive */
	unsigned int time;	/* the GMT time of creation, in DOS format */
	unsigned int date;  /* the GMT date of creation, in DOS format */
	unsigned long central_start; /* starting offset of central directory */
	unsigned long central_end; /*index right after the last byte of central*/
} ZIPfile;


/* Open a new ZIP file.  Takes the name of the zip file and an optional
 * comment to be included in the file.  Returns a new ZIPfile structure
 * which is used by JzipAdd and JzipClose
 */
ZIPfile* JzipOpen(char *filename, char *comment);

/* Add a file to a ZIP archive.  Fullname is the path relative to the
 * current directory.  Filename is what the name will be stored as in the
 * archive, and thus what it will be restored as.  zipfile is a structure
 * returned from a previous call to JzipOpen.
 *
 * Non-zero return code means error (although usually the function will
 * call exit() rather than return an error--gotta fix this).
 */
int JzipAdd(char *fullname, char *filename, ZIPfile *zipfile,
	int compression_level);

/* Finalize a ZIP archive.  Adds all the footer information to the end of
 * the file and closes it.  Also DELETES THE ZIPFILE STRUCTURE that was
 * passed in.  So you never have to allocate or free a ZIPfile yourself.
 *
 * Non-zero return code means error (although usually the function will
 * call exit() rather than return an error--gotta fix this).
 */
int JzipClose (ZIPfile *zipfile);


#endif /* ZIP_H */
