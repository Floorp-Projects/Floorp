/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
/* eslint-disable no-unused-vars */
/* global PropTypes React */

/**
 * Shorthand for creating elements (to avoid using a JSX preprocessor)
 */
const r = React.createElement;

/**
 * Information box used at the top of listings.
 */
window.InfoBox = class InfoBox extends React.Component {
  render() {
    return (
      r("div", {className: "info-box"},
        r("div", {className: "info-box-content"},
          this.props.children,
        ),
      )
    );
  }
};
window.InfoBox.propTypes = {
  children: PropTypes.node,
};

/**
 * Button using in-product styling.
 */
window.FxButton = class FxButton extends React.Component {
  render() {
    return (
      r("button", Object.assign({}, this.props, {children: undefined}),
        r("div", {className: "button-box"},
          this.props.children,
        ),
      )
    );
  }
};
window.FxButton.propTypes = {
  children: PropTypes.node,
};

/**
 * Wrapper class for a value that is provided by the frame script.
 *
 * Emits a "GetRemoteValue:{name}" page event on load to fetch the initial
 * value, and listens for "ReceiveRemoteValue:{name}" page callbacks to receive
 * the value when it updates.
 *
 * @example
 * const myRemoteValue = new RemoteValue("MyValue", 5);
 * class MyComponent extends React.Component {
 *   constructor(props) {
 *     super(props);
 *     this.state = {
 *       myValue: null,
 *     };
 *   }
 *
 *   componentWillMount() {
 *     myRemoteValue.subscribe(this);
 *   }
 *
 *   componentWillUnmount() {
 *     myRemoteValue.unsubscribe(this);
 *   }
 *
 *   receiveRemoteValue(name, value) {
 *     this.setState({myValue: value});
 *   }
 *
 *   render() {
 *     return r("div", {}, this.state.myValue);
 *   }
 * }
 */
class RemoteValue {
  constructor(name, defaultValue = null) {
    this.name = name;
    this.handlers = [];
    this.value = defaultValue;

    document.addEventListener(`ReceiveRemoteValue:${this.name}`, this);
    sendPageEvent(`GetRemoteValue:${this.name}`);
  }

  /**
   * Subscribe to this value as it updates. Handlers are called with the current
   * value immediately after subscribing.
   * @param {Object} handler
   *   Object with a receiveRemoteValue(name, value) method that is called with
   *   the name and value of this RemoteValue when it is updated.
   */
  subscribe(handler) {
    this.handlers.push(handler);
    handler.receiveRemoteValue(this.name, this.value);
  }

  /**
   * Remove a previously-registered handler.
   * @param {Object} handler
   */
  unsubscribe(handler) {
    this.handlers = this.handlers.filter(h => h !== handler);
  }

  handleEvent(event) {
    this.value = event.detail;
    for (const handler of this.handlers) {
      handler.receiveRemoteValue(this.name, this.value);
    }
  }
}

/**
 * Collection of RemoteValue instances used within the page.
 */
const remoteValues = {
  studyList: new RemoteValue("StudyList"),
  shieldLearnMoreHref: new RemoteValue("ShieldLearnMoreHref"),
  studiesEnabled: new RemoteValue("StudiesEnabled"),
  shieldTranslations: new RemoteValue("ShieldTranslations"),
};

/**
 * Dispatches a page event to the privileged frame script for this tab.
 * @param {String} action
 * @param {Object} data
 */
function sendPageEvent(action, data) {
  const event = new CustomEvent("ShieldPageEvent", {bubbles: true, detail: {action, data}});
  document.dispatchEvent(event);
}
