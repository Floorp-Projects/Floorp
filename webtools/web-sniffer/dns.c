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

#define server (unsigned char *) "host.domain.com"

#define QR	0		/* query */
#define OPCODE	0		/* standard query */
#define AA	0		/* authoritative answer */
#define TC	0		/* truncation */
#define RD	1		/* recursion desired */

#define RA	0		/* recursion available */
#define Z	0		/* reserved */
#define RCODE	0		/* response code */

static unsigned short ID = 0xbeef;

void
reportContentType(void *a, unsigned char *contentType)
{
}

void
reportHTML(void *a, Input *input)
{
}

void
reportHTMLAttributeName(void *a, HTML *html, Input *input)
{
}

void
reportHTMLAttributeValue(void *a, HTML *html, Input *input)
{
}

void
reportHTMLTag(void *a, HTML *html, Input *input)
{
}

void
reportHTMLText(void *a, Input *input)
{
}

void
reportHTTP(void *a, Input *input)
{
}

void
reportHTTPBody(void *a, Input *input)
{
}

void
reportHTTPCharSet(void *a, unsigned char *charset)
{
}

void
reportHTTPHeaderName(void *a, Input *input)
{
}

void
reportHTTPHeaderValue(void *a, Input *input, unsigned char *url)
{
}

void
reportStatus(void *a, char *message, char *file, int line)
{
}

void
reportTime(int task, struct timeval *theTime)
{
}

static unsigned char *
putDomainName(unsigned char *p, char *name)
{
	char	*begin;
	char	*q;

	q = name;
	while (*q)
	{
		begin = q;
		while (*q && (*q != '.'))
		{
			q++;
		}
		if (q - begin > 0)
		{
			*p++ = q - begin;
			q = begin;
			while (*q && (*q != '.'))
			{
				*p++ = *q++;
			}
			if (*q == '.')
			{
				q++;
			}
		}
	}
	*p++ = 0;

	return p;
}

int
main(int argc, char *argv[])
{
	unsigned char	buf[1024];
	int		bytesTransferred;
	int		c;
	int		fd;
	int		i;
	int		len;
	unsigned char	*p;

	if (!netInit())
	{
		return 1;
	}
	if (!threadInit())
	{
		return 1;
	}

	fd = netConnect(NULL, server, 53);
	if (fd < 0)
	{
		fprintf(stderr, "netConnect failed\n");
		return 1;
	}

	p = buf + 2;
	p[0] = (ID >> 8);
	p[1] = (ID & 0xff);
	p[2] = (QR | OPCODE | AA | TC | RD);
	p[3] = (RA | Z | RCODE);
	p[4] = 0; /* QDCOUNT */
	p[5] = 1; /* QDCOUNT */
	p[6] = 0; /* ANCOUNT */
	p[7] = 0; /* ANCOUNT */
	p[8] = 0; /* NSCOUNT */
	p[9] = 0; /* NSCOUNT */
	p[10] = 0; /* ARCOUNT */
	p[11] = 0; /* ARCOUNT */

	p = putDomainName(&p[12], "w3.org");

	/* A = 1 = host address */

	p[0] = 0;
	p[1] = 15; /* MX = mail exchange */
	p[2] = 0;
	p[3] = 1; /* IN = Internet */

	len = (p + 4) - buf;

	buf[0] = 0;
	buf[1] = len - 2;

	bytesTransferred = send(fd, buf, len, 0);
	if (bytesTransferred != len)
	{
		fprintf(stderr, "wrong number of bytes written\n");
		return 1;
	}

	bytesTransferred = recv(fd, buf, sizeof(buf), 0);

	for (i = 0; i < bytesTransferred; i++)
	{
		c = buf[i];
		if ((0x20 <= c) && (c <= 0x7e))
		{
			printf("%02d: 0x%02x = '%c'\n", i, c, c);
		}
		else
		{
			printf("%02d: 0x%02x\n", i, c);
		}
	}

	printf("%d bytes read\n", bytesTransferred);
	printf("ID 0x%04x\n", (buf[2] << 8) | buf[3]);
	printf("QR %d\n", buf[4] >> 7);
	printf("RCODE %d\n", buf[5] & 0xf);
	printf("QDCOUNT %d\n", (buf[ 6] << 8) | buf[ 7]);
	printf("ANCOUNT %d\n", (buf[ 8] << 8) | buf[ 9]);
	printf("NSCOUNT %d\n", (buf[10] << 8) | buf[11]);
	printf("ARCOUNT %d\n", (buf[12] << 8) | buf[13]);

	return 0;
}
