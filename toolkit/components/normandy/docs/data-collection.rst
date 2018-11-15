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

.. data:: RUNNER_INVALID_SIGNATURE

   Normandy failed to verify the signature of the fetched recipes.

.. data:: RUNNER_NETWORK_ERROR

   There was a network-related error while fetching recipes.

.. data:: RUNNER_SERVER_ERROR

   The data returned by the server when fetching the recipe is invalid in some
   way.

.. data:: RUNNER_SUCCESS

   The operation completed successfully. Individual failures with actions and
   recipes may have been reported separately.

Action Status
^^^^^^^^^^^^^
For each action available from the Normandy service, one of the
following statuses is reported under the key
``normandy/action/<action name>``:

.. data:: ACTION_NETWORK_ERROR

   There was a network-related error while fetching actions

.. data:: ACTION_PRE_EXECUTION_ERROR

   There was an error while running the pre-execution hook for the action.

.. data:: ACTION_POST_EXECUTION_ERROR

   There was an error while running the post-execution hook for the action.

.. data:: ACTION_SERVER_ERROR

   The data returned by the server when fetching the action is invalid in some
   way.

.. data:: ACTION_SUCCESS

   The operation completed successfully. Individual failures with recipes may
   be reported separately.

Recipe Status
^^^^^^^^^^^^^
For each recipe that is fetched and executed, one of the following statuses is
reported under the key ``normandy/recipe/<recipe id>``:

.. data:: RECIPE_ACTION_DISABLED

   The action for this recipe failed in some way and was disabled, so the recipe
   could not be executed.

.. data:: RECIPE_EXECUTION_ERROR

   An error occurred while executing the recipe.

.. data:: RECIPE_INVALID_ACTION

   The action specified by the recipe was invalid and it could not be executed.

.. data:: RECIPE_SUCCESS

   The recipe was executed successfully.


Enrollment
-----------
Normandy records enrollment and unenrollment of users into studies, and
records that data using `Telemetry Events`_. All data is stored in the
``normandy`` category.

.. _Telemetry Events: https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/telemetry/collection/events.html

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
         * ``"user-preference-changed"``: The study preference was
           changed on the user branch. This could mean the user
           changed the preference, or that some other mechanism set a
           non-default value for the preference.
         * ``"user-preference-changed-sideload"``: The study
           preference was changed on the user branch while Normandy was
           inactive. This could mean that the value was manually
           changed in a profile while Firefox was not running.
         * ``"unknown"``: A reason was not specificied. This should be
           considered a bug.

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
