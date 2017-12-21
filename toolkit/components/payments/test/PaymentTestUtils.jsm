"use strict";

this.EXPORTED_SYMBOLS = ["PaymentTestUtils"];

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

this.PaymentTestUtils = {
  /**
   * Common content tasks functions to be used with ContentTask.spawn.
   */
  ContentTasks: {
    /* eslint-env mozilla/frame-script */
    /**
     * Add a completion handler to the existing `showPromise` to call .complete().
     * @returns {Object} representing the PaymentResponse
     */
    addCompletionHandler: async () => {
      let response = await content.showPromise;
      response.complete();
      return {
        response: response.toJSON(),
        // XXX: Bug NNN: workaround for `details` not being included in `toJSON`.
        methodDetails: response.details,
      };
    },

    /**
     * Create a new payment request and cache it as `rq`.
     *
     * @param {Object} args
     * @param {PaymentMethodData[]} methodData
     * @param {PaymentDetailsInit} details
     * @param {PaymentOptions} options
     */
    createRequest: ({methodData, details, options}) => {
      const rq = new content.PaymentRequest(methodData, details, options);
      content.rq = rq; // assign it so we can retrieve it later
    },

    /**
     * Create a new payment request cached as `rq` and then show it.
     *
     * @param {Object} args
     * @param {PaymentMethodData[]} methodData
     * @param {PaymentDetailsInit} details
     * @param {PaymentOptions} options
     */
    createAndShowRequest: ({methodData, details, options}) => {
      const rq = new content.PaymentRequest(methodData, details, options);
      content.rq = rq; // assign it so we can retrieve it later
      content.showPromise = rq.show();
    },
  },

  DialogContentTasks: {
    /**
     * Click the cancel button
     *
     * Don't await on this task since the cancel can close the dialog before
     * ContentTask can resolve the promise.
     *
     * @returns {undefined}
     */
    manuallyClickCancel: () => {
      content.document.getElementById("cancel").click();
    },

    /**
     * Do the minimum possible to complete the payment succesfully.
     * @returns {undefined}
     */
    completePayment: () => {
      content.document.getElementById("pay").click();
    },
  },

  /**
   * Common PaymentMethodData for testing
   */
  MethodData: {
    basicCard: {
      supportedMethods: "basic-card",
    },
  },

  /**
   * Common PaymentDetailsInit for testing
   */
  Details: {
    total60USD: {
      total: {
        label: "Total due",
        amount: { currency: "USD", value: "60.00" },
      },
    },
  },
};
