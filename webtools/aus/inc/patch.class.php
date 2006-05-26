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
 * AUS Patch class.
 * @package aus
 * @subpackage inc
 * @author Mike Morgan
 *
 * This class is for handling patch objects.
 * These carry relevant information about partial or complete patches.
 */
class Patch extends AUS_Object {

    // Patch metadata.
    var $type;
    var $url;
    var $hashFunction;
    var $hashValue;
    var $size;
    var $build;

    // Array that maps versions onto their respective branches.
    var $branchVersions;

    // Array the defines which channels are flagged as 'nightly' channels.
    var $nightlyChannels;

    // Valid patch flag.
    var $isPatch;

    // Is this patch a complete or partial patch?
    var $patchType;

    // Update metadata, read from patch file.
    var $updateType;
    var $updateVersion;
    var $updateExtensionVersion;
    
    // Do we have Update metadata information?
    var $hasUpdateInfo;
    var $hasDetailsUrl;

    /**
     * Constructor.
     */
    function Patch($branchVersions=array(),$nightlyChannels,$type='complete') {
        $this->setBranchVersions($branchVersions);
        $this->setNightlyChannels($nightlyChannels);
        $this->setVar('isPatch',false);
        $this->setVar('patchType',$type); 
        $this->setVar('hasUpdateInfo',false);
        $this->setVar('hasDetailsUrl',false);
    }

    /**
     * Set the filepath for the snippet based on product/platform/locale and
     * SOURCE_DIR, which is set in config.
     *
     * @param string $product
     * @param string $platform
     * @param string $locale
     * @param string $version
     * @param string $build
     * @param string $buildSource
     * @param string $channel
     *
     * @return boolean
     */
    function setPath ($product,$platform,$locale,$version=null,$build,$buildSource,$channel) {
        switch($buildSource) {
            case 3:
                return $this->setVar('path',OVERRIDE_DIR.'/'.$product.'/'.$version.'/'.$platform.'/'.$build.'/'.$locale.'/'.$channel.'/'.$this->patchType.'.txt',true);
                break;
            case 2:
                return $this->setVar('path',SOURCE_DIR.'/'.$buildSource.'/'.$product.'/'.$version.'/'.$platform.'/'.$build.'/'.$locale.'/'.$this->patchType.'.txt',true);
                break;
        }
        return false;
    }

    /**
     * Read the given file and store its contents in our Patch object.
     *
     * @param string $path
     *
     * @return boolean
     */
    function setSnippet ($path) {
        if ($file = explode("\n",file_get_contents($path,true))) {
            $this->setVar('type',$file[0]);
            $this->setVar('url',$file[1]);
            $this->setVar('hashFunction',$file[2]);
            $this->setVar('hashValue',$file[3]);
            $this->setVar('size',$file[4]);
            $this->setVar('build',$file[5]);

            // Attempt to read update information.
            // @TODO Add ability to set updateType, once it exists in the build snippet.
            if ($this->isComplete() && isset($file[6]) && isset($file[7])) {
                $this->setVar('updateVersion',$file[6],true);
                $this->setVar('updateExtensionVersion',$file[7],true);
                $this->setVar('hasUpdateInfo',true,true);
            }
            
            if ($this->isComplete() && isset($file[8])) {
                $this->setVar('detailsUrl',$file[8],true);
                $this->setVar('hasDetailsUrl',true,true);
            }
            
            return true;
        }

        return false;
    }

    /**
     * Attempt to read and parse the designated source file.
     * How and where the file is read depends on the client version.
     *
     * For more information on why this is a little complicated, see:
     * https://intranet.mozilla.org/AUS:Version2:Roadmap:Multibranch
     *
     * @param string $product
     * @param string $platform
     * @param string $locale
     * @param string $version
     * @param string $build
     * @param string $channel
     *
     * @return boolean
     */
    function findPatch($product,$platform,$locale,$version,$build,$channel=null) {

        // Determine the branch of the client's version.
        $branchVersion = $this->getBranch($version);

        // If a specific update exists for the specified channel, it takes priority over the branch update.
        if (!empty($channel) && $this->setPath($product,$platform,$locale,$branchVersion,$build,3,$channel) && file_exists($this->path) && filesize($this->path) > 0) {
                $this->setSnippet($this->path); 
                $this->setVar('isPatch',true,true);
                return true;
        }

        // Otherwise, if it is a complete patch and a nightly channel, force the complete update to take the user to the latest build.
        elseif ($this->isComplete() && $this->isNightlyChannel($channel)) {

            // Get the latest build for this branch.
            $latestbuild = $this->getLatestBuild($product,$branchVersion,$platform);

            if ($this->setPath($product,$platform,$locale,$branchVersion,$latestbuild,2,$channel) && file_exists($this->path) && filesize($this->path) > 0) {
                $this->setSnippet($this->path); 
                $this->setVar('isPatch',true,true);
                return true;
            }
        } 


        // Otherwise, check for the partial snippet info.  If an update exists, pass it along.
        elseif ($this->isNightlyChannel($channel) && $this->setPath($product,$platform,$locale,$branchVersion,$build,2,$channel) && file_exists($this->path) && filesize($this->path) > 0) {
                $this->setSnippet($this->path); 
                $this->setVar('isPatch',true,true);
                return true;
        } 

        // Note: Other data sets were made obsolete in 0.6.  May incoming/0,1 rest in peace.

        // If we get here, we know for sure that no updates exist for the current request..
        // Return false by default, which prompts the "no updates" XML output.
        return false;
    }

    /**
     * Compare passed build to build in snippet.
     * Returns true if the snippet build is newer than the client build.
     *
     * @param string $build
     * @return boolean
     */
    function isNewBuild($build) {
        return ($this->build>$build) ? true : false;
    }

    /**
     * Set the branch versions array.
     *
     * @param array $branchVersions
     * @return boolean
     */
    function setBranchVersions($branchVersions) {
        return $this->setVar('branchVersions',$branchVersions);
    }

    /**
     * Set the nightly channels array.
     *
     * @param array $branchVersions
     * @return boolean
     */
    function setNightlyChannels($nightlyChannels) {
       return $this->setVar('nightlyChannels',$nightlyChannels);
    }

    /**
     * Determine whether or not the given channel is flagged as nightly.
     * 
     * @param string $channel
     *
     * @return bool
     */
    function isNightlyChannel($channel) {
        return in_array($channel,$this->nightlyChannels);
    }


    /**
     * Determine whether or not the incoming version is a product BRANCH.
     *
     * @param string $version
     * @return string|false
     */
    function getBranch($version) {
       return (isset($this->branchVersions[$version])) ? $this->branchVersions[$version] : false;
    }

    /**
     * Determine whether or not something is Trunk.
     *
     * @param string $version
     * @return boolean
     */
    function isTrunk($version) {
       return ($version == 'trunk') ? true : false;
    }

    /**
     * Does this object contain a valid patch file?
     */
    function isPatch() {
       return $this->isPatch;
    }

    /**
     * Determine whether or not this patch is complete.
     */
    function isComplete() {
       return ($this->patchType === 'complete') ? true : false;
    }

    /**
     * Determine whether or not this patch has a details URL.
     */
    function hasDetailsUrl() {
       return $this->hasDetailsUrl;
    }

    /**
     * Determine whether or not this patch has update information.
     */
    function hasUpdateInfo() {
       return $this->hasUpdateInfo;
    }

    /**
     * Determine whether or not the to_build matches the latest build for a partial patch.
     * @param string $build
     *
     * @return bool
     */
    function isOneStepFromLatest($build) {
        return ($this->build == $build) ? true : false;
    }

    /**
     * Get the latest build for this branch.
     * @param string $product
     * @param string $branchVersion
     * @param string $platform
     */
    function getLatestBuild($product,$branchVersion,$platform) {
        $files = array();
        $fp = opendir(SOURCE_DIR.'/2/'.$product.'/'.$branchVersion.'/'.$platform);
        while (false !== ($filename = readdir($fp))) {
            if ($filename!='.' && $filename!='..') {
                $files[] = $filename;
            }       
        }
        closedir($fp);
        
        rsort($files,SORT_NUMERIC); 

        return $files[1];
    }
}
?>
