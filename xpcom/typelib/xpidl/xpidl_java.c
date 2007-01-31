/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Allen (michael.allen@sun.com)
 *   Frank Mitchell (frank.mitchell@sun.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Generate Java interfaces from XPIDL.
 */

#include "xpidl.h"
#include <ctype.h>
#include <glib.h>

#ifdef XP_WIN
#include <windef.h>
#define PATH_MAX  MAX_PATH
#elif defined(XP_OS2) && !defined(PATH_MAX)
#include <os2.h>
#define PATH_MAX  CCHMAXPATH
#endif

/* XXX Bug 331178 - nsIScriptSecurityManager inherits from non-scriptable
 * interface nsIXPCSecurityManager. To work around that, we write out a
 * Java interface for nsIXPCSecurityManager, but don't give it any methods.
 */
#define CREATE_NSIXPCSECURITYMANAGER

/* XXX Bug 324035 - XPIDL cannot tell whether a forward declared interface is
 * itself defined in an IDL file (in which case XPIDL will have also written
 * out a Java interface file) or if it is just defined in a C header (in which
 * case there will be an error when trying to compile the Java interfaces).
 * This workaround lists the infringing interfaces and writes out "nsISupports"
 * when encountering those types.
 */
#define HANDLE_NON_IDL_IFACES

/* XXX Bug 340009 - SWT depends on several [noscript] methods in order to
 * embed Mozilla in an SWT view.  This hack makes those methods available
 * to Java embedders.
 */
#define OUTPUT_SWT_NOSCRIPT_METHODS

/* XXX If an interface method throws an exception, how do we handle it? */
/*#define HANDLE_EXCEPTIONS*/


static char* subscriptIdentifier(TreeState *state, char *str);

static char* javaKeywords[] = {
  "abstract", "default"  , "if"        , "private"     , "throw"       ,
  "boolean" , "do"       , "implements", "protected"   , "throws"      ,
  "break"   , "double"   , "import",     "public"      , "transient"   ,
  "byte"    , "else"     , "instanceof", "return"      , "try"         ,
  "case"    , "extends"  , "int"       , "short"       , "void"        ,
  "catch"   , "final"    , "interface" , "static"      , "volatile"    ,
  "char"    , "finally"  , "long"      , "super"       , "while"       ,
  "class"   , "float"    , "native"    , "switch"      ,
  "const"   , "for"      , "new"       , "synchronized",
  "continue", "goto"     , "package"   , "this"        ,
    /* added in Java 1.2 */
  "strictfp",
    /* added in Java 1.4 */
  "assert"  ,
    /* added in Java 5.0 */
  "enum"    ,
    /* Java constants */
  "true"    , "false"    , "null"      ,
    /* java.lang.Object methods                                           *
     *    - don't worry about "toString", since it does the same thing    *
     *      as Object's "toString"                                        */
  "clone"   , "equals"   , "finalize"  , "getClass"    , "hashCode"    ,
  "notify"  , "notifyAll", /*"toString"  ,*/ "wait"
};

#ifdef HANDLE_NON_IDL_IFACES
static char* nonIDLIfaces[] = {
    "nsIPresShell",
    "nsIDocument",
    "nsIObjectFrame",
    "nsObjectFrame",
    "nsIFrame",
    "nsIContent",
    "nsILayoutHistoryState",
    "nsIMdbEnv",
    "nsIMdbTable",
    "nsIMdbRow",
    "nsIChannelSecurityInfo",
    "nsIUnicodeDecoder",
    "nsIUnicodeEncoder",
    "nsIServiceManagerObsolete",
    "nsIWordBreaker",
    "nsISecureEnv",
    "nsIScrollbarMediator",
    "nsIScriptContext",
    "nsIScriptGlobalObject",
    "nsIScriptElement",
    "nsIFrameSelection",
    "nsIWidget",
    "nsIMenuItem"
};
#define NONIDLS(state)      (((struct java_priv_data *)state->priv)->nonIDLIfaces)
#endif

#define TYPEDEFS(state)     (((struct java_priv_data *)state->priv)->typedefTable)
#define PRIVDATA(state)     (((struct java_priv_data *)state->priv))
#define KEYWORDS(state)     (((struct java_priv_data *)state->priv)->keywords)

static void
write_indent(FILE *outfile) {
    fputs("  ", outfile);
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

    while (*iidName) {
        fputc(toupper(*iidName++), file);
    }

    fputs("_IID", file);
    return TRUE;
}

static gboolean
java_prolog(TreeState *state)
{
    int len, i;
    state->priv = calloc(1, sizeof(struct java_priv_data));
    if (!state->priv)
        return FALSE;

    TYPEDEFS(state) = 0;
    TYPEDEFS(state) = g_hash_table_new(g_str_hash, g_str_equal);
    if (!TYPEDEFS(state)) {
        /* XXX report error */
        free(state->priv);
        return FALSE;
    }

    KEYWORDS(state) = 0;
    KEYWORDS(state) = g_hash_table_new(g_str_hash, g_str_equal);
    if (!KEYWORDS(state)) {
        g_hash_table_destroy(TYPEDEFS(state));
        free(state->priv);
        return FALSE;
    }
    len = sizeof(javaKeywords)/sizeof(*javaKeywords);
    for (i = 0; i < len; i++) {
        g_hash_table_insert(KEYWORDS(state),
                            javaKeywords[i],
                            javaKeywords[i]);
    }

#ifdef HANDLE_NON_IDL_IFACES
    NONIDLS(state) = 0;
    NONIDLS(state) = g_hash_table_new(g_str_hash, g_str_equal);
    if (!NONIDLS(state)) {
        g_hash_table_destroy(TYPEDEFS(state));
        free(state->priv);
        return FALSE;
    }
    len = sizeof(nonIDLIfaces)/sizeof(*nonIDLIfaces);
    for (i = 0; i < len; i++) {
        g_hash_table_insert(NONIDLS(state),
                            nonIDLIfaces[i],
                            nonIDLIfaces[i]);
    }
#endif

    return TRUE;
}

static gboolean 
java_epilog(TreeState *state)
{
    /* points to other elements of the tree, so just destroy the table */
    g_hash_table_destroy(TYPEDEFS(state));
    g_hash_table_destroy(KEYWORDS(state));
#ifdef HANDLE_NON_IDL_IFACES
    g_hash_table_destroy(NONIDLS(state));
#endif

    free(state->priv);
    state->priv = NULL;
    
    return TRUE;
}

static gboolean
forward_declaration(TreeState *state) 
{
    /*
     * Java doesn't need forward declarations unless the declared 
     * class resides in a different package.
     */
#if 0
    IDL_tree iface = state->tree;
    const char *className = IDL_IDENT(IDL_FORWARD_DCL(iface).ident).str;
    const char *pkgName = "org.mozilla.xpcom";
    if (!className)
        return FALSE;
    /* XXX: Get package name and compare */
    fprintf(state->file, "import %s.%s;\n", pkgName, className);
#endif

    return TRUE;
}


static gboolean
interface_declaration(TreeState *state) 
{
    char outname[PATH_MAX];
    char* p;
    IDL_tree interface = state->tree;
    IDL_tree iterator = NULL;
    char *interface_name = 
      subscriptIdentifier(state, IDL_IDENT(IDL_INTERFACE(interface).ident).str);
    const char *iid = NULL;
    char iid_parsed[UUID_LENGTH];
    GSList *doc_comments = IDL_IDENT(IDL_INTERFACE(interface).ident).comments;

    if (!verify_interface_declaration(interface))
        return FALSE;

    /*
     * We only want to output scriptable interfaces
     */
    if (!IDL_tree_property_get(IDL_INTERFACE(interface).ident, "scriptable")) {
        /*
         * XXX SWT uses non-scriptable interface 'nsIAppShell' (bug 270892), so
         * include that one.
         */
        if (strcmp(interface_name, "nsIAppShell") != 0
#ifdef CREATE_NSIXPCSECURITYMANAGER
            && strcmp(interface_name, "nsIXPCSecurityManager") != 0
#endif
            )
            return TRUE;
    }

    /*
     * Each Java interface must be in its own file.
     */
    p = strrchr(state->filename, '/');
    if (p) {
        strncpy(outname, state->filename, p + 1 - state->filename);
        outname[p + 1 - state->filename] = '\0';
    }
    strcat(outname, interface_name);
    strcat(outname, ".java");

    state->file = fopen(outname, "w");
    if (!state->file) {
        perror("error opening output file");
        return FALSE;
    }

    fprintf(state->file, "/*\n * DO NOT EDIT.  THIS FILE IS GENERATED FROM\n"
            " * %s.idl\n */\n", state->basename);

    /* package name (namespace) */
    fputs("\npackage org.mozilla.interfaces;\n\n", state->file);


    iid = IDL_tree_property_get(IDL_INTERFACE(interface).ident, "uuid");
    if (iid) {
        /*
         * Parse uuid and then output resulting nsID to string, to
         * validate uuid.
         */
        struct nsID id;
        if (!xpidl_parse_iid(&id, iid)) {
            IDL_tree_error(state->tree, "cannot parse IID %s\n", iid);
            return FALSE;
        }
        if (!xpidl_sprint_iid(&id, iid_parsed)) {
            IDL_tree_error(state->tree, "error formatting IID %s\n", iid);
            return FALSE;
        }
    } else {
        IDL_tree_error(state->tree, "interface %s lacks a uuid attribute\n", 
                       interface_name);
        return FALSE;
    }

    /*
     * Write any interface comments
     */
    if (doc_comments != NULL)
        printlist(state->file, doc_comments);

    /*
     * Write "public interface <foo>"
     */
    fprintf(state->file, "public interface %s", interface_name);

    /*
     * Check for inheritence, and iterator over the inherited names,
     * if any.
     */

    if ((iterator = IDL_INTERFACE(interface).inheritance_spec)) {
        fputs(" extends ", state->file);

        do {
            fprintf(state->file, "%s", 
                    IDL_IDENT(IDL_LIST(iterator).data).str);
        
            if (IDL_LIST(iterator).next) {
                fputs(", ", state->file);
            }
        } while ((iterator = IDL_LIST(iterator).next));
    }

    fputs(" {\n\n", state->file);

    /*
     * Write interface constants for IID
     */
    if (iid) {
        /* String NS_ISUPPORTS_IID = "{00000000-0000-0000-c000-000000000046}";*/
        write_indent(state->file);
        fputs("String ", state->file);
        write_classname_iid_define(state->file, interface_name);
        fputs(" =\n", state->file);
        write_indent(state->file);
        write_indent(state->file);
        fprintf(state->file, "\"{%s}\";\n\n", iid_parsed);
    }

    /*
     * Advance the state of the tree, go on to process more
     */
    
    state->tree = IDL_INTERFACE(interface).body;

    if (state->tree && !xpidl_process_node(state)) {
        return FALSE;
    }


    fputs("}", state->file);

    fclose(state->file);

    return TRUE;
}

static gboolean
process_list(TreeState *state)
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
xpcom_to_java_type(TreeState *state, IDL_tree param) 
{
    IDL_tree real_type, type;
    IDL_tree up;

    if (!state->tree) {
        fputs("Object", state->file);
        return TRUE;
    }

    /* Could be a typedef; try to map it to the real type */
    real_type = find_underlying_type(state->tree);
    type = real_type ? real_type : state->tree;

    switch(IDL_NODE_TYPE(type)) {

    case IDLN_TYPE_INTEGER: {

        switch(IDL_TYPE_INTEGER(type).f_type) {

        case IDL_INTEGER_TYPE_SHORT:
            if (IDL_TYPE_INTEGER(type).f_signed)
                fputs("short", state->file);
            else
                fputs("int", state->file);
            break;

        case IDL_INTEGER_TYPE_LONG:
            if (IDL_TYPE_INTEGER(type).f_signed)
                fputs("int", state->file);
            else
                fputs("long", state->file);
            break;

        case IDL_INTEGER_TYPE_LONGLONG:
            if (IDL_TYPE_INTEGER(type).f_signed)
                fputs("long", state->file);
            else
                fputs("double", state->file);
            break;
        
        default:
            g_error("   Unknown integer type: %d\n",
                    IDL_TYPE_INTEGER(type).f_type);
            return FALSE;

        }

        break;
    }

    case IDLN_TYPE_CHAR:
    case IDLN_TYPE_WIDE_CHAR:
        fputs("char", state->file);
        break;

    case IDLN_TYPE_WIDE_STRING:
    case IDLN_TYPE_STRING:
        fputs("String", state->file);
        break;

    case IDLN_TYPE_BOOLEAN:
        fputs("boolean", state->file);
        break;

    case IDLN_TYPE_OCTET:
        /* If real type is 'PRUint8', promote to 'short' */
        if (IDL_NODE_TYPE(state->tree) == IDLN_IDENT) {
            if (strcmp(IDL_IDENT(state->tree).str, "PRUint8") == 0) {
                fputs("short", state->file);
                break;
            }
        }
        fputs("byte", state->file);
        break;

    case IDLN_TYPE_FLOAT:
        switch(IDL_TYPE_FLOAT(type).f_type) {

        case IDL_FLOAT_TYPE_FLOAT:
            fputs("float", state->file);
            break;

        case IDL_FLOAT_TYPE_DOUBLE:
            fputs("double", state->file);
            break;
        
        default:
            g_error("    Unknown floating point typ: %d\n",
                    IDL_NODE_TYPE(type));
            break;
        }
        break;


    case IDLN_IDENT:
      if (!(up = IDL_NODE_UP(type))) {
          IDL_tree_error(state->tree,
                         "ERROR: orphan ident %s in param list\n",
                         IDL_IDENT(state->tree).str);
          return FALSE;
      }
      switch (IDL_NODE_TYPE(up)) {
        case IDLN_FORWARD_DCL:
        case IDLN_INTERFACE: {
          char *className;
          const char *iid_is;
handle_iid_is:
          /* might get here via the goto, so re-check type */
          if (IDL_NODE_TYPE(up) == IDLN_INTERFACE)
              className = IDL_IDENT(IDL_INTERFACE(up).ident).str;
          else if (IDL_NODE_TYPE(up) == IDLN_FORWARD_DCL)
              className = IDL_IDENT(IDL_FORWARD_DCL(up).ident).str;
          else
              className = IDL_IDENT(IDL_NATIVE(up).ident).str;

          iid_is = NULL;
          if (IDL_NODE_TYPE(state->tree) == IDLN_PARAM_DCL) {
              IDL_tree simple = IDL_PARAM_DCL(state->tree).simple_declarator;
              iid_is = IDL_tree_property_get(simple, "iid_is");
          }

          if (iid_is) {
              fputs("nsISupports", state->file);
          } else {
              /*
               * In JavaXPCOM, we handle weak references internally; no need
               * for the |nsIWeakReference| interface.  So just return
               * |nsISupports|.
               */
              if (strcmp(className, "nsIWeakReference") == 0) {
                  fputs("nsISupports", state->file);
              } else {
#ifdef HANDLE_NON_IDL_IFACES
                  char *nonidl = g_hash_table_lookup(NONIDLS(state), className);
                  if (nonidl) {
                      fputs("nsISupports", state->file);
                  } else
#endif
                  {
                      fprintf(state->file, "%s", className);
                  }
              }
          }
          break;
        }
        case IDLN_NATIVE: {
            char *ident;

            /* jband - adding goto for iid_is when type is native */
            if (param) {
                if (IDL_NODE_TYPE(param) == IDLN_PARAM_DCL &&
                    IDL_tree_property_get(IDL_PARAM_DCL(param).simple_declarator,
                                          "iid_is"))
                {
                    state->tree = param;
                    goto handle_iid_is;
                }
            }

            ident = IDL_IDENT(type).str;
            if (IDL_tree_property_get(type, "nsid")) {
                fputs("String", state->file);
            } else if (IDL_tree_property_get(type, "domstring")) {
                fputs("String", state->file);
            } else if (IDL_tree_property_get(type, "astring")) {
                fputs("String", state->file);
            } else if (IDL_tree_property_get(type, "utf8string")) {
                fputs("String", state->file);
            } else if (IDL_tree_property_get(type, "cstring")) {
                fputs("String", state->file);
            } else {
                const char* user_type = IDL_NATIVE(IDL_NODE_UP(type)).user_type;
                IDL_tree real_type = 
                    g_hash_table_lookup(TYPEDEFS(state), user_type);

                if (real_type) {
                    gboolean rc;
                    IDL_tree orig_tree = state->tree;

                    state->tree = real_type;
                    rc = xpcom_to_java_type(state, param);

                    state->tree = orig_tree;
                    return rc;
                } else {
                    if (strcmp(user_type, "PRInt8") == 0) {
                        fputs("byte", state->file);
                    } else if (strcmp(user_type, "PRInt16") == 0 ||
                               strcmp(user_type, "PRUint8") == 0) {
                        fputs("short", state->file);
                    } else if (strcmp(user_type, "PRInt32") == 0 ||
                               strcmp(user_type, "int") == 0 ||
                               strcmp(user_type, "PRUint16") == 0) {
                        fputs("int", state->file);
                    } else if (strcmp(user_type, "PRInt64") == 0 ||
                               strcmp(user_type, "PRUint32") == 0) {
                        fputs("long", state->file);
                    } else if (strcmp(user_type, "PRUint64") == 0) {
                        fputs("double", state->file);
                    } else if (strcmp(user_type, "PRBool") == 0) {
                        fputs("boolean", state->file);
                    } else if (strcmp(user_type, "char") == 0 ||
                               strcmp(user_type, "const char") == 0 ||
                               strcmp(user_type, "unsigned char") == 0) {
                        if (IDL_tree_property_get(type, "ptr")) {
                            fputs("byte[]", state->file);
                        } else {
                            fputs("char", state->file);
                        }
                    } else if (strcmp(user_type, "nsIID") == 0) {
                        fputs("String", state->file);
                    } else if (strcmp(user_type, "nsString") == 0 ||
                               strcmp(user_type, "nsAString") == 0 ||
                               strcmp(user_type, "nsACString") == 0) {
                        fputs("String", state->file);
                    } else {
                        fputs("long", state->file);
                    }
                }
            }
            break;
          }
        default:
          if (IDL_NODE_TYPE(IDL_NODE_UP(up)) == IDLN_TYPE_DCL) {
              /* restart with the underlying type */
              gboolean rc;
              IDL_tree orig_tree = state->tree;
              state->tree = IDL_TYPE_DCL(IDL_NODE_UP(up)).type_spec;
              rc = xpcom_to_java_type(state, param);
              state->tree = orig_tree;
              return rc;
          }
          IDL_tree_error(state->tree,
                         "can't handle %s ident in param list\n",
                         "that type of"
                         );
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

    return TRUE;

}

static gboolean
xpcom_to_java_param(TreeState *state)
{
    IDL_tree param = state->tree;
    state->tree = IDL_PARAM_DCL(param).param_type_spec;

    /*
     * Put in type of parameter
     */

    if (!xpcom_to_java_type(state, param)) {
        return FALSE;
    }

    /*
     * If the parameter is out or inout, make it a Java array of the
     * appropriate type
     */

    if (IDL_PARAM_DCL(param).attr != IDL_PARAM_IN) {
        fputs("[]", state->file);
    }

    /*
     * If the parameter is an array make it a Java array
     */
    if (IDL_tree_property_get(IDL_PARAM_DCL(param).simple_declarator, "array"))
        fputs("[]", state->file);

    /*
     * Put in name of parameter 
     */
    fputc(' ', state->file);
    fputs(subscriptIdentifier(state, 
                        IDL_IDENT(IDL_PARAM_DCL(param).simple_declarator).str), 
          state->file);

    return TRUE;
}


static gboolean
type_declaration(TreeState *state) 
{
    /*
     * Unlike C, Java has no type declaration directive.
     * Instead, we record the mapping, and look up the actual type
     * when needed.
     */
    IDL_tree type = IDL_TYPE_DCL(state->tree).type_spec;
    IDL_tree dcls = IDL_TYPE_DCL(state->tree).dcls;

    /* XXX: check for illegal types */

    g_hash_table_insert(TYPEDEFS(state),
                        IDL_IDENT(IDL_LIST(dcls).data).str,
                        type);

    return TRUE;
}

#ifdef OUTPUT_SWT_NOSCRIPT_METHODS
static gboolean
print_noscript_method(TreeState *state)
{
    IDL_tree iface = IDL_NODE_UP(IDL_NODE_UP(state->tree));
    char *className = IDL_IDENT(IDL_INTERFACE(iface).ident).str;
    if (strcmp(className, "nsIBaseWindow") == 0 ||
        strcmp(className, "nsIEmbeddingSiteWindow") == 0)
        return TRUE;
    return FALSE;
}
#endif

static gboolean
method_declaration(TreeState *state) 
{
    const char* array = NULL;
    struct _IDL_OP_DCL *method = &IDL_OP_DCL(state->tree);
    gboolean method_notxpcom = 
        (IDL_tree_property_get(method->ident, "notxpcom") != NULL);
    gboolean method_noscript = 
        (IDL_tree_property_get(method->ident, "noscript") != NULL);
    IDL_tree iterator = NULL;
    IDL_tree retval_param = NULL;
    char *method_name =
                  g_strdup_printf("%c%s",
                                  tolower(IDL_IDENT(method->ident).str[0]),
                                  IDL_IDENT(method->ident).str + 1);
    GSList *doc_comments = IDL_IDENT(method->ident).comments;

    if (!verify_method_declaration(state->tree))
        return FALSE;

#ifdef OUTPUT_SWT_NOSCRIPT_METHODS
    if (method_notxpcom)
        return TRUE;
    if (method_noscript && !print_noscript_method(state))
        return TRUE;
#else
    /* do not write nonscriptable methods */
    if (method_notxpcom || method_noscript) {
        return TRUE;
    }
#endif

#ifdef CREATE_NSIXPCSECURITYMANAGER
    /* Since this interface is non-scriptable, we treat all of its methods
     * as if they were [noscript] */
    {
        IDL_tree iface = IDL_NODE_UP(IDL_NODE_UP(state->tree));
        char *className = IDL_IDENT(IDL_INTERFACE(iface).ident).str;
        if (strcmp(className, "nsIXPCSecurityManager") == 0)
            return TRUE;
    }
#endif

    if (doc_comments != NULL) {
        write_indent(state->file);
        printlist(state->file, doc_comments);
    }

    /*
     * Write return type
     * Unlike C++ headers, Java interfaces return the declared 
     * return value; an exception indicates XPCOM method failure.
     */
    write_indent(state->file);
    if (method->op_type_spec) {
        state->tree = method->op_type_spec;
        if (!xpcom_to_java_type(state, NULL)) {
            return FALSE;
        }
    } else {
        /* Check for retval attribute */
        for (iterator = method->parameter_dcls; iterator != NULL; 
             iterator = IDL_LIST(iterator).next) {

            IDL_tree original_tree = state->tree;
            IDL_tree simple_decl;

            state->tree = IDL_LIST(iterator).data;
            simple_decl = IDL_PARAM_DCL(state->tree).simple_declarator;

            if (IDL_tree_property_get(simple_decl, "retval")) {
                IDL_tree param;

                retval_param = iterator;
                array = IDL_tree_property_get(simple_decl, "array");

                /*
                 * Put in type of parameter
                 */
                param = state->tree;
                state->tree = IDL_PARAM_DCL(state->tree).param_type_spec;
                if (!xpcom_to_java_type(state, param)) {
                    return FALSE;
                }

                if (array) {
                    fputs("[]", state->file);
                }
            }

            state->tree = original_tree;
        }

        if (retval_param == NULL) {
            fputs("void", state->file);
        }
    }
 
    /*
     * Write method name
     */
    fprintf(state->file, " %s(", subscriptIdentifier(state, method_name));

    /*
     * Write parameters
     */
    for (iterator = method->parameter_dcls; iterator != NULL; 
         iterator = IDL_LIST(iterator).next) {

        /* Skip "retval" */
        if (iterator == retval_param) {
            continue;
        }

        if (iterator != method->parameter_dcls) {
            fputs(", ", state->file);
        }
        
        state->tree = IDL_LIST(iterator).data;

        if (!xpcom_to_java_param(state)) {
            return FALSE;
        }
    }

    fputs(")", state->file);

#ifdef HANDLE_EXCEPTIONS
    if (method->raises_expr) {
        IDL_tree iter = method->raises_expr;
        IDL_tree dataNode = IDL_LIST(iter).data;

        fputs(" throws ", state->file);
        fputs(IDL_IDENT(dataNode).str, state->file);
        iter = IDL_LIST(iter).next;

        while (iter) {
            dataNode = IDL_LIST(iter).data;
            fprintf(state->file, ", %s", IDL_IDENT(dataNode).str);
            iter = IDL_LIST(iter).next;
        }
    }
#endif

    fputs(";\n\n", state->file);

    return TRUE;
}


static gboolean
constant_declaration(TreeState *state)
{
    struct _IDL_CONST_DCL *declaration = &IDL_CONST_DCL(state->tree);
    const char *name = IDL_IDENT(declaration->ident).str;
    GSList *doc_comments = IDL_IDENT(declaration->ident).comments;
    IDL_tree real_type;

    const char* format;
    const char* type_str;
    gboolean is_long;

    gboolean success;

    if (!verify_const_declaration(state->tree))
        return FALSE;

    /* Could be a typedef; try to map it to the real type. */
    real_type = find_underlying_type(declaration->const_type);
    real_type = real_type ? real_type : declaration->const_type;

    /* Consts must be in an interface */
    if (!IDL_NODE_UP(IDL_NODE_UP(state->tree)) ||
        IDL_NODE_TYPE(IDL_NODE_UP(IDL_NODE_UP(state->tree))) != 
        IDLN_INTERFACE) {

        XPIDL_WARNING((state->tree, IDL_WARNING1,
                       "A constant \"%s\" was declared outside an interface."
                       "  It was ignored.", name));

        return TRUE;
    }

    /*
     * The C++ header XPIDL module only allows for shorts and longs (ints)
     * to be constants, so we will follow the same convention
     */

    is_long = FALSE;

    success = (IDLN_TYPE_INTEGER == IDL_NODE_TYPE(real_type));

    if (success) {
        gboolean is_signed = IDL_TYPE_INTEGER(real_type).f_signed;
        format = is_signed ? "%" IDL_LL "d" : "%" IDL_LL "u";

        switch(IDL_TYPE_INTEGER(real_type).f_type) {
            case IDL_INTEGER_TYPE_SHORT:
                if (is_signed)
                    type_str = "short";
                else
                    type_str = "int";
                break;

            case IDL_INTEGER_TYPE_LONG:
                if (is_signed) {
                    type_str = "int";
                } else {
                    type_str = "long";
                    is_long = TRUE;
                }
                break;
            
            default:
                /* Whoops, it's some other kind of number */
                success = FALSE;
                break;
        }
    } else {
        IDL_tree_error(state->tree,
                       "const declaration \'%s\' must be of type short or long",
                       name);
        return FALSE;
    }

    if (!success) {
        XPIDL_WARNING((state->tree, IDL_WARNING1,
                       "A constant \"%s\" was not of type short or long."
                       "  It was ignored.", name));
        return FALSE;
    }

    if (doc_comments != NULL) {
        write_indent(state->file);
        printlist(state->file, doc_comments);
    }

    write_indent(state->file);
    fprintf(state->file, "%s %s = ", type_str,
            subscriptIdentifier(state, (char*) name));
    fprintf(state->file, format, IDL_INTEGER(declaration->const_exp).value);
    fprintf(state->file, "%s;\n\n", is_long ? "L" : "");

    return TRUE;

}

#define ATTR_IDENT(tree) (IDL_IDENT(IDL_LIST(IDL_ATTR_DCL((tree)).simple_declarations).data))
#define ATTR_PROPS(tree) (IDL_LIST(IDL_ATTR_DCL((tree)).simple_declarations).data)
#define ATTR_TYPE_DECL(tree) (IDL_ATTR_DCL((tree)).param_type_spec)


static gboolean
attribute_declaration(TreeState *state)
{
    char *attribute_name;
    GSList *doc_comments;
    gboolean read_only;

    if (!verify_attribute_declaration(state->tree))
        return FALSE;

    attribute_name = ATTR_IDENT(state->tree).str;
    read_only = IDL_ATTR_DCL(state->tree).f_readonly;

#ifdef OUTPUT_SWT_NOSCRIPT_METHODS
    if (IDL_tree_property_get(ATTR_PROPS(state->tree), "notxpcom"))
        return TRUE;
    if (IDL_tree_property_get(ATTR_PROPS(state->tree), "noscript") &&
        !print_noscript_method(state))
        return TRUE;
#else
    if (IDL_tree_property_get(ATTR_PROPS(state->tree), "notxpcom") ||
        IDL_tree_property_get(ATTR_PROPS(state->tree), "noscript"))
        return TRUE;
#endif

    doc_comments =
        IDL_IDENT(IDL_LIST(IDL_ATTR_DCL
                           (state->tree).simple_declarations).data).comments;
    if (doc_comments != NULL) {
        write_indent(state->file);
        printlist(state->file, doc_comments);
    }

    /*
     * Write the proper Java return value for the get operation
     */
    write_indent(state->file);
    state->tree = ATTR_TYPE_DECL(state->tree);
    if (!xpcom_to_java_type(state, NULL)) {
        return FALSE;
    }
    
    /*
     * Write the name of the accessor ("get") method.
     */
    fprintf(state->file, " get%c%s();\n\n",
            toupper(attribute_name[0]), attribute_name + 1);

    if (!read_only) {
        if (doc_comments != NULL) {
            write_indent(state->file);
            printlist(state->file, doc_comments);
        }

        /*
         * Write attribute access method name and return type
         */
        write_indent(state->file);
        fprintf(state->file, "void set%c%s(",
                toupper(attribute_name[0]), 
                attribute_name+1);
        
        /*
         * Write the proper Java type for the set operation
         */
        if (!xpcom_to_java_type(state, NULL)) {
            return FALSE;
        }

        /*
         * Write the name of the formal parameter.
         */
        fprintf(state->file, " a%c%s);\n\n", toupper(attribute_name[0]),
                attribute_name + 1);
    }

    return TRUE;
}


static gboolean
enum_declaration(TreeState *state)
{
    XPIDL_WARNING((state->tree, IDL_WARNING1,
                   "enums not supported, enum \'%s\' ignored",
                   IDL_IDENT(IDL_TYPE_ENUM(state->tree).ident).str));
    return TRUE;
}

backend *
xpidl_java_dispatch(void)
{
    static backend result;
    static nodeHandler table[IDLN_LAST];
    static gboolean initialized = FALSE;

    result.emit_prolog = java_prolog;
    result.emit_epilog = java_epilog;

    if (!initialized) {
        table[IDLN_INTERFACE] = interface_declaration;
        table[IDLN_LIST] = process_list;

        table[IDLN_OP_DCL] = method_declaration;
        table[IDLN_ATTR_DCL] = attribute_declaration;
        table[IDLN_CONST_DCL] = constant_declaration;

        table[IDLN_TYPE_DCL] = type_declaration;
        table[IDLN_FORWARD_DCL] = forward_declaration;

        table[IDLN_TYPE_ENUM] = enum_declaration;

        initialized = TRUE;
    }

    result.dispatch_table = table;
    return &result;
}

char* subscriptIdentifier(TreeState *state, char *str)
{
    char *sstr = NULL;
    char *keyword = g_hash_table_lookup(KEYWORDS(state), str);
    if (keyword) {
        sstr = g_strdup_printf("_%s", keyword);
        return sstr;
    }
    return str;
}

