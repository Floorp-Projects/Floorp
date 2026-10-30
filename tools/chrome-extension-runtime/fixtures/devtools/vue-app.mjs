// SPDX-License-Identifier: MPL-2.0
/** @import {VueFixtureGlobal,VueComponent,VueCountProps} from "../../vue-fixture-types.ts" */
const vueFixtureGlobals =
  /** @type {VueFixtureGlobal} */ (/** @type {unknown} */ (globalThis));
const { Vue, document } = vueFixtureGlobals;
if (Vue.version !== "3.5.44") {
  throw Error("Unexpected actual Vue framework version");
}
/** @type {VueComponent<VueCountProps>} */
const FloorpVueCounterLabel = {
  name: "FloorpVueCounterLabel",
  props: { count: Number },
  setup(props) {
    return () =>
      Vue.h("output", { id: "vue-count" }, `Vue count: ${props.count}`);
  },
};
/** @type {VueComponent<Record<string,never>>} */
const FloorpVueCounter = {
  name: "FloorpVueCounter",
  setup() {
    const count = Vue.ref(0);
    return () =>
      Vue.h("section", null, [
        Vue.h("h2", null, "Floorp authored actual Vue counter"),
        Vue.h(FloorpVueCounterLabel, { count: count.value }),
        Vue.h("button", {
          id: "vue-increment",
          onClick: () => count.value++,
        }, "Increment actual Vue state"),
      ]);
  },
};
const root = document.getElementById("vue-root");
if (!root) throw Error("Authored Vue root is missing");
Vue.createApp(FloorpVueCounter).mount(root);
export {};
