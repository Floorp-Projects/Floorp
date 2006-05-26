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
 * @package aus
 * @subpackage inc
 * @author Mike Morgan
 */
class Xml extends AUS_Object {
    var $xmlOutput;
    var $xmlHeader;
    var $xmlFooter;
    var $xmlPatchLines;

    /**
     * Constructor, sets overall header and footer.
     */
    function Xml() {
        $this->xmlHeader = '<?xml version="1.0"?>'."\n".'<updates>';
        $this->xmlFooter = "\n".'</updates>';
        $this->xmlOutput = $this->xmlHeader;
        $this->xmlPatchLines = '';
    }

    /**
     * Start an update block.
     * @param object $update
     */
    function startUpdate($update) {
        $type = htmlentities($update->type);
        $version = htmlentities($update->version);
        $extensionVersion = htmlentities($update->extensionVersion);
        $build = htmlentities($update->build);
        $details = htmlentities($update->details);

        $details_xml = "";
        if (strlen($details) > 0) {
            $details_xml = " detailsURL=\"{$details}\"";
        }

        $this->xmlOutput .= <<<startUpdate

    <update type="{$type}" version="{$version}" extensionVersion="{$extensionVersion}" buildID="{$build}" {$details_xml}>
startUpdate;

        /**
         * @TODO Add buildID attribute to <update> element.
         *
         * Right now it is pending QA on the client side, so we will leave it
         * out for now.
         *
         * buildID="{$build}"
         */
    }

    /**
     * Set a patch line.  This pulls info from a patch object.
     * @param object $patch
     */
    function setPatchLine($patch) {
        $type = htmlentities($patch->type);
        $url = htmlentities($patch->url);
        $hashFunction = htmlentities($patch->hashFunction);
        $hashValue = htmlentities($patch->hashValue);
        $size = htmlentities($patch->size);

        $this->xmlPatchLines .= <<<patchLine

        <patch type="{$type}" URL="{$url}" hashFunction="{$hashFunction}" hashValue="{$hashValue}" size="{$size}"/>
patchLine;
    }

    /**
     * Determines whether or not patchLines have been set.
     * @return bool
     */
    function hasPatchLine() {
        return (empty($this->xmlPatchLines)) ? false : true;
    }

    /**
     * End an update block.
     */
    function endUpdate() {
        $this->xmlOutput .= <<<endUpdate

    </update>
endUpdate;
    }

    /**
     * Add patchLines to output.
     */
    function drawPatchLines() {
        $this->xmlOutput .= $this->xmlPatchLines;
    }

    /**
     * Get XML output.
     * @return $string $this->xmlOutput
     */
    function getOutput() {
        $this->xmlOutput .= $this->xmlFooter;
        return $this->xmlOutput; 
    }
}
?>
