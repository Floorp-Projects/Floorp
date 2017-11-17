"use strict";

/* eslint-disable mozilla/no-cpows-in-tests */
add_task(async function test_total() {
  const testTask = ({methodData, details}) => {
    is(content.document.querySelector("#total > currency-amount").textContent,
       "$60.00",
       "Check total currency amount");
  };
  const args = {
    methodData: [PTU.MethodData.basicCard],
    details: PTU.Details.total60USD,
  };
  await spawnInDialogForMerchantTask(PTU.ContentTasks.createRequest, testTask, args);
});
