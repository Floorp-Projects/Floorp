/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nspr.h"
#include "prpriv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WIN32
#include <windows.h>
#include <process.h>
#endif
#if defined(_PR_PTHREADS)
#include <pthread.h>
#endif
#ifdef SYMBIAN
#include <getopt.h>
#endif

#if defined(XP_OS2)
#define INCL_DOSFILEMGR
#include <os2.h>
#include <getopt.h>
#include <errno.h>
#endif /* XP_OS2 */

static int _debug_on = 0;

#ifdef WINCE
#define setbuf(x,y)
#endif

#ifdef XP_WIN
#define mode_t int
#endif

#define DPRINTF(arg) if (_debug_on) printf arg

PRLock *lock;
PRMonitor *mon;
PRInt32 count;
int thread_count;

#ifdef WIN16
#define	BUF_DATA_SIZE	256 * 120
#else
#define	BUF_DATA_SIZE	256 * 1024
#endif

#define NUM_RDWR_THREADS	10
#define NUM_DIRTEST_THREADS	4
#define CHUNK_SIZE 512

typedef struct buffer {
	char	data[BUF_DATA_SIZE];
} buffer;

typedef struct File_Rdwr_Param {
	char	*pathname;
	char	*buf;
	int	offset;
	int	len;
} File_Rdwr_Param;

#ifdef XP_PC
#ifdef XP_OS2
char *TEST_DIR = "prdir";
#else
char *TEST_DIR = "C:\\temp\\prdir";
#endif
char *FILE_NAME = "pr_testfile";
char *HIDDEN_FILE_NAME = "hidden_pr_testfile";
#else
#ifdef SYMBIAN
char *TEST_DIR = "c:\\data\\testfile_dir";
#else
char *TEST_DIR = "/tmp/testfile_dir";
#endif
char *FILE_NAME = "pr_testfile";
char *HIDDEN_FILE_NAME = ".hidden_pr_testfile";
#endif
buffer *in_buf, *out_buf;
char pathname[256], renamename[256];
#ifdef WINCE
WCHAR wPathname[256];
#endif
#define TMPDIR_LEN	64
char testdir[TMPDIR_LEN];
static PRInt32 PR_CALLBACK DirTest(void *argunused);
PRInt32 dirtest_failed = 0;

PRThread* create_new_thread(PRThreadType type,
							void (*start)(void *arg),
							void *arg,
							PRThreadPriority priority,
							PRThreadScope scope,
							PRThreadState state,
							PRUint32 stackSize, PRInt32 index)
{
PRInt32 native_thread = 0;

	PR_ASSERT(state == PR_UNJOINABLE_THREAD);

#if defined(_PR_PTHREADS) || defined(WIN32) || defined(XP_OS2)

	switch(index %  4) {
		case 0:
			scope = (PR_LOCAL_THREAD);
			break;
		case 1:
			scope = (PR_GLOBAL_THREAD);
			break;
		case 2:
			scope = (PR_GLOBAL_BOUND_THREAD);
			break;
		case 3:
			native_thread = 1;
			break;
		default:
			PR_NOT_REACHED("Invalid scope");
			break;
	}
	if (native_thread) {
#if defined(_PR_PTHREADS)
		pthread_t tid;
		if (!pthread_create(&tid, NULL, start, arg))
			return((PRThread *) tid);
		else
			return (NULL);
#elif defined(XP_OS2)
        TID tid;

        tid = (TID)_beginthread((void(* _Optlink)(void*))start,
                                NULL, 32768, arg);
        if (tid == -1) {
          printf("_beginthread failed. errno %d\n", errno);
          return (NULL);
        }
        else
          return((PRThread *) tid);
#else
		HANDLE thandle;
		unsigned tid;
		
		thandle = (HANDLE) _beginthreadex(
						NULL,
						stackSize,
						(unsigned (__stdcall *)(void *))start,
						arg,
						STACK_SIZE_PARAM_IS_A_RESERVATION,
						&tid);		
		return((PRThread *) thandle);
#endif
	} else {
		return(PR_CreateThread(type,start,arg,priority,scope,state,stackSize));
	}
#else
	return(PR_CreateThread(type,start,arg,priority,scope,state,stackSize));
#endif
}

static void PR_CALLBACK File_Write(void *arg)
{
PRFileDesc *fd_file;
File_Rdwr_Param *fp = (File_Rdwr_Param *) arg;
char *name, *buf;
int offset, len;

	setbuf(stdout, NULL);
	name = fp->pathname;
	buf = fp->buf;
	offset = fp->offset;
	len = fp->len;
	
	fd_file = PR_Open(name, PR_RDWR | PR_CREATE_FILE, 0777);
	if (fd_file == NULL) {
		printf("testfile failed to create/open file %s\n",name);
		return;
	}
	if (PR_Seek(fd_file, offset, PR_SEEK_SET) < 0) {
		printf("testfile failed to seek in file %s\n",name);
		return;
	}	
	if ((PR_Write(fd_file, buf, len)) < 0) {
		printf("testfile failed to write to file %s\n",name);
		return;
	}	
	DPRINTF(("Write out_buf[0] = 0x%x\n",(*((int *) buf))));
	PR_Close(fd_file);
	PR_DELETE(fp);

	PR_EnterMonitor(mon);
	--thread_count;
	PR_Notify(mon);
	PR_ExitMonitor(mon);
}

static void PR_CALLBACK File_Read(void *arg)
{
PRFileDesc *fd_file;
File_Rdwr_Param *fp = (File_Rdwr_Param *) arg;
char *name, *buf;
int offset, len;

	setbuf(stdout, NULL);
	name = fp->pathname;
	buf = fp->buf;
	offset = fp->offset;
	len = fp->len;
	
	fd_file = PR_Open(name, PR_RDONLY, 0);
	if (fd_file == NULL) {
		printf("testfile failed to open file %s\n",name);
		return;
	}
	if (PR_Seek(fd_file, offset, PR_SEEK_SET) < 0) {
		printf("testfile failed to seek in file %s\n",name);
		return;
	}	
	if ((PR_Read(fd_file, buf, len)) < 0) {
		printf("testfile failed to read to file %s\n",name);
		return;
	}	
	DPRINTF(("Read in_buf[0] = 0x%x\n",(*((int *) buf))));
	PR_Close(fd_file);
	PR_DELETE(fp);

	PR_EnterMonitor(mon);
	--thread_count;
	PR_Notify(mon);
	PR_ExitMonitor(mon);
}


static PRInt32 Misc_File_Tests(char *pathname)
{
PRFileDesc *fd_file;
int len, rv = 0;
PRFileInfo file_info, file_info1;
char tmpname[1024];

	setbuf(stdout, NULL);
	/*
	 * Test PR_Available, PR_Seek, PR_GetFileInfo, PR_Rename, PR_Access
	 */

	fd_file = PR_Open(pathname, PR_RDWR | PR_CREATE_FILE, 0777);

	if (fd_file == NULL) {
		printf("testfile failed to create/open file %s\n",pathname);
		return -1;
	}
	if (PR_GetOpenFileInfo(fd_file, &file_info) < 0) {
		printf("testfile PR_GetFileInfo failed on file %s\n",pathname);
		rv = -1;
		goto cleanup;
	}
	if (PR_Access(pathname, PR_ACCESS_EXISTS) != 0) {
		printf("testfile PR_Access failed on file %s\n",pathname);
		rv = -1;
		goto cleanup;
	}
	if (PR_Access(pathname, PR_ACCESS_WRITE_OK) != 0) {
		printf("testfile PR_Access failed on file %s\n",pathname);
		rv = -1;
		goto cleanup;
	}
	if (PR_Access(pathname, PR_ACCESS_READ_OK) != 0) {
		printf("testfile PR_Access failed on file %s\n",pathname);
		rv = -1;
		goto cleanup;
	}


	if (PR_GetFileInfo(pathname, &file_info) < 0) {
		printf("testfile PR_GetFileInfo failed on file %s\n",pathname);
		rv = -1;
		goto cleanup;
	}
	if (file_info.type != PR_FILE_FILE) {
	printf(
	"testfile: Error - PR_GetFileInfo returned incorrect type for file %s\n",
		pathname);
		rv = -1;
		goto cleanup;
	}
	if (file_info.size != 0) {
		printf(
		"testfile PR_GetFileInfo returned incorrect size (%d should be 0) for file %s\n",
		file_info.size, pathname);
		rv = -1;
		goto cleanup;
	}
	file_info1 = file_info;

	len = PR_Available(fd_file);
	if (len < 0) {
		printf("testfile PR_Available failed on file %s\n",pathname);
		rv = -1;
		goto cleanup;
	} else if (len != 0) {
		printf(
		"testfile PR_Available failed: expected/returned = %d/%d bytes\n",
			0, len);
		rv = -1;
		goto cleanup;
	}
	if (PR_GetOpenFileInfo(fd_file, &file_info) < 0) {
		printf("testfile PR_GetFileInfo failed on file %s\n",pathname);
		goto cleanup;
	}
	if (LL_NE(file_info.creationTime , file_info1.creationTime)) {
		printf(
		"testfile PR_GetFileInfo returned incorrect status-change time: %s\n",
		pathname);
		printf("ft = %lld, ft1 = %lld\n",file_info.creationTime,
									file_info1.creationTime);
		rv = -1;
		goto cleanup;
	}
	len = PR_Write(fd_file, out_buf->data, CHUNK_SIZE);
	if (len < 0) {
		printf("testfile failed to write to file %s\n",pathname);
		rv = -1;
		goto cleanup;
	}
	if (PR_GetOpenFileInfo(fd_file, &file_info) < 0) {
		printf("testfile PR_GetFileInfo failed on file %s\n",pathname);
		goto cleanup;
	}
	if (file_info.size != CHUNK_SIZE) {
		printf(
		"testfile PR_GetFileInfo returned incorrect size (%d should be %d) for file %s\n",
		file_info.size, CHUNK_SIZE, pathname);
		rv = -1;
		goto cleanup;
	}
	if (LL_CMP(file_info.modifyTime, < , file_info1.modifyTime)) {
		printf(
		"testfile PR_GetFileInfo returned incorrect modify time: %s\n",
		pathname);
		printf("ft = %lld, ft1 = %lld\n",file_info.modifyTime,
									file_info1.modifyTime);
		rv = -1;
		goto cleanup;
	}

	len = PR_Available(fd_file);
	if (len < 0) {
		printf("testfile PR_Available failed on file %s\n",pathname);
		rv = -1;
		goto cleanup;
	} else if (len != 0) {
		printf(
		"testfile PR_Available failed: expected/returned = %d/%d bytes\n",
			0, len);
		rv = -1;
		goto cleanup;
	}
	
	PR_Seek(fd_file, 0, PR_SEEK_SET);
	len = PR_Available(fd_file);
	if (len < 0) {
		printf("testfile PR_Available failed on file %s\n",pathname);
		rv = -1;
		goto cleanup;
	} else if (len != CHUNK_SIZE) {
		printf(
		"testfile PR_Available failed: expected/returned = %d/%d bytes\n",
			CHUNK_SIZE, len);
		rv = -1;
		goto cleanup;
	}
    PR_Close(fd_file);

	strcpy(tmpname,pathname);
	strcat(tmpname,".RENAMED");
	if (PR_FAILURE == PR_Rename(pathname, tmpname)) {
		printf("testfile failed to rename file %s\n",pathname);
		rv = -1;
		goto cleanup;
	}

	fd_file = PR_Open(pathname, PR_RDWR | PR_CREATE_FILE, 0777);
	len = PR_Write(fd_file, out_buf->data, CHUNK_SIZE);
    PR_Close(fd_file);
	if (PR_SUCCESS == PR_Rename(pathname, tmpname)) {
		printf("testfile renamed to existing file %s\n",pathname);
	}

	if ((PR_Delete(tmpname)) < 0) {
		printf("testfile failed to unlink file %s\n",tmpname);
		rv = -1;
	}

cleanup:
	if ((PR_Delete(pathname)) < 0) {
		printf("testfile failed to unlink file %s\n",pathname);
		rv = -1;
	}
	return rv;
}


static PRInt32 PR_CALLBACK FileTest(void)
{
PRDir *fd_dir;
int i, offset, len, rv = 0;
PRThread *t;
PRThreadScope scope = PR_GLOBAL_THREAD;
File_Rdwr_Param *fparamp;

	/*
	 * Create Test dir
	 */
	if ((PR_MkDir(TEST_DIR, 0777)) < 0) {
		printf("testfile failed to create dir %s\n",TEST_DIR);
		return -1;
	}
	fd_dir = PR_OpenDir(TEST_DIR);
	if (fd_dir == NULL) {
		printf("testfile failed to open dir %s\n",TEST_DIR);
		rv =  -1;
		goto cleanup;	
	}

    PR_CloseDir(fd_dir);

	strcat(pathname, TEST_DIR);
	strcat(pathname, "/");
	strcat(pathname, FILE_NAME);

	in_buf = PR_NEW(buffer);
	if (in_buf == NULL) {
		printf(
		"testfile failed to alloc buffer struct\n");
		rv =  -1;
		goto cleanup;	
	}
	out_buf = PR_NEW(buffer);
	if (out_buf == NULL) {
		printf(
		"testfile failed to alloc buffer struct\n");
		rv =  -1;
		goto cleanup;	
	}

	/*
	 * Start a bunch of writer threads
	 */
	offset = 0;
	len = CHUNK_SIZE;
	PR_EnterMonitor(mon);
	for (i = 0; i < NUM_RDWR_THREADS; i++) {
		fparamp = PR_NEW(File_Rdwr_Param);
		if (fparamp == NULL) {
			printf(
			"testfile failed to alloc File_Rdwr_Param struct\n");
			rv =  -1;
			goto cleanup;	
		}
		fparamp->pathname = pathname;
		fparamp->buf = out_buf->data + offset;
		fparamp->offset = offset;
		fparamp->len = len;
		memset(fparamp->buf, i, len);

		t = create_new_thread(PR_USER_THREAD,
			      File_Write, (void *)fparamp, 
			      PR_PRIORITY_NORMAL,
			      scope,
			      PR_UNJOINABLE_THREAD,
			      0, i);
		offset += len;
	}
	thread_count = i;
	/* Wait for writer threads to exit */
	while (thread_count) {
		PR_Wait(mon, PR_INTERVAL_NO_TIMEOUT);
	}
	PR_ExitMonitor(mon);


	/*
	 * Start a bunch of reader threads
	 */
	offset = 0;
	len = CHUNK_SIZE;
	PR_EnterMonitor(mon);
	for (i = 0; i < NUM_RDWR_THREADS; i++) {
		fparamp = PR_NEW(File_Rdwr_Param);
		if (fparamp == NULL) {
			printf(
			"testfile failed to alloc File_Rdwr_Param struct\n");
			rv =  -1;
			goto cleanup;	
		}
		fparamp->pathname = pathname;
		fparamp->buf = in_buf->data + offset;
		fparamp->offset = offset;
		fparamp->len = len;

		t = create_new_thread(PR_USER_THREAD,
			      File_Read, (void *)fparamp, 
			      PR_PRIORITY_NORMAL,
			      scope,
			      PR_UNJOINABLE_THREAD,
			      0, i);
		offset += len;
		if ((offset + len) > BUF_DATA_SIZE)
			break;
	}
	thread_count = i;

	/* Wait for reader threads to exit */
	while (thread_count) {
		PR_Wait(mon, PR_INTERVAL_NO_TIMEOUT);
	}
	PR_ExitMonitor(mon);

	if (memcmp(in_buf->data, out_buf->data, offset) != 0) {
		printf("File Test failed: file data corrupted\n");
		rv =  -1;
		goto cleanup;	
	}

	if ((PR_Delete(pathname)) < 0) {
		printf("testfile failed to unlink file %s\n",pathname);
		rv =  -1;
		goto cleanup;	
	}

	/*
	 * Test PR_Available, PR_Seek, PR_GetFileInfo, PR_Rename, PR_Access
	 */
	if (Misc_File_Tests(pathname) < 0) {
		rv = -1;
	}

cleanup:
	if ((PR_RmDir(TEST_DIR)) < 0) {
		printf("testfile failed to rmdir %s\n", TEST_DIR);
		rv = -1;
	}
	return rv;
}

struct dirtest_arg {
	PRMonitor	*mon;
	PRInt32		done;
};

static PRInt32 RunDirTest(void)
{
int i;
PRThread *t;
PRMonitor *mon;
struct dirtest_arg thrarg;

	mon = PR_NewMonitor();
	if (!mon) {
		printf("RunDirTest: Error - failed to create monitor\n");
		dirtest_failed = 1;
		return -1;
	}
	thrarg.mon = mon;

	for (i = 0; i < NUM_DIRTEST_THREADS; i++) {

		thrarg.done= 0;
		t = create_new_thread(PR_USER_THREAD,
			      DirTest, &thrarg, 
			      PR_PRIORITY_NORMAL,
			      PR_LOCAL_THREAD,
			      PR_UNJOINABLE_THREAD,
			      0, i);
		if (!t) {
			printf("RunDirTest: Error - failed to create thread\n");
			dirtest_failed = 1;
			return -1;
		}
		PR_EnterMonitor(mon);
		while (!thrarg.done)
			PR_Wait(mon, PR_INTERVAL_NO_TIMEOUT);
		PR_ExitMonitor(mon);

	}
	PR_DestroyMonitor(mon);
	return 0;
}

static PRInt32 PR_CALLBACK DirTest(void *arg)
{
struct dirtest_arg *tinfo = (struct dirtest_arg *) arg;
PRFileDesc *fd_file;
PRDir *fd_dir;
int i;
int path_len;
PRDirEntry *dirEntry;
PRFileInfo info;
PRInt32 num_files = 0;
#if defined(XP_PC) && defined(WIN32)
HANDLE hfile;
#endif

#define  FILES_IN_DIR 20

	/*
	 * Create Test dir
	 */
	DPRINTF(("Creating test dir %s\n",TEST_DIR));
	if ((PR_MkDir(TEST_DIR, 0777)) < 0) {
		printf(
			"testfile failed to create dir %s [%d, %d]\n",
			TEST_DIR, PR_GetError(), PR_GetOSError());
		return -1;
	}
	fd_dir = PR_OpenDir(TEST_DIR);
	if (fd_dir == NULL) {
		printf(
			"testfile failed to open dirctory %s [%d, %d]\n",
			TEST_DIR, PR_GetError(), PR_GetOSError());
		return -1;
	}

	strcpy(pathname, TEST_DIR);
	strcat(pathname, "/");
	strcat(pathname, FILE_NAME);
	path_len = strlen(pathname);
	
	for (i = 0; i < FILES_IN_DIR; i++) {

		sprintf(pathname + path_len,"%d%s",i,"");

		DPRINTF(("Creating test file %s\n",pathname));

		fd_file = PR_Open(pathname, PR_RDWR | PR_CREATE_FILE, 0777);

		if (fd_file == NULL) {
			printf(
					"testfile failed to create/open file %s [%d, %d]\n",
					pathname, PR_GetError(), PR_GetOSError());
			return -1;
		}
        PR_Close(fd_file);
	}
#if defined(XP_UNIX) || (defined(XP_PC) && defined(WIN32)) || defined(XP_OS2) || defined(XP_BEOS)
	/*
	 * Create a hidden file - a platform-dependent operation
	 */
	strcpy(pathname, TEST_DIR);
	strcat(pathname, "/");
	strcat(pathname, HIDDEN_FILE_NAME);
#if defined(XP_UNIX) || defined(XP_BEOS)
	DPRINTF(("Creating hidden test file %s\n",pathname));
	fd_file = PR_Open(pathname, PR_RDWR | PR_CREATE_FILE, 0777);

	if (fd_file == NULL) {
		printf(
				"testfile failed to create/open hidden file %s [%d, %d]\n",
				pathname, PR_GetError(), PR_GetOSError());
		return -1;
	}

    PR_Close(fd_file);

#elif defined(WINCE)
	DPRINTF(("Creating hidden test file %s\n",pathname));
    MultiByteToWideChar(CP_ACP, 0, pathname, -1, wPathname, 256); 
	hfile = CreateFile(wPathname, GENERIC_READ,
						FILE_SHARE_READ|FILE_SHARE_WRITE,
						NULL,
						CREATE_NEW,
						FILE_ATTRIBUTE_HIDDEN,
						NULL);
	if (hfile == INVALID_HANDLE_VALUE) {
		printf("testfile failed to create/open hidden file %s [0, %d]\n",
				pathname, GetLastError());
		return -1;
	}
	CloseHandle(hfile);
						
#elif defined(XP_PC) && defined(WIN32)
	DPRINTF(("Creating hidden test file %s\n",pathname));
	hfile = CreateFile(pathname, GENERIC_READ,
						FILE_SHARE_READ|FILE_SHARE_WRITE,
						NULL,
						CREATE_NEW,
						FILE_ATTRIBUTE_HIDDEN,
						NULL);
	if (hfile == INVALID_HANDLE_VALUE) {
		printf("testfile failed to create/open hidden file %s [0, %d]\n",
				pathname, GetLastError());
		return -1;
	}
	CloseHandle(hfile);
						
#elif defined(OS2)
	DPRINTF(("Creating hidden test file %s\n",pathname));
	fd_file = PR_Open(pathname, PR_RDWR | PR_CREATE_FILE, (int)FILE_HIDDEN);

	if (fd_file == NULL) {
		printf("testfile failed to create/open hidden file %s [%d, %d]\n",
				pathname, PR_GetError(), PR_GetOSError());
		return -1;
	}
	PR_Close(fd_file);
#endif	/* XP_UNIX */

#endif	/* XP_UNIX || (XP_PC && WIN32) */


	if (PR_FAILURE == PR_CloseDir(fd_dir))
	{
		printf(
			"testfile failed to close dirctory %s [%d, %d]\n",
			TEST_DIR, PR_GetError(), PR_GetOSError());
		return -1;
	}
	fd_dir = PR_OpenDir(TEST_DIR);
	if (fd_dir == NULL) {
		printf(
			"testfile failed to reopen dirctory %s [%d, %d]\n",
			TEST_DIR, PR_GetError(), PR_GetOSError());
		return -1;
	}
  
	/*
	 * List all files, including hidden files
	 */
	DPRINTF(("Listing all files in directory %s\n",TEST_DIR));
#if defined(XP_UNIX) || (defined(XP_PC) && defined(WIN32)) || defined(XP_OS2) || defined(XP_BEOS)
	num_files = FILES_IN_DIR + 1;
#else
	num_files = FILES_IN_DIR;
#endif
	while ((dirEntry = PR_ReadDir(fd_dir, PR_SKIP_BOTH)) != NULL) {
		num_files--;
		strcpy(pathname, TEST_DIR);
		strcat(pathname, "/");
		strcat(pathname, dirEntry->name);
		DPRINTF(("\t%s\n",dirEntry->name));

		if ((PR_GetFileInfo(pathname, &info)) < 0) {
			printf(
				"testfile failed to GetFileInfo file %s [%d, %d]\n",
				pathname, PR_GetError(), PR_GetOSError());
			return -1;
		}
		
		if (info.type != PR_FILE_FILE) {
			printf(
				"testfile incorrect fileinfo for file %s [%d, %d]\n",
				pathname, PR_GetError(), PR_GetOSError());
			return -1;
		}
	}
	if (num_files != 0)
	{
		printf(
			"testfile failed to find all files in directory %s [%d, %d]\n",
			TEST_DIR, PR_GetError(), PR_GetOSError());
		return -1;
	}

    PR_CloseDir(fd_dir);

#if defined(XP_UNIX) || (defined(XP_PC) && defined(WIN32)) || defined(XP_OS2) || defined(XP_BEOS)

	/*
	 * List all files, except hidden files
	 */

	fd_dir = PR_OpenDir(TEST_DIR);
	if (fd_dir == NULL) {
		printf(
			"testfile failed to reopen dirctory %s [%d, %d]\n",
			TEST_DIR, PR_GetError(), PR_GetOSError());
		return -1;
	}
  
	DPRINTF(("Listing non-hidden files in directory %s\n",TEST_DIR));
	while ((dirEntry = PR_ReadDir(fd_dir, PR_SKIP_HIDDEN)) != NULL) {
		DPRINTF(("\t%s\n",dirEntry->name));
		if (!strcmp(HIDDEN_FILE_NAME, dirEntry->name)) {
			printf("testfile found hidden file %s\n", pathname);
			return -1;
		}

	}
	/*
	 * Delete hidden file
	 */
	strcpy(pathname, TEST_DIR);
	strcat(pathname, "/");
	strcat(pathname, HIDDEN_FILE_NAME);
	if (PR_FAILURE == PR_Delete(pathname)) {
		printf(
			"testfile failed to delete hidden file %s [%d, %d]\n",
			pathname, PR_GetError(), PR_GetOSError());
		return -1;
	}

    PR_CloseDir(fd_dir);
#endif	/* XP_UNIX || (XP_PC && WIN32) */

	strcpy(renamename, TEST_DIR);
	strcat(renamename, ".RENAMED");
	if (PR_FAILURE == PR_Rename(TEST_DIR, renamename)) {
		printf(
			"testfile failed to rename directory %s [%d, %d]\n",
			TEST_DIR, PR_GetError(), PR_GetOSError());
		return -1;
	}
    
	if (PR_FAILURE == PR_MkDir(TEST_DIR, 0777)) {
		printf(
			"testfile failed to recreate dir %s [%d, %d]\n",
			TEST_DIR, PR_GetError(), PR_GetOSError());
		return -1;
	}
	if (PR_SUCCESS == PR_Rename(renamename, TEST_DIR)) {
		printf(
			"testfile renamed directory to existing name %s\n",
			renamename);
		return -1;
	}

	if (PR_FAILURE == PR_RmDir(TEST_DIR)) {
		printf(
			"testfile failed to rmdir %s [%d, %d]\n",
			TEST_DIR, PR_GetError(), PR_GetOSError());
		return -1;
	}

	if (PR_FAILURE == PR_Rename(renamename, TEST_DIR)) {
		printf(
			"testfile failed to rename directory %s [%d, %d]\n",
			renamename, PR_GetError(), PR_GetOSError());
		return -1;
	}
	fd_dir = PR_OpenDir(TEST_DIR);
	if (fd_dir == NULL) {
		printf(
			"testfile failed to reopen directory %s [%d, %d]\n",
			TEST_DIR, PR_GetError(), PR_GetOSError());
		return -1;
	}

	strcpy(pathname, TEST_DIR);
	strcat(pathname, "/");
	strcat(pathname, FILE_NAME);
	path_len = strlen(pathname);
	
	for (i = 0; i < FILES_IN_DIR; i++) {

		sprintf(pathname + path_len,"%d%s",i,"");

		if (PR_FAILURE == PR_Delete(pathname)) {
			printf(
				"testfile failed to delete file %s [%d, %d]\n",
				pathname, PR_GetError(), PR_GetOSError());
			return -1;
		}
	}

    PR_CloseDir(fd_dir);

	if (PR_FAILURE == PR_RmDir(TEST_DIR)) {
		printf(
			"testfile failed to rmdir %s [%d, %d]\n",
			TEST_DIR, PR_GetError(), PR_GetOSError());
		return -1;
	}
	PR_EnterMonitor(tinfo->mon);
	tinfo->done = 1;
	PR_Notify(tinfo->mon);
	PR_ExitMonitor(tinfo->mon);

	return 0;
}
/************************************************************************/

/*
 * Test file and directory NSPR APIs
 */

int main(int argc, char **argv)
{
#ifdef WIN32
	PRUint32 len;
#endif
#if defined(XP_UNIX) || defined(XP_OS2)
        int opt;
        extern char *optarg;
	extern int optind;
#endif
#if defined(XP_UNIX) || defined(XP_OS2)
        while ((opt = getopt(argc, argv, "d")) != EOF) {
                switch(opt) {
                        case 'd':
                                _debug_on = 1;
                                break;
                        default:
                                break;
                }
        }
#endif
	PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    PR_STDIO_INIT();

	mon = PR_NewMonitor();
	if (mon == NULL) {
		printf("testfile: PR_NewMonitor failed\n");
		exit(2);
	}
#ifdef WIN32

#ifdef WINCE
    {
        WCHAR tdir[TMPDIR_LEN];
        len = GetTempPath(TMPDIR_LEN, tdir);
        if ((len > 0) && (len < (TMPDIR_LEN - 6))) {
            /*
             * enough space for prdir
             */
            WideCharToMultiByte(CP_ACP, 0, tdir, -1, testdir, TMPDIR_LEN, 0, 0); 
        }
    }
#else
	len = GetTempPath(TMPDIR_LEN, testdir);
#endif      /* WINCE */

	if ((len > 0) && (len < (TMPDIR_LEN - 6))) {
		/*
		 * enough space for prdir
		 */
		strcpy((testdir + len),"prdir");
		TEST_DIR = testdir;
		printf("TEST_DIR = %s\n",TEST_DIR);
	}
#endif      /* WIN32 */

	if (FileTest() < 0) {
		printf("File Test failed\n");
		exit(2);
	}
	printf("File Test passed\n");
	if ((RunDirTest() < 0) || dirtest_failed) {
		printf("Dir Test failed\n");
		exit(2);
	}
	printf("Dir Test passed\n");

	PR_DestroyMonitor(mon);
	PR_Cleanup();
    return 0;
}
