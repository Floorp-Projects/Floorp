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

typedef struct StackEntry
{
	unsigned char		*str;
	struct StackEntry	*next;
	struct StackEntry	*previous;
} StackEntry;

typedef struct Stack
{
	StackEntry	*bottom;
	StackEntry	*top;
} Stack;

static URL *
urlAlloc(void)
{
	URL	*result;

	result = calloc(sizeof(URL), 1);
	if (!result)
	{
		fprintf(stderr, "cannot calloc URL\n");
		exit(0);
	}
	result->port = -1;

	return result;
}

void
urlFree(URL *url)
{
	FREE(url->file);
	FREE(url->fragment);
	FREE(url->host);
	FREE(url->login);
	FREE(url->net_loc);
	FREE(url->params);
	FREE(url->password);
	FREE(url->path);
	FREE(url->pathWithoutFile);
	FREE(url->query);
	FREE(url->scheme);
	FREE(url->url);
	FREE(url);
}

static void
urlEmbellish(URL *url)
{
	unsigned char	*at;
	unsigned char	*colon;
	unsigned char	*host;
	unsigned char	*login;
	unsigned char	*p;

	p = (unsigned char *) strrchr((char *) url->path, '/');
	if (p)
	{
		FREE(url->pathWithoutFile);
		url->pathWithoutFile = copySizedString(url->path,
			p + 1 - url->path);
		p++;
	}
	else
	{
		p = url->path;
	}
	if (p[0])
	{
		FREE(url->file);
		url->file = copyString(p);
	}
	if (url->net_loc)
	{
		at = (unsigned char *) strchr((char *) url->net_loc, '@');
		if (at)
		{
			login = url->net_loc;
			colon = (unsigned char *) strchr((char *) login, ':');
			if (colon && (colon < at))
			{
				url->password = copySizedString(colon + 1,
					at - colon - 1);
				url->login = copySizedString(login,
					colon - login);
			}
			else
			{
				url->login = copySizedString(login,
					at - login);
			}
			host = at + 1;
		}
		else
		{
			host = url->net_loc;
		}
		colon = (unsigned char *) strchr((char *) host, ':');
		if (colon)
		{
			url->host = lowerCase(copySizedString(host,
				colon - host));
			sscanf((char *) colon + 1, "%d", &url->port);
		}
		else
		{
			FREE(url->host);
			url->host = lowerCase(copyString(host));
		}
	}
}

URL *
urlParse(const unsigned char *urlStr)
{
	unsigned char	c;
	unsigned char	*net_loc;
	unsigned char	*p;
	unsigned char	*path;
	unsigned char	*str;
	URL		*url;

	if ((!urlStr) || (!*urlStr))
	{
		return NULL;
	}
	url = urlAlloc();
	url->url = copyString(urlStr);
	str = copyString(urlStr);
	p = (unsigned char *) strchr((char *) str, '#');
	if (p)
	{
		url->fragment = copyString(p);
		*p = 0;
	}
	p = str;
	c = *p;
	while
	(
		(('a' <= c) && (c <= 'z')) ||
		(('A' <= c) && (c <= 'Z')) ||
		(('0' <= c) && (c <= '9')) ||
		(c == '+') ||
		(c == '.') ||
		(c == '-')
	)
	{
		p++;
		c = *p;
	}
	if ((c == ':') && (p > str))
	{
		url->scheme = lowerCase(copySizedString(str, p - str));
		p++;
	}
	else
	{
		p = str;
	}
	if ((p[0] == '/') && (p[1] == '/'))
	{
		net_loc = p + 2;
		p = (unsigned char *) strchr((char *) net_loc, '/');
		if (p)
		{
			if (p > net_loc)
			{
				url->net_loc = copySizedString(net_loc,
					p - net_loc);
			}
		}
		else
		{
			if (*net_loc)
			{
				url->net_loc = copyString(net_loc);
			}
			p = (unsigned char *) strchr((char *) net_loc, 0);
		}
	}
	path = p;
	p = (unsigned char *) strchr((char *) p, '?');
	if (p)
	{
		url->query = copyString(p);
		*p = 0;
	}
	p = path;
	p = (unsigned char *) strchr((char *) p, ';');
	if (p)
	{
		url->params = copyString(p);
		*p = 0;
	}
	url->path = copyString(path);

	urlEmbellish(url);

	free(str);

	return url;
}

static unsigned char *
pop(Stack *stack)
{
	unsigned char		*result;
	StackEntry		*top;

	if (stack->top)
	{
		top = stack->top;
		result = top->str;
		stack->top = top->previous;
		if (stack->top)
		{
			stack->top->next = NULL;
		}
		else
		{
			stack->bottom = NULL;
		}
		free(top);
	}
	else
	{
		result = NULL;
	}

	return result;
}

static void
push(Stack *stack, unsigned char *str)
{
	StackEntry	*entry;

	entry = calloc(sizeof(StackEntry), 1);
	if (!entry)
	{
		fprintf(stderr, "cannot calloc StackEntry\n");
		exit(0);
	}
	entry->str = str;
	entry->next = NULL;
	entry->previous = stack->top;
	if (stack->top)
	{
		stack->top->next = entry;
	}
	stack->top = entry;
	if (!stack->bottom)
	{
		stack->bottom = entry;
	}
}

static unsigned char *
bottom(Stack *stack)
{
	StackEntry		*bottom;
	unsigned char		*result;

	bottom = stack->bottom;
	if (bottom)
	{
		result = bottom->str;
		stack->bottom = bottom->next;
		if (stack->bottom)
		{
			stack->bottom->previous = NULL;
		}
		free(bottom);
	}
	else
	{
		result = NULL;
	}

	return result;
}

static Stack *
stackAlloc(void)
{
	Stack	*stack;

	stack = calloc(sizeof(Stack), 1);
	if (!stack)
	{
		fprintf(stderr, "cannot calloc Stack\n");
		exit(0);
	}

	return stack;
}

static void
stackFree(Stack *stack)
{
	free(stack);
}

static void
urlCanonicalizePath(URL *url)
{
	int		absolute;
	unsigned char	*begin;
	unsigned char	*p;
	unsigned char	*slash;
	Stack		*stack;
	unsigned char	*str;

	p = url->path;
	if ((!p) || (!*p))
	{
		return;
	}
	if (p[0] == '/')
	{
		absolute = 1;
		p++;
	}
	else
	{
		absolute = 0;
	}

	stack = stackAlloc();
	while (*p)
	{
		begin = p;
		p = (unsigned char *) strchr((char *) begin, '/');
		if (!p)
		{
			p = (unsigned char *) strchr((char *) begin, 0);
		}
		if (p == begin)
		{
		}
		else if ((p == begin + 1) && (begin[0] == '.'))
		{
		}
		else if
		(
			(p == begin + 2) &&
			(begin[0] == '.') &&
			(begin[1] == '.')
		)
		{
			slash = pop(stack);
			str = pop(stack);
			if (!str)
			{
				push(stack, copyString((unsigned char *) ".."));
				if (*p)
				{
					push(stack, copyString(
						(unsigned char *) "/"));
				}
			}
			else if (!strcmp((char *) str, ".."))
			{
				push(stack, str);
				push(stack, slash);
				push(stack, copyString((unsigned char *) ".."));
				if (*p)
				{
					push(stack, copyString(
						(unsigned char *) "/"));
				}
			}
			else
			{
				free(slash);
				free(str);
			}
		}
		else
		{
			push(stack, copySizedString(begin, p - begin));
			if (*p)
			{
				push(stack, copyString((unsigned char *) "/"));
			}
		}
		if (*p)
		{
			p++;
		}
	}

	if (absolute)
	{
		url->path[0] = '/';
		url->path[1] = 0;
	}
	else
	{
		url->path[0] = 0;
	}
	while (1)
	{
		p = bottom(stack);
		if (p)
		{
			strcat((char *) url->path, (char *) p);
			free(p);
		}
		else
		{
			break;
		}
	}
	stackFree(stack);
}

URL *
urlRelative(const unsigned char *baseURL, const unsigned char *relativeURL)
{
	URL		*base;
	int		len;
	URL		*rel;
	unsigned char	*tmp;

	if ((!baseURL) || (!*baseURL))
	{
		return urlParse(relativeURL);
	}
	if ((!relativeURL) || (!*relativeURL))
	{
		return urlParse(baseURL);
	}
	rel = urlParse(relativeURL);
	if (rel->scheme)
	{
		return rel;
	}
	else
	{
		base = urlParse(baseURL);
		if (base->scheme)
		{
			rel->scheme = copyString(base->scheme);
		}
		else
		{
			/* XXX Base is supposed to have scheme. Oh well. */
			return rel;
		}
	}
	if (rel->net_loc)
	{
		goto step7;
	}
	else
	{
		rel->net_loc = copyString(base->net_loc);
	}
	if (rel->path && rel->path[0] == '/')
	{
		goto step7;
	}
	if ((!rel->path) || (!*rel->path))
	{
		FREE(rel->path);
		rel->path = copyString(base->path);
		if (rel->params)
		{
			goto step7;
		}
		rel->params = copyString(base->params);
		if (rel->query)
		{
			goto step7;
		}
		rel->query = copyString(base->query);
		goto step7;
	}
	if (base->pathWithoutFile)
	{
		tmp = rel->path;
		rel->path = appendString(base->pathWithoutFile, rel->path);
		FREE(tmp);
	}
	urlCanonicalizePath(rel);

step7:
	len = strlen((char *) rel->scheme);
	len += 1; /* ":" */
	if (rel->net_loc)
	{
		len += 2 + strlen((char *) rel->net_loc); /* "//net_loc" */
	}
	if (rel->path)
	{
		len += strlen((char *) rel->path);
	}
	if (rel->params)
	{
		len += strlen((char *) rel->params);
	}
	if (rel->query)
	{
		len += strlen((char *) rel->query);
	}
	if (rel->fragment)
	{
		len += strlen((char *) rel->fragment);
	}
	FREE(rel->url);
	rel->url = calloc(len + 1, 1);
	if (!rel->url)
	{
		fprintf(stderr, "cannot calloc url\n");
		exit(0);
	}
	strcpy((char *) rel->url, (char *) rel->scheme);
	strcat((char *) rel->url, ":");
	if (rel->net_loc)
	{
		strcat((char *) rel->url, "//");
		strcat((char *) rel->url, (char *) rel->net_loc);
	}
	if (rel->path)
	{
		strcat((char *) rel->url, (char *) rel->path);
	}
	if (rel->params)
	{
		strcat((char *) rel->url, (char *) rel->params);
	}
	if (rel->query)
	{
		strcat((char *) rel->url, (char *) rel->query);
	}
	if (rel->fragment)
	{
		strcat((char *) rel->url, (char *) rel->fragment);
	}

	urlEmbellish(rel);

	urlFree(base);

	return rel;
}

void
urlDecode(unsigned char *url)
{
	unsigned char	c;
	unsigned char	*in;
	unsigned char	*out;
	unsigned int	tmp;

	in = url;
	out = url;
	while (1)
	{
		c = *in++;
		if (!c)
		{
			break;
		}
		else if (c == '%')
		{
			sscanf((char *) in, "%02x", &tmp);
			if (*in)
			{
				in++;
				if (*in)
				{
					in++;
				}
			}
			*out++ = tmp;
		}
		else
		{
			*out++ = c;
		}
	}
	*out++ = 0;
}
