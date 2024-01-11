Messaging System Schemas
========================

Docs
----

More information about `Messaging System`__.

.. __: /browser/components/newtab/content-src/asrouter/docs

Messages
--------

There are JSON schemas for each type of message that the Firefox Messaging
System handles:

* `CFR URLBar Chiclet <cfr_urlbar_chiclet_schema_>`_
* `Extension Doorhanger <extension_doorhanger_schema_>`_
* `Infobar <infobar_schema_>`_
* `Spotlight <spotlight_schema_>`_
* `Toast Notification <toast_notification_schema_>`_
* `Toolbar Badge <toolbar_badge_schema_>`_
* `Update Action <update_action_schema_>`_
* `Whats New <whats_new_schema_>`_
* `Private Browsing Newtab Promo Message <pbnewtab_promo_schema_>`_

Together, they are combined into the `Messaging Experiments
<messaging_experiments_schema_>`_ via a `script <make_schemas_script_>`_. This
is the schema used for Nimbus experiments that target messaging features. All
incoming messaging experiments will be validated against this schema.

Schema Changes
--------------

To add a new message type to the Messaging Experiments schema:

1. Add your message template schema.

   Your message template schema only needs to define the following fields at a
   minimum:

   * ``template``: a string field that defines an identifier for your message.
     This must be either a ``const`` or ``enum`` field.

     For example, the ``template`` field of Spotlight looks like:

     .. code-block:: json

        { "type": "string", "const": "spotlight" }

   * ``content``: an object field that defines your per-message unique content.

   If your message requires ``targeting``, you must add a targeting field.

   If your message supports triggering, there is a definition you can reference
   the ``MessageTrigger`` `shared definition <Shared Definitions_>`_.

   The ``groups``, ``frequency``, and ``priority`` fields will automatically be
   inherited by your message.

2. Ensure the schema has an ``$id`` member. This allows for references (e.g.,
   ``{ "$ref": "#!/$defs/Foo" }``) to work in the bundled schema. See docs on
   `bundling JSON schemas <jsonschema_bundling_>`_ for more information.

3. Add the new schema to the list in `make-schemas.py <make_schemas_script_>`_.
4. Build the new schema by running:

   .. code-block:: shell

      cd browser/components/newtab/content-src/asrouter/schemas/
      ../../../../../../mach python make-schemas.py

5. Commit the results.

Likewise, if you are modifying a message schema you must rebuild the generated
schema:

.. code-block:: shell

   cd browser/components/newtab/content-src/asrouter/schemas/
   ../../../../../../mach python make-schemas.py

If you do not, the `Firefox MS Schemas CI job <make_schemas_check_>`_ will fail.

.. _run_make_schemas:

You can run this locally via:

.. code-block:: shell

   cd browser/components/newtab/content-src/asrouter/schemas/
   ../../../../../../mach xpcshell extract-test-corpus.js
   ../../../../../../mach python make-schemas.py --check

This test will re-generate the schema and compare it to
``MessagingExperiment.schema.json``. If there is a difference, it will fail.
The test will also validate the list of in-tree messages with the same schema
validator that Experimenter uses to ensure that our schemas are compatible with
Experimenter.

Shared Definitions
------------------

Some definitions are shared across multiple schemas. Instead of copying and
pasting the definitions between them and then having to manually keep them up to
date, we keep them in a common schema that contains these defintitions:
`FxMsCommon.schema.json <common_schema_>`_.  Any definition that will be re-used
across multiple schemas should be added to the common schema, which will have
its definitions bundled into the generated schema. All references to the common
schema will be rewritten in the generated schema.

The definitions listed in this file are:

* ``Message``, which defines the common fields present in each FxMS message;
* ``MessageTrigger``, which defines a method that may trigger the message to be
  presented to the user;
* ``localizableText``, for when either a string or a string ID (for translation
  purposes) can be used; and
* ``localizedText``, for when a string ID is required.

An example of using the ``localizableText`` definition in a message schema follows:

.. code-block:: json

   {
     "type": "object",
     "properties": {
       "message": {
         "$ref": "file:///FxMSCommon.schema.json#/$defs/localizableText"
         "description": "The message as a string or string ID"
       },
     }
   }


Schema Tests
------------

We have in-tree tests (`Test_CFRMessageProvider`_,
`Test_OnboardingMessageProvider`_, and `Test_PanelTestProvider`_), which
validate existing messages with the generated schema.

We also have compatibility tests for ensuring that our schemas work in
`Experimenter`_.  `Experimenter`_ uses a different JSON schema validation
library, which is reused in the `Firefox MS Schemas CI job
<make_schemas_check_>`_. This test validates a test corpus from
`CFRMessageProvider`_, `OnboardingMessageProvider`_, and `PanelTestProvider`_
with the same JSON schema validation library and configuration as Experimenter.

See how to run these tests :ref:`above <run_make_schemas_>`.


Triggers and actions
---------------------

.. toctree::
  :maxdepth: 2

  SpecialMessageActionSchemas/index
  TriggerActionSchemas/index

..  _cfr_urlbar_chiclet_schema: https://searchfox.org/mozilla-central/source/browser/components/newtab/content-src/asrouter/templates/CFR/templates/CFRUrlbarChiclet.schema.json
..  _extension_doorhanger_schema: https://searchfox.org/mozilla-central/source/browser/components/newtab/content-src/asrouter/templates/CFR/templates/ExtensionDoorhanger.schema.json
..  _infobar_schema: https://searchfox.org/mozilla-central/source/browser/components/newtab/content-src/asrouter/templates/CFR/templates/InfoBar.schema.json
..  _spotlight_schema: https://searchfox.org/mozilla-central/source/browser/components/newtab/content-src/asrouter/templates/OnboardingMessage/Spotlight.schema.json
..  _toast_notification_schema: https://searchfox.org/mozilla-central/source/browser/components/newtab/content-src/asrouter/templates/ToastNotification/ToastNotification.schema.json
..  _toolbar_badge_schema: https://searchfox.org/mozilla-central/source/browser/components/newtab/content-src/asrouter/templates/OnboardingMessage/ToolbarBadgeMessage.schema.json
..  _update_action_schema: https://searchfox.org/mozilla-central/source/browser/components/newtab/content-src/asrouter/templates/OnboardingMessage/UpdateAction.schema.json
..  _whats_new_schema: https://searchfox.org/mozilla-central/source/browser/components/newtab/content-src/asrouter/templates/OnboardingMessage/WhatsNewMessage.schema.json
..  _protections_panel_schema: https://searchfox.org/mozilla-central/source/browser/components/newtab/content-src/asrouter/templates/OnboardingMessage/ProtectionsPanelMessage.schema.json
..  _pbnewtab_promo_schema: https://searchfox.org/mozilla-central/source/browser/components/newtab/content-src/asrouter/templates/PBNewtab/NewtabPromoMessage.schema.json
..  _messaging_experiments_schema: https://searchfox.org/mozilla-central/source/browser/components/newtab/content-src/asrouter/schemas/MessagingExperiment.schema.json
..  _common_schema: https://searchfox.org/mozilla-central/source/browser/components/newtab/content-src/asrouter/schemas/FxMSCommon.schema.json

..  _make_schemas_script: https://searchfox.org/mozilla-central/source/browser/components/newtab/content-src/asrouter/schemas/make-schemas.py
..  _jsonschema_bundling: https://json-schema.org/understanding-json-schema/structuring.html#bundling
..  _make_schemas_check: https://searchfox.org/mozilla-central/source/taskcluster/ci/source-test/python.yml#425-438

..  _Experimenter: https://experimenter.info

..  _CFRMessageProvider: https://searchfox.org/mozilla-central/source/browser/components/newtab/lib/CFRMessageProvider.jsm
..  _PanelTestProvider: https://searchfox.org/mozilla-central/source/browser/components/newtab/lib/PanelTestProvider.jsm
..  _OnboardingMessageProvider: https://searchfox.org/mozilla-central/source/browser/components/newtab/lib/OnboardingMessageProvider.jsm
..  _Test_CFRMessageProvider: https://searchfox.org/mozilla-central/source/browser/components/newtab/test/xpcshell/test_CFMessageProvider.js
..  _Test_OnboardingMessageProvider: https://searchfox.org/mozilla-central/source/browser/components/newtab/test/xpcshell/test_OnboardingMessageProvider.js
..  _Test_PanelTestProvider: https://searchfox.org/mozilla-central/source/browser/components/newtab/test/xpcshell/test_PanelTestProvider.js
