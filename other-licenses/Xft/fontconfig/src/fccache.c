/*
 * $XFree86: xc/lib/fontconfig/src/fccache.c,v 1.3 2002/02/19 07:50:43 keithp Exp $
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

#include "fcint.h"

static unsigned int
FcFileCacheHash (const FcChar8	*string)
{
    unsigned int    h = 0;
    FcChar8	    c;

    while ((c = *string++))
	h = (h << 1) ^ c;
    return h;
}

FcChar8 *
FcFileCacheFind (FcFileCache	*cache,
		 const FcChar8	*file,
		 int		id,
		 int		*count)
{
    unsigned int    hash;
    const FcChar8   *match;
    FcFileCacheEnt  *c, *name;
    int		    maxid;
    struct stat	    statb;
    
    match = file;
    
    hash = FcFileCacheHash (match);
    name = 0;
    maxid = -1;
    for (c = cache->ents[hash % FC_FILE_CACHE_HASH_SIZE]; c; c = c->next)
    {
	if (c->hash == hash && !strcmp (match, c->file))
	{
	    if (c->id > maxid)
		maxid = c->id;
	    if (c->id == id)
	    {
		if (stat ((char *) file, &statb) < 0)
		{
		    if (FcDebug () & FC_DBG_CACHE)
			printf (" file missing\n");
		    return 0;
		}
		if (statb.st_mtime != c->time)
		{
		    if (FcDebug () & FC_DBG_CACHE)
			printf (" timestamp mismatch (was %d is %d)\n",
				(int) c->time, (int) statb.st_mtime);
		    return 0;
		}
		if (!c->referenced)
		{
		    cache->referenced++;
		    c->referenced = FcTrue;
		}
		name = c;
	    }
	}
    }
    if (!name)
	return 0;
    *count = maxid + 1;
    return name->name;
}

/*
 * Cache file syntax is quite simple:
 *
 * "file_name" id time "font_name" \n
 */
 
static FcChar8 *
FcFileCacheReadString (FILE *f, FcChar8 *dest, int len)
{
    int		c;
    FcBool	escape;
    FcChar8	*d;
    int		size;
    int		i;

    while ((c = getc (f)) != EOF)
	if (c == '"')
	    break;
    if (c == EOF)
	return FcFalse;
    if (len == 0)
	return FcFalse;
    
    size = len;
    i = 0;
    d = dest;
    escape = FcFalse;
    while ((c = getc (f)) != EOF)
    {
	if (!escape)
	{
	    switch (c) {
	    case '"':
		c = '\0';
		break;
	    case '\\':
		escape = FcTrue;
		continue;
	    }
	}
	if (i == size)
	{
	    FcChar8 *new = malloc (size * 2);
	    if (!new)
		break;
	    memcpy (new, d, size);
	    size *= 2;
	    if (d != dest)
		free (d);
	    d = new;
	}
	d[i++] = c;
	if (c == '\0')
	    return d;
	escape = FcFalse;
    }
    if (d != dest)
	free (d);
    return 0;
}

static FcBool
FcFileCacheReadUlong (FILE *f, unsigned long *dest)
{
    unsigned long   t;
    int		    c;

    while ((c = getc (f)) != EOF)
    {
	if (!isspace (c))
	    break;
    }
    if (c == EOF)
	return FcFalse;
    t = 0;
    for (;;)
    {
	if (c == EOF || isspace (c))
	    break;
	if (!isdigit (c))
	    return FcFalse;
	t = t * 10 + (c - '0');
	c = getc (f);
    }
    *dest = t;
    return FcTrue;
}

static FcBool
FcFileCacheReadInt (FILE *f, int *dest)
{
    unsigned long   t;
    FcBool	    ret;

    ret = FcFileCacheReadUlong (f, &t);
    if (ret)
	*dest = (int) t;
    return ret;
}

static FcBool
FcFileCacheReadTime (FILE *f, time_t *dest)
{
    unsigned long   t;
    FcBool	    ret;

    ret = FcFileCacheReadUlong (f, &t);
    if (ret)
	*dest = (time_t) t;
    return ret;
}

static FcBool
FcFileCacheAdd (FcFileCache	*cache,
		 const FcChar8	*file,
		 int		id,
		 time_t		time,
		 const FcChar8	*name,
		 FcBool		replace)
{
    FcFileCacheEnt    *c;
    FcFileCacheEnt    **prev, *old;
    unsigned int    hash;

    if (FcDebug () & FC_DBG_CACHE)
    {
	printf ("%s face %s/%d as %s\n", replace ? "Replace" : "Add",
		file, id, name);
    }
    hash = FcFileCacheHash (file);
    for (prev = &cache->ents[hash % FC_FILE_CACHE_HASH_SIZE]; 
	 (old = *prev);
	 prev = &(*prev)->next)
    {
	if (old->hash == hash && old->id == id && !strcmp (old->file, file))
	    break;
    }
    if (*prev)
    {
	if (!replace)
	    return FcFalse;

	old = *prev;
	if (old->referenced)
	    cache->referenced--;
	*prev = old->next;
	free (old);
	cache->entries--;
    }
	
    c = malloc (sizeof (FcFileCacheEnt) +
		strlen ((char *) file) + 1 +
		strlen ((char *) name) + 1);
    if (!c)
	return FcFalse;
    c->next = *prev;
    *prev = c;
    c->hash = hash;
    c->file = (FcChar8 *) (c + 1);
    c->id = id;
    c->name = c->file + strlen ((char *) file) + 1;
    strcpy (c->file, file);
    c->time = time;
    c->referenced = replace;
    strcpy (c->name, name);
    cache->entries++;
    return FcTrue;
}

FcFileCache *
FcFileCacheCreate (void)
{
    FcFileCache	*cache;
    int		h;

    cache = malloc (sizeof (FcFileCache));
    if (!cache)
	return 0;
    for (h = 0; h < FC_FILE_CACHE_HASH_SIZE; h++)
	cache->ents[h] = 0;
    cache->entries = 0;
    cache->referenced = 0;
    cache->updated = FcFalse;
    return cache;
}

void
FcFileCacheDestroy (FcFileCache *cache)
{
    FcFileCacheEnt *c, *next;
    int		    h;

    for (h = 0; h < FC_FILE_CACHE_HASH_SIZE; h++)
    {
	for (c = cache->ents[h]; c; c = next)
	{
	    next = c->next;
	    free (c);
	}
    }
    free (cache);
}

void
FcFileCacheLoad (FcFileCache	*cache,
		 const FcChar8	*cache_file)
{
    FILE	    *f;
    FcChar8	    file_buf[8192], *file;
    int		    id;
    time_t	    time;
    FcChar8	    name_buf[8192], *name;

    f = fopen ((char *) cache_file, "r");
    if (!f)
	return;

    cache->updated = FcFalse;
    file = 0;
    name = 0;
    while ((file = FcFileCacheReadString (f, file_buf, sizeof (file_buf))) &&
	   FcFileCacheReadInt (f, &id) &&
	   FcFileCacheReadTime (f, &time) &&
	   (name = FcFileCacheReadString (f, name_buf, sizeof (name_buf))))
    {
	(void) FcFileCacheAdd (cache, file, id, time, name, FcFalse);
	if (file != file_buf)
	    free (file);
	if (name != name_buf)
	    free (name);
	file = 0;
	name = 0;
    }
    if (file && file != file_buf)
	free (file);
    if (name && name != name_buf)
	free (name);
    fclose (f);
}

FcBool
FcFileCacheUpdate (FcFileCache	    *cache,
		   const FcChar8    *file,
		   int		    id,
		   const FcChar8    *name)
{
    const FcChar8   *match;
    struct stat	    statb;
    FcBool	    ret;

    match = file;

    if (stat ((char *) file, &statb) < 0)
	return FcFalse;
    ret = FcFileCacheAdd (cache, match, id, 
			    statb.st_mtime, name, FcTrue);
    if (ret)
	cache->updated = FcTrue;
    return ret;
}

static FcBool
FcFileCacheWriteString (FILE *f, const FcChar8 *string)
{
    char    c;

    if (putc ('"', f) == EOF)
	return FcFalse;
    while ((c = *string++))
    {
	switch (c) {
	case '"':
	case '\\':
	    if (putc ('\\', f) == EOF)
		return FcFalse;
	    /* fall through */
	default:
	    if (putc (c, f) == EOF)
		return FcFalse;
	}
    }
    if (putc ('"', f) == EOF)
	return FcFalse;
    return FcTrue;
}

static FcBool
FcFileCacheWriteUlong (FILE *f, unsigned long t)
{
    int	    pow;
    unsigned long   temp, digit;

    temp = t;
    pow = 1;
    while (temp >= 10)
    {
	temp /= 10;
	pow *= 10;
    }
    temp = t;
    while (pow)
    {
	digit = temp / pow;
	if (putc ((char) digit + '0', f) == EOF)
	    return FcFalse;
	temp = temp - pow * digit;
	pow = pow / 10;
    }
    return FcTrue;
}

static FcBool
FcFileCacheWriteInt (FILE *f, int i)
{
    return FcFileCacheWriteUlong (f, (unsigned long) i);
}

static FcBool
FcFileCacheWriteTime (FILE *f, time_t t)
{
    return FcFileCacheWriteUlong (f, (unsigned long) t);
}

FcBool
FcFileCacheSave (FcFileCache	*cache,
		 const FcChar8	*cache_file)
{
    FcChar8	    *lck;
    FcChar8	    *tmp;
    FILE	    *f;
    int		    h;
    FcFileCacheEnt *c;

    if (!cache->updated && cache->referenced == cache->entries)
	return FcTrue;
    
    lck = malloc (strlen ((char *) cache_file)*2 + 4);
    if (!lck)
	goto bail0;
    tmp = lck + strlen ((char *) cache_file) + 2;
    strcpy ((char *) lck, (char *) cache_file);
    strcat ((char *) lck, "L");
    strcpy ((char *) tmp, (char *) cache_file);
    strcat ((char *) tmp, "T");
    if (link ((char *) lck, (char *) cache_file) < 0 && errno != ENOENT)
	goto bail1;
    if (access ((char *) tmp, F_OK) == 0)
	goto bail2;
    f = fopen ((char *) tmp, "w");
    if (!f)
	goto bail2;

    for (h = 0; h < FC_FILE_CACHE_HASH_SIZE; h++)
    {
	for (c = cache->ents[h]; c; c = c->next)
	{
	    if (!c->referenced)
		continue;
	    if (!FcFileCacheWriteString (f, c->file))
		goto bail4;
	    if (putc (' ', f) == EOF)
		goto bail4;
	    if (!FcFileCacheWriteInt (f, c->id))
		goto bail4;
	    if (putc (' ', f) == EOF)
		goto bail4;
	    if (!FcFileCacheWriteTime (f, c->time))
		goto bail4;
	    if (putc (' ', f) == EOF)
		goto bail4;
	    if (!FcFileCacheWriteString (f, c->name))
		goto bail4;
	    if (putc ('\n', f) == EOF)
		goto bail4;
	}
    }

    if (fclose (f) == EOF)
	goto bail3;
    
    if (rename ((char *) tmp, (char *) cache_file) < 0)
	goto bail3;
    
    unlink ((char *) lck);
    cache->updated = FcFalse;
    return FcTrue;

bail4:
    fclose (f);
bail3:
    unlink ((char *) tmp);
bail2:
    unlink ((char *) lck);
bail1:
    free (lck);
bail0:
    return FcFalse;
}

FcBool
FcFileCacheReadDir (FcFontSet *set, const FcChar8 *cache_file)
{
    FcPattern	    *font;
    FILE	    *f;
    FcChar8	    *path;
    FcChar8	    *base;
    FcChar8	    file_buf[8192], *file;
    int		    id;
    FcChar8	    name_buf[8192], *name;
    FcBool	    ret = FcFalse;

    if (FcDebug () & FC_DBG_CACHE)
    {
	printf ("FcFileCacheReadDir cache_file \"%s\"\n", cache_file);
    }
    
    f = fopen ((char *) cache_file, "r");
    if (!f)
    {
	if (FcDebug () & FC_DBG_CACHE)
	{
	    printf (" no cache file\n");
	}
	goto bail0;
    }

    base = (FcChar8 *) strrchr ((char *) cache_file, '/');
    if (!base)
	goto bail1;
    base++;
    path = malloc (base - cache_file + 8192 + 1);
    if (!path)
	goto bail1;
    memcpy (path, cache_file, base - cache_file);
    base = path + (base - cache_file);
    
    file = 0;
    name = 0;
    while ((file = FcFileCacheReadString (f, file_buf, sizeof (file_buf))) &&
	   FcFileCacheReadInt (f, &id) &&
	   (name = FcFileCacheReadString (f, name_buf, sizeof (name_buf))))
    {
	font = FcNameParse (name);
	if (font)
	{
	    strcpy (base, file);
	    if (FcDebug () & FC_DBG_CACHEV)
	    {
		printf (" dir cache file \"%s\"\n", file);
	    }
	    FcPatternAddString (font, FC_FILE, path);
	    if (!FcFontSetAdd (set, font))
		goto bail2;
	}
	if (file != file_buf)
	    free (file);
	if (name != name_buf)
	    free (name);
	file = name = 0;
    }
    if (FcDebug () & FC_DBG_CACHE)
    {
	printf (" cache loaded\n");
    }
    
    ret = FcTrue;
bail2:
    free (path);
    if (file && file != file_buf)
	free (file);
    if (name && name != name_buf)
	free (name);
bail1:
    fclose (f);
bail0:
    return ret;
}

FcBool
FcFileCacheWriteDir (FcFontSet *set, const FcChar8 *cache_file)
{
    FcPattern	    *font;
    FILE	    *f;
    FcChar8	    *name;
    const FcChar8   *file, *base;
    int		    n;
    int		    id;
    FcBool	    ret;

    if (FcDebug () & FC_DBG_CACHE)
	printf ("FcFileCacheWriteDir cache_file \"%s\"\n", cache_file);
    
    f = fopen ((char *) cache_file, "w");
    if (!f)
    {
	if (FcDebug () & FC_DBG_CACHE)
	    printf (" can't create \"%s\"\n", cache_file);
	goto bail0;
    }
    for (n = 0; n < set->nfont; n++)
    {
	font = set->fonts[n];
	if (FcPatternGetString (font, FC_FILE, 0, (FcChar8 **) &file) != FcResultMatch)
	    goto bail1;
	base = (FcChar8 *) strrchr ((char *) file, '/');
	if (base)
	    base = base + 1;
	else
	    base = file;
	if (FcPatternGetInteger (font, FC_INDEX, 0, &id) != FcResultMatch)
	    goto bail1;
	if (FcDebug () & FC_DBG_CACHEV)
	    printf (" write file \"%s\"\n", base);
	if (!FcFileCacheWriteString (f, base))
	    goto bail1;
	if (putc (' ', f) == EOF)
	    goto bail1;
	if (!FcFileCacheWriteInt (f, id))
	    goto bail1;
        if (putc (' ', f) == EOF)
	    goto bail1;
	name = FcNameUnparse (font);
	if (!name)
	    goto bail1;
	ret = FcFileCacheWriteString (f, name);
	free (name);
	if (!ret)
	    goto bail1;
	if (putc ('\n', f) == EOF)
	    goto bail1;
    }
    if (fclose (f) == EOF)
	goto bail0;
    
    if (FcDebug () & FC_DBG_CACHE)
	printf (" cache written\n");
    return FcTrue;
    
bail1:
    fclose (f);
bail0:
    unlink ((char *) cache_file);
    return FcFalse;
}
