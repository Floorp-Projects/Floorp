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

/**********************************************************************
 resources.c

 Turns a Netscape.ad file into a netscape.cfg file so we can do
 l10n for Dogbert.

 Usage:
		resources Netscape.ad
**********************************************************************/

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <sechash.h>


#define OUT_FILE_NAME "netscape.cfg"
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))


struct mapping {
    char* resource;
    char* pref_name;
    char* verb;
    char* value;
    char mnemonic;
};


struct mapping map[] = {

    /* Help Menu labels */
    {"*menuBar*manual", "menu.help.item_0.label"},
    {"*menuBar*relnotes", "menu.help.item_1.label"},
    {"*menuBar*productInfo", "menu.help.item_2.label"},
    {"*menuBar*upgrade", "menu.help.item_4.label"},
    {"*menuBar*registration", "menu.help.item_5.label"},
    {"*menuBar*feedback", "menu.help.item_6.label"},
    {"*menuBar*intl", "menu.help.item_8.label"},
    {"*menuBar*aboutSecurity", "menu.help.item_9.label"},
    {"*menuBar*aboutUsenet", "menu.help.item_10.label"},
    {"*menuBar*aboutplugins", "menu.help.item_12.label"},
    {"*menuBar*aboutfonts", "menu.help.item_13.label"},

    /* Help Menu URLs */
    {"*url*manual", "menu.help.item_0.url"},
    {"*url*relnotes", "menu.help.item_1.url"},
    {"*url*productInfo", "menu.help.item_2.url"},
    {"*url*feedback", "menu.help.item_4.url"},
    {"*url*registration", "menu.help.item_5.url"},
    {"*url*upgrade", "menu.help.item_6.url"},
    {"*url*intl", "menu.help.item_8.url"},
    {"*url*aboutSecurity", "menu.help.item_9.url"},
    {"*url*aboutUsenet", "menu.help.item_10.url"},
    {"*url*aboutplugins", "menu.help.item_12.url"},
    {"*url*aboutfonts", "menu.help.item_13.url"},

    /* Communicator/Bookmarks/Guide labels */
    {"*menuBar*inetIndex", "menu.places.item_0.label"},
    {"*menuBar*inetWhite", "menu.places.item_1.label"},
    {"*menuBar*inetYellow", "menu.places.item_2.label"},
    {"*menuBar*whatsNew", "menu.places.item_3.label"},
    {"*menuBar*whatsCool", "menu.places.item_4.label"},

    /* Communicator/Bookmarks/Guide URLs */
    {"*url.about", "menu.places.item_0.url"},
    {"*url.white", "menu.places.item_1.url"},
    {"*url.yellow", "menu.places.item_2.url"},
    {"*url.whats_new", "menu.places.item_3.url"},
    {"*url.whats_cool", "menu.places.item_4.url"},

    /* Toolbar labels */
    {"*toolBar*destinations*inetIndex", "toolbar.places.item_0.label"},
    {"*toolBar*destinations*inetWhite", "toolbar.places.item_1.label"},
    {"*toolBar*destinations*inetYellow", "toolbar.places.item_2.label"},
    {"*toolBar*destinations*whatsNew", "toolbar.places.item_3.label"},
    {"*toolBar*destinations*whatsCool", "toolbar.places.item_4.label"},

    /* Toolbar URLs */
    {"*url.about", "toolbar.places.item_0.url"},
    {"*url.white", "toolbar.places.item_1.url"},
    {"*url.yellow", "toolbar.places.item_2.url"},
    {"*url.whats_new", "toolbar.places.item_3.url"},
    {"*url.whats_cool", "toolbar.places.item_4.url"},

    /* i18n stuff */
    {"*url.netscape", "browser.startup.homepage", "pref"},
    {"*versionLocale", "intl.accept_languages", "pref"},

    /* Personal Toolbar labels */
    /* Personal Toolbar URLs */
};


char output_buf[10 * 1024];


/*
 * usage
 */
void
usage(char* prog_name)
{
    fprintf(stderr, "Usage: %s filename\n", prog_name);
}


/*
 * append_to_file
 */
void
append_to_file(char buf[])
{
    strcat(output_buf, buf);
}


/*
 * get_value
 */
char*
get_value(char buf[])
{
    char* ptr = strchr(buf, ':') + 1;
    while ( isspace(*ptr) ) ptr++;
    return ptr;
}


/*
 * is_label
 */
int
is_label(char buf[])
{
    return ( strstr(buf, ".labelString:") != NULL );
}


/*
 * is_mnemonic
 */
int
is_mnemonic(char buf[])
{
    return ( strstr(buf, ".mnemonic:") != NULL );
}


/*
 * is_url
 */
int
is_url(char buf[])
{
    return ( !strncmp(buf, "*url.", 5) );
}


/*
 * process_line
 */
void
process_line(char buf[])
{
    int i;

    for ( i = 0; i < COUNT(map); i++ ) {
        if ( !strncmp(map[i].resource, buf, strlen(map[i].resource)) ) {
            if ( is_label(buf) ) {
                map[i].value = strdup(get_value(buf));
            } else
            if ( is_mnemonic(buf) ) {
                map[i].mnemonic = *get_value(buf);
            } else {
                map[i].value = strdup(get_value(buf));
            }
        }
    }
}


/*
 * obscure
 */
static void
obscure(char* buf)
{
    int i;
    static int offset = 0;
    int len = buf ? strlen(buf) : 0;
 
    for ( i = 0; i < len; i++, offset++ ) {
        buf[i]+= 7;
    }
}


/*
 * write_hash
 */
static void
write_hash(FILE* file)
{
    char buf[256];
    unsigned char digest[16];
    unsigned char magic_key[] = "VonGloda5652TX75235ISBN";
    int len;
 
    MD5Context* md5_cxt = MD5_NewContext();
    MD5_Begin(md5_cxt);
 
    /* start with the magic key */
    MD5_Update(md5_cxt, magic_key, sizeof(magic_key));
 
    MD5_Update(md5_cxt, output_buf, strlen(output_buf));
 
    MD5_End(md5_cxt, digest, &len, 16);
 
    MD5_DestroyContext(md5_cxt, PR_TRUE);
    sprintf(buf, "// %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
            digest[0], digest[1], digest[2], digest[3],
            digest[4], digest[5], digest[6], digest[7],
            digest[8], digest[9], digest[10], digest[11],
            digest[12], digest[13], digest[14], digest[15]);
 
    obscure(buf);
 
    if ( fwrite(buf, sizeof(char), strlen(buf), file) != strlen(buf) ) {
        perror("fwrite");
        exit(errno);
    }
}


/*
 * output_file
 */
void
output_file(FILE* out)
{
    int i, j, k;
    int len;
    char buf[256];

    append_to_file("with ( PrefConfig ) {\n");

    for ( i = 0; i < COUNT(map); i++ ) {
        if ( map[i].value ) {
            if ( !is_url(map[i].resource) ) {
                if ( strchr(map[i].value, '&') ) {
                    for ( j = 0, k = 0; j < strlen(map[i].value); j++, k++ ) { 
                        buf[k] = map[i].value[j];
                        if ( buf[k] == '&' ) buf[++k] = '&';
                    }
                    free(map[i].value);
                    buf[k] = '\0';
                    map[i].value = strdup(buf);
                }
                if ( map[i].mnemonic ) {
                    for ( j = 0, k = 0; j < strlen(map[i].value); j++, k++ ) { 
                        buf[k] = map[i].value[j];
                        if ( buf[k] == map[i].mnemonic ) {
                            buf[k++] = '&';
                            buf[k] = map[i].mnemonic;
                            map[i].mnemonic = '\0';
                        }
                    }
                    free(map[i].value);
                    buf[k] = '\0';
                    map[i].value = strdup(buf);
                }
            }
            sprintf(buf, "%s(\"%s\", \"%s\");\n", 
					map[i].verb ? map[i].verb : "config", 
					map[i].pref_name, 
					map[i].value);
            append_to_file(buf);
        }
    }

    append_to_file("}\n");
    write_hash(out);
    len = strlen(output_buf);
    obscure(output_buf);
    fwrite(output_buf, sizeof(char), len, out);
}


/*
 * fatal
 */
void
fatal(char* progname, int error)
{
	fprintf(stderr, "%s failed\n", progname);
	exit(error);
}


/*
 * main
 */
int
main(int argc, char* argv[])
{
    FILE* in;
    FILE* out;
    char buf[64 * 1024];
	int line = 0;

    if ( argc != 2 ) {
        usage(argv[0]);
        fatal(argv[0], 1);
    }

    if ( (in = fopen(argv[1], "r")) == NULL ) {
        perror(argv[1]);
        fatal(argv[0], errno);
    }

    if ( (out = fopen(OUT_FILE_NAME, "w")) == NULL ) {
        perror(OUT_FILE_NAME);
        fatal(argv[0], errno);
    }

    while ( fgets(buf, sizeof(buf), in) != NULL ) {
		line++;
		if ( strchr(buf, '\n') == NULL ) {
			fprintf(stderr, "%s, line %d: Line too long\n", argv[1], line);
			fatal(argv[0], 1);
		}
        *(char*) strchr(buf, '\n') = '\0';
        process_line(buf);
    }

    output_file(out);

    if ( fclose(in) != 0 ) {
        perror(argv[1]);
        fatal(argv[0], errno);
    }

    if ( fclose(out) != 0 ) {
        perror(OUT_FILE_NAME);
        fatal(argv[0], errno);
    }

	fprintf(stderr, "Created " OUT_FILE_NAME "\n");

    return 0;
}


