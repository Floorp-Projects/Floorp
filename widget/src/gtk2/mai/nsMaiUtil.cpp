/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems are Copyright (C) 2002 Sun
 * Microsystems, Inc. All Rights Reserved.
 *
 * Original Author: Bolian Yin (bolian.yin@sun.com)
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

#include <stdlib.h>

#include "nsMaiHook.h"
#include "nsMaiUtil.h"
#include "nsMaiAppRoot.h"

static gboolean mai_shutdown(void);
static void mai_delete_root(void);
static MaiAppRoot *mai_create_root(void);

static void mai_util_class_init(MaiUtilClass *klass);

/* atkutil.h */

static guint mai_util_add_global_event_listener(GSignalEmissionHook listener,
                                                const gchar *event_type);
static void mai_util_remove_global_event_listener(guint remove_listener);
static AtkObject *mai_util_get_root(void);
static G_CONST_RETURN gchar *mai_util_get_toolkit_name(void);
static G_CONST_RETURN gchar *mai_util_get_toolkit_version(void);
static gboolean mai_add_toplevel_accessible(nsIAccessible *toplevel);
static gboolean mai_remove_toplevel_accessible(nsIAccessible *toplevel);

/* Misc */

static void _listener_info_destroy(gpointer data);
static guint add_listener (GSignalEmissionHook listener,
                           const gchar *object_type,
                           const gchar *signal,
                           const gchar *hook_data);

static GHashTable *listener_list = NULL;
static gint listener_idx = 1;

typedef struct _MaiUtilListenerInfo MaiUtilListenerInfo;

/* supporting */
static int mai_initialized = FALSE;
static MaiHook sMaiHook;
PRLogModuleInfo *gMaiLog = NULL;

struct _MaiUtilListenerInfo
{
    gint key;
    guint signal_id;
    gulong hook_id;
};

GType
mai_util_get_type(void)
{
    static GType type = 0;

    if (!type) {
        static const GTypeInfo tinfo = {
            sizeof(MaiUtilClass),
            (GBaseInitFunc) NULL, /* base init */
            (GBaseFinalizeFunc) NULL, /* base finalize */
            (GClassInitFunc) mai_util_class_init, /* class init */
            (GClassFinalizeFunc) NULL, /* class finalize */
            NULL, /* class data */
            sizeof(MaiUtil), /* instance size */
            0, /* nb preallocs */
            (GInstanceInitFunc) NULL, /* instance init */
            NULL /* value table */
        };

        type = g_type_register_static(ATK_TYPE_UTIL,
                                      "MaiUtil", &tinfo, GTypeFlags(0));
    }
    return type;
}

/* intialize the the atk interface (function pointers) with MAI implementation.
 * When atk bridge get loaded, these interface can be used.
 */
static void
mai_util_class_init(MaiUtilClass *klass)
{
    AtkUtilClass *atk_class;
    gpointer data;

    data = g_type_class_peek(ATK_TYPE_UTIL);
    atk_class = ATK_UTIL_CLASS(data);

    atk_class->add_global_event_listener =
        mai_util_add_global_event_listener;
    atk_class->remove_global_event_listener =
        mai_util_remove_global_event_listener;
    /*
      atk_class->add_key_event_listener =
      mai_util_add_key_event_listener;
      atk_class->remove_key_event_listener =
      mai_util_remove_key_event_listener;
    */
    atk_class->get_root = mai_util_get_root;
    atk_class->get_toolkit_name = mai_util_get_toolkit_name;
    atk_class->get_toolkit_version = mai_util_get_toolkit_version;

    listener_list = g_hash_table_new_full(g_int_hash, g_int_equal, NULL,
                                          _listener_info_destroy);
}

static guint
mai_util_add_global_event_listener(GSignalEmissionHook listener,
                                   const gchar *event_type)
{
    guint rc = 0;
    gchar **split_string;

    split_string = g_strsplit (event_type, ":", 3);

    if (split_string) {
        if (!strcmp ("window", split_string[0])) {
            /*  ???
                static gboolean initialized = FALSE;

                if (!initialized) {
                do_window_event_initialization ();
                initialized = TRUE;
                }
                rc = add_listener (listener, "MaiWindow",
                split_string[1], event_type);
            */
        }
        else {
            rc = add_listener (listener, split_string[1], split_string[2],
                               event_type);
        }
    }
    return rc;
}

static void
mai_util_remove_global_event_listener(guint remove_listener)
{
    if (remove_listener > 0) {
        MaiUtilListenerInfo *listener_info;
        gint tmp_idx = remove_listener;

        listener_info = (MaiUtilListenerInfo *)
            g_hash_table_lookup(listener_list, &tmp_idx);

        if (listener_info != NULL) {
            /* Hook id of 0 and signal id of 0 are invalid */
            if (listener_info->hook_id != 0 && listener_info->signal_id != 0) {
                /* Remove the emission hook */
                g_signal_remove_emission_hook(listener_info->signal_id,
                                              listener_info->hook_id);

                /* Remove the element from the hash */
                g_hash_table_remove(listener_list, &tmp_idx);
            }
            else {
                /* do not use g_warning with such complex format args, */
                /* Forte CC can not preprocess it correctly */
                g_log((gchar *)0, G_LOG_LEVEL_WARNING,
                      "Invalid listener hook_id %ld or signal_id %d\n",
                      listener_info->hook_id, listener_info->signal_id);

            }
        }
        else {
            /* do not use g_warning with such complex format args, */
            /* Forte CC can not preprocess it correctly */
            g_log((gchar *)0, G_LOG_LEVEL_WARNING,
                  "No listener with the specified listener id %d",
                  remove_listener);
        }
    }
    else {
        g_warning("Invalid listener_id %d", remove_listener);
    }
}

AtkObject *
mai_util_get_root(void)
{
    static AtkObject *gRootAtkObject = NULL;
    MaiAppRoot *root;

    if (!gRootAtkObject && (root = mai_create_root()))
        gRootAtkObject = root->GetAtkObject();
    return gRootAtkObject;
}

G_CONST_RETURN gchar *
mai_util_get_toolkit_name(void)
{
    return MAI_NAME;
}

G_CONST_RETURN gchar *
mai_util_get_toolkit_version(void)
{
    return MAI_VERSION;
}

void
_listener_info_destroy(gpointer data)
{
    free(data);
}

guint
add_listener (GSignalEmissionHook listener,
              const gchar *object_type,
              const gchar *signal,
              const gchar *hook_data)
{
    GType type;
    guint signal_id;
    gint rc = 0;

    type = g_type_from_name(object_type);
    if (type) {
        signal_id = g_signal_lookup(signal, type);
        if (signal_id > 0) {
            MaiUtilListenerInfo *listener_info;

            rc = listener_idx;

            listener_info =  (MaiUtilListenerInfo *)
                g_malloc(sizeof(MaiUtilListenerInfo));
            listener_info->key = listener_idx;
            listener_info->hook_id =
                g_signal_add_emission_hook(signal_id, 0, listener,
                                           g_strdup(hook_data),
                                           (GDestroyNotify)g_free);
            listener_info->signal_id = signal_id;

            g_hash_table_insert(listener_list, &(listener_info->key),
                                listener_info);
            listener_idx++;
        }
        else {
            g_warning("Invalid signal type %s\n", signal);
        }
    }
    else {
        g_warning("Invalid object type %s\n", object_type);
    }
    return rc;
}

gboolean
mai_init(void)
{
    if (mai_initialized) {
        return TRUE;
    }
    mai_initialized = TRUE;

    //no MAI_LOG should come before this
    if (!gMaiLog) {
        gMaiLog = PR_NewLogModule("Mai");
        PR_ASSERT(gMaiLog);
    }

    MAI_LOG_DEBUG(("Mozilla Atk Implementation initialized\n"));
    g_type_init();
    /* Initialize the MAI Utility class */
    g_type_class_unref(g_type_class_ref(MAI_TYPE_UTIL));


    return TRUE;
}

gboolean
mai_shutdown(void)
{
    if (!mai_initialized) {
        return TRUE;
    }
    mai_initialized = FALSE;
    MAI_LOG_DEBUG(("Mozilla Atk Implementation shutdown\n"));
    gMaiHook = NULL;
    mai_delete_root();
    return TRUE;
}

int
gtk_module_init(gint *argc, char **argv[])
{
    mai_init();
    return 0;
}

/* supporting funcs */

static MaiAppRoot *sRootAccessible = NULL;

MaiAppRoot *
mai_create_root(void)
{
    if (!mai_initialized) {
        return NULL;
    }
    if (!sRootAccessible) {
        sRootAccessible = new MaiAppRoot();
        NS_ASSERTION(sRootAccessible, "Fail to create MaiAppRoot");

        /* initialize the MAI hook
         * MAI provide the functions of "startup", "shutdown", "add toplvel"
         * "remove toplevel", which are used by Mozilla through the MAI hook.
         */

        gMaiHook = &sMaiHook;
        sMaiHook.MaiShutdown = mai_shutdown;
        sMaiHook.MaiStartup = mai_init;
        sMaiHook.AddTopLevelAccessible = mai_add_toplevel_accessible;
        sMaiHook.RemoveTopLevelAccessible = mai_remove_toplevel_accessible;
    }
    return sRootAccessible;
}

MaiAppRoot *
mai_get_root(void)
{
    return sRootAccessible;
}

void
mai_delete_root(void)
{
    if (sRootAccessible) {
        delete sRootAccessible;
        sRootAccessible = NULL;
    }
}

/* return the reference of MaiCache.
 */
MaiCache *
mai_get_cache(void)
{
    if (!mai_initialized) {
        return NULL;
    }
    MaiAppRoot *root = mai_get_root();
    if (root)
        return root->GetCache();
    return NULL;
}

gboolean
mai_add_toplevel_accessible(nsIAccessible *toplevel)
{
    g_return_val_if_fail(toplevel != NULL, TRUE);

#if 1
    MaiAppRoot *root;
    root = mai_get_root();
    if (!root)
        return FALSE;
    MaiTopLevel *mai_top_level = new MaiTopLevel(toplevel);
    g_return_val_if_fail(mai_top_level != NULL, PR_FALSE);
    gboolean res = root->AddMaiTopLevel(mai_top_level);

    /* root will add ref for itself use */
    g_object_unref(mai_top_level->GetAtkObject());
    return res;
#endif

#if 0
    MaiAppRoot *root;
    root = mai_get_root();
    if (!root)
        return FALSE;

    MaiTopLevel *mai_top_level =
        NS_STATIC_CAST(MaiTopLevel*, MaiTopLevel::CreateAndCache(toplevel));

    g_return_val_if_fail(mai_top_level != NULL, PR_FALSE);
    gboolean res = root->AddMaiTopLevel(mai_top_level);

    return res;
#endif

}

gboolean
mai_remove_toplevel_accessible(nsIAccessible *toplevel)
{
    g_return_val_if_fail(toplevel != NULL, TRUE);

    MaiAppRoot *root;
    root = mai_get_root();
    if (root) {
        MaiTopLevel *mai_top_level = root->FindMaiTopLevel(toplevel);
        return root->RemoveMaiTopLevel(mai_top_level);
    }
    else
        return FALSE;
}
