/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1713725 - Shim Rich Relevance personalized shopping
 *
 * Sites may expect the Rich Relevance personalized shopping API to load,
 * breaking if it is blocked. This shim attempts to limit breakage on those
 * site to just the personalized shopping aspects, by stubbing out the APIs.
 */

if (!window.r3_common) {
  const jsonCallback = window.RR?.jsonCallback;
  const defaultCallback = window.RR?.defaultCallback;

  const getRandomString = (l = 66) => {
    const v = crypto.getRandomValues(new Uint8Array(l));
    const s = Array.from(v, c => c.toString(16)).join("");
    return s.slice(0, l);
  };

  const call = (fn, ...args) => {
    if (typeof fn === "function") {
      try {
        fn(...args);
      } catch (e) {
        console.error(e);
      }
    }
  };

  class r3_generic {
    type = "GENERIC";
    createScript() {}
    destroy() {}
  }

  class r3_addtocart extends r3_generic {
    type = "ADDTOCART";
    addItemIdToCart() {}
  }

  class r3_addtoregistry extends r3_generic {
    type = "ADDTOREGISTRY";
    addItemIdCentsQuantity() {}
  }

  class r3_brand extends r3_generic {
    type = "BRAND";
  }

  class r3_cart extends r3_generic {
    type = "CART";
    addItemId() {}
    addItemIdCentsQuantity() {}
    addItemIdDollarsAndCentsQuantity() {}
    addItemIdPriceQuantity() {}
  }

  class r3_category extends r3_generic {
    type = "CATEGORY";
    addItemId() {}
    setId() {}
    setName() {}
    setParentId() {}
    setTopName() {}
  }

  class r3_common extends r3_generic {
    type = "COMMON";
    baseUrl = "https://recs.richrelevance.com/rrserver/";
    devFlags = {};
    jsFileName = "p13n_generated.js";
    RICHSORT = {
      paginate() {},
      filterPrice() {},
      filterAttribute() {},
    };
    addCategoryHintId() {}
    addClickthruParams() {}
    addContext() {}
    addFilter() {}
    addFilterBrand() {}
    addFilterCategory() {}
    addItemId() {}
    addItemIdToCart() {}
    addPlacementType() {}
    addRefinement() {}
    addSearchTerm() {}
    addSegment() {}
    blockItemId() {}
    enableCfrad() {}
    enableRad() {}
    forceDebugMode() {}
    forceDevMode() {}
    forceDisplayMode() {}
    forceLocale() {}
    initFromParams() {}
    setApiKey() {}
    setBaseUrl() {}
    setCartValue() {}
    setChannel() {}
    setClickthruServer() {}
    setCurrency() {}
    setDeviceId() {}
    setFilterBrandsIncludeMatchingElements() {}
    setForcedTreatment() {}
    setImageServer() {}
    setLanguage() {}
    setMVTForcedTreatment() {}
    setNoCookieMode() {}
    setPageBrand() {}
    setPrivateMode() {}
    setRefinementFallback() {}
    setRegionId() {}
    setRegistryId() {}
    setRegistryType() {}
    setSessionId() {}
    setUserId() {}
    useDummyData() {}
  }

  class r3_error extends r3_generic {
    type = "ERROR";
  }

  class r3_home extends r3_generic {
    type = "HOME";
  }

  class r3_item extends r3_generic {
    type = "ITEM";
    addAttribute() {}
    addCategory() {}
    addCategoryId() {}
    setBrand() {}
    setEndDate() {}
    setId() {}
    setImageId() {}
    setLinkId() {}
    setName() {}
    setPrice() {}
    setRating() {}
    setRecommendable() {}
    setReleaseDate() {}
    setSalePrice() {}
  }

  class r3_personal extends r3_generic {
    type = "PERSONAL";
  }

  class r3_purchased extends r3_generic {
    type = "PURCHASED";
    addItemId() {}
    addItemIdCentsQuantity() {}
    addItemIdDollarsAndCentsQuantity() {}
    addItemIdPriceQuantity() {}
    setOrderNumber() {}
    setPromotionCode() {}
    setShippingCost() {}
    setTaxes() {}
    setTotalPrice() {}
  }

  class r3_search extends r3_generic {
    type = "SEARCH";
    addItemId() {}
    setTerms() {}
  }

  class r3_wishlist extends r3_generic {
    type = "WISHLIST";
    addItemId() {}
  }

  const RR = {
    add() {},
    addItemId() {},
    addItemIdCentsQuantity() {},
    addItemIdDollarsAndCentsQuantity() {},
    addItemIdPriceQuantity() {},
    addItemIdToCart() {},
    addObject() {},
    addSearchTerm() {},
    c() {},
    charset: "UTF-8",
    checkParamCookieValue: () => null,
    d: document,
    data: {
      JSON: {
        placements: [],
      },
    },
    debugWindow() {},
    set defaultCallback(fn) {
      call(fn);
    },
    fixName: n => n,
    genericAddItemPriceQuantity() {},
    get() {},
    getDomElement(a) {
      return typeof a === "string" && a ? document.querySelector(a) : null;
    },
    id() {},
    insert() {},
    insertDynamicPlacement() {},
    isArray: a => Array.isArray(a),
    js() {},
    set jsonCallback(fn) {
      call(fn, {});
    },
    l: document.location.href,
    lc() {},
    noCookieMode: false,
    ol() {},
    onloadCalled: true,
    pq() {},
    rcsCookieDefaultDuration: 364,
    registerPageType() {},
    registeredPageTypes: {
      ADDTOCART: r3_addtocart,
      ADDTOREGISTRY: r3_addtoregistry,
      BRAND: r3_brand,
      CART: r3_cart,
      CATEGORY: r3_category,
      COMMON: r3_common,
      ERROR: r3_error,
      GENERIC: r3_generic,
      HOME: r3_home,
      ITEM: r3_item,
      PERSONAL: r3_personal,
      PURCHASED: r3_purchased,
      SEARCH: r3_search,
      WISHLIST: r3_wishlist,
    },
    renderDynamicPlacements() {},
    set() {},
    setCharset() {},
    U: "undefined",
    unregisterAllPageType() {},
    unregisterPageType() {},
  };

  Object.assign(window, {
    r3() {},
    r3_addtocart,
    r3_addtoregistry,
    r3_brand,
    r3_cart,
    r3_category,
    r3_common,
    r3_error,
    r3_generic,
    r3_home,
    r3_item,
    r3_personal,
    r3_placement() {},
    r3_purchased,
    r3_search,
    r3_wishlist,
    RR,
    rr_addLoadEvent() {},
    rr_annotations_array: [undefined],
    rr_call_after_flush() {},
    rr_create_script() {},
    rr_dynamic: {
      placements: [],
    },
    rr_flush() {},
    rr_flush_onload() {},
    rr_insert_placement() {},
    rr_onload_called: true,
    rr_placement_place_holders: [],
    rr_placements: [],
    rr_recs: {
      placements: [],
    },
    rr_remote_data: getRandomString(),
    rr_v: "1.2.6.20210212",
  });

  call(jsonCallback);
  call(defaultCallback, {});
}
