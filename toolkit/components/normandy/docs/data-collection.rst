Data Collection
===============
This document describes the types of data that Normandy collects.

Uptake
------
Normandy monitors the execution of recipes and reports to
:ref:`telemetry` the amount of successful and failed runs. This data
is reported using :ref:`telemetry/collection/uptake` under the
``normandy`` namespace.

Runner Status
^^^^^^^^^^^^^
Once per-fetch and execution of recipes, one of the following statuses is
reported under the key ``normandy/runner``:

.. data:: RUNNER_SUCCESS

   :Telemetry value: success

   The operation completed successfully. Individual failures with actions and
   recipes may have been reported separately.

.. data:: RUNNER_SERVER_ERROR

   :Telemetry value: server_error

   The data returned by the server when fetching the recipe is invalid in some
   way.

Action Status
^^^^^^^^^^^^^
For each action available from the Normandy service, one of the
following statuses is reported under the key
``normandy/action/<action name>``:

.. data:: ACTION_NETWORK_ERROR

   :Telemetry value: network_error

   There was a network-related error while fetching actions

.. data:: ACTION_PRE_EXECUTION_ERROR

   :Telemetry value: custom_1_error

   There was an error while running the pre-execution hook for the action.

.. data:: ACTION_POST_EXECUTION_ERROR

   :Telemetry value: custom_2_error

   There was an error while running the post-execution hook for the action.

.. data:: ACTION_SERVER_ERROR

   :Telemetry value: server_error

   The data returned by the server when fetching the action is invalid in some
   way.

.. data:: ACTION_SUCCESS

   :Telemetry value: success

   The operation completed successfully. Individual failures with recipes may
   be reported separately.

Recipe Status
^^^^^^^^^^^^^
For each recipe that is fetched and executed, one of the following statuses is
reported under the key ``normandy/recipe/<recipe id>``:

.. data:: RECIPE_ACTION_DISABLED

   :Telemetry value: custom_1_error

   The action for this recipe failed in some way and was disabled, so the recipe
   could not be executed.

.. data:: RECIPE_DIDNT_MATCH_FILTER

   :Telemetry value: backoff

   The recipe included a Jexl filter that the client did not match, so
   the recipe was not executed.

.. data:: RECIPE_EXECUTION_ERROR

   :Telemetry value: apply_error

   An error occurred while executing the recipe.

.. data:: RECIPE_FILTER_BROKEN

   :Telemetry value: content_error

   An error occurred while evaluating the Jexl filter for this
   recipe. Sometimes this represents a bug in the Jexl filter
   parser/evaluator, such as in `bug 1477156
   <https://bugzilla.mozilla.org/show_bug.cgi?id=1477156>`_, or it can
   represent an error fetching some data that a Jexl recipe needs such
   as `bug 1447804
   <https://bugzilla.mozilla.org/show_bug.cgi?id=1447804>`_.

.. data:: RECIPE_INVALID_ACTION

   :Telemetry value: download_error

   The action specified by the recipe was invalid and it could not be executed.

.. data:: RECIPE_SUCCESS

   :Telemetry value: success

   The recipe was executed successfully.

.. data:: RECIPE_SIGNATURE_INVALID

   :Telemetry value: signature_error

   Normandy failed to verify the signature of the recipe.


Additionally, Normandy reports a :ref:`keyed scalar <Scalars>` to measure recipe
freshness. This scalar is called ``normandy.recipe_freshness``, and it
corresponds to the ``last_modified`` date of each recipe (using its ID
as the key), reported as seconds since 1970 in UTC.


Enrollment
-----------
Normandy records enrollment and unenrollment of users into studies, and
records that data using :ref:`Telemetry Events <eventtelemetry>`. All data is stored in the
``normandy`` category.


Preference Studies
^^^^^^^^^^^^^^^^^^
Enrollment
   method
      The string ``"enroll"``
   object
      The string ``"preference_study"``
   value
      The name of the study (``recipe.arguments.slug``).
   extra
      branch
         The name of the branch the user was assigned to (example:
         ``"control"`` or ``"experiment"``).
      experimentType
         The type of preference experiment. Currently this can take
         values "exp" and "exp-highpop", the latter being for
         experiments targeting large numbers of users.

Enrollment Failed
   method
      The string ``"enrollFailed"``
   object
      The string ``"preference_study"``
   value
      The slug of the study (``recipe.arguments.slug``)
   extra
      reason
         The reason for unenrollment. Possible values are:

         * ``"invalid-branch"``: The recipe specifies an invalid preference
           branch (not to be confused with the experiment branch). Valid values
           are "default" and "user".
      preferenceBranch
         If the reason was ``"invalid-branch"``, the branch that was
         specified, truncated to 80 characters.

Unenrollment
   method
      The string ``"unenroll"``.
   object
      The string ``"preference_study"``.
   value
      The name of the study (``recipe.arguments.slug``).
   extra
      didResetValue
         The string ``"true"`` if the preference was set back to its
         original value, ``"false"`` if it was left as its current
         value. This can happen when, for example, the user changes a
         preference that was involved in a user-branch study.
      reason
         The reason for unenrollment. Possible values are:

         * ``"recipe-not-seen"``: The recipe was no longer
           applicable to this client This can be because the recipe
           was disabled, or the user no longer matches the recipe's
           filter.
         * ``"unknown"``: A reason was not specified. This should be
           considered a bug.

Unenroll Failed
   method
      The string ``"unenrollFailed"``.
   object
      The string ``"preference_study"``.
   value
      The name of the study (``recipe.arguments.slug``)
   extra
      reason
         A code describing the reason the unenroll failed. Possible values are:

         * ``"does-not-exist"``: The system attempted to unenroll a study that
           does not exist. This is a bug.
         * ``"already-unenrolled"``: The system attempted to unenroll a study
           that has already been unenrolled. This is likely a bug.
      caller
         On Nightly builds only, a string identifying the source of the requested stop.
      originalReason
         The code that would had been used for the unenrollment, had it not failed.

Experimental Preference Changed
   method
      The string ``"expPrefChanged"``
   object
      The string ``"preference_study"``.
   value
      The name of the study (``recipe.arguments.slug``)
   extra
      preferenceName
         The name of the preference that changed. Note that the value of the
         preference (old or new) is not given.
      reason
         A code describing the reason that Normandy detected the preference
         change. Possible values are:

         * ``"atEnroll"``: The preferences already had user value when the
           experiment started.
         * ``"sideload"``: A preference was changed while Normandy's observers
           weren't active, likely while the browser was shut down.
         * ``"observer"``: The preference was observed to change by Normandy at
           runtime.

Preference Rollouts
^^^^^^^^^^^^^^^^^^^
Enrollment
   Sent when a user first enrolls in a rollout.

   method
      The string ``"enroll"``
   object
      The string ``"preference_rollout"``
   value
      The slug of the rollout (``recipe.arguments.slug``)

Enroll Failed
   Sent when a user attempts to enroll in a rollout, but the enrollment process fails.

   method
      The string ``"enrollFailed"``
   object
      The string ``"preference_rollout"``
   value
      The slug of the rollout (``recipe.arguments.slug``)
   extra
      reason
         A code describing the reason the unenroll failed. Possible values are:

         * ``"invalid type"``: The preferences specified in the rollout do not
           match the preferences built in to the browser. The represents a
           misconfiguration of the preferences in the recipe on the server.
         * ``"would-be-no-op"``: All of the preference specified in the rollout
           already have the given values. This represents an error in targeting
           on the server.
         * ``"conflict"``: At least one of the preferences specified in the
           rollout is already managed by another active rollout.
      preference
         For ``reason="invalid type"``, the first preference that was invalid.
         For ``reason="conflict"``, the first preference that is conflicting.

Update
   Sent when the preferences specified in the recipe have changed, and the
   client updates the preferences of the browser to match.

   method
      The string ``"update"``
   object
      The string ``"preference_rollout"``
   value
      The slug of the rollout (``recipe.arguments.slug``)
   extra
      previousState
         The state the rollout was in before this update (such as ``"active"`` or ``"graduated"``).

Graduation
   Sent when Normandy determines that further intervention is no longer
   needed for this rollout. After this point, Normandy will stop making
   changes to the browser for this rollout, unless the rollout recipe changes
   to specify preferences different than the built-in.

   method
      The string ``"graduate"``
   object
      The string ``"preference_rollout"``
   value
      The slug of the rollout (``recipe.arguments.slug``)
   extra
      reason
         A code describing the reason for the graduation. Possible values are:

         * ``"all-prefs-match"``: All preferences specified in the rollout now
           have built-in values that match the rollouts values.
           ``"in-graduation-set"``: The browser has changed versions (usually
           updated) to one that specifies this rollout no longer applies and
           should be graduated regardless of the built-in preference values.
           This behavior is controlled by the constant
           ``PreferenceRollouts.GRADUATION_SET``.

Add-on Studies
^^^^^^^^^^^^^^
Enrollment
   method
      The string ``"enroll"``
   object
      The string ``"addon_study"``
   value
      The name of the study (``recipe.arguments.name``).
   extra
      addonId
         The add-on's ID (example: ``"feature-study@shield.mozilla.com"``).
      addonVersion
         The add-on's version (example: ``"1.2.3"``).

Enroll Failure
   method
      The string ``"enrollFailed"``
   object
      The string ``"addon_study"``
   value
      The name of the study (``recipe.arguments.name``).
   reason
      A string containing the filename and line number of the code
      that failed, and the name of the error thrown. This information
      is purposely limited to avoid leaking personally identifiable
      information. This should be considered a bug.

Update
   method
      The string ``"update"``,
   object
      The string ``"addon_study"``,
   value
      The name of the study (``recipe.arguments.name``).
   extra
      addonId
         The add-on's ID (example: ``"feature-study@shield.mozilla.com"``).
      addonVersion
         The add-on's version (example: ``"1.2.3"``).

Update Failure
   method
      The string ``"updateFailed"``
   object
      The string ``"addon_study"``
   value
      The name of the study (``recipe.arguments.name``).
   extra
      reason
         A string containing the filename and line number of the code
         that failed, and the name of the error thrown. This information
         is purposely limited to avoid leaking personally identifiable
         information. This should be considered a bug.

Unenrollment
   method
      The string ``"unenroll"``.
   object
      The string ``"addon_study"``.
   value
      The name of the study (``recipe.arguments.name``).
   extra
      addonId
         The add-on's ID (example: ``"feature-study@shield.mozilla.com"``).
      addonVersion
         The add-on's version (example: ``"1.2.3"``).
      reason
         The reason for unenrollment. Possible values are:

         * ``"install-failure"``: The add-on failed to install.
         * ``"individual-opt-out"``: The user opted-out of this
           particular study.
         * ``"general-opt-out"``: The user opted-out of studies in
           general.
         * ``"recipe-not-seen"``: The recipe was no longer applicable
           to this client. This can be because the recipe was
           disabled, or the user no longer matches the recipe's
           filter.
         * ``"uninstalled"``: The study's add-on as uninstalled by some
           mechanism. For example, this could be a user action or the
           add-on self-uninstalling.
         * ``"uninstalled-sideload"``: The study's add-on was
           uninstalled while Normandy was inactive. This could be that
           the add-on is no longer compatible, or was manually removed
           from a profile.
         * ``"unknown"``: A reason was not specified. This should be
           considered a bug.
