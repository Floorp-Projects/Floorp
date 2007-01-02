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
 * The Original Code is survey.mozilla.com site.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Wil Clouser <clouserw@mozilla.com> (Original Author)
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


/**
 * A simple class to print a csv from a 2 dimensional array.
 *
 */
class csv
{
    var $filename = '';

    var $data = array();

    /**
     * @param string name of the file to send
     */
    function csv($name=null) 
    {
        $this->filename = $name;

        if (empty($this->filename)) {
            $this->filename = 'export-'.date('Y-m-d').'.csv';
        }
    }


    /**
     * Function to load data from an array into the object
     * @param array a 2 dimensional array
     */
    function loadDataFromArray(&$array)
    {
        $this->data = $array;
    }

    /**
     * Will send the CSV to the browser (including headers)
     */
    function sendCSV()
    {
        $this->_sendHeaders();

        $this->_cleanData();

        foreach ($this->data as $var => $val) {
            // We put quotes around each value here
            $line = implode('","',$val);
            echo "\"{$line}\"\n";
        }
    }

    /**
     * Cleans data for export into a csv (quotes and newlines)
     * @access private
     */
    function _cleanData()
    {
        foreach ($this->data as $var => $val) {
            // escape the quotes by doubling them
            $val = str_replace('"','""',$val);

            // fix newlines
            $val = str_replace("\n\r", "\n", str_replace("\r", "\n", $val));
        }
    }

    /**
     * Sends headers for a .csv
     * @access private
     */
    function _sendHeaders()
    {
        header("Content-type: application/x-csv");
        header('Content-disposition: inline; filename="'.$this->filename.'"');
        header('Cache-Control: private');
        header('Pragma: public'); 	
    }

}
