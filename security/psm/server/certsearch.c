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
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/* LDAP Cert Search */
#if 0
#include <ldap.h>
#endif
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "certsearch.h"

/* XXX TRUE & FALSE is not defined (on Unix): get by for now by using this 
 * macro */
#define SSM_LDAP_TRUE 1
#define SSM_LDAP_FALSE 0


int LDAPCertSearch (const char * rcpt_address, const char * server_name,
                    const char * baseDN, int port, int connect_type,
                    const char * certdb_path, const char * auth_dn, 
                    const char * auth_password, const char * mail_attribs,
                    const char * cert_attribs, cert_struct * certs[])
{
#if 0
	int rtnval;
	LDAP * ld;

	char *filter, *tstr, *tmpstr, *tmpstr1;
	int addr_len, attr_count = 0, filter_length = 0;
	char mail_array[3][80];

	char **cert_attrib_array, **tmp_cert_array;

	LDAPMessage *reslt, *entry;
	int attrib_name_count, entry_count;
	void * cert;

	/* First some simple param checking */
	if (!server_name || !*server_name)
		return 1;
	if (!rcpt_address || !*rcpt_address)
		return 2;
	if (connect_type < CLEAR_ANON || connect_type > SSL_CERTAUTH)
		return 3;

	if (!cert_attribs || !*cert_attribs)
		return 2;

	/* Okay, try to init connection to LDAP Server */
	if (connect_type == CLEAR_ANON || connect_type == CLEAR_AUTH)
		ld = ldap_init(server_name, 389);
#if 0
	/* don't bother with SSL yet */
	else {		/* SSL Connection */
		/* First init Client connection to db files needed */
		if (connect_type == SSL_CERTAUTH) {
/* don't bother yet.  keydbpath is full path of key3.db file (e.g. ...users/bruces)
			if (ldapssl_clientauth_init(certdb_path, NULL, needkeydb, keydbpath, 
			                        NULL) < 0)
				return 4; */
		} else {
			if (ldapssl_client_init(certdb_path, NULL) < 0)
				return 4;
		}

		/* Now attempt SSL connection to LDAP Server */
		ld = ldapssl_init(server_name, port, 1);
	}
#endif
	if (!ld) 	/* failed to init connection to LDAP server */
		return 4;

	/* Now bind to server (NULL/Anon bind if not AUTH type connection) */
	if (connect_type == CLEAR_ANON || connect_type == SSL_ANON)
		rtnval = ldap_simple_bind_s(ld, NULL, NULL);
	else if (connect_type == CLEAR_AUTH || connect_type == SSL_AUTH)
		rtnval = ldap_simple_bind_s(ld, auth_dn, auth_password);
/*	else if (connect_type == SSL_CERTAUTH)
		rtnval = ldap_sasl_bind_s(ld, auth_dn, [mechanism], [cred], NULL, 
		                          NULL, servercredp) */
	if (rtnval != LDAP_SUCCESS)
		return 5;

	/*
	// Now ready to search
	*/

	/* First create the filter from attrib(s) and rcpt_address */

	/* first compute size of filter to create, and collect mail attribs */
	addr_len = strlen(rcpt_address);
	tmpstr1 = (char *)PR_Malloc(strlen(mail_attribs)+1);
	strcpy(tmpstr1, mail_attribs);
	tmpstr = tmpstr1;
	while (tstr = strchr(tmpstr, ',')) {
		*tstr = '\0';
		filter_length += addr_len + strlen(tmpstr) + 5;
		strcpy(mail_array[attr_count++], tmpstr);
		tmpstr = tstr+1;
	}
	/* and get last mail attribute */
	filter_length += addr_len + strlen(tmpstr) + 5;
	strcpy(mail_array[attr_count++], tmpstr);
	filter = (char *)PR_Malloc(filter_length);
	PR_Free(tmpstr1);

	/* should figure out way to generalize this :-( */
	if (attr_count == 1)
		sprintf(filter, "(%s=%s)", mail_array[0], rcpt_address);
	else if (attr_count == 2)
		sprintf(filter, "(|(%s=%s)(%s=%s))", mail_array[0], rcpt_address,
		        mail_array[1], rcpt_address);
	else if (attr_count == 3)
		sprintf(filter, "(|(%s=%s)(%s=%s)(%s=%s))", mail_array[0], rcpt_address,
		        mail_array[1], rcpt_address, mail_array[2], rcpt_address);
	else
		return 6;		/* too many mail attribs (should fix this eventually) */

	/* Also get list of Cert attribs */
	cert_attrib_array = (char **)PR_Malloc(50);	/* space for array of ptrs */
	tmp_cert_array = cert_attrib_array;
		
	tmpstr = (char *)PR_Malloc(strlen(cert_attribs)+1);
	strcpy(tmpstr, cert_attribs);
	tmpstr1 = tmpstr;
	while (tstr = strchr(tmpstr, ',')) {
		*tstr = '\0';
		*tmp_cert_array = (char *)PR_Malloc(strlen(tmpstr)+1);
		strcpy(*tmp_cert_array, tmpstr);
		*tmp_cert_array++;
		tmpstr = tstr+1;
	}
	/* get last attribute, and put in NULL entry as end */
	*tmp_cert_array = (char *)PR_Malloc(strlen(tmpstr)+1);
	strcpy(*tmp_cert_array, tmpstr);
	*tmp_cert_array++;
	*tmp_cert_array = NULL;
	PR_Free(tmpstr1);

	/* Now perform the search and check response */
	rtnval = ldap_search_s(ld, baseDN, LDAP_SCOPE_SUBTREE, filter,
	                       cert_attrib_array, SSM_LDAP_FALSE, &reslt);
	PR_Free(filter);
	if (rtnval != LDAP_SUCCESS) {
		for (tmp_cert_array = cert_attrib_array; *tmp_cert_array; 
		     tmp_cert_array++)
			PR_Free(*tmp_cert_array);
		PR_Free(cert_attrib_array);
		return 7;		/* LDAP Failure */
	}

	entry_count = ldap_count_entries(ld, reslt);
	if (entry_count == 0) {
		for (tmp_cert_array = cert_attrib_array; *tmp_cert_array; 
		     tmp_cert_array++)
			PR_Free(*tmp_cert_array);
		PR_Free(cert_attrib_array);
		ldap_msgfree(reslt);
		ldap_unbind(ld);
		return -1;		/* no entry found for rcpt */
	}
	if (entry_count > 1) {
		for (tmp_cert_array = cert_attrib_array; *tmp_cert_array; 
		     tmp_cert_array++)
			PR_Free(*tmp_cert_array);
		PR_Free(cert_attrib_array);
		ldap_msgfree(reslt);
		ldap_unbind(ld);
		return 8;		/* multiple entries found for rcpt */
	}

	/* 
	 *Okay, got an entry, check for Cert 
	 */

	rtnval = 9;		/* default is to return no attrib found */

	/* Now check each cert attribute type */
	entry = ldap_first_entry(ld, reslt);
	attrib_name_count = 0;
	tmp_cert_array = cert_attrib_array;
	while (*tmp_cert_array) {
		struct berval cert_attrib_struct;
		struct berval **cert_out_list;
		struct cert_struct_def * cert_ptr;
		int valcount=0;

		cert_out_list = ldap_get_values_len(ld, entry, *tmp_cert_array);
		if (cert_out_list) {	/* Values found for this cert attr */
			while (SSM_LDAP_TRUE) {		/* just get count for malloc first */
				if (!cert_out_list[valcount++])
					break;
			}
			certs[attrib_name_count] = PR_Malloc(valcount * sizeof(cert_struct));
			cert_ptr = certs[attrib_name_count];

			valcount =0;
			while (SSM_LDAP_TRUE) {
				if (!cert_out_list[valcount])
					break;
				cert_attrib_struct = *cert_out_list[valcount++];
				cert = PR_Malloc(cert_attrib_struct.bv_len);
				memcpy(cert, cert_attrib_struct.bv_val, 
				       cert_attrib_struct.bv_len);
				cert_ptr->cert = cert;
				cert_ptr->cert_len = cert_attrib_struct.bv_len;
				cert_ptr++;
				rtnval = 0;		/* found at least one Cert */
			}
			cert_ptr->cert_len = 0;	/* end list */
		}
		ldap_value_free_len(cert_out_list);
		*tmp_cert_array++;
		attrib_name_count++;
	}
	for (tmp_cert_array = cert_attrib_array; *tmp_cert_array; 
	     tmp_cert_array++)
		PR_Free(*tmp_cert_array);
	PR_Free(cert_attrib_array);
	ldap_msgfree(reslt);
	ldap_unbind(ld);
	return rtnval;
#else
	return 1;
#endif
}
