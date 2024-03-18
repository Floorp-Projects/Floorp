package org.mozilla.fenix.components

import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.components.metrics.fonts.FontEnumerationWorker
import org.mozilla.fenix.components.metrics.fonts.FontParser

class FontParserTest {
    @Test
    fun testSanityAssertion() {
        /*
         Changing the below constant causes _all_ Nightly users to send a (large) Telemetry event containing
         their font information. Do not change this value unless you explicitly intend this.
         */
        assertEquals(4, FontEnumerationWorker.kDesiredSubmissions)
    }

    @Test
    fun testFontParsing() {
        val assetManager = InstrumentationRegistry.getInstrumentation().context.assets
        val font1 = FontParser.parse("no-path", assetManager.open("resources/TestTTF.ttf"))
        assertEquals(
            "\u0000T\u0000e\u0000s\u0000t\u0000 \u0000T" +
                "\u0000T\u0000F",
            font1.family,
        )
        assertEquals(
            "\u0000V\u0000e\u0000r\u0000s\u0000i\u0000o\u0000n\u0000 \u00001\u0000." +
                "\u00000\u00000\u00000",
            font1.fontVersion,
        )
        assertEquals(
            "\u0000T\u0000e\u0000s\u0000t\u0000 \u0000T\u0000T\u0000F",
            font1.fullName,
        )
        assertEquals("\u0000R\u0000e\u0000g\u0000u\u0000l\u0000a\u0000r", font1.subFamily)
        assertEquals(
            "\u0000F\u0000o\u0000n\u0000t\u0000T\u0000o\u0000o\u0000l\u0000s\u0000:\u0000 " +
                "\u0000T\u0000e\u0000s\u0000t\u0000 \u0000T\u0000T\u0000F\u0000:\u0000 \u00002\u00000\u00001\u00005",
            font1.uniqueSubFamily,
        )
        assertEquals(
            "C4E8CE309F44A131D061D73B2580E922A7F5ECC8D7109797AC0FF58BF8723B7B",
            font1.hash,
        )
        assertEquals(3516272951, font1.created)
        assertEquals(3573411749, font1.modified)
        assertEquals(65536, font1.revision)
        val font2 = FontParser.parse("no-path", assetManager.open("resources/TestTTC.ttc"))
        assertEquals(
            "\u0000T\u0000e\u0000s\u0000t\u0000 \u0000T" +
                "\u0000T\u0000F",
            font2.family,
        )
        assertEquals(
            "\u0000V\u0000e\u0000r\u0000s\u0000i\u0000o\u0000n\u0000 \u00001\u0000." +
                "\u00000\u00000\u00000",
            font2.fontVersion,
        )
        assertEquals(
            "\u0000T\u0000e\u0000s\u0000t\u0000 \u0000T\u0000T\u0000F",
            font2.fullName,
        )
        assertEquals("\u0000R\u0000e\u0000g\u0000u\u0000l\u0000a\u0000r", font1.subFamily)
        assertEquals(
            "\u0000F\u0000o\u0000n\u0000t\u0000T\u0000o\u0000o\u0000l\u0000s\u0000:\u0000 " +
                "\u0000T\u0000e\u0000s\u0000t\u0000 \u0000T\u0000T\u0000F\u0000:\u0000 \u00002\u00000\u00001\u00005",
            font2.uniqueSubFamily,
        )
        assertEquals(
            "A8521588045ED5F1F8B07EECAAC06ED3186C644655BFAC00DD4507CD316FBDC5",
            font2.hash,
        )
        assertEquals(3516272951, font2.created)
        assertEquals(3573411749, font2.modified)
        assertEquals(65536, font2.revision)
    }
}
