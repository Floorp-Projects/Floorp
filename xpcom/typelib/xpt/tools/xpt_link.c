/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * A utility for linking multiple typelib files. 
 */ 

#include "xpt_xdr.h"
#include <stdio.h>
#ifdef XP_MAC
#include <stat.h>
#else
#include <sys/stat.h>
#endif
#include <stdlib.h>
#include <string.h>
#include "prlong.h"

#ifndef NULL
#define NULL (void *) 0
#endif

/* Forward declarations. */
typedef struct fixElement fixElement;
static int compare_IDEs_by_IID(const void *ap, const void *bp);
static int compare_IDE_with_zero(const void *ap);
static int compare_IDEs_by_name(const void *ap, const void *bp);
static int compare_IDEs_by_name_space(const void *ap, const void *bp);
static int compare_strings(const void *ap, const void *bp);
static int compare_pointers(const void *ap, const void *bp);
static int compare_fixElements_by_IID(const void *ap, const void *bp);
static int compare_fixElements_by_name(const void *ap, const void *bp);
static int compare_IIDs(const void *ap, const void *bp);
PRBool shrink_IDE_array(XPTInterfaceDirectoryEntry *ide, 
                        int element_to_delete, int num_interfaces);
PRBool update_fix_array(XPTArena *arena, fixElement *fix, int element_to_delete, 
                        int num_interfaces, int replacement); 
static int get_new_index(const fixElement *fix, int num_elements,
                         int target_file, int target_interface);
PRBool copy_IDE(XPTInterfaceDirectoryEntry *from, 
                XPTInterfaceDirectoryEntry *to);
PRBool copy_fixElement(fixElement *from, fixElement *to);
static void print_IID(struct nsID *iid, FILE *file);
static void xpt_link_usage(char *argv[]);

struct fixElement {
    nsID iid;
    char* name;
    int file_num;
    int interface_num;
    PRBool is_deleted;
    /* These next two variables will point you to the substitute interface
     * if this one has been deleted.
     */
    int maps_to_file_num;
    int maps_to_interface_num;
};

/* Global variables. */
PRUint16 trueNumberOfInterfaces = 0;
PRUint16 totalNumberOfInterfaces = 0;
PRUint16 oldTotalNumberOfInterfaces = 0;

#if defined(XP_MAC) && defined(XPIDL_PLUGIN)

#define main xptlink_main
int xptlink_main(int argc, char *argv[]);
extern size_t mac_get_file_length(const char* filename);
#define get_file_length mac_get_file_length

#else

static size_t get_file_length(const char* filename)
{
    struct stat file_stat;
    if (stat(filename, &file_stat) != 0) {
        perror("FAILED: get_file_length");
        exit(1);
    }
    return file_stat.st_size;
}

#endif /* XP_MAC && XPIDL_PLUGIN */

int 
main(int argc, char **argv)
{
    XPTArena *arena;
    XPTState *state;
    XPTCursor curs, *cursor = &curs;
    XPTHeader *header;
    XPTInterfaceDirectoryEntry *IDE_array = NULL;
    XPTInterfaceDescriptor *id;
    XPTTypeDescriptor *td;
    XPTAnnotation *ann, *first_ann;
    PRUint32 header_sz, len;
    PRUint32 oldOffset;
    PRUint32 newOffset;
    size_t flen = 0;
    char *head, *data, *whole;
    FILE *in, *out;
    fixElement *fix_array = NULL;
    int i,j;
    int k = 0;

    if (argc < 3) {
        xpt_link_usage(argv);
        return 1;
    }
        
    arena = XPT_NewArena(1024 * 10, sizeof(double), "main xpt_link arena");
    if (!arena) {
        perror("FAILED: XPT_NewArena");
        return 1;
    }

    first_ann = XPT_NewAnnotation(arena, XPT_ANN_LAST, NULL, NULL);

    for (i=2; i<argc; i++) {
        char *name = argv[i];

        flen = get_file_length(name);
        if (!flen) {
            fprintf(stderr, "ERROR: file %s is zero length\n", name);
            return 1;
        }

        in = fopen(name, "rb");
        if (!in) {
            perror("FAILED: fopen");
            return 1;
        }

        whole = XPT_MALLOC(arena, flen);
        if (!whole) {
            perror("FAILED: XPT_MALLOC for whole");
            return 1;
        }
        
        if (flen > 0) {
            size_t rv = fread(whole, 1, flen, in);
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
                fprintf(stdout, "XPT_MakeCursor failed for %s\n", name);
                return 1;
            }
            if (!XPT_DoHeader(arena, cursor, &header)) {
                fprintf(stdout,
                        "DoHeader failed for %s.  Is %s a valid .xpt file?\n",
                        name, name);
                return 1;
            }                                        
            
            oldTotalNumberOfInterfaces = totalNumberOfInterfaces;
            totalNumberOfInterfaces += header->num_interfaces;
            if (header->num_interfaces > 0) {
                XPTInterfaceDirectoryEntry *newIDE;
                fixElement *newFix;
  
                newIDE = (XPTInterfaceDirectoryEntry *)
                    XPT_MALLOC(arena, totalNumberOfInterfaces * 
                               sizeof(XPTInterfaceDirectoryEntry));
                if (!newIDE) {
                    perror("FAILED: XPT_MALLOC of IDE_array");
                    return 1;
                }

                if (IDE_array) {
                    if (oldTotalNumberOfInterfaces)
                        memcpy(newIDE, IDE_array,
                               oldTotalNumberOfInterfaces * 
                               sizeof(XPTInterfaceDirectoryEntry));
                    XPT_FREE(arena, IDE_array);
                }
                IDE_array = newIDE;


                newFix = (fixElement *)
                    XPT_MALLOC(arena, 
                               totalNumberOfInterfaces * sizeof(fixElement));
                if (!newFix) {
                    perror("FAILED: XPT_MALLOC of fix_array");
                    return 1;
                }

                if (fix_array) {
                    if (oldTotalNumberOfInterfaces)
                        memcpy(newFix, fix_array,
                               oldTotalNumberOfInterfaces * 
                               sizeof(fixElement));
                    XPT_FREE(arena, fix_array);
                }
                fix_array = newFix;
            
                for (j=0; j<header->num_interfaces; j++) {
                    if (!copy_IDE(&header->interface_directory[j], 
                                  &IDE_array[k])) {
                        perror("FAILED: 1st copying of IDE");
                        return 1;
                    }
                    fix_array[k].iid = IDE_array[k].iid;
                    fix_array[k].name = IDE_array[k].name;
                    fix_array[k].file_num = i-2;
                    fix_array[k].interface_num = j+1;
                    fix_array[k].is_deleted = PR_FALSE;
                    fix_array[k].maps_to_file_num = i-2;
                    fix_array[k].maps_to_interface_num = j+1;

                    k++;
                }
            }
            
            /* Copy the annotations if they are not 'empty'
             */
            if (header->annotations != NULL &&
                header->annotations->flags != XPT_ANN_LAST) {
                ann = first_ann;
                while (ann->next != NULL) {
                    ann = ann->next;
                }
                ann->next = header->annotations;
            }
            
            XPT_FREEIF(arena, header);
            if (state)
                XPT_DestroyXDRState(state);
            XPT_FREE(arena, whole);
            flen = 0;            

        } else {
            fclose(in);
            perror("FAILED: file length <= 0");
            return 1;
        }
    }

    /* Make sure the last annotation is the only one marked as XP_ANN_LAST.
     */
    ann = first_ann;
    while (ann->next != NULL) {
        ann->flags &= ~XPT_ANN_LAST;
        ann = ann->next;
    }    
    ann->flags |= XPT_ANN_LAST;

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
        if (compare_strings(IDE_array[i-1].name, 
                            IDE_array[i].name) == 0 && 
            compare_strings(IDE_array[i-1].name_space, 
                             IDE_array[i].name_space) == 0) {
            
            /* If one of the interfaces is unresolved, delete that one 
             * preferentailly.
             */
            if (!IDE_array[i-1].interface_descriptor) {
                /* Shrink the IDE_array to delete the duplicate interface.
                 */
                if (!shrink_IDE_array(IDE_array, 
                                      i-1, 
                                      trueNumberOfInterfaces)) {
                    perror("FAILED: shrink_IDE_array");
                    return 1;
                }
                /* Update the fix array. This involves moving the deleted 
                 * entry to the end of the array (rather than deleting it)
                 * and mapping it to the "replacement" element so we can
                 * update interface indices appropriately later.
                 */
                update_fix_array(arena, fix_array, i-1, 
                                 totalNumberOfInterfaces, i);
                /* Decrement the true number of interfaces since we just
                 * deleted one. There's more than one way to get out of
                 * this loop.
                 */
                trueNumberOfInterfaces--;
            } else {
                if (!IDE_array[i].interface_descriptor ||
                    (compare_IIDs(&IDE_array[i-1].iid, &IDE_array[i].iid) == 0))
                {
                    /* Shrink the IDE_array to delete the duplicate interface.
                     */
                    if (!shrink_IDE_array(IDE_array, 
                                          i, 
                                          trueNumberOfInterfaces)) {
                        perror("FAILED: shrink_IDE_array");
                        return 1;
                    }
                    /* Update the fix array. This involves moving the deleted 
                     * entry to the end of the array (rather than deleting it)
                     * and mapping it to the "replacement" element so we can
                     * update interface indices appropriately later.
                     */
                    update_fix_array(arena, fix_array, i, 
                                     totalNumberOfInterfaces, i-1);
                    /* Decrement the true number of interfaces since we just
                     * deleted one. There's more than one way to get out of
                     * this loop.
                     */
                    trueNumberOfInterfaces--;
                } else {
                    /* Found interfaces with duplicate names but different
                     * iids! */
                    char *ns = IDE_array[i].name_space;
                    fprintf(stderr,
                            "ERROR: found duplicate definitions of interface "
                            "%s%s%s with iids \n",
                            ns ? ns : "", ns ? "::" : "", IDE_array[i].name);
                    print_IID(&IDE_array[i].iid, stderr);
                    fprintf(stderr, " and ");
                    print_IID(&IDE_array[i-1].iid, stderr);
                    fprintf(stderr, "\n");
                    return 1;
                }
            }
        } else {
            /* Only increment if there was no name_space::name collision.
             */
            i++;
        }
    }

    /* Sort the IDE_array (put them in their final order) so that updating
     * of indices will be meaningful.
     */
    qsort(IDE_array, 
          trueNumberOfInterfaces, 
          sizeof(XPTInterfaceDirectoryEntry), 
          compare_IDEs_by_IID);

    /* Sort the fix_array to match the IDE_array. 
     */
    qsort(fix_array, 
          trueNumberOfInterfaces, 
          sizeof(fixElement), 
          compare_fixElements_by_IID);

    /* Iterate through the remaining interfaces (those not deleted) 
     * looking for references to interfaces (such as id->parent_interface)
     * which need an updated index number.
     */
    for (i=0; i<trueNumberOfInterfaces; i++) {
        
        /* Define id to save some keystrokes.
         */
        id = IDE_array[i].interface_descriptor;

        /* Check for unresolved interface.
         */
        if (id) {
            
            /* Fix parent_interface first.
             */
            if (id->parent_interface && id->parent_interface != 0) {
                id->parent_interface = 
                    get_new_index(fix_array, totalNumberOfInterfaces,
                                  fix_array[i].file_num, id->parent_interface);
            }
            /* Iterate through the method descriptors looking for params of
             * type TD_INTERFACE_TYPE.
             */
            for (j=0; j<id->num_methods; j++) {
                /* Cycle through the params first.
                 */
                for (k=0; k<id->method_descriptors[j].num_args; k++) {
                    /* Define td to save some keystrokes.
                     */
                    td = &id->method_descriptors[j].params[k].type;

                    while (XPT_TDP_TAG(td->prefix) == TD_ARRAY) {
                        td = &id->additional_types[td->type.additional_type];
                    }

                    if (XPT_TDP_TAG(td->prefix) == TD_INTERFACE_TYPE) {
                        td->type.iface = 
                            get_new_index(fix_array, 
                                          totalNumberOfInterfaces,
                                          fix_array[i].file_num, 
                                          td->type.iface);
                    }                                                
                }

                /* Check the result param too. Define td again to save 
                 * some keystrokes.
                 */
                td = &id->method_descriptors[j].result->type;
                if (XPT_TDP_TAG(td->prefix) == TD_INTERFACE_TYPE) {
                    td->type.iface = 
                        get_new_index(fix_array, totalNumberOfInterfaces,
                                      fix_array[i].file_num, 
                                      td->type.iface);
                }                
            }
        } 
    }
    
    /* Iterate through the array quickly looking for duplicate IIDS.
     * This shouldn't happen, i.e. is a failure condition, so bail
     * if we find a duplicate. If we have more than one entry, start 
     * at one so we don't accidentally grep the ether.
     */
    if (trueNumberOfInterfaces>1) {
        for (i=1; i<trueNumberOfInterfaces; i++) {
            /* Only complain if the IIDs are identical and nonzero. */
            if (compare_IIDs(&IDE_array[i-1].iid, &IDE_array[i].iid) == 0 && 
                compare_IDE_with_zero(&IDE_array[i]) != 0) {
                fprintf(stderr, "FATAL ERROR:\n"
                        "Duplicate IID detected (");
                print_IID(&IDE_array[i-1].iid, stderr);
                fprintf(stderr, ") in\n"
                        "interface %s::%s from %s\n"
                        "and\n" 
                        "interface %s::%s from %s\n",
                        IDE_array[i-1].name_space ? 
                        IDE_array[i-1].name_space : "", 
                        IDE_array[i-1].name, 
                        argv[fix_array[i-1].file_num+2],
                        IDE_array[i].name_space ? 
                        IDE_array[i].name_space : "", 
                        IDE_array[i].name, 
                        argv[fix_array[i].file_num+2]);
                return 1;
            }
        }
    }

    header = XPT_NewHeader(arena, (PRUint16)trueNumberOfInterfaces);
    header->annotations = first_ann;
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
    
    XPT_SetDataOffset(state, header_sz);

    if (!XPT_MakeCursor(state, XPT_HEADER, header_sz, cursor)) {
        perror("FAILED: error making cursor");
        return 1;
    }
    oldOffset = cursor->offset;
    if (!XPT_DoHeader(arena, cursor, &header)) {
        perror("FAILED: error doing Header");
        return 1;
    }
    newOffset = cursor->offset;
    XPT_GetXDRDataLength(state, XPT_HEADER, &len);
    header->file_length = len;
    XPT_GetXDRDataLength(state, XPT_DATA, &len);
    header->file_length += len;
    XPT_SeekTo(cursor, oldOffset);
    if (!XPT_DoHeaderPrologue(arena, cursor, &header, NULL)) {
        perror("FAILED: error doing Header");
        return 1;
    }
    XPT_SeekTo(cursor, newOffset);
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
        fprintf(stderr, "Error writing file: %s\n", argv[1]);
    } else {
/*        fprintf(stderr, "File written: %s\n", argv[1]); */
    }
 
    if (state)
        XPT_DestroyXDRState(state);
    
    XPT_DestroyArena(arena);

    return 0;        
}

static int 
compare_IDEs_by_IID(const void *ap,
                    const void *bp)
{
    const XPTInterfaceDirectoryEntry *ide1 = ap, *ide2 = bp;
    
    int answer = compare_IIDs(&ide1->iid, &ide2->iid);
    if(!answer)
        answer = compare_strings(ide1->name, ide2->name);

    return answer;
}  

/* For detecting unresolved interfaces. */
const nsID iid_zero = { 0x0, 0x0, 0x0,
                        { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };

static int
compare_IDE_with_zero(const void *ap)
{
    const XPTInterfaceDirectoryEntry *ide1 = ap;

    return compare_IIDs(&ide1->iid, &iid_zero);
}

static int 
compare_fixElements_by_IID(const void *ap,
                           const void *bp)
{
    const fixElement *fix1 = ap, *fix2 = bp;
    
    int answer = compare_IIDs(&fix1->iid, &fix2->iid);
    if(!answer)
        answer = compare_strings(fix1->name, fix2->name);

    return answer;
}  

static int 
compare_IDEs_by_name(const void *ap,
                     const void *bp)
{
    const XPTInterfaceDirectoryEntry *ide1 = ap, *ide2 = bp;

    int answer = compare_strings(ide1->name, ide2->name);
    if(!answer)
        answer = compare_pointers(ide1->name, ide2->name);

    return answer;
}

static int 
compare_IDEs_by_name_space(const void *ap,
                           const void *bp)
{
    const XPTInterfaceDirectoryEntry *ide1 = ap, *ide2 = bp;
    
    return compare_strings(ide1->name_space, ide2->name_space);
}

static int 
compare_strings(const void *ap, const void *bp)
{
    const char *string1 = ap, *string2 = bp;

    if (!string1 && !string2)
        return 0;
    if (!string1)
        return -1;
    if (!string2)
        return 1;

    return strcmp(string1, string2);    
}     

static int 
compare_pointers(const void *ap, const void *bp)
{
    if (ap == bp) {
#ifdef DEBUG_jband
        perror("name addresses were equal!");
#endif
        return 0;
    }
    if (ap > bp)
        return 1;
    return -1;
}        

static int 
compare_fixElements_by_name(const void *ap,
                            const void *bp)
{
    const fixElement *fix1 = ap, *fix2 = bp;

    int answer= compare_strings(fix1->name, fix2->name);
    if(!answer)
        answer = compare_pointers(fix1->name, fix2->name);

    return answer;
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

/* update_fix_array marks a fixElement as deleted, updates its mapping 
 * to point to the "replacement" element, and moves it to the end of 
 * the array.
 */
PRBool 
update_fix_array(XPTArena *arena, fixElement *fix, int element_to_delete, 
                 int num_interfaces, int replacement) 
{
    fixElement *deleted;
    int i;  

    if (element_to_delete >= num_interfaces) {
        return PR_FALSE;
    }

    deleted = XPT_CALLOC(arena, sizeof(fixElement));
    if (!copy_fixElement(&fix[element_to_delete], deleted)) {
        return PR_FALSE;
    }
    deleted->is_deleted = PR_TRUE;
    deleted->maps_to_file_num = fix[replacement].file_num;
    deleted->maps_to_interface_num = fix[replacement].interface_num;
    
    for (i=element_to_delete+1; i<num_interfaces; i++) {
        if (!copy_fixElement(&fix[i], &fix[i-1])) {
            return PR_FALSE;
        }
    }

    if (!copy_fixElement(deleted, &fix[num_interfaces-1])) {
        return PR_FALSE;
    }
    
    return PR_TRUE;
}

/* get_new_index returns the new interface index by walking the fix_array
 * until the file_num and interface_num match the target values. If a match
 * is found, it checks to see if that element has been deleted. If it has 
 * been deleted, it follows that elements mapping until it gets to the 
 * proper interface (recursion). FYI, Indices are one-based; zero 
 * represents the special case (no parent interface). 
 */
static int 
get_new_index(const fixElement *fix, int num_elements, 
              int target_file, int target_interface) 
{
    int i;
    
    for (i=0; i<num_elements; i++) { 
        if (fix[i].file_num == target_file &&
            fix[i].interface_num == target_interface) {
            if (fix[i].is_deleted) {
                return get_new_index(fix, num_elements, 
                                     fix[i].maps_to_file_num,
                                     fix[i].maps_to_interface_num);
            }
            return i+1;
        }
    }
    
    return 0;
}

PRBool
copy_IDE(XPTInterfaceDirectoryEntry *from, XPTInterfaceDirectoryEntry *to) 
{
    if (!from || !to) {
        return PR_FALSE;
    }
    
    to->iid = from->iid;
    to->name = from->name;
    to->name_space = from->name_space;
    to->interface_descriptor = from->interface_descriptor;
    return PR_TRUE;
}

PRBool
copy_fixElement(fixElement *from, fixElement *to)
{
    if (!from || !to) {
        return PR_FALSE;
    }

    to->iid = from->iid;
    to->name = from->name;
    to->file_num = from->file_num;
    to->interface_num = from->interface_num;
    to->is_deleted = from->is_deleted;
    to->maps_to_file_num = from->maps_to_file_num;
    to->maps_to_interface_num = from->maps_to_interface_num;

    return PR_TRUE;    
}

static void
print_IID(struct nsID *iid, FILE *file)
{
    fprintf(file, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            (PRUint32) iid->m0, (PRUint32) iid->m1,(PRUint32) iid->m2,
            (PRUint32) iid->m3[0], (PRUint32) iid->m3[1],
            (PRUint32) iid->m3[2], (PRUint32) iid->m3[3],
            (PRUint32) iid->m3[4], (PRUint32) iid->m3[5],
            (PRUint32) iid->m3[6], (PRUint32) iid->m3[7]); 
}

static void
xpt_link_usage(char *argv[]) 
{
    fprintf(stdout, "Usage: %s outfile file1.xpt file2.xpt ...\n"
            "       Links multiple typelib files into one outfile\n", argv[0]);
}

