/* GMODULE - GLIB wrapper code for dynamic module loading
 * Copyright (C) 1998 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* 
 * MT safe
 */

#include	"gmodule.h"
#include	"gmoduleconf.h"
#include	<errno.h>
#include	<string.h>


/* We maintain a list of modules, so we can reference count them.
 * That's needed because some platforms don't support refernce counts on
 * modules e.g. the shl_* implementation of HP-UX
 * (http://www.stat.umn.edu/~luke/xls/projects/dlbasics/dlbasics.html).
 * Also, the module for the program itself is kept seperatedly for
 * faster access and because it has special semantics.
 */


/* --- structures --- */
struct _GModule
{
  gchar	*file_name;
  gpointer handle;
  guint ref_count : 31;
  guint is_resident : 1;
  GModuleUnload unload;
  GModule *next;
};


/* --- prototypes --- */
static gpointer		_g_module_open		(const gchar	*file_name,
						 gboolean	 bind_lazy);
static void		_g_module_close		(gpointer	 handle,
						 gboolean	 is_unref);
static gpointer		_g_module_self		(void);
static gpointer		_g_module_symbol	(gpointer	 handle,
						 const gchar	*symbol_name);
static gchar*		_g_module_build_path	(const gchar	*directory,
						 const gchar	*module_name);
static inline void	g_module_set_error	(const gchar	*error);
static inline GModule*	g_module_find_by_handle (gpointer	 handle);
static inline GModule*	g_module_find_by_name	(const gchar	*name);


/* --- variables --- */
G_LOCK_DEFINE_STATIC (GModule);
const char           *g_log_domain_gmodule = "GModule";
static GModule	     *modules = NULL;
static GModule	     *main_module = NULL;
static GStaticPrivate module_error_private = G_STATIC_PRIVATE_INIT;


/* --- inline functions --- */
static inline GModule*
g_module_find_by_handle (gpointer handle)
{
  GModule *module;
  GModule *retval = NULL;
  
  G_LOCK (GModule);
  if (main_module && main_module->handle == handle)
    retval = main_module;
  else
    for (module = modules; module; module = module->next)
      if (handle == module->handle)
	{
	  retval = module;
	  break;
	}
  G_UNLOCK (GModule);

  return retval;
}

static inline GModule*
g_module_find_by_name (const gchar *name)
{
  GModule *module;
  GModule *retval = NULL;
  
  G_LOCK (GModule);
  for (module = modules; module; module = module->next)
    if (strcmp (name, module->file_name) == 0)
	{
	  retval = module;
	  break;
	}
  G_UNLOCK (GModule);

  return retval;
}

static inline void
g_module_set_error (const gchar *error)
{
  g_static_private_set (&module_error_private, g_strdup (error), g_free);
  errno = 0;
}


/* --- include platform specifc code --- */
#define	CHECK_ERROR(rv)	{ g_module_set_error (NULL); }
#if	(G_MODULE_IMPL == G_MODULE_IMPL_DL)
#include "gmodule-dl.c"
#elif	(G_MODULE_IMPL == G_MODULE_IMPL_DLD)
#include "gmodule-dld.c"
#elif	(G_MODULE_IMPL == G_MODULE_IMPL_WIN32)
#include "gmodule-win32.c"
#else
#undef	CHECK_ERROR
#define	CHECK_ERROR(rv)	{ g_module_set_error ("dynamic modules are " \
                                              "not supported by this system"); return rv; }
static gpointer
_g_module_open (const gchar	*file_name,
		gboolean	 bind_lazy)
{
  return NULL;
}
static void
_g_module_close	(gpointer	 handle,
		 gboolean	 is_unref)
{
}
static gpointer
_g_module_self (void)
{
  return NULL;
}
static gpointer
_g_module_symbol (gpointer	 handle,
		  const gchar	*symbol_name)
{
  return NULL;
}
static gchar*
_g_module_build_path (const gchar *directory,
		      const gchar *module_name)
{
  return NULL;
}
#endif	/* no implementation */

#if defined (NATIVE_WIN32) && defined (__LCC__)
int __stdcall 
LibMain (void         *hinstDll,
	 unsigned long dwReason,
	 void         *reserved)
{
  return 1;
}
#endif /* NATIVE_WIN32 && __LCC__ */


/* --- functions --- */
gboolean
g_module_supported (void)
{
  CHECK_ERROR (FALSE);
  
  return TRUE;
}

GModule*
g_module_open (const gchar    *file_name,
	       GModuleFlags    flags)
{
  GModule *module;
  gpointer handle;
  
  CHECK_ERROR (NULL);
  
  if (!file_name)
    {      
      G_LOCK (GModule);
      if (!main_module)
	{
	  handle = _g_module_self ();
	  if (handle)
	    {
	      main_module = g_new (GModule, 1);
	      main_module->file_name = NULL;
	      main_module->handle = handle;
	      main_module->ref_count = 1;
	      main_module->is_resident = TRUE;
	      main_module->unload = NULL;
	      main_module->next = NULL;
	    }
	}
      G_UNLOCK (GModule);

      return main_module;
    }
  
  /* we first search the module list by name */
  module = g_module_find_by_name (file_name);
  if (module)
    {
      module->ref_count++;
      
      return module;
    }
  
  /* open the module */
  handle = _g_module_open (file_name, (flags & G_MODULE_BIND_LAZY) != 0);
  if (handle)
    {
      gchar *saved_error;
      GModuleCheckInit check_init;
      const gchar *check_failed = NULL;
      
      /* search the module list by handle, since file names are not unique */
      module = g_module_find_by_handle (handle);
      if (module)
	{
	  _g_module_close (module->handle, TRUE);
	  module->ref_count++;
	  g_module_set_error (NULL);
	  
	  return module;
	}
      
      saved_error = g_strdup (g_module_error ());
      g_module_set_error (NULL);
      
      module = g_new (GModule, 1);
      module->file_name = g_strdup (file_name);
      module->handle = handle;
      module->ref_count = 1;
      module->is_resident = FALSE;
      module->unload = NULL;
      G_LOCK (GModule);
      module->next = modules;
      modules = module;
      G_UNLOCK (GModule);
      
      /* check initialization */
      if (g_module_symbol (module, "g_module_check_init", (gpointer) &check_init))
	check_failed = check_init (module);
      
      /* we don't call unload() if the initialization check failed. */
      if (!check_failed)
	g_module_symbol (module, "g_module_unload", (gpointer) &module->unload);
      
      if (check_failed)
	{
	  gchar *error;

	  error = g_strconcat ("GModule initialization check failed: ", check_failed, NULL);
	  g_module_close (module);
	  module = NULL;
	  g_module_set_error (error);
	  g_free (error);
	}
      else
	g_module_set_error (saved_error);

      g_free (saved_error);
    }
  
  return module;
}

gboolean
g_module_close (GModule	       *module)
{
  CHECK_ERROR (FALSE);
  
  g_return_val_if_fail (module != NULL, FALSE);
  g_return_val_if_fail (module->ref_count > 0, FALSE);
  
  module->ref_count--;
  
  if (!module->ref_count && !module->is_resident && module->unload)
    {
      GModuleUnload unload;

      unload = module->unload;
      module->unload = NULL;
      unload (module);
    }

  if (!module->ref_count && !module->is_resident)
    {
      GModule *last;
      GModule *node;
      
      last = NULL;
      
      G_LOCK (GModule);
      node = modules;
      while (node)
	{
	  if (node == module)
	    {
	      if (last)
		last->next = node->next;
	      else
		modules = node->next;
	      break;
	    }
	  last = node;
	  node = last->next;
	}
      module->next = NULL;
      G_UNLOCK (GModule);
      
      _g_module_close (module->handle, FALSE);
      g_free (module->file_name);
      
      g_free (module);
    }
  
  return g_module_error() == NULL;
}

void
g_module_make_resident (GModule *module)
{
  g_return_if_fail (module != NULL);

  module->is_resident = TRUE;
}

gchar*
g_module_error (void)
{
  return g_static_private_get (&module_error_private);
}

gboolean
g_module_symbol (GModule	*module,
		 const gchar	*symbol_name,
		 gpointer	*symbol)
{
  gchar *module_error;
  if (symbol)
    *symbol = NULL;
  CHECK_ERROR (FALSE);
  
  g_return_val_if_fail (module != NULL, FALSE);
  g_return_val_if_fail (symbol_name != NULL, FALSE);
  g_return_val_if_fail (symbol != NULL, FALSE);
  
#ifdef	G_MODULE_NEED_USCORE
  {
    gchar *name;

    name = g_strconcat ("_", symbol_name, NULL);
    *symbol = _g_module_symbol (module->handle, name);
    g_free (name);
  }
#else	/* !G_MODULE_NEED_USCORE */
  *symbol = _g_module_symbol (module->handle, symbol_name);
#endif	/* !G_MODULE_NEED_USCORE */
  
  if ((module_error = g_module_error()))
    {
      gchar *error;

      error = g_strconcat ("`", symbol_name, "': ", module_error, NULL);
      g_module_set_error (error);
      g_free (error);
      *symbol = NULL;
      return FALSE;
    }
  
  return TRUE;
}

gchar*
g_module_name (GModule *module)
{
  g_return_val_if_fail (module != NULL, NULL);
  
  if (module == main_module)
    return "main";
  
  return module->file_name;
}

gchar*
g_module_build_path (const gchar *directory,
		     const gchar *module_name)
{
  g_return_val_if_fail (module_name != NULL, NULL);
  
  return _g_module_build_path (directory, module_name);
}
