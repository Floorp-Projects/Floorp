/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

