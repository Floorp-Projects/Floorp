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
 * The Original Code is Web Sniffer.
 * 
 * The Initial Developer of the Original Code is Erik van der Poel.
 * Portions created by Erik van der Poel are
 * Copyright (C) 1998,1999,2000 Erik van der Poel.
 * All Rights Reserved.
 * 
 * Contributor(s): Bruce Robson
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "hash.h"
#include "html.h"
#include "http.h"
#include "io.h"
#include "main.h"
#include "url.h"
#include "utils.h"

#define IS_WHITE_SPACE(c) \
	( \
		((c) == ' ' ) || \
		((c) == '\t') || \
		((c) == '\r') || \
		((c) == '\n') \
	)

typedef struct HTMLState
{
	unsigned short	mask;
	unsigned short	saved;
	unsigned short	unGotten;
	HTML		*html;
} HTMLState;

static HashTable *tagTable = NULL;

static HTMLHandler tagHandler = NULL;

static char *urlAttributes[] =
{
	"a", "href",
	"applet", "codebase",
	"area", "href",
	"base", "href",
	"blockquote", "cite",
	"body", "background",
	"del", "cite",
	"form", "action",
	"frame", "longdesc",
	"frame", "src",
	"head", "profile",
	"iframe", "longdesc",
	"iframe", "src",
	"img", "longdesc",
	"img", "src",
	"img", "usemap",
	"input", "src",
	"input", "usemap",
	"ins", "cite",
	"link", "href",
	"object", "archive",
	"object", "classid",
	"object", "codebase",
	"object", "data",
	"object", "usemap",
	"q", "cite",
	"script", "for",
	"script", "src",
	NULL
};

static int htmlInitialized = 0;

static HashTable *knownTagTable = NULL;

static char *knownTags[] =
{
	"!doctype",
	"a",
	"address",
	"applet",
	"area",
	"b",
	"base",
	"basefont",
	"big",
	"blink",
	"blockquote",
	"body",
	"br",
	"caption",
	"cell",
	"center",
	"certificate",
	"charles",
	"cite",
	"code",
	"colormap",
	"dd",
	"dir",
	"div",
	"dl",
	"dt",
	"em",
	"embed",
	"font",
	"form",
	"frame",
	"frameset",
	"h1",
	"h2",
	"h3",
	"h4",
	"h5",
	"h6",
	"head",
	"hr",
	"html",
	"hype",
	"i",
	"ilayer",
	"image",
	"img",
	"inlineinput",
	"input",
	"isindex",
	"jean",
	"kbd",
	"keygen",
	"layer",
	"li",
	"link",
	"listing",
	"map",
	"media",
	"menu",
	"meta",
	"mquote",
	"multicol",
	"nobr",
	"noembed",
	"noframes",
	"nolayer",
	"noscript",
	"nscp_close",
	"nscp_open",
	"nscp_reblock",
	"nsdt",
	"object",
	"ol",
	"option",
	"p",
	"param",
	"plaintext",
	"pre",
	"s",
	"samp",
	"script",
	"select",
	"server",
	"small",
	"spacer",
	"span",
	"spell",
	"strike",
	"strong",
	"style",
	"sub",
	"subdoc",
	"sup",
	"table",
	"td",
	"textarea",
	"th",
	"title",
	"tr",
	"tt",
	"u",
	"ul",
	"var",
	"wbr",
	"xmp",
	NULL
};

static void
diag(int line, HTMLState *state, unsigned short c)
{
	fprintf(stderr, "%s(%d): 0x%02x(%c) tag %s attr %s\n", __FILE__, line,
		c, c, state->html->tag ? (char *) state->html->tag : "NULL",
		state->html->currentAttribute ?
			(char *) state->html->currentAttribute->name : "NULL");
	fprintf(stderr, "(%s)\n", state->html->url);
}

static void
htmlInit(void)
{
	char	**p;

	knownTagTable = hashAlloc(NULL);
	p = knownTags;
	while (*p)
	{
		hashAdd(knownTagTable, copyString((unsigned char *) *p), NULL);
		p++;
	}
	htmlInitialized = 1;
}

static void
htmlCheckForBaseURL(HTML* html)
{
	if
	(
		(!strcmp((char *) html->tag, "base")) &&
		(!strcmp((char *) html->currentAttribute->name, "href"))
	)
	{
		FREE(html->base);
		html->base = copyString(html->currentAttribute->value);
	}
}

static void
htmlCheckForURLAttribute(HTML *html)
{
	char	**p;

	html->currentAttributeIsURL = 0;
	p = urlAttributes;
	while (*p)
	{
		if
		(
			(!strcmp((char *) html->tag, p[0])) &&
			(!strcmp((char *) html->currentAttribute->name, p[1]))
		)
		{
			html->currentAttributeIsURL = 1;
			break;
		}
		p += 2;
	}
}

static void
htmlCheckAttribute(HTML *html)
{
	htmlCheckForBaseURL(html);
	htmlCheckForURLAttribute(html);
}

void
htmlRegister(char *tag, char *attributeName, HTMLHandler handler)
{
	HashEntry	*attrEntry;
	HashEntry	*tagEntry;

	if (!tagTable)
	{
		tagTable = hashAlloc(NULL);
	}

	tagEntry = hashLookup(tagTable, (unsigned char *) tag);
	if (!tagEntry)
	{
		tagEntry = hashAdd(tagTable, (unsigned char *) tag,
			hashAlloc(NULL));
	}
	attrEntry = hashLookup(tagEntry->value,
		(unsigned char *) attributeName);
	if (attrEntry)
	{
		attrEntry->value = (void *) handler;
	}
	else
	{
		hashAdd(tagEntry->value, (unsigned char *) attributeName,
			(void *) handler);
	}
}

void
htmlRegisterURLHandler(HTMLHandler handler)
{
	char	**p;

	p = urlAttributes;
	while (*p)
	{
		htmlRegister(p[0], p[1], handler);
		p += 2;
	}
}

static void
callHandler(void *a, HTML *html)
{
	HashEntry	*attrEntry;
	HashEntry	*tagEntry;

	if (!tagTable)
	{
		return;
	}
	tagEntry = hashLookup(tagTable, html->tag);
	if (tagEntry)
	{
		attrEntry = hashLookup(tagEntry->value,
			html->currentAttribute->name);
		if (attrEntry)
		{
			(*((HTMLHandler) attrEntry->value))(a, html);
		}
	}
}

void
htmlRegisterTagHandler(HTMLHandler handler)
{
	tagHandler = handler;
}

static unsigned short
htmlGetByte(Input *input, HTMLState *state)
{
	unsigned short	c;
	unsigned short	ret;
	unsigned short	tmp;

	if (state->unGotten != 256)
	{
		tmp = state->unGotten;
		state->unGotten = 256;
		return tmp;
	}

	c = getByte(input);
	if (c == 256)
	{
		ret = c;
	}
	else if (c == 0x1b)
	{
		c = getByte(input);
		if (c == 256)
		{
			ret = c;
		}
		else if (c == '$')
		{
			c = getByte(input);
			if (c == 256)
			{
				ret = c;
			}
			else if (c == '(')
			{
				/* throw away 4th byte in ESC sequence */
				getByte(input);
				state->mask = 0x80;
				c = getByte(input);
				if (c == 256)
				{
					ret = c;
				}
				else
				{
					ret = c | state->mask;
				}
			}
			else
			{
				state->mask = 0x80;
				c = getByte(input);
				if (c == 256)
				{
					ret = c;
				}
				else
				{
					ret = c | state->mask;
				}
			}
		}
		else if (c == '(')
		{
			state->mask = 0;
			/* throw away 3rd byte in ESC sequence */
			getByte(input);
			ret = getByte(input);
		}
		else
		{
			unGetByte(input);
			ret = 0x1b;
		}
	}
	else
	{
		ret = c | state->mask;
	}

	state->saved = ret;

	return ret;
}

static void
htmlUnGetByte(HTMLState *state)
{
	state->unGotten = state->saved;
}

static unsigned short
eatWhiteSpace(Input *input, HTMLState *state, unsigned short c)
{
	while
	(
		(c == ' ') ||
		(c == '\t') ||
		(c == '\r') ||
		(c == '\n')
	)
	{
		c = htmlGetByte(input, state);
	}

	return c;
}

static void
htmlFreeAttributes(HTMLState *state)
{
	HTMLAttribute	*attr;
	HTMLAttribute	*tmp;

	attr = state->html->attributes;
	state->html->attributes = NULL;
	while (attr)
	{
		free(attr->name);
		free(attr->value);
		tmp = attr;
		attr = attr->next;
		free(tmp);
	}
}

static unsigned short
readAttribute(void *a, Input *input, HTMLState *state, unsigned short c)
{
	HTMLAttribute	*attr;
	unsigned short	quote;

	mark(input, -1);
	reportHTML(a, input);
	while
	(
		(c != 256) &&
		(c != '>') &&
		(c != '=') &&
		(c != ' ') &&
		(c != '\t') &&
		(c != '\r') &&
		(c != '\n')
	)
	{
		c = htmlGetByte(input, state);
	}
	mark(input, -1);
	attr = calloc(sizeof(HTMLAttribute), 1);
	if (!attr)
	{
		fprintf(stderr, "cannot calloc HTMLAttribute\n");
		exit(0);
	}
	if (state->html->currentAttribute)
	{
		state->html->currentAttribute->next = attr;
	}
	else
	{
		if (state->html->attributes)
		{
			htmlFreeAttributes(state);
		}
		state->html->attributes = attr;
	}
	state->html->currentAttribute = attr;
	attr->name = copyLower(input);
	reportHTMLAttributeName(a, state->html, input);
	if ((c == 256) || (c == '>'))
	{
		return c;
	}
	if (c != '=')
	{
		c = eatWhiteSpace(input, state, c);
	}
	if ((c == 256) || (c == '>'))
	{
		return c;
	}
	if (c == '=')
	{
		c = eatWhiteSpace(input, state, htmlGetByte(input, state));
		if ((c == '"') || (c == '\''))
		{
			quote = c;
			mark(input, 0);
			reportHTML(a, input);
			do
			{
				c = htmlGetByte(input, state);
			} while ((c != 256) && (c != quote));
			if (c == 256)
			{
				diag(__LINE__, state, c);
			}
			mark(input, -1);
			attr->value = copy(input);
			htmlCheckAttribute(state->html);
			reportHTMLAttributeValue(a, state->html, input);
			c = htmlGetByte(input, state);
		}
		else
		{
			mark(input, -1);
			reportHTML(a, input);
			while
			(
				(c != 256) &&
				(c != '>') &&
				(c != ' ') &&
				(c != '\t') &&
				(c != '\r') &&
				(c != '\n')
			)
			{
				if ((c == '"') || (c == '\''))
				{
					diag(__LINE__, state, c);
				}
				c = htmlGetByte(input, state);
			}
			mark(input, -1);
			attr->value = copy(input);
			htmlCheckAttribute(state->html);
			reportHTMLAttributeValue(a, state->html, input);
		}
		callHandler(a, state->html);
		if (c == '>')
		{
			return c;
		}
	}
	return eatWhiteSpace(input, state, c);
}

static int
caseCompare(char *str, Input *input, HTMLState *state, unsigned short *ret)
{
	unsigned short	c;
	int		i;

	for (i = 0; str[i]; i++)
	{
		c = htmlGetByte(input, state);
		if (tolower(c) != tolower(str[i]))
		{
			*ret = c;
			return 0;
		}
	}
	c = htmlGetByte(input, state);
	*ret = c;
	return 1;
}

static unsigned short
readTag(void *a, Input *input, HTMLState *state)
{
	unsigned short	c;

	mark(input, -1);
	reportHTML(a, input);

	c = htmlGetByte(input, state);
	if (c == '!')
	{
		c = htmlGetByte(input, state);
		if (c == '-')
		{
			c = htmlGetByte(input, state);
			if (c == '-')
			{
				const unsigned char *beginningOfComment =
					current(input);
				while (1)
				{
					c = htmlGetByte(input, state);
					if (c == '-')
					{
						c = htmlGetByte(input, state);
						if (c == '-')
						{
							c = htmlGetByte(input,
							state);
							if (c == '>')
							{
							    return htmlGetByte(input, state);
							}
							else if (c == '-')
							{
							  do
							  {
							    c = htmlGetByte(input, state);
							  } while (c == '-');
							  if (c == '>')
							  {
							    return htmlGetByte(input, state);
							  }
							}
						}
					}
					if (c == 256)
					{
					    set(input, beginningOfComment);
					    while (1)
					    {
						c = htmlGetByte(input, state);
						if (c == '>')
						{
						    return htmlGetByte(input, state);
						}
						else if (c == 256)
						{
						    fprintf(stderr,
							"bad comment\n");
						    mark(input, -1);
						    FREE(state->html->tag);
						    state->html->tag =
							copyString((unsigned
								char *) "!--");
						    state->html->tagIsKnown = 1;
						    reportHTMLTag(a,
							state->html, input);
						    return c;
						}
					    }
					}
				}
			}
			else
			{
				htmlUnGetByte(state);
			}
		}
		else
		{
			htmlUnGetByte(state);
		}
	}
	else
	{
		htmlUnGetByte(state);
	}

	do
	{
		c = htmlGetByte(input, state);
	}
	while
	(
		(c != 256) &&
		(c != '>') &&
		(c != ' ') &&
		(c != '\t') &&
		(c != '\r') &&
		(c != '\n')
	);
	mark(input, -1);
	FREE(state->html->tag);
	state->html->tag = copyLower(input);
	if (hashLookup(knownTagTable, (*state->html->tag == '/') ?
		state->html->tag + 1 : state->html->tag))
	{
		state->html->tagIsKnown = 1;
	}
	else
	{
		state->html->tagIsKnown = 0;
	}
	reportHTMLTag(a, state->html, input);
	if (c == 256)
	{
		return c;
	}
	else if (c == '>')
	{
		return htmlGetByte(input, state);
	}
	c = eatWhiteSpace(input, state, c);
	if (c == 256)
	{
		return c;
	}
	else if (c == '>')
	{
		return htmlGetByte(input, state);
	}
	do
	{
		c = readAttribute(a, input, state, c);
	} while ((c != 256) && (c != '>'));
	state->html->currentAttribute = NULL;
	if (tagHandler)
	{
		(*tagHandler)(a, state->html);
	}
	if (c == '>')
	{
		return htmlGetByte(input, state);
	}

	return c;
}

static unsigned short
readText(void *a, Input *input, HTMLState *state)
{
	unsigned short	c;

	mark(input, -1);
	reportHTML(a, input);
	do
	{
		c = htmlGetByte(input, state);
	} while ((c != 256) && (c != '<'));
	mark(input, -1);
	reportHTMLText(a, input);

	return c;
}

static unsigned short
dealWithScript(Input *input, HTMLState *state, unsigned short c)
{
	if (state->html->tag &&
		(!strcasecmp((char *) state->html->tag, "script")))
	{
		while (1)
		{
			if (c == 256)
			{
				break;
			}
			if (c == '<')
			{
				if (caseCompare("/script>", input, state, &c))
				{
					FREE(state->html->tag);
					break;
				}
			}
			c = htmlGetByte(input, state);
		}
	}

	return c;
}

void
htmlRead(void *a, Input *input, unsigned char *base)
{
	unsigned short	c;
	HTML		html;
	HTMLState	state;

	if (!htmlInitialized)
	{
		htmlInit();
	}

	html.base = copyString(base);
	html.url = copyString(base);
	html.tag = NULL;
	html.attributes = NULL;
	html.currentAttribute = NULL;

	state.mask = 0;
	state.saved = 0;
	state.unGotten = 256;
	state.html = &html;

	c = htmlGetByte(input, &state);
	while (c != 256)
	{
		if (c == '<')
		{
			c = htmlGetByte(input, &state);
			htmlUnGetByte(&state);
			if
			(
				(('a' <= c) && (c <= 'z')) ||
				(('A' <= c) && (c <= 'Z')) ||
				(c == '/') ||
				(c == '!')
			)
			{
				c = readTag(a, input, &state);
				c = dealWithScript(input, &state, c);
			}
			else
			{
				diag(__LINE__, &state, c);
				c = readText(a, input, &state);
			}
		}
		else
		{
			c = readText(a, input, &state);
		}
	}

	FREE(html.base);
	FREE(html.tag);
	htmlFreeAttributes(&state);
}

unsigned char *
toHTML(unsigned char *str)
{
	unsigned char   *escaped_str;
	unsigned char   *result;

        escaped_str = escapeHTML(str);

	result = NULL;

        result = calloc(strlen((char *) escaped_str)+2, 1);
	if (!result)
	{
	        fprintf(stderr, "cannot calloc toHTML string\n");
		exit(0);
	}
        result[0] = '"';
        strcat((char *) result, (char *) escaped_str);
        strcat((char *) result, "\"");

	return result;
}

unsigned char *
escapeHTML(unsigned char *str)
{
	char		buf[2];
	int		i;
	int		j;
	int		len;
	char		*replacement;
	unsigned char	*result;

	buf[1] = 0;
	len = 0;
	result = NULL;

	for (i = 0; i < 2; i++)
	{
		for (j = 0; str[j]; j++)
		{
			switch (str[j])
			{
			case '<':
				replacement = "&lt;";
				break;
			case '>':
				replacement = "&gt;";
				break;
			case '&':
				replacement = "&amp;";
				break;
			case '"':
				replacement = "&quot;";
				break;
			default:
				replacement = buf;
				buf[0] = str[j];
				break;
			}
			if (result)
			{
				strcat((char *) result, replacement);
			}
			else
			{
				len += strlen(replacement);
			}
		}
		if (!result)
		{
			result = calloc(len + 1, 1);
			if (!result)
			{
				fprintf(stderr,
					"cannot calloc escapeHTML string\n");
				exit(0);
			}
		}
	}

	return result;
}
