/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#define VIEWER_DEMO16           40027
#define VIEWER_DEMO17           40028

#define VIEWER_PRINT            40030
#define VIEWER_PRINT_SETUP      40031
#define VIEWER_ONE_COLUMN       40035
#define VIEWER_TWO_COLUMN       40036
#define VIEWER_THREE_COLUMN     40037


#define VIEWER_VISUAL_DEBUGGING     40050
#define VIEWER_VISUAL_EVENT_DEBUGGING 40051
#define VIEWER_REFLOW_TEST          40052
#define VIEWER_DUMP_CONTENT         40053
#define VIEWER_DUMP_FRAMES          40054
#define VIEWER_DUMP_VIEWS           40055
#define VIEWER_DUMP_STYLE_SHEETS    40056
#define VIEWER_DUMP_STYLE_CONTEXTS  40057
#define VIEWER_DEBUGROBOT           40058
#define VIEWER_SHOW_CONTENT_SIZE    40059
#define VIEWER_SHOW_FRAME_SIZE      40060
#define VIEWER_SHOW_STYLE_SIZE      40061
#define VIEWER_DEBUGSAVE            40062
#define VIEWER_SHOW_CONTENT_QUALITY 40063
#define VIEWER_TOGGLE_SELECTION     40064

#define VIEWER_TABLE_INSPECTOR      40067
#define VIEWER_IMAGE_INSPECTOR      40068
#define VIEWER_NATIVE_WIDGET_MODE   40069
#define VIEWER_GFX_WIDGET_MODE      40070
#define VIEWER_DISPLAYTEXT          40071
#define VIEWER_DISPLAYHTML          40072
#define VIEWER_GFX_SCROLLBARS_ON    40073
#define VIEWER_GFX_SCROLLBARS_OFF   40074
#define VIEWER_GOTO_TEST_URL1       40075
#define VIEWER_GOTO_TEST_URL2       40076
#define VIEWER_SAVE_TEST_URL1       40077
#define VIEWER_SAVE_TEST_URL2       40078
#define VIEWER_USE_DTD_MODE         40079
#define VIEWER_STANDARD_MODE        40080
#define VIEWER_NAV_QUIRKS_MODE      40081
#define VIEWER_DSP_REFLOW_CNTS_ON     40084
#define VIEWER_DSP_REFLOW_CNTS_OFF    40085
#define VIEWER_DEBUG_DUMP_REFLOW_TOTS 40086

#define VIEWER_TOGGLE_PAINT_FLASHING         40200
#define VIEWER_TOGGLE_PAINT_DUMPING          40210
#define VIEWER_TOGGLE_INVALIDATE_DUMPING     40220
#define VIEWER_TOGGLE_EVENT_DUMPING          40230
#define VIEWER_TOGGLE_MOTION_EVENT_DUMPING   40240
#define VIEWER_TOGGLE_CROSSING_EVENT_DUMPING 40250

#define VIEWER_SELECT_STYLE_LIST    40500
#define VIEWER_SELECT_STYLE_DEFAULT 40501
#define VIEWER_SELECT_STYLE_ONE     40502
#define VIEWER_SELECT_STYLE_TWO     40503
#define VIEWER_SELECT_STYLE_THREE   40504
#define VIEWER_SELECT_STYLE_FOUR    40505

#define VIEWER_EDIT_SET_BGCOLOR_RED 40548
#define VIEWER_EDIT_SET_BGCOLOR_YELLOW 40549
#define VIEWER_EDIT_INSERT_TABLE    40550
#define VIEWER_EDIT_INSERT_CELL     40551
#define VIEWER_EDIT_INSERT_COLUMN   40552
#define VIEWER_EDIT_INSERT_ROW      40553
#define VIEWER_EDIT_DELETE_TABLE    40554
#define VIEWER_EDIT_DELETE_CELL     40555
#define VIEWER_EDIT_DELETE_COLUMN   40556
#define VIEWER_EDIT_DELETE_ROW      40557
#define VIEWER_EDIT_JOIN_CELL_RIGHT 40558

#define VIEWER_ZOOM_BASE            40600
#define VIEWER_ZOOM_500             40650
#define VIEWER_ZOOM_300             40630
#define VIEWER_ZOOM_200             40620
#define VIEWER_ZOOM_100             40610
#define VIEWER_ZOOM_070             40607
#define VIEWER_ZOOM_050             40605
#define VIEWER_ZOOM_030             40603
#define VIEWER_ZOOM_020             40602



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

#define VIEWER_PURIFY_SHOW_NEW_LEAKS            40910
#define VIEWER_PURIFY_SHOW_ALL_LEAKS            40911
#define VIEWER_PURIFY_CLEAR_ALL_LEAKS           40912
#define VIEWER_PURIFY_SHOW_ALL_HANDLES_IN_USE   40913
#define VIEWER_PURIFY_SHOW_NEW_IN_USE           40914
#define VIEWER_PURIFY_SHOW_ALL_IN_USE           40915
#define VIEWER_PURIFY_CLEAR_ALL_IN_USE          40916
#define VIEWER_PURIFY_HEAP_VALIDATE             40917

#endif /* resources_h___ */


