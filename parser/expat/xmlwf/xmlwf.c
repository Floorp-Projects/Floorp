/*
The contents of this file are subject to the Mozilla Public License
Version 1.0 (the "License"); you may not use this file except in
compliance with the License. You may obtain a copy of the License at
http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS"
basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
License for the specific language governing rights and limitations
under the License.

The Original Code is expat.

The Initial Developer of the Original Code is James Clark.
Portions created by James Clark are Copyright (C) 1998
James Clark. All Rights Reserved.

Contributor(s):
*/

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "xmlparse.h"
#include "codepage.h"
#include "xmlfile.h"
#include "xmltchar.h"

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#define NSSEP T('#')

static void characterData(void *userData, const XML_Char *s, int len)
{
  FILE *fp = userData;
  for (; len > 0; --len, ++s) {
    switch (*s) {
    case T('&'):
      fputts(T("&amp;"), fp);
      break;
    case T('<'):
      fputts(T("&lt;"), fp);
      break;
    case T('>'):
      fputts(T("&gt;"), fp);
      break;
    case T('"'):
      fputts(T("&quot;"), fp);
      break;
    case 9:
    case 10:
    case 13:
      ftprintf(fp, T("&#%d;"), *s);
      break;
    default:
      puttc(*s, fp);
      break;
    }
  }
}

/* Lexicographically comparing UTF-8 encoded attribute values,
is equivalent to lexicographically comparing based on the character number. */

static int attcmp(const void *att1, const void *att2)
{
  return tcscmp(*(const XML_Char **)att1, *(const XML_Char **)att2);
}

static void startElement(void *userData, const XML_Char *name, const XML_Char **atts)
{
  int nAtts;
  const XML_Char **p;
  FILE *fp = userData;
  puttc(T('<'), fp);
  fputts(name, fp);

  p = atts;
  while (*p)
    ++p;
  nAtts = (p - atts) >> 1;
  if (nAtts > 1)
    qsort((void *)atts, nAtts, sizeof(XML_Char *) * 2, attcmp);
  while (*atts) {
    puttc(T(' '), fp);
    fputts(*atts++, fp);
    puttc(T('='), fp);
    puttc(T('"'), fp);
    characterData(userData, *atts, tcslen(*atts));
    puttc(T('"'), fp);
    atts++;
  }
  puttc(T('>'), fp);
}

static void endElement(void *userData, const XML_Char *name)
{
  FILE *fp = userData;
  puttc(T('<'), fp);
  puttc(T('/'), fp);
  fputts(name, fp);
  puttc(T('>'), fp);
}

static void startElementNS(void *userData, const XML_Char *name, const XML_Char **atts)
{
  int nAtts;
  int nsi;
  const XML_Char **p;
  FILE *fp = userData;
  const XML_Char *sep;
  puttc(T('<'), fp);

  sep = tcsrchr(name, NSSEP);
  if (sep) {
    fputts(T("ns0:"), fp);
    fputts(sep + 1, fp);
    fputts(T(" xmlns:ns0=\""), fp);
    characterData(userData, name, sep - name);
    puttc(T('"'), fp);
    nsi = 1;
  }
  else {
    fputts(name, fp);
    nsi = 0;
  }

  p = atts;
  while (*p)
    ++p;
  nAtts = (p - atts) >> 1;
  if (nAtts > 1)
    qsort((void *)atts, nAtts, sizeof(XML_Char *) * 2, attcmp);
  while (*atts) {
    name = *atts++;
    sep = tcsrchr(name, NSSEP);
    if (sep) {
      ftprintf(fp, T(" xmlns:ns%d=\""), nsi);
      characterData(userData, name, sep - name);
      puttc(T('"'), fp);
      name = sep + 1;
      ftprintf(fp, T(" ns%d:"), nsi++);
    }
    else
      puttc(T(' '), fp);
    fputts(name, fp);
    puttc(T('='), fp);
    puttc(T('"'), fp);
    characterData(userData, *atts, tcslen(*atts));
    puttc(T('"'), fp);
    atts++;
  }
  puttc(T('>'), fp);
}

static void endElementNS(void *userData, const XML_Char *name)
{
  FILE *fp = userData;
  const XML_Char *sep;
  puttc(T('<'), fp);
  puttc(T('/'), fp);
  sep = tcsrchr(name, NSSEP);
  if (sep) {
    fputts(T("ns0:"), fp);
    fputts(sep + 1, fp);
  }
  else
    fputts(name, fp);
  puttc(T('>'), fp);
}

static void processingInstruction(void *userData, const XML_Char *target, const XML_Char *data)
{
  FILE *fp = userData;
  puttc(T('<'), fp);
  puttc(T('?'), fp);
  fputts(target, fp);
  puttc(T(' '), fp);
  fputts(data, fp);
  puttc(T('?'), fp);
  puttc(T('>'), fp);
}

static void defaultCharacterData(XML_Parser parser, const XML_Char *s, int len)
{
  XML_DefaultCurrent(parser);
}

static void defaultStartElement(XML_Parser parser, const XML_Char *name, const XML_Char **atts)
{
  XML_DefaultCurrent(parser);
}

static void defaultEndElement(XML_Parser parser, const XML_Char *name)
{
  XML_DefaultCurrent(parser);
}

static void defaultProcessingInstruction(XML_Parser parser, const XML_Char *target, const XML_Char *data)
{
  XML_DefaultCurrent(parser);
}

static void nopCharacterData(XML_Parser parser, const XML_Char *s, int len)
{
}

static void nopStartElement(XML_Parser parser, const XML_Char *name, const XML_Char **atts)
{
}

static void nopEndElement(XML_Parser parser, const XML_Char *name)
{
}

static void nopProcessingInstruction(XML_Parser parser, const XML_Char *target, const XML_Char *data)
{
}

static void markup(XML_Parser parser, const XML_Char *s, int len)
{
  FILE *fp = XML_GetUserData(parser);
  for (; len > 0; --len, ++s)
    puttc(*s, fp);
}

static
void metaLocation(XML_Parser parser)
{
  const XML_Char *uri = XML_GetBase(parser);
  if (uri)
    ftprintf(XML_GetUserData(parser), T(" uri=\"%s\""), uri);
  ftprintf(XML_GetUserData(parser),
           T(" byte=\"%ld\" line=\"%d\" col=\"%d\""),
	   XML_GetCurrentByteIndex(parser),
	   XML_GetCurrentLineNumber(parser),
	   XML_GetCurrentColumnNumber(parser));
}

static
void metaStartDocument(XML_Parser parser)
{
  fputts(T("<document>\n"), XML_GetUserData(parser));
}

static
void metaEndDocument(XML_Parser parser)
{
  fputts(T("</document>\n"), XML_GetUserData(parser));
}

static
void metaStartElement(XML_Parser parser, const XML_Char *name, const XML_Char **atts)
{
  FILE *fp = XML_GetUserData(parser);
  ftprintf(fp, T("<starttag name=\"%s\""), name);
  metaLocation(parser);
  if (*atts) {
    fputts(T(">\n"), fp);
    do {
      ftprintf(fp, T("<attribute name=\"%s\" value=\""), atts[0]);
      characterData(fp, atts[1], tcslen(atts[1]));
      fputts(T("\"/>\n"), fp);
    } while (*(atts += 2));
    fputts(T("</starttag>\n"), fp);
  }
  else
    fputts(T("/>\n"), fp);
}

static
void metaEndElement(XML_Parser parser, const XML_Char *name)
{
  FILE *fp = XML_GetUserData(parser);
  ftprintf(fp, T("<endtag name=\"%s\""), name);
  metaLocation(parser);
  fputts(T("/>\n"), fp);
}

static
void metaProcessingInstruction(XML_Parser parser, const XML_Char *target, const XML_Char *data)
{
  FILE *fp = XML_GetUserData(parser);
  ftprintf(fp, T("<pi target=\"%s\" data=\""), target);
  characterData(fp, data, tcslen(data));
  puttc(T('"'), fp);
  metaLocation(parser);
  fputts(T("/>\n"), fp);
}

static
void metaComment(XML_Parser parser, const XML_Char *data)
{
  FILE *fp = XML_GetUserData(parser);
  fputts(T("<comment data=\""), fp);
  characterData(fp, data, tcslen(data));
  puttc(T('"'), fp);
  metaLocation(parser);
  fputts(T("/>\n"), fp);
}

static
void metaStartCdataSection(XML_Parser parser)
{
  FILE *fp = XML_GetUserData(parser);
  fputts(T("<startcdata"), fp);
  metaLocation(parser);
  fputts(T("/>\n"), fp);
}

static
void metaEndCdataSection(XML_Parser parser)
{
  FILE *fp = XML_GetUserData(parser);
  fputts(T("<endcdata"), fp);
  metaLocation(parser);
  fputts(T("/>\n"), fp);
}

static
void metaCharacterData(XML_Parser parser, const XML_Char *s, int len)
{
  FILE *fp = XML_GetUserData(parser);
  fputts(T("<chars str=\""), fp);
  characterData(fp, s, len);
  puttc(T('"'), fp);
  metaLocation(parser);
  fputts(T("/>\n"), fp);
}

static
void metaUnparsedEntityDecl(XML_Parser parser,
			       const XML_Char *entityName,
			       const XML_Char *base,
			       const XML_Char *systemId,
			       const XML_Char *publicId,
			       const XML_Char *notationName)
{
  FILE *fp = XML_GetUserData(parser);
  ftprintf(fp, T("<entity name=\"%s\""), entityName);
  if (publicId)
    ftprintf(fp, T(" public=\"%s\""), publicId);
  fputts(T(" system=\""), fp);
  characterData(fp, systemId, tcslen(systemId));
  puttc(T('"'), fp);
  ftprintf(fp, T(" notation=\"%s\""), notationName);
  metaLocation(parser);
  fputts(T("/>\n"), fp);
}

static
void metaNotationDecl(XML_Parser parser,
		      const XML_Char *notationName,
		      const XML_Char *base,
		      const XML_Char *systemId,
		      const XML_Char *publicId)
{
  FILE *fp = XML_GetUserData(parser);
  ftprintf(fp, T("<notation name=\"%s\""), notationName);
  if (publicId)
    ftprintf(fp, T(" public=\"%s\""), publicId);
  if (systemId) {
    fputts(T(" system=\""), fp);
    characterData(fp, systemId, tcslen(systemId));
    puttc(T('"'), fp);
  }
  metaLocation(parser);
  fputts(T("/>\n"), fp);
}

static
void metaStartNamespaceDecl(XML_Parser parser,
			    const XML_Char *prefix,
			    const XML_Char *uri)
{
  FILE *fp = XML_GetUserData(parser);
  fputts(T("<startns"), fp);
  if (prefix)
    ftprintf(fp, T(" prefix=\"%s\""), prefix);
  if (uri) {
    fputts(T(" ns=\""), fp);
    characterData(fp, uri, tcslen(uri));
    fputts(T("\"/>\n"), fp);
  }
  else
    fputts(T("/>\n"), fp);
}

static
void metaEndNamespaceDecl(XML_Parser parser, const XML_Char *prefix)
{
  FILE *fp = XML_GetUserData(parser);
  if (!prefix)
    fputts(T("<endns/>\n"), fp);
  else
    ftprintf(fp, T("<endns prefix=\"%s\"/>\n"), prefix);
}

static
int unknownEncodingConvert(void *data, const char *p)
{
  return codepageConvert(*(int *)data, p);
}

static
int unknownEncoding(void *userData,
		    const XML_Char *name,
		    XML_Encoding *info)
{
  int cp;
  static const XML_Char prefixL[] = T("windows-");
  static const XML_Char prefixU[] = T("WINDOWS-");
  int i;

  for (i = 0; prefixU[i]; i++)
    if (name[i] != prefixU[i] && name[i] != prefixL[i])
      return 0;
  
  cp = 0;
  for (; name[i]; i++) {
    static const XML_Char digits[] = T("0123456789");
    const XML_Char *s = tcschr(digits, name[i]);
    if (!s)
      return 0;
    cp *= 10;
    cp += s - digits;
    if (cp >= 0x10000)
      return 0;
  }
  if (!codepageMap(cp, info->map))
    return 0;
  info->convert = unknownEncodingConvert;
  /* We could just cast the code page integer to a void *,
  and avoid the use of release. */
  info->release = free;
  info->data = malloc(sizeof(int));
  if (!info->data)
    return 0;
  *(int *)info->data = cp;
  return 1;
}

static
void usage(const XML_Char *prog)
{
  ftprintf(stderr, T("usage: %s [-n] [-r] [-w] [-x] [-d output-dir] [-e encoding] file ...\n"), prog);
  exit(1);
}

int tmain(int argc, XML_Char **argv)
{
  int i;
  const XML_Char *outputDir = 0;
  const XML_Char *encoding = 0;
  unsigned processFlags = XML_MAP_FILE;
  int windowsCodePages = 0;
  int outputType = 0;
  int useNamespaces = 0;

#ifdef _MSC_VER
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF|_CRTDBG_LEAK_CHECK_DF);
#endif

  i = 1;
  while (i < argc && argv[i][0] == T('-')) {
    int j;
    if (argv[i][1] == T('-') && argv[i][2] == T('\0')) {
      i++;
      break;
    }
    j = 1;
    if (argv[i][j] == T('r')) {
      processFlags &= ~XML_MAP_FILE;
      j++;
    }
    if (argv[i][j] == T('n')) {
      useNamespaces = 1;
      j++;
    }
    if (argv[i][j] == T('x')) {
      processFlags |= XML_EXTERNAL_ENTITIES;
      j++;
    }
    if (argv[i][j] == T('w')) {
      windowsCodePages = 1;
      j++;
    }
    if (argv[i][j] == T('m')) {
      outputType = 'm';
      j++;
    }
    if (argv[i][j] == T('c')) {
      outputType = 'c';
      useNamespaces = 0;
      j++;
    }
    if (argv[i][j] == T('t')) {
      outputType = 't';
      j++;
    }
    if (argv[i][j] == T('d')) {
      if (argv[i][j + 1] == T('\0')) {
	if (++i == argc)
	  usage(argv[0]);
	outputDir = argv[i];
      }
      else
	outputDir = argv[i] + j + 1;
      i++;
    }
    else if (argv[i][j] == T('e')) {
      if (argv[i][j + 1] == T('\0')) {
	if (++i == argc)
	  usage(argv[0]);
	encoding = argv[i];
      }
      else
	encoding = argv[i] + j + 1;
      i++;
    }
    else if (argv[i][j] == T('\0') && j > 1)
      i++;
    else
      usage(argv[0]);
  }
  if (i == argc)
    usage(argv[0]);
  for (; i < argc; i++) {
    FILE *fp = 0;
    XML_Char *outName = 0;
    int result;
    XML_Parser parser;
    if (useNamespaces)
      parser = XML_ParserCreateNS(encoding, NSSEP);
    else
      parser = XML_ParserCreate(encoding);
    if (outputType == 't') {
      /* This is for doing timings; this gives a more realistic estimate of
	 the parsing time. */
      outputDir = 0;
      XML_SetElementHandler(parser, nopStartElement, nopEndElement);
      XML_SetCharacterDataHandler(parser, nopCharacterData);
      XML_SetProcessingInstructionHandler(parser, nopProcessingInstruction);
    }
    else if (outputDir) {
      const XML_Char *file = argv[i];
      if (tcsrchr(file, T('/')))
	file = tcsrchr(file, T('/')) + 1;
#ifdef WIN32
      if (tcsrchr(file, T('\\')))
	file = tcsrchr(file, T('\\')) + 1;
#endif
      outName = malloc((tcslen(outputDir) + tcslen(file) + 2) * sizeof(XML_Char));
      tcscpy(outName, outputDir);
      tcscat(outName, T("/"));
      tcscat(outName, file);
      fp = tfopen(outName, T("wb"));
      if (!fp) {
	tperror(outName);
	exit(1);
      }
      setvbuf(fp, NULL, _IOFBF, 16384);
#ifdef XML_UNICODE
      puttc(0xFEFF, fp);
#endif
      XML_SetUserData(parser, fp);
      switch (outputType) {
      case 'm':
	XML_UseParserAsHandlerArg(parser);
	XML_SetElementHandler(parser, metaStartElement, metaEndElement);
	XML_SetProcessingInstructionHandler(parser, metaProcessingInstruction);
	XML_SetCommentHandler(parser, metaComment);
	XML_SetCdataSectionHandler(parser, metaStartCdataSection, metaEndCdataSection);
	XML_SetCharacterDataHandler(parser, metaCharacterData);
	XML_SetUnparsedEntityDeclHandler(parser, metaUnparsedEntityDecl);
	XML_SetNotationDeclHandler(parser, metaNotationDecl);
	XML_SetNamespaceDeclHandler(parser, metaStartNamespaceDecl, metaEndNamespaceDecl);
	metaStartDocument(parser);
	break;
      case 'c':
	XML_UseParserAsHandlerArg(parser);
	XML_SetDefaultHandler(parser, markup);
	XML_SetElementHandler(parser, defaultStartElement, defaultEndElement);
	XML_SetCharacterDataHandler(parser, defaultCharacterData);
	XML_SetProcessingInstructionHandler(parser, defaultProcessingInstruction);
	break;
      default:
	if (useNamespaces)
	  XML_SetElementHandler(parser, startElementNS, endElementNS);
	else
	  XML_SetElementHandler(parser, startElement, endElement);
	XML_SetCharacterDataHandler(parser, characterData);
	XML_SetProcessingInstructionHandler(parser, processingInstruction);
	break;
      }
    }
    if (windowsCodePages)
      XML_SetUnknownEncodingHandler(parser, unknownEncoding, 0);
    result = XML_ProcessFile(parser, argv[i], processFlags);
    if (outputDir) {
      if (outputType == 'm')
	metaEndDocument(parser);
      fclose(fp);
      if (!result)
	tremove(outName);
      free(outName);
    }
    XML_ParserFree(parser);
  }
  return 0;
}
