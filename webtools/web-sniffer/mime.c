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

#define IS_WHITE_SPACE(c)		\
	(				\
		((c) == ' ' ) ||	\
		((c) == '\t') ||	\
		((c) == '\r') ||	\
		((c) == '\n')		\
	)

#define IS_NON_WHITE_SPACE(c)		\
	(				\
		((c) != '\0') &&	\
		((c) != ' ' ) &&	\
		((c) != '\t') &&	\
		((c) != '\r') &&	\
		((c) != '\n')		\
	)

typedef struct ContentTypeParameter
{
	unsigned char			*name;
	unsigned char			*value;
	struct ContentTypeParameter	*next;
} ContentTypeParameter;

struct ContentType
{
	unsigned char		*type;
	ContentTypeParameter	*parameters;
	ContentTypeParameter	*currentParameter;
};

ContentType *
mimeParseContentType(unsigned char *str)
{
	ContentType		*contentType;
	unsigned char		*name;
	unsigned char		*p;
	ContentTypeParameter	*parameter;
	unsigned char		*slash;
	unsigned char		*subtype;
	unsigned char		*type;
	unsigned char		*value;

	if ((!str) || (!*str))
	{
		return NULL;
	}
	while (IS_WHITE_SPACE(*str))
	{
		str++;
	}
	if (!*str)
	{
		return NULL;
	}
	slash = (unsigned char *) strchr((char *) str, '/');
	if ((!slash) || (slash == str))
	{
		return NULL;
	}
	p = slash - 1;
	while (IS_WHITE_SPACE(*p))
	{
		p--;
	}
	p++;
	type = lowerCase(copySizedString(str, p - str));
	p = slash + 1;
	while (IS_WHITE_SPACE(*p))
	{
		p++;
	}
	if (!*p)
	{
		free(type);
		return NULL;
	}
	subtype = p;
	while (IS_NON_WHITE_SPACE(*p) && (*p != ';'))
	{
		p++;
	}
	subtype = lowerCase(copySizedString(subtype, p - subtype));
	contentType = calloc(sizeof(ContentType), 1);
	if (!contentType)
	{
		fprintf(stderr, "cannot calloc ContentType\n");
		exit(0);
	}
	contentType->type = calloc(strlen((char *) type) + 1 +
		strlen((char *) subtype) + 1, 1);
	if (!contentType->type)
	{
		fprintf(stderr, "cannot calloc type\n");
		exit(0);
	}
	strcpy((char *) contentType->type, (char *) type);
	strcat((char *) contentType->type, "/");
	strcat((char *) contentType->type, (char *) subtype);
	free(type);
	free(subtype);
	while (IS_WHITE_SPACE(*p))
	{
		p++;
	}
	if (!*p)
	{
		return contentType;
	}
	if (*p != ';')
	{
		fprintf(stderr, "expected semicolon; got '%c'\n", *p);
		fprintf(stderr, "str: %s\n", str);
		return contentType;
	}
	while (1)
	{
		p++;
		while (IS_WHITE_SPACE(*p))
		{
			p++;
		}
		if (!*p)
		{
			fprintf(stderr, "expected parameter: %s\n", str);
			break;
		}
		name = p;
		while (IS_NON_WHITE_SPACE(*p) && (*p != '='))
		{
			p++;
		}
		parameter = calloc(sizeof(ContentTypeParameter), 1);
		if (!parameter)
		{
			fprintf(stderr, "cannot calloc parameter\n");
			exit(0);
		}
		parameter->name = lowerCase(copySizedString(name, p - name));
		while (IS_WHITE_SPACE(*p))
		{
			p++;
		}
		if (*p != '=')
		{
			fprintf(stderr, "expected '=': %s\n", str);
			return contentType;
		}
		p++;
		while (IS_WHITE_SPACE(*p))
		{
			p++;
		}
		if (*p == '"')
		{
			p++;
			value = p;
			while ((*p) && (*p != '"'))
			{
				p++;
			}
			if (!*p)
			{
				fprintf(stderr, "expected '\"': %s\n", str);
				return contentType;
			}
			parameter->value = copySizedString(value, p - value);
			p++;
		}
		else
		{
			value = p;
			while (IS_NON_WHITE_SPACE(*p) && (*p != ';'))
			{
				p++;
			}
			parameter->value = copySizedString(value, p - value);
		}
		if (contentType->currentParameter)
		{
			contentType->currentParameter->next = parameter;
		}
		else
		{
			contentType->parameters = parameter;
		}
		contentType->currentParameter = parameter;
		while (IS_WHITE_SPACE(*p))
		{
			p++;
		}
		if (!*p)
		{
			break;
		}
	}

	return contentType;
}

unsigned char *
mimeGetContentTypeParameter(ContentType *contentType, char *name)
{
	ContentTypeParameter	*parameter;

	if (!contentType)
	{
		return NULL;
	}

	parameter = contentType->parameters;
	while (parameter)
	{
		if (!strcasecmp((char *) parameter->name, name))
		{
			return copyString(parameter->value);
		}
		parameter = parameter->next;
	}

	return NULL;
}

void
mimeFreeContentType(ContentType *contentType)
{
	ContentTypeParameter	*param;
	ContentTypeParameter	*tmp;

	if (contentType)
	{
		FREE(contentType->type);
		param = contentType->parameters;
		while (param)
		{
			tmp = param;
			FREE(param->name);
			FREE(param->value);
			param = param->next;
			free(tmp);
		}
		free(contentType);
	}
}

unsigned char *
mimeGetContentType(ContentType *contentType)
{
	if (contentType)
	{
		return copyString(contentType->type);
	}
	else
	{
		return NULL;
	}
}
