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
 * Generate XPCOM headers from XPIDL.
 */

#include "xpidl.h"
#include <ctype.h>

/* is this node from an aggregate type (interface)? */
#define UP_IS_AGGREGATE(node)                                                 \
    (IDL_NODE_UP(node) &&                                                     \
     IDL_NODE_TYPE(IDL_NODE_UP(node)) == IDLN_INTERFACE)

#define UP_IS_NATIVE(node)                                                    \
    (IDL_NODE_UP(node) &&                                                     \
     IDL_NODE_TYPE(IDL_NODE_UP(node)) == IDLN_NATIVE)

/* is this type output in the form "<foo> *"? */
#define STARRED_TYPE(node) (IDL_NODE_TYPE(node) == IDLN_TYPE_STRING ||        \
			    IDL_NODE_TYPE(node) == IDLN_TYPE_WIDE_STRING ||               \
			    (IDL_NODE_TYPE(node) == IDLN_IDENT &&                         \
			     UP_IS_AGGREGATE(node)) )

#define IDL_OUTPUT_FLAGS (IDLF_OUTPUT_NO_NEWLINES |                           \
                          IDLF_OUTPUT_NO_QUALIFY_IDENTS |                     \
                          IDLF_OUTPUT_PROPERTIES)

static void
dump_IDL(TreeState *state)
{
    IDL_tree_to_IDL(state->tree, state->ns, state->file, IDL_OUTPUT_FLAGS);
}

#define DUMP_IDL_COMMENT(state)                                               \
  fputs("\n  /* ", state->file);                                              \
  dump_IDL(state);                                                            \
  fputs(" */\n", state->file);

static void
write_header(gpointer key, gpointer value, gpointer user_data)
{
    char *ident = (char *)value;
    TreeState *state = (TreeState *)user_data;
    fprintf(state->file, "#include \"%s.h\" /* interface %s */\n",
            ident, ident);
}

static gboolean
pass_1(TreeState *state)
{
    if (state->tree) {
        char *define = g_basename(state->basename);
        fprintf(state->file, "/*\n * DO NOT EDIT.  THIS FILE IS GENERATED FROM"
                " %s.idl\n */\n", state->basename);
        fprintf(state->file, "\n#ifndef __gen_%s_h__\n"
                "#define __gen_%s_h__\n\n",
                define, define);
        if (g_hash_table_size(state->includes)) {
            g_hash_table_foreach(state->includes, write_header, state);
            fputc('\n', state->file);
        }
    } else {
        fprintf(state->file, "\n#endif /* __gen_%s_h__ */\n", state->basename);
    }
    return TRUE;
}

static gboolean
output_classname_iid_define(FILE *file, const char *className)
{
    const char *iidName;
    if (className[0] == 'n' && className[1] == 's') {
        /* backcompat naming styles */
        fputs("NS_", file);
        iidName = className + 2;
    } else {
        iidName = className;
    }
    while (*iidName)
        fputc(toupper(*iidName++), file);
    fputs("_IID", file);
    return TRUE;
}

static gboolean
interface(TreeState *state)
{
    IDL_tree iface = state->tree, iter;
    char *className = IDL_IDENT(IDL_INTERFACE(iface).ident).str;
    const char *iid;

    fprintf(state->file, "\n/* starting interface %s */\n",
            className);
    iid = IDL_tree_property_get(iface, "uuid");
    if (iid) {
        /* XXX use nsID parsing routines to validate? */
        if (strlen(iid) != 36)
            /* XXX report error */
            return FALSE;
        fprintf(state->file, "\n/* {%s} */\n#define ", iid);
        if (!output_classname_iid_define(state->file, className))
            return FALSE;
        fprintf(state->file, "_STR \"%s\"\n#define ", iid);
        if (!output_classname_iid_define(state->file, className))
            return FALSE;
        /* This is such a gross hack... */
        fprintf(state->file, " \\\n  {0x%.8s, 0x%.4s, 0x%.4s, \\\n    "
                "{ 0x%.2s, 0x%.2s, 0x%.2s, 0x%.2s, "
                "0x%.2s, 0x%.2s, 0x%.2s, 0x%.2s }}\n\n",
                iid, iid + 9, iid + 14, iid + 19, iid + 21, iid + 24,
                iid + 26, iid + 28, iid + 30, iid + 32, iid + 34);
    }
    fprintf(state->file, "class %s", className);
    if ((iter = IDL_INTERFACE(iface).inheritance_spec)) {
        fputs(" : ", state->file);
        for (; iter; iter = IDL_LIST(iter).next) {
            fprintf(state->file, "public %s",
                    IDL_IDENT(IDL_LIST(iter).data).str);
            if (IDL_LIST(iter).next)
                fputs(", ", state->file);
        }
    }
    fputs(" {\n"
          " public: \n"
          "  static const nsIID& IID() {\n"
          "    static nsIID iid = ",
          state->file);
    if (!output_classname_iid_define(state->file, className))
        return FALSE;
    fputs(";\n    return iid;\n  }\n", state->file);

    state->tree = IDL_INTERFACE(iface).body;

    if (state->tree && !process_node(state))
        return FALSE;

    fputs("};\n", state->file);

    return TRUE;
}

static gboolean
list(TreeState *state)
{
    IDL_tree iter;
    for (iter = state->tree; iter; iter = IDL_LIST(iter).next) {
        state->tree = IDL_LIST(iter).data;
        if (!process_node(state))
            return FALSE;
    }
    return TRUE;
}

static gboolean
xpcom_type(TreeState *state)
{
    if (!state->tree) {
        fputs("void", state->file);
        return TRUE;
    }

    switch(IDL_NODE_TYPE(state->tree)) {
      case IDLN_TYPE_INTEGER: {
          gboolean sign = IDL_TYPE_INTEGER(state->tree).f_signed;
          switch (IDL_TYPE_INTEGER(state->tree).f_type) {
            case IDL_INTEGER_TYPE_SHORT:
              fputs(sign ? "PRInt16" : "PRUint16", state->file);
              break;
            case IDL_INTEGER_TYPE_LONG:
              fputs(sign ? "PRInt32" : "PRUint32", state->file);
              break;
            case IDL_INTEGER_TYPE_LONGLONG:
              fputs(sign ? "PRInt64" : "PRUint64", state->file);
              break;
            default:
              g_error("Unknown integer type %d\n",
                      IDL_TYPE_INTEGER(state->tree).f_type);
              return FALSE;
          }
          break;
      }
      case IDLN_TYPE_CHAR:
        fputs("char", state->file);
        break;
      case IDLN_TYPE_WIDE_CHAR:
        fputs("PRUint16", state->file);	/* wchar_t? */
        break;
      case IDLN_TYPE_WIDE_STRING:
        fputs("PRUnichar *", state->file);
        break;
      case IDLN_TYPE_STRING:
        fputs("char *", state->file);
        break;
      case IDLN_TYPE_BOOLEAN:
        fputs("PRBool", state->file);
        break;
      case IDLN_IDENT:
        if (UP_IS_NATIVE(state->tree)) {
            fputs(IDL_NATIVE(IDL_NODE_UP(state->tree)).user_type, state->file);
        } else {
            fputs(IDL_IDENT(state->tree).str, state->file);
        }
        if (UP_IS_AGGREGATE(state->tree))
            fputs(" *", state->file);
        break;
      default:
        fprintf(state->file, "unknown_type_%d", IDL_NODE_TYPE(state->tree));
        break;
    }
    return TRUE;
}

static gboolean
type_integer(TreeState *state)
{
    IDL_tree p = state->tree;

    if (!IDL_TYPE_INTEGER(p).f_signed)
        fputs("unsigned ", state->file);

    switch(IDL_TYPE_INTEGER(p).f_type) {
      case IDL_INTEGER_TYPE_SHORT:
        printf("short");
        break;
      case IDL_INTEGER_TYPE_LONG:
        printf("long");
        break;
      case IDL_INTEGER_TYPE_LONGLONG:
        printf("long long");
        break;
    }
    return TRUE;
}

static gboolean
type(TreeState *state)
{
    if (!state->tree) {
        fputs("void", state->file);
        return TRUE;
    }

    switch(IDL_NODE_TYPE(state->tree)) {
      case IDLN_TYPE_INTEGER:
        return type_integer(state);
      case IDLN_TYPE_STRING:
        fputs("string", state->file);
        return TRUE;
      case IDLN_TYPE_WIDE_STRING:
        fputs("wstring", state->file);
        return TRUE;
      case IDLN_TYPE_CHAR:
        fputs("char", state->file);
        return TRUE;
      case IDLN_TYPE_WIDE_CHAR:
        fputs("wchar", state->file);
        return TRUE;
      case IDLN_TYPE_BOOLEAN:
        fputs("boolean", state->file);
        return TRUE;
      case IDLN_IDENT:
        fputs(IDL_IDENT(state->tree).str, state->file);
        break;
      default:
        fprintf(state->file, "unknown_type_%d", IDL_NODE_TYPE(state->tree));
        return TRUE;
    }

    return TRUE;
}

static gboolean
param_dcls(TreeState *state)
{
    IDL_tree iter;
    fputs("(", state->file);
    for (iter = state->tree; iter; iter = IDL_LIST(iter).next) {
        struct _IDL_PARAM_DCL decl = IDL_PARAM_DCL(IDL_LIST(iter).data);
        switch(decl.attr) {
          case IDL_PARAM_IN:
            fputs("in ", state->file);
            break;
          case IDL_PARAM_OUT:
            fputs("out ", state->file);
            break;
          case IDL_PARAM_INOUT:
            fputs("inout ", state->file);
            break;
          default:;
        }
        state->tree = (IDL_tree)decl.param_type_spec;
        if (!type(state))
            return FALSE;
        fputs(" ", state->file);
        state->tree = (IDL_tree)decl.simple_declarator;
        if (!process_node(state))
            return FALSE;
        if (IDL_LIST(iter).next)
            fputs(", ", state->file);
    }
    return TRUE;
}

/*
 * An attribute declaration looks like:
 *
 * [ IDL_ATTR_DCL]
 *   - param_type_spec [IDL_TYPE_* or NULL for void]
 *   - simple_declarations [IDL_LIST]
 *     - data [IDL_IDENT]
 *     - next [IDL_LIST or NULL if no more idents]
 *       - data [IDL_IDENT]
 */

#define ATTR_IDENT(tree) (IDL_IDENT(IDL_LIST(IDL_ATTR_DCL(tree).simple_declarations).data))
#define ATTR_TYPE_DECL(tree) (IDL_ATTR_DCL(tree).param_type_spec)
#define ATTR_TYPE(tree) (IDL_NODE_TYPE(ATTR_TYPE_DECL(tree)))

static gboolean
attr_accessor(TreeState *state, gboolean getter)
{
    char *attrname = ATTR_IDENT(state->tree).str;
        IDL_tree orig = state->tree;
        fprintf(state->file, "  NS_IMETHOD %cet%c%s(",
                getter ? 'G' : 'S',
                toupper(attrname[0]), attrname + 1);
        state->tree = ATTR_TYPE_DECL(state->tree);
        if (!xpcom_type(state))
            return FALSE;
        state->tree = orig;
        fprintf(state->file, "%s%sa%c%s) = 0;\n",
                (STARRED_TYPE(orig) ? "" : " "),
                getter ? "*" : "",
                toupper(attrname[0]), attrname + 1);
    return TRUE;
}

static gboolean
attr_dcl(TreeState *state)
{
    gboolean ro = IDL_ATTR_DCL(state->tree).f_readonly;
    DUMP_IDL_COMMENT(state);
    return attr_accessor(state, TRUE) && (ro || attr_accessor(state, FALSE));
}

static gboolean
do_enum(TreeState *state)
{
    IDL_tree enumb = state->tree, iter;
    
    fprintf(state->file, "enum %s {\n",
            IDL_IDENT(IDL_TYPE_ENUM(enumb).ident).str);

    for (iter = IDL_TYPE_ENUM(enumb).enumerator_list;
         iter; iter = IDL_LIST(iter).next)
        fprintf(state->file, "  %s%s\n", IDL_IDENT(IDL_LIST(iter).data).str,
                IDL_LIST(iter).next ? ",": "");

    fputs("};\n\n", state->file);
    return TRUE;
}

static gboolean
do_typedef(TreeState *state)
{
    IDL_tree type = IDL_TYPE_DCL(state->tree).type_spec,
        dcls = IDL_TYPE_DCL(state->tree).dcls,
        complex;
    fputs("typedef ", state->file);
    fputs(" ", state->file);

    if (IDL_NODE_TYPE(type) == IDLN_TYPE_SEQUENCE) {
        fprintf(stderr, "SEQUENCE!\n");
    } else {
        state->tree = type;
        if (!xpcom_type(state))
            return FALSE;
        if (IDL_NODE_TYPE(complex = IDL_LIST(dcls).data) == IDLN_TYPE_ARRAY) {
            fprintf(state->file, "%s[%ld]",
                    IDL_IDENT(IDL_TYPE_ARRAY(complex).ident).str,
                 (long)IDL_INTEGER(IDL_LIST(IDL_TYPE_ARRAY(complex).size_list).
                                data).value);
        } else {
            fputs(IDL_IDENT(IDL_LIST(dcls).data).str, state->file);
        }
    }
    fputs(";\n", state->file);
    return TRUE;
}

/*
 * param generation:
 * in string foo	    -->     nsString *foo
 * out string foo       -->     nsString **foo;
 * inout string foo     -->     nsString **foo;
 */

static gboolean
xpcom_param(TreeState *state)
{
    IDL_tree param = state->tree;
    state->tree = IDL_PARAM_DCL(param).param_type_spec;

    if (!xpcom_type(state))
        return FALSE;
    fprintf(state->file, "%s%s",
            STARRED_TYPE(state->tree)  ? "" : " ",
            IDL_PARAM_DCL(param).attr == IDL_PARAM_IN ? "" : "*");
    fprintf(state->file, "%s",
            IDL_IDENT(IDL_PARAM_DCL(param).simple_declarator).str);
    return TRUE;
}

/*
 * A method is an `operation', therefore a method decl is an `op dcl'.
 * I blame Elliot.
 */
static gboolean
op_dcl(TreeState *state)
{
    struct _IDL_OP_DCL op = IDL_OP_DCL(state->tree);
    IDL_tree iter;

    DUMP_IDL_COMMENT(state);

    fprintf(state->file, "  NS_IMETHOD %s(", IDL_IDENT(op.ident).str);
    for (iter = op.parameter_dcls; iter; iter = IDL_LIST(iter).next) {
        state->tree = IDL_LIST(iter).data;
        if (!xpcom_param(state))
            return FALSE;
        if (IDL_LIST(iter).next || op.op_type_spec || op.f_varargs)
            fputs(", ", state->file);
    }

    /* make IDL return value into trailing out argument */
    if (op.op_type_spec) {
        IDL_tree fake_param = IDL_param_dcl_new(IDL_PARAM_OUT,
                                                op.op_type_spec,
                                                IDL_ident_new("_retval"));
        if (!fake_param)
            return FALSE;
        state->tree = fake_param;
        if (!xpcom_param(state))
            return FALSE;
        if (op.f_varargs)
            fputs(", ", state->file);
    }

    /* varargs go last */
    if (op.f_varargs) {
        fputs("nsVarArgs *_varargs", state->file);
    }
    fputs(") = 0;\n", state->file);
    return TRUE;
}

static void
dump_codefrag_line(gpointer data, gpointer user_data)
{
    TreeState *state = (TreeState *)user_data;
    char *line = (char *)data;
    fputs(line, state->file);
    fputc('\n', state->file);
}

static gboolean
codefrag(TreeState *state)
{
    if (strcmp(IDL_CODEFRAG(state->tree).desc, "C++"))
        return TRUE;
    g_slist_foreach(IDL_CODEFRAG(state->tree).lines, dump_codefrag_line,
                    (gpointer)state);
    fputc('\n', state->file);
    return TRUE;
}

nodeHandler *headerDispatch()
{
    static nodeHandler table[IDLN_LAST];
    static gboolean initialized = FALSE;

    if (!initialized) {
        table[IDLN_NONE] = pass_1;
        table[IDLN_LIST] = list;
        table[IDLN_ATTR_DCL] = attr_dcl;
        table[IDLN_OP_DCL] = op_dcl;
        table[IDLN_PARAM_DCL] = param_dcls;
        table[IDLN_TYPE_ENUM] = do_enum;
        table[IDLN_INTERFACE] = interface;
        table[IDLN_CODEFRAG] = codefrag;
        table[IDLN_TYPE_DCL] = do_typedef;
        initialized = TRUE;
    }
  
    return table;  
}
