# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import pytest


@pytest.mark.messaging_survey
def test_survey_navigates_correctly(setup_experiment, gradlewbuild):
    setup_experiment()
    gradlewbuild.test("SurveyExperimentIntegrationTest#checkSurveyNavigatesCorrectly")


@pytest.mark.messaging_survey
def test_survey_no_thanks_navigates_correctly(setup_experiment, gradlewbuild):
    setup_experiment()
    gradlewbuild.test(
        "SurveyExperimentIntegrationTest#checkSurveyNoThanksNavigatesCorrectly"
    )


@pytest.mark.messaging_homescreen
def test_homescreen_survey_dismisses_correctly(setup_experiment, gradlewbuild):
    setup_experiment()
    gradlewbuild.test(
        "SurveyExperimentIntegrationTest#checkHomescreenSurveyDismissesCorrectly"
    )


@pytest.mark.messaging_survey
def test_survey_landscape_looks_correct(setup_experiment, gradlewbuild):
    setup_experiment()
    gradlewbuild.test(
        "SurveyExperimentIntegrationTest#checkSurveyLandscapeLooksCorrect"
    )
