/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Generate typelib files for use with InterfaceInfo.
 * http://www.mozilla.org/scriptable/typelib_file.html
 */

#include "xpidl.h"
#include <xpt_xdr.h>
#include <xpt_struct.h>
#include <time.h>               /* XXX XP? */

struct priv_data {
    XPTHeader *header;
    uint16 ifaces;
    GHashTable *interface_map;
    XPTInterfaceDescriptor *current;
    XPTArena *arena;
    uint16 next_method;
    uint16 next_const;
    uint16 next_type;   /* used for 'additional_types' for idl arrays */
};

#define HEADER(state)     (((struct priv_data *)state->priv)->header)
#define IFACES(state)     (((struct priv_data *)state->priv)->ifaces)
#define IFACE_MAP(state)  (((struct priv_data *)state->priv)->interface_map)
#define CURRENT(state)    (((struct priv_data *)state->priv)->current)
#define ARENA(state)      (((struct priv_data *)state->priv)->arena)
#define NEXT_METH(state)  (((struct priv_data *)state->priv)->next_method)
#define NEXT_CONST(state) (((struct priv_data *)state->priv)->next_const)
#define NEXT_TYPE(state)  (((struct priv_data *)state->priv)->next_type)

#ifdef DEBUG_shaver
/* #define DEBUG_shaver_sort */
#endif

typedef struct {
    char     *full_name;
    char     *name;
    char     *name_space;
    char     *iid;
    gboolean is_forward_dcl;
} NewInterfaceHolder;

static NewInterfaceHolder*
CreateNewInterfaceHolder(char *name, char *name_space, char *iid, 
                         gboolean is_forward_dcl)
{
    NewInterfaceHolder *holder = calloc(1, sizeof(NewInterfaceHolder));
    if (holder) {
        holder->is_forward_dcl = is_forward_dcl;
        if (name)
            holder->name = xpidl_strdup(name);
        if (name_space)
            holder->name_space = xpidl_strdup(name_space);
        if (holder->name && holder->name_space) {
            holder->full_name = calloc(1, strlen(holder->name) +
                                          strlen(holder->name_space) + 2);
        }
        if (holder->full_name) {
            strcpy(holder->full_name, holder->name_space);
            strcat(holder->full_name, ".");
            strcat(holder->full_name, holder->name);
        }
        else
            holder->full_name = holder->name;
        if (iid)
            holder->iid = xpidl_strdup(iid);
    }
    return holder;
}

static void
DeleteNewInterfaceHolder(NewInterfaceHolder *holder)
{
    if (holder) {
        if (holder->full_name && holder->full_name != holder->name)
            free(holder->full_name);
        if (holder->name)
            free(holder->name);
        if (holder->name_space)
            free(holder->name_space);
        if (holder->iid)
            free(holder->iid);
        free(holder);
    }
}

/*
 * If p is an ident for an interface, and we don't have an entry in the
 * interface map yet, add one.
 */
static gboolean
add_interface_maybe(IDL_tree_func_data *tfd, gpointer user_data)
{
    TreeState *state = user_data;
    IDL_tree up;
    if (IDL_NODE_TYPE(tfd->tree) == IDLN_IDENT) {
        IDL_tree_type node_type = IDL_NODE_TYPE((up = IDL_NODE_UP(tfd->tree)));
        if (node_type == IDLN_INTERFACE || node_type == IDLN_FORWARD_DCL) {

            /* We only want to add a new entry if there is no entry by this 
             * name or if the previously found entry was just a forward 
             * declaration and the new entry is not.
             */

            char *iface = IDL_IDENT(tfd->tree).str;
            NewInterfaceHolder *old_holder = (NewInterfaceHolder *) 
                    g_hash_table_lookup(IFACE_MAP(state), iface);
            if (old_holder && old_holder->is_forward_dcl &&
                node_type != IDLN_FORWARD_DCL)
            {
                g_hash_table_remove(IFACE_MAP(state), iface);
                DeleteNewInterfaceHolder(old_holder);
                old_holder = NULL;
            }
            if (!old_holder) {
                /* XXX should we parse here and store a struct nsID *? */
                char *iid = (char *)IDL_tree_property_get(tfd->tree, "uuid");
                char *name_space = (char *)
                            IDL_tree_property_get(tfd->tree, "namespace");
                NewInterfaceHolder *holder =
                        CreateNewInterfaceHolder(iface, name_space, iid,
                                     (gboolean) node_type == IDLN_FORWARD_DCL);
                if (!holder)
                    return FALSE;
                g_hash_table_insert(IFACE_MAP(state),
                                    holder->full_name, holder);
                IFACES(state)++;
#ifdef DEBUG_shaver_ifaces
                fprintf(stderr, "adding interface #%d: %s/%s\n", IFACES(state),
                        iface, iid[0] ? iid : "<unresolved>");
#endif
            }
        } else {
#ifdef DEBUG_shaver_ifaces
            fprintf(stderr, "ident %s isn't an interface (%s)\n",
                    IDL_IDENT(tfd->tree).str, IDL_NODE_TYPE_NAME(up));
#endif
        }
    }

    return TRUE;
}

/* Find all the interfaces referenced in the tree (uses add_interface_maybe) */
static gboolean
find_interfaces(IDL_tree_func_data *tfd, gpointer user_data)
{
    IDL_tree node = NULL;

    switch (IDL_NODE_TYPE(tfd->tree)) {
      case IDLN_ATTR_DCL:
        node = IDL_ATTR_DCL(tfd->tree).param_type_spec;
        break;
      case IDLN_OP_DCL:
         IDL_tree_walk_in_order(IDL_OP_DCL(tfd->tree).parameter_dcls, find_interfaces,
                               user_data);
        node = IDL_OP_DCL(tfd->tree).op_type_spec;
        break;
      case IDLN_PARAM_DCL:
        node = IDL_PARAM_DCL(tfd->tree).param_type_spec;
        break;
      case IDLN_INTERFACE:
        node = IDL_INTERFACE(tfd->tree).inheritance_spec;
        if (node)
            xpidl_list_foreach(node, add_interface_maybe, user_data);
        node = IDL_INTERFACE(tfd->tree).ident;
        break;
      case IDLN_FORWARD_DCL:
        node = IDL_FORWARD_DCL(tfd->tree).ident;
        break;
      default:
        node = NULL;
    }

    if (node && IDL_NODE_TYPE(node) == IDLN_IDENT) {
        IDL_tree_func_data new_tfd;
        new_tfd.tree = node;
        add_interface_maybe(&new_tfd, user_data);
    }

    return TRUE;
}

#ifdef DEBUG_shaver
/* for calling from gdb */
static void
print_IID(struct nsID *iid, FILE *file)
{
    char iid_buf[UUID_LENGTH];

    xpidl_sprint_iid(iid, iid_buf);
    fprintf(file, "%s\n", iid_buf);
}
#endif

/* fill the interface_directory IDE table from the interface_map */
static gboolean
fill_ide_table(gpointer key, gpointer value, gpointer user_data)
{
    TreeState *state = user_data;
    NewInterfaceHolder *holder = (NewInterfaceHolder *) value;
    struct nsID id;
    XPTInterfaceDirectoryEntry *ide;

    XPT_ASSERT(holder);

#ifdef DEBUG_shaver_ifaces
    fprintf(stderr, "filling %s\n", holder->full_name);
#endif

    if (holder->iid) {
        if (strlen(holder->iid) != 36) {
            IDL_tree_error(state->tree, "IID %s is the wrong length\n",
                           holder->iid);
            return FALSE;
        }
        if (!xpidl_parse_iid(&id, holder->iid)) {
            IDL_tree_error(state->tree, "cannot parse IID %s\n", holder->iid);
            return FALSE;
        }
    } else {
        memset(&id, 0, sizeof(id));
    }

    ide = &(HEADER(state)->interface_directory[IFACES(state)]);
    if (!XPT_FillInterfaceDirectoryEntry(ARENA(state), ide, &id, holder->name,
                                         holder->name_space, NULL)) {
        IDL_tree_error(state->tree, "INTERNAL: XPT_FillIDE failed for %s\n",
                       holder->full_name);
        return FALSE;
    }

    IFACES(state)++;
    DeleteNewInterfaceHolder(holder);
    return TRUE;
}

static int
compare_IDEs(const void *ap, const void *bp)
{
    const XPTInterfaceDirectoryEntry *a = ap, *b = bp;
    const nsID *aid = &a->iid, *bid = &b->iid;
    const char *ans, *bns;

    int i;
#define COMPARE(field) if (aid->field > bid->field) return 1; \
                       if (bid->field > aid->field) return -1;
    COMPARE(m0);
    COMPARE(m1);
    COMPARE(m2);
    for (i = 0; i < 8; i++) {
        COMPARE(m3[i]);
    }

    /* defend against NULL name_space by using empty string. */
    ans = a->name_space ? a->name_space : "";
    bns = b->name_space ? b->name_space : "";

    if (a->name_space && b->name_space) {
        if ((i = strcmp(a->name_space, b->name_space)))
            return i;
    } else {
        if (a->name_space || b->name_space) {
            if (a->name_space)
                return -1;
            return 1;
        }
    }
    /* these had better not be NULL... */
    return strcmp(a->name, b->name);
#undef COMPARE
}

/* sort the IDE block as per the typelib spec: IID order, unresolved first */
static void
sort_ide_block(TreeState *state)
{
    XPTInterfaceDirectoryEntry *ide;
    int i;

    /* boy, I sure hope qsort works correctly everywhere */
#ifdef DEBUG_shaver_sort
    fputs("before sort:\n", stderr);
    for (i = 0; i < IFACES(state); i++) {
        fputs("  ", stderr);
        print_IID(&HEADER(state)->interface_directory[i].iid, stderr);
        fputc('\n', stderr);
    }
#endif
    qsort(HEADER(state)->interface_directory, IFACES(state),
          sizeof(*ide), compare_IDEs);
#ifdef DEBUG_shaver_sort
    fputs("after sort:\n", stderr);
    for (i = 0; i < IFACES(state); i++) {
        fputs("  ", stderr);
        print_IID(&HEADER(state)->interface_directory[i].iid, stderr);
        fputc('\n', stderr);
    }
#endif

    for (i = 0; i < IFACES(state); i++) {
        ide = HEADER(state)->interface_directory + i;
        g_hash_table_insert(IFACE_MAP(state), ide->name, (void *)(i + 1));
    }

    return;
}

static gboolean
typelib_list(TreeState *state)
{
    IDL_tree iter;
    for (iter = state->tree; iter; iter = IDL_LIST(iter).next) {
        state->tree = IDL_LIST(iter).data;
        if (!xpidl_process_node(state))
            return FALSE;
    }
    return TRUE;
}

static gboolean
typelib_prolog(TreeState *state)
{
    state->priv = calloc(1, sizeof(struct priv_data));
    if (!state->priv)
        return FALSE;
    IFACES(state) = 0;
    IFACE_MAP(state) = g_hash_table_new(g_str_hash, g_str_equal);
    if (!IFACE_MAP(state)) {
        /* XXX report error */
        free(state->priv);
        return FALSE;
    }
    /* find all interfaces, top-level and referenced by others */
    IDL_tree_walk_in_order(state->tree, find_interfaces, state);
    ARENA(state) = XPT_NewArena(1024, sizeof(double), "main xpidl arena");
    HEADER(state) = XPT_NewHeader(ARENA(state), IFACES(state));

    /* fill IDEs from hash table */
    IFACES(state) = 0;
    g_hash_table_foreach_remove(IFACE_MAP(state), fill_ide_table, state);

    /* if any are left then we must have failed in fill_ide_table */
    if (g_hash_table_size(IFACE_MAP(state)))
        return FALSE;

    /* sort the IDEs by IID order and store indices in the interface map */
    sort_ide_block(state);

    return TRUE;
}

static gboolean
typelib_epilog(TreeState *state)
{
    XPTState *xstate = XPT_NewXDRState(XPT_ENCODE, NULL, 0);
    XPTCursor curs, *cursor = &curs;
    PRUint32 i, len, header_sz;
    PRUint32 oldOffset;
    PRUint32 newOffset;
    char *data;

    /* Write any annotations */
    if (emit_typelib_annotations) {
        PRUint32 annotation_len, written_so_far;
        char *annotate_val, *timestr;
        time_t now;
        static char *annotation_format = 
            "Created from %s.idl\nCreation date: %sInterfaces:";

        /* fill in the annotations, listing resolved interfaces in order */

        (void)time(&now);
        timestr = ctime(&now);

        /* Avoid dependence on nspr; no PR_smprintf and friends. */

        /* How large should the annotation string be? */
        annotation_len = strlen(annotation_format) + strlen(state->basename) +
            strlen(timestr);
        for (i = 0; i < HEADER(state)->num_interfaces; i++) {
            XPTInterfaceDirectoryEntry *ide;
            ide = &HEADER(state)->interface_directory[i];
            if (ide->interface_descriptor) {
                annotation_len += strlen(ide->name) + 1;
            }
        }

        annotate_val = (char *) malloc(annotation_len);
        written_so_far = sprintf(annotate_val, annotation_format,
                                 state->basename, timestr);
        
        for (i = 0; i < HEADER(state)->num_interfaces; i++) {
            XPTInterfaceDirectoryEntry *ide;
            ide = &HEADER(state)->interface_directory[i];
            if (ide->interface_descriptor) {
                written_so_far += sprintf(annotate_val + written_so_far, " %s",
                                          ide->name);
            }
        }

        HEADER(state)->annotations =
            XPT_NewAnnotation(ARENA(state), 
                              XPT_ANN_LAST | XPT_ANN_PRIVATE,
                              XPT_NewStringZ(ARENA(state), "xpidl 0.99.9"),
                              XPT_NewStringZ(ARENA(state), annotate_val));
        free(annotate_val);
    } else {
        HEADER(state)->annotations =
            XPT_NewAnnotation(ARENA(state), XPT_ANN_LAST, NULL, NULL);
    }

    if (!HEADER(state)->annotations) {
        /* XXX report out of memory error */
        return FALSE;
    }

    /* Write the typelib */
    header_sz = XPT_SizeOfHeaderBlock(HEADER(state));

    if (!xstate ||
        !XPT_MakeCursor(xstate, XPT_HEADER, header_sz, cursor))
        goto destroy_header;
    oldOffset = cursor->offset;
    if (!XPT_DoHeader(ARENA(state), cursor, &HEADER(state)))
        goto destroy;
    newOffset = cursor->offset;
    XPT_GetXDRDataLength(xstate, XPT_HEADER, &len);
    HEADER(state)->file_length = len;
    XPT_GetXDRDataLength(xstate, XPT_DATA, &len);
    HEADER(state)->file_length += len;
    XPT_SeekTo(cursor, oldOffset);
    if (!XPT_DoHeaderPrologue(ARENA(state), cursor, &HEADER(state), NULL))
        goto destroy;
    XPT_SeekTo(cursor, newOffset);
    XPT_GetXDRData(xstate, XPT_HEADER, &data, &len);
    fwrite(data, len, 1, state->file);
    XPT_GetXDRData(xstate, XPT_DATA, &data, &len);
    fwrite(data, len, 1, state->file);

 destroy:
    XPT_DestroyXDRState(xstate);
 destroy_header:
    /* XXX XPT_DestroyHeader(HEADER(state)) */

    XPT_FreeHeader(ARENA(state), HEADER(state));
    XPT_DestroyArena(ARENA(state));

    /* XXX should destroy priv_data here */

    return TRUE;
}

static XPTInterfaceDirectoryEntry *
FindInterfaceByName(XPTInterfaceDirectoryEntry *ides, uint16 num_interfaces,
                    const char *name)
{
    uint16 i;
    for (i = 0; i < num_interfaces; i++) {
        if (!strcmp(ides[i].name, name))
            return &ides[i];
    }
    return NULL;
}

static gboolean
typelib_interface(TreeState *state)
{
    IDL_tree iface = state->tree, iter;
    char *name = IDL_IDENT(IDL_INTERFACE(iface).ident).str;
    XPTInterfaceDirectoryEntry *ide;
    XPTInterfaceDescriptor *id;
    uint16 parent_id = 0;
    PRUint8 interface_flags = 0;

    if (!verify_interface_declaration(iface))
        return FALSE;

    if (IDL_tree_property_get(IDL_INTERFACE(iface).ident, "scriptable"))
        interface_flags |= XPT_ID_SCRIPTABLE;

    if (IDL_tree_property_get(IDL_INTERFACE(iface).ident, "function"))
        interface_flags |= XPT_ID_FUNCTION;

    ide = FindInterfaceByName(HEADER(state)->interface_directory,
                              HEADER(state)->num_interfaces, name);
    if (!ide) {
        IDL_tree_error(iface, "ERROR: didn't find interface %s in "
                       "IDE block. Giving up.\n", name);
        return FALSE;
    }

    if ((iter = IDL_INTERFACE(iface).inheritance_spec)) {
        char *parent;
        if (IDL_LIST(iter).next) {
            IDL_tree_error(iface,
                           "ERROR: more than one parent interface for %s\n",
                           name);
            return FALSE;
        }
        parent = IDL_IDENT(IDL_LIST(iter).data).str;
        parent_id = (uint16)(uint32)g_hash_table_lookup(IFACE_MAP(state),
                                                        parent);
        if (!parent_id) {
            IDL_tree_error(iface,
                           "ERROR: no index found for %s. Giving up.\n",
                           parent);
            return FALSE;
        }
    }

    id = XPT_NewInterfaceDescriptor(ARENA(state), parent_id, 0, 0, 
                                    interface_flags);
    if (!id)
        return FALSE;

    CURRENT(state) = ide->interface_descriptor = id;
#ifdef DEBUG_shaver_ifaces
    fprintf(stderr, "DBG: starting interface %s @ %p\n", name, id);
#endif

    NEXT_METH(state) = 0;
    NEXT_CONST(state) = 0;
    NEXT_TYPE(state) = 0;

    state->tree = IDL_INTERFACE(iface).body;
    if (state->tree && !xpidl_process_node(state))
        return FALSE;
#ifdef DEBUG_shaver_ifaces
    fprintf(stderr, "DBG: ending interface %s\n", name);
#endif
    return TRUE;
}

static gboolean
find_arg_with_name(TreeState *state, const char *name, int16 *argnum)
{
    int16 count;
    IDL_tree params;

    XPT_ASSERT(state);
    XPT_ASSERT(name);
    XPT_ASSERT(argnum);

    params = IDL_OP_DCL(IDL_NODE_UP(IDL_NODE_UP(state->tree))).parameter_dcls;
    for (count = 0;
         params != NULL && IDL_LIST(params).data != NULL;
         params = IDL_LIST(params).next, count++)
    {
        const char *cur_name = IDL_IDENT(
                IDL_PARAM_DCL(IDL_LIST(params).data).simple_declarator).str;
        if (!strcmp(cur_name, name)) {
            /* XXX ought to verify that this is the right type here */
            /* XXX for iid_is this must be an iid */
            /* XXX for size_is and length_is this must be a uint32 */
            *argnum = count;
            return TRUE;
        }
    }
    return FALSE;
}

/* return value is for success or failure */
static gboolean
get_size_and_length(TreeState *state, IDL_tree type, 
                    int16 *size_is_argnum, int16 *length_is_argnum,
                    gboolean *has_size_is, gboolean *has_length_is)
{
    *has_size_is = FALSE;
    *has_length_is = FALSE;

    if (IDL_NODE_TYPE(state->tree) == IDLN_PARAM_DCL) {
        IDL_tree sd = IDL_PARAM_DCL(state->tree).simple_declarator;
        const char *size_is;
        const char *length_is;

        /* only if size_is is found does any of this matter */
        size_is = IDL_tree_property_get(sd, "size_is");
        if (!size_is)
            return TRUE;

        if (!find_arg_with_name(state, size_is, size_is_argnum)) {
            IDL_tree_error(state->tree, "can't find matching argument for "
                           "[size_is(%s)]\n", size_is);
            return FALSE;
        }
        *has_size_is = TRUE;

        /* length_is is optional */
        length_is = IDL_tree_property_get(sd, "length_is");
        if (length_is) {
            *has_length_is = TRUE;
            if (!find_arg_with_name(state, length_is, length_is_argnum)) {
                IDL_tree_error(state->tree, "can't find matching argument for "
                               "[length_is(%s)]\n", length_is);
                return FALSE;
            }
        }
    }
    return TRUE;
}

static gboolean
fill_td_from_type(TreeState *state, XPTTypeDescriptor *td, IDL_tree type)
{
    IDL_tree up;
    int16 size_is_argnum;
    int16 length_is_argnum;
    gboolean has_size_is;
    gboolean has_length_is;
    gboolean is_array = FALSE;

    if (type) {

        /* deal with array */

        if (IDL_NODE_TYPE(state->tree) == IDLN_PARAM_DCL) {
            IDL_tree sd = IDL_PARAM_DCL(state->tree).simple_declarator;
            if (IDL_tree_property_get(sd, "array")) {

                is_array = TRUE;

                /* size_is is required! */
                if (!get_size_and_length(state, type, 
                                         &size_is_argnum, &length_is_argnum,
                                         &has_size_is, &has_length_is)) {
                    /* error was reported by helper function */
                    return FALSE;
                }

                if (!has_size_is) {
                   IDL_tree_error(state->tree, "[array] requires [size_is()]\n");
                    return FALSE;
                }

                td->prefix.flags = TD_ARRAY | XPT_TDP_POINTER;
                td->argnum = size_is_argnum;

                if (has_length_is)
                    td->argnum2 = length_is_argnum;
                else
                    td->argnum2 = size_is_argnum;

                /* 
                * XXX - NOTE - this will be broken for multidimensional 
                * arrays because of the realloc XPT_InterfaceDescriptorAddTypes
                * uses. The underlying 'td' can change as we recurse in to get
                * additional dimensions. Luckily, we don't yet support more
                * than on dimension in the arrays
                */
                /* setup the additional_type */                
                if (!XPT_InterfaceDescriptorAddTypes(ARENA(state), 
                                                     CURRENT(state), 1)) {
                    g_error("out of memory\n");
                    return FALSE;
                }
                td->type.additional_type = NEXT_TYPE(state);
                td = &CURRENT(state)->additional_types[NEXT_TYPE(state)];
                NEXT_TYPE(state)++ ;
            }
        }

handle_typedef:
        switch (IDL_NODE_TYPE(type)) {
          case IDLN_TYPE_INTEGER: {
              gboolean sign = IDL_TYPE_INTEGER(type).f_signed;
              switch(IDL_TYPE_INTEGER(type).f_type) {
                case IDL_INTEGER_TYPE_SHORT:
                  td->prefix.flags = sign ? TD_INT16 : TD_UINT16;
                  break;
                case IDL_INTEGER_TYPE_LONG:
                  td->prefix.flags = sign ? TD_INT32 : TD_UINT32;
                  break;
                case IDL_INTEGER_TYPE_LONGLONG:
                  td->prefix.flags = sign ? TD_INT64 : TD_UINT64;
                  break;
              }
              break;
          }
          case IDLN_TYPE_CHAR:
            td->prefix.flags = TD_CHAR;
            break;
          case IDLN_TYPE_WIDE_CHAR:
            td->prefix.flags = TD_WCHAR;
            break;
          case IDLN_TYPE_STRING:
            if (is_array) {
                td->prefix.flags = TD_PSTRING | XPT_TDP_POINTER;
            } else {
                if (!get_size_and_length(state, type, 
                                         &size_is_argnum, &length_is_argnum,
                                         &has_size_is, &has_length_is)) {
                    /* error was reported by helper function */
                    return FALSE;
                }
                if (has_size_is) {
                    td->prefix.flags = TD_PSTRING_SIZE_IS | XPT_TDP_POINTER;
                    td->argnum = size_is_argnum;
                    if (has_length_is)
                        td->argnum2 = length_is_argnum;
                    else
                        td->argnum2 = size_is_argnum;
                } else {
                    td->prefix.flags = TD_PSTRING | XPT_TDP_POINTER;
                }
            }
            break;
          case IDLN_TYPE_WIDE_STRING:
            if (is_array) {
                td->prefix.flags = TD_PWSTRING | XPT_TDP_POINTER;
            } else {
                if (!get_size_and_length(state, type, 
                                         &size_is_argnum, &length_is_argnum,
                                         &has_size_is, &has_length_is)) {
                    /* error was reported by helper function */
                    return FALSE;
                }
                if (has_size_is) {
                    td->prefix.flags = TD_PWSTRING_SIZE_IS | XPT_TDP_POINTER;
                    td->argnum = size_is_argnum;
                    if (has_length_is)
                        td->argnum2 = length_is_argnum;
                    else
                        td->argnum2 = size_is_argnum;
                } else {
                    td->prefix.flags = TD_PWSTRING | XPT_TDP_POINTER;
                }
            }
            break;
          case IDLN_TYPE_BOOLEAN:
            td->prefix.flags = TD_BOOL;
            break;
          case IDLN_TYPE_OCTET:
            td->prefix.flags = TD_UINT8;
            break;
          case IDLN_TYPE_FLOAT:
            switch (IDL_TYPE_FLOAT (type).f_type) {
              case IDL_FLOAT_TYPE_FLOAT:
                td->prefix.flags = TD_FLOAT;
                break;
              case IDL_FLOAT_TYPE_DOUBLE:
                td->prefix.flags = TD_DOUBLE;
                break;
              /* XXX 'long double' just ignored, or what? */
              default: break;
            }
            break;
          case IDLN_IDENT:
            if (!(up = IDL_NODE_UP(type))) {
                IDL_tree_error(state->tree,
                               "ERROR: orphan ident %s in param list\n",
                               IDL_IDENT(type).str);
                return FALSE;
            }
            switch (IDL_NODE_TYPE(up)) {
                /* This whole section is abominably ugly */
              case IDLN_FORWARD_DCL:
              case IDLN_INTERFACE: {
                XPTInterfaceDirectoryEntry *ide, *ides;
                uint16 num_ifaces;
                char *className;
                const char *iid_is;
handle_iid_is:
                ides = HEADER(state)->interface_directory;
                num_ifaces = HEADER(state)->num_interfaces;
                /* might get here via the goto, so re-check type */
                if (IDL_NODE_TYPE(up) == IDLN_INTERFACE)
                    className = IDL_IDENT(IDL_INTERFACE(up).ident).str;
                else if (IDL_NODE_TYPE(up) == IDLN_FORWARD_DCL)
                    className = IDL_IDENT(IDL_FORWARD_DCL(up).ident).str;
                else
                    className = IDL_IDENT(IDL_NATIVE(up).ident).str;
                iid_is = NULL;

                if (IDL_NODE_TYPE(state->tree) == IDLN_PARAM_DCL) {
                    iid_is =
                        IDL_tree_property_get(IDL_PARAM_DCL(state->tree).simple_declarator,
                                              "iid_is");
                }
                if (iid_is) {
                    int16 argnum;
                    if (!find_arg_with_name(state, iid_is, &argnum)) {
                        IDL_tree_error(state->tree,
                                       "can't find matching argument for "
                                       "[iid_is(%s)]\n", iid_is);
                        return FALSE;
                    }
                    td->prefix.flags = TD_INTERFACE_IS_TYPE | XPT_TDP_POINTER;
                    td->argnum = argnum;
                } else {
                    td->prefix.flags = TD_INTERFACE_TYPE | XPT_TDP_POINTER;
                    ide = FindInterfaceByName(ides, num_ifaces, className);
                    if (!ide || ide < ides || ide > ides + num_ifaces) {
                        IDL_tree_error(state->tree,
                                       "unknown iface %s in param\n",
                                       className);
                        return FALSE;
                    }
                    td->type.iface = ide - ides + 1;
#ifdef DEBUG_shaver_index
                    fprintf(stderr, "DBG: index %d for %s\n",
                            td->type.iface, className);
#endif
                }
                break;
              }
              case IDLN_NATIVE: {
                  char *ident;

                  /* jband - adding goto for iid_is when type is native */
                  if (IDL_NODE_TYPE(state->tree) == IDLN_PARAM_DCL &&
                      IDL_tree_property_get(IDL_PARAM_DCL(state->tree).simple_declarator,
                                              "iid_is"))
                      goto handle_iid_is;

                  ident = IDL_IDENT(type).str;
                  if (IDL_tree_property_get(type, "nsid")) {
                      td->prefix.flags = TD_PNSIID;
                      if (IDL_tree_property_get(type, "ref"))
                          td->prefix.flags |= XPT_TDP_POINTER | XPT_TDP_REFERENCE;
                      else if (IDL_tree_property_get(type,"ptr"))
                          td->prefix.flags |= XPT_TDP_POINTER;
                  } else if (IDL_tree_property_get(type, "domstring")) {
                      td->prefix.flags = TD_DOMSTRING | XPT_TDP_POINTER;
                      if (IDL_tree_property_get(type, "ref"))
                          td->prefix.flags |= XPT_TDP_REFERENCE;
                  } else {
                      td->prefix.flags = TD_VOID | XPT_TDP_POINTER;
                  }
                  break;
                }
              default:
                if (IDL_NODE_TYPE(IDL_NODE_UP(up)) == IDLN_TYPE_DCL) {
                    /* restart with the underlying type */
                    IDL_tree new_type;
                    new_type = IDL_TYPE_DCL(IDL_NODE_UP(up)).type_spec;
#ifdef DEBUG_shaver_misc
                    fprintf(stderr, "following %s typedef to %s\n",
                            IDL_IDENT(type).str, IDL_NODE_TYPE_NAME(new_type));
#endif
                    /* 
                    *  Do a nice messy goto rather than recursion so that
                    *  we can avoid screwing up the *array* information.
                    */
/*                    return fill_td_from_type(state, td, new_type); */
                    if (new_type) {
                        type = new_type;
                        goto handle_typedef;
                    } else {
                        /* do what we would do in recursion if !type */
                        td->prefix.flags = TD_VOID;
                        return TRUE;
                    }
                }
                IDL_tree_error(state->tree,
                               "can't handle %s ident in param list\n",
#ifdef DEBUG_shaver
                               /* XXX is this safe to use on Win now? */
                               IDL_NODE_TYPE_NAME(IDL_NODE_UP(type))
#else
                               "that type of"
#endif
                               );
#ifdef DEBUG_shaver
                XPT_ASSERT(0);
#endif
                return FALSE;
            }
            break;
          default:
            IDL_tree_error(state->tree, "can't handle %s in param list\n",
#ifdef DEBUG_shaver
                           /* XXX is this safe to use on Win now? */
                           IDL_NODE_TYPE_NAME(IDL_NODE_UP(type))
#else
                        "that type"
#endif
            );
            return FALSE;
        }
    } else {
        td->prefix.flags = TD_VOID;
    }

    return TRUE;
}

static gboolean
fill_pd_from_type(TreeState *state, XPTParamDescriptor *pd, uint8 flags,
                  IDL_tree type)
{
    pd->flags = flags;
    return fill_td_from_type(state, &pd->type, type);
}

static gboolean
fill_pd_from_param(TreeState *state, XPTParamDescriptor *pd, IDL_tree tree)
{
    uint8 flags = 0;
    gboolean is_dipper_type = DIPPER_TYPE(IDL_PARAM_DCL(tree).param_type_spec);

    switch (IDL_PARAM_DCL(tree).attr) {
      case IDL_PARAM_IN:
        flags = XPT_PD_IN;
        break;
      case IDL_PARAM_OUT:
        flags = XPT_PD_OUT;
        break;
      case IDL_PARAM_INOUT:
        flags = XPT_PD_IN | XPT_PD_OUT;
        break;
    }

    if (IDL_tree_property_get(IDL_PARAM_DCL(tree).simple_declarator,
                              "retval")) {
        if (flags != XPT_PD_OUT) {
            IDL_tree_error(tree, "can't have [retval] with in%s param "
                           "(only out)\n",
                           flags & XPT_PD_OUT ? "out" : "");
            return FALSE;
        }
        flags |= XPT_PD_RETVAL;
    }

    if (is_dipper_type && (flags & XPT_PD_OUT)) {
        flags &= ~XPT_PD_OUT; 
        flags |= XPT_PD_IN | XPT_PD_DIPPER;
    }

    if (IDL_tree_property_get(IDL_PARAM_DCL(tree).simple_declarator,
                              "shared")) {
        if (flags & XPT_PD_IN) {
            IDL_tree_error(tree, "can't have [shared] with in%s param "
                           "(only out)\n",
                           flags & XPT_PD_OUT ? "out" : "");
            return FALSE;
        }
        flags |= XPT_PD_SHARED;
    }

    /* stick param where we can see it later */
    state->tree = tree;
    return fill_pd_from_type(state, pd, flags,
                             IDL_PARAM_DCL(tree).param_type_spec);
}

/* XXXshaver common with xpidl_header.c */
#define ATTR_IDENT(tree) (IDL_IDENT(IDL_LIST(IDL_ATTR_DCL(tree).simple_declarations).data))
#define ATTR_TYPE_DECL(tree) (IDL_ATTR_DCL(tree).param_type_spec)
#define ATTR_TYPE(tree) (IDL_NODE_TYPE(ATTR_TYPE_DECL(tree)))

static gboolean
fill_pd_as_nsresult(XPTParamDescriptor *pd)
{
    pd->type.prefix.flags = TD_UINT32; /* TD_NSRESULT */
    return TRUE;
}

static gboolean
typelib_attr_accessor(TreeState *state, XPTMethodDescriptor *meth,
                      gboolean getter, gboolean hidden)
{
    uint8 methflags = 0;
    uint8 pdflags = 0;

    methflags |= getter ? XPT_MD_GETTER : XPT_MD_SETTER;
    methflags |= hidden ? XPT_MD_HIDDEN : 0;
    if (!XPT_FillMethodDescriptor(ARENA(state), meth, methflags,
                                  ATTR_IDENT(state->tree).str, 1))
        return FALSE;

    if (getter) {
        if (DIPPER_TYPE(ATTR_TYPE_DECL(state->tree))) {
            pdflags |= (XPT_PD_RETVAL | XPT_PD_IN | XPT_PD_DIPPER);
        } else {
            pdflags |= (XPT_PD_RETVAL | XPT_PD_OUT);
        }
    } else {
        pdflags |= XPT_PD_IN;
    }

    if (!fill_pd_from_type(state, meth->params, pdflags,
                           ATTR_TYPE_DECL(state->tree)))
        return FALSE;

    fill_pd_as_nsresult(meth->result);
    NEXT_METH(state)++;
    return TRUE;
}

static gboolean
typelib_attr_dcl(TreeState *state)
{
    XPTInterfaceDescriptor *id = CURRENT(state);
    XPTMethodDescriptor *meth;
    gboolean read_only = IDL_ATTR_DCL(state->tree).f_readonly;

    /* XXX this only handles the first ident; elsewhere too... */
    IDL_tree ident =
        IDL_LIST(IDL_ATTR_DCL(state->tree).simple_declarations).data;

    /* If it's marked [noscript], mark it as hidden in the typelib. */
    gboolean hidden = (IDL_tree_property_get(ident, "noscript") != NULL);

    if (!verify_attribute_declaration(state->tree))
        return FALSE;

    if (!XPT_InterfaceDescriptorAddMethods(ARENA(state), id, 
                                           (PRUint16) (read_only ? 1 : 2)))
        return FALSE;

    meth = &id->method_descriptors[NEXT_METH(state)];

    return typelib_attr_accessor(state, meth, TRUE, hidden) &&
        (read_only || typelib_attr_accessor(state, meth + 1, FALSE, hidden));
}

static gboolean
typelib_op_dcl(TreeState *state)
{
    XPTInterfaceDescriptor *id = CURRENT(state);
    XPTMethodDescriptor *meth;
    struct _IDL_OP_DCL *op = &IDL_OP_DCL(state->tree);
    IDL_tree iter;
    uint16 num_args = 0;
    uint8 op_flags = 0;
    gboolean op_notxpcom = (IDL_tree_property_get(op->ident, "notxpcom")
                            != NULL);
    gboolean op_noscript = (IDL_tree_property_get(op->ident, "noscript")
                            != NULL);

    if (!verify_method_declaration(state->tree))
        return FALSE;

    if (!XPT_InterfaceDescriptorAddMethods(ARENA(state), id, 1))
        return FALSE;

    meth = &id->method_descriptors[NEXT_METH(state)];

    for (iter = op->parameter_dcls; iter; iter = IDL_LIST(iter).next)
        num_args++;             /* count params */
    if (op->op_type_spec && !op_notxpcom)
        num_args++;             /* fake param for _retval */

    if (op_noscript)
        op_flags |= XPT_MD_HIDDEN;
    if (op_notxpcom)
        op_flags |= XPT_MD_NOTXPCOM;

    /* XXXshaver constructor? */

#ifdef DEBUG_shaver_method
    fprintf(stdout, "DBG: adding method %s (nargs %d)\n",
            IDL_IDENT(op->ident).str, num_args);
#endif
    if (!XPT_FillMethodDescriptor(ARENA(state), meth, op_flags, 
                                  IDL_IDENT(op->ident).str,
                                  (uint8) num_args))
        return FALSE;

    for (num_args = 0, iter = op->parameter_dcls; iter;
         iter = IDL_LIST(iter).next, num_args++) {
        XPTParamDescriptor *pd = &meth->params[num_args];
        if (!fill_pd_from_param(state, pd, IDL_LIST(iter).data))
            return FALSE;
    }

    /* stick retval param where we can see it later */
    state->tree = op->op_type_spec;

    /* XXX unless [notxpcom] */
    if (!op_notxpcom) {
        if (op->op_type_spec) {
            uint8 pdflags = DIPPER_TYPE(op->op_type_spec) ?
                                (XPT_PD_RETVAL | XPT_PD_IN | XPT_PD_DIPPER) :
                                (XPT_PD_RETVAL | XPT_PD_OUT);
    
            if (!fill_pd_from_type(state, &meth->params[num_args],
                                   pdflags, op->op_type_spec))
                return FALSE;
        }

        if (!fill_pd_as_nsresult(meth->result))
            return FALSE;
    } else {
#ifdef DEBUG_shaver_notxpcom
        fprintf(stderr, "%s is notxpcom\n", IDL_IDENT(op->ident).str);
#endif
        if (!fill_pd_from_type(state, meth->result, XPT_PD_RETVAL,
                               op->op_type_spec))
            return FALSE;
    }
    NEXT_METH(state)++;
    return TRUE;
}

static gboolean
typelib_const_dcl(TreeState *state)
{
    struct _IDL_CONST_DCL *dcl = &IDL_CONST_DCL(state->tree);
    const char *name = IDL_IDENT(dcl->ident).str;
    gboolean is_long;
    gboolean sign;
    IDL_tree real_type;
    XPTInterfaceDescriptor *id;
    XPTConstDescriptor *cd;
    IDL_longlong_t value;

    if (!verify_const_declaration(state->tree))
        return FALSE;

    /* Could be a typedef; try to map it to the real type. */
    real_type = find_underlying_type(dcl->const_type);
    real_type = real_type ? real_type : dcl->const_type;
    is_long = (IDL_TYPE_INTEGER(real_type).f_type == IDL_INTEGER_TYPE_LONG);

    id = CURRENT(state);
    if (!XPT_InterfaceDescriptorAddConsts(ARENA(state), id, 1))
        return FALSE;
    cd = &id->const_descriptors[NEXT_CONST(state)];
    
    cd->name = IDL_IDENT(dcl->ident).str;
#ifdef DEBUG_shaver_const
    fprintf(stderr, "DBG: adding const %s\n", cd->name);
#endif
    if (!fill_td_from_type(state, &cd->type, dcl->const_type))
        return FALSE;
    
    value = IDL_INTEGER(dcl->const_exp).value;
    sign = IDL_TYPE_INTEGER(dcl->const_type).f_signed;
    if (is_long) {
        if (sign)
            cd->value.i32 = value;
        else
            cd->value.ui32 = value;
    } else {
        if (sign)
            cd->value.i16 = value;
        else
            cd->value.ui16 = value;
    }
    NEXT_CONST(state)++;
    return TRUE;
}

static gboolean
typelib_enum(TreeState *state)
{
    XPIDL_WARNING((state->tree, IDL_WARNING1,
                   "enums not supported, enum \'%s\' ignored",
                   IDL_IDENT(IDL_TYPE_ENUM(state->tree).ident).str));
    return TRUE;
}

backend *
xpidl_typelib_dispatch(void)
{
    static backend result;
    static nodeHandler table[IDLN_LAST];
    static gboolean initialized = FALSE;

    result.emit_prolog = typelib_prolog;
    result.emit_epilog = typelib_epilog;

    if (!initialized) {
        /* Initialize non-NULL elements */
        table[IDLN_LIST] = typelib_list;
        table[IDLN_ATTR_DCL] = typelib_attr_dcl;
        table[IDLN_OP_DCL] = typelib_op_dcl;
        table[IDLN_INTERFACE] = typelib_interface;
        table[IDLN_CONST_DCL] = typelib_const_dcl;
        table[IDLN_TYPE_ENUM] = typelib_enum;
        table[IDLN_NATIVE] = check_native;
        initialized = TRUE;
    }

    result.dispatch_table = table;
    return &result;
}



