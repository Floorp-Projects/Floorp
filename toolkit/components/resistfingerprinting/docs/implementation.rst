=========================
Implementation
=========================

Checking if an object should resist fingerprinting is ideally done by referencing the `Document's ShouldResistFingerprinting <https://searchfox.org/mozilla-central/search?q=symbol:_ZNK7mozilla3dom8Document26ShouldResistFingerprintingENS_9RFPTargetE&redirect=false>`_ method.  This is both fast and correct.  In certain other situations, you may need to call some of the ``nsContentUtils::ShouldResistFingerprinting`` functions.  When doing so, you should avoid calling either of the functions marked *dangerous*.

As you can see in the callgraph below, directly calling a *dangerous* function will skip some of the checks that occur further up-stack.

.. mermaid::

	graph TD
	  SRFP["ShouldResistFingerprinting()"]
	  click SRFP href "https://searchfox.org/mozilla-central/search?q=symbol:_ZN14nsContentUtils26ShouldResistFingerprintingEN7mozilla9RFPTargetE&redirect=false"

	  SRGP_GO["ShouldResistFingerprinting(nsIGlobalObject* aGlobalObject"]
	  click SRGP_GO href "https://searchfox.org/mozilla-central/search?q=symbol:_ZN14nsContentUtils26ShouldResistFingerprintingEP15nsIGlobalObjectN7mozilla9RFPTargetE&redirect=false"

	  GO_SRFP["nsIGlobalObject*::ShouldResistFingerprinting()"]
	  click GO_SRFP href "https://searchfox.org/mozilla-central/search?q=symbol:_ZNK15nsIGlobalObject26ShouldResistFingerprintingEN7mozilla9RFPTargetE&redirect=false"

	  Doc_SRFP["Document::ShouldResistFingerprinting()<br />System Principal Check"]
	  click Doc_SRFP href "https://searchfox.org/mozilla-central/search?q=symbol:_ZNK7mozilla3dom8Document26ShouldResistFingerprintingENS_9RFPTargetE&redirect=false"

	  SRFP_char["ShouldResistFingerprinting(const char*)"]
	  click SRFP_char href "https://searchfox.org/mozilla-central/search?q=symbol:_ZN14nsContentUtils26ShouldResistFingerprintingEPKcN7mozilla9RFPTargetE&redirect=false"

	  SRFP_callertype_go["ShouldResistFingerprinting(CallerType, nsIGlobalObject*)<br />System Principal Check"]
	  click SRFP_callertype_go href "https://searchfox.org/mozilla-central/search?q=symbol:_ZN14nsContentUtils26ShouldResistFingerprintingEN7mozilla3dom10CallerTypeEP15nsIGlobalObjectNS0_9RFPTargetE&redirect=false"

	  SRFP_docshell["ShouldResistFingerprinting(nsIDocShell*)"]
	  click SRFP_docshell href "https://searchfox.org/mozilla-central/search?q=symbol:_ZN14nsContentUtils26ShouldResistFingerprintingEP11nsIDocShellN7mozilla9RFPTargetE&redirect=false"

	  SRFP_channel["ShouldResistFingerprinting(nsIChannel*)<br />ETPSaysShouldNotResistFingerprinting Check<br />CookieJarSettingsSaysShouldResistFingerprinting Check"]
	  click SRFP_channel href "https://searchfox.org/mozilla-central/search?q=symbol:_ZN14nsContentUtils26ShouldResistFingerprintingEP10nsIChannelN7mozilla9RFPTargetE&redirect=false"

	  SRFP_uri["ShouldResistFingerprinting_dangerous(nsIURI*, OriginAttributes)<br />PBM Check<br />Scheme (inc WebExtension) Check<br />About Page Check<br />URI & Partition Key Exempt Check"]
	  click SRFP_uri href "https://searchfox.org/mozilla-central/search?q=symbol:_ZN14nsContentUtils36ShouldResistFingerprinting_dangerousEP6nsIURIRKN7mozilla16OriginAttributesEPKcNS2_9RFPTargetE&redirect=false"

	  SRFP_principal["ShouldResistFingerprinting_dangerous(nsIPrincipal*)<br />System Principal Check<br />PBM Check<br />Scheme Check<br />About Page Check<br />Web Extension Principal Check<br />URI & Partition Key Exempt Check"]
	  click SRFP_principal href "https://searchfox.org/mozilla-central/search?q=symbol:_ZN14nsContentUtils36ShouldResistFingerprinting_dangerousEP12nsIPrincipalPKcN7mozilla9RFPTargetE&redirect=false"




	  SRFP_principal --> |null| SRFP_char

	  SRFP_uri --> |null| SRFP_char

	  SRFP_channel -->|null| SRFP_char
	  SRFP_channel --> |Document Load| SRFP_uri
	  SRFP_channel --> |Subresource Load| SRFP_principal

	  SRFP_docshell -->|null| SRFP_char
	  SRFP_docshell --> Doc_SRFP

	  SRFP_callertype_go --> SRGP_GO
	  SRFP_char --> SRFP
	  SRGP_GO -->|null| SRFP_char
	  SRGP_GO --> GO_SRFP

	  GO_SRFP --> |innerWindow, outerWindow| Doc_SRFP
	  Doc_SRFP --> SRFP_channel


Exemptions and Targets
~~~~~~~~~~~~~~~~~~~~~~

Fingerprinting Resistance takes into account many things to determine if we should alter behavior:

* Whether we are the System Principal
* Whether we are a Web Extension
* Whether Fingerprinting Resistance is applied to all browsing modes or only Private Browsing Mode
* Whether the specific site you are visiting has been granted an exemption (taking into account the framing page)
* Whether the specific **activity** is granted an exemption

All callsites for ``ShouldResistFingerprinting`` take a (currently) optional ``RFPTarget`` value, which defaults to ``Unknown``.  While arguments such as ``Document`` or ``nsIChannel`` provide context for the first four exemptions above, the Target provides context for the final one.  A Target is a Web API or an activity - such as a Pointer Event, the Screen Orientation, or plugging a gamepad into your computer (and therefore producing a `gamepadconnected event <https://www.w3.org/TR/gamepad/#event-gamepadconnected>`_).  Most Targets correlate strongly to a specific Web API, but not all do: for example whether or not to automatically reject Canvas extraction requests from third parties is a separate Target from prompting to reject canvas extraction.

In some situations we may *not* alter our behavior for a certain activity - this could be based on the fingerprinting resistance mode you are using, or per-site overrides to correct breakage.  Targets are defined `RFPTargets.inc <https://searchfox.org/mozilla-central/source/toolkit/components/resistfingerprinting/RFPTargets.inc>`_.
