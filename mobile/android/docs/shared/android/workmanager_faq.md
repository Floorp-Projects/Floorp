# WorkManager FAQ
The [WorkManager documentation](https://developer.android.com/reference/androidx/work/WorkManager) does not clearly explain all of the different edge cases that may be encountered. After some testing with this API, the edge cases that were tested are documented here for future reference:

- When the device is turned off and turned on before the one-time scheduled job, does the job complete?
  - Yes
- When the device is turned off and misses the one-time scheduled job time, does the job complete (i.e. “catches up”)?
  - No
- What happens when exceptions are thrown inside a Worker?
  - The job results in a `Failure`. By default, **this exception is not caught by Sentry**.
- If a job crashes, does it crash the foregrounded app?
  - No
