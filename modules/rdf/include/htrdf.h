/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef htrdf_h___
#define htrdf_h___

/* the RDF HT API */

#include "rdf.h"
#ifdef NSPR20
#else
#include "prtypes.h"
#include "prfile.h"
#endif

#include "ntypes.h"

/*
 * Hyper Tree Api
 *
 * Hyper tree is the tree view of the rdf store. This is for use by the FE in
 * implementing the nav center.
 *
 * The RDF structure could be a gnarly graph whose nodes are RDF_Resource.
 * There can be multiple tree views (HT_View) into this graph. Corresponding
 * to each view, there is a hyper tree whose nodes are HT_Resource. 
 * Each HT_Resource belongs to exactly one HT_View. 
 * The FE iterate over the hypertree to draw it. It can start the iteration
 * either at the top or at some interior node. When some state change occurs
 * in the hypertree, the FE will get notified about the node on which the
 * change took place. At that point, it can either redraw the whole thing
 * or do a partial redraw. The FE does this by iterating over the relevant
 * portions of the hypertree.  Since the hypertree is a strict tree, the FE
 * can also iterate upwards.
 *
 *
 * Possible state changes to the hypertree of an HT_View include : 
 * (i) addition/deletion of nodes
 * (ii) containers closing/opening
 * (iii) selection changes
 * These changes could occur either because of 
 * (i) User clicking around the tree
 * (ii) Network activity
 * (iii) scripts running
 * The FE can recieve notifications about these activities. 
 * If the FE does not want to be notified about something, it can set 
 * the notification mask to block certain notifications.
 */

NSPR_BEGIN_EXTERN_C

/* Opaque data structures */
typedef struct _HT_ViewStruct* HT_View;

typedef struct _HT_PaneStruct* HT_Pane;

typedef struct _HT_CursorStruct* HT_Cursor;

typedef struct _HT_ResourceStruct* HT_Resource;


/*
 * This is the Notification structure that gets passed to the HT layer on
 * creation of the view. This should be allocated/static memory. HT layer
 * will store a pointer to this structure. On CloseView, a View Closed
 * event will be generated and the notification function will be called.
 * At this point, the module using HT can free the memory associated with
 * the HT_Notification struct.
 */


typedef uint32 HT_NotificationMask;
typedef uint32 HT_Event;
typedef uint32 HT_Error;

#define HT_NoErr		0
#define HT_Err			1

struct _HT_NotificationStruct;

typedef void (*HT_NotificationProc)(struct _HT_NotificationStruct*, 
				 HT_Resource node, HT_Event whatHappened);

typedef struct _HT_NotificationStruct {
        HT_NotificationProc notifyProc;
        void* data;
} HT_NotificationStruct;


/*
 * HT_Notification events and masks
 */


typedef HT_NotificationStruct* HT_Notification;

#define HT_EVENT_NODE_ADDED                     0x00000001UL
#define HT_EVENT_NODE_DELETED_DATA              0x00000002UL
#define HT_EVENT_NODE_DELETED_NODATA            0x00000004UL
#define HT_EVENT_NODE_VPROP_CHANGED             0x00000008UL
#define HT_EVENT_NODE_SELECTION_CHANGED         0x00000010UL
#define HT_EVENT_NODE_OPENCLOSE_CHANGED         0x00000020UL
#define HT_EVENT_VIEW_CLOSED                    0x00000040UL	/* same as HT_EVENT_VIEW_DELETED */
#define HT_EVENT_VIEW_DELETED                   0x00000040UL	/* same as HT_EVENT_VIEW_CLOSED */
#define HT_EVENT_VIEW_SELECTED                  0x00000080UL
#define HT_EVENT_VIEW_ADDED                     0x00000100UL
#define HT_EVENT_NODE_OPENCLOSE_CHANGING        0x00000200UL
#define HT_EVENT_VIEW_SORTING_CHANGED		0x00000400UL
#define HT_EVENT_VIEW_REFRESH			0x00000800UL
#define	HT_EVENT_VIEW_WORKSPACE_REFRESH		0x00001000UL
#define	HT_EVENT_NODE_EDIT			0x00002000UL
#define	HT_EVENT_WORKSPACE_EDIT			0x00004000UL
#define	HT_EVENT_VIEW_HTML_ADD			0x00008000UL
#define	HT_EVENT_VIEW_HTML_REMOVE		0x00010000UL
#define HT_EVENT_NO_NOTIFICATION_MASK           0x00000000UL
#define HT_EVENT_DEFAULT_NOTIFICATION_MASK      0xFFFFFFFFUL


/*-----------------------------------------------------------------------*/
/*                    View/Pane Creation / Destruction / Management           */
/*-----------------------------------------------------------------------*/

PR_PUBLIC_API(HT_Pane) HT_PaneFromResource(RDF_Resource r, HT_Notification n, PRBool autoFlush);

/* NewQuickFilePane
 * Creates a pane consisting of one view.  This view has the RDF resource
 * corresponding to the Quickfile folder as its root.
 */

PR_PUBLIC_API(HT_Pane) HT_NewQuickFilePane (HT_Notification notify);

/* NewPersonalToolbarPane
 * Creates a pane consisting of one view.  This view has the RDF resource
 * corresponding to the Personal Toolbar folder as its root.
 */

PR_PUBLIC_API(HT_Pane) HT_NewPersonalToolbarPane (HT_Notification notify);

/* HT_NewBreadcrumbPane
 */

PR_PUBLIC_API(HT_Pane) HT_NewBreadcrumbPane (HT_Notification notify);

PR_PUBLIC_API(void) HT_AddToContainer (HT_Resource container, char *url, char *optionalTitle);
PR_PUBLIC_API(void) HT_AddBookmark (char *url, char *optionalTitle);

/* CreateView
 * Takes an rdf node as the root of the view tree and creates a XP view.
 * HT_Notification would be to the notifier when there is a state change.
 */

PR_PUBLIC_API(HT_Pane) HT_NewPane (HT_Notification notify);

/* DeleteView
 * Destroy a valid view created via CreateView
 */
PR_PUBLIC_API(HT_Error)	HT_DeleteView (HT_View view);
PR_PUBLIC_API(HT_Error) HT_DeletePane (HT_Pane pane);

/* HT_TopNode
 * Obtain the top node associated with a view tree
 */
PR_PUBLIC_API(HT_Resource)  HT_TopNode (HT_View view);

PR_PUBLIC_API(void*) HT_GetViewFEData (HT_View node);
PR_PUBLIC_API(void) HT_SetViewFEData (HT_View node, void* data);
PR_PUBLIC_API(void*) HT_GetPaneFEData (HT_Pane pane);
PR_PUBLIC_API(void) HT_SetPaneFEData (HT_Pane pane, void* data);

PR_PUBLIC_API(HT_View) HT_GetSelectedView (HT_Pane pane);
PR_PUBLIC_API(HT_Error) HT_SetSelectedView (HT_Pane pane, HT_View view);


enum    _HT_ViewType	{
        HT_VIEW_BOOKMARK=0, HT_VIEW_HISTORY, HT_VIEW_SITEMAP
        } ;
typedef	enum	_HT_ViewType		HT_ViewType;

/*
 * HT_GetViewType: find a particular view type (returns NULL if not found or unknown)
 */
PR_PUBLIC_API(HT_View) HT_GetViewType (HT_Pane pane, HT_ViewType viewType);


/*
 * HT_GetView
 * Obtain the view tree associated with a node
 */
PR_PUBLIC_API(HT_View)      HT_GetView (HT_Resource node);
PR_PUBLIC_API(HT_Pane)      HT_GetPane (HT_View view);

/*
 * HT_GetViewData / HT_SetViewData
 * get/set FE specific data to be associated with a view
 */
PR_PUBLIC_API(void*) HT_GetNodeFEData (HT_Resource node);
PR_PUBLIC_API(void) HT_SetNodeFEData (HT_Resource node, void* data);

/*
 * HT_GetNotificationMask / HT_SetNotificationMask
 * get/set the notification mask associated with a view
 */
PR_PUBLIC_API(HT_Error) HT_GetNotificationMask (HT_Pane node, HT_NotificationMask *mask);
PR_PUBLIC_API(HT_Error) HT_SetNotificationMask (HT_Pane node, HT_NotificationMask mask);


/*-----------------------------------------------------------------------*/
/*                          View Traversal                               */
/*-----------------------------------------------------------------------*/ 

PR_PUBLIC_API(char *)	 HT_GetViewName(HT_View view);

/*
 * HT_GetNthView
 */
PR_PUBLIC_API(HT_View)  HT_GetNthView (HT_Pane pane, uint32 theIndex);
PR_PUBLIC_API(uint32)	HT_GetViewIndex(HT_View view);
PR_PUBLIC_API(uint32)	HT_GetViewListCount(HT_Pane pane);

/*
 * HT_GetNthItem / HT_GetNodeIndex
 * get the nth resource in a view (or NULL if not in view),
 * or find a resource's index in a view (or -1 if not in view)
 */
PR_PUBLIC_API(HT_Resource)  HT_GetNthItem (HT_View view, uint32 theIndex);
PR_PUBLIC_API(uint32)	HT_GetNodeIndex(HT_View view, HT_Resource node);
PR_PUBLIC_API(uint32)	HT_GetItemListCount(HT_View view);
PR_PUBLIC_API(uint16)	HT_GetItemIndentation(HT_Resource r);
/*
 * HT_GetParent
 * obtain the parent of a node
 */
PR_PUBLIC_API(HT_Resource)  HT_GetParent (HT_Resource node);

/*
 * HT_NodeDisplayString (XXX needs work)
 * obtain the name of a node
 */
PR_PUBLIC_API(HT_Error)     HT_NodeDisplayString (HT_Resource node, char *buffer, int bufferLen);
PR_PUBLIC_API(HT_Error)     HT_ViewDisplayString (HT_View view, char *buffer, int bufferLen);

PR_PUBLIC_API(PRBool)	HT_GetNodeData (HT_Resource node, void *token,
					uint32 tokenType, void **data);
PR_PUBLIC_API(PRBool)	HT_IsNodeDataEditable(HT_Resource node,
					void *token, uint32 tokenType);
PR_PUBLIC_API(HT_Error) HT_SetNodeData (HT_Resource node, void *token,
					uint32 tokenType, void *data);
PR_PUBLIC_API(HT_Error) HT_SetNodeName (HT_Resource node, void *data);

/*
 * HT_GetLargeIconURL / HT_GetSmallIconURL
 * obtain the large/small icon URLs for a node if available, otherwise return NULL
 */

PR_PUBLIC_API(char *)	HT_GetWorkspaceLargeIconURL (HT_View view);
PR_PUBLIC_API(char *)	HT_GetWorkspaceSmallIconURL (HT_View view);
PR_PUBLIC_API(char *)	HT_GetNodeLargeIconURL (HT_Resource r);
PR_PUBLIC_API(char *)	HT_GetNodeSmallIconURL (HT_Resource r);

PR_PUBLIC_API(char *)	HT_GetLargeIconURL (HT_Resource r);	/* obsolete! */
PR_PUBLIC_API(char *)	HT_GetSmallIconURL (HT_Resource r);	/* obsolete! */

/*
 * HT_NewColumnCursor / HT_GetNextColumn / HT_DeleteColumnCursor
 * obtain column information
 */

enum    _HT_ColumnType	{
        HT_COLUMN_UNKNOWN=0, HT_COLUMN_STRING, HT_COLUMN_DATE_STRING,
	HT_COLUMN_DATE_INT, HT_COLUMN_INT, HT_COLUMN_RESOURCE
        } ;
typedef	enum	_HT_ColumnType		HT_ColumnType;

PR_PUBLIC_API(HT_Cursor)	HT_NewColumnCursor (HT_View view);
PR_PUBLIC_API(PRBool)		HT_GetNextColumn(HT_Cursor cursor, char **colName,
					uint32 *colWidth, void **token, uint32 *tokenType);
PR_PUBLIC_API(void)		HT_DeleteColumnCursor(HT_Cursor cursor);
PR_PUBLIC_API(void)		HT_SetColumnOrder(HT_View view, void *srcColToken,
						void *destColToken,
						PRBool afterDestFlag);
PR_PUBLIC_API(void)		HT_SetSortColumn(HT_View view, void *token,
						uint32 tokenType, PRBool descendingFlag);
PR_PUBLIC_API(PRBool)		HT_ContainerSupportsNaturalOrderSort(HT_Resource container);


/*
 * HT Menu Commands
 */

enum    _HT_MenuCmd     {
        HT_CMD_SEPARATOR=0, HT_CMD_OPEN, HT_CMD_OPEN_FILE, HT_CMD_PRINT_FILE,
        HT_CMD_OPEN_NEW_WIN, HT_CMD_OPEN_COMPOSER, HT_CMD_OPEN_AS_WORKSPACE,
        HT_CMD_NEW_BOOKMARK, HT_CMD_NEW_FOLDER, HT_CMD_NEW_SEPARATOR,
        HT_CMD_MAKE_ALIAS, HT_CMD_ADD_TO_BOOKMARKS, HT_CMD_SAVE_AS,
        HT_CMD_CREATE_SHORTCUT, HT_CMD_SET_TOOLBAR_FOLDER,
        HT_CMD_SET_BOOKMARK_MENU, HT_CMD_SET_BOOKMARK_FOLDER, HT_CMD_CUT,
        HT_CMD_COPY, HT_CMD_PASTE, HT_CMD_DELETE_FILE, HT_CMD_DELETE_FOLDER,
        HT_CMD_REVEAL_FILEFOLDER, HT_CMD_PROPERTIES, HT_CMD_RENAME_WORKSPACE,
	HT_CMD_DELETE_WORKSPACE, HT_CMD_MOVE_WORKSPACE_UP, HT_CMD_MOVE_WORKSPACE_DOWN,
	HT_CMD_REFRESH, HT_CMD_EXPORT, HT_CMD_REMOVE_BOOKMARK_MENU,
	HT_CMD_REMOVE_BOOKMARK_FOLDER, HT_CMD_SET_PASSWORD, HT_CMD_REMOVE_PASSWORD,
	HT_CMD_EXPORTALL, HT_CMD_UNDO, HT_CMD_NEW_WORKSPACE, HT_CMD_RENAME
        };
typedef enum    _HT_MenuCmd     HT_MenuCmd;

PR_PUBLIC_API(HT_Cursor)	HT_NewContextMenuCursor(HT_Resource r);
PR_PUBLIC_API(HT_Cursor)	HT_NewContextualMenuCursor (HT_View view,
							PRBool workspaceMenuCmds,
							PRBool backgroundMenuCmds);
PR_PUBLIC_API(PRBool)		HT_NextContextMenuItem(HT_Cursor c, HT_MenuCmd *menuCmd);
PR_PUBLIC_API(void)		HT_DeleteContextMenuCursor(HT_Cursor c);
PR_PUBLIC_API(char *)		HT_GetMenuCmdName(HT_MenuCmd menuCmd);
PR_PUBLIC_API(HT_Error)		HT_DoMenuCmd(HT_Pane pane, HT_MenuCmd menuCmd);
PR_PUBLIC_API(PRBool)		HT_IsMenuCmdEnabled(HT_Pane pane, HT_MenuCmd menuCmd);


/*
 * HT_Properties
 * show HTML dialog of node's properties
 */
PR_PUBLIC_API(void)	HT_Properties (HT_Resource r);


/*
 * HT_GetRDFResource
 * obtain the RDF_Resource associated with a HT node
 */
PR_PUBLIC_API(RDF_Resource) HT_GetRDFResource (HT_Resource node);

/*
 * Access the node's name and URL
 */

PR_PUBLIC_API(char*) HT_GetNodeURL(HT_Resource node);
PR_PUBLIC_API(char*) HT_GetNodeName(HT_Resource node);

/*-----------------------------------------------------------------------*/
/*                          Accessor and Mutators                        */
/*-----------------------------------------------------------------------*/

/*
 * HT_IsSeparator
 * determine whether node is a separator
 */
PR_PUBLIC_API(PRBool) HT_IsSeparator (HT_Resource node);

/*
 * HT_IsContainer
 * determine whether node is a container
 */
PR_PUBLIC_API(PRBool)   HT_IsContainer (HT_Resource node);
PR_PUBLIC_API(uint32)	HT_GetCountVisibleChildren(HT_Resource node);

/* 
 * HT_DataSource : obtain the origin of the data
 * HT_IsLocalData : is the data local?
 */

PR_PUBLIC_API(PRBool) HT_IsLocalData (HT_Resource node) ;
PR_PUBLIC_API(char *) HT_DataSource (HT_Resource node) ;


PR_PUBLIC_API(HT_Pane) HT_GetHTPaneList ();
PR_PUBLIC_API(HT_Pane) HT_GetNextHTPane (HT_Pane pane);
/*
 * HT_IsSelected / HT_GetSelectedState / HT_SetSelectedState
 * manage selection state of a node;  get/set operations will generate
 * a HT_EVENT_NODE_SELECTION_CHANGED notification unless masked out
 */
PR_PUBLIC_API(PRBool)   HT_IsSelected (HT_Resource node);
PR_PUBLIC_API(HT_Error) HT_GetSelectedState (HT_Resource node, PRBool *selectedState);
PR_PUBLIC_API(HT_Error) HT_SetSelectedState (HT_Resource node, PRBool isSelected);

PR_PUBLIC_API(HT_Error)	HT_SetSelection (HT_Resource node);
PR_PUBLIC_API(HT_Error)	HT_SetSelectionAll (HT_View view, PRBool selectedState);
PR_PUBLIC_API(HT_Error)	HT_SetSelectionRange (HT_Resource node1, HT_Resource node2);

PR_PUBLIC_API(HT_Resource)	HT_GetNextSelection(HT_View view, HT_Resource startingNode);
PR_PUBLIC_API(void)	HT_ToggleSelection(HT_Resource node);

PR_PUBLIC_API(PRBool)	HT_Launch(HT_Resource node, MWContext *context);

/*
 * HT_NewCursor, HT_GetNextItem, HT_DeleteCursor
 * Used to iterate over a container's children. Until the container has been
 * opened at least once, you won't see any of the children.
 */

PR_PUBLIC_API(HT_Cursor) HT_NewCursor (HT_Resource node) ;
PR_PUBLIC_API(HT_Error) HT_DeleteCursor (HT_Cursor cursor) ;
PR_PUBLIC_API(HT_Resource) HT_GetNextItem (HT_Cursor cursor) ;

/*
 * HT_IsContainerOpen / HT_GetOpenState / HT_SetOpenState
 * manage open state of a node;  get/set operations will generate
 * a HT_EVENT_NODE_OPENCLOSE_CHANGED notification unless masked out
 */
PR_PUBLIC_API(PRBool)   HT_IsContainerOpen (HT_Resource node);
PR_PUBLIC_API(HT_Error) HT_GetOpenState (HT_Resource containerNode, PRBool *openState);
PR_PUBLIC_API(HT_Error) HT_SetOpenState (HT_Resource containerNode, PRBool isOpen);

/*
 * HT_ItemHasForwardSibling / HT_ItemHasBackwardSibling
 * determine if a given node has a following/previous sibling node
 */
PR_PUBLIC_API(PRBool)	HT_ItemHasForwardSibling(HT_Resource r);
PR_PUBLIC_API(PRBool)	HT_ItemHasBackwardSibling(HT_Resource r);

PR_PUBLIC_API(void)	HT_NewWorkspace(HT_Pane pane, char *id, char *optionalTitle);
PR_PUBLIC_API(void)	HT_SetWorkspaceOrder(HT_View src, HT_View dest, PRBool afterDestFlag);

/*-----------------------------------------------------------------------*/
/*                    Creating new containers                            */
/*-----------------------------------------------------------------------*/

PR_PUBLIC_API(HT_Resource) HT_MakeNewContainer(HT_Resource parent, char* name);   

/*-----------------------------------------------------------------------*/
/*                    Drag and Drop */
/*              drop actions should be made an enum                         */
/*-----------------------------------------------------------------------*/

typedef uint8 HT_DropAction;


#define DROP_NOT_ALLOWED 0
#define COPY_MOVE_CONTENT 1
#define UPLOAD_RDF       2
#define COPY_MOVE_LINK   3
#define UPLOAD_LFS       4

PR_PUBLIC_API(HT_DropAction)   HT_CanDropHTROn(HT_Resource dropTarget, HT_Resource obj); 
PR_PUBLIC_API(HT_DropAction)   HT_CanDropURLOn(HT_Resource dropTarget, char* url); 
PR_PUBLIC_API(HT_DropAction)   HT_DropHTROn(HT_Resource dropTarget, HT_Resource obj); 
PR_PUBLIC_API(HT_DropAction)   HT_DropURLOn(HT_Resource dropTarget, char* url); 
PR_PUBLIC_API(HT_DropAction)   HT_DropURLAndTitleOn(HT_Resource dropTarget,
							char* url, char *title);

PR_PUBLIC_API(HT_DropAction)   HT_CanDropHTRAtPos(HT_Resource dropTarget, HT_Resource obj, 
						  PRBool before); 
PR_PUBLIC_API(HT_DropAction)   HT_CanDropURLAtPos(HT_Resource dropTarget, char* url, 
						  PRBool before); 
PR_PUBLIC_API(HT_DropAction)   HT_DropHTRAtPos(HT_Resource dropTarget, HT_Resource obj, 
					       PRBool before); 
PR_PUBLIC_API(HT_DropAction)   HT_DropURLAtPos(HT_Resource dropTarget, char* url, 
					       PRBool before); 
PR_PUBLIC_API(HT_DropAction)   HT_DropURLAndTitleAtPos(HT_Resource dropTarget,
							char* url, char *title, PRBool before);


/*-----------------------------------------------------------------------*/
/*                    Editing                                            */
/*-----------------------------------------------------------------------*/

PR_PUBLIC_API(PRBool) HT_RemoveChild  (HT_Resource parent, HT_Resource child);


/*-----------------------------------------------------------------------*/
/*                    Other                                            */
/*-----------------------------------------------------------------------*/


PR_PUBLIC_API(RDF) RDF_GetNavCenterDB();
PR_PUBLIC_API(void) HT_InformRDFOfNewDocument(char* address);

PR_PUBLIC_API(PRBool) HT_HasHTMLPane(HT_View htView);
PR_PUBLIC_API(char *) HT_HTMLPaneHeight(HT_View htView);

PR_PUBLIC_API(void) HT_AddSitemapFor(HT_Pane htPane, char *pUrl, char *pSitemapUrl, char* name);
PR_PUBLIC_API(void) HT_AddRelatedLinksFor(HT_Pane htPane, char *pUrl);
PR_PUBLIC_API(void) HT_ExitPage(HT_Pane htPane, char *pUrl);

NSPR_END_EXTERN_C

#endif /* htrdf_h___ */
