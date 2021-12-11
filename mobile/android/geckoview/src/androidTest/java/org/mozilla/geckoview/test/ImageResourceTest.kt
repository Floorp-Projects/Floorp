/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.filters.MediumTest
import androidx.test.ext.junit.runners.AndroidJUnit4

import org.hamcrest.Matchers.*
import org.junit.Test
import org.junit.runner.RunWith

import org.mozilla.geckoview.GeckoResult
import org.mozilla.gecko.util.ImageResource

class TestImage(
        val path: String,
        val type: String?,
        val sizes: String?,
        val widths: Array<Int>?,
        val heights: Array<Int>?) {}

@RunWith(AndroidJUnit4::class)
@MediumTest
class ImageResourceTest : BaseSessionTest() {
    companion object {
        val kValidTestImage1 = TestImage(
            "path.ico", "image/icon", "16x16 32x32 64x64",
            arrayOf(16, 32, 64),
            arrayOf(16, 32, 64)
        )

        val kValidTestImage2 = TestImage(
            "path.png", "image/png", "128x128",
            arrayOf(128),
            arrayOf(128)
        )

        val kValidTestImage3 = TestImage(
            "path.jpg", "image/jpg", "256x256",
            arrayOf(256),
            arrayOf(256)
        )

        val kValidTestImage4 = TestImage(
            "path.png", "image/png", "300x128",
            arrayOf(300),
            arrayOf(128)
        )

        val kValidTestImage5 = TestImage(
            "path.svg", "image/svg", "any",
            arrayOf(0),
            arrayOf(0)
        )

        val kValidTestImage6 = TestImage(
            "path.svg", null, null,
            null,
            null
        )
    }

    fun verifyEqual(image: ImageResource, base: TestImage) {
        assertThat(
                "Path should match",
                image.src,
                equalTo(base.path))
        assertThat(
                "Type should match",
                image.type,
                equalTo(base.type))

        assertThat(
                "Sizes should match",
                image.sizes?.size,
                equalTo(base.widths?.size))

        assertThat(
                "Sizes should match",
                image.sizes?.size,
                equalTo(base.heights?.size))

        if (image.sizes == null) {
            return;
        }
        for (i in 0 until image.sizes!!.size) {
            assertThat(
                    "Sizes widths should match",
                    image.sizes!![i].width,
                    equalTo(base.widths!![i]))
            assertThat(
                    "Sizes heights should match",
                    image.sizes!![i].height,
                    equalTo(base.heights!![i]))
        }
    }

    fun testValidImage(base: TestImage) {
        var image = ImageResource(base.path, base.type, base.sizes)
        verifyEqual(image, base)
    }

    fun buildCollection(bases: Array<TestImage>) : ImageResource.Collection {
        val builder = ImageResource.Collection.Builder()

        bases.forEach {
            builder.add(ImageResource(it.path, it.type, it.sizes))
        }

        return builder.build()
    }

    @Test
    fun validImage() {
        testValidImage(kValidTestImage1)
        testValidImage(kValidTestImage2)
        testValidImage(kValidTestImage3)
        testValidImage(kValidTestImage4)
        testValidImage(kValidTestImage5)
        testValidImage(kValidTestImage6)
    }

    @Test
    fun invalidImageSize() {
        val invalidImage1 = TestImage(
            "path.ico", "image/icon", "16x16 32",
            arrayOf(16),
            arrayOf(16)
        )
        testValidImage(invalidImage1)

        val invalidImage2 = TestImage(
            "path.ico", "image/icon", "16x16 32xa32",
            arrayOf(16),
            arrayOf(16)
        )
        testValidImage(invalidImage2)

        val invalidImage3 = TestImage(
            "path.ico", "image/icon", "",
            null,
            null
        )
        testValidImage(invalidImage3)

        val invalidImage4 = TestImage(
            "path.ico", "image/icon", "abxab",
            null,
            null
        )
        testValidImage(invalidImage4)
    }

    @Test
    fun getBestRegular() {
        val collection = buildCollection(arrayOf(
            kValidTestImage1, kValidTestImage2, kValidTestImage3,
            kValidTestImage4))
        // 16, 32, 64
        verifyEqual(collection.getBest(10)!!, kValidTestImage1)
        verifyEqual(collection.getBest(16)!!, kValidTestImage1)
        verifyEqual(collection.getBest(30)!!, kValidTestImage1)
        verifyEqual(collection.getBest(90)!!, kValidTestImage1)

        // 128
        verifyEqual(collection.getBest(100)!!, kValidTestImage2)
        verifyEqual(collection.getBest(120)!!, kValidTestImage2)
        verifyEqual(collection.getBest(140)!!, kValidTestImage2)

        // 256
        verifyEqual(collection.getBest(210)!!, kValidTestImage3)
        verifyEqual(collection.getBest(256)!!, kValidTestImage3)
        verifyEqual(collection.getBest(270)!!, kValidTestImage3)

        // 300
        verifyEqual(collection.getBest(280)!!, kValidTestImage4)
        verifyEqual(collection.getBest(10000)!!, kValidTestImage4)
    }

    @Test
    fun getBestAny() {
        val collection = buildCollection(arrayOf(
            kValidTestImage1, kValidTestImage2, kValidTestImage3,
            kValidTestImage4, kValidTestImage5))
        // any
        verifyEqual(collection.getBest(10)!!, kValidTestImage5)
        verifyEqual(collection.getBest(16)!!, kValidTestImage5)
        verifyEqual(collection.getBest(30)!!, kValidTestImage5)
        verifyEqual(collection.getBest(90)!!, kValidTestImage5)
        verifyEqual(collection.getBest(100)!!, kValidTestImage5)
        verifyEqual(collection.getBest(120)!!, kValidTestImage5)
        verifyEqual(collection.getBest(140)!!, kValidTestImage5)
        verifyEqual(collection.getBest(210)!!, kValidTestImage5)
        verifyEqual(collection.getBest(256)!!, kValidTestImage5)
        verifyEqual(collection.getBest(270)!!, kValidTestImage5)
        verifyEqual(collection.getBest(280)!!, kValidTestImage5)
        verifyEqual(collection.getBest(10000)!!, kValidTestImage5)
    }

    @Test
    fun getBestNull() {
        // Don't include `any` since two `any` cases would result in undefined
        // results.
        val collection = buildCollection(arrayOf(
            kValidTestImage1, kValidTestImage2, kValidTestImage3,
            kValidTestImage4, kValidTestImage6))
        // null, handled as any
        verifyEqual(collection.getBest(10)!!, kValidTestImage6)
        verifyEqual(collection.getBest(16)!!, kValidTestImage6)
        verifyEqual(collection.getBest(30)!!, kValidTestImage6)
        verifyEqual(collection.getBest(90)!!, kValidTestImage6)
        verifyEqual(collection.getBest(100)!!, kValidTestImage6)
        verifyEqual(collection.getBest(120)!!, kValidTestImage6)
        verifyEqual(collection.getBest(140)!!, kValidTestImage6)
        verifyEqual(collection.getBest(210)!!, kValidTestImage6)
        verifyEqual(collection.getBest(256)!!, kValidTestImage6)
        verifyEqual(collection.getBest(270)!!, kValidTestImage6)
        verifyEqual(collection.getBest(280)!!, kValidTestImage6)
        verifyEqual(collection.getBest(10000)!!, kValidTestImage6)
    }

    @Test
    fun getBitmap() {
        val actualWidth = 265
        val actualHeight = 199

        val testImage = TestImage(
            createTestUrl("/assets/www/images/test.gif"),
            "image/gif",
            "any",
            arrayOf(0),
            arrayOf(0)
        )
        val collection = buildCollection(arrayOf(testImage))
        val image = collection.getBest(actualWidth)

        verifyEqual(image!!, testImage)

        sessionRule.waitForResult(image.getBitmap(actualWidth)
            .then { bitmap ->
                assertThat(
                        "Bitmap should be non-null",
                        bitmap,
                        notNullValue())
                assertThat(
                        "Bitmap width should match",
                        bitmap!!.getWidth(),
                        equalTo(actualWidth))
                assertThat(
                        "Bitmap height should match",
                        bitmap.getHeight(),
                        equalTo(actualHeight))

                GeckoResult.fromValue(null)
            })
    }
}
