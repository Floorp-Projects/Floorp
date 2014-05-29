/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop, sinon */

var expect = chai.expect;

describe("loop.webapp", function() {
  "use strict";

  var sandbox;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("Router", function() {
    var conversation, fakeSessionData;

    beforeEach(function() {
      conversation = new loop.shared.models.ConversationModel();
      fakeSessionData = {
        sessionId:    "sessionId",
        sessionToken: "sessionToken",
        apiKey:       "apiKey"
      };
    });

    describe("#constructor", function() {
      it("should require a ConversationModel instance", function() {
        expect(function() {
          new loop.webapp.Router();
        }).to.Throw(Error, /missing required conversation/);
      });

      it("should load the HomeView", function() {
        sandbox.stub(loop.webapp.Router.prototype, "loadView");

        var router = new loop.webapp.Router({conversation: conversation});

        sinon.assert.calledOnce(router.loadView);
        sinon.assert.calledWithMatch(router.loadView,
                                     {$el: {selector: "#home"}});
      });
    });

    describe("constructed", function() {
      var router;

      beforeEach(function() {
        router = new loop.webapp.Router({conversation: conversation});
      });

      describe("#loadView", function() {
        // XXX hard to test as hell… functional?
        it("should load the passed view");
      });
    });

    describe("Routes", function() {
      var router;

      beforeEach(function() {
        router = new loop.webapp.Router({conversation: conversation});
        sandbox.stub(router, "loadView");
      });

      describe("#home", function() {
        it("should load the HomeView", function() {
          router.home();

          sinon.assert.calledOnce(router.loadView);
          sinon.assert.calledWithMatch(router.loadView,
                                       {$el: {selector: "#home"}});
        });
      });

      describe("#initiate", function() {
        it("should load the ConversationFormView and set the token",
          function() {
            router.initiate("fakeToken");

            sinon.assert.calledOnce(router.loadView);
            sinon.assert.calledWithMatch(router.loadView, {
              $el: {selector: "#conversation-form"},
              model: {attributes: {loopToken: "fakeToken"}}
            });
          });
      });

      describe("#conversation", function() {
        it("should load the ConversationView if session is set", function() {
          sandbox.stub(loop.shared.views.ConversationView.prototype,
            "initialize");
          conversation.set("sessionId", "fakeSessionId");

          router.conversation();

          sinon.assert.calledOnce(router.loadView);
          sinon.assert.calledWithMatch(router.loadView, {
            $el: {selector: "#conversation"}
          });
        });

        it("should redirect to #call/{token} if session isn't ready",
          function() {
            conversation.set("loopToken", "fakeToken");
            sandbox.stub(router, "navigate");

            router.conversation();

            sinon.assert.calledOnce(router.navigate);
            sinon.assert.calledWithMatch(router.navigate, "call/fakeToken");
          });

        it("should redirect to #home if no call token is set", function() {
          sandbox.stub(router, "navigate");

          router.conversation();

          sinon.assert.calledOnce(router.navigate);
          sinon.assert.calledWithMatch(router.navigate, "home");
        });
      });
    });

    describe("Events", function() {
      it("should navigate to call/ongoing once the call session is ready",
        function() {
          sandbox.stub(loop.webapp.Router.prototype, "navigate");
          var router = new loop.webapp.Router({conversation: conversation});

          conversation.setReady(fakeSessionData);

          sinon.assert.calledOnce(router.navigate);
          sinon.assert.calledWith(router.navigate, "call/ongoing");
        });
    });
  });

  describe("Router", function() {
    var router, conversation;

    beforeEach(function() {
      conversation = new loop.shared.models.ConversationModel({
        loopToken: "fake"
      });
      router = new loop.webapp.Router({conversation: conversation});
    });

    describe("#constructor", function() {
      it("should define a default active view", function() {
        expect(router.activeView).to.be.an.instanceOf(loop.webapp.HomeView);
      });
    });

    describe("#loadView", function() {
      it("should set the active view", function() {
        router.loadView(new loop.webapp.ConversationFormView({
          model: conversation
        }));

        expect(router.activeView).to.be.an.instanceOf(
          loop.webapp.ConversationFormView);
      });
    });
  });

  describe("ConversationFormView", function() {
    describe("#initiate", function() {
      var conversation, initiate, view, fakeSubmitEvent;

      beforeEach(function() {
        conversation = new loop.shared.models.ConversationModel();
        view = new loop.webapp.ConversationFormView({model: conversation});
        fakeSubmitEvent = {preventDefault: sinon.spy()};
      });

      it("should start the conversation establishment process", function() {
        initiate = sinon.stub(conversation, "initiate");
        conversation.set("loopToken", "fake");

        view.initiate(fakeSubmitEvent);

        sinon.assert.calledOnce(fakeSubmitEvent.preventDefault);
        sinon.assert.calledOnce(initiate);
      });
    });
  });
});
