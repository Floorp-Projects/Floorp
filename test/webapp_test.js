/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop, sinon */

var expect = chai.expect;

describe("loop.webapp", function() {
  "use strict";

  var sandbox, fakeXHR, requests = [];

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    fakeXHR = sandbox.useFakeXMLHttpRequest();
    requests = [];
    // https://github.com/cjohansen/Sinon.JS/issues/393
    fakeXHR.xhr.onCreate = function(xhr) {
      requests.push(xhr);
    };
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("Router", function() {
    var conversation, fakeSessionData;

    beforeEach(function() {
      conversation = new loop.webapp.ConversationModel();
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
          sandbox.stub(loop.webapp.ConversationView.prototype, "initialize");
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

  describe("ConversationModel", function() {
    var conversation, fakeSessionData;

    beforeEach(function() {
      conversation = new loop.webapp.ConversationModel();
      fakeSessionData = {
        sessionId:    "sessionId",
        sessionToken: "sessionToken",
        apiKey:       "apiKey"
      };
    });

    describe("#initiate", function() {
      it("should prevent launching a conversation when token is missing",
        function() {
          expect(function() {
            conversation.initiate();
          }).to.Throw(Error, /missing required attribute loopToken/);
        });

      it("should update conversation session information from server data",
        function() {
          conversation.set("loopToken", "fakeToken");
          conversation.initiate();

          expect(requests).to.have.length.of(1);

          requests[0].respond(200, {"Content-Type": "application/json"},
                                   JSON.stringify(fakeSessionData));

          expect(conversation.get("sessionId")).eql("sessionId");
          expect(conversation.get("sessionToken")).eql("sessionToken");
          expect(conversation.get("apiKey")).eql("apiKey");
        });

      it("should trigger session:ready without fetching session data over "+
         "HTTP when already set", function(done) {
          sandbox.stub(jQuery, "ajax");
          conversation.set("loopToken", "fakeToken");
          conversation.set(fakeSessionData);

          conversation.on("session:ready", function() {
            sinon.assert.notCalled($.ajax);
            done();
          }).initiate();
        });

      it("should trigger a `session:error` on failure", function(done) {
        conversation.set("loopToken", "fakeToken");
        conversation.initiate();

        conversation.on("session:error", function(err) {
          expect(err.message).to.match(/failed: HTTP 400 Bad Request; fake/);
          done();
        });

        requests[0].respond(400, {"Content-Type": "application/json"},
                                  JSON.stringify({error: "fake"}));
      });
    });

    describe("#setReady", function() {
      it("should update conversation session information", function() {
        conversation.setReady(fakeSessionData);

        expect(conversation.get("sessionId")).eql("sessionId");
        expect(conversation.get("sessionToken")).eql("sessionToken");
        expect(conversation.get("apiKey")).eql("apiKey");
      });

      it("should trigger a `session:ready` event", function(done) {
        conversation.on("session:ready", function() {
          done();
        }).setReady(fakeSessionData);
      });
    });
  });

  describe("Router", function() {
    var router, conversation;

    beforeEach(function() {
      conversation = new loop.webapp.ConversationModel({loopToken: "fake"});
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
        conversation = new loop.webapp.ConversationModel();
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
