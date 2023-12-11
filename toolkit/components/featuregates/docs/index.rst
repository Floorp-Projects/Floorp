.. _components/featuregates:

=============
Feature Gates
=============

A feature gate is a high level tool to turn features on and off. It provides
metadata about features, a simple, opinionated API, and avoid many potential
pitfalls of other systems, such as using preferences directly. It is designed
to be compatible with tools that want to know and affect the state of
features in Firefox over time and in the wild.

Feature Definitions
===================

All features must have a definition, specified in
``toolkit/components/featuregates/Features.toml``. These definitions include
data such as references to title and description strings (to be shown to users),
and bug numbers (to track the development of the feature over time). Here is an
example feature definition with an id of ``demo-feature``:

.. code-block:: toml

   [demo-feature]
   title = "experimental-features-demo-feature"
   description = "experimental-features-demo-feature-description"
   restart-required = false
   bug-numbers = [1479127]
   type = "boolean"
   is-public = {default = false, nightly = true}
   default-value = {default = false, nightly = true}

The references defined in the `title` and `description` values point to strings
stored in ``toolkit/locales/en-US/toolkit/featuregates/features.ftl``. The `title`
string should specify the user-facing string as the `label` attribute.

.. _targeted value:

Targeted values
---------------

Several fields can take a value that indicates it varies by channel and OS.
These are known as *targeted values*. The simplest computed value is to
simply provide the value:

.. code-block:: toml

   default-value = true

A more interesting example is to make a feature default to true on Nightly,
but be disabled otherwise. That would look like this:

.. code-block:: toml

   default-value = {default = false, nightly = true}

Values can depend on multiple conditions. For example, to enable a feature
only on Nightly running on Windows:

.. code-block:: toml

   default-value = {default = false, "nightly,win" = true}

Multiple sets of conditions can be specified, however use caution here: if
multiple sets could match (except ``default``), the set chosen is undefined.
An example of safely using multiple conditions:

.. code-block:: toml

   default-value = {default = false, nightly = true, "beta,win" = true}

The ``default`` condition is required. It is used as a fallback in case no
more-specific case matches. The conditions allowed are

* ``default``
* ``release``
* ``beta``
* ``dev-edition``
* ``nightly``
* ``esr``
* ``win``
* ``mac``
* ``linux``
* ``android``

Fields
------

title
    Required. The string ID of the human readable name for the feature, meant to be shown to
    users. Should fit onto a single line. The actual string should be defined in
    ``toolkit/locales/en-US/toolkit/featuregates/features.ftl`` with the user-facing value
    defined as the `label` attribute of the string.

description
    Required. The string ID of the human readable description for the feature, meant to be shown to
    users. Should be at most a paragraph. The actual string should be defined in
    ``toolkit/locales/en-US/toolkit/featuregates/features.ftl``.

description-links
    Optional. A dictionary of key-value pairs that are referenced in the description. The key
    name must appear in the description localization text as
    <a data-l10n-name="key-name">. For example in Features.toml:

.. code-block:: toml

   [demo-feature]
   title = "experimental-features-demo-feature"
   description = "experimental-features-demo-feature-description"
   description-links = {exampleCom = "https://example.com", exampleOrg = "https://example.org"}
   restart-required = false
   bug-numbers = [1479127]
   type = "boolean"
   is-public = {default = false, nightly = true}
   default-value = {default = false, nightly = true}

and in features.ftl:

.. code-block:: fluent

   experimental-features-demo-feature =
       .label = Example Demo Feature
   experimental-features-demo-feature-description = Example demo feature that can point to <a data-l10n-name="exampleCom">.com</a> links and <a data-l10n-name="exampleOrg">.org</a> links.

bug-numbers
    Required. A list of bug numbers related to this feature. This should
    likely be the metabug for the the feature, but any related bugs can be
    included. At least one bug is required.

restart-required
    Required. If this feature requires a the browser to be restarted for changes
    to take effect, this field should be ``true``. Otherwise, the field should
    be ``false``. Features should aspire to not require restarts and react to
    changes to the preference dynamically.

type
    Required. The type of value this feature relates to. The only legal value is
    ``boolean``, but more may be added in the future.

preference
    Optional. The preference used to track the feature. If a preference is not
    provided, one will be automatically generated based on the feature ID. It is
    not recommended to specify a preference directly, except to integrate with
    older code. In the future, alternate storage mechanisms may be used if a
    preference is not supplied.

default-value
    Optional. This is a `targeted value`_ describing
    the value for the feature if no other changes have been made, such as in
    a fresh profile. If not provided, the default for a boolean type feature
    gate will be ``false`` for all profiles.

is-public
    Optional. This is a `targeted value`_ describing
    on which branches this feature should be exposed to users. When a feature
    is made public, it may show up in a future UI that allows users to opt-in
    to experimental features. This is not related to ``about:preferences`` or
    ``about:config``. If not provided, the default is to make a feature
    private for all channels.


Feature Gate API
================

..
    (comment) The below lists should be kept in sync with the contents of the
    classes they are documenting. An explicit list is used so that the
    methods can be put in a particular order.

.. js:autoclass:: FeatureGate
   :members: addObserver, removeObserver, isEnabled, fromId

.. js:autoclass:: FeatureGateImplementation
   :members: id, title, description, type, bugNumbers, isPublic, defaultValue, restartRequired, preference, addObserver, removeObserver, removeAllObservers, getValue, isEnabled

   Feature implementors should use the methods :func:`fromId`,
   :func:`addListener`, :func:`removeListener` and
   :func:`removeAllListeners`. Additionally, metadata is available for UI and
   analysis.
