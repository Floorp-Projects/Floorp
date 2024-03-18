# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import pytest


@pytest.mark.parametrize("load_branches", [("branch")], indirect=True)
def test_survey_navigates_correctly(
    setup_experiment, gradlewbuild, load_branches, check_ping_for_experiment
):
    setup_experiment(load_branches)
    gradlewbuild.test("SurveyExperimentIntegrationTest#checkSurveyNavigatesCorrectly")
    assert check_ping_for_experiment(reason="enrollment", branch=load_branches[0])


@pytest.mark.parametrize("load_branches", [("branch")], indirect=True)
def test_survey_no_thanks_navigates_correctly(
    setup_experiment, gradlewbuild, load_branches, check_ping_for_experiment
):
    setup_experiment(load_branches)
    gradlewbuild.test(
        "SurveyExperimentIntegrationTest#checkSurveyNoThanksNavigatesCorrectly"
    )
    assert check_ping_for_experiment(reason="enrollment", branch=load_branches[0])


@pytest.mark.parametrize("load_branches", [("branch")], indirect=True)
def test_homescreen_survey_dismisses_correctly(
    setup_experiment, gradlewbuild, load_branches, check_ping_for_experiment
):
    setup_experiment(load_branches)
    gradlewbuild.test(
        "SurveyExperimentIntegrationTest#checkHomescreenSurveyDismissesCorrectly"
    )
    assert check_ping_for_experiment(reason="enrollment", branch=load_branches[0])


@pytest.mark.parametrize("load_branches", [("branch")], indirect=True)
def test_survey_landscape_looks_correct(
    setup_experiment, gradlewbuild, load_branches, check_ping_for_experiment
):
    setup_experiment(load_branches)
    gradlewbuild.test(
        "SurveyExperimentIntegrationTest#checkSurveyLandscapeLooksCorrect"
    )
    assert check_ping_for_experiment(reason="enrollment", branch=load_branches[0])
