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
                            IDL_NODE_TYPE(node) == IDLN_TYPE_WIDE_STRING ||   \
                            (IDL_NODE_TYPE(node) == IDLN_IDENT &&             \
                             UP_IS_AGGREGATE(node)))

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
    char *define = g_basename(state->basename);
    if (state->tree) {
        fprintf(state->file, "/*\n * DO NOT EDIT.  THIS FILE IS GENERATED FROM"
                " %s.idl\n */\n", state->basename);
        fprintf(state->file, "\n#ifndef __gen_%s_h__\n"
                "#define __gen_%s_h__\n",
                define, define);
        if (g_hash_table_size(state->includes)) {
            fputc('\n', state->file);
            g_hash_table_foreach(state->includes, write_header, state);
        }
        if (emit_js_stub_decls) {
            fputs("\n"
                  "#ifdef XPIDL_JS_STUBS\n"
                  "#include \"jsapi.h\"\n"
                  "#endif\n",
                  state->file);
        }
    } else {
        fprintf(state->file, "\n#endif /* __gen_%s_h__ */\n", define);
    }
    return TRUE;
}

static gboolean
write_classname_iid_define(FILE *file, const char *className)
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
    const char *name_space;

    fprintf(state->file,   "\n/* starting interface:    %s */\n",
            className);

#ifndef LIBIDL_MAJOR_VERSION
    /* pre-VERSIONing libIDLs put the attributes on the interface */
    name_space = IDL_tree_property_get(iface, "namespace");
    iid = IDL_tree_property_get(iface, "uuid");
#else
    /* post-VERSIONing (>= 0.5.11) put the attributes on the ident */
    name_space = IDL_tree_property_get(IDL_INTERFACE(iface).ident, "namespace");
    iid = IDL_tree_property_get(IDL_INTERFACE(iface).ident, "uuid");
#endif

    if (name_space) {
        fprintf(state->file, "/* namespace:             %s */\n",
                name_space);
        fprintf(state->file, "/* fully qualified name:  %s.%s */\n",
                name_space,className);
    }

    if (iid) {
        /* XXX use nsID parsing routines to validate? */
        if (strlen(iid) != 36)
            /* XXX report error */
            return FALSE;
        fprintf(state->file, "\n/* {%s} */\n#define ", iid);
        if (!write_classname_iid_define(state->file, className))
            return FALSE;
        fprintf(state->file, "_STR \"%s\"\n#define ", iid);
        if (!write_classname_iid_define(state->file, className))
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
        do {
            fprintf(state->file, "public %s",
                    IDL_IDENT(IDL_LIST(iter).data).str);
            if (IDL_LIST(iter).next)
                fputs(", ", state->file);
        } while ((iter = IDL_LIST(iter).next));
    }
    fputs(" {\n"
          " public: \n", state->file);
    if (iid) {
        fputs("  NS_DEFINE_STATIC_IID_ACCESSOR(", state->file);
        if (!write_classname_iid_define(state->file, className))
            return FALSE;
        fputs(")\n", state->file);
/* XX remove this old code... emitting the macro instead */
/*
        fputs("  static const nsIID& GetIID() {\n"
              "    static nsIID iid = ",
              state->file);
        if (!write_classname_iid_define(state->file, className))
            return FALSE;
        fputs(";\n    return iid;\n  }\n", state->file);
*/
    }

    state->tree = IDL_INTERFACE(iface).body;

    if (state->tree && !xpidl_process_node(state))
        return FALSE;

    /*
     * XXXbe keep this statement until -m stub dies
     * shaver sez: use the #ifdef to prevent pollution of non-stubbed headers.
     *
     * IMPORTANT: Only _static_ things can go here (inside the
     * #ifdef), or you lose the vtable invariance upon which so much
     * of (XP)COM is based.
     */
    if (emit_js_stub_decls) {
        fprintf(state->file,
            "\n"
            "#ifdef XPIDL_JS_STUBS\n"
            "  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);\n"
            "  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, %s *priv);\n"
            "#endif\n",
            className);
    }
    fputs("};\n", state->file);

    return TRUE;
}

static gboolean
list(TreeState *state)
{
    IDL_tree iter;
    for (iter = state->tree; iter; iter = IDL_LIST(iter).next) {
        state->tree = IDL_LIST(iter).data;
        if (!xpidl_process_node(state))
            return FALSE;
    }
    return TRUE;
}

/* XXXbe static */ gboolean
xpcom_type(TreeState *state)
{
    if (!state->tree) {
        fputs("void", state->file);
        return TRUE;
    }

    switch (IDL_NODE_TYPE(state->tree)) {
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
        fputs("PRUint16", state->file); /* wchar_t? */
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
      case IDLN_TYPE_OCTET:
        fputs("PRUint8", state->file);
        break;
      case IDLN_TYPE_FLOAT:
        switch (IDL_TYPE_FLOAT(state->tree).f_type) {
          case IDL_FLOAT_TYPE_FLOAT:
            fputs("float", state->file);
            break;
          case IDL_FLOAT_TYPE_DOUBLE:
            fputs("double", state->file);
            break;
          /* XXX 'long double' just ignored, or what? */
          default:
            fprintf(state->file, "unknown_type_%d", IDL_NODE_TYPE(state->tree));
            break;
        }
        break;
      case IDLN_IDENT:
        if (UP_IS_NATIVE(state->tree)) {
            fputs(IDL_NATIVE(IDL_NODE_UP(state->tree)).user_type, state->file);
            if (IDL_tree_property_get(state->tree, "ptr")) {
                fputs(" *", state->file);
            } else if (IDL_tree_property_get(state->tree, "ref")) {
                fputs(" &", state->file);
            }
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
    xpidl_write_comment(state, 2);
    return attr_accessor(state, TRUE) && (ro || attr_accessor(state, FALSE));
}

static gboolean
do_enum(TreeState *state)
{
    IDL_tree_warning(state->tree, IDL_WARNING1,
                     "enums not supported, enum \'%s\' ignored",
                     IDL_IDENT(IDL_TYPE_ENUM(state->tree).ident).str);
    return TRUE;
#if 0
    IDL_tree enumb = state->tree, iter;

    fprintf(state->file, "enum %s {\n",
            IDL_IDENT(IDL_TYPE_ENUM(enumb).ident).str);

    for (iter = IDL_TYPE_ENUM(enumb).enumerator_list;
         iter;
         iter = IDL_LIST(iter).next) {
        fprintf(state->file, "  %s%s\n", IDL_IDENT(IDL_LIST(iter).data).str,
                IDL_LIST(iter).next ? ",": "");
    }

    fputs("};\n\n", state->file);
    return TRUE;
#endif
}

static gboolean
do_const_dcl(TreeState *state)
{
    struct _IDL_CONST_DCL *dcl = &IDL_CONST_DCL(state->tree);
    const char *name = IDL_IDENT(dcl->ident).str;
    gboolean success;

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
        case IDL_INTEGER_TYPE_LONG:
            break;
        default:
            success = FALSE;
        }
    }

    if(success) {
        fprintf(state->file, "\n  enum { %s = %d };\n",
                             name, (int) IDL_INTEGER(dcl->const_exp).value);
    } else {
        IDL_tree_warning(state->tree, IDL_WARNING1,
            "const decl \'%s\' was not of type short or long, ignored", name);
    }
    return TRUE;
}

static gboolean
do_typedef(TreeState *state)
{
    IDL_tree type = IDL_TYPE_DCL(state->tree).type_spec,
        dcls = IDL_TYPE_DCL(state->tree).dcls,
        complex;

    if (IDL_NODE_TYPE(type) == IDLN_TYPE_SEQUENCE) {
        IDL_tree_warning(state->tree, IDL_WARNING1,
                         "sequences not supported, ignored");
    } else {
        state->tree = type;
        fputs("typedef ", state->file);
        if (!xpcom_type(state))
            return FALSE;
        fputs(" ", state->file);
        if (IDL_NODE_TYPE(complex = IDL_LIST(dcls).data) == IDLN_TYPE_ARRAY) {
            fprintf(state->file, "%s[%ld]",
                    IDL_IDENT(IDL_TYPE_ARRAY(complex).ident).str,
                 (long)IDL_INTEGER(IDL_LIST(IDL_TYPE_ARRAY(complex).size_list).
                                data).value);
        } else {
            fputs(" ", state->file);
            fputs(IDL_IDENT(IDL_LIST(dcls).data).str, state->file);
        }
        fputs(";\n", state->file);
    }
    return TRUE;
}

/*
 * param generation:
 * in string foo        -->     nsString *foo
 * out string foo       -->     nsString **foo;
 * inout string foo     -->     nsString **foo;
 */

/* XXXbe static */ gboolean
xpcom_param(TreeState *state)
{
    IDL_tree param = state->tree;
    state->tree = IDL_PARAM_DCL(param).param_type_spec;

    /* in string, wstring, explicitly marked [const], and nsid are const */
    if (IDL_PARAM_DCL(param).attr == IDL_PARAM_IN &&
        (IDL_NODE_TYPE(state->tree) == IDLN_TYPE_STRING ||
         IDL_NODE_TYPE(state->tree) == IDLN_TYPE_WIDE_STRING ||
         IDL_tree_property_get(IDL_OP_DCL(param).ident, "const") ||
         IDL_tree_property_get(state->tree, "nsid"))) {
        fputs("const ", state->file);
    }

    if (!xpcom_type(state))
        return FALSE;

    /* unless the type ended in a *, add a space */
    if (!STARRED_TYPE(state->tree))
        fputc(' ', state->file);

    /* out and inout params get a bonus *! */
    if (IDL_PARAM_DCL(param).attr != IDL_PARAM_IN)
        fputc('*', state->file);

    fputs(IDL_IDENT(IDL_PARAM_DCL(param).simple_declarator).str, state->file);
    return TRUE;
}

/*
 * A forward declaration, usually an interface.
 */
static gboolean
forward_dcl(TreeState *state)
{
    IDL_tree iface = state->tree;
    char *className = IDL_IDENT(IDL_FORWARD_DCL(iface).ident).str;
    if (!className)
        return FALSE;
    fprintf(state->file, "class %s; /* forward decl */\n", className);
    return TRUE;
}

/*
 * A method is an `operation', therefore a method decl is an `op dcl'.
 * I blame Elliot.
 */
static gboolean
op_dcl(TreeState *state)
{
    struct _IDL_OP_DCL *op = &IDL_OP_DCL(state->tree);
    gboolean op_notxpcom =
        (IDL_tree_property_get(op->ident, "notxpcom") != NULL);
    IDL_tree iter;

    xpidl_write_comment(state, 2);

    fputs("  ", state->file);
    if (op_notxpcom) {
        state->tree = op->op_type_spec;
        fputs("NS_IMETHOD_(", state->file);
        if (!xpcom_type(state))
            return FALSE;
        fputc(')', state->file);
    } else {
        fputs("NS_IMETHOD", state->file);
    }
    fprintf(state->file, " %s(", IDL_IDENT(op->ident).str);
    for (iter = op->parameter_dcls; iter; iter = IDL_LIST(iter).next) {
        state->tree = IDL_LIST(iter).data;
        if (!xpcom_param(state))
            return FALSE;
        if (IDL_LIST(iter).next || op->op_type_spec || op->f_varargs)
            fputs(", ", state->file);
    }

    /* make IDL return value into trailing out argument */
    if (op->op_type_spec && !op_notxpcom) {
        IDL_tree fake_param = IDL_param_dcl_new(IDL_PARAM_OUT,
                                                op->op_type_spec,
                                                IDL_ident_new("_retval"));
        if (!fake_param)
            return FALSE;
        state->tree = fake_param;
        if (!xpcom_param(state))
            return FALSE;
        if (op->f_varargs)
            fputs(", ", state->file);
    }

    /* varargs go last */
    if (op->f_varargs) {
        fputs("nsVarArgs *_varargs", state->file);
    }
    fputs(") = 0;\n", state->file);
    return TRUE;
}

static void
write_codefrag_line(gpointer data, gpointer user_data)
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
    g_slist_foreach(IDL_CODEFRAG(state->tree).lines, write_codefrag_line,
                    (gpointer)state);
    fputc('\n', state->file);
    return TRUE;
}

nodeHandler *
xpidl_header_dispatch(void)
{
    static nodeHandler table[IDLN_LAST];

    if (!table[IDLN_NONE]) {
        table[IDLN_NONE] = pass_1;
        table[IDLN_LIST] = list;
        table[IDLN_ATTR_DCL] = attr_dcl;
        table[IDLN_OP_DCL] = op_dcl;
        table[IDLN_FORWARD_DCL] = forward_dcl;
        table[IDLN_TYPE_ENUM] = do_enum;
        table[IDLN_INTERFACE] = interface;
        table[IDLN_CODEFRAG] = codefrag;
        table[IDLN_TYPE_DCL] = do_typedef;
        table[IDLN_CONST_DCL] = do_const_dcl;
    }

    return table;
}
