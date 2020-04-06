Suitabilities
=============

When Normandy's core passes a recipe to an action, it also passes a
*suitability*, which is the result of evaluating a recipe's filters,
compatibility, signatures, and other checks. This gives actions more
information to make decisions with, which is especially important for
experiments.

Temporary errors
----------------
Some of the suitabilities below represent *temporary errors*. These could be
caused by infrastructure problems that prevent the recipe from working
correctly, or are otherwise not the fault of the recipe itself. These
suitabilities should not immediately cause a change in state. If the problem
persists, then eventually it should be considered permanent and state should
be changed.

In the case of a permanent failure, action such as unenrollment should happen
immediately. For temporary failures, that action should be delayed until the
failure persists longer than some threshold. It is up to individual actions
to track and manage this transition.

List of Suitabilities
---------------------

``FILTER_MATCH``
~~~~~~~~~~~~~~~~
All checks have passed, and the recipe is suitable to execute in this client.
Experiments and Rollouts should enroll or update. Heartbeats should be shown
to the user, etc.

``SIGNATURE_ERROR``
~~~~~~~~~~~~~~~~~~~
The recipe's signature is not valid. If any action is taken this recipe
should be treated with extreme suspicion.

This should be considered a temporary error, because it may be related to
server errors, local clocks, or other temporary problems.

``CAPABILITES_MISMATCH``
~~~~~~~~~~~~~~~~~~~~~~~~
The recipe requires capabilities that this recipe runner does not have. Use
caution when interacting with this recipe, as it may not match the expected
schema.

This should be considered a permanent error, because it is the result of a
choice made on the server.

``FILTER_MISMATCH``
~~~~~~~~~~~~~~~~~~~
This client does not match the recipe's filter, but it is otherwise a
suitable recipe.

This should be considered a permanent error, since the filter explicitly does
not match the client.

``FILTER_ERROR``
~~~~~~~~~~~~~~~~
There was an error while evaluating the filter. It is unknown if this client
matches this filter. This may be temporary, due to network errors, or
permanent due to syntax errors.

This should be considered a temporary error, because it may be the result of
infrastructure, such as `Classify Client <./services.html#classify-client>`_,
temporarily failing.

``ARGUMENTS_INVALID``
~~~~~~~~~~~~~~~~~~~~~
The arguments of the recipe do not match the expected schema for the named
action.

This should be considered a permanent error, since the arguments are generally validated by the server. This likely represents an unrecogonized compatibility error.

