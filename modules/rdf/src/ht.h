/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef	_RDF_HT_H_
#define	_RDF_HT_H_


#include "xpassert.h"
#include "xp_qsort.h"
#include "xp_time.h"
#include "client.h"
#include "net.h"
#include "xpgetstr.h"
#include "xp_str.h"
#include "htmldlgs.h"
#include "xp_ncent.h"
#include "xpassert.h"
#include "nspr.h"
#include "prefapi.h"
#include "fe_proto.h"
#include "intl_csi.h"
#include "prinit.h"
#include "prthread.h"
#include "libevent.h"

#ifdef	XP_MAC
#include "stdlib.h"
#include <Aliases.h>
#endif

#include "rdf.h"
#include "rdf-int.h"
#include "math.h"
#include "htrdf.h"

#ifdef	SMART_MAIL
#include "pm2rdf.h"
#endif

/* HT data structures and defines */

#define ITEM_LIST_SIZE			500		/* XXX ITEM_LIST_SIZE should be dynamic */
#define ITEM_LIST_ELEMENT_SIZE		20

#define NUM_MENU_CMDS           	40

#define RDF_SITEMAP			1
#define RDF_RELATED_LINKS		2
#define FROM_PAGE			1
#define GUESS_FROM_PREVIOUS_PAGE	2

#define HTDEL				remoteStoreRemove

	/* external string references in allxpstr */
extern	int	RDF_HTML_STR, RDF_HTML_STR_1, RDF_HTML_STR_2, RDF_HTML_STR_3;
extern	int	RDF_HTML_STR_4, RDF_HTML_STR_5, RDF_HTML_STR_NUMBER;
extern	int	RDF_HTML_WINDATE, RDF_HTML_MACDATE, RDF_CMD_0, RDF_DATA_1, RDF_DATA_2;
extern	int	RDF_DELETEFILE, RDF_UNABLETODELETEFILE, RDF_DELETEFOLDER;
extern	int	RDF_UNABLETODELETEFOLDER, RDF_SITEMAPNAME;
extern	int	RDF_RELATEDLINKSNAME, RDF_DEFAULTCOLUMNNAME;
extern	int	RDF_NEWWORKSPACEPROMPT, RDF_DELETEWORKSPACE;
extern	int	RDF_ADDITIONS_ALLOWED, RDF_DELETION_ALLOWED;
extern	int	RDF_ICON_URL_LOCKED, RDF_NAME_LOCKED, RDF_COPY_ALLOWED;
extern	int	RDF_MOVE_ALLOWED, RDF_WORKSPACE_POS_LOCKED;
extern	int	RDF_MAIN_TITLE, RDF_COLOR_TITLE, RDF_HTML_INFOHEADER_STR;
extern	int	RDF_MISSION_CONTROL_TITLE, RDF_TREE_COLORS_TITLE, RDF_SELECTION_COLORS_TITLE;
extern	int	RDF_COLUMN_COLORS_TITLE, RDF_TITLEBAR_COLORS_TITLE, RDF_HTML_MAININFOHEADER_STR;
extern	int	RDF_HTML_EMPTYHEADER_STR, RDF_HTML_COLOR_STR, RDF_SETCOLOR_JS, RDF_DEFAULTCOLOR_JS;
extern	int	RDF_COLOR_LAYER, RDF_HTMLCOLOR_STR;
extern	int	RDF_SELECT_START, RDF_SELECT_END, RDF_SELECT_OPTION;
extern	int	RDF_FIND_STR1, RDF_FIND_STR2, RDF_FIND_INPUT_STR;
extern	int	RDF_LOCAL_LOCATION_STR, RDF_REMOTE_LOCATION_STR, RDF_ALL_LOCATION_STR;
extern	int	RDF_CONTAINS_STR, RDF_IS_STR, RDF_IS_NOT_STR, RDF_STARTS_WITH_STR, RDF_ENDS_WITH_STR;
extern	int	RDF_FIND_TITLE, RDF_FIND_FULLNAME_STR, RDF_SHORTCUT_CONFLICT_STR, RDF_FTP_NAME_STR;

#ifdef	HT_PASSWORD_RTNS
extern	int	RDF_NEWPASSWORD, RDF_CONFIRMPASSWORD;
extern	int	RDF_MISMATCHPASSWORD, RDF_ENTERPASSWORD;
#endif


#define	MISSION_CONTROL_RDF_PREF	"browser.navcenter.admin"
#define	NETSCAPE_RDF_FILENAME		"netscape.rdf"

typedef struct _SBProviderStruct {
	struct _SBProviderStruct	*next;
	char				*url;
	char				*name;
	PRBool				containerp;
	PRBool				openp;
} SBProviderStruct;
typedef SBProviderStruct* SBProvider;

typedef struct _HT_PaneStruct {
	struct _HT_PaneStruct		*next;
	void				*pdata;
	HT_Notification			ns;
	PLHashTable			*hash;
	HT_NotificationMask		mask;
	RDF				db;
	RDF_Notification		rns;
	struct _HT_ViewStruct		*viewList;
	struct _HT_ViewStruct		*selectedView;
	struct _HT_URLSiteMapAssoc	*smp;
	struct _HT_URLSiteMapAssoc	*sbp;
	uint32				viewListCount;
	uint32				loadingCount;
	PRBool				autoFlushFlag;
	SBProvider			smartBrowsingProviders;
	PRBool				dirty;
	PRBool				personaltoolbar;
	PRBool				toolbar;
	PRBool				bookmarkmenu;
	PRBool				special;
	int					windowType;
	char				*windowURL;
	char				*templateType;
	struct _HT_PaneStruct		*templatePane;
	char				*htdburl;
	RDFT				htdb;
} HT_PaneStruct;

typedef	struct HT_ColumnStruct {
	struct HT_ColumnStruct		*next;
	char				*name;
	uint32				width;
	uint32				tokenType;
	void				*token;
	void				*feData;
	PRBool				isHiddenFlag;
} HT_ColumnStruct, *HT_Column;

typedef struct _HT_ViewStruct {
	struct _HT_ViewStruct		*next;
	HT_Pane				pane;
	HT_Resource			top;
	void				*pdata;
	HT_Column			columns;
	uint32				workspacePos;
	struct _HT_ResourceStruct	***itemList;
	uint32				itemListSize;
	uint32				itemListCount;
	uint32				topNodeIndex;
	uint32				selectedNodeHint;
	uint32				sortTokenType;
	void				*sortToken;
	PRBool				descendingFlag;
	PRBool				refreshingItemListp;
	PRBool				inited;
	PRBool				collapsedFlag;
	PRBool				hiddenFlag;
	RDF_Resource			treeRel;
} HT_ViewStruct;

typedef	struct _HT_ValueStruct {
	struct _HT_ValueStruct		*next;
	uint32				tokenType;
	RDF_Resource			token;
	void				*data;
} HT_ValueStruct, *HT_Value;

#define	HT_CONTAINER_FLAG		0x0001
#define	HT_OPEN_FLAG			0x0002
#define	HT_AUTOFLUSH_OPEN_FLAG		0x0004
#define	HT_HIDDEN_FLAG			0x0008
#define	HT_SELECTED_FLAG		0x0010
#define	HT_VOLATILE_URL_FLAG		0x0020
#define	HT_CONTENTS_LOADING_FLAG	0x0040
#define	HT_PASSWORDOK_FLAG		0x0080
#define	HT_INITED_FLAG			0x0100
#define	HT_DIRTY_FLAG			0x0200
#define	HT_ENABLED_FLAG			0x0400

typedef struct _HT_ResourceStruct {
	struct _HT_ResourceStruct	*nextItem; 
	HT_View				view;
	HT_Resource			parent;
	RDF_Resource			node;
	void				*feData;
	char				*dataSource;
	char				*url[2];
	HT_Value			values;
	HT_Resource			child;
	HT_Resource			*children;		/* used by sorting */
	uint32				unsortedIndex;		/* used by sorting */
	uint32				itemListIndex;
	uint32				numChildren, numChildrenTotal;
	uint16				flags;
	uint16				depth;
	HT_Resource			next;
	/* a pane or view might have multiple occurances of a RDF_Resource.
	The hash table just points to the first of them. This allows us to
	make a linked list of it */
} HT_ResourceStruct;

typedef	struct	_HT_MenuCommandStruct	{
	struct	_HT_MenuCommandStruct	*next;
	HT_MenuCmd			menuCmd;
	char				*name;
	RDF_Resource			graphCommand;
} _HT_MenuCommandStruct, *HT_MenuCommand;

typedef struct _HT_CursorStruct {
	HT_Resource			container;
	HT_Resource			node;
	RDF_Cursor			cursor;
	uint32				numElements;
	HT_Column			columns;
	uint16				contextMenuIndex;
	PRBool				foundValidMenuItem;
	PRBool				isWorkspaceFlag;
	PRBool				isBackgroundFlag;
	PRBool				commandExtensions;
	PRBool				commandListBuild;
	HT_MenuCmd			menuCmd;
	HT_MenuCommand			menuCommandList;
} HT_CursorStruct;

typedef	struct _HT_Icon {
	struct _HT_Icon			*next;
	char				*name;
} _HT_Icon, *HT_Icon;

typedef struct	_htmlElement	{
	struct	_htmlElement		*next;
	HT_Resource			node;
	RDF_Resource			token;
	uint32				tokenType;
} _htmlElement, *_htmlElementPtr;

typedef struct _HT_URLSiteMapAssoc {
	uint8				siteToolType;
    uint8               origin;
    uint8               onDisplayp;
	char				*url;
	RDF_Resource		sitemap;
    char*               name;
    char*               sitemapUrl;
    RDFT                db;
	struct _HT_URLSiteMapAssoc	*next;
} HT_URLSiteMapAssoc;



/* HT function prototypes */

XP_BEGIN_PROTOS

void				HT_Startup();
void				HT_Shutdown();
PRBool				ht_UpdateURLstate(char *url, PRBool inProgressFlag, int status);
void				htLoadBegins(URL_Struct *urls, char *url);
void				htLoadComplete(MWContext *cx, URL_Struct *urls, char *url, int status);
void				htTimerRoutine(void *timerID);
PRBool				possiblyUpdateView(HT_View view);
void				updateViewItem(HT_Resource node);
HT_Resource			newHTEntry (HT_View view, RDF_Resource node);
void				addWorkspace(HT_Pane pane, RDF_Resource r, void *feData);
void				deleteWorkspace(HT_Pane pane, RDF_Resource r);
void				htrdfNotifFunc (RDF_Event ns, void* pdata);
void				bmkNotifFunc (RDF_Event ns, void* pdata);
void				refreshItemListInt (HT_View view, HT_Resource node);
PRBool				relatedLinksContainerp (HT_Resource node);
int				nodeCompareRtn(HT_Resource *node1, HT_Resource *node2);
void				sortNodes(HT_View view, HT_Resource parent, HT_Resource *children, uint32 numChildren);
uint32				refreshItemList1(HT_View view, HT_Resource node);
void				refreshItemList (HT_Resource node, HT_Event whatHappened);
void				refreshPanes();
PRBool				initToolbars (HT_Pane pane);
HT_Pane				paneFromResource(RDF db, RDF_Resource resource, HT_Notification notify, PRBool autoFlushFlag, PRBool autoOpenFlag, PRBool useColumns);
void				htMetaTagURLExitFunc (URL_Struct *urls, int status, MWContext *cx);
void				htLookInCacheForMetaTags(char *url);
void				htSetBookmarkAddDateToNow(RDF_Resource r);
RDF				newHTPaneDB();
RDF				HTRDF_GetDB();
PRBool				initViews (HT_Pane pane);
void				htNewWorkspace(HT_Pane pane, char *id, char *optionalTitle, uint32 workspacePos);
HT_PaneStruct *			HT_GetHTPaneList ();
HT_PaneStruct *			HT_GetNextHTPane (HT_PaneStruct* pane);
void				htSetWorkspaceOrder(RDF_Resource src, RDF_Resource dest, PRBool afterDestFlag);
HT_View				HT_NewView (RDF_Resource topNode, HT_Pane pane, PRBool useColumns, void *feData, PRBool autoOpen);
void				sendNotification (HT_Resource node, HT_Event whatHappened, RDF_Resource s, HT_ColumnType type);
void				deleteHTNode(HT_Resource node);
void				destroyViewInt (HT_Resource r, PRBool saveOpenState);
void				htDeletePane(HT_Pane pane, PRBool saveWorkspaceOrder);
void				saveWorkspaceOrder(HT_Pane pane);
void				resynchItem (HT_Resource node, void *token, void *data, PRBool assertAction);
void				resynchContainer (HT_Resource container);
HT_Resource			addContainerItem (HT_Resource container, RDF_Resource item);
void				refreshContainerIndexes(HT_Resource container);
void				removeHTFromHash (HT_Pane pane, HT_Resource item);
void				deleteHTSubtree (HT_Resource subtree);
void				deleteContainerItem (HT_Resource container, RDF_Resource item);
uint32				fillContainer (HT_Resource node);
void				sendColumnNotification (HT_View view, void *token, uint32 tokenType, HT_Event whatHappened);
PRBool				htIsMenuCmdEnabled(HT_Pane pane, HT_MenuCmd menuCmd, PRBool isWorkspaceFlag, PRBool isBackgroundFlag);
void				freeMenuCommandList();
void				exportCallbackWrite(PRFileDesc *fp, char *str);
void				exportCallback(MWContext *context, char *filename, RDF_Resource node);
void				htEmptyClipboard(RDF_Resource parent);
void				htCopyReference(RDF_Resource original, RDF_Resource newParent, PRBool empty);
PRBool				htVerifyUniqueToken(HT_Resource node, void *token, uint32 tokenType, char *data);
PRBool				ht_isURLReal(HT_Resource node);
char *				buildInternalIconURL(HT_Resource node, PRBool *volatileURLFlag,	PRBool largeIconFlag, PRBool workspaceFlag);
char *				getIconURL( HT_Resource node, PRBool toolbarFlag, HT_IconType state);
PRBool				htIsPropertyInMoreOptions(RDF_Resource r);
void				addHtmlElement(HT_Resource node, RDF_Resource token, uint32 tokenType);
void				freeHtmlElementList();
_htmlElementPtr			findHtmlElement(void *token);
void				freeHtmlElement(void *token);
char *				constructHTMLTagData(char *dynStr, int strID, char *data);
char *				constructHTML(char *dynStr, HT_Resource node, void *token, uint32 tokenType);
char *				constructHTMLPermission(char *dynStr, HT_Resource node, RDF_Resource token, char *permText);
PRBool				htIsOpLocked(HT_Resource node, RDF_Resource token);
PRBool				rdfFindDialogHandler(XPDialogState *dlgstate, char **argv, int argc, unsigned int button);
char *				constructBasicHTML(char *dynStr, int strID, char *data1, char *data2);
void				setHiddenState (HT_Resource node);
void				htSetFindResourceName(RDF db, RDF_Resource r);
void				htOpenTo(HT_View view, RDF_Resource u, PRBool selectView);
PRBool				mutableContainerp (RDF_Resource node);
char *				possiblyCleanUpTitle (char* title);
PRBool				htRemoveChild(HT_Resource parent, HT_Resource child, PRBool moveToTrash);
void				ht_SetPassword(HT_Resource node, char *password);
PRBool				ht_hasPassword(HT_Resource node);
PRBool				ht_checkPassword(HT_Resource node, PRBool alwaysCheck);
HT_DropAction			htLaunchSmartNode(HT_Resource dropTarget, char *fullURL);
HT_DropAction			dropOnSmartNode(HT_Resource dropTarget, HT_Resource dropObject, PRBool justAction);
HT_DropAction			dropOnSmartURL(HT_Resource dropTarget, char *objTitle, PRBool justAction);
HT_DropAction			dropOn (HT_Resource dropTarget, HT_Resource dropObject, PRBool justAction);
void				Win32FileCopyMove(HT_Resource dropTarget, HT_Resource dropObject);
HT_DropAction			copyMoveRDFLink (HT_Resource dropTarget, HT_Resource dropObject);
HT_DropAction			copyMoveRDFLinkAtPos (HT_Resource dropx, HT_Resource dropObject, PRBool before);
HT_DropAction			uploadLFS (HT_Resource dropTarget, HT_Resource dropObject);
HT_DropAction			uploadRDFFile (HT_Resource dropTarget, HT_Resource dropObject);
HT_DropAction			esfsCopyMoveContent (HT_Resource dropTarget, HT_Resource dropObject);
RDF_BT				urlResourceType (char* url);
HT_DropAction			dropURLOn (HT_Resource dropTarget, char* objURL, char *objTitle, PRBool justAction);
void				replacePipeWithColon(char* url);
HT_DropAction			copyRDFLinkURL (HT_Resource dropTarget, char* objURL, char *objTitle);
HT_DropAction			copyRDFLinkURLAt (HT_Resource dropx, char* objURL, char *objTitle, PRBool before);
HT_DropAction			uploadLFSURL (HT_Resource dropTarget, char* objURL);
HT_DropAction			uploadRDFFileURL (HT_Resource dropTarget, char* objURL);
HT_DropAction			esfsCopyMoveContentURL (HT_Resource dropTarget, char* objURL);
RDFT				HTADD(HT_Pane pane, RDF_Resource u, RDF_Resource s, void* v);
HT_URLSiteMapAssoc *		makeNewSMP (HT_Pane htPane, char* pUrl, char* sitemapurl);
void				RetainOldSitemaps (HT_Pane htPane, char *pUrl);
void				populateSBProviders (HT_Pane htPane);
SBProvider			SBProviderOfNode (HT_Resource node);
PRBool				implicitDomainURL (char* url);
PRBool				domainMatches (char *dom, char *url);
void				nextDomain (char* dom, size_t *n);
PRBool				relatedLinksEnabledURL (char* url);
void				cleanupInt (HT_Pane htPane, HT_URLSiteMapAssoc *nsmp, RDF_Resource parent);
HT_Pane				newTemplatePane(char* templateName);
void				PaneDeleteSBPCleanup (HT_Pane htPane);

XP_END_PROTOS

#endif
