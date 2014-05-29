/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*global loop:true*/

var loop = loop || {};
loop.webapp = (function() {
  "use strict";

  /**
   * Base Loop server URL.
   *
   * XXX: should be configurable, but how?
   *
   * @type {String}
   */
  var baseApiUrl = "http://localhost:5000";

  /**
   * App router.
   * @type {loop.webapp.Router}
   */
  var router;

  /**
   * Current conversation model instance.
   * @type {loop.webapp.ConversationModel}
   */
  var conversation;

  /**
   * Conversation model.
   */
  var ConversationModel = Backbone.Model.extend({
    defaults: {
      loopToken:    undefined, // Loop conversation token
      sessionId:    undefined, // TB session id
      sessionToken: undefined, // TB session token
      apiKey:       undefined  // TB api key
    },

    /**
     * Initiates a conversation, requesting call session information to the Loop
     * server and updates appropriately the current model attributes with the
     * data.
     *
     * Triggered events:
     *
     * - `session:ready` when the session information have been succesfully
     *   retrieved from the server;
     * - `session:error` when the request failed.
     *
     * @param  {Object} options Conversation options.
     * @throws {Error} If no conversation token is set
     */
    initiate: function(options) {
      options = options || {};

      if (!this.get("loopToken")) {
        throw new Error("missing required attribute loopToken");
      }

      var request = $.ajax({
        url:         baseApiUrl + "/calls/" + this.get("loopToken"),
        method:      "POST",
        contentType: "application/json",
        data:        JSON.stringify({}),
        dataType:    "json"
      });

      request.done(this.setReady.bind(this));

      request.fail(function(xhr, _, statusText) {
        var serverError = xhr.status + " " + statusText;
        if (typeof xhr.responseJSON === "object" && xhr.responseJSON.error) {
          serverError += "; " + xhr.responseJSON.error;
        }
        this.trigger("session:error", new Error(
          "Retrieval of session information failed: HTTP " + serverError));
      }.bind(this));
    },

    /**
     * Sets session information and triggers the `session:ready` event.
     *
     * @param {Object} sessionData Conversation session information.
     */
    setReady: function(sessionData) {
      // Explicit property assignment to prevent later "surprises"
      this.set({
        sessionId:    sessionData.sessionId,
        sessionToken: sessionData.sessionToken,
        apiKey:       sessionData.apiKey
      }).trigger("session:ready", this);
      return this;
    }
  });

  /**
   * Base Backbone view.
   */
  var BaseView = Backbone.View.extend({
    /**
     * Hides view element.
     *
     * @return {BaseView}
     */
    hide: function() {
      this.$el.hide();
      return this;
    },

    /**
     * Shows view element.
     *
     * @return {BaseView}
     */
    show: function() {
      this.$el.show();
      return this;
    }
  });

  /**
   * Homepage view.
   */
  var HomeView = BaseView.extend({
    el: "#home"
  });

  /**
   * Conversation launcher view. A ConversationModel is associated and attached
   * as a `model` property.
   */
  var ConversationFormView = BaseView.extend({
    el: "#conversation-form",

    events: {
      "submit": "initiate"
    },

    initialize: function() {
      this.listenTo(this.model, "session:error", function(error) {
        // XXX: display a proper error notification to end user, probably
        //      reusing the BB notification system from the Loop desktop client.
        alert(error);
      });
    },

    initiate: function(event) {
      event.preventDefault();
      this.model.initiate();
    }
  });

  /**
   * Conversation view.
   */
  var ConversationView = BaseView.extend({
    el: "#conversation"
  });

  /**
   * App Router. Allows defining a main active view and ease toggling it when
   * the active route changes.
   * @link http://mikeygee.com/blog/backbone.html
   */
  var Router = Backbone.Router.extend({
    _conversation: undefined,
    activeView: undefined,

    routes: {
      "": "home",
      "call/:token": "initiate"
    },

    initialize: function(options) {
      options = options || {};
      if (!options.conversation) {
        throw new Error("missing required conversation");
      }
      this._conversation = options.conversation;
      this.listenTo(this._conversation, "session:ready",
                    this._onConversationSessionReady);

      // Load default view
      this.loadView(new HomeView());
    },

    /**
     * Called when a conversation session is ready.
     *
     * @param  {ConversationModel} conversation Conversation model instance.
     */
    _onConversationSessionReady: function(conversation) {
      // XXX: navigate to the conversation route
      //      establish conversation with TB sdk
      //      setup conversation view accordingly
      alert("conversation session ready");
      console.log("conversation session info", conversation);
    },

    /**
     * Loads and render current active view.
     *
     * @param {BaseView} view View.
     */
    loadView : function(view) {
      if (this.activeView) {
        this.activeView.hide();
      }
      this.activeView = view.render().show();
    },

    /**
     * Default entry point.
     */
    home: function() {
      this.loadView(new HomeView());
    },

    /**
     * Loads conversation launcher view, setting the received conversation token
     * to the current conversation model.
     *
     * @param  {String} loopToken Loop conversation token.
     */
    initiate: function(loopToken) {
      this._conversation.set("loopToken", loopToken);
      this.loadView(new ConversationFormView({model: this._conversation}));
    },

    /**
     * Loads conversation establishment view.
     *
     */
    conversation: function() {
      this.loadView(new ConversationView({model: this._conversation}));
    }
  });

  /**
   * App initialization.
   */
  function init() {
    conversation = new ConversationModel();
    router = new Router({conversation: conversation});
    Backbone.history.start();
  }

  return {
    BaseView: BaseView,
    ConversationFormView: ConversationFormView,
    ConversationModel: ConversationModel,
    HomeView: HomeView,
    init: init,
    Router: Router
  };
})();
