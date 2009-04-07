#ifdef OS2

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <dirent.h>
#include <errno.h>

/*#ifndef __EMX__ 
#include <libx.h>
#endif */

#define INCL_DOSFILEMGR
#define INCL_DOSERRORS
#include <os2.h>

#if OS2 >= 2
# define FFBUF	FILEFINDBUF3
# define Word	ULONG
  /*
   * LS20 recommends a request count of 100, but according to the
   * APAR text it does not lead to missing files, just to funny
   * numbers of returned entries.
   *
   * LS30 HPFS386 requires a count greater than 2, or some files
   * are missing (those starting with a character less that '.').
   *
   * Novell loses entries which overflow the buffer. In previous
   * versions of dirent2, this could have lead to missing files
   * when the average length of 100 directory entries was 40 bytes
   * or more (quite unlikely for files on a Novell server).
   *
   * Conclusion: Make sure that the entries all fit into the buffer
   * and that the buffer is large enough for more than 2 entries
   * (each entry is at most 300 bytes long). And ignore the LS20
   * effect.
   */
# define Count	25
# define BufSz	(25 * (sizeof(FILEFINDBUF3)+1))
#else
# define FFBUF	FILEFINDBUF
# define Word	USHORT
# define BufSz	1024
# define Count	3
#endif

#if defined(__IBMC__) || defined(__IBMCPP__)
  #define error(rc) _doserrno = rc, errno = EOS2ERR
#elif defined(MICROSOFT)
  #define error(rc) _doserrno = rc, errno = 255
#else
  #define error(rc) errno = 255
#endif

struct _dirdescr {
	HDIR		handle;		/* DosFindFirst handle */
	char		fstype;		/* filesystem type */
	Word		count;		/* valid entries in <ffbuf> */
	long		number;		/* absolute number of next entry */
	int		index;		/* relative number of next entry */
	FFBUF *		next;		/* pointer to next entry */
	char		name[MAXPATHLEN+3]; /* directory name */
	unsigned	attrmask;	/* attribute mask for seekdir */
	struct dirent	entry;		/* buffer for directory entry */
	BYTE		ffbuf[BufSz];
};

/*
 * Return first char of filesystem type, or 0 if unknown.
 */
static char
getFSType(const char *path)
{
	static char cache[1+26];
	char drive[3], info[512];
	Word unit, infolen;
	char r;

	if (isalpha(path[0]) && path[1] == ':') {
		unit = toupper(path[0]) - '@';
		path += 2;
	} else {
		ULONG driveMap;
#if OS2 >= 2
		if (DosQueryCurrentDisk(&unit, &driveMap))
#else
		if (DosQCurDisk(&unit, &driveMap))
#endif
			return 0;
	}

	if ((path[0] == '\\' || path[0] == '/')
	 && (path[1] == '\\' || path[1] == '/'))
		return 0;

	if (cache [unit])
		return cache [unit];

	drive[0] = '@' + unit;
	drive[1] = ':';
	drive[2] = '\0';
	infolen = sizeof info;
#if OS2 >= 2
	if (DosQueryFSAttach(drive, 0, FSAIL_QUERYNAME, (PVOID)info, &infolen))
		return 0;
	if (infolen >= sizeof(FSQBUFFER2)) {
		FSQBUFFER2 *p = (FSQBUFFER2 *)info;
		r = p->szFSDName[p->cbName];
	} else
#else
	if (DosQFSAttach((PSZ)drive, 0, FSAIL_QUERYNAME, (PVOID)info, &infolen, 0))
		return 0;
	if (infolen >= 9) {
		char *p = info + sizeof(USHORT);
		p += sizeof(USHORT) + *(USHORT *)p + 1 + sizeof(USHORT);
		r = *p;
	} else
#endif
		r = 0;
	return cache [unit] = r;
}

char *
abs_path(const char *name, char *buffer, int len)
{
	char buf[4];
	if (isalpha(name[0]) && name[1] == ':' && name[2] == '\0') {
		buf[0] = name[0];
		buf[1] = name[1];
		buf[2] = '.';
		buf[3] = '\0';
		name = buf;
	}
#if OS2 >= 2
	if (DosQueryPathInfo((PSZ)name, FIL_QUERYFULLNAME, buffer, len))
#else
	if (DosQPathInfo((PSZ)name, FIL_QUERYFULLNAME, (PBYTE)buffer, len, 0L))
#endif
		return NULL;
	return buffer;
}

DIR *
openxdir(const char *path, unsigned att_mask)
{
	DIR *dir;
	char name[MAXPATHLEN+3];
	Word rc;

	dir = malloc(sizeof(DIR));
	if (dir == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	strncpy(name, path, MAXPATHLEN);
	name[MAXPATHLEN] = '\0';
	switch (name[strlen(name)-1]) {
	default:
		strcat(name, "\\");
	case '\\':
	case '/':
	case ':':
		;
	}
	strcat(name, ".");
	if (!abs_path(name, dir->name, MAXPATHLEN+1))
		strcpy(dir->name, name);
	if (dir->name[strlen(dir->name)-1] == '\\')
		strcat(dir->name, "*");
	else
		strcat(dir->name, "\\*");

	dir->fstype = getFSType(dir->name);
	dir->attrmask = att_mask | A_DIR;

	dir->handle = HDIR_CREATE;
	dir->count = 100;
#if OS2 >= 2
	rc = DosFindFirst(dir->name, &dir->handle, dir->attrmask,
		dir->ffbuf, sizeof dir->ffbuf, &dir->count, FIL_STANDARD);
#else
	rc = DosFindFirst((PSZ)dir->name, &dir->handle, dir->attrmask,
		(PFILEFINDBUF)dir->ffbuf, sizeof dir->ffbuf, &dir->count, 0);
#endif
	switch (rc) {
	default:
		free(dir);
		error(rc);
		return NULL;
	case NO_ERROR:
	case ERROR_NO_MORE_FILES:
		;
	}

	dir->number = 0;
	dir->index = 0;
	dir->next = (FFBUF *)dir->ffbuf;

	return (DIR *)dir;
}

DIR *
opendir(const char *pathname)
{
	return openxdir(pathname, 0);
}

struct dirent *
readdir(DIR *dir)
{
	static int dummy_ino = 2;

	if (dir->index == dir->count) {
		Word rc;
		dir->count = 100;
#if OS2 >= 2
		rc = DosFindNext(dir->handle, dir->ffbuf,
			sizeof dir->ffbuf, &dir->count);
#else
		rc = DosFindNext(dir->handle, (PFILEFINDBUF)dir->ffbuf,
			sizeof dir->ffbuf, &dir->count);
#endif
		if (rc) {
			error(rc);
			return NULL;
		}

		dir->index = 0;
		dir->next = (FFBUF *)dir->ffbuf;
	}

	if (dir->index == dir->count)
		return NULL;

	memcpy(dir->entry.d_name, dir->next->achName, dir->next->cchName);
	dir->entry.d_name[dir->next->cchName] = '\0';
	dir->entry.d_ino = dummy_ino++;
	dir->entry.d_reclen = dir->next->cchName;
	dir->entry.d_namlen = dir->next->cchName;
	dir->entry.d_size = dir->next->cbFile;
	dir->entry.d_attribute = dir->next->attrFile;
	dir->entry.d_time = *(USHORT *)&dir->next->ftimeLastWrite;
	dir->entry.d_date = *(USHORT *)&dir->next->fdateLastWrite;

	switch (dir->fstype) {
	case 'F': /* FAT */
	case 'C': /* CDFS */
		if (dir->next->attrFile & FILE_DIRECTORY)
			strupr(dir->entry.d_name);
		else
			strlwr(dir->entry.d_name);
	}

#if OS2 >= 2
	dir->next = (FFBUF *)((BYTE *)dir->next + dir->next->oNextEntryOffset);
#else
	dir->next = (FFBUF *)((BYTE *)dir->next->achName + dir->next->cchName + 1);
#endif
	++dir->number;
	++dir->index;

	return &dir->entry;
}

long
telldir(DIR *dir)
{
	return dir->number;
}

void
seekdir(DIR *dir, long off)
{
	if (dir->number > off) {
		char name[MAXPATHLEN+2];
		Word rc;

		DosFindClose(dir->handle);

		strcpy(name, dir->name);
		strcat(name, "*");

		dir->handle = HDIR_CREATE;
		dir->count = 32767;
#if OS2 >= 2
		rc = DosFindFirst(name, &dir->handle, dir->attrmask,
			dir->ffbuf, sizeof dir->ffbuf, &dir->count, FIL_STANDARD);
#else
		rc = DosFindFirst((PSZ)name, &dir->handle, dir->attrmask,
			(PFILEFINDBUF)dir->ffbuf, sizeof dir->ffbuf, &dir->count, 0);
#endif
		switch (rc) {
		default:
			error(rc);
			return;
		case NO_ERROR:
		case ERROR_NO_MORE_FILES:
			;
		}

		dir->number = 0;
		dir->index = 0;
		dir->next = (FFBUF *)dir->ffbuf;
	}

	while (dir->number < off && readdir(dir))
		;
}

void
closedir(DIR *dir)
{
	DosFindClose(dir->handle);
	free(dir);
}

/*****************************************************************************/

#ifdef TEST

main(int argc, char **argv)
{
	int i;
	DIR *dir;
	struct dirent *ep;

	for (i = 1; i < argc; ++i) {
		dir = opendir(argv[i]);
		if (!dir)
			continue;
		while (ep = readdir(dir))
			if (strchr("\\/:", argv[i] [strlen(argv[i]) - 1]))
				printf("%s%s\n", argv[i], ep->d_name);
			else
				printf("%s/%s\n", argv[i], ep->d_name);
		closedir(dir);
	}

	return 0;
}

#endif

#endif /* OS2 */

