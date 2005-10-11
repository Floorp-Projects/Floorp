<?php
/**
 * Version class. Represents version of app/addon in FVF
 *  "major[.minor[.release[.build+]]]"
 * @package amo
 * @subpackage lib
 * @link http://www.mozilla.org/projects/firefox/extensions/update.html
 */
class Version 
{
    var $major;
    var $minor;
    var $release;
    var $build;
    var $plus;

    /**
     * Class constructor.
     * 
     * @param string $aVersionString FVF version conformant string
     */
    function Version($aVersionString=NULL) {
        if (isset($aVersionString)) {
            $this->parseString($aVersionString);
        }
    }
    
    /**
     * Parse a version string into useful chunks.
     * 
     * @param string $aVersionString FVF conformant version string
     */
    function parseString($aVersionString) {
        assert(strlen($aVersionString));
        
        // in case we're being re-used
        $this->major = NULL;
        $this->minor = NULL;
        $this->release = NULL;
        $this->build = NULL;
        $this->plus = NULL;
    
        // holder for the chunks, to be filled by preg_match
        $matches = array();

        if (preg_match('/^(\d+)(?:\.(\d+))?(?:\.(\d+))?(?:\.(\d+)([a-zA-Z0-9]+)?)?$/', $aVersionString, $matches)) {

            if (isset($matches[1])) {
                $this->major = intval($matches[1]);

                if (isset($matches[2])) {
                    $this->minor = intval($matches[2]);

                    if (isset($matches[3])) {
                        $this->release = intval($matches[3]);

                        if (isset($matches[4])) {
                            $this->build = intval($matches[4]);

                            if (isset($matches[5])) {
                                $this->plus = $matches[5];
                            }
                        }
                    }
                }
            }
        }

        assert($aVersionString == $this->toString());
    }

    /**
     * Is this a valid version
     *
     * @return boolean TRUE if the instance represents a valid version 
     *                 FALSE otherwise
     *
     * @todo this may or may not be required. it simply relies on the
     *       fact that major is not optional. any valid string would
     *       have been parsed, and major set
     */
     function isValid() {
         return isset($this->major);
     }

    /**
     * Static method to compare one Version to another Version
     * 
     * @param a a Version
     * @param a a Version
     * @returns < 0 if a < b
     *          = 0 if a == b
     *          > 0 if a > b
     */
    function compare($a, $b) {
        assert(is_a($a, 'Version'));
        assert(is_a($b, 'Version'));
        assert($a->isValid());
        assert($b->isValid());
    
        if ($a->major != $b->major) {
            return $a->major - $b->major;
        }
        
        if ($a->minor != $b->minor) {
            return $a->minor - $b->minor;
        }
        
        if ($a->release != $b->release) {
            return $a->release - $b->release;
        }
        
        if ($a->build != $b->build) {
            return $a->build - $b->build;
        }
            
        return 0;
    }

    /**
     * Produce a string version 
     * 
     * @returns string the version encoded in FVF conformant string
     */
    function toString() {
        $chunks = array($this->major);

        if (isset($this->minor)) {
            $chunks[] = $this->minor;

            if (isset($this->release)) {
                $chunks[] = $this->release;

                if (isset($this->build)) {

                    if (isset($this->plus)) {
                        $chunks[] = $this->build . $this->plus;
                    } else {
                        $chunks[] = $this->build;
                    }
                }
            }
        }

        return implode('.', $chunks);
    }
}
?>
