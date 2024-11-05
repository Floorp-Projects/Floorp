/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const kTestDataTransferType = "x-moz-datatransfer-test";
const kTestDataTransferData = "Dragged Test Data";

// Common base of DragSourceChildContext and DragTargetChildContext
export class DragChildContextBase {
  // The name of the subtype of this object.
  subtypeName = "";

  // Map of counts of events (indexed by event type) that are expected before the next checkExpected
  // NB: A second expected array is maintained on the document object.  This is because the source and
  // target of the drag may be in the same document, in which case source-document events
  // will obviously appear on the target-document and vice-versa.
  expected = {};

  // Array of all of the relevantEvents received on dragElement.
  events = [];

  // (Document) event handler, which is the method with 'this' bound.
  eventHandler = null;
  documentEventHandler = null;

  // Array of all of the relevantEvents received on the document.
  // We expect this to be the same list as the list of events that
  // dragElement should receive, unless the other element involved
  // in the drag is in the same document.  @see expected.
  documentEvents = [];

  // The window
  dragWindow = null;

  // The element being dragged from or to.  Set as parameter to initialize.
  dragElement = null;

  // Position of element in client coords.
  clientPos = null;

  // Position of element in screen coords
  screenPos = null;

  // Was there a drag session before the test started?
  alreadyHadSession = false;

  // Array of monitored event types.  Set as parameter to initialize.
  relevantEvents = [];

  // Label added to diagnostic output to identify the specific drag.
  // Set as parameter to initialize.
  contextLabel = null;

  // Should an exception be thrown when an incorrect event is received.
  // Useful for debugging.  Set as parameter to initialize.
  throwOnExtraMessage = false;

  // Should events other than dragstart and drop have access to the
  // dataTransfer?  Set as parameter to initialize.
  expectProtectedDataTransferAccess = false;

  window = null;

  dragService = null;

  expect(aEvType) {
    this.expected[aEvType] += 1;
    this.dragWindow.document.expected[aEvType] += 1;
  }

  async checkExpected() {
    for (let ev of this.relevantEvents) {
      this.is(
        this.events[ev].length,
        this.expected[ev],
        `\telement received proper number of ${ev} events.`
      );
      this.is(
        this.documentEvents[ev].length,
        this.dragWindow.document.expected[ev],
        `\tdocument received proper number of ${ev} events.`
      );
    }
  }

  checkHasDrag(aShouldHaveDrag = true) {
    this.info(
      `${this.subtypeName} had pre-existing drag: ${this.alreadyHadSession}`
    );
    this.ok(
      !!this.dragService.getCurrentSession(this.dragWindow) ==
        aShouldHaveDrag || this.alreadyHadSession,
      `Has ${!aShouldHaveDrag ? "no " : ""}drag session`
    );
  }

  checkSessionHasAction() {
    this.checkHasDrag();
    if (this.alreadyHadSession) {
      return;
    }
    this.ok(
      this.dragService.getCurrentSession(this.dragWindow).dragAction !==
        Ci.nsIDragService.DRAGDROP_ACTION_NONE,
      "Drag session has valid action"
    );
  }

  // Adapted from EventUtils
  nodeIsFlattenedTreeDescendantOf(aPossibleDescendant, aPossibleAncestor) {
    do {
      if (aPossibleDescendant == aPossibleAncestor) {
        return true;
      }
      aPossibleDescendant = aPossibleDescendant.flattenedTreeParentNode;
    } while (aPossibleDescendant);
    return false;
  }

  // Adapted from EventUtils
  getInclusiveFlattenedTreeParentElement(aNode) {
    for (
      let inclusiveAncestor = aNode;
      inclusiveAncestor;
      inclusiveAncestor = this.getFlattenedTreeParentNode(inclusiveAncestor)
    ) {
      if (inclusiveAncestor.nodeType == Node.ELEMENT_NODE) {
        return inclusiveAncestor;
      }
    }
    return null;
  }

  constructor(aSubtypeName, aDragWindow, aParams) {
    this.subtypeName = aSubtypeName;
    this.dragWindow = aDragWindow;
    this.dragService = Cc["@mozilla.org/widget/dragservice;1"].getService(
      Ci.nsIDragService
    );

    Object.assign(this, aParams);

    this.info = msg => {
      aParams.info(`[${this.contextLabel}|${this.subtypeName}]| ${msg}`);
    };
    this.ok = (cond, msg) => {
      aParams.ok(cond, `[${this.contextLabel}|${this.subtypeName}]| ${msg}`);
    };
    this.is = (v1, v2, msg) => {
      aParams.is(v1, v2, `[${this.contextLabel}|${this.subtypeName}]| ${msg}`);
    };

    this.alreadyHadSession = !!this.dragService.getCurrentSession(
      this.dragWindow
    );

    this.initializeElementInfo(this.dragElementId);

    // Register for events on both the drag element AND the document so we can
    // detect that the right events were/were not sent at the right time.
    this.registerForRelevantEvents();
  }

  initializeElementInfo(aDragElementId) {
    this.dragElement = this.dragWindow.document.getElementById(aDragElementId);
    this.ok(this.dragElement, "dragElement exists");
    let rect = this.dragElement.getBoundingClientRect();
    let rectStr =
      `left: ${rect.left}, top: ${rect.top}, ` +
      `right: ${rect.right}, bottom: ${rect.bottom}`;
    this.info(`getBoundingClientRect(): ${rectStr}`);
    const scale = this.dragWindow.devicePixelRatio;
    this.clientPos = [
      this.dragElement.offsetLeft * scale,
      this.dragElement.offsetTop * scale,
    ];
    this.screenPos = [
      (this.dragWindow.mozInnerScreenX + this.dragElement.offsetLeft) * scale,
      (this.dragWindow.mozInnerScreenY + this.dragElement.offsetTop) * scale,
    ];
  }

  // Checks that the event was expected and, if so, adds the event to
  // the list of events of that type.
  eventHandlerFn(aEv) {
    this.events[aEv.type].push(aEv);
    this.info(`Element received ${aEv.type}`);

    // In order to properly test the dataTransfer, we need to try to access the
    // dataTransfer under the principal of the web page.  Otherwise, we will run
    // under the system principal and dataTransfer access will always be given.
    let sandbox = this.dragWindow.SpecialPowers.unwrap(
      Cu.Sandbox(this.dragWindow.document.nodePrincipal)
    );

    sandbox.is = this.is;
    sandbox.kTestDataTransferType = kTestDataTransferType;
    sandbox.kTestDataTransferData = kTestDataTransferData;
    sandbox.aEv = aEv;

    let getFromDataTransfer = Cu.evalInSandbox(
      "(" +
        function () {
          return aEv.dataTransfer.getData(kTestDataTransferType);
        } +
        ")",
      sandbox
    );
    let setInDataTransfer = Cu.evalInSandbox(
      "(" +
        function () {
          return aEv.dataTransfer.setData(
            kTestDataTransferType,
            kTestDataTransferData
          );
        } +
        ")",
      sandbox
    );
    let clearDataTransfer = Cu.evalInSandbox(
      "(" +
        function () {
          return aEv.dataTransfer.setData(kTestDataTransferType, "");
        } +
        ")",
      sandbox
    );

    try {
      if (aEv.type == "dragstart") {
        // Add some additional data to the DataTransfer so we can look for it
        // as we get later events.
        this.is(
          getFromDataTransfer(),
          "",
          `[${aEv.type}]| DataTransfer didn't have kTestDataTransferType`
        );
        setInDataTransfer();
        this.is(
          getFromDataTransfer(),
          kTestDataTransferData,
          `[${aEv.type}]| Successfully added kTestDataTransferType to DataTransfer`
        );
      } else if (aEv.type == "drop") {
        this.is(
          getFromDataTransfer(),
          kTestDataTransferData,
          `[${aEv.type}]| Successfully read from DataTransfer`
        );
        try {
          clearDataTransfer();
          this.ok(false, "Writing to DataTransfer throws an exception");
        } catch (ex) {
          this.ok(true, "Got exception: " + ex);
        }
        this.is(
          getFromDataTransfer(),
          kTestDataTransferData,
          `[${aEv.type}]| Properly failed to write to DataTransfer`
        );
      } else if (
        aEv.type == "dragenter" ||
        aEv.type == "dragover" ||
        aEv.type == "dragleave" ||
        aEv.type == "dragend"
      ) {
        this.is(
          getFromDataTransfer(),
          this.expectProtectedDataTransferAccess ? kTestDataTransferData : "",
          `[${aEv.type}]| ${
            this.expectProtectedDataTransferAccess
              ? "Successfully"
              : "Unsuccessfully"
          } read from DataTransfer`
        );
      }
    } catch (ex) {
      this.ok(false, "Handler did not throw an uncaught exception: " + ex);
    }

    if (
      this.throwOnExtraMessage &&
      this.events[aEv.type].length > this.expected[aEv.type]
    ) {
      throw new Error(
        `[${this.contextLabel}|${this.subtypeName}] Received unexpected ${
          aEv.type
        } | received ${this.events[aEv.type].length} > expected ${
          this.expected[aEv.type]
        } | event: ${aEv}`
      );
    }
  }

  documentEventHandlerFn(aEv) {
    this.documentEvents[aEv.type].push(aEv);
    this.info(`Document received ${aEv.type}`);
    if (
      this.throwOnExtraMessage &&
      this.documentEvents[aEv.type].length >
        this.dragWindow.document.expected[aEv.type]
    ) {
      throw new Error(
        `[${this.contextLabel}|${
          this.subtypeName
        }] Document received unexpected ${aEv.type} | received ${
          this.documentEvents[aEv.type].length
        } > expected ${
          this.dragWindow.document.expected[aEv.type]
        } | event: ${aEv}`
      );
    }
  }

  registerForRelevantEvents() {
    this.eventHandler = this.eventHandlerFn.bind(this);
    this.documentEventHandler = this.documentEventHandlerFn.bind(this);
    if (!this.dragWindow.document.expected) {
      this.dragWindow.document.expected = [];
    }
    for (let ev of this.relevantEvents) {
      this.events[ev] = [];
      this.documentEvents[ev] = [];
      this.expected[ev] = 0;
      // See the comment on the declaration of this.expected for the reason
      // why we define the expected array for document events on the document
      // itself.
      this.dragWindow.document.expected[ev] = 0;
      this.dragElement.addEventListener(ev, this.eventHandler);
      this.dragWindow.document.addEventListener(ev, this.documentEventHandler);
    }
  }

  getElementPositions() {
    this.info(`clientpos: ${this.clientPos}, screenPos: ${this.screenPos}`);
    return { clientPos: this.clientPos, screenPos: this.screenPos };
  }

  async cleanup() {
    for (let ev of this.relevantEvents) {
      this.dragElement.removeEventListener(ev, this.eventHandler);
      this.dragWindow.document.removeEventListener(
        ev,
        this.documentEventHandler
      );
    }
  }
}
