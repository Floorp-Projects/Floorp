/*
*****************************************************************************************
*                                                                                       *
* COPYRIGHT:                                                                            *
*   (C) Copyright Taligent, Inc.,  1996                                                 *
*   (C) Copyright International Business Machines Corporation,  1996                    *
*   Licensed Material - Program-Property of IBM - All Rights Reserved.                  *
*   US Government Users Restricted Rights - Use, duplication, or disclosure             *
*   restricted by GSA ADP Schedule Contract with IBM Corp.                              *
*                                                                                       *
*****************************************************************************************
*
* File locid.h
*
* Created by: Helena Shih
*
* Modification History:
*
*	Date		Name		Description
*	02/11/97	aliu		Changed gLocPath to fgLocPath and added methods to
*							get and set it.
*	04/02/97	aliu		Made operator!= inline; fixed return value of getName().
*	04/15/97	aliu		Cleanup for AIX/Win32.
*	04/24/97	aliu		Numerous changes per code review.
*****************************************************************************************
*/

#ifndef _LOCID
#define _LOCID

#ifndef _UNISTRING
#include "unistring.h"
#endif

/**    
 *
 * A <code>Locale</code> object represents a specific geographical, political,
 * or cultural region. An operation that requires a <code>Locale</code> to perform
 * its task is called <em>locale-sensitive</em> and uses the <code>Locale</code>
 * to tailor information for the user. For example, displaying a number
 * is a locale-sensitive operation--the number should be formatted
 * according to the customs/conventions of the user's native country,
 * region, or culture.
 *
 * <P>
 * You create a <code>Locale</code> object using one of the two constructors in
 * this class:
 * <blockquote>
 * <pre>
 * .      Locale(String language, String country)
 * .      Locale(String language, String country, String variant)
 * </pre>
 * </blockquote>
 * The first argument to both constructors is a valid <STRONG>ISO
 * Language Code.</STRONG> These codes are the lower-case two-letter
 * codes as defined by ISO-639.
 * You can find a full list of these codes at a number of sites, such as:
 * <BR><a href ="http://www.ics.uci.edu/pub/ietf/http/related/iso639.txt">
 * <code>http://www.ics.uci.edu/pub/ietf/http/related/iso639.txt</code></a>
 *
 * <P>
 * The second argument to both constructors is a valid <STRONG>ISO Country
 * Code.</STRONG> These codes are the upper-case two-letter codes
 * as defined by ISO-3166.
 * You can find a full list of these codes at a number of sites, such as:
 * <BR><a href="http://www.chemie.fu-berlin.de/diverse/doc/ISO_3166.html">
 * <code>http://www.chemie.fu-berlin.de/diverse/doc/ISO_3166.html</code></a>
 *
 * <P>
 * The second constructor requires a third argument--the <STRONG>Variant.</STRONG>
 * The Variant codes are vendor and browser-specific.
 * For example, use WIN for Windows, MAC for Macintosh, and POSIX for POSIX.
 * Where there are two variants, separate them with an underscore, and
 * put the most important one first. For
 * example, a Traditional Spanish collation might be referenced, with
 * "ES", "ES", "Traditional_WIN".
 *
 * <P>
 * Because a <code>Locale</code> object is just an identifier for a region,
 * no validity check is performed when you construct a <code>Locale</code>.
 * If you want to see whether particular resources are available for the
 * <code>Locale</code> you construct, you must query those resources. For
 * example, ask the <code>NumberFormat</code> for the locales it supports
 * using its <code>getAvailableLocales</code> method.
 * <BR><STRONG>Note:</STRONG> When you ask for a resource for a particular
 * locale, you get back the best available match, not necessarily
 * precisely what you asked for. For more information, look at
 * <a href="java.util.ResourceBundle.html"><code>ResourceBundle</code></a>.
 *
 * <P>
 * The <code>Locale</code> class provides a number of convenient constants
 * that you can use to create <code>Locale</code> objects for commonly used
 * locales. For example, the following refers to a <code>Locale</code> object
 * for the United States:
 * <blockquote>
 * <pre>
 * .      Locale::US
 * </pre>
 * </blockquote>
 *
 * <P>
 * Once you've created a <code>Locale</code> you can query it for information about
 * itself. Use <code>getCountry</code> to get the ISO Country Code and
 * <code>getLanguage</code> to get the ISO Language Code. You can
 * use <code>getDisplayCountry</code> to get the
 * name of the country suitable for displaying to the user. Similarly,
 * you can use <code>getDisplayLanguage</code> to get the name of
 * the language suitable for displaying to the user. Interestingly,
 * the <code>getDisplayXXX</code> methods are themselves locale-sensitive
 * and have two versions: one that uses the default locale and one
 * that takes a locale as an argument and displays the name or country in
 * a language appropriate to that locale.
 *
 * <P>
 * The TIFC provides a number of classes that perform locale-sensitive
 * operations. For example, the <code>NumberFormat</code> class formats
 * numbers, currency, or percentages in a locale-sensitive manner. Classes
 * such as <code>NumberFormat</code> have a number of convenience methods
 * for creating a default object of that type. For example, the
 * <code>NumberFormat</code> class provides these three convenience methods
 * for creating a default <code>NumberFormat</code> object:
 * <blockquote>
 * <pre>
 * .      NumberFormat.getInstance()
 * .      NumberFormat.getCurrencyInstance()
 * .      NumberFormat.getPercentInstance()
 * </pre>
 * </blockquote>
 * Each of these methods has two variants; one with an explicit locale
 * and one without; the latter using the default locale.
 * <blockquote>
 * <pre>
 * .      NumberFormat.getInstance(myLocale)
 * .      NumberFormat.getCurrencyInstance(myLocale)
 * .      NumberFormat.getPercentInstance(myLocale)
 * </pre>
 * </blockquote>
 * A <code>Locale</code> is the mechanism for identifying the kind of object
 * (<code>NumberFormat</code>) that you would like to get. The locale is
 * <STRONG>just</STRONG> a mechanism for identifying objects,
 * <STRONG>not</STRONG> a container for the objects themselves.
 *
 * <P>
 * Each class that performs locale-sensitive operations allows you
 * to get all the available objects of that type. You can sift
 * through these objects by language, country, or variant,
 * and use the display names to present a menu to the user.
 * For example, you can create a menu of all the collation objects
 * suitable for a given language. Such classes implement these
 * three class methods:
 * <blockquote>
 * <pre>
 * .      static Locale* getAvailableLocales(t_int32& numLocales)
 * .      static UnicodeString& getDisplayName(const Locale&  objectLocale,
 * .                                           const Locale&  displayLocale,
 * .                                           UnicodeString& displayName)
 * .      static UnicodeString& getDisplayName(const Locale&  objectLocale,
 * .                                           UnicodeString& displayName)
 * </pre>
 * </blockquote>
 */
#ifdef NLS_MAC
#pragma export on
#endif

class T_UTILITY_API Locale 
{
public:

    /**
     * Useful constants for language.
     */
    static const Locale ENGLISH;
    static const Locale FRENCH;
    static const Locale GERMAN;
    static const Locale ITALIAN;
    static const Locale JAPANESE;
    static const Locale KOREAN;
    static const Locale CHINESE;
 	static const Locale SCHINESE;	// Simplified Chinese
    static const Locale TCHINESE;	// Traditional Chinese

    /**
     * Useful constants for country.
     */
    static const Locale FRANCE;
    static const Locale GERMANY;
    static const Locale ITALY;
    static const Locale JAPAN;
    static const Locale KOREA;
    static const Locale CHINA;		// Alias for PRC
    static const Locale PRC;		// People's Republic of China
    static const Locale TAIWAN;		// Republic of China
    static const Locale UK;
    static const Locale US;
    static const Locale CANADA;
    static const Locale CANADA_FRENCH;

   /**
    * Construct an empty locale. It's only used when a fill-in parameter is
    * needed.
    */
								Locale(); 

   /**
    * Construct an locale from language.
    *
    * @param language Lowercase two-letter ISO-639 code.
    */
								Locale(	const	UnicodeString&	newLanguage);

    /**
     * Construct a locale from language, country.
     *
     * @param language Uppercase two-letter ISO-639 code.
     * @param country  Uppercase two-letter ISO-3166 code.
     */
								Locale(	const	UnicodeString&	language, 
										const	UnicodeString&	country);

    /**
     * Construct a locale from language, country, variant.
     *
     * @param language Lowercase two-letter ISO-639 code.
     * @param country  Uppercase two-letter ISO-3166 code.
     * @param variant  Uppercase vendor and browser specific code. See class
     *                 description.
     */
								Locale(	const	UnicodeString&	language, 
										const	UnicodeString&	country, 
										const	UnicodeString&	variant);

    /**
     * Initializes an Locale object from another Locale object.
     *
     * @param other The Locale object being copied in.
     */
								Locale(const	Locale&	other);

   /**
     * Initializes an Locale object from char*
     *
     * lang can be any one of lang, lang_co, or lang_co_variant
     */
								Locale(const	char *loc_id);
								
	/**
     * Destructor
     */
								~Locale() {  }
    /**
     * Replaces the entire contents of *this with the specified value.
     *
     * @param other The Locale object being copied in.
     * @return      *this
     */
  	Locale&						operator=(const	Locale&	other);

    /**
     * Checks if two locale keys are the same.
     *
     * @param other The locale key object to be compared with this.
     * @return      True if the two locale keys are the same, false otherwise.
     */
				t_bool			operator==(const	Locale&		other) const;	

    /**
     * Checks if two locale keys are not the same.
     *
     * @param other The locale key object to be compared with this.
     * @return      True if the two locale keys are not the same, false
     *              otherwise.
     */
				t_bool			operator!=(const	Locale&		other) const;

    /**
     * Common methods of getting the current default Locale. Used for the
     * presentation: menus, dialogs, etc. Generally set once when your applet or
     * application is initialized, then never reset. (If you do reset the
     * default locale, you probably want to reload your GUI, so that the change
     * is reflected in your interface.)
     *
     * More advanced programs will allow users to use different locales for
     * different fields, e.g. in a spreadsheet.
     *
     * Note that the initial setting will match the host system.
     */
	static	const	Locale&		getDefault();

    /**
     * Sets the default. Normally set once at the beginning of applet or
     * application, then never reset. setDefault does NOT reset the host locale.
     *
     * @param newLocale Locale to set to.
     */
    static		void			setDefault(const	Locale&		newLocale,
													ErrorCode&	success);

    /**
	 * Fills in "lang" with the locale's two-letter ISO-639 language code.
	 * @param lang	Receives the language code.
	 * @return		A reference to "lang".
     */
				UnicodeString&	getLanguage(		UnicodeString&	lang) const;
    /**
	 * Fills in "cntry" with the locale's two-letter ISO-3166 country code.
	 * @param cntry	Receives the country code.
	 * @return		A reference to "cntry".
     */
				UnicodeString&	getCountry(			UnicodeString&	cntry) const;
    /**
	 * Fills in "var" with the locale's variant code.
	 * @param var	Receives the variant code.
	 * @return		A reference to "var".
     */
				UnicodeString&	getVariant(			UnicodeString&	var) const;

    /**
     * Fills in "name" the programmatic name of the entire locale, with the language,
     * country and variant separated by underbars. If a field is missing, at
     * most one underbar will occur. Example: "en", "de_DE", "en_US_WIN",
     * "de_POSIX", "fr_MAC"
	 * @param var	Receives the programmatic locale name.
	 * @return		A reference to "name".
     */
				UnicodeString&	getName(		UnicodeString&	name) const;

    /**
	 * Fills in "name" with the locale's three-letter language code, as specified
	 * in ISO draft standard ISO-639-2..
	 * @param name	Receives the three-letter language code.
	 * @return		A reference to "name".
     */
				UnicodeString&	getISO3Language(UnicodeString&	name) const;

    /**
	 * Fills in "name" with the locale's three-letter ISO-3166 country code.
	 * @param name	Receives the three-letter country code.
	 * @return		A reference to "name".
     */
				UnicodeString&	getISO3Country(	UnicodeString&	name) const;

    /**
     * Returns the Windows LCID value corresponding to this locale.
	 * This value is stored in the resource data for the locale as a one-to-four-digit
	 * hexadecimal number.  If the resource is missing, in the wrong format, or
	 * there is no Windows LCID value that corresponds to this locale, returns 0.
     */
				t_uint32		getLCID() const;

    /**
	 * Fills in "dispLang" with the name of this locale's language in a format suitable for
	 * user display in the default locale.  For example, if the locale's language code is
	 * "fr" and the default locale's language code is "en", this function would set
	 * dispLang to "French".
	 * @param dispLang	Receives the language's display name.
	 * @return			A reference to "dispLang".
     */
				UnicodeString&	getDisplayLanguage(UnicodeString&	dispLang) const;

    /**
	 * Fills in "dispLang" with the name of this locale's language in a format suitable for
	 * user display in the locale specified by "inLocale".  For example, if the locale's
	 * language code is "en" and inLocale's language code is "fr", this function would set
	 * dispLang to "Anglais".
	 * @param inLocale	Specifies the locale to be used to display the name.  In other words,
	 *					if the locale's language code is "en", passing Locale::FRENCH for
	 *					inLocale would result in "Anglais", while passing Locale::GERMAN
	 *					for inLocale would result in "Englisch".
	 * @param dispLang	Receives the language's display name.
	 * @return			A reference to "dispLang".
     */
				UnicodeString&	getDisplayLanguage(	const	Locale&			inLocale,
															UnicodeString&	dispLang) const;
    /**
	 * Fills in "dispCountry" with the name of this locale's country in a format suitable
	 * for user display in the default locale.  For example, if the locale's country code
	 * is "FR" and the default locale's language code is "en", this function would set
	 * dispCountry to "France".
	 * @param dispCountry	Receives the country's display name.
	 * @return				A reference to "dispCountry".
     */
				UnicodeString&	getDisplayCountry(			UnicodeString& dispCountry) const;
    /**
	 * Fills in "dispCountry" with the name of this locale's country in a format suitable
	 * for user display in the locale specified by "inLocale".  For example, if the locale's
	 * country code is "US" and inLocale's language code is "fr", this function would set
	 * dispCountry to "Etats-Unis".
	 * @param inLocale		Specifies the locale to be used to display the name.  In other
	 *                      words, if the locale's country code is "US", passing
	 *                      Locale::FRENCH for inLocale would result in "États-Unis", while
	 *                      passing Locale::GERMAN for inLocale would result in
	 *						"Vereinigte Staaten".
	 * @param dispCountry	Receives the country's display name.
	 * @return				A reference to "dispCountry".
     */
				UnicodeString&	getDisplayCountry(	const	Locale&			inLocale,
															UnicodeString&	dispCountry) const;

    /**
	 * Fills in "dispVar" with the name of this locale's variant code in a format suitable
	 * for user display in the default locale.
	 * @param dispVar	Receives the variant's name.
	 * @return			A reference to "dispVar".
     */
				UnicodeString&	getDisplayVariant(		UnicodeString& dispVar) const;
    /**
	 * Fills in "dispVar" with the name of this locale's variant code in a format
	 * suitable for user display in the locale specified by "inLocale".
	 * @param inLocale	Specifies the locale to be used to display the name.
	 * @param dispVar	Receives the variant's display name.
	 * @return			A reference to "dispVar".
     */
				UnicodeString&	getDisplayVariant(	const	Locale&			inLocale,
															UnicodeString&	dispVar) const;
    /**
	 * Fills in "name" with the name of this locale in a format suitable for user display 
	 * in the default locale.  This function uses getDisplayLanguage(), getDisplayCountry(),
	 * and getDisplayVariant() to do its work, and outputs the display name in the format
	 * "language (country[,variant])".  For example, if the default locale is en_US, then
	 * fr_FR's display name would be "French (France)", and es_MX_Traditional's display name
	 * would be "Spanish (Mexico,Traditional)".
	 * @param name	Receives the locale's display name.
	 * @return		A reference to "name".
     */
				UnicodeString&	getDisplayName(			UnicodeString&	name) const;
    /**
	 * Fills in "name" with the name of this locale in a format suitable for user display 
	 * in the locale specfied by "inLocale".  This function uses getDisplayLanguage(),
	 * getDisplayCountry(), and getDisplayVariant() to do its work, and outputs the display
	 * name in the format "language (country[,variant])".  For example, if inLocale is
	 * fr_FR, then en_US's display name would be "Anglais (États-Unis)", and no_NO_NY's
	 * display name would be "norvégien (Norvège,NY)".
	 * @param inLocale	Specifies the locale to be used to display the name.
	 * @param name		Receives the locale's display name.
	 * @return			A reference to "name".
     */
				UnicodeString&	getDisplayName(	const	Locale&			inLocale,
														UnicodeString&	name) const;
    /**
     * Generates a hash code for the locale. Since Locales are often used in hashtables, 
	 * caches the value for speed.
     */
				t_int32			hashCode() const;

    /**
	 * Retuns a list of all installed locales.
	 * @param count	Receives the number of locales in the list.
	 * @return		A pointer to an array of Locale objects.  This array is the list
	 *				of all locales with installed resource files.  The called does NOT
	 *				get ownership of this list, and must NOT delete it.
     */
	static	const	Locale*		getAvailableLocales(t_int32& count);

    /**
     * Get the path to the ResourceBundle locale files. This path will be a
     * platform-specific path name ending in a directory separator, so that file
     * names may be concatenated to it. This path may be changed by calling
     * setDataDirectory(). If setDataDirectory() has not been called yet,
     * getDataDirectory() will return a platform-dependent default path as
     * specified by TPlatformUtilities::getDefaultDataDirectory().
     *
     * @return Current data path.
     */
	static	const	char*		getDataDirectory();

    /**
     * Set the path to the ResourceBundle locale files. After making this call,
     * all objects in the Unicode Analytics package will read ResourceBundle
     * data files in the specified directory in order to obtain locale data.
     *
     * @param path The new data path to be set to.
     */
	static	void				setDataDirectory(const char* path);

    /**
     * Performs the functions necessary to terminate the library.
     */
        static  void terminateLibrary(void);

private:

	enum	EPOSIXPortion {
		LANGUAGE,
		COUNTRY,
		VARIANT
	};

	static		void			parsePOSIXID(const	UnicodeString&		str,
													UnicodeString&		result,
													EPOSIXPortion		part);
private:
	// These strings describe the resources we attempt to load from
	// the locale ResourceBundle data file.
	static  const char*			kLocaleString;
	static	const char*			kShortLanguage;
	static	const char*			kShortCountry;
	static	const char*			kLocaleID;
	static	const char*			kLanguages;
	static	const char*			kCountries;

	// The default locale we use if we can't get one from the host.
	static	const char*			kDefaultLocaleOfLastResort;

	static	Locale				defaultLocale;
	static	Locale*				localeList;
	static	t_int32				localeListCount;
	static	char*				fgDataDirectory;

			UnicodeString		id;
};

#ifdef NLS_MAC
#pragma export off
#endif


inline t_int32 
Locale::hashCode() const
{
    return( id.hashCode() );
}

inline t_bool
Locale::operator==(	const	Locale&	other) const
{
	return (this == &other || id == other.id);
}

inline t_bool
Locale::operator!=(const	Locale&		other) const
{
	return !operator==(other);
}

#endif
