/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.helpers

import mozilla.components.service.pocket.data.PocketGlobalVideoRecommendation
import mozilla.components.service.pocket.data.PocketListenArticleMetadata

private const val POCKET_DIR = "pocket"

/**
 * Accessors to resources used in testing. These files are available in `app/src/test/resources`.
 */
enum class PocketTestResource(private val path: String) {
    // For expected Kotlin data type representations of this test data, see the companion object.
    POCKET_VIDEO_RECOMMENDATION("$POCKET_DIR/video_recommendations.json"),
    LISTEN_ARTICLE_METADATA("$POCKET_DIR/listen_article_metadata.json"),

    // NEVER COMMIT THESE KEY FILES. Add this file with a valid API key to use this resource.
    API_KEY("$POCKET_DIR/apiKey.txt"),
    LISTEN_ACCESS_TOKEN("$POCKET_DIR/listenAccessToken.txt"); // note: file must not end in newline.

    /** @return the raw resource. */
    fun get(): String = this::class.java.classLoader!!.getResource(path)!!.readText()

    companion object {
        fun getExpectedPocketVideoRecommendationFirstTwo(): List<PocketGlobalVideoRecommendation> = listOf(
            PocketGlobalVideoRecommendation(
                id = 27587,
                url = "https://www.youtube.com/watch?v=953Qt4FnAcU",
                tvURL = "https://www.youtube.com/tv#/watch/video/idle?v=953Qt4FnAcU",
                title = "How a Master Pastry Chef Uses Architecture to Make Sky High Pastries",
                excerpt = "At New York City's International Culinary Center, host Rebecca DeAngelis makes a modern day croquembouche with architect-turned pastry chef Jansen Chan",
                domain = "youtube.com",
                imageSrc = "https://img-getpocket.cdn.mozilla.net/direct?url=http%3A%2F%2Fimg.youtube.com%2Fvi%2F953Qt4FnAcU%2Fmaxresdefault.jpg&resize=w450",
                publishedTimestamp = "0",
                sortId = 0,
                popularitySortId = 20,
                authors = listOf(
                    PocketGlobalVideoRecommendation.Author(
                        id = "96612022",
                        name = "Eater",
                        url = "http://www.youtube.com/channel/UCRzPUBhXUZHclB7B5bURFXw"
                    )
                )
            ),
            PocketGlobalVideoRecommendation(
                id = 27581,
                url = "https://www.youtube.com/watch?v=GHZ7-kq3GDQ",
                tvURL = "https://www.youtube.com/tv#/watch/video/idle?v=GHZ7-kq3GDQ",
                title = "How Does Having Too Much Power Affect Your Brain?",
                excerpt = "Power is tied to a hierarchical escalator that we rise up through decisions and actions. But once we have power, how does it affect our brain and behavior? How Our Brains Respond to People Who Aren't Like Us - https://youtu.be/KIwe_O0am4URead More:The age of adolescencehttps://www.thelancet.com/jour",
                domain = "youtube.com",
                imageSrc = "https://img-getpocket.cdn.mozilla.net/direct?url=http%3A%2F%2Fimg.youtube.com%2Fvi%2FGHZ7-kq3GDQ%2Fmaxresdefault.jpg&resize=w450",
                publishedTimestamp = "0",
                sortId = 1,
                popularitySortId = 17,
                authors = listOf(
                    PocketGlobalVideoRecommendation.Author(
                        id = "96612138",
                        name = "Seeker",
                        url = "http://www.youtube.com/channel/UCzWQYUVCpZqtN93H8RR44Qw"
                    )
                )
            )
        )

        fun getExpectedListenArticleMetadata(): PocketListenArticleMetadata = PocketListenArticleMetadata(
            format = "mp3",
            audioUrl = "https://scout-streaming-2018.s3.amazonaws.com/76cd4614-3bba-4272-b07c-0b3161aed7d9.mp3",
            status = PocketListenArticleMetadata.Status.AVAILABLE,
            voice = "Salli",
            durationSeconds = 353,
            size = "1771738"
        )
    }
}
