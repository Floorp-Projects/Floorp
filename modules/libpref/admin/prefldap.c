/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* LDAP functions callable from JavaScript */

#include "ldap.h"
#include "xp_mcom.h"
#ifdef XP_WIN
#include "jsapi.h"
#endif
#include "prefldap.h"

/*
 * Creates an LDAP search URL given a comma-separated list of attributes.
 * Returns a list of key=values separated by '\n'
 */
#if defined(XP_WIN)
PR_IMPLEMENT(char*) 
#else
char * 
#endif
pref_get_ldap_attributes(char* host, char* base, char* filter, char* attrs,
	char** return_error)
{
	char *value = NULL;
	LDAP* ld;
	int err, i;
	char *url;
	LDAPMessage *result;
	LDAPMessage	*e;
	char *a;
	BerElement	*ber;
	char **vals;
	
	ld = ldap_init(host, LDAP_PORT);
	if (!ld)
		return value;
		
	url = (char*) malloc(sizeof(char) *
		 (strlen(host) + strlen(base) + strlen(filter) + strlen(attrs) + 20));
	if (!url)
		return value;
		
	XP_SPRINTF(url, "ldap://%s/%s?%s?sub?%s", host, base, attrs, filter);
	
	err = ldap_url_search_s( ld, url, 0, &result );
	XP_FREE(url);
	if (err != LDAP_SUCCESS) {
		*return_error = ldap_err2string(err);
		return value;
	}
	
	e = ldap_first_entry( ld, result );

	if (e) {
		a = ldap_first_attribute( ld, e, &ber );
		if (a) {
			int total_buf_size = 200;
			int remaining_buf_size = total_buf_size;
			value = (char*) malloc(sizeof(char*) * total_buf_size);
			if (!value)
				return NULL;
			value[0] = '\0';
			
			for ( ; a != NULL; a = ldap_next_attribute( ld, e, ber )) {
				vals = ldap_get_values( ld, e, a );
				if (vals && vals[0]) {
					remaining_buf_size -= (strlen(a) + strlen(vals[0]) + 2);
					if (remaining_buf_size < 1) {
						remaining_buf_size += 2 * total_buf_size;
						total_buf_size += 2 * total_buf_size;
						value = (char*) realloc(value, sizeof(char*) * total_buf_size);
						if (!value)
							return NULL;
					}
					
					strcat(value, "\n");
					strcat(value, a);
					strcat(value, "=");
					strcat(value, vals[0]);
					
					ldap_value_free( vals );
				}
			}
			ldap_memfree(a);
		}
		if (ber)
			ber_free(ber, 0);
	}

	ldap_msgfree(result);
	ldap_unbind(ld);
	
	return value;
}

