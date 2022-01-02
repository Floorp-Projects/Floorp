
/* ---------------------------------------------------------------------------
    Stuff to fake unix file I/O on windows boxes
    ------------------------------------------------------------------------*/

#ifndef WINFILE_H
#define WINFILE_H

#ifdef _WINDOWS
/* hacked out of <dirent.h> on an SGI */
#if defined(XP_WIN32) || defined(_WIN32)
/* 32-bit stuff here */
#include <windows.h>
#include <stdlib.h>
#ifdef __MINGW32__
#include <sys/types.h>
#include <sys/stat.h>
#else
#include <sys\types.h>
#include <sys\stat.h>
#endif

typedef struct DIR_Struct {
    void* directoryPtr;
    WIN32_FIND_DATA data;
} DIR;

#define _ST_FSTYPSZ 16

#if !defined(__BORLANDC__) && !defined(__GNUC__)
typedef unsigned long mode_t;
typedef long uid_t;
typedef long gid_t;
typedef long off_t;
typedef unsigned long nlink_t;
#endif

typedef struct timestruc {
    time_t tv_sec; /* seconds */
    long tv_nsec;  /* and nanoseconds */
} timestruc_t;

struct dirent {              /* data from readdir() */
    ino_t d_ino;             /* inode number of entry */
    off_t d_off;             /* offset of disk direntory entry */
    unsigned short d_reclen; /* length of this record */
    char d_name[_MAX_FNAME]; /* name of file */
};

#if !defined(__BORLANDC__) && !defined(__GNUC__)
#define S_ISDIR(s) ((s)&_S_IFDIR)
#endif

#else /* _WIN32 */
/* 16-bit windows stuff */

#include <sys\types.h>
#include <sys\stat.h>
#include <dos.h>

/*	Getting cocky to support multiple file systems */
typedef struct dirStruct_tag {
    struct _find_t file_data;
    char c_checkdrive;
} dirStruct;

typedef struct DIR_Struct {
    void* directoryPtr;
    dirStruct data;
} DIR;

#define _ST_FSTYPSZ 16
typedef unsigned long mode_t;
typedef long uid_t;
typedef long gid_t;
typedef long off_t;
typedef unsigned long nlink_t;

typedef struct timestruc {
    time_t tv_sec; /* seconds */
    long tv_nsec;  /* and nanoseconds */
} timestruc_t;

struct dirent {              /* data from readdir() */
    ino_t d_ino;             /* inode number of entry */
    off_t d_off;             /* offset of disk direntory entry */
    unsigned short d_reclen; /* length of this record */
#ifdef XP_WIN32
    char d_name[_MAX_FNAME]; /* name of file */
#else
    char d_name[20]; /* name of file */
#endif
};

#define S_ISDIR(s) ((s)&_S_IFDIR)

#endif /* 16-bit windows */

#define CONST const

#endif /* _WINDOWS */

#endif /* WINFILE_H */
