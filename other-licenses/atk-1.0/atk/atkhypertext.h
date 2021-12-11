/* ATK -  Accessibility Toolkit
 * Copyright 2001 Sun Microsystems Inc.
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

#ifndef __ATK_HYPERTEXT_H__
#define __ATK_HYPERTEXT_H__

#include <atk/atkobject.h>
#include <atk/atkhyperlink.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * The AtkHypertext interface provides standard  mechanisms for manipulating 
 * hyperlinks.
 */

#define ATK_TYPE_HYPERTEXT                    (atk_hypertext_get_type ())
#define ATK_IS_HYPERTEXT(obj)                 G_TYPE_CHECK_INSTANCE_TYPE ((obj), ATK_TYPE_HYPERTEXT)
#define ATK_HYPERTEXT(obj)                    G_TYPE_CHECK_INSTANCE_CAST ((obj), ATK_TYPE_HYPERTEXT, AtkHypertext)
#define ATK_HYPERTEXT_GET_IFACE(obj)          (G_TYPE_INSTANCE_GET_INTERFACE ((obj), ATK_TYPE_HYPERTEXT, AtkHypertextIface))

#ifndef _TYPEDEF_ATK_HYPERTEXT_
#define _TYPEDEF_ATK_HYPERTEXT_
typedef struct _AtkHypertext AtkHypertext;
#endif
typedef struct _AtkHypertextIface AtkHypertextIface;

struct _AtkHypertextIface
{
  GTypeInterface parent;

  AtkHyperlink*(* get_link)                 (AtkHypertext       *hypertext,
                                             gint               link_index);
  gint         (* get_n_links)              (AtkHypertext       *hypertext);
  gint         (* get_link_index)           (AtkHypertext       *hypertext,
                                             gint               char_index);

  /*
   * signal handlers
   */
  void         (* link_selected)            (AtkHypertext       *hypertext,
                                             gint               link_index);

  AtkFunction pad1;
  AtkFunction pad2;
  AtkFunction pad3;
};
GType atk_hypertext_get_type (void);

AtkHyperlink* atk_hypertext_get_link       (AtkHypertext *hypertext,
                                            gint          link_index);
gint          atk_hypertext_get_n_links    (AtkHypertext *hypertext);
gint          atk_hypertext_get_link_index (AtkHypertext *hypertext,
                                            gint          char_index);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __ATK_HYPERTEXT_H__ */
