/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ShoppingProduct, isProductURL } = ChromeUtils.importESModule(
  "chrome://global/content/shopping/ShoppingProduct.mjs"
);

add_task(function test_product_fromUrl() {
  Assert.deepEqual(
    ShoppingProduct.fromURL(),
    { valid: false },
    "Passing a nothing returns empty result object"
  );

  Assert.deepEqual(
    ShoppingProduct.fromURL(12345),
    { valid: false },
    "Passing a number returns empty result object"
  );

  Assert.deepEqual(
    ShoppingProduct.fromURL("https://www.walmart.com/ip/926485654"),
    { valid: false },
    "String urls returns empty result object"
  );

  Assert.deepEqual(
    ShoppingProduct.fromURL(new URL("https://www.mozilla.org")),
    { host: "mozilla.org", valid: false },
    "Invalid Url returns a full result object"
  );

  Assert.deepEqual(
    ShoppingProduct.fromURL(new URL("https://www.walmart.com/ip/926485654"))
      .host,
    "walmart.com",
    "WWW in host is ignored"
  );

  Assert.deepEqual(
    ShoppingProduct.fromURL(
      new URL("https://staging.walmart.com/ip/926485654")
    ),
    { host: "staging.walmart.com", valid: false },
    "Subdomain in valid Product Url returns partial result"
  );

  Assert.deepEqual(
    ShoppingProduct.fromURL(new URL("https://walmart.co.uk/ip/926485654")),
    { host: "walmart.co.uk", sitename: "walmart", valid: false },
    "Invalid in Product TLD returns partial result"
  );

  Assert.deepEqual(
    ShoppingProduct.fromURL(new URL("https://walmart.com")),
    { host: "walmart.com", sitename: "walmart", tld: "com", valid: false },
    "Non-Product page returns partial result"
  );

  Assert.deepEqual(
    ShoppingProduct.fromURL(new URL("https://walmart.com/ip/926485654")),
    {
      host: "walmart.com",
      sitename: "walmart",
      tld: "com",
      id: "926485654",
      valid: true,
    },
    "Valid Product Url returns a full result object"
  );

  Assert.deepEqual(
    ShoppingProduct.fromURL(new URL("http://walmart.com/ip/926485654")),
    {
      host: "walmart.com",
      sitename: "walmart",
      tld: "com",
      id: "926485654",
      valid: true,
    },
    "Protocol is not checked"
  );
});

add_task(function test_product_isProduct() {
  let product = {
    host: "walmart.com",
    sitename: "walmart",
    tld: "com",
    id: "926485654",
    valid: true,
  };
  Assert.equal(
    ShoppingProduct.isProduct(product),
    true,
    "Passing a Product object returns true"
  );
  Assert.equal(
    ShoppingProduct.isProduct({ host: "walmart.com", sitename: "walmart" }),
    false,
    "Passing an incomplete ShoppingProduct object returns false"
  );
  Assert.equal(
    ShoppingProduct.isProduct(),
    false,
    "Passing nothing returns false"
  );
});

add_task(function test_amazon_product_urls() {
  let product;
  let url_com = new URL(
    "https://www.amazon.com/Furmax-Electric-Adjustable-Standing-Computer/dp/B09TJGHL5F/"
  );
  let url_ca = new URL(
    "https://www.amazon.ca/JBL-Flip-Essential-Waterproof-Bluetooth/dp/B0C3NNGWFN/"
  );
  let url_uk = new URL(
    "https://www.amazon.co.uk/placeholder_title/dp/B0B8KGPHS7/"
  );
  let url_content = new URL("https://www.amazon.com/stores/node/20648519011");

  product = ShoppingProduct.fromURL(url_com);
  Assert.equal(ShoppingProduct.isProduct(product), true, "Url is a product");
  Assert.equal(product.id, "B09TJGHL5F", "Product id was found in Url");

  product = ShoppingProduct.fromURL(url_ca);
  Assert.equal(
    ShoppingProduct.isProduct(product),
    false,
    "Url is not a supported tld"
  );

  product = ShoppingProduct.fromURL(url_uk);
  Assert.equal(
    ShoppingProduct.isProduct(product),
    false,
    "Url is not a supported tld"
  );

  product = ShoppingProduct.fromURL(url_content);
  Assert.equal(
    ShoppingProduct.isProduct(product),
    false,
    "Url is not a product"
  );
});

add_task(function test_walmart_product_urls() {
  let product;
  let url_com = new URL(
    "https://www.walmart.com/ip/Kent-Bicycles-29-Men-s-Trouvaille-Mountain-Bike-Medium-Black-and-Taupe/823391155"
  );
  let url_ca = new URL(
    "https://www.walmart.ca/en/ip/cherries-jumbo/6000187473587"
  );
  let url_content = new URL(
    "https://www.walmart.com/browse/food/grilling-foods/976759_1567409_8808777"
  );

  product = ShoppingProduct.fromURL(url_com);
  Assert.equal(ShoppingProduct.isProduct(product), true, "Url is a product");
  Assert.equal(product.id, "823391155", "Product id was found in Url");

  product = ShoppingProduct.fromURL(url_ca);
  Assert.equal(
    ShoppingProduct.isProduct(product),
    false,
    "Url is not a valid tld"
  );

  product = ShoppingProduct.fromURL(url_content);
  Assert.equal(
    ShoppingProduct.isProduct(product),
    false,
    "Url is not a product"
  );
});

add_task(function test_bestbuy_product_urls() {
  let product;
  let url_com = new URL(
    "https://www.bestbuy.com/site/ge-profile-ultrafast-4-8-cu-ft-large-capacity-all-in-one-washer-dryer-combo-with-ventless-heat-pump-technology-carbon-graphite/6530134.p?skuId=6530134"
  );
  let url_ca = new URL(
    "https://www.bestbuy.ca/en-ca/product/segway-ninebot-kickscooter-f40-electric-scooter-40km-range-30km-h-top-speed-dark-grey/15973012"
  );
  let url_content = new URL(
    "https://www.bestbuy.com/site/home-appliances/major-appliances-sale-event/pcmcat321600050000.c"
  );

  product = ShoppingProduct.fromURL(url_com);
  Assert.equal(ShoppingProduct.isProduct(product), true, "Url is a product");
  Assert.equal(product.id, "6530134.p", "Product id was found in Url");

  product = ShoppingProduct.fromURL(url_ca);
  Assert.equal(
    ShoppingProduct.isProduct(product),
    false,
    "Url is not a valid tld"
  );

  product = ShoppingProduct.fromURL(url_content);
  Assert.equal(
    ShoppingProduct.isProduct(product),
    false,
    "Url is not a product"
  );
});

add_task(function test_isProductURL() {
  let product_string =
    "https://www.amazon.com/Furmax-Electric-Adjustable-Standing-Computer/dp/B09TJGHL5F/";
  let product_url = new URL(product_string);
  let product_uri = Services.io.newURI(product_string);
  Assert.equal(
    isProductURL(product_url),
    true,
    "Passing a product URL returns true"
  );
  Assert.equal(
    isProductURL(product_uri),
    true,
    "Passing a product URI returns true"
  );

  let content_string =
    "https://www.walmart.com/browse/food/grilling-foods/976759_1567409_8808777";
  let content_url = new URL(content_string);
  let content_uri = Services.io.newURI(content_string);
  Assert.equal(
    isProductURL(content_url),
    false,
    "Passing a content URL returns false"
  );
  Assert.equal(
    isProductURL(content_uri),
    false,
    "Passing a content URI returns false"
  );

  Assert.equal(isProductURL(), false, "Passing nothing returns false");

  Assert.equal(isProductURL(1234), false, "Passing a number returns false");

  Assert.equal(
    isProductURL("1234"),
    false,
    "Passing a junk string returns false"
  );
});

add_task(function test_new_ShoppingProduct() {
  let product_string =
    "https://www.amazon.com/Furmax-Electric-Adjustable-Standing-Computer/dp/B09TJGHL5F/";
  let product_url = new URL(product_string);
  let product_uri = Services.io.newURI(product_string);
  let productURL = new ShoppingProduct(product_url);
  Assert.equal(
    productURL.isProduct(),
    true,
    "Passing a product URL returns a valid product"
  );
  let productURI = new ShoppingProduct(product_uri);
  Assert.equal(
    productURI.isProduct(),
    true,
    "Passing a product URI returns a valid product"
  );
});
