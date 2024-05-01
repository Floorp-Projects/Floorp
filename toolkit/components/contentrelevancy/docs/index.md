# Content Relevancy

This is the home for the project: Interest-based Content Relevance Ranking & Personalization for Firefox,
a client-based privacy preserving approach to enhancing content experience of Firefox.

```{toctree}
:titlesonly:
:maxdepth: 1
:glob:

telemetry.md
```

## Overview

The following diagram illustrates the system overview of the component.
The system consists of three main parts: the relevancy manager, the relevancy store,
and the internal & external data sources.
The cross-platform component [`relevancy`][relevancy-component] serves as the backbone
that is responsible for interest classification, persistence, and ranking.

```{mermaid}
graph TB
subgraph Firefox
  direction LR
  subgraph rmgr[Relevancy Manager]
    subgraph rcmp[Rust Component]
      classify[Perform Interest Classification]
      manage[Manage]
      query[Query & Rank]
    end
    iic[Initiate Interest Classification]
    rtuc[Respond to User Commands]
    irt[Input Retriever]
  end

  subgraph places[Places]
    direction TB
    tfv[(Top Frecent Visits)]
    mrv[(Most Recent Visits)]
    others[(Other Inputs)]
  end

  subgraph rstore[Relevancy Store]
    direction TB
    iui[Inferred User Interests]
    icm[Ingested Classifier Metadata]
  end
end

subgraph settings[Remote Settings]
  rs[("content-relevancy")]
end

iic --> |call| classify
classify --> |write| rstore
query --> |"query (read-only)"| rstore
manage --> |update| rstore
places --> |input| irt --> iic
rs --- |fetch<br/> classification<br/> data| classify
rtuc --> |call| manage
```

### Relevancy Manager

The relevancy manager manages the following tasks for the relevancy component:
- Start a browser update timer to periodically perform interest classifications
- Listen and respond to user commands such as enable / disable the component,
  update / reset the relevancy store upon Places changes, etc.
- Nimbus integration
- Telemetry

The interest classification is fulfilled by the `relevancy` Rust component.
The `relevancy` component downloads & ingests interest data (e.g. "small classifiers")
from Remote Settings and then applies those classifiers to the chosen inputs.
Classification results are persisted in the relevancy store for future uses.

### Relevancy Store

The relevancy store, an SQLite database, serves as the storage for both
the user interests and the ingested classifier metadata. It's managed by the
`relevancy` Rust component, which provides consumers (e.g. the relevancy manager)
with a series of APIs for ingestion, querying, ranking, and updating.

### Data Sources

Interest classification relies on various internal & external data sources to function.
- Interest classifiers and metadata are prepared offline and served through
  Remote Settings. The download and ingestion are handled by the Rust component
- Classification inputs (e.g. top frecent visits and most recent visits) are retrieved
  from Places. An input utility module is provided to facilitate the input retrieval

## Working on the Code

### Component Structure

- `ContentRelevancyManager.sys.mjs` implements the relevancy manager. A singleton
  relevancy manager is initialized as a background task in `BrowserGlue.sys.mjs`
- Modules defined in `private` should be treated as internal artifacts for the
  component and should not be consumed by other browser components.
  `InputUtils.sys.mjs` defines several utility functions for input retrieval

### Telemetry

Please reference the [telemetry](./telemetry.md) section.

### Testing

Since the component is relatively small and does not have any user facing interfaces,
XPCShell tests are recommended for general testing. Mochitests can be used if you're
testing functionalities that would require a more complete browser environment. Only
the Nimbus tests are written as Mochitests at the moment.

Notes:
- The component requires a browser profile for both Places and the relevancy store,
  `do_get_profile()` is called in `head.js`
- Most XPCShell tests are skipped for Android as some test dependencies (`PlacesTestUtils`)
  are not available on Android

[relevancy-component]: https://github.com/mozilla/application-services/tree/main/components/relevancy
