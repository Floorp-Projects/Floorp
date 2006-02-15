<?php
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Reporter (r.m.o).
 *
 * The Initial Developer of the Original Code is
 *      Robert Accettura <robert@accettura.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2006
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

require_once('../config.inc.php');
require_once($config['base_path'].'/includes/iolib.inc.php');
require_once($config['base_path'].'/includes/db.inc.php');
require_once($config['base_path'].'/includes/security.inc.php');


// Turn off Error Reporting
error_reporting(0);

// Headers
printheaders();

if($securitylib->isLoggedIn() === true){
    $db = NewDBConnection($config['db_dsn']);
    $db->SetFetchMode(ADODB_FETCH_ASSOC);

    $query = $db->Execute("SELECT screenshot_data, screenshot.screenshot_format
                           FROM screenshot
                           WHERE screenshot_report_id = ".$db->quote($_GET['report_id']));
    if(!$query){
        exit;
    }

    // Output the MIME header
    $imageExtension = array_search($query->fields['screenshot_format'], $config['screenshot_imageTypes']);

    // This should never happen, but we test for it regardless
    if($imageExtension === false){
        // XXX -> we should redirect to an error image or someting to that effect as
        //        in most cases, nobody would even see this error.
        print "Invalid Image";
    }

    // Headers
    header("Content-Type: ".$query->fields['screenshot_format']);
    header("Content-disposition: inline; filename=".$_GET['report_id'].".".$imageExtension);

    // Output the image
    echo $query->fields['screenshot_data'];
} else {
    // XXX -> we should redirect to an error image or someting to that effect as
    //        in most cases, nobody would even see this error.
    print "You are not authorized to view this";
}
?>