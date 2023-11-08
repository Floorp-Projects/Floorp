/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* eslint no-undef: 0 no-unused-vars: 0 */

add_task(async function test_deserializeSharedIdInvalidTypes() {
  await runTestInContent(() => {
    for (const invalidType of [false, 42, {}, []]) {
      info(`Checking type: '${invalidType}'`);

      const serializedValue = {
        sharedId: invalidType,
      };

      Assert.throws(
        () => deserialize(realm, serializedValue, { nodeCache }),
        /InvalidArgumentError:/,
        `Got expected error for type ${invalidType}`
      );
    }
  });
});

add_task(async function test_deserializeSharedIdInvalidValue() {
  await runTestInContent(() => {
    const serializedValue = {
      sharedId: "foo",
    };

    Assert.throws(
      () => deserialize(realm, serializedValue, { nodeCache }),
      /NoSuchNodeError:/,
      "Got expected error for unknown 'sharedId'"
    );
  });
});

add_task(async function test_deserializeSharedId() {
  await loadURL(inline("<div>"));

  await runTestInContent(() => {
    const domEl = content.document.querySelector("div");
    const domElRef = nodeCache.getOrCreateNodeReference(domEl, seenNodeIds);

    const serializedValue = {
      sharedId: domElRef,
    };

    const node = deserialize(realm, serializedValue, { nodeCache });

    Assert.equal(node, domEl);
  });
});

add_task(async function test_deserializeSharedIdPrecedenceOverHandle() {
  await loadURL(inline("<div>"));

  await runTestInContent(() => {
    const domEl = content.document.querySelector("div");
    const domElRef = nodeCache.getOrCreateNodeReference(domEl, seenNodeIds);

    const serializedValue = {
      handle: "foo",
      sharedId: domElRef,
    };

    const node = deserialize(realm, serializedValue, { nodeCache });

    Assert.equal(node, domEl);
  });
});

add_task(async function test_deserializeSharedIdNoWindowRealm() {
  await loadURL(inline("<div>"));

  await runTestInContent(() => {
    const domEl = content.document.querySelector("div");
    const domElRef = nodeCache.getOrCreateNodeReference(domEl, seenNodeIds);

    const serializedValue = {
      sharedId: domElRef,
    };

    Assert.throws(
      () => deserialize(new Realm(), serializedValue, { nodeCache }),
      /NoSuchNodeError/,
      `Got expected error for a non-window realm`
    );
  });
});

// Bug 1819902: Instead of a browsing context check compare the origin
add_task(async function test_deserializeSharedIdOtherBrowsingContext() {
  await loadURL(inline("<iframe>"));

  await runTestInContent(() => {
    const iframeEl = content.document.querySelector("iframe");
    const domEl = iframeEl.contentWindow.document.createElement("div");
    iframeEl.contentWindow.document.body.appendChild(domEl);

    const domElRef = nodeCache.getOrCreateNodeReference(domEl, seenNodeIds);

    const serializedValue = {
      sharedId: domElRef,
    };

    const node = deserialize(realm, serializedValue, { nodeCache });

    Assert.equal(node, null);
  });
});

add_task(async function test_serializeRemoteComplexValues() {
  await loadURL(inline("<div>"));

  await runTestInContent(() => {
    const domEl = content.document.querySelector("div");
    const domElRef = nodeCache.getOrCreateNodeReference(domEl, seenNodeIds);

    const REMOTE_COMPLEX_VALUES = [
      {
        value: content.document.querySelector("div"),
        serialized: {
          type: "node",
          sharedId: domElRef,
          value: {
            attributes: {},
            childNodeCount: 0,
            localName: "div",
            namespaceURI: "http://www.w3.org/1999/xhtml",
            nodeType: 1,
            shadowRoot: null,
          },
        },
      },
      {
        value: content.document.querySelectorAll("div"),
        serialized: {
          type: "nodelist",
          value: [
            {
              type: "node",
              sharedId: domElRef,
              value: {
                nodeType: 1,
                localName: "div",
                namespaceURI: "http://www.w3.org/1999/xhtml",
                childNodeCount: 0,
                attributes: {},
                shadowRoot: null,
              },
            },
          ],
        },
      },
      {
        value: content.document.getElementsByTagName("div"),
        serialized: {
          type: "htmlcollection",
          value: [
            {
              type: "node",
              sharedId: domElRef,
              value: {
                nodeType: 1,
                localName: "div",
                namespaceURI: "http://www.w3.org/1999/xhtml",
                childNodeCount: 0,
                attributes: {},
                shadowRoot: null,
              },
            },
          ],
        },
      },
    ];

    for (const type of REMOTE_COMPLEX_VALUES) {
      serializeAndAssertRemoteValue(type);
    }
  });
});

add_task(async function test_serializeWindow() {
  await loadURL(inline("<iframe>"));

  await runTestInContent(() => {
    const REMOTE_COMPLEX_VALUES = [
      {
        value: content,
        serialized: {
          type: "window",
          value: {
            context: content.browsingContext.browserId.toString(),
            isTopBrowsingContext: true,
          },
        },
      },
      {
        value: content.frames[0],
        serialized: {
          type: "window",
          value: {
            context: content.frames[0].browsingContext.id.toString(),
          },
        },
      },
      {
        value: content.document.querySelector("iframe").contentWindow,
        serialized: {
          type: "window",
          value: {
            context: content.document
              .querySelector("iframe")
              .contentWindow.browsingContext.id.toString(),
          },
        },
      },
    ];

    for (const type of REMOTE_COMPLEX_VALUES) {
      serializeAndAssertRemoteValue(type);
    }
  });
});

add_task(async function test_serializeNodeChildren() {
  await loadURL(inline("<div></div><iframe/>"));

  await runTestInContent(() => {
    // Add the used elements to the cache so that we know the unique reference.
    const bodyEl = content.document.body;
    const domEl = bodyEl.querySelector("div");
    const iframeEl = bodyEl.querySelector("iframe");

    const bodyElRef = nodeCache.getOrCreateNodeReference(bodyEl, seenNodeIds);
    const domElRef = nodeCache.getOrCreateNodeReference(domEl, seenNodeIds);
    const iframeElRef = nodeCache.getOrCreateNodeReference(
      iframeEl,
      seenNodeIds
    );

    const dataSet = [
      {
        node: bodyEl,
        serializationOptions: {
          maxDomDepth: null,
        },
        serialized: {
          type: "node",
          sharedId: bodyElRef,
          value: {
            nodeType: 1,
            localName: "body",
            namespaceURI: "http://www.w3.org/1999/xhtml",
            childNodeCount: 2,
            children: [
              {
                type: "node",
                sharedId: domElRef,
                value: {
                  nodeType: 1,
                  localName: "div",
                  namespaceURI: "http://www.w3.org/1999/xhtml",
                  childNodeCount: 0,
                  children: [],
                  attributes: {},
                  shadowRoot: null,
                },
              },
              {
                type: "node",
                sharedId: iframeElRef,
                value: {
                  nodeType: 1,
                  localName: "iframe",
                  namespaceURI: "http://www.w3.org/1999/xhtml",
                  childNodeCount: 0,
                  children: [],
                  attributes: {},
                  shadowRoot: null,
                },
              },
            ],
            attributes: {},
            shadowRoot: null,
          },
        },
      },
      {
        node: bodyEl,
        serializationOptions: {
          maxDomDepth: 0,
        },
        serialized: {
          type: "node",
          sharedId: bodyElRef,
          value: {
            attributes: {},
            childNodeCount: 2,
            localName: "body",
            namespaceURI: "http://www.w3.org/1999/xhtml",
            nodeType: 1,
            shadowRoot: null,
          },
        },
      },
      {
        node: bodyEl,
        serializationOptions: {
          maxDomDepth: 1,
        },
        serialized: {
          type: "node",
          sharedId: bodyElRef,
          value: {
            attributes: {},
            childNodeCount: 2,
            children: [
              {
                type: "node",
                sharedId: domElRef,
                value: {
                  attributes: {},
                  childNodeCount: 0,
                  localName: "div",
                  namespaceURI: "http://www.w3.org/1999/xhtml",
                  nodeType: 1,
                  shadowRoot: null,
                },
              },
              {
                type: "node",
                sharedId: iframeElRef,
                value: {
                  attributes: {},
                  childNodeCount: 0,
                  localName: "iframe",
                  namespaceURI: "http://www.w3.org/1999/xhtml",
                  nodeType: 1,
                  shadowRoot: null,
                },
              },
            ],
            localName: "body",
            namespaceURI: "http://www.w3.org/1999/xhtml",
            nodeType: 1,
            shadowRoot: null,
          },
        },
      },
      {
        node: domEl,
        serializationOptions: {
          maxDomDepth: 0,
        },
        serialized: {
          type: "node",
          sharedId: domElRef,
          value: {
            attributes: {},
            childNodeCount: 0,
            localName: "div",
            namespaceURI: "http://www.w3.org/1999/xhtml",
            nodeType: 1,
            shadowRoot: null,
          },
        },
      },
      {
        node: domEl,
        serializationOptions: {
          maxDomDepth: 1,
        },
        serialized: {
          type: "node",
          sharedId: domElRef,
          value: {
            attributes: {},
            childNodeCount: 0,
            children: [],
            localName: "div",
            namespaceURI: "http://www.w3.org/1999/xhtml",
            nodeType: 1,
            shadowRoot: null,
          },
        },
      },
    ];

    for (const { node, serializationOptions, serialized } of dataSet) {
      const { maxDomDepth } = serializationOptions;
      info(`Checking '${node.localName}' with maxDomDepth ${maxDomDepth}`);

      const serializationInternalMap = new Map();

      const serializedValue = serialize(
        node,
        serializationOptions,
        "none",
        serializationInternalMap,
        realm,
        { nodeCache, seenNodeIds }
      );

      Assert.deepEqual(serializedValue, serialized, "Got expected structure");
    }
  });
});

add_task(async function test_serializeNodeEmbeddedWithin() {
  await loadURL(inline("<div>"));

  await runTestInContent(() => {
    // Add the used elements to the cache so that we know the unique reference.
    const bodyEl = content.document.body;
    const domEl = bodyEl.querySelector("div");
    const domElRef = nodeCache.getOrCreateNodeReference(domEl, seenNodeIds);

    const dataSet = [
      {
        embedder: "array",
        wrapper: node => [node],
        serialized: {
          type: "array",
          value: [
            {
              type: "node",
              sharedId: domElRef,
              value: {
                attributes: {},
                childNodeCount: 0,
                localName: "div",
                namespaceURI: "http://www.w3.org/1999/xhtml",
                nodeType: 1,
                shadowRoot: null,
              },
            },
          ],
        },
      },
      {
        embedder: "map",
        wrapper: node => {
          const map = new Map();
          map.set(node, "elem");
          return map;
        },
        serialized: {
          type: "map",
          value: [
            [
              {
                type: "node",
                sharedId: domElRef,
                value: {
                  attributes: {},
                  childNodeCount: 0,
                  localName: "div",
                  namespaceURI: "http://www.w3.org/1999/xhtml",
                  nodeType: 1,
                  shadowRoot: null,
                },
              },
              {
                type: "string",
                value: "elem",
              },
            ],
          ],
        },
      },
      {
        embedder: "map",
        wrapper: node => {
          const map = new Map();
          map.set("elem", node);
          return map;
        },
        serialized: {
          type: "map",
          value: [
            [
              "elem",
              {
                type: "node",
                sharedId: domElRef,
                value: {
                  attributes: {},
                  childNodeCount: 0,
                  localName: "div",
                  namespaceURI: "http://www.w3.org/1999/xhtml",
                  nodeType: 1,
                  shadowRoot: null,
                },
              },
            ],
          ],
        },
      },
      {
        embedder: "object",
        wrapper: node => ({ elem: node }),
        serialized: {
          type: "object",
          value: [
            [
              "elem",
              {
                type: "node",
                sharedId: domElRef,
                value: {
                  attributes: {},
                  childNodeCount: 0,
                  localName: "div",
                  namespaceURI: "http://www.w3.org/1999/xhtml",
                  nodeType: 1,
                  shadowRoot: null,
                },
              },
            ],
          ],
        },
      },
      {
        embedder: "set",
        wrapper: node => {
          const set = new Set();
          set.add(node);
          return set;
        },
        serialized: {
          type: "set",
          value: [
            {
              type: "node",
              sharedId: domElRef,
              value: {
                attributes: {},
                childNodeCount: 0,
                localName: "div",
                namespaceURI: "http://www.w3.org/1999/xhtml",
                nodeType: 1,
                shadowRoot: null,
              },
            },
          ],
        },
      },
    ];

    for (const { embedder, wrapper, serialized } of dataSet) {
      info(`Checking embedding node within ${embedder}`);

      const serializationInternalMap = new Map();

      const serializedValue = serialize(
        wrapper(domEl),
        { maxDomDepth: 0 },
        "none",
        serializationInternalMap,
        realm,
        { nodeCache }
      );

      Assert.deepEqual(serializedValue, serialized, "Got expected structure");
    }
  });
});

add_task(async function test_serializeShadowRoot() {
  await runTestInContent(() => {
    for (const mode of ["open", "closed"]) {
      info(`Checking shadow root with mode '${mode}'`);
      const customElement = content.document.createElement(
        `${mode}-custom-element`
      );
      const insideShadowRootElement = content.document.createElement("input");
      content.document.body.appendChild(customElement);
      const shadowRoot = customElement.attachShadow({ mode });
      shadowRoot.appendChild(insideShadowRootElement);

      // Add the used elements to the cache so that we know the unique reference.
      const customElementRef = nodeCache.getOrCreateNodeReference(
        customElement,
        seenNodeIds
      );
      const shadowRootRef = nodeCache.getOrCreateNodeReference(
        shadowRoot,
        seenNodeIds
      );
      const insideShadowRootElementRef = nodeCache.getOrCreateNodeReference(
        insideShadowRootElement,
        seenNodeIds
      );

      const dataSet = [
        {
          node: customElement,
          serializationOptions: {
            maxDomDepth: 1,
          },
          serialized: {
            type: "node",
            sharedId: customElementRef,
            value: {
              attributes: {},
              childNodeCount: 0,
              children: [],
              localName: `${mode}-custom-element`,
              namespaceURI: "http://www.w3.org/1999/xhtml",
              nodeType: 1,
              shadowRoot: {
                sharedId: shadowRootRef,
                type: "node",
                value: {
                  childNodeCount: 1,
                  mode,
                  nodeType: 11,
                },
              },
            },
          },
        },
        {
          node: customElement,
          serializationOptions: {
            includeShadowTree: "open",
            maxDomDepth: 1,
          },
          serialized: {
            type: "node",
            sharedId: customElementRef,
            value: {
              attributes: {},
              childNodeCount: 0,
              children: [],
              localName: `${mode}-custom-element`,
              namespaceURI: "http://www.w3.org/1999/xhtml",
              nodeType: 1,
              shadowRoot: {
                sharedId: shadowRootRef,
                type: "node",
                value: {
                  childNodeCount: 1,
                  mode,
                  nodeType: 11,
                  ...(mode === "open"
                    ? {
                        children: [
                          {
                            type: "node",
                            sharedId: insideShadowRootElementRef,
                            value: {
                              nodeType: 1,
                              localName: "input",
                              namespaceURI: "http://www.w3.org/1999/xhtml",
                              childNodeCount: 0,
                              attributes: {},
                              shadowRoot: null,
                            },
                          },
                        ],
                      }
                    : {}),
                },
              },
            },
          },
        },
        {
          node: customElement,
          serializationOptions: {
            includeShadowTree: "all",
            maxDomDepth: 1,
          },
          serialized: {
            type: "node",
            sharedId: customElementRef,
            value: {
              attributes: {},
              childNodeCount: 0,
              children: [],
              localName: `${mode}-custom-element`,
              namespaceURI: "http://www.w3.org/1999/xhtml",
              nodeType: 1,
              shadowRoot: {
                sharedId: shadowRootRef,
                type: "node",
                value: {
                  childNodeCount: 1,
                  mode,
                  nodeType: 11,
                  children: [
                    {
                      type: "node",
                      sharedId: insideShadowRootElementRef,
                      value: {
                        nodeType: 1,
                        localName: "input",
                        namespaceURI: "http://www.w3.org/1999/xhtml",
                        childNodeCount: 0,
                        attributes: {},
                        shadowRoot: null,
                      },
                    },
                  ],
                },
              },
            },
          },
        },
      ];

      for (const { node, serializationOptions, serialized } of dataSet) {
        const { maxDomDepth, includeShadowTree } = serializationOptions;
        info(
          `Checking shadow root with maxDomDepth ${maxDomDepth} and includeShadowTree ${includeShadowTree}`
        );

        const serializationInternalMap = new Map();

        const serializedValue = serialize(
          node,
          serializationOptions,
          "none",
          serializationInternalMap,
          realm,
          { nodeCache }
        );

        Assert.deepEqual(serializedValue, serialized, "Got expected structure");
      }
    }
  });
});

add_task(async function test_serializeNodeSharedId() {
  await loadURL(inline("<div>"));

  await runTestInContent(() => {
    const domEl = content.document.querySelector("div");

    // Already add the domEl to the cache so that we know the unique reference.
    const domElRef = nodeCache.getOrCreateNodeReference(domEl, seenNodeIds);

    const serializedValue = serialize(
      domEl,
      { maxDomDepth: 0 },
      "root",
      serializationInternalMap,
      realm,
      { nodeCache, seenNodeIds }
    );

    Assert.equal(nodeCache.size, 1, "No additional reference added");
    Assert.equal(serializedValue.sharedId, domElRef);
    Assert.notEqual(serializedValue.handle, domElRef);
  });
});

function runTestInContent(callback) {
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [callback.toString()],
    async callback => {
      const { NodeCache } = ChromeUtils.importESModule(
        "chrome://remote/content/shared/webdriver/NodeCache.sys.mjs"
      );
      const { Realm, WindowRealm } = ChromeUtils.importESModule(
        "chrome://remote/content/shared/Realm.sys.mjs"
      );
      const { deserialize, serialize, setDefaultSerializationOptions } =
        ChromeUtils.importESModule(
          "chrome://remote/content/webdriver-bidi/RemoteValue.sys.mjs"
        );

      function assertInternalIds(serializationInternalMap, amount) {
        const remoteValuesWithInternalIds = Array.from(
          serializationInternalMap.values()
        ).filter(remoteValue => !!remoteValue.internalId);

        Assert.equal(
          remoteValuesWithInternalIds.length,
          amount,
          "Got expected amount of internalIds in serializationInternalMap"
        );
      }

      const nodeCache = new NodeCache();
      const seenNodeIds = new Map();
      const realm = new WindowRealm(content);
      const serializationInternalMap = new Map();

      function serializeAndAssertRemoteValue(remoteValue) {
        const { value, serialized } = remoteValue;
        const serializationOptionsWithDefaults =
          setDefaultSerializationOptions();
        const serializationInternalMapWithNone = new Map();

        info(`Checking '${serialized.type}' with none ownershipType`);

        const serializedValue = serialize(
          value,
          serializationOptionsWithDefaults,
          "none",
          serializationInternalMapWithNone,
          realm,
          { nodeCache, seenNodeIds }
        );

        assertInternalIds(serializationInternalMapWithNone, 0);
        Assert.deepEqual(serialized, serializedValue, "Got expected structure");

        info(`Checking '${serialized.type}' with root ownershipType`);
        const serializationInternalMapWithRoot = new Map();
        const serializedWithRoot = serialize(
          value,
          serializationOptionsWithDefaults,
          "root",
          serializationInternalMapWithRoot,
          realm,
          { nodeCache, seenNodeIds }
        );

        assertInternalIds(serializationInternalMapWithRoot, 0);
        Assert.equal(
          typeof serializedWithRoot.handle,
          "string",
          "Got a handle property"
        );
        Assert.deepEqual(
          Object.assign({}, serialized, { handle: serializedWithRoot.handle }),
          serializedWithRoot,
          "Got expected structure, plus a generated handle id"
        );
      }

      // eslint-disable-next-line no-eval
      eval(`(${callback})()`);
    }
  );
}
