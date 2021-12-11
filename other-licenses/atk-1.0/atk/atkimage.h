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

#ifndef __ATK_IMAGE_H__
#define __ATK_IMAGE_H__

#include <atk/atkobject.h>
#include <atk/atkutil.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * The AtkImage interface should be supported by any object that has an 
 * associated image. This interface provides the standard mechanism for 
 * an assistive technology to get descriptive information about images.
 */

#define ATK_TYPE_IMAGE                   (atk_image_get_type ())
#define ATK_IS_IMAGE(obj)                G_TYPE_CHECK_INSTANCE_TYPE ((obj), ATK_TYPE_IMAGE)
#define ATK_IMAGE(obj)                   G_TYPE_CHECK_INSTANCE_CAST ((obj), ATK_TYPE_IMAGE, AtkImage)
#define ATK_IMAGE_GET_IFACE(obj)         (G_TYPE_INSTANCE_GET_INTERFACE ((obj), ATK_TYPE_IMAGE, AtkImageIface))

#ifndef _TYPEDEF_ATK_IMAGE_
#define _TYPEDEF_ATK_IMAGE_
typedef struct _AtkImage AtkImage;
#endif
typedef struct _AtkImageIface AtkImageIface;

struct _AtkImageIface
{
  GTypeInterface parent;
  void          	( *get_image_position)    (AtkImage		 *image,
                                                   gint                  *x,
				                   gint	                 *y,
    			                           AtkCoordType	         coord_type);
  G_CONST_RETURN gchar* ( *get_image_description) (AtkImage              *image);
  void                  ( *get_image_size)        (AtkImage              *image,
                                                   gint                  *width,
                                                   gint                  *height);
  gboolean              ( *set_image_description) (AtkImage              *image,
                                                   const gchar           *description);
  G_CONST_RETURN gchar* ( *get_image_locale)      (AtkImage              *image);

  AtkFunction           pad1;
	
};

GType  atk_image_get_type             (void);

G_CONST_RETURN gchar* atk_image_get_image_description (AtkImage   *image);

void     atk_image_get_image_size        (AtkImage           *image,
                                          gint               *width,
                                          gint               *height);

gboolean atk_image_set_image_description (AtkImage           *image,
                                          const gchar       *description);
void     atk_image_get_image_position    (AtkImage	     *image,
                                          gint               *x,
					  gint	             *y,
    					  AtkCoordType	     coord_type);

G_CONST_RETURN gchar* atk_image_get_image_locale (AtkImage   *image);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __ATK_IMAGE_H__ */
