/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.menu.ext

import android.graphics.Color
import mozilla.components.concept.menu.candidate.CompoundMenuCandidate
import mozilla.components.concept.menu.candidate.ContainerStyle
import mozilla.components.concept.menu.candidate.DecorativeTextMenuCandidate
import mozilla.components.concept.menu.candidate.DividerMenuCandidate
import mozilla.components.concept.menu.candidate.DrawableMenuIcon
import mozilla.components.concept.menu.candidate.HighPriorityHighlightEffect
import mozilla.components.concept.menu.candidate.LowPriorityHighlightEffect
import mozilla.components.concept.menu.candidate.RowMenuCandidate
import mozilla.components.concept.menu.candidate.SmallMenuCandidate
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import org.junit.Assert.assertEquals
import org.junit.Test

class MenuCandidateTest {

    @Test
    fun `higher priority items will be selected by max`() {
        assertEquals(
            HighPriorityHighlightEffect(Color.BLACK),
            sequenceOf(
                LowPriorityHighlightEffect(Color.BLUE),
                HighPriorityHighlightEffect(Color.BLACK),
            ).max(),
        )
    }

    @Test
    fun `items earlier in sequence will be selected by max`() {
        assertEquals(
            LowPriorityHighlightEffect(Color.BLUE),
            sequenceOf(
                LowPriorityHighlightEffect(Color.BLUE),
                LowPriorityHighlightEffect(Color.YELLOW),
            ).max(),
        )
    }

    @Test
    fun `effects returns effects from row candidate`() {
        assertEquals(
            listOf(
                LowPriorityHighlightEffect(Color.BLUE),
                LowPriorityHighlightEffect(Color.YELLOW),
            ),
            listOf(
                RowMenuCandidate(
                    listOf(
                        SmallMenuCandidate(
                            "",
                            icon = DrawableMenuIcon(
                                null,
                                effect = LowPriorityHighlightEffect(Color.BLUE),
                            ),
                        ),
                        SmallMenuCandidate(
                            "",
                            icon = DrawableMenuIcon(
                                null,
                                effect = LowPriorityHighlightEffect(Color.RED),
                            ),
                            containerStyle = ContainerStyle(isVisible = false),
                        ),
                        SmallMenuCandidate(
                            "",
                            icon = DrawableMenuIcon(
                                null,
                                effect = LowPriorityHighlightEffect(Color.RED),
                            ),
                            containerStyle = ContainerStyle(isEnabled = false),
                        ),
                        SmallMenuCandidate(
                            "",
                            icon = DrawableMenuIcon(
                                null,
                                effect = LowPriorityHighlightEffect(Color.YELLOW),
                            ),
                        ),
                    ),
                ),
            ).effects().toList(),
        )
    }

    @Test
    fun `effects returns effects from text candidates`() {
        assertEquals(
            listOf(
                HighPriorityHighlightEffect(Color.BLUE),
                LowPriorityHighlightEffect(Color.YELLOW),
                HighPriorityHighlightEffect(Color.BLACK),
                HighPriorityHighlightEffect(Color.BLUE),
                LowPriorityHighlightEffect(Color.RED),
            ),
            listOf(
                TextMenuCandidate(
                    "",
                    start = DrawableMenuIcon(
                        null,
                        effect = LowPriorityHighlightEffect(Color.YELLOW),
                    ),
                    effect = HighPriorityHighlightEffect(Color.BLUE),
                ),
                DecorativeTextMenuCandidate(""),
                TextMenuCandidate(""),
                DividerMenuCandidate(),
                TextMenuCandidate(
                    "",
                    effect = HighPriorityHighlightEffect(Color.BLACK),
                ),
                TextMenuCandidate(
                    "",
                    containerStyle = ContainerStyle(isVisible = false),
                    effect = HighPriorityHighlightEffect(Color.BLACK),
                ),
                TextMenuCandidate(
                    "",
                    end = DrawableMenuIcon(
                        null,
                        effect = LowPriorityHighlightEffect(Color.RED),
                    ),
                    effect = HighPriorityHighlightEffect(Color.BLUE),
                ),
            ).effects().toList(),
        )
    }

    @Test
    fun `effects returns effects from compound candidates`() {
        assertEquals(
            listOf(
                HighPriorityHighlightEffect(Color.BLUE),
                LowPriorityHighlightEffect(Color.YELLOW),
                HighPriorityHighlightEffect(Color.BLACK),
                LowPriorityHighlightEffect(Color.RED),
            ),
            listOf(
                CompoundMenuCandidate(
                    "",
                    isChecked = true,
                    start = DrawableMenuIcon(
                        null,
                        effect = LowPriorityHighlightEffect(Color.YELLOW),
                    ),
                    end = CompoundMenuCandidate.ButtonType.CHECKBOX,
                    effect = HighPriorityHighlightEffect(Color.BLUE),
                ),
                CompoundMenuCandidate(
                    "",
                    isChecked = false,
                    end = CompoundMenuCandidate.ButtonType.SWITCH,
                    effect = HighPriorityHighlightEffect(Color.BLACK),
                ),
                CompoundMenuCandidate(
                    "",
                    isChecked = false,
                    end = CompoundMenuCandidate.ButtonType.SWITCH,
                    containerStyle = ContainerStyle(isEnabled = false),
                    effect = HighPriorityHighlightEffect(Color.BLACK),
                ),
                CompoundMenuCandidate(
                    "",
                    isChecked = true,
                    start = DrawableMenuIcon(
                        null,
                        effect = LowPriorityHighlightEffect(Color.RED),
                    ),
                    end = CompoundMenuCandidate.ButtonType.CHECKBOX,
                ),
            ).effects().toList(),
        )
    }
}
