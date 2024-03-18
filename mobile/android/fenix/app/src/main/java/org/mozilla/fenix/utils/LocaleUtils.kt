/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import java.util.Locale

/**
 * Helper class for displaying Locale name.
 */
object LocaleUtils {

    /**
     * Returns a name for the locale that is appropriate for display to the user. The display name
     * will be in the language of the specified locale.
     *
     * For example, in an any language locale, the German language locale or "de" would be
     * presented as "Deutsch".
     *
     * @param locale The locale to determine a language name.
     * @return A default title case display name in the locale's language.
     */
    fun getDisplayName(locale: Locale): String {
        val displayName = locale.getDisplayName(locale)
            .replaceFirstChar { if (it.isLowerCase()) it.titlecase(locale) else it.toString() }
        if (displayName.equals(locale.toString(), ignoreCase = true)) {
            return LOCALE_TO_DISPLAY_NATIVE_NAME_MAP[locale.toString()] ?: displayName
        }
        return displayName
    }

    /**
     * Returns a name for the locale that is appropriate for display to the user based on the user's
     * specified language.
     *
     * For example, in an an English language locale, the German language locale or "de" would be
     * presented as "German".
     *
     * @param userLocale The locale the user is using.
     * @param languageLocale The locale to localize the language name into the user's language.
     *
     * @return A default language name in the specified user's locale language.
     */
    fun getLocalizedDisplayName(userLocale: Locale, languageLocale: Locale): String {
        val displayName = Locale.forLanguageTag(languageLocale.toLanguageTag()).getDisplayName(userLocale)
        return if (userLocale == Locale.ENGLISH) {
            LOCALE_TO_DISPLAY_ENGLISH_NAME_MAP[languageLocale.toString()] ?: displayName
        } else {
            displayName
        }
    }

    private val LOCALE_TO_DISPLAY_NATIVE_NAME_MAP: Map<String, String> = mapOf(
        "an" to "Aragonés",
        "anp" to "अंगिका",
        "ar" to "العربية",
        "ast" to "Asturianu",
        "ay" to "Aimara",
        "az" to "Azərbaycan dili",
        "be" to "беларуская мова",
        "bg" to "български език",
        "bn" to "বাংলা",
        "br" to "Brezhoneg",
        "bs" to "Bosanski jezik",
        "ca" to "Català",
        "cak" to "Kaqchikel",
        "ceb" to "Cebuano",
        "co" to "Corsu",
        "cs" to "čeština",
        "cy" to "Cymraeg",
        "da" to "dansk",
        "de" to "Deutsch",
        "dsb" to "dolnoserbski",
        "el" to "ελληνικά",
        "eo" to "Esperanto",
        "es" to "Español",
        "et" to "Eesti",
        "eu" to "Euskara",
        "fa" to "فارسی",
        "ff" to "Fulfulde",
        "fi" to "Suomi",
        "fr" to "Français",
        "fy-NL" to "Frisian",
        "ga-IE" to "Gaeilge",
        "gd" to "Gàidhlig",
        "gl" to "Galego",
        "gn" to "Avañe'ẽ",
        "gu-IN" to "ગુજરાતી",
        "he" to "עברית",
        "hi-IN" to "हिन्दी",
        "hil" to "Ilonggo",
        "hr" to "hrvatski jezik",
        "hsb" to "Hornjoserbsce",
        "hu" to "Magyar",
        "hus" to "Tének",
        "hy-AM" to "հայերեն",
        "ia" to "Interlingua",
        "id" to "Bahasa Indonesia",
        "is" to "Íslenska",
        "it" to "Italiano",
        "ixl" to "Ixil",
        "ja" to "日本語 (にほんご)",
        "jv" to "Basa Jawa",
        "ka" to "ქართული",
        "kaa" to "Karakalpak",
        "kab" to "Taqbaylit",
        "kk" to "қазақ тілі",
        "kmr" to "Kurmancî",
        "kn" to "ಕನ್ನಡ",
        "ko" to "한국어",
        "lij" to "Ligure",
        "lo" to "ພາສາລາວ",
        "lt" to "lietuvių kalba",
        "mix" to "Tu'un savi",
        "ml" to "മലയാളം",
        "mr" to "मराठी",
        "ms" to "Bahasa Melayu ملايو‎",
        "my" to "ဗမာစာ",
        "meh" to "Tu´un savi ñuu Yasi'í Yuku Iti",
        "nb-NO" to "Bokmål",
        "ne-NP" to "नेपाली",
        "nl" to "Nederlands",
        "nn-NO" to "Nynorsk",
        "nv" to "Diné bizaad",
        "oc" to "Occitan",
        "pa-IN" to "Panjābī",
        "pl" to "Polszczyzna",
        "pt" to "Português",
        "pai" to "Paa ipai",
        "ppl" to "Náhuat Pipil",
        "quy" to "Chanka Qhichwa",
        "quc" to "K'iche'",
        "rm" to "Rumantsch Grischun",
        "ro" to "Română",
        "ru" to "русский",
        "sat" to "ᱥᱟᱱᱛᱟᱲᱤ",
        "sk" to "Slovak",
        "skr" to "سرائیکی",
        "sl" to "Slovenian",
        "sn" to "ChiShona",
        "sq" to "Shqip",
        "sr" to "српски језик",
        "su" to "Basa Sunda",
        "sv-SE" to "Svenska",
        "ta" to "தமிழ்",
        "te" to "తెలుగు",
        "tg" to "тоҷикӣ, toçikī, تاجیکی‎",
        "th" to "ไทย",
        "tl" to "Wikang Tagalog",
        "tok" to "Toki Pona",
        "tr" to "Türkçe",
        "trs" to "Triqui",
        "tt" to "татарча",
        "tsz" to "P'urhepecha",
        "uk" to "Українська",
        "ur" to "اردو",
        "uz" to "Oʻzbek",
        "vec" to "Vèneto",
        "vi" to "Tiếng Việt",
        "wo" to "Wolof",
        "zam" to "DíɁztè",
    )

    val LOCALE_TO_DISPLAY_ENGLISH_NAME_MAP: Map<String, String> = mapOf(
        "an" to "Aragonese",
        "ar" to "Arabic",
        "ast" to "Asturianu",
        "az" to "Azerbaijani",
        "be" to "Belarusian",
        "bg" to "Bulgarian",
        "bn" to "Bengali",
        "br" to "Breton",
        "bs" to "Bosnian",
        "ca" to "Catalan",
        "cak" to "Kaqchikel",
        "ceb" to "Cebuano",
        "co" to "Corsican",
        "cs" to "Czech",
        "cy" to "Welsh",
        "da" to "Danish",
        "de" to "German",
        "dsb" to "Sorbian, Lower",
        "el" to "Greek",
        "eo" to "Esperanto",
        "es" to "Spanish",
        "et" to "Estonian",
        "eu" to "Basque",
        "fa" to "Persian",
        "ff" to "Fulah",
        "fi" to "Finnish",
        "fr" to "French",
        "fy-NL" to "Frisian",
        "ga-IE" to "Irish",
        "gd" to "Gaelic",
        "gl" to "Galician",
        "gn" to "Guarani",
        "gu-IN" to "Gujarati",
        "he" to "Hebrew",
        "hi-IN" to "Hindi",
        "hil" to "Hiligaynon",
        "hr" to "Croatian",
        "hsb" to "Sorbian, Upper",
        "hu" to "Hungarian",
        "hy-AM" to "Armenian",
        "id" to "Indonesian",
        "is" to "Icelandic",
        "it" to "Italian",
        "ja" to "Japanese",
        "ka" to "Georgian",
        "kaa" to "Karakalpak",
        "kab" to "Kabyle",
        "kk" to "Kazakh",
        "kmr" to "Kurmanji Kurdish",
        "kn" to "Kannada",
        "ko" to "Korean",
        "lij" to "Ligurian",
        "lo" to "Lao",
        "lt" to "Lithuanian",
        "mix" to "Mixtepec Mixtec",
        "ml" to "Malayalam",
        "mr" to "Marathi",
        "ms" to "Malay",
        "my" to "Burmese",
        "nb-NO" to "Norwegian Bokmål",
        "ne-NP" to "Nepali",
        "nl" to "Dutch, Flemish",
        "nn-NO" to "Norwegian Nynorsk",
        "nv" to "Navajo, Navaho",
        "oc" to "Occitan",
        "pa-IN" to "Punjabi",
        "pl" to "Polish",
        "pt-BR" to "",
        "pt-PT" to "",
        "rm" to "Romansh",
        "ro" to "Română",
        "ru" to "Russian",
        "sat" to "Santali",
        "sk" to "Slovak",
        "skr" to "Saraiki",
        "sl" to "Slovenian",
        "sq" to "Albanian",
        "sr" to "Serbian",
        "su" to "Sundanese",
        "sv-SE" to "Swedish",
        "ta" to "Tamil",
        "te" to "Telugu",
        "tg" to "Tajik",
        "th" to "Thai",
        "tl" to "Tagalog",
        "tok" to "Toki Pona",
        "tr" to "Turkish",
        "trs" to "Triqui",
        "uk" to "Ukrainian",
        "ur" to "Urdu",
        "uz" to "Uzbek",
        "vec" to "Venitian",
        "vi" to "Vietnamese",
    )
}
