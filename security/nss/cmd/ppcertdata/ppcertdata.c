/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "secutil.h"
#include "nss.h"

unsigned char binary_line[64 * 1024];

int
main(int argc, const char** argv)
{
    int skip_count = 0;
    int bytes_read;
    char line[133];

    if (argc > 1) {
        skip_count = atoi(argv[1]);
    }
    if (argc > 2 || skip_count < 0) {
        printf("Usage: %s [ skip_columns ] \n", argv[0]);
        return 1;
    }

    NSS_NoDB_Init(NULL);

    while (fgets(line, 132, stdin) && (bytes_read = strlen(line)) > 0) {
        int bytes_written;
        char* found;
        char* in = line + skip_count;
        int left = bytes_read - skip_count;
        int is_cert;
        int is_serial;
        int is_name;
        int is_hash;
        int use_pp = 0;
        int out = 0;
        SECItem der = { siBuffer, NULL, 0 };

        line[bytes_read] = 0;
        if (bytes_read <= skip_count)
            continue;
        fwrite(in, 1, left, stdout);
        found = strstr(in, "MULTILINE_OCTAL");
        if (!found)
            continue;
        fflush(stdout);

        is_cert = (NULL != strstr(in, "CKA_VALUE"));
        is_serial = (NULL != strstr(in, "CKA_SERIAL_NUMBER"));
        is_name = (NULL != strstr(in, "CKA_ISSUER")) ||
                  (NULL != strstr(in, "CKA_SUBJECT"));
        is_hash = (NULL != strstr(in, "_HASH"));
        while (fgets(line, 132, stdin) &&
               (bytes_read = strlen(line)) > 0) {
            in = line + skip_count;
            left = bytes_read - skip_count;

            if ((left >= 3) && !strncmp(in, "END", 3))
                break;
            while (left >= 4) {
                if (in[0] == '\\' && isdigit(in[1]) &&
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
        der.len = out;
        if (is_cert)
            SECU_PrintSignedData(stdout, &der, "Certificate", 0,
                                 SECU_PrintCertificate);
        else if (is_name)
            SECU_PrintDERName(stdout, &der, "Name", 0);
        else if (is_serial) {
            if (out > 2 && binary_line[0] == 2 &&
                out == 2 + binary_line[1]) {
                der.data += 2;
                der.len -= 2;
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
