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
typedef struct IDEElement IDEElement;
typedef struct parentElement parentElement;
IDEElement *create_IDEElement(XPTInterfaceDirectoryEntry *ide);
void add_IDEElement(IDEElement *e);
int compare_IIDs(nsID *one, nsID *two);
parentElement *create_parentElement(XPTInterfaceDirectoryEntry *ide, nsID *iid);
void add_parentElement(parentElement *p);
static void xpt_dump_usage(char *argv[]); 

/* Linker-specific structs. */
struct IDEElement {
    struct IDEElement *IDE_next;
    XPTInterfaceDirectoryEntry  *interface_directory;
};

struct parentElement {
    struct parentElement *parent_next;
    XPTInterfaceDirectoryEntry *child;
    nsID *parent_iid;
};

/* Global variables. */
static IDEElement *first_ide = NULL;
static parentElement *first_parent = NULL;
int numberOfInterfaces = 0;

int 
main(int argc, char **argv)
{
    XPTState *state;
    XPTCursor curs, *cursor = &curs;
    XPTHeader *header;
    XPTInterfaceDirectoryEntry *newIDE;
    XPTAnnotation *ann;
    IDEElement *current_ide;
    parentElement *current_parent;
    uint32 header_sz, len;
    struct stat file_stat;
    size_t flen = 0;
    char *head, *data, *whole;
    FILE *in, *out;
    int i,j;

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
                fprintf(stdout, "MakeCursor failed\n");
                return 1;
            }
            if (!XPT_DoHeader(cursor, &header)) {
                fprintf(stdout, "DoHeader failed\n");
                return 1;
            }                                        
            
            for (j=0; j<header->num_interfaces; j++) {
                newIDE= malloc(sizeof(XPTInterfaceDirectoryEntry));
                newIDE->iid = header->interface_directory[j].iid;
                newIDE->name = header->interface_directory[j].name;
                newIDE->name_space = 
                    header->interface_directory[j].name_space;
                newIDE->interface_descriptor = 
                    header->interface_directory[j].interface_descriptor;
                    
                add_IDEElement(create_IDEElement(newIDE));
            }
            
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
    
    current_parent = first_parent;
    while (current_parent) {
        current_ide = first_ide;
        while (current_ide) {
            if (compare_IIDs(&current_ide->interface_directory->iid, 
                             current_parent->parent_iid) == 0) {
                current_parent->child->interface_descriptor->parent_interface = current_ide->interface_directory;
                break;
            }
            current_ide = current_ide->IDE_next;
            if (current_ide == NULL) {
                current_parent->child->interface_descriptor->parent_interface = NULL;
            }
        }
        current_parent = current_parent->parent_next;
    }

    current_ide = first_ide;
    for (i=0; i<numberOfInterfaces; i++) {
        if (!current_ide) {
            header->num_interfaces = i;
            break;
        }
        header->interface_directory[i].iid = 
            current_ide->interface_directory->iid;
        header->interface_directory[i].name = 
            current_ide->interface_directory->name;
        header->interface_directory[i].name_space = 
            current_ide->interface_directory->name_space;
        header->interface_directory[i].interface_descriptor = 
            current_ide->interface_directory->interface_descriptor;
        current_ide = current_ide->IDE_next;
    }

    for (current_parent = first_parent; 
         current_parent != NULL; 
         current_parent = current_parent->parent_next) {
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

IDEElement *
create_IDEElement(XPTInterfaceDirectoryEntry *ide)
{
    IDEElement *ide_element;
    ide_element = (IDEElement *)malloc(sizeof(IDEElement));
    if (ide_element == NULL) {
        perror("FAILED: create_IDEElement malloc");
        return NULL;
    }
    ide_element->interface_directory = ide;
    ide_element->IDE_next = NULL;
    return ide_element;
}

void 
add_IDEElement(IDEElement *e)
{
    PRBool placed = PR_FALSE;
    nsID *one = &e->interface_directory->iid;    
    int comparison;
    IDEElement *current, *last;
    XPTInterfaceDescriptor *id;
    
    if (first_ide == NULL) {
        first_ide = e;
        numberOfInterfaces++;
        return;
    }

    last = NULL;
    current = first_ide; 
    while (!placed) {
        nsID *two = &current->interface_directory->iid;
        comparison = compare_IIDs(one, two);
        if (comparison > 0) {
            if (current->IDE_next == NULL) {
                current->IDE_next = e;
                numberOfInterfaces++;
                placed = PR_TRUE;
            } else {
                last = current;
                current = current->IDE_next;
            }
        } else {
            if (comparison < 0) {
                if (last) {
                    last->IDE_next = e;
                } else {
                    first_ide = e;
                }
                e->IDE_next = current;
                numberOfInterfaces++;
                placed = PR_TRUE;
            } else { 
                placed = PR_TRUE;
            }
        }
    }
    id = e->interface_directory->interface_descriptor;
    if (id->parent_interface) {
        add_parentElement(create_parentElement(e->interface_directory, 
                                               &id->parent_interface->iid));
    }
    return;
}

parentElement *
create_parentElement(XPTInterfaceDirectoryEntry *new_child, nsID *new_iid)
{
    parentElement *parent_element;
    parent_element = (parentElement *)malloc(sizeof(parentElement));
    if (!parent_element) {
        perror("FAILED: create_parentElement malloc");
        return NULL;
    }
    parent_element->child = new_child;
    parent_element->parent_iid = new_iid;
    parent_element->parent_next = NULL;
    return parent_element;
}

void 
add_parentElement(parentElement *e)
{
    parentElement *p;
    
    if (first_parent == NULL) {
        first_parent = e;
        return;
    }
    
    for (p=first_parent; p->parent_next != NULL; p = p->parent_next)
        ;
    p->parent_next = e;
    
    return;
}

int compare_IIDs(nsID *one, nsID *two) {
    int i;
    
    if (one->m0 > two->m0) {
        return 1;
    } else {
        if (one->m0 < two->m0) {
            return -1;
        } else {
            if (one->m1 > two->m1) {
                return 1;
            } else {
                if (one->m1 < two->m1) {
                    return -1;
                } else {
                    if (one->m2 > two->m2) {
                        return 1;
                    } else {
                        if (one->m2 < two->m2) {
                            return -1;
                        } else {
                            for (i=0; i<8; i++) {
                                if (one->m3[i] > two->m3[i]) {
                                    return 1;
                                } else {
                                    if (one->m3[i] < two->m3[i]) {
                                        return -1;
                                    }
                                }
                            }
                            return 0;
                        }
                    }
                }
            }
        }
    }
}

static void
xpt_dump_usage(char *argv[]) 
{
    fprintf(stdout, "Usage: %s outfile file1.xpt file2.xpt ...\n"
            "       Links multiple typelib files into one outfile\n", argv[0]);
}

