<?php
// ***** BEGIN LICENSE BLOCK *****
//
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
// The Original Code is AUS.
//
// The Initial Developer of the Original Code is Mike Morgan.
// 
// Portions created by the Initial Developer are Copyright (C) 2006 
// the Initial Developer. All Rights Reserved.
//
// Contributor(s):
//   Mike Morgan <morgamic@mozilla.com>
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

/**
 * Common functions
 *
 * @package aus
 * @subpackage sanity
 */

/**
 * Read a directory and return a sorted array of its contents.
 * @string $dir path to directory
 * @string $pattern pattern matching valid filenames
 * @return array
 */
function ls($dir,$pattern, $sort='desc') {
    $files = array();
    $fp = opendir($dir);
    while (false !== ($filename = readdir($fp))) {
        if (preg_match($pattern,$filename)) {
            $files[] = $filename;
        }
    }
    closedir($fp);

    if ($sort=='asc') {
        rsort($files);
    } else {
        sort($files); 
    }

    return $files;
}

/**
 * Write a string to a file.
 * @string $file file
 * @string $string string
 */
function write($file,$string) {
    if ($fp = fopen($file,'w')) {
        fwrite($fp,$string);
        fclose($fp);
    }
}

/**
 * Get date.
 * @param string $datestring
 * @param boolean $fuzzy
 * @return string
 */
function timify($datestring,$fuzzy=true) {
    $year = substr($datestring,0,4);
    $month = substr($datestring,4,2);
    $day = substr($datestring,6,2);

    if (!$fuzzy) {
        $hour = substr($datestring,8,2);
        $minute = substr($datestring,10,2);
        $second = substr($datestring,12,2);

        return date( 'D F j, Y, g:i a', mktime($hour, $minute, $second, $month, $day, $year));
    } else {

        return date( 'D F j, Y', mktime(0, 0, 0, $month, $day, $year));
    }
}
?>
