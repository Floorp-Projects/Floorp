/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is SniffURI.
 *
 * The Initial Developer of the Original Code is
 * Erik van der Poel <erik@vanderpoel.org>.
 * Portions created by the Initial Developer are Copyright (C) 1998-2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * ***** END LICENSE BLOCK ***** */

#include "all.h"

#define bufBlock 2048

struct Buf
{
	unsigned char	*buf;
	unsigned long	alloc;
	unsigned long	curr;
	unsigned long	endData;
	unsigned long	markBegin;
	unsigned long	markEnd;
	int		fd;
	int		error;
};

static int bufGrow(Buf *buf, int size);

Buf *
bufAlloc(int fd)
{
	Buf	*buf;

	buf = calloc(1, sizeof(Buf));
	if (!buf)
	{
		return NULL;
	}
	buf->alloc = 1;
	buf->buf = malloc(buf->alloc);
	if (!buf->buf)
	{
		free(buf);
		return NULL;
	}
	buf->fd = fd;

	return buf;
}

unsigned char *
bufCopy(Buf *buf)
{
	return copySizedString(&buf->buf[buf->markBegin],
		buf->markEnd - buf->markBegin);
}

unsigned char *
bufCopyLower(Buf *buf)
{
	return lowerCase(copySizedString(&buf->buf[buf->markBegin],
		buf->markEnd - buf->markBegin));
}

unsigned char *
bufCopyMemory(Buf *buf, unsigned long *len)
{
	unsigned char	*ret;

	*len = buf->markEnd - buf->markBegin;
	ret = malloc(*len);
	if (!ret)
	{
		fprintf(stderr, "cannot calloc block\n");
		exit(0);
	}
	memcpy(ret, &buf->buf[buf->markBegin], *len);

	return ret;
}

unsigned long
bufCurrent(Buf *buf)
{
	return buf->curr;
}

int
bufError(Buf *buf)
{
	return buf->error;
}

void
bufFree(Buf *buf)
{
	free(buf->buf);
	free(buf);
}

unsigned short
bufGetByte(Buf *buf)
{
	int	n;

	if (buf->curr < buf->endData)
	{
		return buf->buf[buf->curr++];
	}
	if (!bufGrow(buf, bufBlock))
	{
		buf->curr++;
		return 256;
	}
	n = recv(buf->fd, &buf->buf[buf->endData], bufBlock, 0);
	if (n < 0)
	{
		buf->curr++;
		return 256;
	}
	else if (n == 0)
	{
		buf->curr++;
		return 256;
	}
	buf->endData += n;

	return buf->buf[buf->curr++];
}

static int
bufGrow(Buf *buf, int size)
{
	int	need;

	need = buf->curr + size;
	if (buf->alloc >= need)
	{
		return 1;
	}
	while (buf->alloc < need)
	{
		buf->alloc *= 2;
	}
	buf->buf = realloc(buf->buf, buf->alloc);
	if (!buf->buf)
	{
		buf->error = 1;
		return 0;
	}

	return 1;
}

void
bufMark(Buf *buf, int offset)
{
	buf->markBegin = buf->markEnd;
	buf->markEnd = buf->curr + offset;
}

void
bufPutChar(Buf *buf, unsigned char c)
{
	if (buf->error)
	{
		return;
	}

	if (buf->curr >= buf->alloc)
	{
		if (!bufGrow(buf, 1))
		{
			return;
		}
	}

	buf->buf[buf->curr++] = c;
	buf->endData = buf->curr;
}

void
bufPutString(Buf *buf, unsigned char *str)
{
	int	len;

	if (buf->error)
	{
		return;
	}

	len = strlen((char *) str);
	if (buf->curr + len >= buf->alloc)
	{
		if (!bufGrow(buf, len))
		{
			return;
		}
	}

	while (*str)
	{
		buf->buf[buf->curr++] = *str++;
	}
	buf->endData = buf->curr;
}

void
bufSend(Buf *buf)
{
	if (buf->error)
	{
		fprintf(stderr, "bufSend error\n");
		return;
	}

	send(buf->fd, buf->buf, buf->curr, 0);
}

void
bufSet(Buf *buf, unsigned long offset)
{
	buf->curr = offset;
}

void
bufSetFD(Buf *buf, int fd)
{
	buf->fd = fd;
}

unsigned short
bufTrimTrailingWhiteSpace(Buf *buf)
{
	unsigned char	c;

	buf->curr -= 2;
	do
	{
		c = buf->buf[buf->curr--];
	} while
	(
		(c == ' ') ||
		(c == '\t') ||
		(c == '\r') ||
		(c == '\n')
	);
	buf->curr += 2;

	return buf->buf[buf->curr++];
}

void
bufUnGetByte(Buf *buf)
{
	if (buf->curr > 0)
	{
		buf->curr--;
	}
}

void
bufWrite(Buf *buf)
{
	write(buf->fd, &buf->buf[buf->curr], buf->endData - buf->curr);
}
