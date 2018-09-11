/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
/* global classnames PropTypes React ReactDOM */

/**
 * Shorthand for creating elements (to avoid using a JSX preprocessor)
 */
const r = React.createElement;

/**
 * Dispatches a page event to the privileged frame script for this tab.
 * @param {String} action
 * @param {Object} data
 */
function sendPageEvent(action, data) {
  const event = new CustomEvent("ShieldPageEvent", { bubbles: true, detail: { action, data } });
  document.dispatchEvent(event);
}

/**
 * Handle basic layout and routing within about:studies.
 */
class AboutStudies extends React.Component {
  constructor(props) {
    super(props);

    this.remoteValueNameMap = {
      StudyList: "addonStudies",
      ShieldLearnMoreHref: "learnMoreHref",
      StudiesEnabled: "studiesEnabled",
      ShieldTranslations: "translations",
    };

    this.state = {};
    for (const stateName of Object.values(this.remoteValueNameMap)) {
      this.state[stateName] = null;
    }
  }

  componentWillMount() {
    for (const remoteName of Object.keys(this.remoteValueNameMap)) {
      document.addEventListener(`ReceiveRemoteValue:${remoteName}`, this);
      sendPageEvent(`GetRemoteValue:${remoteName}`);
    }
  }

  componentWillUnmount() {
    for (const remoteName of Object.keys(this.remoteValueNameMap)) {
      document.removeEventListener(`ReceiveRemoteValue:${remoteName}`, this);
    }
  }

  /** Event handle to receive remote values from documentAddEventListener */
  handleEvent({ type, detail: value }) {
    const prefix = "ReceiveRemoteValue:";
    if (type.startsWith(prefix)) {
      const name = type.substring(prefix.length);
      this.setState({ [this.remoteValueNameMap[name]]: value });
    }
  }

  render() {
    const { translations, learnMoreHref, studiesEnabled, addonStudies } = this.state;

    // Wait for all values to be loaded before rendering. Some of the values may
    // be falsey, so an explicit null check is needed.
    if (Object.values(this.state).some(v => v === null)) {
      return null;
    }

    return (
      r("div", { className: "about-studies-container main-content" },
        r(WhatsThisBox, { translations, learnMoreHref, studiesEnabled }),
        r(StudyList, { translations, addonStudies }),
      )
    );
  }
}

/**
 * Explains the contents of the page, and offers a way to learn more and update preferences.
 */
class WhatsThisBox extends React.Component {
  handleUpdateClick() {
    sendPageEvent("NavigateToDataPreferences");
  }

  render() {
    const { learnMoreHref, studiesEnabled, translations } = this.props;

    return (
      r("div", { className: "info-box" },
        r("div", { className: "info-box-content" },
          r("span", {},
            studiesEnabled ? translations.enabledList : translations.disabledList,
          ),
          r("a", { id: "shield-studies-learn-more", href: learnMoreHref }, translations.learnMore),

          r("button", { id: "shield-studies-update-preferences", onClick: this.handleUpdateClick },
            r("div", { className: "button-box" },
              navigator.platform.includes("Win") ? translations.updateButtonWin : translations.updateButtonUnix
            ),
          )
        )
      )
    );
  }
}

/**
 * Shows a list of studies, with an option to end in-progress ones.
 */
class StudyList extends React.Component {
  render() {
    const { addonStudies, translations } = this.props;

    if (!addonStudies.length) {
      return r("p", { className: "study-list-info" }, translations.noStudies);
    }

    addonStudies.sort((a, b) => {
      if (a.active !== b.active) {
        return a.active ? -1 : 1;
      }
      return b.studyStartDate - a.studyStartDate;
    });

    return (
      r("ul", { className: "study-list" },
        addonStudies.map(study => (
          r(StudyListItem, { key: study.name, study, translations })
        ))
      )
    );
  }
}
StudyList.propTypes = {
  addonStudies: PropTypes.array.isRequired,
  translations: PropTypes.object.isRequired,
};

/**
 * Details about an individual study, with an option to end it if it is active.
 */
class StudyListItem extends React.Component {
  constructor(props) {
    super(props);
    this.handleClickRemove = this.handleClickRemove.bind(this);
  }

  handleClickRemove() {
    sendPageEvent("RemoveStudy", { recipeId: this.props.study.recipeId, reason: "individual-opt-out" });
  }

  render() {
    const { study, translations } = this.props;
    return (
      r("li", {
        className: classnames("study", { disabled: !study.active }),
        "data-study-name": study.name,
      },
        r("div", { className: "study-icon" },
          study.name.slice(0, 1)
        ),
        r("div", { className: "study-details" },
          r("div", { className: "study-name" }, study.name),
          r("div", { className: "study-description", title: study.description },
            r("span", { className: "study-status" }, study.active ? translations.activeStatus : translations.completeStatus),
            r("span", {}, "\u2022"), // &bullet;
            r("span", {}, study.description),
          ),
        ),
        r("div", { className: "study-actions" },
          study.active &&
          r("button", { className: "remove-button", onClick: this.handleClickRemove },
            r("div", { className: "button-box" },
              translations.removeButton
            ),
          )
        ),
      )
    );
  }
}
StudyListItem.propTypes = {
  study: PropTypes.shape({
    recipeId: PropTypes.number.isRequired,
    name: PropTypes.string.isRequired,
    active: PropTypes.boolean,
    description: PropTypes.string.isRequired,
  }).isRequired,
  translations: PropTypes.object.isRequired,
};

ReactDOM.render(r(AboutStudies), document.getElementById("app"));
