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
typedef struct fixElement fixElement;
static int compare_IDEs_by_IID(const void *ap, const void *bp);
static int compare_IDEs_by_name(const void *ap, const void *bp);
static int compare_IDEs_by_name_space(const void *ap, const void *bp);
static int compare_fixElements_by_name(const void *ap, const void *bp);
static int compare_IIDs(const void *ap, const void *bp);
PRBool shrink_IDE_array(XPTInterfaceDirectoryEntry *ide, 
                        int element_to_delete, int num_interfaces);
PRBool update_fix_array(fixElement *deleted, int new_index);
static int get_new_index(const fixElement *fix, int num_elements,
                         int target_file, int target_interface);
PRBool copy_IDE(XPTInterfaceDirectoryEntry *from, 
                XPTInterfaceDirectoryEntry *to);
static void xpt_dump_usage(char *argv[]); 

struct fixElement {
    nsID *iid;
    char* name;
    int file_num;
    int interface_num;
    PRBool is_deleted;
    int index;
};

/* Global variables. */
int trueNumberOfInterfaces = 0;
int totalNumberOfInterfaces = 0;

int 
main(int argc, char **argv)
{
    XPTState *state;
    XPTCursor curs, *cursor = &curs;
    XPTHeader *header;
    XPTInterfaceDirectoryEntry *newIDE, *IDE_array;
    XPTInterfaceDescriptor *id;
    XPTTypeDescriptor *td;
    XPTAnnotation *ann;
    uint32 header_sz, len;
    struct stat file_stat;
    size_t flen = 0;
    char *head, *data, *whole;
    FILE *in, *out;
    fixElement *newFix, *fix_array;
    int i,j,l;
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
            
            totalNumberOfInterfaces += header->num_interfaces;
            if (k == 0) {
                IDE_array = PR_CALLOC(totalNumberOfInterfaces * sizeof(XPTInterfaceDirectoryEntry));
                fix_array = PR_CALLOC(totalNumberOfInterfaces * sizeof(fixElement));
            } else {
                newIDE = PR_REALLOC(IDE_array, totalNumberOfInterfaces * sizeof(XPTInterfaceDirectoryEntry));
                newFix = PR_REALLOC(fix_array, totalNumberOfInterfaces * sizeof(fixElement));

                if (!newIDE) {
                    perror("FAILED: PR_REALLOC of IDE_array");
                    return 1;
                }
                if (!newFix) {
                    perror("FAILED: PR_REALLOC of newFix");
                    return 1;
                }
                IDE_array = newIDE;
                fix_array = newFix;
            }
            
            for (j=0; j<header->num_interfaces; j++) {
                if (!copy_IDE(&header->interface_directory[j], 
                              &IDE_array[k])) {
                    perror("FAILED: 1st copying of IDE");
                    return 1;
                }
                fix_array[k].iid = &IDE_array[k].iid;
                fix_array[k].name = IDE_array[k].name;
                fix_array[k].file_num = i-2;
                fix_array[k].interface_num = j+1;
                fix_array[k].is_deleted = PR_FALSE;

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

    /* Sort both IDE_array and fix_array by name so we can check for 
     * name_space::name collisions. 
     */
    qsort(IDE_array, 
          totalNumberOfInterfaces, 
          sizeof(XPTInterfaceDirectoryEntry), 
          compare_IDEs_by_name);

    qsort(fix_array, 
          totalNumberOfInterfaces, 
          sizeof(fixElement), 
          compare_fixElements_by_name);

    /* Iterate through fix_array now that it is in its final form (sorted)
     * and set the index. Index is i+1 because index 0 is reserved for
     * the special case (no parent interface).
     */
    for (i=0; i<totalNumberOfInterfaces; i++) {
        fix_array[k].index = i+1;
    }

    /* trueNumberOfInterfaces == number of interfaces left after deletions
     * are made. Initialize it here to be the same as the total number of
     * interfaces we'ce encountered thus far.
     */
    trueNumberOfInterfaces = totalNumberOfInterfaces;

    /* Iterate through the sorted interfaces. Start at one so we don't 
     * accidentally walk off the end of the array.
     */
    i = 1;
    while (i != trueNumberOfInterfaces) {
        
        /* Check for name_space::name collision. 
         */
        if (compare_IDEs_by_name(&IDE_array[i-1], &IDE_array[i]) == 0 && 
            compare_IDEs_by_name_space(&IDE_array[i-1], &IDE_array[i]) == 0) {
            
            /* If one of the interfaces is unresolved, delete that one 
             * preferentailly.
             */
            if (!&IDE_array[i-1].iid) {
                /* Shrink the IDE_array to delete the duplicate interface.
                 */
                if (!shrink_IDE_array(IDE_array, 
                                      i-1, 
                                      trueNumberOfInterfaces)) {
                    perror("FAILED: shrink_IDE_array");
                    return 1;
                }
                /* Update the fix_array so that the index that corresponds
                 * to the just-deleted IDE now points to the surviving IDE
                 * of the same name. Indices are one-based, so the second 
                 * argument is passed as i+1 (instead of i).
                 */
                update_fix_array(&fix_array[i-1], i+1);
                /* Decrement the true number of interfaces since we just
                 * deleted one. There's more than one way to get out of
                 * this loop.
                 */
                trueNumberOfInterfaces--;
            } else {
                if (!&IDE_array[i].iid || 
                    (compare_IIDs(&IDE_array[i-1].iid, &IDE_array[i].iid) == 0)) {
                    /* Shrink the IDE_array to delete the duplicate interface.
                     */
                    if (!shrink_IDE_array(IDE_array, 
                                          i, 
                                          trueNumberOfInterfaces)) {
                        perror("FAILED: shrink_IDE_array");
                        return 1;
                    }
                    /* Update the fix_array so that the index that corresponds
                     * to the just-deleted IDE now points to the surviving IDE
                     * of the same name. Indices are one-based, so the second 
                     * argument is passed as i (instead of i-1).
                     */
                    update_fix_array(&fix_array[i], i);
                    /* Decrement the true number of interfaces since we just
                     * deleted one. There's more than one way to get out of
                     * this loop.
                     */
                    trueNumberOfInterfaces--;
                }
            }
            
        } else {
            /* Only increment if there was no name_space::name collision.
             */
            i++;
        }
    }

    /* Iterate through the remaining interfaces. There's some magic here - 
     * we walk fix_array at the same time skipping over ones that have 
     * been deleted. This should work correctly since we only ever delete
     * things in a linear manner.
     */
    for (i=0,j=0; 
         i<trueNumberOfInterfaces && j<totalNumberOfInterfaces; 
         i++,j++) {
        
        /* Define id to save some keystrokes.
         */
        id = IDE_array[i].interface_descriptor;
        /* Skip ahead in fix_array until past the deleted elements.
         */
        while (fix_array[j].is_deleted) {
            j++;
        }
        /* Fix parent_interface first.
         */
        if (id->parent_interface != 0) {
            id->parent_interface = 
                get_new_index(fix_array, totalNumberOfInterfaces,
                              fix_array[j].file_num, id->parent_interface);
        }
        /* Iterate through the method descriptors looking for params of
         * type TD_INTERFACE_TYPE.
         */
        for (k=0; k<id->num_methods; k++) {
            /* Cycle through the params first.
             */
            for (l=0; l<id->method_descriptors[k].num_args; l++) {
                /* Define td to save some keystrokes.
                 */
                td = &id->method_descriptors[k].params[l].type;
                if (XPT_TDP_TAG(td->prefix) == TD_INTERFACE_TYPE) {
                    td->type.interface = 
                        get_new_index(fix_array, 
                                      totalNumberOfInterfaces,
                                      fix_array[j].file_num, 
                                      td->type.interface);
                }                                                
            }

            /* Check the result param too. Define td again to save 
             * some keystrokes.
             */
            td = &id->method_descriptors[k].result->type;
            if (XPT_TDP_TAG(td->prefix) == TD_INTERFACE_TYPE) {
                td->type.interface = 
                    get_new_index(fix_array, totalNumberOfInterfaces,
                                  fix_array[j].file_num, td->type.interface);
            }                
        }
    } 

    /* Sort the IDE_array by IID for output to xpt file.
     */
    qsort(IDE_array, 
          trueNumberOfInterfaces, 
          sizeof(XPTInterfaceDirectoryEntry), 
          compare_IDEs_by_IID);

    /* Iterate through the array quickly looking for duplicate IIDS.
     * This shouldn't happen, i.e. is a failure condition, so bail
     * if we find a duplicate. If we have more than one entry, start 
     * at one so we don't accidentally grep the ether.
     */
    if (trueNumberOfInterfaces>1) {
        for (i=1; i<trueNumberOfInterfaces; i++) {
            if (compare_IDEs_by_IID(&IDE_array[i-1], &IDE_array[i]) == 0) {
                perror("Error: Whoa, Nellie! You've got duplicate IIDs.");
                return 1;
            }
        }
    }

    header = XPT_NewHeader(trueNumberOfInterfaces);
    ann = XPT_NewAnnotation(XPT_ANN_LAST, NULL, NULL);
    header->annotations = ann;
    for (i=0; i<trueNumberOfInterfaces; i++) {
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

static int 
compare_IDEs_by_IID(const void *ap,
                    const void *bp)
{
    const XPTInterfaceDirectoryEntry *ide1 = ap, *ide2 = bp;
    
    return compare_IIDs(&ide1->iid, &ide2->iid);
}  

static int 
compare_IDEs_by_name(const void *ap,
                    const void *bp)
{
    const XPTInterfaceDirectoryEntry *ide1 = ap, *ide2 = bp;
    
    return strcmp(ide1->name, ide2->name);
}

static int 
compare_IDEs_by_name_space(const void *ap,
                           const void *bp)
{
    const XPTInterfaceDirectoryEntry *ide1 = ap, *ide2 = bp;
    
    return strcmp(ide1->name_space, ide2->name_space);
}

static int 
compare_fixElements_by_name(const void *ap,
                    const void *bp)
{
    const fixElement *fix1 = ap, *fix2 = bp;
    
    return strcmp(fix1->name, fix2->name);
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
shrink_IDE_array(XPTInterfaceDirectoryEntry *ide, int element_to_delete,
                    int num_interfaces)
{
    int i;

    if (element_to_delete >= num_interfaces) {
        return PR_FALSE;
    }

    for (i=element_to_delete+1; i<num_interfaces; i++) {
        if (!copy_IDE(&ide[i], &ide[i-1])) {
            return PR_FALSE;
        }
    }

    return PR_TRUE;
}

/* update_fix_array marks a fixElement as deleted and updates the index 
 *  marker. Deleted elements are not used for comparison purposes with 
 * IDE_array. There only purpose is to set the interface index (if 
 * necessary) for id->parent_interfaces and TypeDescriptors once the IDEs 
 * are sorted and pruned. 
 */
PRBool 
update_fix_array(fixElement *deleted, int new_index) 
{
    deleted->is_deleted = PR_TRUE;
    deleted->index = new_index;
    
    return PR_TRUE;
}

/* get_new_index returns the new interface index by walking the fix_array
 * until the file_num and interface_num match the target values. Indices 
 * are one-based; zero represents the special case (no parent interface). 
 */
static int 
get_new_index(const fixElement *fix, int num_elements, 
              int target_file, int target_interface) 
{
    int i;
    
    for (i=0; i<num_elements; i++) { 
        if (fix[i].file_num == target_file &&
            fix[i].interface_num == target_interface)
            return fix[i].index;
    }
    
    return 0;
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

