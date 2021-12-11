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

#ifndef __ATK_H__
#define __ATK_H__

#define __ATK_H_INSIDE__

// XXX this is a Mozilla hack to avoid using atkversion.h which needs to be run
// through m4, and we don't want to deal with that.  Fortunately we don't need
// any of the fancyness in that header to deal with compile time version
// detection.
#define ATK_AVAILABLE_IN_2_12 extern

#include <atk/atkobject.h>
#include <atk/atkaction.h>
#include <atk/atkcomponent.h>
#include <atk/atkdocument.h>
#include <atk/atkeditabletext.h>
#include <atk/atkgobjectaccessible.h>
#include <atk/atkhyperlink.h>
#include <atk/atkhyperlinkimpl.h>
#include <atk/atkhypertext.h>
#include <atk/atkimage.h>
#include <atk/atknoopobject.h>
#include <atk/atknoopobjectfactory.h>
#include <atk/atkobjectfactory.h>
#include <atk/atkplug.h>
#include <atk/atkregistry.h>
#include <atk/atkrelation.h>
#include <atk/atkrelationset.h>
#include <atk/atkrelationtype.h>
#include <atk/atkselection.h>
#include <atk/atksocket.h>
#include <atk/atkstate.h>
#include <atk/atkstateset.h>
#include <atk/atkstreamablecontent.h>
#include <atk/atktable.h>
#include <atk/atktablecell.h>
#include <atk/atktext.h>
#include <atk/atkutil.h>
#include <atk/atkvalue.h>

#undef __ATK_H_INSIDE__

#endif /* __ATK_H__ */
