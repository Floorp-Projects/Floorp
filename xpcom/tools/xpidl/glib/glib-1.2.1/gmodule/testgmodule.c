/* testgmodule.c - test program for GMODULE
 * Copyright (C) 1998 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
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

#undef	G_LOG_DOMAIN
#include	<gmodule.h>


G_MODULE_EXPORT void
g_clash_func (void)
{
  g_print ("GModule: Hello global clash\n");
}

typedef	void (*SimpleFunc) (void);
typedef	void (*GModuleFunc) (GModule *);

SimpleFunc gplugin_clash_func;

int
main (int   arg,
      char *argv[])
{
  GModule *module_self, *module_a, *module_b;
  gchar *string;
  gchar *plugin_a, *plugin_b;
  SimpleFunc f_a, f_b, f_self;
  GModuleFunc gmod_f;

  string = g_get_current_dir ();
  g_print ("testgmodule (%s):\n", string);

#ifdef NATIVE_WIN32
  plugin_a = g_strconcat (string, "\\libgplugin_a.dll", NULL);
  plugin_b = g_strconcat (string, "\\libgplugin_b.dll", NULL);
#else /* !NATIVE_WIN32 */
  plugin_a = g_strconcat (string, "/.libs/", "libgplugin_a.so", NULL);
  plugin_b = g_strconcat (string, "/.libs/", "libgplugin_b.so", NULL);
#endif /* NATIVE_WIN32 */
  g_free (string);

  /* module handles
   */
  g_print ("get main module handle\n");
  module_self = g_module_open (NULL, G_MODULE_BIND_LAZY);
  if (!module_self)
    {
      g_print ("error: %s\n", g_module_error ());
      return 1;
    }
  g_print ("load plugin from \"%s\"\n", plugin_a);
  module_a = g_module_open (plugin_a, G_MODULE_BIND_LAZY);
  if (!module_a)
    {
      g_print ("error: %s\n", g_module_error ());
      return 1;
    }
  g_print ("load plugin from \"%s\"\n", plugin_b);
  module_b = g_module_open (plugin_b, G_MODULE_BIND_LAZY);
  if (!module_b)
    {
      g_print ("error: %s\n", g_module_error ());
      return 1;
    }

  /* get plugin specific symbols and call them
   */
  string = "gplugin_a_func";
  g_print ("retrive symbol `%s' from \"%s\"\n", string, g_basename (g_module_name (module_a)));
  if (!g_module_symbol (module_a, string, (gpointer) &f_a))
    {
      g_print ("error: %s\n", g_module_error ());
      return 1;
    }
  string = "gplugin_b_func";
  g_print ("retrive symbol `%s' from \"%s\"\n", string, g_basename (g_module_name (module_b)));
  if (!g_module_symbol (module_b, string, (gpointer) &f_b))
    {
      g_print ("error: %s\n", g_module_error ());
      return 1;
    }
  g_print ("call plugin function(%p) A: ", f_a);
  f_a ();
  g_print ("call plugin function(%p) B: ", f_b);
  f_b ();

  /* get and call globally clashing functions
   */
  string = "g_clash_func";
  g_print ("retrive symbol `%s' from \"%s\"\n", string, g_basename (g_module_name (module_self)));
  if (!g_module_symbol (module_self, string, (gpointer) &f_self))
    {
      g_print ("error: %s\n", g_module_error ());
      return 1;
    }
  g_print ("retrive symbol `%s' from \"%s\"\n", string, g_basename (g_module_name (module_a)));
  if (!g_module_symbol (module_a, string, (gpointer) &f_a))
    {
      g_print ("error: %s\n", g_module_error ());
      return 1;
    }
  g_print ("retrive symbol `%s' from \"%s\"\n", string, g_basename (g_module_name (module_b)));
  if (!g_module_symbol (module_b, string, (gpointer) &f_b))
    {
      g_print ("error: %s\n", g_module_error ());
      return 1;
    }
  g_print ("call plugin function(%p) self: ", f_self);
  f_self ();
  g_print ("call plugin function(%p) A: ", f_a);
  f_a ();
  g_print ("call plugin function(%p) B: ", f_b);
  f_b ();

  /* get and call clashing plugin functions
   */
  string = "gplugin_clash_func";
  g_print ("retrive symbol `%s' from \"%s\"\n", string, g_basename (g_module_name (module_a)));
  if (!g_module_symbol (module_a, string, (gpointer) &f_a))
    {
      g_print ("error: %s\n", g_module_error ());
      return 1;
    }
  g_print ("retrive symbol `%s' from \"%s\"\n", string, g_basename (g_module_name (module_b)));
  if (!g_module_symbol (module_b, string, (gpointer) &f_b))
    {
      g_print ("error: %s\n", g_module_error ());
      return 1;
    }
  g_print ("call plugin function(%p) A: ", f_a);
  gplugin_clash_func = f_a;
  gplugin_clash_func ();
  g_print ("call plugin function(%p) B: ", f_b);
  gplugin_clash_func = f_b;
  gplugin_clash_func ();

  /* call gmodule function form A
   */
  string = "gplugin_a_module_func";
  g_print ("retrive symbol `%s' from \"%s\"\n", string, g_basename (g_module_name (module_a)));
  if (!g_module_symbol (module_a, string, (gpointer) &gmod_f))
    {
      g_print ("error: %s\n", g_module_error ());
      return 1;
    }
  g_print ("call plugin A's module function(%p):\n{\n", gmod_f);
  gmod_f (module_b);
  g_print ("}\n");

  
  /* unload plugins
   */
  g_print ("unload plugin A:\n");
  if (!g_module_close (module_a))
    g_print ("error: %s\n", g_module_error ());
  g_print ("unload plugin B:\n");
  if (!g_module_close (module_b))
    g_print ("error: %s\n", g_module_error ());

#if 0
  g_log_set_fatal_mask ("GModule", G_LOG_FATAL_MASK|G_LOG_LEVEL_WARNING);
  g_module_symbol (0, 0, 0);
  g_warning("jahooo");
  g_on_error_query (".libs/testgmodule");
#endif
  
  return 0;
}
