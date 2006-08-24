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

#ifndef __ATK_STATE_H__
#define __ATK_STATE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <glib-object.h>

/**
 *AtkStateType:
 *@ATK_STATE_INVALID: Indicates an invalid state
 *@ATK_STATE_ACTIVE: Indicates a window is currently the active window
 *@ATK_STATE_ARMED: Indicates that the object is armed.
 *@ATK_STATE_BUSY: Indicates the current object is busy.  This state may be used by implementors of Document to indicate that content loading is in process.
 *@ATK_STATE_CHECKED: Indicates this object is currently checked
 *@ATK_STATE_DEFUNCT: Indicates the user interface object corresponding to this object no longer exists
 *@ATK_STATE_EDITABLE: Indicates the user can change the contents of this object
 *@ATK_STATE_ENABLED: Indicates that this object is enabled. An inconsistent GtkToggleButton is an example of an object which is sensitive but not enabled.
 *@ATK_STATE_EXPANDABLE: Indicates this object allows progressive disclosure of its children
 *@ATK_STATE_EXPANDED: Indicates this object its expanded
 *@ATK_STATE_FOCUSABLE: Indicates this object can accept keyboard focus, which means all events resulting from typing on the keyboard will normally be passed to it when it has focus
 *@ATK_STATE_FOCUSED: Indicates this object currently has the keyboard focus
 *@ATK_STATE_HORIZONTAL: Indicates the orientation of this object is horizontal
 *@ATK_STATE_ICONIFIED: Indicates this object is minimized and is represented only by an icon
 *@ATK_STATE_MODAL: Indicates something must be done with this object before the user can interact with an object in a different window
 *@ATK_STATE_MULTI_LINE: Indicates this (text) object can contain multiple lines of text
 *@ATK_STATE_MULTISELECTABLE: Indicates this object allows more than one of its children to be selected at the same time
 *@ATK_STATE_OPAQUE: Indicates this object paints every pixel within its rectangular region.
 *@ATK_STATE_PRESSED: Indicates this object is currently pressed
 *@ATK_STATE_RESIZABLE: Indicates the size of this object is not fixed
 *@ATK_STATE_SELECTABLE: Indicates this object is the child of an object that allows its children to be selected and that this child is one of those children that can be selected
 *@ATK_STATE_SELECTED: Indicates this object is the child of an object that allows its children to be selected and that this child is one of those children that has been selected
 *@ATK_STATE_SENSITIVE: Indicates this object is sensitive
 *@ATK_STATE_SHOWING: Indicates this object, the object's parent, the object's parent's parent, and so on, are all visible
 *@ATK_STATE_SINGLE_LINE: Indicates this (text) object can contain only a single line of text
 *@ATK_STATE_STALE: Indicates that the index associated with this object has changed since the user accessed the object.
 *@ATK_STATE_TRANSIENT: Indicates this object is transient
 *@ATK_STATE_VERTICAL: Indicates the orientation of this object is vertical
 *@ATK_STATE_VISIBLE: Indicates this object is visible
 *@ATK_STATE_MANAGES_DESCENDANTS: Indicates that "active-descendant-changed" event
 * is sent when children become 'active' (i.e. are selected or navigated to onscreen).
 * Used to prevent need to enumerate all children in very large containers, like tables.
 *@ATK_STATE_INDETERMINATE: Indicates that a check box is in a state other than checked or not checked.
 *@ATK_STATE_TRUNCATED: Indicates that an object is truncated, e.g. a text value in a speradsheet cell.
 *@ATK_STATE_REQUIRED: Indicates that explicit user interaction with an object is required by the user interface, e.g. a required field in a "web-form" interface.
 *@ATK_STATE_INVALID_ENTRY: Indicates that the object has encountered an error condition due to failure of input validation. For instance, a form control may acquire this state in response to invalid or malformed user input.
 *@ATK_STATE_SUPPORTS_AUTOCOMPLETION: Indicates that the object may exhibit "typeahead" behavior in response to user keystrokes, e.g. one keystroke may result in the insertion of several characters into an entry, or result in the auto-selection of an item in a list.  This state supplants @ATK_ROLE_AUTOCOMPLETE.
 *@ATK_STATE_SELECTABLE_TEXT:Indicates that the object in question supports text selection. It should only be exposed on objects which implement the Text interface, in order to distinguish this state from @ATK_STATE_SELECTABLE, which infers that the object in question is a selectable child of an object which implements Selection. While similar, text selection and subelement selection are distinct operations.
 *@ATK_STATE_DEFAULT: Indicates that the object is the "default" active component, i.e. the object which is activated by an end-user press of the "Enter" or "Return" key.  Typically a "close" or "submit" button.
 *@ATK_STATE_ANIMATED: Indicates that the object changes its appearance dynamically as an inherent part of its presentation.  This state may come and go if an object is only temporarily animated on the way to a 'final' onscreen presentation.
 *@ATK_STATE_VISITED: Indicates that the object (typically a hyperlink) has already been 'activated', and/or its backing data has already been downloaded, rendered, or otherwise "visited".
 *@ATK_STATE_LAST_DEFINED: Not a valid state, used for finding end of enumeration
 *
 *The possible types of states of an object
 **/ 
typedef enum
{
  ATK_STATE_INVALID,
  ATK_STATE_ACTIVE,
  ATK_STATE_ARMED,
  ATK_STATE_BUSY,
  ATK_STATE_CHECKED,
  ATK_STATE_DEFUNCT,
  ATK_STATE_EDITABLE,
  ATK_STATE_ENABLED,
  ATK_STATE_EXPANDABLE,
  ATK_STATE_EXPANDED,
  ATK_STATE_FOCUSABLE,
  ATK_STATE_FOCUSED,
  ATK_STATE_HORIZONTAL,
  ATK_STATE_ICONIFIED,
  ATK_STATE_MODAL,
  ATK_STATE_MULTI_LINE,
  ATK_STATE_MULTISELECTABLE,
  ATK_STATE_OPAQUE,
  ATK_STATE_PRESSED,
  ATK_STATE_RESIZABLE,
  ATK_STATE_SELECTABLE,
  ATK_STATE_SELECTED,
  ATK_STATE_SENSITIVE,
  ATK_STATE_SHOWING,
  ATK_STATE_SINGLE_LINE,
  ATK_STATE_STALE,
  ATK_STATE_TRANSIENT,
  ATK_STATE_VERTICAL,
  ATK_STATE_VISIBLE,
  ATK_STATE_MANAGES_DESCENDANTS,
  ATK_STATE_INDETERMINATE,
  ATK_STATE_TRUNCATED,
  ATK_STATE_REQUIRED,
  ATK_STATE_INVALID_ENTRY,
  ATK_STATE_SUPPORTS_AUTOCOMPLETION,
  ATK_STATE_SELECTABLE_TEXT,
  ATK_STATE_DEFAULT,
  ATK_STATE_ANIMATED,
  ATK_STATE_VISITED,
	
  ATK_STATE_LAST_DEFINED
} AtkStateType;

typedef guint64      AtkState;

AtkStateType atk_state_type_register            (const gchar *name);

G_CONST_RETURN gchar* atk_state_type_get_name   (AtkStateType type);
AtkStateType          atk_state_type_for_name   (const gchar  *name);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __ATK_STATE_H__ */
