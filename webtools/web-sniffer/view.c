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

#define CONTROL_START "<font color=#FF0000>"
#define CONTROL(str) CONTROL_START str CONTROL_END
#define CONTROL_END "</font>"
#define NL "\n"

static int verbose = 0;

static void
print(View *view, Buf *b)
{
	char		buf[1024];
	char		*hex;
	char		hexBuf[4];
	int		i;
	unsigned long	inLen;
	unsigned long	j;
	int		len;
	char		*p;
	char		*replacement;
	char		*result;
	unsigned char	*str;

	hex = "0123456789ABCDEF";
	str = bufCopyMemory(b, &inLen);

	buf[1] = 0;
	len = 0;
	p = NULL;
	result = NULL;

	for (i = 0; i < 2; i++)
	{
		for (j = 0; j < inLen; j++)
		{
			switch (str[j])
			{
			case '\r':
				if (((j + 1) < inLen) && (str[j + 1] == '\n'))
				{
					j++;
					replacement = CONTROL("CRLF") NL;
				}
				else
				{
					replacement = CONTROL("CR") NL;
				}
				break;
			case '\n':
				replacement = CONTROL("LF") NL;
				break;
			case '\t':
				replacement = CONTROL("TAB");
				break;
			case 0x1b:
				replacement = CONTROL("ESC");
				break;
			case '<':
				replacement = "&lt;";
				break;
			case '>':
				replacement = "&gt;";
				break;
			case '&':
				replacement = "&amp;";
				break;
			case '\\':
			case '"':
				if (view->backslash)
				{
					buf[0] = '\\';
					buf[1] = str[j];
					buf[2] = 0;
				}
				else
				{
					buf[0] = str[j];
					buf[1] = 0;
				}
				replacement = buf;
				break;
			default:
				if ((str[j] <= 0x1f) || (str[j] >= 0x7f))
				{
					replacement = buf;
					strcpy(buf, CONTROL_START);
					hexBuf[0] = 'x';
					hexBuf[1] = hex[str[j] >> 4];
					hexBuf[2] = hex[str[j] & 0x0f];
					hexBuf[3] = 0;
					strcat(buf, hexBuf);
					strcat(buf, CONTROL_END);
				}
				else
				{
					replacement = buf;
					buf[0] = str[j];
					buf[1] = 0;
				}
				break;
			}
			if (result)
			{
				strcpy(p, replacement);
				p += strlen(replacement);
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
					"cannot calloc toHTML string\n");
				exit(0);
			}
			p = result;
		}
	}

	fprintf(view->out, "%s", result);

	free(result);
	free(str);
}

void
viewHTML(App *app, Buf *buf)
{
	fprintf(app->view.out, "<font color=#009900>");
	print(&app->view, buf);
	fprintf(app->view.out, "</font>");
}

void
viewHTMLAttributeName(App *app, Buf *buf)
{
	fprintf(app->view.out, "<font color=#FF6600>");
	print(&app->view, buf);
	fprintf(app->view.out, "</font>");
}

void
viewHTMLAttributeValue(App *app, Buf *buf)
{
	fprintf(app->view.out, "<font color=#3333FF>");
	print(&app->view, buf);
	fprintf(app->view.out, "</font>");
}

void
viewHTMLDeclaration(App *app, Buf *buf)
{
	fprintf(app->view.out, "<font color=#999933><i>");
	print(&app->view, buf);
	fprintf(app->view.out, "</i></font>");
}

void
viewHTMLProcessingInstruction(App *app, Buf *buf)
{
	fprintf(app->view.out, "<font color=#660000><i>");
	print(&app->view, buf);
	fprintf(app->view.out, "</i></font>");
}

void
viewHTMLTag(App *app, Buf *buf)
{
	fprintf(app->view.out, "<font color=#CC33CC>");
	print(&app->view, buf);
	fprintf(app->view.out, "</font>");
}

void
viewHTMLText(App *app, Buf *buf)
{
	print(&app->view, buf);
}

void
viewHTTP(App *app, Buf *buf)
{
	print(&app->view, buf);
}

void
viewHTTPHeaderName(App *app, Buf *buf)
{
	fprintf(app->view.out, "<font color=#FF6600>");
	print(&app->view, buf);
	fprintf(app->view.out, "</font>");
}

void
viewHTTPHeaderValue(App *app, Buf *buf)
{
	fprintf(app->view.out, "<font color=#3333FF>");
	print(&app->view, buf);
	fprintf(app->view.out, "</font>");
}

void
viewVerbose(void)
{
	verbose = 1;
}

void
viewPrintHTML(App *app, char *str)
{
	fprintf(app->view.out, "%s", str);
	fflush(app->view.out);
}

void
viewReport(App *app, char *str)
{
	unsigned char	*esc;

	if (verbose)
	{
		esc = escapeHTML((unsigned char *) str);
		fprintf(app->view.out, "%s\n", (char *) esc);
		free(esc);
		fflush(app->view.out);
	}
}

void
viewReportHTML(App *app, char *str)
{
	if (verbose)
	{
		fprintf(app->view.out, "%s\n", str);
		fflush(app->view.out);
	}
}
