def test_survey_navigates_correctly(setup_experiment, gradlewbuild):
    setup_experiment()
    gradlewbuild.test("SurveyExperimentIntegrationTest#checkSurveyNavigatesCorrectly")
