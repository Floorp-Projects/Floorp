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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
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
#include <stdlib.h>
#include <string.h>

int
main(int argc, char **argv)
{
    char *curstr;
    char *nextstr;
    unsigned int firstval;
    unsigned int secondval;
    unsigned int val;
    unsigned char buf[5];
    int count;
    
    if ( argc != 2 ) {
	fprintf(stderr, "wrong number of args\n");
	exit(-1);
    }
    
    curstr = argv[1];
    
    nextstr = strchr(curstr, '.');
    
    if ( nextstr == NULL ) {
	fprintf(stderr, "only one component\n");
	exit(-1);
    }
    
    *nextstr = '\0';
    firstval = atoi(curstr);

    curstr = nextstr + 1;
    
    nextstr = strchr(curstr, '.');

    if ( nextstr ) {
	*nextstr = '\0';
    }

    secondval = atoi(curstr);
    
    if ( ( firstval < 0 ) || ( firstval > 2 ) ) {
	fprintf(stderr, "first component out of range\n");
	exit(-1);
	
    }
    
    if ( ( secondval < 0 ) || ( secondval > 39 ) ) {
	fprintf(stderr, "second component out of range\n");
	exit(-1);
    }
    
    printf("0x%x, ", ( firstval * 40 ) + secondval );
    while ( nextstr ) {
	curstr = nextstr + 1;

	nextstr = strchr(curstr, '.');

	if ( nextstr ) {
	    *nextstr = '\0';
	}

	memset(buf, 0, sizeof(buf));
	val = atoi(curstr);
	count = 0;
	while ( val ) {
	    buf[count] = ( val & 0x7f );
	    val = val >> 7;
	    count++;
	}

	while ( count-- ) {
	    if ( count ) {
		printf("0x%x, ", buf[count] | 0x80 );
	    } else {
		printf("0x%x, ", buf[count] );
	    }
	}
    }
    printf("\n");
    return 0;
}

