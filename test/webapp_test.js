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
