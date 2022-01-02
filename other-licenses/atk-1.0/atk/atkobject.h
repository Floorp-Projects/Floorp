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

#ifndef __ATK_OBJECT_H__
#define __ATK_OBJECT_H__

#include <glib-object.h>
#include <atk/atkstate.h>
#include <atk/atkrelationtype.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * AtkObject represents the minimum information all accessible objects
 * return. This information includes accessible name, accessible
 * description, role and state of the object, as well information about
 * its parent and children. It is also possible to obtain more specific
 * accessibility information about a component if it supports one or more
 * of the following interfaces:
 */

/**
 *AtkRole:
 *@ATK_ROLE_INVALID: Invalid role
 *@ATK_ROLE_ACCEL_LABEL: A label which represents an accelerator
 *@ATK_ROLE_ALERT: An object which is an alert to the user. Assistive
 *Technologies typically respond to ATK_ROLE_ALERT by reading the entire
 *onscreen contents of containers advertising this role.  Should be used for
 *warning dialogs, etc.
 *@ATK_ROLE_ANIMATION: An object which is an animated image
 *@ATK_ROLE_ARROW: An arrow in one of the four cardinal directions
 *@ATK_ROLE_CALENDAR:  An object that displays a calendar and allows the user to
 *select a date
 *@ATK_ROLE_CANVAS: An object that can be drawn into and is used to trap events
 *@ATK_ROLE_CHECK_BOX: A choice that can be checked or unchecked and provides a
 *separate indicator for the current state
 *@ATK_ROLE_CHECK_MENU_ITEM: A menu item with a check box
 *@ATK_ROLE_COLOR_CHOOSER: A specialized dialog that lets the user choose a
 *color
 *@ATK_ROLE_COLUMN_HEADER: The header for a column of data
 *@ATK_ROLE_COMBO_BOX: A collapsible list of choices the user can select from
 *@ATK_ROLE_DATE_EDITOR: An object whose purpose is to allow a user to edit a
 *date
 *@ATK_ROLE_DESKTOP_ICON: An inconifed internal frame within a DESKTOP_PANE
 *@ATK_ROLE_DESKTOP_FRAME: A pane that supports internal frames and iconified
 *versions of those internal frames
 *@ATK_ROLE_DIAL: An object whose purpose is to allow a user to set a value
 *@ATK_ROLE_DIALOG: A top level window with title bar and a border
 *@ATK_ROLE_DIRECTORY_PANE: A pane that allows the user to navigate through and
 *select the contents of a directory
 *@ATK_ROLE_DRAWING_AREA: An object used for drawing custom user interface
 *elements
 *@ATK_ROLE_FILE_CHOOSER: A specialized dialog that lets the user choose a file
 *@ATK_ROLE_FILLER: A object that fills up space in a user interface
 *@ATK_ROLE_FONT_CHOOSER: A specialized dialog that lets the user choose a font
 *@ATK_ROLE_FRAME: A top level window with a title bar, border, menubar, etc.
 *@ATK_ROLE_GLASS_PANE: A pane that is guaranteed to be painted on top of all
 *panes beneath it
 *@ATK_ROLE_HTML_CONTAINER: A document container for HTML, whose children
 *represent the document content
 *@ATK_ROLE_ICON: A small fixed size picture, typically used to decorate
 *components
 *@ATK_ROLE_IMAGE: An object whose primary purpose is to display an image
 *@ATK_ROLE_INTERNAL_FRAME: A frame-like object that is clipped by a desktop
 *pane
 *@ATK_ROLE_LABEL: An object used to present an icon or short string in an
 *interface
 *@ATK_ROLE_LAYERED_PANE: A specialized pane that allows its children to be
 *drawn in layers, providing a form of stacking order
 *@ATK_ROLE_LIST: An object that presents a list of objects to the user and
 *allows the user to select one or more of them
 *@ATK_ROLE_LIST_ITEM: An object that represents an element of a list
 *@ATK_ROLE_MENU: An object usually found inside a menu bar that contains a list
 *of actions the user can choose from
 *@ATK_ROLE_MENU_BAR: An object usually drawn at the top of the primary dialog
 *box of an application that contains a list of menus the user can choose from
 *@ATK_ROLE_MENU_ITEM: An object usually contained in a menu that presents an
 *action the user can choose
 *@ATK_ROLE_OPTION_PANE: A specialized pane whose primary use is inside a DIALOG
 *@ATK_ROLE_PAGE_TAB: An object that is a child of a page tab list
 *@ATK_ROLE_PAGE_TAB_LIST: An object that presents a series of panels (or page
 *tabs), one at a time, through some mechanism provided by the object
 *@ATK_ROLE_PANEL: A generic container that is often used to group objects
 *@ATK_ROLE_PASSWORD_TEXT: A text object uses for passwords, or other places
 *where the text content is not shown visibly to the user
 *@ATK_ROLE_POPUP_MENU: A temporary window that is usually used to offer the
 *user a list of choices, and then hides when the user selects one of those
 *choices
 *@ATK_ROLE_PROGRESS_BAR: An object used to indicate how much of a task has been
 *completed
 *@ATK_ROLE_PUSH_BUTTON: An object the user can manipulate to tell the
 *application to do something
 *@ATK_ROLE_RADIO_BUTTON: A specialized check box that will cause other radio
 *buttons in the same group to become unchecked when this one is checked
 *@ATK_ROLE_RADIO_MENU_ITEM: A check menu item which belongs to a group. At each
 *instant exactly one of the radio menu items from a group is selected
 *@ATK_ROLE_ROOT_PANE: A specialized pane that has a glass pane and a layered
 *pane as its children
 *@ATK_ROLE_ROW_HEADER: The header for a row of data
 *@ATK_ROLE_SCROLL_BAR: An object usually used to allow a user to incrementally
 *view a large amount of data.
 *@ATK_ROLE_SCROLL_PANE: An object that allows a user to incrementally view a
 *large amount of information
 *@ATK_ROLE_SEPARATOR: An object usually contained in a menu to provide a
 *visible and logical separation of the contents in a menu
 *@ATK_ROLE_SLIDER: An object that allows the user to select from a bounded
 *range
 *@ATK_ROLE_SPLIT_PANE: A specialized panel that presents two other panels at
 *the same time
 *@ATK_ROLE_SPIN_BUTTON: An object used to get an integer or floating point
 *number from the user
 *@ATK_ROLE_STATUSBAR: An object which reports messages of minor importance to
 *the user
 *@ATK_ROLE_TABLE: An object used to represent information in terms of rows and
 *columns
 *@ATK_ROLE_TABLE_CELL: A cell in a table
 *@ATK_ROLE_TABLE_COLUMN_HEADER: The header for a column of a table
 *@ATK_ROLE_TABLE_ROW_HEADER: The header for a row of a table
 *@ATK_ROLE_TEAR_OFF_MENU_ITEM: A menu item used to tear off and reattach its
 *menu
 *@ATK_ROLE_TERMINAL: An object that represents an accessible terminal.  @Since:
 *ATK-0.6
 *@ATK_ROLE_TEXT: An interactive widget that supports multiple lines of text and
 * optionally accepts user input, but whose purpose is not to solicit user
 *input. Thus ATK_ROLE_TEXT is appropriate for the text view in a plain text
 *editor but inappropriate for an input field in a dialog box or web form. For
 *widgets whose purpose is to solicit input from the user, see ATK_ROLE_ENTRY
 *and ATK_ROLE_PASSWORD_TEXT. For generic objects which display a brief amount
 *of textual information, see ATK_ROLE_STATIC.
 *@ATK_ROLE_TOGGLE_BUTTON: A specialized push button that can be checked or
 *unchecked, but does not provide a separate indicator for the current state
 *@ATK_ROLE_TOOL_BAR: A bar or palette usually composed of push buttons or
 *toggle buttons
 *@ATK_ROLE_TOOL_TIP: An object that provides information about another object
 *@ATK_ROLE_TREE: An object used to represent hierarchical information to the
 *user
 *@ATK_ROLE_TREE_TABLE: An object capable of expanding and collapsing rows as
 *well as showing multiple columns of data.   @Since: ATK-0.7
 *@ATK_ROLE_UNKNOWN: The object contains some Accessible information, but its
 *role is not known
 *@ATK_ROLE_VIEWPORT: An object usually used in a scroll pane
 *@ATK_ROLE_WINDOW: A top level window with no title or border.
 *@ATK_ROLE_HEADER: An object that serves as a document header. @Since:
 *ATK-1.1.1
 *@ATK_ROLE_FOOTER: An object that serves as a document footer.  @Since:
 *ATK-1.1.1
 *@ATK_ROLE_PARAGRAPH: An object which is contains a paragraph of text content.
 *@Since: ATK-1.1.1
 *@ATK_ROLE_RULER: An object which describes margins and tab stops, etc. for
 *text objects which it controls (should have CONTROLLER_FOR relation to such).
 *@Since: ATK-1.1.1
 *@ATK_ROLE_APPLICATION: The object is an application object, which may contain
 *@ATK_ROLE_FRAME objects or other types of accessibles.  The root accessible of
 *any application's ATK hierarchy should have ATK_ROLE_APPLICATION.   @Since:
 *ATK-1.1.4
 *@ATK_ROLE_AUTOCOMPLETE: The object is a dialog or list containing items for
 *insertion into an entry widget, for instance a list of words for completion of
 *a text entry.   @Since: ATK-1.3
 *@ATK_ROLE_EDITBAR: The object is an editable text object in a toolbar. @Since:
 *ATK-1.5
 *@ATK_ROLE_EMBEDDED: The object is an embedded container within a document or
 *panel.  This role is a grouping "hint" indicating that the contained objects
 *share a context.  @Since: ATK-1.7.2
 *@ATK_ROLE_ENTRY: The object is a component whose textual content may be
 *entered or modified by the user, provided @ATK_STATE_EDITABLE is present.
 *@Since: ATK-1.11
 *@ATK_ROLE_CHART: The object is a graphical depiction of quantitative data. It
 *may contain multiple subelements whose attributes and/or description may be
 *queried to obtain both the quantitative data and information about how the
 *data is being presented. The LABELLED_BY relation is particularly important in
 *interpreting objects of this type, as is the accessible-description property.
 *@Since: ATK-1.11
 *@ATK_ROLE_CAPTION: The object contains descriptive information, usually
 *textual, about another user interface element such as a table, chart, or
 *image.  @Since: ATK-1.11
 *@ATK_ROLE_DOCUMENT_FRAME: The object is a visual frame or container which
 *contains a view of document content. Document frames may occur within another
 *Document instance, in which case the second document may be said to be
 *embedded in the containing instance. HTML frames are often
 *ROLE_DOCUMENT_FRAME. Either this object, or a singleton descendant, should
 *implement the Document interface.  @Since: ATK-1.11
 *@ATK_ROLE_HEADING: The object serves as a heading for content which follows it
 *in a document. The 'heading level' of the heading, if availabe, may be
 *obtained by querying the object's attributes.
 *@ATK_ROLE_PAGE: The object is a containing instance which encapsulates a page
 *of information. @ATK_ROLE_PAGE is used in documents and content which support
 *a paginated navigation model.  @Since: ATK-1.11
 *@ATK_ROLE_SECTION: The object is a containing instance of document content
 *which constitutes a particular 'logical' section of the document. The type of
 *content within a section, and the nature of the section division itself, may
 *be obtained by querying the object's attributes. Sections may be nested.
 *@Since: ATK-1.11
 *@ATK_ROLE_REDUNDANT_OBJECT: The object is redundant with another object in the
 *hierarchy, and is exposed for purely technical reasons.  Objects of this role
 *should normally be ignored by clients. @Since: ATK-1.11
 *@ATK_ROLE_FORM: The object is a container for form controls, for instance as
 *part of a web form or user-input form within a document.  This role is
 *primarily a tag/convenience for clients when navigating complex documents, it
 *is not expected that ordinary GUI containers will always have ATK_ROLE_FORM.
 *@Since: ATK-1.12.0
 *@ATK_ROLE_LINK: The object is a hypertext anchor, i.e. a "link" in a
 * hypertext document.  Such objects are distinct from 'inline'
 * content which may also use the Hypertext/Hyperlink interfaces
 * to indicate the range/location within a text object where
 * an inline or embedded object lies.  @Since: ATK-1.12.1
 *@ATK_ROLE_INPUT_METHOD_WINDOW: The object is a window or similar viewport
 * which is used to allow composition or input of a 'complex character',
 * in other words it is an "input method window." @Since: ATK-1.12.1
 *@ATK_ROLE_TABLE_ROW: A row in a table.  @Since: ATK-2.1.0
 *@ATK_ROLE_TREE_ITEM: An object that represents an element of a tree.  @Since:
 *ATK-2.1.0
 *@ATK_ROLE_DOCUMENT_SPREADSHEET: A document frame which contains a spreadsheet.
 *@Since: ATK-2.1.0
 *@ATK_ROLE_DOCUMENT_PRESENTATION: A document frame which contains a
 *presentation or slide content.  @Since: ATK-2.1.0
 *@ATK_ROLE_DOCUMENT_TEXT: A document frame which contains textual content, such
 *as found in a word processing application.  @Since: ATK-2.1.0
 *@ATK_ROLE_DOCUMENT_WEB: A document frame which contains HTML or other markup
 *suitable for display in a web browser.  @Since: ATK-2.1.0
 *@ATK_ROLE_DOCUMENT_EMAIL: A document frame which contains email content to be
 *displayed or composed either in plain text or HTML.  @Since: ATK-2.1.0
 *@ATK_ROLE_COMMENT: An object found within a document and designed to present a
 *comment, note, or other annotation. In some cases, this object might not be
 *visible until activated.  @Since: ATK-2.1.0
 *@ATK_ROLE_LIST_BOX: A non-collapsible list of choices the user can select
 *from. @Since: ATK-2.1.0
 *@ATK_ROLE_GROUPING: A group of related widgets. This group typically has a
 *label. @Since: ATK-2.1.0
 *@ATK_ROLE_IMAGE_MAP: An image map object. Usually a graphic with multiple
 *hotspots, where each hotspot can be activated resulting in the loading of
 *another document or section of a document. @Since: ATK-2.1.0
 *@ATK_ROLE_NOTIFICATION: A transitory object designed to present a message to
 *the user, typically at the desktop level rather than inside a particular
 *application.  @Since: ATK-2.1.0
 *@ATK_ROLE_INFO_BAR: An object designed to present a message to the user within
 *an existing window. @Since: ATK-2.1.0
 *@ATK_ROLE_LEVEL_BAR: A bar that serves as a level indicator to, for instance,
 *show the strength of a password or the state of a battery.  @Since: ATK-2.7.3
 *@ATK_ROLE_TITLE_BAR: A bar that serves as the title of a window or a
 * dialog. @Since: ATK-2.12
 *@ATK_ROLE_BLOCK_QUOTE: An object which contains a text section
 * that is quoted from another source. @Since: ATK-2.12
 *@ATK_ROLE_AUDIO: An object which represents an audio element. @Since: ATK-2.12
 *@ATK_ROLE_VIDEO: An object which represents a video element. @Since: ATK-2.12
 *@ATK_ROLE_DEFINITION: A definition of a term or concept. @Since: ATK-2.12
 *@ATK_ROLE_ARTICLE: A section of a page that consists of a
 * composition that forms an independent part of a document, page, or
 * site. Examples: A blog entry, a news story, a forum post. @Since:
 * ATK-2.12
 *@ATK_ROLE_LANDMARK: A region of a web page intended as a
 * navigational landmark. This is designed to allow Assistive
 * Technologies to provide quick navigation among key regions within a
 * document. @Since: ATK-2.12
 *@ATK_ROLE_LOG: A text widget or container holding log content, such
 * as chat history and error logs. In this role there is a
 * relationship between the arrival of new items in the log and the
 * reading order. The log contains a meaningful sequence and new
 * information is added only to the end of the log, not at arbitrary
 * points. @Since: ATK-2.12
 *@ATK_ROLE_MARQUEE: A container where non-essential information
 * changes frequently. Common usages of marquee include stock tickers
 * and ad banners. The primary difference between a marquee and a log
 * is that logs usually have a meaningful order or sequence of
 * important content changes. @Since: ATK-2.12
 *@ATK_ROLE_MATH: A text widget or container that holds a mathematical
 * expression. @Since: ATK-2.12
 *@ATK_ROLE_RATING: A widget whose purpose is to display a rating,
 * such as the number of stars associated with a song in a media
 * player. Objects of this role should also implement
 * AtkValue. @Since: ATK-2.12
 *@ATK_ROLE_TIMER: An object containing a numerical counter which
 * indicates an amount of elapsed time from a start point, or the time
 * remaining until an end point. @Since: ATK-2.12
 *@ATK_ROLE_DESCRIPTION_LIST: An object that represents a list of
 * term-value groups. A term-value group represents a individual
 * description and consist of one or more names
 * (ATK_ROLE_DESCRIPTION_TERM) followed by one or more values
 * (ATK_ROLE_DESCRIPTION_VALUE). For each list, there should not be
 * more than one group with the same term name. @Since: ATK-2.12
 *@ATK_ROLE_DESCRIPTION_TERM: An object that represents the term, or
 * name, part of a term-description group in a description
 * list. @Since: ATK-2.12
 *@ATK_ROLE_DESCRIPTION_VALUE: An object that represents the
 * description, definition or value of a term-description group in a
 * description list. The values within a group are alternatives,
 * meaning that you can have several ATK_ROLE_DESCRIPTION_VALUE for a
 * given ATK_ROLE_DESCRIPTION_TERM. @Since: ATK-2.12
 *@ATK_ROLE_STATIC: A generic non-container object whose purpose is to display a
 * brief amount of information to the user and whose role is known by the
 * implementor but lacks semantic value for the user. Examples in which
 * ATK_ROLE_STATIC is appropriate include the message displayed in a message box
 * and an image used as an alternative means to display text. ATK_ROLE_STATIC
 * should not be applied to widgets which are traditionally interactive, objects
 * which display a significant amount of content, or any object which has an
 * accessible relation pointing to another object. Implementors should expose
 *the displayed information through the accessible name of the object. If doing
 *so seems inappropriate, it may indicate that a different role should be used.
 *For labels which describe another widget, see ATK_ROLE_LABEL. For text views,
 *see ATK_ROLE_TEXT. For generic containers, see ATK_ROLE_PANEL. For objects
 *whose role is not known by the implementor, see ATK_ROLE_UNKNOWN. @Since:
 *ATK-2.16.
 *@ATK_ROLE_MATH_FRACTION: An object that represents a mathematical fraction.
 * @Since: ATK-2.16.
 *@ATK_ROLE_MATH_ROOT: An object that represents a mathematical expression
 * displayed with a radical. @Since: ATK-2.16.
 *@ATK_ROLE_SUBSCRIPT: An object that contains text that is displayed as a
 * subscript. @Since: ATK-2.16.
 *@ATK_ROLE_SUPERSCRIPT: An object that contains text that is displayed as a
 * superscript. @Since: ATK-2.16.
 *@ATK_ROLE_FOOTNOTE: An object that contains the text of a footnote. @Since:
 *ATK-2.26.
 *@ATK_ROLE_CONTENT_DELETION: Content previously deleted or proposed to be
 * deleted, e.g. in revision history or a content view providing suggestions
 * from reviewers. (Since: 2.34)
 *@ATK_ROLE_CONTENT_INSERTION: Content previously inserted or proposed to be
 * inserted, e.g. in revision history or a content view providing suggestions
 * from reviewers. (Since: 2.34)
 *@ATK_ROLE_MARK: A run of content that is marked or highlighted, such as for
 * reference purposes, or to call it out as having a special purpose. If the
 * marked content has an associated section in the document elaborating on the
 * reason for the mark, then %ATK_RELATION_DETAILS should be used on the mark
 * to point to that associated section. In addition, the reciprocal relation
 * %ATK_RELATION_DETAILS_FOR should be used on the associated content section
 * to point back to the mark. (Since: 2.36)
 *@ATK_ROLE_SUGGESTION: A container for content that is called out as a proposed
 * change from the current version of the document, such as by a reviewer of the
 * content. This role should include either %ATK_ROLE_CONTENT_DELETION and/or
 * %ATK_ROLE_CONTENT_INSERTION children, in any order, to indicate what the
 * actual change is. (Since: 2.36)
 *@ATK_ROLE_LAST_DEFINED: not a valid role, used for finding end of the
 *enumeration
 *
 * Describes the role of an object
 *
 * These are the built-in enumerated roles that UI components can have in
 * ATK.  Other roles may be added at runtime, so an AtkRole >=
 * ATK_ROLE_LAST_DEFINED is not necessarily an error.
 **/
typedef enum {
  ATK_ROLE_INVALID = 0,
  ATK_ROLE_ACCEL_LABEL, /*<nick=accelerator-label>*/
  ATK_ROLE_ALERT,
  ATK_ROLE_ANIMATION,
  ATK_ROLE_ARROW,
  ATK_ROLE_CALENDAR,
  ATK_ROLE_CANVAS,
  ATK_ROLE_CHECK_BOX,
  ATK_ROLE_CHECK_MENU_ITEM,
  ATK_ROLE_COLOR_CHOOSER,
  ATK_ROLE_COLUMN_HEADER,
  ATK_ROLE_COMBO_BOX,
  ATK_ROLE_DATE_EDITOR,
  ATK_ROLE_DESKTOP_ICON,
  ATK_ROLE_DESKTOP_FRAME,
  ATK_ROLE_DIAL,
  ATK_ROLE_DIALOG,
  ATK_ROLE_DIRECTORY_PANE,
  ATK_ROLE_DRAWING_AREA,
  ATK_ROLE_FILE_CHOOSER,
  ATK_ROLE_FILLER,
  ATK_ROLE_FONT_CHOOSER,
  ATK_ROLE_FRAME,
  ATK_ROLE_GLASS_PANE,
  ATK_ROLE_HTML_CONTAINER,
  ATK_ROLE_ICON,
  ATK_ROLE_IMAGE,
  ATK_ROLE_INTERNAL_FRAME,
  ATK_ROLE_LABEL,
  ATK_ROLE_LAYERED_PANE,
  ATK_ROLE_LIST,
  ATK_ROLE_LIST_ITEM,
  ATK_ROLE_MENU,
  ATK_ROLE_MENU_BAR,
  ATK_ROLE_MENU_ITEM,
  ATK_ROLE_OPTION_PANE,
  ATK_ROLE_PAGE_TAB,
  ATK_ROLE_PAGE_TAB_LIST,
  ATK_ROLE_PANEL,
  ATK_ROLE_PASSWORD_TEXT,
  ATK_ROLE_POPUP_MENU,
  ATK_ROLE_PROGRESS_BAR,
  ATK_ROLE_PUSH_BUTTON,
  ATK_ROLE_RADIO_BUTTON,
  ATK_ROLE_RADIO_MENU_ITEM,
  ATK_ROLE_ROOT_PANE,
  ATK_ROLE_ROW_HEADER,
  ATK_ROLE_SCROLL_BAR,
  ATK_ROLE_SCROLL_PANE,
  ATK_ROLE_SEPARATOR,
  ATK_ROLE_SLIDER,
  ATK_ROLE_SPLIT_PANE,
  ATK_ROLE_SPIN_BUTTON,
  ATK_ROLE_STATUSBAR,
  ATK_ROLE_TABLE,
  ATK_ROLE_TABLE_CELL,
  ATK_ROLE_TABLE_COLUMN_HEADER,
  ATK_ROLE_TABLE_ROW_HEADER,
  ATK_ROLE_TEAR_OFF_MENU_ITEM,
  ATK_ROLE_TERMINAL,
  ATK_ROLE_TEXT,
  ATK_ROLE_TOGGLE_BUTTON,
  ATK_ROLE_TOOL_BAR,
  ATK_ROLE_TOOL_TIP,
  ATK_ROLE_TREE,
  ATK_ROLE_TREE_TABLE,
  ATK_ROLE_UNKNOWN,
  ATK_ROLE_VIEWPORT,
  ATK_ROLE_WINDOW,
  ATK_ROLE_HEADER,
  ATK_ROLE_FOOTER,
  ATK_ROLE_PARAGRAPH,
  ATK_ROLE_RULER,
  ATK_ROLE_APPLICATION,
  ATK_ROLE_AUTOCOMPLETE,
  ATK_ROLE_EDITBAR,
  ATK_ROLE_EMBEDDED,
  ATK_ROLE_ENTRY,
  ATK_ROLE_CHART,
  ATK_ROLE_CAPTION,
  ATK_ROLE_DOCUMENT_FRAME,
  ATK_ROLE_HEADING,
  ATK_ROLE_PAGE,
  ATK_ROLE_SECTION,
  ATK_ROLE_REDUNDANT_OBJECT,
  ATK_ROLE_FORM,
  ATK_ROLE_LINK,
  ATK_ROLE_INPUT_METHOD_WINDOW,
  ATK_ROLE_TABLE_ROW,
  ATK_ROLE_TREE_ITEM,
  ATK_ROLE_DOCUMENT_SPREADSHEET,
  ATK_ROLE_DOCUMENT_PRESENTATION,
  ATK_ROLE_DOCUMENT_TEXT,
  ATK_ROLE_DOCUMENT_WEB,
  ATK_ROLE_DOCUMENT_EMAIL,
  ATK_ROLE_COMMENT,
  ATK_ROLE_LIST_BOX,
  ATK_ROLE_GROUPING,
  ATK_ROLE_IMAGE_MAP,
  ATK_ROLE_NOTIFICATION,
  ATK_ROLE_INFO_BAR,
  ATK_ROLE_LEVEL_BAR,
  ATK_ROLE_TITLE_BAR,
  ATK_ROLE_BLOCK_QUOTE,
  ATK_ROLE_AUDIO,
  ATK_ROLE_VIDEO,
  ATK_ROLE_DEFINITION,
  ATK_ROLE_ARTICLE,
  ATK_ROLE_LANDMARK,
  ATK_ROLE_LOG,
  ATK_ROLE_MARQUEE,
  ATK_ROLE_MATH,
  ATK_ROLE_RATING,
  ATK_ROLE_TIMER,
  ATK_ROLE_DESCRIPTION_LIST,
  ATK_ROLE_DESCRIPTION_TERM,
  ATK_ROLE_DESCRIPTION_VALUE,
  ATK_ROLE_STATIC,
  ATK_ROLE_MATH_FRACTION,
  ATK_ROLE_MATH_ROOT,
  ATK_ROLE_SUBSCRIPT,
  ATK_ROLE_SUPERSCRIPT,
  ATK_ROLE_FOOTNOTE,
  ATK_ROLE_CONTENT_DELETION,
  ATK_ROLE_CONTENT_INSERTION,
  ATK_ROLE_MARK,
  ATK_ROLE_SUGGESTION,
  ATK_ROLE_LAST_DEFINED
} AtkRole;

AtkRole atk_role_register(const gchar* name);

/**
 *AtkLayer:
 *@ATK_LAYER_INVALID: The object does not have a layer
 *@ATK_LAYER_BACKGROUND: This layer is reserved for the desktop background
 *@ATK_LAYER_CANVAS: This layer is used for Canvas components
 *@ATK_LAYER_WIDGET: This layer is normally used for components
 *@ATK_LAYER_MDI: This layer is used for layered components
 *@ATK_LAYER_POPUP: This layer is used for popup components, such as menus
 *@ATK_LAYER_OVERLAY: This layer is reserved for future use.
 *@ATK_LAYER_WINDOW: This layer is used for toplevel windows.
 *
 * Describes the layer of a component
 *
 * These enumerated "layer values" are used when determining which UI
 * rendering layer a component is drawn into, which can help in making
 * determinations of when components occlude one another.
 **/
typedef enum {
  ATK_LAYER_INVALID,
  ATK_LAYER_BACKGROUND,
  ATK_LAYER_CANVAS,
  ATK_LAYER_WIDGET,
  ATK_LAYER_MDI,
  ATK_LAYER_POPUP,
  ATK_LAYER_OVERLAY,
  ATK_LAYER_WINDOW
} AtkLayer;

/**
 * AtkAttributeSet:
 *
 * This is a singly-linked list (a #GSList) of #AtkAttribute. It is
 * used by atk_text_get_run_attributes(), atk_text_get_default_attributes()
 * and atk_editable_text_set_run_attributes()
 **/
typedef GSList AtkAttributeSet;

/**
 * AtkAttribute:
 * @name: The attribute name. Call atk_text_attr_get_name()
 * @value: the value of the attribute, represented as a string.
 * Call atk_text_attr_get_value() for those which are strings.
 * For values which are numbers, the string representation of the number
 * is in value.
 *
 * A string name/value pair representing a text attribute.
 **/
typedef struct _AtkAttribute AtkAttribute;

struct _AtkAttribute {
  gchar* name;
  gchar* value;
};

#define ATK_TYPE_OBJECT (atk_object_get_type())
#define ATK_OBJECT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), ATK_TYPE_OBJECT, AtkObject))
#define ATK_OBJECT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), ATK_TYPE_OBJECT, AtkObjectClass))
#define ATK_IS_OBJECT(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), ATK_TYPE_OBJECT))
#define ATK_IS_OBJECT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), ATK_TYPE_OBJECT))
#define ATK_OBJECT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), ATK_TYPE_OBJECT, AtkObjectClass))

#define ATK_TYPE_IMPLEMENTOR (atk_implementor_get_type())
#define ATK_IS_IMPLEMENTOR(obj) \
  G_TYPE_CHECK_INSTANCE_TYPE((obj), ATK_TYPE_IMPLEMENTOR)
#define ATK_IMPLEMENTOR(obj) \
  G_TYPE_CHECK_INSTANCE_CAST((obj), ATK_TYPE_IMPLEMENTOR, AtkImplementor)
#define ATK_IMPLEMENTOR_GET_IFACE(obj)                        \
  (G_TYPE_INSTANCE_GET_INTERFACE((obj), ATK_TYPE_IMPLEMENTOR, \
                                 AtkImplementorIface))

typedef struct _AtkImplementor AtkImplementor; /* dummy typedef */
typedef struct _AtkImplementorIface AtkImplementorIface;

typedef struct _AtkObject AtkObject;
typedef struct _AtkObjectClass AtkObjectClass;
typedef struct _AtkRelationSet AtkRelationSet;
typedef struct _AtkStateSet AtkStateSet;

/**
 * AtkPropertyValues:
 * @property_name: The name of the ATK property which is being presented or
 *which has been changed.
 * @old_value: The old property value, NULL; in some contexts this value is
 *undefined (see note below).
 * @new_value: The new value of the named property.
 *
 * @note: For most properties the old_value field of AtkPropertyValues will
 * not contain a valid value.
 *
 * Currently, the only property for which old_value is used is
 * accessible-state; for instance if there is a focus state the
 * property change handler will be called for the object which lost the focus
 * with the old_value containing an AtkState value corresponding to focused
 * and the property change handler will be called for the object which
 * received the focus with the new_value containing an AtkState value
 * corresponding to focused.
 *
 **/
struct _AtkPropertyValues {
  const gchar* property_name;
  GValue old_value;
  GValue new_value;
};

typedef struct _AtkPropertyValues AtkPropertyValues;

typedef gboolean (*AtkFunction)(gpointer data);
/*
 * For most properties the old_value field of AtkPropertyValues will
 * not contain a valid value.
 *
 * Currently, the only property for which old_value is used is
 * accessible-state; for instance if there is a focus state the
 * property change handler will be called for the object which lost the focus
 * with the old_value containing an AtkState value corresponding to focused
 * and the property change handler will be called for the object which
 * received the focus with the new_value containing an AtkState value
 * corresponding to focused.
 */
typedef void (*AtkPropertyChangeHandler)(AtkObject*, AtkPropertyValues*);

struct _AtkObject {
  GObject parent;

  gchar* description;
  gchar* name;
  AtkObject* accessible_parent;
  AtkRole role;
  AtkRelationSet* relation_set;
  AtkLayer layer;
};

struct _AtkObjectClass {
  GObjectClass parent;

  /*
   * Gets the accessible name of the object
   */
  G_CONST_RETURN gchar* (*get_name)(AtkObject* accessible);
  /*
   * Gets the accessible description of the object
   */
  G_CONST_RETURN gchar* (*get_description)(AtkObject* accessible);
  /*
   * Gets the accessible parent of the object
   */
  AtkObject* (*get_parent)(AtkObject* accessible);

  /*
   * Gets the number of accessible children of the object
   */
  gint (*get_n_children)(AtkObject* accessible);
  /*
   * Returns a reference to the specified accessible child of the object.
   * The accessible children are 0-based so the first accessible child is
   * at index 0, the second at index 1 and so on.
   */
  AtkObject* (*ref_child)(AtkObject* accessible, gint i);
  /*
   * Gets the 0-based index of this object in its parent; returns -1 if the
   * object does not have an accessible parent.
   */
  gint (*get_index_in_parent)(AtkObject* accessible);
  /*
   * Gets the RelationSet associated with the object
   */
  AtkRelationSet* (*ref_relation_set)(AtkObject* accessible);
  /*
   * Gets the role of the object
   */
  AtkRole (*get_role)(AtkObject* accessible);
  AtkLayer (*get_layer)(AtkObject* accessible);
  gint (*get_mdi_zorder)(AtkObject* accessible);
  /*
   * Gets the state set of the object
   */
  AtkStateSet* (*ref_state_set)(AtkObject* accessible);
  /*
   * Sets the accessible name of the object
   */
  void (*set_name)(AtkObject* accessible, const gchar* name);
  /*
   * Sets the accessible description of the object
   */
  void (*set_description)(AtkObject* accessible, const gchar* description);
  /*
   * Sets the accessible parent of the object
   */
  void (*set_parent)(AtkObject* accessible, AtkObject* parent);
  /*
   * Sets the accessible role of the object
   */
  void (*set_role)(AtkObject* accessible, AtkRole role);
  /*
   * Specifies a function to be called when a property changes value
   */
  guint (*connect_property_change_handler)(AtkObject* accessible,
                                           AtkPropertyChangeHandler* handler);
  /*
   * Removes a property change handler which was specified using
   * connect_property_change_handler
   */
  void (*remove_property_change_handler)(AtkObject* accessible,
                                         guint handler_id);
  void (*initialize)(AtkObject* accessible, gpointer data);
  /*
   * The signal handler which is executed when there is a change in the
   * children of the object
   */
  void (*children_changed)(AtkObject* accessible, guint change_index,
                           gpointer changed_child);
  /*
   * The signal handler which is executed  when there is a focus event
   * for an object.
   */
  void (*focus_event)(AtkObject* accessible, gboolean focus_in);
  /*
   * The signal handler which is executed  when there is a property_change
   * signal for an object.
   */
  void (*property_change)(AtkObject* accessible, AtkPropertyValues* values);
  /*
   * The signal handler which is executed  when there is a state_change
   * signal for an object.
   */
  void (*state_change)(AtkObject* accessible, const gchar* name,
                       gboolean state_set);
  /*
   * The signal handler which is executed when there is a change in the
   * visible data for an object
   */
  void (*visible_data_changed)(AtkObject* accessible);

  /*
   * The signal handler which is executed when there is a change in the
   * 'active' child or children of the object, for instance when
   * interior focus changes in a table or list.  This signal should be emitted
   * by objects whose state includes ATK_STATE_MANAGES_DESCENDANTS.
   */
  void (*active_descendant_changed)(AtkObject* accessible, gpointer* child);

  /*
   * Gets a list of properties applied to this object as a whole, as an
   * #AtkAttributeSet consisting of name-value pairs. Since ATK 1.12
   */
  AtkAttributeSet* (*get_attributes)(AtkObject* accessible);

  const gchar* (*get_object_locale)(AtkObject* accessible);

  AtkFunction pad1;
};

GType atk_object_get_type(void);

struct _AtkImplementorIface {
  GTypeInterface parent;

  AtkObject* (*ref_accessible)(AtkImplementor* implementor);
};
GType atk_implementor_get_type(void);

/*
 * This method uses the ref_accessible method in AtkImplementorIface,
 * if the object's class implements AtkImplementorIface.
 * Otherwise it returns %NULL.
 *
 * IMPORTANT:
 * Note also that because this method may return flyweight objects,
 * it increments the returned AtkObject's reference count.
 * Therefore it is the responsibility of the calling
 * program to unreference the object when no longer needed.
 * (c.f. gtk_widget_get_accessible() where this is not the case).
 */
AtkObject* atk_implementor_ref_accessible(AtkImplementor* implementor);

/*
 * Properties directly supported by AtkObject
 */

G_CONST_RETURN gchar* atk_object_get_name(AtkObject* accessible);
G_CONST_RETURN gchar* atk_object_get_description(AtkObject* accessible);
AtkObject* atk_object_get_parent(AtkObject* accessible);
gint atk_object_get_n_accessible_children(AtkObject* accessible);
AtkObject* atk_object_ref_accessible_child(AtkObject* accessible, gint i);
AtkRelationSet* atk_object_ref_relation_set(AtkObject* accessible);
AtkRole atk_object_get_role(AtkObject* accessible);
AtkLayer atk_object_get_layer(AtkObject* accessible);
gint atk_object_get_mdi_zorder(AtkObject* accessible);
AtkAttributeSet* atk_object_get_attributes(AtkObject* accessible);
AtkStateSet* atk_object_ref_state_set(AtkObject* accessible);
gint atk_object_get_index_in_parent(AtkObject* accessible);
void atk_object_set_name(AtkObject* accessible, const gchar* name);
void atk_object_set_description(AtkObject* accessible,
                                const gchar* description);
void atk_object_set_parent(AtkObject* accessible, AtkObject* parent);
void atk_object_set_role(AtkObject* accessible, AtkRole role);

guint atk_object_connect_property_change_handler(
    AtkObject* accessible, AtkPropertyChangeHandler* handler);
void atk_object_remove_property_change_handler(AtkObject* accessible,
                                               guint handler_id);

void atk_object_notify_state_change(AtkObject* accessible, AtkState state,
                                    gboolean value);
void atk_object_initialize(AtkObject* accessible, gpointer data);

G_CONST_RETURN gchar* atk_role_get_name(AtkRole role);
AtkRole atk_role_for_name(const gchar* name);

/* NEW in 1.1: convenience API */
gboolean atk_object_add_relationship(AtkObject* object,
                                     AtkRelationType relationship,
                                     AtkObject* target);
gboolean atk_object_remove_relationship(AtkObject* object,
                                        AtkRelationType relationship,
                                        AtkObject* target);
G_CONST_RETURN gchar* atk_role_get_localized_name(AtkRole role);

/* */

/*
 * Note: the properties which are registered with the GType
 *   property registry, for type ATK_TYPE_OBJECT, are as follows:
 *
 *   "accessible-name"
 *   "accessible-description"
 *   "accessible-parent"
 *   "accessible-role"
 *   "accessible-value"
 *   "accessible-component-layer"
 *   "accessible-component-zorder"
 *   "accessible-table-caption"
 *   "accessible-table-column-description"
 *   "accessible-table-column-header"
 *   "accessible-table-row-description"
 *   "accessible-table-row-header"
 *   "accessible-table-summary"
 *   "accessible-model"
 *
 * accessibility property change listeners should use the
 *   normal GObject property interfaces and "property-change"
 *   signal handler semantics to interpret the property change
 *   information relayed from AtkObject.
 *   (AtkObject instances will connect to the "notify"
 *   signal in their host objects, and relay the signals when appropriate).
 */

/* For other signals, see related interfaces
 *
 *    AtkActionIface,
 *    AtkComponentIface,
 *    AtkHypertextIface,
 *    AtkImageIface,
 *    AtkSelectionIface,
 *    AtkTableIface,
 *    AtkTextIface,
 *    AtkValueIface.
 *
 *  The usage model for obtaining these interface instances is:
 *    ATK_<interfacename>_GET_IFACE(GObject *accessible),
 *    where accessible, though specified as a GObject, is
 *    the AtkObject instance being queried.
 *  More usually, the interface will be used via a cast to the
 *    interface's corresponding "type":
 *
 *    AtkText textImpl = ATK_TEXT(accessible);
 *    if (textImpl)
 *      {
 *        cpos = atk_text_get_caret_position(textImpl);
 *      }
 *
 *  If it's known in advance that accessible implements AtkTextIface,
 *    this is shortened to:
 *
 *    cpos = atk_text_get_caret_position (ATK_TEXT (accessible));
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __ATK_OBJECT_H__ */
