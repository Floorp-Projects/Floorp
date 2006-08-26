<?php

/**
 * This will setup cake to handle language/locales passed in via $_GET['lang'].
 * Keeping with the cake style, this is a class.
 */

class LANGUAGE_CONFIG
{

    var $_default_language = 'en_US';

    var $_default_domain   = 'messages';

    var $_text_domain      = null;

    var $current_language  = null;

    /**
     * If you add a language it needs to be in this array
     */
    var $_valid_languages  = array(
                                'en_US',
                                'de_DE'
                                );


    /**
     * This fills in the text_domain (telling where the .mo files are), as well as
     * s/-/_/g in $_GET['lang'].  That is for a few reasons, including consistency
     * from here on out and PEAR bug #8546 (http://pear.php.net/bugs/bug.php?id=8546)
     * 
     * @param boolean set_language If true, we'll detect and set the current language
     */
    function LANGUAGE_CONFIG($set_language=false)
    {
        // This is where our .mo files are.
        $this->_text_domain = ROOT.'/'.APP_DIR.'/locale';

        // Rewrite the $_GET['lang'] variable so dashes are underscores
        if (array_key_exists('lang', $_GET)) {
            $_GET['lang'] = str_replace('-','_',$_GET['lang']);
        }

        if ($set_language == true) {
            $this->setCurrentLanguage($this->detectCurrentLanguage());
        }

    }

    /**
     * Will look at the current $_GET string for a $lang parameter.  If it exists and
     * is valid, that is our language.  If anything fails, it falls back to
     * $this->_default_language.
     *
     * @return string language (eg. 'en-US' or 'ru')
     */
    function detectCurrentLanguage()
    {
        if (array_key_exists('lang', $_GET)) {
            if (in_array($_GET['lang'], $this->_valid_languages)) {
                $this->current_language = $_GET['lang'];
            }
        }

        // If all else fails, fallback
        if (empty($this->current_language)) {
            $this->current_language = $this->_default_language;
        }

        return $this->current_language;
    }

    /**
     * Simply returns the current language, or the default if one isn't set.
     *
     * @return string current language
     */
    function getCurrentLanguage() 
    {
        if (empty($this->current_language)) {
            return $this->_default_language;
        }

        return $this->current_language;
    }


    /**
     * Runs all the appropriate gettext functions for setting the current language to
     * whatever is passed in (or the default).
     *
     * @param string language (eg. 'en-US' or 'ru')
     * @param string domain basically, what is the name of your .mo file? (before the .)
     * @return boolean true on success, false on failure
     */
    function setCurrentLanguage($lang=null, $domain=null)
    {
        if (empty($lang)) {
            $lang = $this->_default_language;
        }

        if (empty($domain)) {
            $domain = $this->_default_domain;
        }

        // Double check they know what they are talking about.  
        if (!in_array($lang, $this->_valid_languages)) {
            return false;
        }

        $this->current_language = $lang;

        putenv("LANG={$lang}");

        // LC_ALL is going to change dates, currency, and language - perhaps this
        // should be configurable?
        setlocale(LC_ALL, $lang);

        bindtextdomain($domain, $this->_text_domain);

        textdomain($domain);

        return true;
    }




}
?>
