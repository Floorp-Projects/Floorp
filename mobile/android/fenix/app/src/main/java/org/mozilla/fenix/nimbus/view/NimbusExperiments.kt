/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.nimbus.view

import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import org.mozilla.experiments.nimbus.AvailableExperiment
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.list.TextListItem
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * List of Nimbus Experiments.
 *
 * @param experiments List of [AvailableExperiment] that are going to be displayed.
 * @param onExperimentClick Invoked when the user clicks on an [AvailableExperiment].
 */
@Composable
fun NimbusExperiments(
    experiments: List<AvailableExperiment> = listOf(),
    onSelectedExperiment: (AvailableExperiment) -> Unit,
) {
    LazyColumn(
        modifier = Modifier.fillMaxSize(),
    ) {
        items(experiments) { experiment ->
            TextListItem(
                label = experiment.userFacingName,
                description = experiment.userFacingDescription,
                maxDescriptionLines = Int.MAX_VALUE,
                onClick = {
                    onSelectedExperiment(experiment)
                },
            )
        }
    }
}

@Composable
@LightDarkPreview
private fun NimbusExperimentsPreview() {
    val testExperiment = AvailableExperiment(
        userFacingName = "Name",
        userFacingDescription = "Description",
        slug = "slug",
        branches = emptyList(),
        referenceBranch = null,
    )

    FirefoxTheme {
        NimbusExperiments(
            experiments = listOf(
                testExperiment,
                testExperiment,
                testExperiment,
                testExperiment,
            ),
            onSelectedExperiment = {},
        )
    }
}
