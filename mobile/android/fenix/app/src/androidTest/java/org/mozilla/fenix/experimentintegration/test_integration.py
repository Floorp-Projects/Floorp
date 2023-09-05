import pytest


@pytest.mark.parametrize("load_branches", [("branch")], indirect=True)
def test_survey_navigates_correctly(setup_experiment, gradlewbuild, load_branches):
    setup_experiment(load_branches)
    gradlewbuild.test("SurveyExperimentIntegrationTest#checkSurveyNavigatesCorrectly")
