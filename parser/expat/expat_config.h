/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __expat_config_h__
#define __expat_config_h__

#define MOZ_UNICODE
#include "nspr.h"

#ifdef IS_LITTLE_ENDIAN
#define BYTEORDER 1234
#else 
#define BYTEORDER 4321
#endif /* IS_LITTLE_ENDIAN */

#if PR_BYTES_PER_INT != 4
#define int int32_t
#endif /* PR_BYTES_PER_INT != 4 */

/* Other Mozilla code relies on memmove already, so we assume it's available */
#define HAVE_MEMMOVE 1

#define XMLCALL
#define XML_STATIC
#define XMLIMPORT

#define XML_UNICODE
typedef PRUnichar XML_Char;
typedef char XML_LChar;
#define XML_T(x) (PRUnichar)x
#define XML_L(x) x

#define XML_DTD
#define XML_NS

/* avoid conflicts with system version of libexpat */

/* expat.h */
#define XML_SetElementDeclHandler MOZ_XML_SetElementDeclHandler
#define XML_SetAttlistDeclHandler MOZ_XML_SetAttlistDeclHandler
#define XML_SetXmlDeclHandler MOZ_XML_SetXmlDeclHandler
#define XML_ParserCreate MOZ_XML_ParserCreate
#define XML_ParserCreateNS MOZ_XML_ParserCreateNS
#define XML_ParserCreate_MM MOZ_XML_ParserCreate_MM
#define XML_ParserReset MOZ_XML_ParserReset
#define XML_SetEntityDeclHandler MOZ_XML_SetEntityDeclHandler
#define XML_SetElementHandler MOZ_XML_SetElementHandler
#define XML_SetStartElementHandler MOZ_XML_SetStartElementHandler
#define XML_SetEndElementHandler MOZ_XML_SetEndElementHandler
#define XML_SetCharacterDataHandler MOZ_XML_SetCharacterDataHandler
#ifndef __VMS
#define XML_SetProcessingInstructionHandler MOZ_XML_SetProcessingInstructionHandler
#else
#define XML_SetProcessingInstrHandler MOZ_XML_SetProcessingInstrHandler
#endif
#define XML_SetCommentHandler MOZ_XML_SetCommentHandler
#define XML_SetCdataSectionHandler MOZ_XML_SetCdataSectionHandler
#define XML_SetStartCdataSectionHandler MOZ_XML_SetStartCdataSectionHandler
#define XML_SetEndCdataSectionHandler MOZ_XML_SetEndCdataSectionHandler
#define XML_SetDefaultHandler MOZ_XML_SetDefaultHandler
#define XML_SetDefaultHandlerExpand MOZ_XML_SetDefaultHandlerExpand
#define XML_SetDoctypeDeclHandler MOZ_XML_SetDoctypeDeclHandler
#define XML_SetStartDoctypeDeclHandler MOZ_XML_SetStartDoctypeDeclHandler
#define XML_SetEndDoctypeDeclHandler MOZ_XML_SetEndDoctypeDeclHandler
#ifndef __VMS
#define XML_SetUnparsedEntityDeclHandler MOZ_XML_SetUnparsedEntityDeclHandler
#else
#define XML_SetUnparsedEntDeclHandler MOZ_XML_SetUnparsedEntDeclHandler
#endif
#define XML_SetNotationDeclHandler MOZ_XML_SetNotationDeclHandler
#define XML_SetNamespaceDeclHandler MOZ_XML_SetNamespaceDeclHandler
#ifndef __VMS
#define XML_SetStartNamespaceDeclHandler MOZ_XML_SetStartNamespaceDeclHandler
#else
#define XML_SetStartNamespcDeclHandler MOZ_XML_SetStartNamespcDeclHandler
#endif
#define XML_SetEndNamespaceDeclHandler MOZ_XML_SetEndNamespaceDeclHandler
#define XML_SetNotStandaloneHandler MOZ_XML_SetNotStandaloneHandler
#define XML_SetExternalEntityRefHandler MOZ_XML_SetExternalEntityRefHandler
#ifndef __VMS
#define XML_SetExternalEntityRefHandlerArg MOZ_XML_SetExternalEntityRefHandlerArg
#else
#define XML_SetExternalEntRefHandlerArg MOZ_XML_SetExternalEntRefHandlerArg
#endif
#define XML_SetSkippedEntityHandler MOZ_XML_SetSkippedEntityHandler
#define XML_SetUnknownEncodingHandler MOZ_XML_SetUnknownEncodingHandler
#define XML_DefaultCurrent MOZ_XML_DefaultCurrent
#define XML_SetReturnNSTriplet MOZ_XML_SetReturnNSTriplet
#define XML_SetUserData MOZ_XML_SetUserData
#define XML_SetEncoding MOZ_XML_SetEncoding
#define XML_UseParserAsHandlerArg MOZ_XML_UseParserAsHandlerArg
#define XML_UseForeignDTD MOZ_XML_UseForeignDTD
#define XML_SetBase MOZ_XML_SetBase
#define XML_GetBase MOZ_XML_GetBase
#define XML_GetSpecifiedAttributeCount MOZ_XML_GetSpecifiedAttributeCount
#define XML_GetIdAttributeIndex MOZ_XML_GetIdAttributeIndex
#define XML_Parse MOZ_XML_Parse
#define XML_GetBuffer MOZ_XML_GetBuffer
#define XML_ParseBuffer MOZ_XML_ParseBuffer
#define XML_StopParser MOZ_XML_StopParser
#define XML_ResumeParser MOZ_XML_ResumeParser
#define XML_GetParsingStatus MOZ_XML_GetParsingStatus
#define XML_ExternalEntityParserCreate MOZ_XML_ExternalEntityParserCreate
#define XML_SetParamEntityParsing MOZ_XML_SetParamEntityParsing
#define XML_GetErrorCode MOZ_XML_GetErrorCode
#define XML_GetCurrentLineNumber MOZ_XML_GetCurrentLineNumber
#define XML_GetCurrentColumnNumber MOZ_XML_GetCurrentColumnNumber
#define XML_GetCurrentByteIndex MOZ_XML_GetCurrentByteIndex
#define XML_GetCurrentByteCount MOZ_XML_GetCurrentByteCount
#define XML_GetInputContext MOZ_XML_GetInputContext
#define XML_FreeContentModel MOZ_XML_FreeContentModel
#define XML_MemMalloc MOZ_XML_MemMalloc
#define XML_MemRealloc MOZ_XML_MemRealloc
#define XML_MemFree MOZ_XML_MemFree
#define XML_ParserFree MOZ_XML_ParserFree
#define XML_ErrorString MOZ_XML_ErrorString
#define XML_ExpatVersion MOZ_XML_ExpatVersion
#define XML_ExpatVersionInfo MOZ_XML_ExpatVersionInfo
#define XML_GetFeatureList MOZ_XML_GetFeatureList

/* xmlrole.h */
#define XmlPrologStateInit MOZ_XmlPrologStateInit
#ifndef __VMS
#define XmlPrologStateInitExternalEntity MOZ_XmlPrologStateInitExternalEntity
#else
#define XmlPrologStateInitExternalEnt MOZ_XmlPrologStateInitExternalEnt
#endif

/* xmltok.h */
#define XmlParseXmlDecl MOZ_XmlParseXmlDecl
#define XmlParseXmlDeclNS MOZ_XmlParseXmlDeclNS
#define XmlInitEncoding MOZ_XmlInitEncoding
#define XmlInitEncodingNS MOZ_XmlInitEncodingNS
#define XmlGetUtf8InternalEncoding MOZ_XmlGetUtf8InternalEncoding
#define XmlGetUtf16InternalEncoding MOZ_XmlGetUtf16InternalEncoding
#define XmlGetUtf8InternalEncodingNS MOZ_XmlGetUtf8InternalEncodingNS
#define XmlGetUtf16InternalEncodingNS MOZ_XmlGetUtf16InternalEncodingNS
#define XmlUtf8Encode MOZ_XmlUtf8Encode
#define XmlUtf16Encode MOZ_XmlUtf16Encode
#define XmlSizeOfUnknownEncoding MOZ_XmlSizeOfUnknownEncoding
#define XmlInitUnknownEncoding MOZ_XmlInitUnknownEncoding
#define XmlInitUnknownEncodingNS MOZ_XmlInitUnknownEncodingNS

#endif /* __expat_config_h__ */
