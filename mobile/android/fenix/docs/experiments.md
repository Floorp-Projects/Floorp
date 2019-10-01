# Experiments

Fenix uses Mozilla's [Experiments service for experimentation](https://github.com/mozilla-mobile/android-components/blob/master/components/service/experiments/README.md).

## Experiment Naming

Naming must be unique and under 100 characters.

Name experiments in the format of ""fenix_{experiment}_{issue}"", such as "fenix_etp_5651". Multiple word experiment titles should be snake cased.

Branches should be descriptive of the control and treatment.

Example:
Experiment: "fenix_buttonColor_1"
Branches: "control_blue", "treatment_green"

## Applying Experiment Branches

The Fenix engineer should [check for if a user is in and experiment and then apply the branches of this experiment](https://github.com/mozilla-mobile/android-components/blob/master/components/service/experiments/README.md#checking-if-a-user-is-part-of-an-experiment).
See [no-op experiment example](https://github.com/mozilla-mobile/fenix/pull/4551) and [ETP example](https://github.com/mozilla-mobile/fenix/pull/5723).

## Experimenter

[Experimenter](https://experimenter.services.mozilla.com/) is the web application for managing user experiments for Mozilla. It is intended to store information related to the design and status of experiments, and can be used to communicate about the experiment with other involved teams.
Once the Fenix engineer has added the experiment branches to Fenix, they should update the experiment in Experimenter with the unique experiment name, unique branch names, and version code for experiment to take place.
Once required steps have been completed and checked off, clicking the "ready to ship" button will alert the data team they can set up the experiment.

## Data Review

As long as the experiments is using existing telemetry probes, it does not need additional data review. If the experiment requires adding new telemetry/metrics, a data review will additionally have to be filled out and completed in Experimenter.

## QA

QA should test experiments before we turn them on and should verify opting out of experiments works as well. More details on testing experiments [here](https://github.com/mozilla-mobile/android-components/tree/master/components/service/experiments#testing-experiments)

## Experiment Opt Out

Users can opt out of experiments at any time via Settings -> Data collection
