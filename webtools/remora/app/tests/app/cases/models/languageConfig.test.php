<?php
class LanguageConfigTest extends UnitTestCase {

    var $language_config;

	function setUp() 
    {
        $this->language_config = new LANGUAGE_CONFIG(false);
	}

	//Make sure there are no PHP errors
	function testNoErrors() 
    {
		$this->assertNoErrors();
	}

    /**
     * Walk through our languages array and make sure all the language files exist.
     */
    function testLangFilesExist() 
    {
        // yeah, I steal private vars here. php4++

        $text_domain = $this->language_config->_text_domain;

        foreach ($this->language_config->_valid_languages as $lang) {
            // So, gettext() has some logic built into it.  If the current lang is
            // set to, say, 'en-US' but there is no 'en-US' directory, it will fall
            // back to 'en' (if it exists).  We'll emulate this behavior here.

            // First file we'll look for
            $lang_file = "{$text_domain}/{$lang}/LC_MESSAGES/messages.mo";

            if (file_exists($lang_file)) {
                //it's there, we're good to go.
                continue;
            }

            // Is a default lang always 2 characters, or should we look for a dash?
            $lang2 = substr($lang,0,2);

            if (file_exists("{$text_domain}/{$lang2}/LC_MESSAGES/messages.mo")) {
                // Our fallback works, yay.
                continue;
            }

            // Bad things
            $this->fail("Couldn't find language file for ( {$lang} ) or ( {$lang2} ).");
        }

    }

    /**
     *
     */
    function testDetectCurrentLanguage() 
    {

        $_GET['lang'] = null;

        // First part of the test is to try it with nothing - we should get back the default language.
        $this->assertEqual($this->language_config->detectCurrentLanguage(), $this->language_config->getDefaultLanguage());

        // Next we'll try it with a language that doesn't exist:
        $_GET['lang'] = 'xx-YY';
        $this->assertEqual($this->language_config->detectCurrentLanguage(), $this->language_config->getDefaultLanguage());

        // And finally, with one that does exist:
        $_GET['lang'] = 'en_US';
        $this->assertEqual($this->language_config->detectCurrentLanguage(), 'en_US');

        $_GET['lang'] = null;
    }

    function testGetCurrentLanguage() 
    {
        // This isn't much of a test, but really, there isn't much to test...
        $this->assertNotNull($this->language_config->getCurrentLanguage());
    }

    function testSetCurrentLanguage()
    {
        // Try it with a bad language - should return false
        $this->assertFalse($this->language_config->setCurrentLanguage('xx_YY'));


        // Try it with a good language - should return true
        $this->assertTrue($this->language_config->setCurrentLanguage('en_US'));
    }

}
?>  
