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
 * When we're exporting data, we end up having to pass all the parameters
 * via $_GET.  Since we're doing this a few times per page (each graph, and the
 * export hrefs) this helper should make that easier.
 */
class ExportHelper
{
    /**
     * Method to collect the current $_GET parameters and build another string from
     * them.
     *
     * @param string cake URL that you want prepended to the result.  eg: reports/graph/
     * @param array the array of GET variables to add.  These need to be
     *              pre-sanitized to print in html!  This is designed to 
     *              be used from the calling controller like:
     *                   $params = $this->sanitize->html($url_parameters);
     * @param string string to put between url and arguments (probably either '?' or
     *              '&')
     * @param array array of strings which will be ignored
     * @return string string with the url variables appeneded to it
     */
    function buildUrlString($url, $params, $seperator='?', $ignore=array('url'))
    {
        $arguments = '';

        foreach ($params as $var => $val) {
            if (!in_array($var, $ignore)) {
                $arguments .= empty($arguments) ? "{$var}={$val}" : "&amp;{$var}={$val}";
            }
        }

        return "{$this->webroot}{$url}{$seperator}{$arguments}";
    }

    function buildCsvExportString($params)
    {
        if (array_key_exists('product', $params)) {
            $filename = $params['product'];
        } else {
            $filename = DEFAULT_APP_NAME.' '.DEFAULT_APP_VERSION;
        }

        // Our filenames have underscores
        $filename = str_replace(' ','_',$filename);

        $filename = "export-{$filename}.csv";

        if (is_readable(ROOT.DS.APP_DIR.DS.WEBROOT_DIR.DS.'export'.DS.$filename)) {
            return "{$this->webroot}export/{$filename}";
        } else {
            return '';
        }
    }
}
?>
