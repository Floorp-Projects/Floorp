// SPDX-License-Identifier: MPL-2.0
// Input to genuine, frozen extension engines; this module implements no macro engine.
export const uiVisionId = "gcbalfbdmfieckjlnblleoemohcganoc";
export const expectedCustomer = "Frozen package 42";
/** @param {string} url @returns {import("./automation-test-types.ts").VisionMacro} */
export function uiVisionMacro(url) {
  return {
    Name: "Floorp frozen package primary operation",
    CreationDate: "2026-10-19",
    Commands: [
      { Command: "open", Target: url, Value: "" },
      { Command: "type", Target: "id=customer", Value: expectedCustomer },
      { Command: "click", Target: "id=submit-operation", Value: "" },
      {
        Command: "assertText",
        Target: "id=operation-result",
        Value: `Submitted: ${expectedCustomer}`,
      },
    ],
  };
}
export const automaId = "infppggnoaenmfagbfknfkancpbljcca";
export const expectedAutomaCustomer = "Automa native 73195";
/** @param {import("./automation-test-types.ts").AutomaWorkflow} template @param {string} url @returns {import("./automation-test-types.ts").AutomaWorkflow} */
export function automaWorkflow(template, url) {
  const workflow = JSON.parse(JSON.stringify(template));
  workflow.id = "floorp-authored-primary";
  workflow.name = "Floorp authored macro";
  workflow.isProtected = false;
  workflow.isDisabled = false;
  workflow.createdAt = Date.now();
  workflow.updatedAt = Date.now();
  for (const node of workflow.drawflow.nodes) {
    if (node.label === "new-tab") node.data.url = url;
    if (node.label === "forms") {
      Object.assign(node.data, {
        selector: "#customer",
        value: expectedAutomaCustomer,
        delay: "20",
      });
    }
    if (node.label === "event-click") node.data.selector = "#submit-operation";
  }
  return workflow;
}
export const browserflowId = "hfjnppljknigdnnpocjjgdcfmnodoafe";

export const webscraperId = "jnhgnonknehpejjnehehllkliplmbmhn";
export const expectedProducts = [
  ["Apple", "3.25"],
  ["Pear", "4.50"],
];
/** @param {string} url @returns {import("./automation-test-types.ts").Sitemap} */
export function webscraperSitemap(url) {
  return {
    _id: "floorp-authored-products",
    startUrl: [url],
    selectors: [
      {
        id: "products",
        parentSelectors: ["_root"],
        type: "SelectorElement",
        selector: "#products tr",
        multiple: true,
      },
      {
        id: "name",
        parentSelectors: ["products"],
        type: "SelectorText",
        selector: ".name",
        multiple: false,
        regex: "",
      },
      {
        id: "price",
        parentSelectors: ["products"],
        type: "SelectorText",
        selector: ".price",
        multiple: false,
        regex: "",
      },
    ],
  };
}
