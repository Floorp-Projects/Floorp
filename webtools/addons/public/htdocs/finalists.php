<?php
// ***** BEGIN LICENSE BLOCK *****
// Version: MPL 1.1/GPL 2.0/LGPL 2.1
//
// The contents of this file are subject to the Mozilla Public License Version
// 1.1 (the "License"); you may not use this file except in compliance with
// the License. You may obtain a copy of the License at
// http://www.mozilla.org/MPL/
//
// Software distributed under the License is distributed on an "AS IS" basis,
// WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
// for the specific language governing rights and limitations under the
// License.
//
// The Original Code is Mozilla Update.
//
// The Initial Developer of the Original Code is
// Chris "Wolf" Crews.
// Portions created by the Initial Developer are Copyright (C) 2004
// the Initial Developer. All Rights Reserved.
//
// Contributor(s):
//   Chris "Wolf" Crews <psychoticwolf@carolina.rr.com>
//
// Alternatively, the contents of this file may be used under the terms of
// either the GNU General Public License Version 2 or later (the "GPL"), or
// the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
// in which case the provisions of the GPL or the LGPL are applicable instead
// of those above. If you wish to allow use of your version of this file only
// under the terms of either the GPL or the LGPL, and not to allow others to
// use your version of this file under the terms of the MPL, indicate your
// decision by deleting the provisions above and replace them with the notice
// and other provisions required by the GPL or the LGPL. If you do not delete
// the provisions above, a recipient may use your version of this file under
// the terms of any one of the MPL, the GPL or the LGPL.
//
// ***** END LICENSE BLOCK *****
	
startProcessing('finalists.tpl',null,$compileId,'nonav');
require_once('includes.php');

/**
 * Setting up variables.
 */
$guids = array(
'{34274bf4-1d97-a289-e984-17e546307e4f}',
'{097d3191-e6fa-4728-9826-b533d755359d}',
'{B9DAB69C-460E-4085-AE6C-F95B0D858581}',
'{DDC359D1-844A-42a7-9AA1-88A850A938A8}',
'{89506680-e3f4-484c-a2c0-ed711d481eda}',
'{3CE993BF-A3D9-4fd2-B3B6-768CBBC337F8}',
'{268ad77e-cff8-42d7-b479-da60a7b93305}',
'{77b819fa-95ad-4f2c-ac7c-486b356188a9}',
'{bbc21d30-1cff-11da-8cd6-0800200c9a66}',
'{37E4D8EA-8BDA-4831-8EA1-89053939A250}',
'{a089fffd-e0cb-431b-8d3a-ebb8afb26dcf}',
'Reveal@sourmilk.net',
'{a6ca9b3b-5e52-4f47-85d8-cca35bb57596}',
'{53A03D43-5363-4669-8190-99061B2DEBA5}',
'separe@m4ng0.lilik.it',
'xpose@viamatic.com',
'{c45c406e-ab73-11d8-be73-000a95be3b12}',
'{D5EDC062-A372-4936-B782-BD611DD18D86}'
);

$guids_tmp = array();
foreach ($guids as $guid) {
    $guids_tmp[] = "'".$guid."'";
}
$guids_imploded = implode(',',$guids_tmp);

$descriptions = array(
'{34274bf4-1d97-a289-e984-17e546307e4f}'=>"Block ads including Flash ads from their source.  Right click on an ad and select Adblock to block ads.  Hit the status-element and see what has or hasn't been blocked.",
'{097d3191-e6fa-4728-9826-b533d755359d}'=>"Manage Extensions, Themes, Downloads, and more including Web content via Firefox’s sidebar.",
'{B9DAB69C-460E-4085-AE6C-F95B0D858581}'=>"Blog directly within Firefox to LiveJournal, WordPress or Blogger. Select Deepest Sender from the ‘Tools’ menu.",
'{DDC359D1-844A-42a7-9AA1-88A850A938A8}'=>"DownThemAll lets you filter and download all the links contained in any web-page, and lets you pause and resume downloads from previous Firefox sessions.",
'{89506680-e3f4-484c-a2c0-ed711d481eda}'=>"View open Tabs and Windows with Showcase.  You can use it in two ways: global mode (F12) or local mode (Shift + F12). In global mode, a new window will be opened with thumbnails of the pages you've opened in all windows. In local mode, only content in tabs of your current window will be shown.

You can also right click in those thumbnails to perform the most usual operations on them. Mouse middle button can be used to zoom a thumbnail, although other actions can be assigned to it.",
'{3CE993BF-A3D9-4fd2-B3B6-768CBBC337F8}'=>"Get international weather forecasts and display it in any toolbar or status bar.",
'{268ad77e-cff8-42d7-b479-da60a7b93305}'=>"Select from several of your favorite toolbars including including Google, Yahoo, Ask Jeeves, Teoma, Amazon, Download.com and others with one toolbar. The entire toolbar reconfigures when you select a different engine and it includes many advanced features found in each engine.
You can also easily repeat your search on all engines included in toolbar.",
'{77b819fa-95ad-4f2c-ac7c-486b356188a9}'=>"View pages with in Internet Explorer with IE Tab.  Select the Firefox icon on the bottom right of the browser to switch to using the Internet Explorer engine or Firefox to switch to IE.",
'{bbc21d30-1cff-11da-8cd6-0800200c9a66}'=>"Allows sticky notes to be added to any web page, and viewed upon visiting the Web page again.  You can also share sticky notes.  Requires account.",
'{37E4D8EA-8BDA-4831-8EA1-89053939A250}'=>"PDF Download Extension allows you to choose if you want to view a PDF file inside the browser (as PDF or HTML), if you want to view it outside Firefox with your default or custom PDF reader, or if you want to download it.",
'{a089fffd-e0cb-431b-8d3a-ebb8afb26dcf}'=>"Platypus is a Firefox extension which lets you modify a Web page from your browser -- \"What You See Is What You Get\" -- and then save those changes as a GreaseMonkey script so that they'll be repeated the next time you visit the page.",
'Reveal@sourmilk.net'=>"Reveal allows you to see thumbnails of pages in your history by mousing over the back and forward buttons.  With many tabs open, quickly find the page you want, by pressing F2. Reveal also has a rectangular magnifying glass you can use to zoom in on areas of any web page. Comes  with a quick tour of all the features. ",
'{a6ca9b3b-5e52-4f47-85d8-cca35bb57596}'=>"A lightweight RSS and Atom feed aggregator.  Alt+S to open Sage in the Sidebar to start reading feed content.",
'{53A03D43-5363-4669-8190-99061B2DEBA5}'=>"Highlight text, create sticky notes, and more to Web pages and Web sites that are saved to your desktop.  Scrapbook Includes full text search and quick filtering of saved pages.",
'separe@m4ng0.lilik.it'=>"Manage tabs by creating a tab separator.  Right click on a Tab to add a new Tab separator.  Click on the Tab separator to view thumbnail images of web sites that are to the left and right of the Tab separator.",
'xpose@viamatic.com'=>"Click on the icon in the status bar to view all Web pages in Tabbed windows as thumbnail images.  Press F8 to activate foXpose.",
'{c45c406e-ab73-11d8-be73-000a95be3b12}'=>"Web developer toolbar includes various development tools such as window resizing, form and image debugging, links to page validation and optimization tools and much more.",
'{D5EDC062-A372-4936-B782-BD611DD18D86}'=>"RSS news reader with integrated with services such as Feedster and weather information.  Includes online help documentation."
);

$screenshots = array(
'{34274bf4-1d97-a289-e984-17e546307e4f}'=>'adblock-mini.png',
'{097d3191-e6fa-4728-9826-b533d755359d}'=>'all-in-one-mini.png',
'{B9DAB69C-460E-4085-AE6C-F95B0D858581}'=>'deepest-sender-mini.png',
'{DDC359D1-844A-42a7-9AA1-88A850A938A8}'=>'downthemall-small.png',
'{89506680-e3f4-484c-a2c0-ed711d481eda}'=>'firefox-showcase.png',
'{3CE993BF-A3D9-4fd2-B3B6-768CBBC337F8}'=>'forecastfoxenhanced-small.png',
'{268ad77e-cff8-42d7-b479-da60a7b93305}'=>'groowe-small.png',
'{77b819fa-95ad-4f2c-ac7c-486b356188a9}'=>'IE-Tab.png',
'{bbc21d30-1cff-11da-8cd6-0800200c9a66}'=>'stickies-small.png',
'{37E4D8EA-8BDA-4831-8EA1-89053939A250}'=>'pdf-download.png',
'{a089fffd-e0cb-431b-8d3a-ebb8afb26dcf}'=>'platypus.png',
'Reveal@sourmilk.net'=>'reveal.png',
'{a6ca9b3b-5e52-4f47-85d8-cca35bb57596}'=>'sage.png',
'{53A03D43-5363-4669-8190-99061B2DEBA5}'=>'scrapbook-final.png',
'separe@m4ng0.lilik.it'=>'separe.png',
'xpose@viamatic.com'=>'xpose-small.png',
'{c45c406e-ab73-11d8-be73-000a95be3b12}'=>'web-developer-toolbar-small.png',
'{D5EDC062-A372-4936-B782-BD611DD18D86}'=>'wizz-small.png'
);

$authors = array(
'{34274bf4-1d97-a289-e984-17e546307e4f}'=>'Ben Karel (and the Adblock Crew)',
'{DDC359D1-844A-42a7-9AA1-88A850A938A8}'=>'Federico Parodi',
'{a6ca9b3b-5e52-4f47-85d8-cca35bb57596}'=>'Peter Andrews (and the Sage Team)',
'{53A03D43-5363-4669-8190-99061B2DEBA5}'=>'Taiga Gomibuchi',
'separe@m4ng0.lilik.it'=>'Massimo Mangoni',
'{77b819fa-95ad-4f2c-ac7c-486b356188a9}'=>'yuoo2k and Hong Jen Yee (PCMan)'
);

$finalists = array();

// Get data for GUIDs.
$finalists_sql = "
            SELECT
                m.guid,
                m.id, 
                m.name, 
                m.downloadcount,
                m.homepage,
                v.dateupdated,
                v.uri,
                v.size,
                v.version,
                (   
                    SELECT u.username          
                    FROM userprofiles u
                    JOIN authorxref a ON u.userid = a.userid
                    WHERE a.id = m.id
                    ORDER BY u.userid DESC
                    LIMIT 1
                ) as username
            FROM
                main m
            JOIN version v ON m.id = v.id
            WHERE
                v.vid = (SELECT max(vid) FROM version WHERE id=m.id AND approved='YES') AND
                type = 'E' AND
                m.guid IN({$guids_imploded})
            ORDER BY
                LTRIM(m.name)
";

$db->query($finalists_sql, SQL_ALL, SQL_ASSOC);

foreach ($db->record as $var => $val) {
    $val['author'] = !empty($authors[$val['guid']]) ?  $authors[$val['guid']] : $val['username'];
    $finalists[] = $val;
}

// Assign template variables.
$tpl->assign(
    array( 
            'title'        => 'Extend Firefox Contest Finalists',
            'screenshots'  => $screenshots,
            'descriptions' => $descriptions,
            'finalists'    => $finalists,
            'content'      => 'finalists.tpl')
);
?>
