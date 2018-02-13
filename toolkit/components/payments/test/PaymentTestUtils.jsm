"use strict";

this.EXPORTED_SYMBOLS = ["PaymentTestUtils"];

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

    addShippingChangeHandler: ({details}) => {
      content.rq.onshippingaddresschange = ev => {
        ev.updateWith(details);
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
    getShippingOptions: () => {
      let select = content.document.querySelector("shipping-option-picker > rich-select");
      let popupBox = Cu.waiveXrays(select).popupBox;
      let selectedOptionIndex = Array.from(popupBox.children)
                                     .findIndex(item => item.hasAttribute("selected"));
      let selectedOption = popupBox.children[selectedOptionIndex];
      let currencyAmount = selectedOption.querySelector("currency-amount");
      return {
        optionCount: popupBox.children.length,
        selectedOptionIndex,
        selectedOptionValue: selectedOption.getAttribute("value"),
        selectedOptionLabel: selectedOption.getAttribute("label"),
        selectedOptionCurrency: currencyAmount.getAttribute("currency"),
        selectedOptionAmount: currencyAmount.getAttribute("amount"),
      };
    },

    selectShippingAddressByCountry: country => {
      let doc = content.document;
      let addressPicker =
        doc.querySelector("address-picker[selected-state-key='selectedShippingAddress']");
      let select = addressPicker.querySelector("rich-select");
      let option = select.querySelector(`[country="${country}"]`);
      select.click();
      option.click();
    },

    selectShippingOptionById: value => {
      let doc = content.document;
      let optionPicker =
        doc.querySelector("shipping-option-picker");
      let select = optionPicker.querySelector("rich-select");
      let option = select.querySelector(`[value="${value}"]`);
      select.click();
      option.click();
    },

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

    setSecurityCode: ({securityCode}) => {
      // Waive the xray to access the untrusted `securityCodeInput` property
      let picker = Cu.waiveXrays(content.document.querySelector("payment-method-picker"));
      // Unwaive to access the ChromeOnly `setUserInput` API.
      // setUserInput dispatches changes events.
      Cu.unwaiveXrays(picker.securityCodeInput).setUserInput(securityCode);
    },
  },

  /**
   * Common PaymentMethodData for testing
   */
  MethodData: {
    basicCard: {
      supportedMethods: "basic-card",
    },
    bobPay: {
      supportedMethods: "https://www.example.com/bobpay",
    },
  },

  /**
   * Common PaymentDetailsInit for testing
   */
  Details: {
    total60USD: {
      shippingOptions: [
        {
          id: "standard",
          label: "Ground Shipping (2 days)",
          amount: { currency: "USD", value: "5.00" },
          selected: true,
        },
        {
          id: "drone",
          label: "Drone Express (2 hours)",
          amount: { currency: "USD", value: "25.00" },
        },
      ],
      total: {
        label: "Total due",
        amount: { currency: "USD", value: "60.00" },
      },
    },
    total60EUR: {
      shippingOptions: [
        {
          id: "standard",
          label: "Ground Shipping (4 days)",
          amount: { currency: "EUR", value: "7.00" },
          selected: true,
        },
        {
          id: "drone",
          label: "Drone Express (8 hours)",
          amount: { currency: "EUR", value: "29.00" },
        },
      ],
      total: {
        label: "Total due",
        amount: { currency: "EUR", value: "75.00" },
      },
    },
    twoDisplayItems: {
      total: {
        label: "Total due",
        amount: { currency: "USD", value: "32.00" },
      },
      displayItems: [
        {
          label: "First",
          amount: { currency: "USD", value: "1" },
        },
        {
          label: "Second",
          amount: { currency: "USD", value: "2" },
        },
      ],
    },
    twoShippingOptions: {
      total: {
        label: "Total due",
        amount: { currency: "USD", value: "2.00" },
      },
      shippingOptions: [
        {
          id: "1",
          label: "Meh Unreliable Shipping",
          amount: { currency: "USD", value: "1" },
        },
        {
          id: "2",
          label: "Premium Slow Shipping",
          amount: { currency: "USD", value: "2" },
          selected: true,
        },
      ],
    },
    twoShippingOptionsEUR: {
      total: {
        label: "Total due",
        amount: { currency: "EUR", value: "1.75" },
      },
      shippingOptions: [
        {
          id: "1",
          label: "Sloth Shipping",
          amount: { currency: "EUR", value: "1.01" },
        },
        {
          id: "2",
          label: "Velociraptor Shipping",
          amount: { currency: "EUR", value: "63545.65" },
          selected: true,
        },
      ],
    },
    bobPayPaymentModifier: {
      total: {
        label: "Total due",
        amount: { currency: "USD", value: "2.00" },
      },
      displayItems: [
        {
          label: "First",
          amount: { currency: "USD", value: "1" },
        },
      ],
      modifiers: [
        {
          additionalDisplayItems: [
            {
              label: "Credit card fee",
              amount: { currency: "USD", value: "0.50" },
            },
          ],
          supportedMethods: "basic-card",
          total: {
            label: "Total due",
            amount: { currency: "USD", value: "2.50" },
          },
          data: {
            supportedTypes: "credit",
          },
        },
        {
          additionalDisplayItems: [
            {
              label: "Bob-pay fee",
              amount: { currency: "USD", value: "1.50" },
            },
          ],
          supportedMethods: "https://www.example.com/bobpay",
          total: {
            label: "Total due",
            amount: { currency: "USD", value: "3.50" },
          },
        },
      ],
    },
  },

  Options: {
    requestShippingOption: {
      requestShipping: true,
    },
  },
};
