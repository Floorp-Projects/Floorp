/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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

/*
 * A utility for linking multiple typelib files. 
 */ 

#include "xpt_xdr.h"
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include "prlong.h"

#ifndef NULL
#define NULL (void *) 0
#endif

/* Forward declarations. */
int compare_IDEs_by_IID(XPTInterfaceDirectoryEntry *ide1,
                        XPTInterfaceDirectoryEntry *ide2);
int compare_IDEs_by_name(XPTInterfaceDirectoryEntry *ide1,
                        XPTInterfaceDirectoryEntry *ide2);
static int compare_IIDs(const void *ap, const void *bp);
PRBool copy_IDE(XPTInterfaceDirectoryEntry *from, 
                XPTInterfaceDirectoryEntry *to);
static void xpt_dump_usage(char *argv[]); 

/* Global variables. */
int numberOfInterfaces = 0;

int 
main(int argc, char **argv)
{
    XPTState *state;
    XPTCursor curs, *cursor = &curs;
    XPTHeader *header;
    XPTInterfaceDirectoryEntry *newIDE, *IDE_array;
    XPTAnnotation *ann;
    uint32 header_sz, len;
    struct stat file_stat;
    size_t flen = 0;
    char *head, *data, *whole;
    FILE *in, *out;
    int i,j;
    int k = 0;

    if (argc < 3) {
        xpt_dump_usage(argv);
        return 1;
    }
        
    for (i=2; i<argc; i++) {
        if (stat(argv[i], &file_stat) != 0) {
            perror("FAILED: fstat");
            return 1;
        }
        flen = file_stat.st_size;

        in = fopen(argv[i], "rb");
        if (!in) {
            perror("FAILED: fopen");
            return 1;
        }

        whole = malloc(flen);
        if (!whole) {
            perror("FAILED: malloc for whole");
            return 1;
        }
        
        if (flen > 0) {
            size_t rv = fread(whole, 1, flen, in);
            if (rv < 0) {
                perror("FAILED: reading typelib file");
                return 1;
            }
            if (rv < flen) {
                fprintf(stderr, "short read (%d vs %d)! ouch!\n", rv, flen);
                return 1;
            }
            if (ferror(in) != 0 || fclose(in) != 0) {
                perror("FAILED: Unable to read typelib file.\n");
                return 1;
            }
            
            state = XPT_NewXDRState(XPT_DECODE, whole, flen);
            if (!XPT_MakeCursor(state, XPT_HEADER, 0, cursor)) {
                perror("FAILED: XPT_MakeCursor");
                return 1;
            }
            if (!XPT_DoHeader(cursor, &header)) {
                perror("FAILED: XPT_DoHeader");
                return 1;
            }                                        
            
            numberOfInterfaces += header->num_interfaces;
            if (k == 0) {
                IDE_array = PR_CALLOC(numberOfInterfaces * sizeof(XPTInterfaceDirectoryEntry));
            } else {
                newIDE = PR_REALLOC(IDE_array, numberOfInterfaces * sizeof(XPTInterfaceDirectoryEntry));
                if (!newIDE) {
                    perror("FAILED: PR_REALLOC of IDE_array");
                    return 1;
                }
                IDE_array = newIDE;
            }
            
            for (j=0; j<header->num_interfaces; j++) {
                if (!copy_IDE(&header->interface_directory[j], 
                              &IDE_array[k])) {
                    perror("FAILED: 1st copying of IDE");
                    return 1;
                }
                k++;
            }
            
            PR_FREEIF(header)
            if (state)
                XPT_DestroyXDRState(state);
            free(whole);
            flen = 0;            

        } else {
            fclose(in);
            perror("FAILED: file length <= 0");
            return 1;
        }
    }
 
    header = XPT_NewHeader(numberOfInterfaces);
    ann = XPT_NewAnnotation(XPT_ANN_LAST, NULL, NULL);
    header->annotations = ann;
    for (i=0; i<numberOfInterfaces; i++) {
        if (!copy_IDE(&IDE_array[i], &header->interface_directory[i])) {
            perror("FAILED: 2nd copying of IDE");
            return 1;
        }
    }
    
    header_sz = XPT_SizeOfHeaderBlock(header); 

    state = XPT_NewXDRState(XPT_ENCODE, NULL, 0);
    if (!state) {
        perror("FAILED: error creating XDRState");
        return 1;
    }
    
    if (!XPT_MakeCursor(state, XPT_HEADER, header_sz, cursor)) {
        perror("FAILED: error making cursor");
        return 1;
    }
    
    if (!XPT_DoHeader(cursor, &header)) {
        perror("FAILED: error doing Header");
        return 1;
    }
    
    out = fopen(argv[1], "wb");
    if (!out) {
        perror("FAILED: fopen");
        return 1;
    }
 
    XPT_GetXDRData(state, XPT_HEADER, &head, &len);
    fwrite(head, len, 1, out);
 
    XPT_GetXDRData(state, XPT_DATA, &data, &len);
    fwrite(data, len, 1, out);
 
    if (ferror(out) != 0 || fclose(out) != 0) {
        fprintf(stderr, "\nError writing file: %s\n\n", argv[1]);
    } else {
        fprintf(stderr, "\nFile written: %s\n\n", argv[1]);
    }
 
    if (state)
        XPT_DestroyXDRState(state);
    
    return 0;        
}

int 
compare_IDEs_by_IID(XPTInterfaceDirectoryEntry *ide1,
                    XPTInterfaceDirectoryEntry *ide2)
{
    nsID *one = &ide1->iid;
    nsID *two = &ide2->iid; 
    
    return compare_IIDs(one, two);
}  

int 
compare_IDEs_by_name(XPTInterfaceDirectoryEntry *ide1,
                    XPTInterfaceDirectoryEntry *ide2)
{
    return strcmp(ide1->name, ide2->name);
}
    
static int
compare_IIDs(const void *ap, const void *bp)
{
    const nsID *a = ap, *b = bp;
    int i;
#define COMPARE(field) if (a->field > b->field) return 1; \
                       if (b->field > a->field) return -1;
    COMPARE(m0);
    COMPARE(m1);
    COMPARE(m2);
    for (i = 0; i < 8; i++) {
        COMPARE(m3[i]);
    }
    return 0;
#undef COMPARE
}

PRBool
copy_IDE(XPTInterfaceDirectoryEntry *from, XPTInterfaceDirectoryEntry *to) 
{
    if (from == NULL || to == NULL) {
        return PR_FALSE;
    }
    
    to->iid = from->iid;
    to->name = from->name;
    to->name_space = from->name_space;
    to->interface_descriptor = from->interface_descriptor;
    return PR_TRUE;
}

static void
xpt_dump_usage(char *argv[]) 
{
    fprintf(stdout, "Usage: %s outfile file1.xpt file2.xpt ...\n"
            "       Links multiple typelib files into one outfile\n", argv[0]);
}

