/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef resources_h___
#define resources_h___

#define VIEWER_OPEN             40000
#define VIEWER_EXIT             40002
#define PREVIEW_CLOSE           40003
#define VIEW_SOURCE             40004

#define VIEWER_WINDOW_OPEN      40009
#define VIEWER_FILE_OPEN        40010

// Note: must be in ascending sequential order
#define VIEWER_DEMO0            40011
#define VIEWER_DEMO1            40012
#define VIEWER_DEMO2            40013
#define VIEWER_DEMO3            40014
#define VIEWER_DEMO4            40015
#define VIEWER_DEMO5            40016
#define VIEWER_DEMO6            40017
#define VIEWER_DEMO7            40018
#define VIEWER_DEMO8            40019
#define VIEWER_DEMO9            40020
#define VIEWER_DEMO10           40021
#define VIEWER_DEMO11           40022
#define VIEWER_DEMO12           40023
#define VIEWER_DEMO13           40024
#define VIEWER_DEMO14           40025
#define VIEWER_DEMO15           40026

#define VIEWER_VISUAL_DEBUGGING     40028
#define VIEWER_REFLOW_TEST          40029
#define VIEWER_DUMP_CONTENT         40030
#define VIEWER_DUMP_FRAMES          40031
#define VIEWER_DUMP_VIEWS           40032
#define VIEWER_DUMP_STYLE_SHEETS    40033
#define VIEWER_DUMP_STYLE_CONTEXTS  40034
#define VIEWER_DEBUGROBOT           40035
#define VIEWER_SHOW_CONTENT_SIZE    40036
#define VIEWER_SHOW_FRAME_SIZE      40037
#define VIEWER_SHOW_STYLE_SIZE      40038
#define VIEWER_DEBUGSAVE            40039
#define VIEWER_SHOW_CONTENT_QUALITY 40040
#define VIEWER_TOGGLE_SELECTION     40041
#define VIEWER_NAV_QUIRKS_MODE      40042
#define VIEWER_STANDARD_MODE        40043
#define VIEWER_TABLE_INSPECTOR      40044
#define VIEWER_IMAGE_INSPECTOR      40045
#define VIEWER_NATIVE_WIDGET_MODE   40046
#define VIEWER_GFX_WIDGET_MODE      40047
#define VIEWER_DISPLAYTEXT          40048
#define VIEWER_DISPLAYHTML          40049

#define VIEWER_SELECT_STYLE_LIST    40500
#define VIEWER_SELECT_STYLE_DEFAULT 40501
#define VIEWER_SELECT_STYLE_ONE     40502
#define VIEWER_SELECT_STYLE_TWO     40503
#define VIEWER_SELECT_STYLE_THREE   40504
#define VIEWER_SELECT_STYLE_FOUR    40505

#define VIEWER_EDIT_INSERT_TABLE    40550
#define VIEWER_EDIT_INSERT_CELL     40551
#define VIEWER_EDIT_INSERT_COLUMN   40552
#define VIEWER_EDIT_INSERT_ROW      40553
#define VIEWER_EDIT_DELETE_TABLE    40554
#define VIEWER_EDIT_DELETE_CELL     40555
#define VIEWER_EDIT_DELETE_COLUMN   40556
#define VIEWER_EDIT_DELETE_ROW      40557
#define VIEWER_EDIT_JOIN_CELL_RIGHT 40558
#define VIEWER_EDIT_JOIN_CELL_BELOW 40559

#define VIEWER_ZOOM_BASE            40600
#define VIEWER_ZOOM_500             40650
#define VIEWER_ZOOM_300             40630
#define VIEWER_ZOOM_200             40620
#define VIEWER_ZOOM_100             40610
#define VIEWER_ZOOM_070             40607
#define VIEWER_ZOOM_050             40605
#define VIEWER_ZOOM_030             40603
#define VIEWER_ZOOM_020             40602

// Note: must be in ascending sequential order
#define VIEWER_ONE_COLUMN       40050
#define VIEWER_TWO_COLUMN       40051
#define VIEWER_THREE_COLUMN     40052

#define VIEWER_PRINT            40060
#define VIEWER_PRINT_SETUP      40061

#define JS_CONSOLE              40100
#define EDITOR_MODE             40120
#define VIEWER_PREFS            40130

#define VIEWER_EDIT_CUT         40201
#define VIEWER_EDIT_COPY        40202
#define VIEWER_EDIT_PASTE       40203
#define VIEWER_EDIT_SELECTALL   40204
#define VIEWER_EDIT_FINDINPAGE  40205

#define VIEWER_RL_BASE          41000

#define PRVCY_PREFILL           40290
#define PRVCY_QPREFILL          40291
#define PRVCY_DISPLAY_WALLET    40292
#define PRVCY_DISPLAY_COOKIES   40293
#define PRVCY_DISPLAY_SIGNONS   40294

#define VIEWER_TOP100           40300

#define VIEWER_XPTOOLKITDEMOBASE	40900
#define VIEWER_XPTOOLKITTOOLBAR1	VIEWER_XPTOOLKITDEMOBASE
#define VIEWER_XPTOOLKITTREE1	    VIEWER_XPTOOLKITDEMOBASE+1

/* Debug Robot dialog setup */

#define IDD_DEBUGROBOT          101
#define IDC_UPDATE_DISPLAY      40301
#define IDC_VERIFICATION_DIRECTORY 40302
#define IDC_PAGE_LOADS          40303
#define IDC_STATIC              -1

#define IDD_SITEWALKER          200
#define ID_SITE_PREVIOUS        40400
#define ID_SITE_NEXT            40401
#define IDC_SITE_NAME           40402
#define ID_EXIT                 40404

#endif /* resources_h___ */
