/* libgplugin_b.c - test plugin for testgmodule
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

#include        <gmodule.h>

#if defined (NATIVE_WIN32) && defined (__LCC__)
int __stdcall 
LibMain(void         *hinstDll,
	unsigned long dwReason,
	void         *reserved)
{
  return 1;
}
#endif /* NATIVE_WIN32 && __LCC__ */

G_MODULE_EXPORT const gchar*
g_module_check_init (GModule *module)
{
  g_print ("GPluginB: check-init\n");

  return NULL;
}

G_MODULE_EXPORT void
g_module_unload (GModule *module)
{
  g_print ("GPluginB: unloaded\n");
}

G_MODULE_EXPORT void
gplugin_b_func (void)
{
  g_print ("GPluginB: Hello world\n");
}

G_MODULE_EXPORT void
gplugin_clash_func (void)
{
  g_print ("GPluginB: Hello plugin clash\n");
}

G_MODULE_EXPORT void
g_clash_func (void)
{
  g_print ("GPluginB: Hello global clash\n");
}

G_MODULE_EXPORT void
gplugin_say_boo_func (void)
{
  g_print ("GPluginB: BOOH!\n");
}
