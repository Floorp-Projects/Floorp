/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
    uint16 next_method;
    uint16 next_const;
};

#define HEADER(state)     (((struct priv_data *)state->priv)->header)
#define IFACES(state)     (((struct priv_data *)state->priv)->ifaces)
#define IFACE_MAP(state)  (((struct priv_data *)state->priv)->interface_map)
#define CURRENT(state)    (((struct priv_data *)state->priv)->current)
#define NEXT_METH(state)  (((struct priv_data *)state->priv)->next_method)
#define NEXT_CONST(state) (((struct priv_data *)state->priv)->next_const)

#ifdef DEBUG_shaver
/* #define DEBUG_shaver_sort */
#endif

typedef struct {
    char    *full_name;
    char    *name;
    char    *name_space;
    char    *iid;
    PRUint8 flags;
} NewInterfaceHolder;

static NewInterfaceHolder*
CreateNewInterfaceHolder(char *name, char *name_space, 
                         char *iid, PRUint8 flags)
{
    NewInterfaceHolder *holder = calloc(1, sizeof(NewInterfaceHolder));
    if(holder) {
        if(name)
            holder->name = strdup(name);
        if(name_space)
            holder->name_space = strdup(name_space);
        if(holder->name && holder->name_space) {
            holder->full_name = calloc(1, strlen(holder->name) +
                                          strlen(holder->name_space) + 2);
        }
        if(holder->full_name) {
            strcpy(holder->full_name, holder->name_space);
            strcat(holder->full_name, ".");
            strcat(holder->full_name, holder->name);
        }
        else
            holder->full_name = holder->name;
        if(iid)
            holder->iid = strdup(iid);
        holder->flags = flags;
    }
    return holder;
}

static void
DeleteNewInterfaceHolder(NewInterfaceHolder *holder)
{
    if(holder) {
        if(holder->full_name && holder->full_name != holder->name)
            free(holder->full_name);
        if(holder->name)
            free(holder->name);
        if(holder->name_space)
            free(holder->name_space);
        if(holder->iid)
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
        if ((IDL_NODE_TYPE((up = IDL_NODE_UP(tfd->tree))) == IDLN_INTERFACE)) {
            char *iface = IDL_IDENT(tfd->tree).str;
            if (!g_hash_table_lookup(IFACE_MAP(state), iface)) {
                /* XXX should we parse here and store a struct nsID *? */
                char *iid = (char *)IDL_tree_property_get(tfd->tree, "uuid");
                char *name_space = (char *)
                            IDL_tree_property_get(tfd->tree, "namespace");
                PRUint8 flags =
                            IDL_tree_property_get(tfd->tree, "scriptable") ?
                                    XPT_ID_SCRIPTABLE : 0;
                NewInterfaceHolder *holder =
                        CreateNewInterfaceHolder(iface, name_space, iid, flags);
                if(!holder)
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
      default:
        node = NULL;
    }

    if (node && IDL_NODE_TYPE(node) == IDLN_IDENT) {
        IDL_tree_func_data tfd;
        tfd.tree = node;
        add_interface_maybe(&tfd, user_data);
    }

    return TRUE;
}

/* parse str and fill id */

static const char nsIDFmt1[] =
  "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}";

static const char nsIDFmt2[] =
  "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x";

#ifdef DEBUG_shaver
/* for calling from gdb */
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
#endif

/* we cannot link against libxpcom, so we re-implement nsID::Parse here. */
static gboolean
fill_iid(struct nsID *id, char *str)
{
    PRInt32 count = 0;
    PRInt32 n1, n2, n3[8];
    PRInt32 n0, i;

    if (!str[0]) {
        memset(id, 0, sizeof(*id));
        return TRUE;
    }
#ifdef DEBUG_shaver_iid
    fprintf(stderr, "parsing iid   %s\n", str);
#endif

    count = PR_sscanf(str, (str[0] == '{' ? nsIDFmt1 : nsIDFmt2),
                      &n0, &n1, &n2,
                      &n3[0],&n3[1],&n3[2],&n3[3],
                      &n3[4],&n3[5],&n3[6],&n3[7]);

    id->m0 = (PRInt32) n0;
    id->m1 = (PRInt16) n1;
    id->m2 = (PRInt16) n2;
    for (i = 0; i < 8; i++) {
      id->m3[i] = (PRInt8) n3[i];
    }

#ifdef DEBUG_shaver_iid
    if (count == 11) {
        fprintf(stderr, "IID parsed to ");
        print_IID(id, stderr);
        fputs("\n", stderr);
    }
#endif
    return (gboolean)(count == 11);
}

/* fill the interface_directory IDE table from the interface_map */
static gboolean
fill_ide_table(gpointer key, gpointer value, gpointer user_data)
{
    TreeState *state = user_data;
    NewInterfaceHolder *holder = (NewInterfaceHolder *) value;
    struct nsID id;
    XPTInterfaceDirectoryEntry *ide;
    char *iid;

    PR_ASSERT(holder);
    iid = holder->iid ? holder->iid : "";

#ifdef DEBUG_shaver_ifaces
    fprintf(stderr, "filling %s with flags %d\n", holder->full_name, 
                                                  (int)holder->flags);
#endif

    if (!fill_iid(&id, iid)) {
        IDL_tree_error(state->tree, "cannot parse IID %s\n", iid);
        return FALSE;
    }

    ide = &(HEADER(state)->interface_directory[IFACES(state)]);
    if (!XPT_FillInterfaceDirectoryEntry(ide, &id, holder->name,
                                         holder->name_space, NULL,
                                         (void*)holder->flags)) {
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
pass_1(TreeState *state)
{
    gboolean ok = FALSE;

    if (state->tree) {
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
#ifdef DEBUG_shaver_ifaces
        fprintf(stderr, "finding interfaces\n");
#endif
        IDL_tree_walk_in_order(state->tree, find_interfaces, state);
#ifdef DEBUG_shaver_faces
        fprintf(stderr, "found %d interfaces\n", IFACES(state));
#endif
        HEADER(state) = XPT_NewHeader(IFACES(state));


        /* fill IDEs from hash table */
        IFACES(state) = 0;
#ifdef DEBUG_shaver_ifaces
        fprintf(stderr, "filling IDE table\n");
#endif
        g_hash_table_foreach_remove(IFACE_MAP(state), fill_ide_table, state);

        /* if any are left then we must have failed in fill_ide_table */
        if (g_hash_table_size(IFACE_MAP(state)))
            return FALSE;

        /* sort the IDEs by IID order and store indices in the interface map */
        sort_ide_block(state);

        ok = TRUE;
    } else {
        /* write the typelib */
        time_t now;
        char *annotate_val, *data;
        uint32 i, len, header_sz;
        XPTState *xstate = XPT_NewXDRState(XPT_ENCODE, NULL, 0);
        XPTCursor curs, *cursor = &curs;
        /* fill in the annotations, listing resolved interfaces in order */

        (void)time(&now);
        annotate_val = PR_smprintf("Created from %s.idl\nCreation date: %s"
                                   "Interfaces:",
                                   state->basename, ctime(&now));

        for (i = 0; i < HEADER(state)->num_interfaces; i++) {
            XPTInterfaceDirectoryEntry *ide;
            ide = &HEADER(state)->interface_directory[i];
            if (ide->interface_descriptor) {
                annotate_val = PR_sprintf_append(annotate_val, " %s",
                                                 ide->name);
                if (!annotate_val)
                    return FALSE;
            }
        }

        HEADER(state)->annotations =
            XPT_NewAnnotation(XPT_ANN_LAST | XPT_ANN_PRIVATE,
                              XPT_NewStringZ("xpidl 0.99.9"),
                              XPT_NewStringZ(annotate_val));
        PR_smprintf_free(annotate_val);

#ifdef DEBUG_shaver_misc
        fprintf(stderr, "writing the typelib\n");
#endif

        header_sz = XPT_SizeOfHeaderBlock(HEADER(state));

        if (!xstate ||
            !XPT_MakeCursor(xstate, XPT_HEADER, header_sz, cursor))
            goto destroy_header;

        if (!XPT_DoHeader(cursor, &HEADER(state)))
            goto destroy;

        XPT_GetXDRData(xstate, XPT_HEADER, &data, &len);
        fwrite(data, len, 1, state->file);
        XPT_GetXDRData(xstate, XPT_DATA, &data, &len);
        fwrite(data, len, 1, state->file);

        ok = TRUE;

    destroy:
        XPT_DestroyXDRState(xstate);
    destroy_header:
        /* XXX XPT_DestroyHeader(HEADER(state)) */

#ifdef DEBUG_shaver
        fprintf(stderr, "writing typelib was %ssuccessful\n",
                ok ? "" : "not ");
#else
        ;   // msvc would like a statement here
#endif
    }

    return ok;

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

    id = XPT_NewInterfaceDescriptor(parent_id, 0, 0, (uint8) ide->user_data);
    if (!id)
        return FALSE;

    CURRENT(state) = ide->interface_descriptor = id;
#ifdef DEBUG_shaver_ifaces
    fprintf(stderr, "DBG: starting interface %s @ %p\n", name, id);
#endif
    NEXT_METH(state) = 0;
    NEXT_CONST(state) = 0;

    state->tree = IDL_INTERFACE(iface).body;
    if (state->tree && !xpidl_process_node(state))
        return FALSE;
#ifdef DEBUG_shaver_ifaces
    fprintf(stderr, "DBG: ending interface %s\n", name);
#endif
    return TRUE;
}

static gboolean
fill_td_from_type(TreeState *state, XPTTypeDescriptor *td, IDL_tree type)
{
    IDL_tree up;

    if (type) {
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
            /* XXXshaver string-type? */
            td->prefix.flags = TD_PSTRING | XPT_TDP_POINTER;
            break;
          case IDLN_TYPE_WIDE_STRING:
            td->prefix.flags = TD_PWSTRING | XPT_TDP_POINTER;
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
                IDL_tree_error(type,
                               "ERROR: orphan ident %s in param list\n",
                               IDL_IDENT(type).str);
                return FALSE;
            }
            switch (IDL_NODE_TYPE(up)) {
                /* This whole section is abominably ugly */
              case IDLN_INTERFACE: {
                XPTInterfaceDirectoryEntry *ide, *ides;
                uint16 num_ifaces;
                char *className;
                const char *iid_is;
handle_iid_is:
                ides = HEADER(state)->interface_directory;
                num_ifaces = HEADER(state)->num_interfaces;
                className = IDL_IDENT(IDL_INTERFACE(up).ident).str;
                iid_is = NULL;

                if (IDL_NODE_TYPE(state->tree) == IDLN_PARAM_DCL) {
                    iid_is =
                        IDL_tree_property_get(IDL_PARAM_DCL(state->tree).simple_declarator,
                                              "iid_is");
                }
                if (iid_is) {
                    int16 argnum = -1, count;
                    IDL_tree params = IDL_OP_DCL(IDL_NODE_UP(IDL_NODE_UP(state->tree))).parameter_dcls;
                    for (count = 0; IDL_LIST(params).data;
                         params = IDL_LIST(params).next, count++) {
                        char *name;
                        name = IDL_IDENT(IDL_PARAM_DCL(IDL_LIST(params).data).simple_declarator).str;
                        if (!strcmp(name, iid_is)) {
                            /* XXX verify that this is an nsid here */
                            argnum = count;
                            break;
                        }
                    }
                    if (argnum < 0) {
                        IDL_tree_error(type, "can't find matching arg for "
                                       "[iid_is(%s)]\n", iid_is);
                        return FALSE;
                      }
                    td->prefix.flags = TD_INTERFACE_IS_TYPE | XPT_TDP_POINTER;
                    td->type.argnum = argnum;
                } else {
                    td->prefix.flags = TD_INTERFACE_TYPE | XPT_TDP_POINTER;
                    ide = FindInterfaceByName(ides, num_ifaces, className);
                    if (!ide || ide < ides || ide > ides + num_ifaces) {
                        IDL_tree_error(type, "unknown iface %s in param\n",
                                       className);
                        return FALSE;
                    }
                    td->type.interface = ide - ides + 1;
#ifdef DEBUG_shaver_index
                    fprintf(stderr, "DBG: index %d for %s\n",
                            td->type.interface, className);
#endif
                }
                break;
              }
              case IDLN_NATIVE: {
                  char *ident;
                  gboolean isID;

                  /* jband - adding goto for iid_is when type is native */
                  if (IDL_NODE_TYPE(state->tree) == IDLN_PARAM_DCL &&
                      IDL_tree_property_get(IDL_PARAM_DCL(state->tree).simple_declarator,
                                              "iid_is"))
                      goto handle_iid_is;

                  ident = IDL_IDENT(type).str;
                  isID = FALSE;

                  if(IDL_tree_property_get(type, "nsid"))
                      isID = TRUE;

                  /* XXX obsolete the below! */

                  /* check for nsID, nsCID, nsIID, nsIIDRef */
                  if (!isID && ident[0] == 'n' && ident[1] == 's') {
                      ident += 2;
                      if (ident[0] == 'C')
                          ident ++;
                      else if (ident[0] == 'I' && ident[1] == 'I')
                          ident ++;
                      if (ident[0] == 'I' && ident[1] == 'D') {
                          ident += 2;
                          if ((ident[0] == '\0') ||
                              (ident[0] == 'R' && ident[1] == 'e' &&
                               ident[2] == 'f' && ident[3] == '\0')) {
                              isID = TRUE;
                          }
                      }
                  }
                  if (isID) {
#ifdef DEBUG_shaver
                      fprintf(stderr, "doing nsID for %s\n",
                              IDL_IDENT(type).str);
#endif
                      td->prefix.flags = TD_PNSIID | XPT_TDP_POINTER;
                      if(IDL_tree_property_get(type, "ref"))
                          td->prefix.flags |= XPT_TDP_REFERENCE;
                  } else {
#ifdef DEBUG_shaver
                      fprintf(stderr, "not doing nsID for %s\n",
                              IDL_IDENT(type).str);
#endif
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
                    return fill_td_from_type(state, td, new_type);
                }
                IDL_tree_error(type, "can't handle %s ident in param list\n",
#ifdef DEBUG_shaver
                        IDL_NODE_TYPE_NAME(IDL_NODE_UP(type))
#else
                        "that type of"
#endif
                        );
#ifdef DEBUG_shaver
                PR_ASSERT(0);
#endif
                return FALSE;
            }
            break;
          default:
            IDL_tree_error(type, "can't handle %s in param list\n",
#ifdef DEBUG_shaver
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

    if (IDL_tree_property_get(IDL_PARAM_DCL(tree).simple_declarator,
                              "shared")) {
        if (flags == XPT_PD_IN) {
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
                      gboolean getter)
{
#ifdef DEBUG_shaver_attr
    fprintf(stdout, "DBG: adding %cetter for %s\n",
            getter ? 'g' : 's', ATTR_IDENT(state->tree).str);
#endif
    if (!XPT_FillMethodDescriptor(meth, getter ? XPT_MD_GETTER : XPT_MD_SETTER,
                                  ATTR_IDENT(state->tree).str, 1) ||
        !fill_pd_from_type(state, meth->params,
                           getter ? (XPT_PD_RETVAL | XPT_PD_OUT) : XPT_PD_IN,
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
    gboolean ro = IDL_ATTR_DCL(state->tree).f_readonly;

    if (!XPT_InterfaceDescriptorAddMethods(id, ro ? 1 : 2))
        return FALSE;

    meth = &id->method_descriptors[NEXT_METH(state)];

    return typelib_attr_accessor(state, meth, TRUE) &&
        (ro || typelib_attr_accessor(state, meth + 1, FALSE));
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
    gboolean op_notxpcom = !!IDL_tree_property_get(op->ident, "notxpcom");
    gboolean op_noscript = !!IDL_tree_property_get(op->ident, "noscript");

    if (!XPT_InterfaceDescriptorAddMethods(id, 1))
        return FALSE;

    meth = &id->method_descriptors[NEXT_METH(state)];

    for (iter = op->parameter_dcls; iter; iter = IDL_LIST(iter).next)
        num_args++;             /* count params */
    if (op->op_type_spec && !op_notxpcom)
        num_args++;             /* fake param for _retval */
    if (op->f_noscript || op_noscript || op_notxpcom)
        op_flags |= XPT_MD_HIDDEN;
    if (op->f_varargs)
        op_flags |= XPT_MD_VARARGS;
    /* XXXshaver constructor? */

#ifdef DEBUG_shaver_method
    fprintf(stdout, "DBG: adding method %s (nargs %d)\n",
            IDL_IDENT(op->ident).str, num_args);
#endif
    if (!XPT_FillMethodDescriptor(meth, op_flags, IDL_IDENT(op->ident).str,
                                  num_args))
        return FALSE;

    for (num_args = 0, iter = op->parameter_dcls; iter;
         iter = IDL_LIST(iter).next, num_args++) {
        if (!fill_pd_from_param(state, &meth->params[num_args],
                                IDL_LIST(iter).data))
            return FALSE;
    }

    /* XXX unless [nonxpcom] */
    if (!op_notxpcom) {
        if (op->op_type_spec) {
            if (!fill_pd_from_type(state, &meth->params[num_args],
                                   XPT_PD_RETVAL | XPT_PD_OUT,
                                   op->op_type_spec))
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
    gboolean success;
    gboolean is_long;

    /* const -> list -> interface */
    if (!IDL_NODE_UP(IDL_NODE_UP(state->tree)) ||
        IDL_NODE_TYPE(IDL_NODE_UP(IDL_NODE_UP(state->tree)))
        != IDLN_INTERFACE) {
        IDL_tree_warning(state->tree, IDL_WARNING1,
                         "const decl \'%s\' not inside interface, ignored",
                         name);
        return TRUE;
    }

    success = (IDLN_TYPE_INTEGER == IDL_NODE_TYPE(dcl->const_type));
    if(success) {
        switch(IDL_TYPE_INTEGER(dcl->const_type).f_type) {
        case IDL_INTEGER_TYPE_SHORT:
            is_long = FALSE;
            break;
        case IDL_INTEGER_TYPE_LONG:
            is_long = TRUE;
            break;
        default:
            success = FALSE;
        }
    }

    if(success) {
        XPTInterfaceDescriptor *id;
        XPTConstDescriptor *cd;
        IDL_longlong_t value;
        gboolean sign;

        id = CURRENT(state);
        if (!XPT_InterfaceDescriptorAddConsts(id, 1))
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
            if(sign)
                cd->value.i32 = value;
            else
                cd->value.ui32 = value;
        } else {
            if(sign)
                cd->value.i16 = value;
            else
                cd->value.ui16 = value;
        }
        NEXT_CONST(state)++;
    } else {
        IDL_tree_warning(state->tree, IDL_WARNING1,
            "const decl \'%s\' was not of type short or long, ignored", name);
    }
    return TRUE;

#if 0
    XPTInterfaceDescriptor *id;
    XPTConstDescriptor *cd;
    struct _IDL_CONST_DCL *dcl = &IDL_CONST_DCL(state->tree);

    /* const -> list -> interface */
    if (IDL_NODE_TYPE(IDL_NODE_UP(IDL_NODE_UP(state->tree)))
        != IDLN_INTERFACE) {
        IDL_tree_warning(state->tree, IDL_WARNING1,
                         "const decl not inside interface!\n");
        return TRUE;
    }

    id = CURRENT(state);
    if (!XPT_InterfaceDescriptorAddConsts(id, 1))
        return FALSE;
    cd = &id->const_descriptors[NEXT_CONST(state)];

    cd->name = IDL_IDENT(dcl->ident).str;
#ifdef DEBUG_shaver_const
    fprintf(stderr, "DBG: adding const %s\n", cd->name);
#endif

    if (!fill_td_from_type(state, &cd->type, dcl->const_type))
        return FALSE;

    switch (IDL_NODE_TYPE(dcl->const_type)) {
      case IDLN_TYPE_INTEGER: {
          IDL_longlong_t value = IDL_INTEGER(dcl->const_exp).value;
          gboolean sign = IDL_TYPE_INTEGER(dcl->const_type).f_signed;
          switch(IDL_TYPE_INTEGER(dcl->const_type).f_type) {
            case IDL_INTEGER_TYPE_SHORT:
              if(sign)
                  cd->value.i16 = value;
              else
                  cd->value.ui16 = value;
              break;
            case IDL_INTEGER_TYPE_LONG:
              if(sign)
                  cd->value.i32 = value;
              else
                  cd->value.ui32 = value;
              break;
            case IDL_INTEGER_TYPE_LONGLONG:
              /* XXXshaver value -> PRInt64 not legal conversion? */
              if (sign)
                  LL_I2L(cd->value.i64, value);
              else
                  LL_UI2L(cd->value.ui64, value);
              break;
          }
          break;
      }
      case IDLN_TYPE_CHAR:
        cd->value.ch = IDL_CHAR(dcl->const_exp).value[0];
        PR_ASSERT(cd->value.ch);
        break;
      case IDLN_TYPE_WIDE_CHAR:
        cd->value.wch = IDL_WIDE_CHAR(dcl->const_exp).value[0];
        PR_ASSERT(cd->value.wch);
        break;
      case IDLN_TYPE_STRING:
        cd->value.string = XPT_NewStringZ(IDL_STRING(dcl->const_exp).value);
        if (!cd->value.string)
            return FALSE;
        break;
      case IDLN_TYPE_WIDE_STRING:
        PR_ASSERT(0);
        break;
      case IDLN_TYPE_BOOLEAN:
        cd->value.bul = IDL_BOOLEAN(dcl->const_exp).value;
        break;
      case IDLN_TYPE_FLOAT:
        cd->value.flt = (float)IDL_FLOAT(dcl->const_exp).value;
        break;
#if 0 /* IDL doesn't have double */
      case IDLN_TYPE_DOUBLE:
        cd->value.dbl = IDL_FLOAT(dcl->const_exp).value;
#endif
        break;
      case IDLN_IDENT:
        /* XXX check for nsID? */
        break;
      default:
        IDL_tree_error(state->tree, "illegal type for const\n");
        return FALSE;
    }

    NEXT_CONST(state)++;
    return TRUE;
#endif
}

static gboolean
typelib_enum(TreeState *state)
{
    IDL_tree_warning(state->tree, IDL_WARNING1,
                     "enums not supported, enum \'%s\' ignored",
                     IDL_IDENT(IDL_TYPE_ENUM(state->tree).ident).str);
    return TRUE;
}

nodeHandler *
xpidl_typelib_dispatch(void)
{
  static nodeHandler table[IDLN_LAST];
  static gboolean initialized = FALSE;

  if (!initialized) {
      /* Initialize non-NULL elements */
      table[IDLN_NONE] = pass_1;
      table[IDLN_LIST] = typelib_list;
      table[IDLN_ATTR_DCL] = typelib_attr_dcl;
      table[IDLN_OP_DCL] = typelib_op_dcl;
      table[IDLN_INTERFACE] = typelib_interface;
      table[IDLN_CONST_DCL] = typelib_const_dcl;
      table[IDLN_TYPE_ENUM] = typelib_enum;
      initialized = TRUE;
  }

  return table;
}
