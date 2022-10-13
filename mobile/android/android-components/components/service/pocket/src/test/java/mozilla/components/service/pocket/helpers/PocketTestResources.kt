/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.helpers

import mozilla.components.service.pocket.PocketStory.PocketRecommendedStory
import mozilla.components.service.pocket.spocs.api.ApiSpoc
import mozilla.components.service.pocket.spocs.api.ApiSpocCaps
import mozilla.components.service.pocket.spocs.api.ApiSpocShim
import mozilla.components.service.pocket.spocs.db.SpocEntity
import mozilla.components.service.pocket.stories.api.PocketApiStory
import mozilla.components.service.pocket.stories.db.PocketStoryEntity

private const val POCKET_DIR = "pocket"

/**
 * Accessors to resources used in testing.
 */
internal object PocketTestResources {
    val pocketEndointFiveStoriesResponse = this::class.java.classLoader!!.getResource(
        "$POCKET_DIR/stories_recommendations_response.json",
    )!!.readText()

    val pocketEndpointThreeSpocsResponse = this::class.java.classLoader!!.getResource(
        "$POCKET_DIR/sponsored_stories_response.json",
    )!!.readText()

    val apiExpectedPocketStoriesRecommendations: List<PocketApiStory> = listOf(
        PocketApiStory(
            title = "How to Remember Anything You Really Want to Remember, Backed by Science",
            url = "https://getpocket.com/explore/item/how-to-remember-anything-you-really-want-to-remember-backed-by-science",
            imageUrl = "https://img-getpocket.cdn.mozilla.net/{wh}/filters:format(jpeg):quality(60):no_upscale():strip_exif()/https%3A%2F%2Fpocket-image-cache.com%2F1200x%2Ffilters%3Aformat(jpg)%3Aextract_focal()%2Fhttps%253A%252F%252Fwww.incimages.com%252Fuploaded_files%252Fimage%252F1920x1080%252Fgetty-862457080_394628.jpg",
            publisher = "Pocket",
            category = "general",
            timeToRead = 3,
        ),
        PocketApiStory(
            title = "‘I Don’t Want to Be Like a Family With My Co-Workers’",
            url = "https://www.thecut.com/article/i-dont-want-to-be-like-a-family-with-my-co-workers.html",
            imageUrl = "https://img-getpocket.cdn.mozilla.net/{wh}/filters:format(jpeg):quality(60):no_upscale():strip_exif()/https%3A%2F%2Fpyxis.nymag.com%2Fv1%2Fimgs%2Fac8%2Fd22%2F315cd0cf1e3a43edfe0e0548f2edbcb1a1-ask-a-boss.1x.rsocial.w1200.jpg",
            publisher = "The Cut",
            category = "general",
            timeToRead = 5,
        ),
        PocketApiStory(
            title = "How America Failed in Afghanistan",
            url = "https://www.newyorker.com/news/q-and-a/how-america-failed-in-afghanistan",
            imageUrl = "https://img-getpocket.cdn.mozilla.net/{wh}/filters:format(jpeg):quality(60):no_upscale():strip_exif()/https%3A%2F%2Fmedia.newyorker.com%2Fphotos%2F6119484157b611aec9c99b43%2F16%3A9%2Fw_1280%2Cc_limit%2FChotiner-Afghanistan01.jpg",
            publisher = "The New Yorker",
            category = "general",
            timeToRead = 14,
        ),
        PocketApiStory(
            title = "How digital beauty filters perpetuate colorism",
            url = "https://www.technologyreview.com/2021/08/15/1031804/digital-beauty-filters-photoshop-photo-editing-colorism-racism/",
            imageUrl = "https://img-getpocket.cdn.mozilla.net/{wh}/filters:format(jpeg):quality(60):no_upscale():strip_exif()/https%3A%2F%2Fwp.technologyreview.com%2Fwp-content%2Fuploads%2F2021%2F08%2FBeautyScoreColorism.jpg%3Fresize%3D1200%2C600",
            publisher = "MIT Technology Review",
            category = "general",
            timeToRead = 11,
        ),
        PocketApiStory(
            title = "How to Get Rid of Black Mold Naturally",
            url = "https://getpocket.com/explore/item/how-to-get-rid-of-black-mold-naturally",
            imageUrl = "https://img-getpocket.cdn.mozilla.net/{wh}/filters:format(jpeg):quality(60):no_upscale():strip_exif()/https%3A%2F%2Fpocket-image-cache.com%2F1200x%2Ffilters%3Aformat(jpg)%3Aextract_focal()%2Fhttps%253A%252F%252Fpocket-syndicated-images.s3.amazonaws.com%252Farticles%252F6757%252F1628024495_6109ae86db6cc.png",
            publisher = "Pocket",
            category = "general",
            timeToRead = 4,
        ),
    )

    val apiExpectedPocketSpocs: List<ApiSpoc> = listOf(
        ApiSpoc(
            flightId = 191739319,
            title = "Eating Keto Has Never Been So Easy With Green Chef",
            url = "https://i.geistm.com/l/GC_7ReasonsKetoV2_Journiest?bcid=601c567ac5b18a0414cce1d4&bhid=624f3ea9adad7604086ac6b3&utm_content=PKT_A_7ReasonsKetoV2_Journiest_40702022_RawMeatballUGC_130Off_601c567ac5b18a0414cce1d4_624f3ea9adad7604086ac6b3&tv=su4&ct=NAT-PK-PROS-130OFF5WEEK-037&utm_medium=DB&utm_source=pocket~geistm&utm_campaign=PKT_A_7ReasonsKetoV2_Journiest_40702022_RawMeatballUGC_130Off",
            imageSrc = "https://img-getpocket.cdn.mozilla.net/direct?url=realUrl.png&resize=w618-h310",
            sponsor = "Green Chef",
            shim = ApiSpocShim(
                click = "193815086ClickShim",
                impression = "193815086ImpressionShim",
            ),
            priority = 3,
            caps = ApiSpocCaps(
                lifetimeCount = 50,
                flightPeriod = 86400,
                flightCount = 10,
            ),
        ),
        ApiSpoc(
            flightId = 191739667,
            title = "This Leading Cash Back Card Is a Slam Dunk if You Want a One-Card Wallet",
            url = "https://www.fool.com/the-ascent/credit-cards/landing/discover-it-cash-back-review-v2-csr/?utm_site=theascent&utm_campaign=ta-cc-co-pocket-discb-04012022-5-na-firefox&utm_medium=cpc&utm_source=pocket",
            imageSrc = "https://img-getpocket.cdn.mozilla.net/direct?url=https%3A//s.zkcdn.net/Advertisers/359f56a5423c4926ab3aa148e448d839.webp&resize=w618-h310",
            sponsor = "The Ascent",
            shim = ApiSpocShim(
                click = "177986195ClickShim",
                impression = "177986195ImpressionShim",
            ),
            priority = 2,
            caps = ApiSpocCaps(
                lifetimeCount = 50,
                flightPeriod = 86400,
                flightCount = 10,
            ),
        ),
        ApiSpoc(
            flightId = 189212196,
            title = "The Incredible Lawn Hack That Can Make Your Neighbors Green With Envy Over Your Lawn",
            url = "https://go.lawnbuddy.org/zf/50/7673?campaign=SUN_Pocket2022&creative=SUN_LawnCompare4-TheIncredibleLawnHackThatCanMakeYourNeighborsGreenWithEnvyOverYourLawn-WithoutSpendingAFortuneOnNewGrassAndWithoutBreakingASweat-20220420",
            imageSrc = "https://img-getpocket.cdn.mozilla.net/direct?url=https%3A//s.zkcdn.net/Advertisers/ce16302e184342cda0619c08b7604c9c.jpg&resize=w618-h310",
            sponsor = "Sunday",
            shim = ApiSpocShim(
                click = "192560056ClickShim",
                impression = "192560056ImpressionShim",
            ),
            priority = 1,
            caps = ApiSpocCaps(
                lifetimeCount = 50,
                flightPeriod = 86400,
                flightCount = 10,
            ),
        ),
    )

    val dbExpectedPocketStory = PocketStoryEntity(
        title = "How to Get Rid of Black Mold Naturally",
        url = "https://getpocket.com/explore/item/how-to-get-rid-of-black-mold-naturally",
        imageUrl = "https://img-getpocket.cdn.mozilla.net/{wh}/filters:format(jpeg):quality(60):no_upscale():strip_exif()/https%3A%2F%2Fpocket-image-cache.com%2F1200x%2Ffilters%3Aformat(jpg)%3Aextract_focal()%2Fhttps%253A%252F%252Fpocket-syndicated-images.s3.amazonaws.com%252Farticles%252F6757%252F1628024495_6109ae86db6cc.png",
        publisher = "Pocket",
        category = "general",
        timeToRead = 4,
        timesShown = 23,
    )

    val clientExpectedPocketStory = PocketRecommendedStory(
        title = "How digital beauty filters perpetuate colorism",
        url = "https://www.technologyreview.com/2021/08/15/1031804/digital-beauty-filters-photoshop-photo-editing-colorism-racism/",
        imageUrl = "https://img-getpocket.cdn.mozilla.net/{wh}/filters:format(jpeg):quality(60):no_upscale():strip_exif()/https%3A%2F%2Fwp.technologyreview.com%2Fwp-content%2Fuploads%2F2021%2F08%2FBeautyScoreColorism.jpg%3Fresize%3D1200%2C600",
        publisher = "MIT Technology Review",
        category = "general",
        timeToRead = 11,
        timesShown = 3,
    )

    val dbExpectedPocketSpoc = SpocEntity(
        id = 191739319,
        url = "https://i.geistm.com/l/GC_7ReasonsKetoV2_Journiest?bcid=601c567ac5b18a0414cce1d4&bhid=624f3ea9adad7604086ac6b3&utm_content=PKT_A_7ReasonsKetoV2_Journiest_40702022_RawMeatballUGC_130Off_601c567ac5b18a0414cce1d4_624f3ea9adad7604086ac6b3&tv=su4&ct=NAT-PK-PROS-130OFF5WEEK-037&utm_medium=DB&utm_source=pocket~geistm&utm_campaign=PKT_A_7ReasonsKetoV2_Journiest_40702022_RawMeatballUGC_130Off",
        title = "Eating Keto Has Never Been So Easy With Green Chef",
        imageUrl = "https://img-getpocket.cdn.mozilla.net/direct?url=realUrl.png&resize=w618-h310",
        sponsor = "Green Chef",
        clickShim = "193815086ClickShim",
        impressionShim = "193815086ImpressionShim",
        priority = 3,
        lifetimeCapCount = 50,
        flightCapCount = 10,
        flightCapPeriod = 86400,
    )
}
