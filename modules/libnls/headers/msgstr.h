/* 
 *           CONFIDENTIAL AND PROPRIETARY SOURCE CODE OF
 *              NETSCAPE COMMUNICATIONS CORPORATION
 * Copyright (C) 1997 Netscape Communications Corporation.  All Rights
 * Reserved.  Use of this Source Code is subject to the terms of the
 * applicable license agreement from Netscape Communications Corporation.
 * The copyright notice(s) in this Source Code does not indicate actual or
 * intended publication of this Source Code.
 */

#ifndef _MSGSTR_H
#define _MSGSTR_H

#include "nlsxp.h"
#include "resbdl.h"

class NLSRESAPI_PUBLIC_CLASS MessageString
{
public:
	// CONSTRUCTORS
	MessageString(const char* packageName, const char* keyName, const Formattable* objs,
								  t_int32 cnt);
	MessageString(const char* exportFormat);

	// DESTRUCTOR
	~MessageString();

	// GETTERS
	NLS_ErrorCode				status() const;		// Get error status

	// Looks up a value based on the key passed in.  Returns NULL and
	// sets status to NLS_RESOURCE_NOT_FOUND if not found.
	UnicodeString*		getString(PropertyResourceBundle* bundle, Locale *locale = NULL);
	UnicodeString*		getString(const Locale* locale);
	UnicodeString*		getString(const char* acceptLanguage);

	operator const char*() const;

	char* util_pack_formattable(const Formattable* objs, t_int32 cnt);
	Formattable* util_unpack_formattable(char *packed_formattable);

private:
	NLS_ErrorCode				fStatus;
	char*						fStringResource;
	char* util_escape(char *target, const char *from);
	void util_unescape(char *target);
	int validate_resource(const char *packed_info);
};


#endif /* _MSGSTR_H */


