/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is the CertData.txt review helper program.
 *
 * The Initial Developer of the Original Code is
 * Nelson Bolyard
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "secutil.h"
#include "nss.h"

unsigned char  binary_line[64 * 1024];

int
main(int argc, const char ** argv)
{
    int            skip_count = 0;
    int            bytes_read;
    char           line[133];

    if (argc > 1) {
    	skip_count = atoi(argv[1]);
    }
    if (argc > 2 || skip_count < 0) {
        printf("Usage: %s [ skip_columns ] \n", argv[0]);
	return 1;
    }

    NSS_NoDB_Init(NULL);

    while (fgets(line, 132, stdin) && (bytes_read = strlen(line)) > 0 ) {
	int    bytes_written;
	char * found;
	char * in          = line       + skip_count; 
	int    left        = bytes_read - skip_count;
	int    is_cert;
	int    is_serial;
	int    is_name;
	int    is_hash;
	int    use_pp      = 0;
	int    out = 0;
	SECItem der = {siBuffer, NULL, 0 };

	line[bytes_read] = 0;
	if (bytes_read <= skip_count) 
	    continue;
	fwrite(in, 1, left, stdout);
	found = strstr(in, "MULTILINE_OCTAL");
	if (!found) 
	    continue;
	fflush(stdout);

	is_cert   = (NULL != strstr(in, "CKA_VALUE"));
	is_serial = (NULL != strstr(in, "CKA_SERIAL_NUMBER"));
	is_name   = (NULL != strstr(in, "CKA_ISSUER")) ||
		    (NULL != strstr(in, "CKA_SUBJECT"));
	is_hash   = (NULL != strstr(in, "_HASH"));
	while (fgets(line, 132, stdin) && 
	       (bytes_read = strlen(line)) > 0 ) {
	    in   = line       + skip_count; 
	    left = bytes_read - skip_count;

	    if ((left >= 3) && !strncmp(in, "END", 3))
		break;
	    while (left >= 4) {
		if (in[0] == '\\'  && isdigit(in[1]) && 
		    isdigit(in[2]) && isdigit(in[3])) {
		    left -= 4;
		    binary_line[out++] = ((in[1] - '0') << 6) |
					 ((in[2] - '0') << 3) | 
					  (in[3] - '0');
		    in += 4;
		} else 
		    break;
	    }
	}
	der.data = binary_line;
	der.len  = out;
	if (is_cert)
	    SECU_PrintSignedData(stdout, &der, "Certificate", 0,
				 SECU_PrintCertificate);
	else if (is_name)
	    SECU_PrintDERName(stdout, &der, "Name", 0);
	else if (is_serial) {
	    if (out > 2 && binary_line[0] == 2 &&
	        out == 2 + binary_line[1]) {
		der.data += 2;
		der.len  -= 2;
		SECU_PrintInteger(stdout, &der, "DER Serial Number", 0);
	    } else
		SECU_PrintInteger(stdout, &der, "Raw Serial Number", 0);
	} else if (is_hash) 
	    SECU_PrintAsHex(stdout, &der, "Hash", 0);
	else 
	    SECU_PrintBuf(stdout, "Other", binary_line, out);
    }
    NSS_Shutdown();
    return 0;
}

