/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "xp_file.h"

#ifndef _net_xp_file_h
#define _net_xp_file_h


/* The puropse of this file is simply to add a level of abstraction between netlib
 * and the XP_File* api. */

#if defined(NS_NET_FILE)

extern int NET_I_XP_Stat(const char * name, XP_StatStruct * outStat, XP_FileType type);
#define NET_XP_Stat(name, out, type) NET_I_XP_Stat(name, out, type)

extern XP_Dir NET_I_XP_OpenDir(const char * name, XP_FileType type);
#define NET_XP_OpenDir(name, type) NET_I_XP_OpenDir(name, type)

extern void NET_I_XP_CloseDir(XP_Dir dir);
#define NET_XP_CloseDir(dir) NET_I_XP_CloseDir(dir)

extern XP_DirEntryStruct *NET_I_XP_ReadDir(XP_Dir dir);
#define NET_XP_ReadDir(dir) NET_I_XP_ReadDir(dir)

extern int NET_I_XP_Fileno(XP_File fp);
#define NET_XP_Fileno(fp) NET_I_XP_Fileno(fp)

extern int NET_I_XP_FileSeek(XP_File file, long offset, int origin);
#define NET_XP_FileSeek(file,offset,whence) NET_I_XP_FileSeek ((file), (offset), (whence))

extern XP_File NET_I_XP_FileOpen(const char *name, XP_FileType type, const XP_FilePerm perm);
#define NET_XP_FileOpen(name, type, perm) NET_I_XP_FileOpen(name, type, perm)

extern int NET_I_XP_FileRead(char *outBuf, int outBufLen, XP_File fp);
#define NET_XP_FileRead(dest, count, file) NET_I_XP_FileRead(dest, count, file)

extern char *NET_I_XP_FileReadLine(char *outBuf, int outBufLen, XP_File fp);
#define NET_XP_FileReadLine(destBuf, bufSize, file) NET_I_XP_FileReadLine(destBuf, bufSize, file)

extern int NET_I_XP_FileWrite(char *buf, int bufLen, XP_File fp);
#define NET_XP_FileWrite(source, count, file) NET_I_XP_FileWrite(source, count, file)

extern int NET_I_XP_FileClose(XP_File file);
#define NET_XP_FileClose(fp) NET_I_XP_FileClose(fp)

extern int NET_I_XP_FileRemove(const char * name, XP_FileType type);
#define NET_XP_FileRemove(name, type) NET_I_XP_FileRemove(name, type) 

#else

#define NET_XP_Stat(name, outStat, type) XP_Stat(name, outStat, type)

#define NET_XP_OpenDir(name, type) XP_OpenDir(name, type)

#define NET_XP_CloseDir(dir) XP_CloseDir(dir)

#define NET_XP_ReadDir(dir) XP_ReadDir(dir)

#define NET_XP_Fileno(fp) XP_Fileno(fp)

#define NET_XP_FileSeek(file,offset,whence) XP_FileSeek ((file), (offset), (whence))

#define NET_XP_FileOpen(name, type, perm) XP_FileOpen(name, type, perm)

#define NET_XP_FileRead(dest,count,file) XP_FileRead(dest,count,file)

#define NET_XP_FileReadLine(destBuf, bufSize, file) XP_FileReadLine(destBuf, bufSize, file)

#define NET_XP_FileWrite(source, count, file) XP_FileWrite(source, count, file)

#define NET_XP_FileClose(fp) XP_FileClose(fp)

#define NET_XP_FileRemove(name, type) XP_FileRemove(name, type) 
#endif /* NS_NET_FILE */

#endif /* _net_xp_file_h */
