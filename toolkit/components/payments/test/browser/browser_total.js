"use strict";

/* eslint-disable mozilla/no-cpows-in-tests */
add_task(async function test_total() {
  const testTask = ({methodData, details}) => {
    is(content.document.querySelector("#total > .value").textContent,
       details.total.amount.value,
       "Check total value");
    is(content.document.querySelector("#total > .currency").textContent,
       details.total.amount.currency,
       "Check currency");
  };
  const args = {
    methodData: [PTU.MethodData.basicCard],
    details: PTU.Details.total60USD,
  };
  await spawnInDialogForMerchantTask(PTU.ContentTasks.createRequest, testTask, args);
});
