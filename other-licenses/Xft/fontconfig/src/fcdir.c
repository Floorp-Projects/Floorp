/*
 * $XFree86: xc/lib/fontconfig/src/fcdir.c,v 1.2 2002/02/15 06:01:28 keithp Exp $
 *
 * Copyright © 2000 Keith Packard, member of The XFree86 Project, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include "fcint.h"

#define FC_INVALID_FONT_FILE "."

FcBool
FcFileScan (FcFontSet	    *set,
	    FcFileCache	    *cache,
	    FcBlanks	    *blanks,
	    const FcChar8   *file,
	    FcBool	    force)
{
    int		    id;
    FcChar8	    *name;
    FcPattern	    *font;
    FcBool	    ret = FcTrue;
    int		    count;
    
    id = 0;
    do
    {
	if (!force && cache)
	    name = FcFileCacheFind (cache, file, id, &count);
	else
	    name = 0;
	if (name)
	{
	    /* "." means the file doesn't contain a font */
	    if (strcmp (name, FC_INVALID_FONT_FILE) != 0)
	    {
		font = FcNameParse (name);
		if (font)
		    FcPatternAddString (font, FC_FILE, file);
	    }
	    else
		font = 0;
	}
	else
	{
	    if (FcDebug () & FC_DBG_SCAN)
	    {
		printf ("\tScanning file %s...", file);
		fflush (stdout);
	    }
	    font = FcFreeTypeQuery (file, id, blanks, &count);
	    if (FcDebug () & FC_DBG_SCAN)
		printf ("done\n");
	    if (!force && cache)
	    {
		if (font)
		{
		    FcChar8	*unparse;

		    unparse = FcNameUnparse (font);
		    if (unparse)
		    {
			(void) FcFileCacheUpdate (cache, file, id, unparse);
			free (unparse);
		    }
		}
		else
		{
		    /* negative cache files not containing fonts */
		    FcFileCacheUpdate (cache, file, id, (FcChar8 *) FC_INVALID_FONT_FILE);
		}
	    }
	}
	if (font)
	{
	    if (!FcFontSetAdd (set, font))
	    {
		FcPatternDestroy (font);
		font = 0;
		ret = FcFalse;
	    }
	}
	id++;
    } while (font && ret && id < count);
    return ret;
}

FcBool
FcDirScan (FcFontSet	    *set,
	   FcFileCache	    *cache,
	   FcBlanks	    *blanks,
	   const FcChar8    *dir,
	   FcBool	    force)
{
    DIR		    *d;
    struct dirent   *e;
    FcChar8	    *file;
    FcChar8	    *base;
    FcBool	    ret = FcTrue;

    file = (FcChar8 *) malloc (strlen ((char *) dir) + 1 + 256 + 1);
    if (!file)
	return FcFalse;

    strcpy ((char *) file, (char *) dir);
    strcat ((char *) file, "/");
    base = file + strlen ((char *) file);
    if (!force)
    {
	strcpy ((char *) base, FC_DIR_CACHE_FILE);
	
	if (FcFileCacheReadDir (set, file))
	{
	    free (file);
	    return FcTrue;
	}
    }
    
    d = opendir ((char *) dir);
    if (!d)
    {
	free (file);
	return FcFalse;
    }
    while (ret && (e = readdir (d)))
    {
	if (e->d_name[0] != '.')
	{
	    strcpy ((char *) base, (char *) e->d_name);
	    FcFileScan (set, cache, blanks, file, force);
	}
    }
    free (file);
    closedir (d);
    return ret;
}

FcBool
FcDirSave (FcFontSet *set, const FcChar8 *dir)
{
    FcChar8	    *file;
    FcChar8	    *base;
    FcBool	    ret;
    
    file = (FcChar8 *) malloc (strlen ((char *) dir) + 1 + 256 + 1);
    if (!file)
	return FcFalse;

    strcpy ((char *) file, (char *) dir);
    strcat ((char *) file, "/");
    base = file + strlen ((char *) file);
    strcpy ((char *) base, FC_DIR_CACHE_FILE);
    ret = FcFileCacheWriteDir (set, file);
    free (file);
    return ret;
}

