/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
