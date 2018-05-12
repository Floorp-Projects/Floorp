/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
/* global classnames PropTypes r React ReactDOM remoteValues ShieldStudies */

/**
 * Mapping of pages displayed on the sidebar. Keys are the value used in the
 * URL hash to identify the current page.
 *
 * Pages will appear in the sidebar in the order they are defined here. If the
 * URL doesn't contain a hash, the first page will be displayed in the content area.
 */
const PAGES = new Map([
  ["shieldStudies", {
    name: "title",
    component: ShieldStudies,
    icon: "resource://normandy-content/about-studies/img/shield-logo.png",
  }],
]);

/**
 * Handle basic layout and routing within about:studies.
 */
class AboutStudies extends React.Component {
  constructor(props) {
    super(props);

    let hash = new URL(window.location).hash.slice(1);
    if (!PAGES.has(hash)) {
      hash = "shieldStudies";
    }

    this.state = {
      currentPageId: hash,
    };

    this.handleEvent = this.handleEvent.bind(this);
  }

  componentDidMount() {
    remoteValues.shieldTranslations.subscribe(this);
    window.addEventListener("hashchange", this);
  }

  componentWillUnmount() {
    remoteValues.shieldTranslations.unsubscribe(this);
    window.removeEventListener("hashchange", this);
  }

  receiveRemoteValue(name, value) {
    switch (name) {
      case "ShieldTranslations": {
        this.setState({ translations: value });
        break;
      }
      default: {
        console.error(`Unknown remote value ${name}`);
      }
    }
  }

  handleEvent(event) {
    const newHash = new URL(event.newURL).hash.slice(1);
    if (PAGES.has(newHash)) {
      this.setState({currentPageId: newHash});
    }
  }

  render() {
    const currentPageId = this.state.currentPageId;
    const pageEntries = Array.from(PAGES.entries());
    const currentPage = PAGES.get(currentPageId);
    const { translations } = this.state;

    return (
      r("div", {className: "about-studies-container"},
        translations && r(Sidebar, {},
          pageEntries.map(([id, page]) => (
            r(SidebarItem, {
              key: id,
              pageId: id,
              selected: id === currentPageId,
              page,
              translations,
            })
          )),
        ),
        r(Content, {},
          translations && currentPage && r(currentPage.component, {translations})
        ),
      )
    );
  }
}

class Sidebar extends React.Component {
  render() {
    return r("ul", {id: "categories"}, this.props.children);
  }
}
Sidebar.propTypes = {
  children: PropTypes.node,
  translations: PropTypes.object.isRequired,
};

class SidebarItem extends React.Component {
  constructor(props) {
    super(props);
    this.handleClick = this.handleClick.bind(this);
  }

  handleClick() {
    window.location = `#${this.props.pageId}`;
  }

  render() {
    const { page, selected, translations } = this.props;
    return (
      r("li", {
        className: classnames("category", {selected}),
        onClick: this.handleClick,
      },
        page.icon && r("img", {className: "category-icon", src: page.icon}),
        r("span", {className: "category-name"}, translations[page.name]),
      )
    );
  }
}
SidebarItem.propTypes = {
  pageId: PropTypes.string.isRequired,
  page: PropTypes.shape({
    icon: PropTypes.string,
    name: PropTypes.string.isRequired,
  }).isRequired,
  selected: PropTypes.bool,
  translations: PropTypes.object.isRequired,
};

class Content extends React.Component {
  render() {
    return (
      r("div", {className: "main-content"},
        r("div", {className: "content-box"},
          this.props.children,
        ),
      )
    );
  }
}
Content.propTypes = {
  children: PropTypes.node,
};

ReactDOM.render(
  r(AboutStudies),
  document.getElementById("app"),
);
