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
* `Toolbar Badge <toolbar_badge_schema_>`_
* `Update Action <update_action_schema_>`_
* `Whats New <whats_new_schema_>`_
* `Private Browsing Newtab Promo Message <pbnewtab_promo_schema_>`_

Together, they are combined into the `Messaging Experiments
<messaging_experiments_schema_>`_ via a `script <make_schemas_script_>`_. This
is the schema used for Nimbus experiments that target messaging features. All
incoming messaging experiments will be validated against these schemas.

To add a new message type to the Messaging Experiments schema:

1. Ensure the schema has an ``$id`` member. This allows for references (e.g.,
   ``{ "$ref": "#!/$defs/Foo" }``) to work in the bundled schema. See docs on
   `bundling JSON schemas <jsonschema_bundling_>`_ for more information.
2. Add the new schema to the list in `make-schemas.py <make_schemas_script_>`_.
3. Build the new schema by running:

  .. code-block:: shell

     cd browser/components/newtab/schemas
     ../../../../../../mach python make-schemas.py

4. Commit the results.

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
..  _toolbar_badge_schema: https://searchfox.org/mozilla-central/source/browser/components/newtab/content-src/asrouter/templates/OnboardingMessage/ToolbarBadgeMessage.schema.json
..  _update_action_schema: https://searchfox.org/mozilla-central/source/browser/components/newtab/content-src/asrouter/templates/OnboardingMessage/UpdateAction.schema.json
..  _whats_new_schema: https://searchfox.org/mozilla-central/source/browser/components/newtab/content-src/asrouter/templates/OnboardingMessage/WhatsNewMessage.schema.json
..  _pbnewtab_promo_schema: https://searchfox.org/mozilla-central/source/browser/components/newtab/content-src/asrouter/templates/PBNewtab/NewtabPromoMessage.schema.json
..  _messaging_experiments_schema: https://searchfox.org/mozilla-central/source/browser/components/newtab/schemas/MessagingExperiment.schema.json
..  _make_schemas_script: https://searchfox.org/mozilla-central/source/browser/components/newtab/content-src/asrouter/schemas/make-schemas.py
..  _jsonschema_bundling: https://json-schema.org/understanding-json-schema/structuring.html#bundling
