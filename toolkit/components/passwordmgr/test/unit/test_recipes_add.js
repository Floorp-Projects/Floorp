/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests adding and retrieving LoginRecipes in the parent process.
 */

"use strict";

add_task(function* test_init() {
  let parent = new LoginRecipesParent({ defaults: null });
  let initPromise1 = parent.initializationPromise;
  let initPromise2 = parent.initializationPromise;
  Assert.strictEqual(initPromise1, initPromise2, "Check that the same promise is returned");

  let recipesParent = yield initPromise1;
  Assert.ok(recipesParent instanceof LoginRecipesParent, "Check init return value");
  Assert.strictEqual(recipesParent._recipesByHost.size, 0, "Initially 0 recipes");
});

add_task(function* test_get_missing_host() {
  let recipesParent = yield RecipeHelpers.initNewParent();
  let exampleRecipes = recipesParent.getRecipesForHost("example.invalid");
  Assert.strictEqual(exampleRecipes.size, 0, "Check recipe count for example.invalid");

});

add_task(function* test_add_get_simple_host() {
  let recipesParent = yield RecipeHelpers.initNewParent();
  Assert.strictEqual(recipesParent._recipesByHost.size, 0, "Initially 0 recipes");
  recipesParent.add({
    hosts: ["example.com"],
  });
  Assert.strictEqual(recipesParent._recipesByHost.size, 1,
                     "Check number of hosts after the addition");

  let exampleRecipes = recipesParent.getRecipesForHost("example.com");
  Assert.strictEqual(exampleRecipes.size, 1, "Check recipe count for example.com");
  let recipe = [...exampleRecipes][0];
  Assert.strictEqual(typeof(recipe), "object", "Check recipe type");
  Assert.strictEqual(recipe.hosts.length, 1, "Check that one host is present");
  Assert.strictEqual(recipe.hosts[0], "example.com", "Check the one host");
});

add_task(function* test_add_get_non_standard_port_host() {
  let recipesParent = yield RecipeHelpers.initNewParent();
  recipesParent.add({
    hosts: ["example.com:8080"],
  });
  Assert.strictEqual(recipesParent._recipesByHost.size, 1,
                     "Check number of hosts after the addition");

  let exampleRecipes = recipesParent.getRecipesForHost("example.com:8080");
  Assert.strictEqual(exampleRecipes.size, 1, "Check recipe count for example.com:8080");
  let recipe = [...exampleRecipes][0];
  Assert.strictEqual(typeof(recipe), "object", "Check recipe type");
  Assert.strictEqual(recipe.hosts.length, 1, "Check that one host is present");
  Assert.strictEqual(recipe.hosts[0], "example.com:8080", "Check the one host");
});

add_task(function* test_add_multiple_hosts() {
  let recipesParent = yield RecipeHelpers.initNewParent();
  recipesParent.add({
    hosts: ["example.com", "foo.invalid"],
  });
  Assert.strictEqual(recipesParent._recipesByHost.size, 2,
                     "Check number of hosts after the addition");

  let exampleRecipes = recipesParent.getRecipesForHost("example.com");
  Assert.strictEqual(exampleRecipes.size, 1, "Check recipe count for example.com");
  let recipe = [...exampleRecipes][0];
  Assert.strictEqual(typeof(recipe), "object", "Check recipe type");
  Assert.strictEqual(recipe.hosts.length, 2, "Check that two hosts are present");
  Assert.strictEqual(recipe.hosts[0], "example.com", "Check the first host");
  Assert.strictEqual(recipe.hosts[1], "foo.invalid", "Check the second host");

  let fooRecipes = recipesParent.getRecipesForHost("foo.invalid");
  Assert.strictEqual(fooRecipes.size, 1, "Check recipe count for foo.invalid");
  let fooRecipe = [...fooRecipes][0];
  Assert.strictEqual(fooRecipe, recipe, "Check that the recipe is shared");
  Assert.strictEqual(typeof(fooRecipe), "object", "Check recipe type");
  Assert.strictEqual(fooRecipe.hosts.length, 2, "Check that two hosts are present");
  Assert.strictEqual(fooRecipe.hosts[0], "example.com", "Check the first host");
  Assert.strictEqual(fooRecipe.hosts[1], "foo.invalid", "Check the second host");
});

add_task(function* test_add_pathRegex() {
  let recipesParent = yield RecipeHelpers.initNewParent();
  recipesParent.add({
    hosts: ["example.com"],
    pathRegex: /^\/mypath\//,
  });
  Assert.strictEqual(recipesParent._recipesByHost.size, 1,
                     "Check number of hosts after the addition");

  let exampleRecipes = recipesParent.getRecipesForHost("example.com");
  Assert.strictEqual(exampleRecipes.size, 1, "Check recipe count for example.com");
  let recipe = [...exampleRecipes][0];
  Assert.strictEqual(typeof(recipe), "object", "Check recipe type");
  Assert.strictEqual(recipe.hosts.length, 1, "Check that one host is present");
  Assert.strictEqual(recipe.hosts[0], "example.com", "Check the one host");
  Assert.strictEqual(recipe.pathRegex.toString(), "/^\\/mypath\\//", "Check the pathRegex");
});

add_task(function* test_add_selectors() {
  let recipesParent = yield RecipeHelpers.initNewParent();
  recipesParent.add({
    hosts: ["example.com"],
    usernameSelector: "#my-username",
    passwordSelector: "#my-form > input.password",
  });
  Assert.strictEqual(recipesParent._recipesByHost.size, 1,
                     "Check number of hosts after the addition");

  let exampleRecipes = recipesParent.getRecipesForHost("example.com");
  Assert.strictEqual(exampleRecipes.size, 1, "Check recipe count for example.com");
  let recipe = [...exampleRecipes][0];
  Assert.strictEqual(typeof(recipe), "object", "Check recipe type");
  Assert.strictEqual(recipe.hosts.length, 1, "Check that one host is present");
  Assert.strictEqual(recipe.hosts[0], "example.com", "Check the one host");
  Assert.strictEqual(recipe.usernameSelector, "#my-username", "Check the usernameSelector");
  Assert.strictEqual(recipe.passwordSelector, "#my-form > input.password", "Check the passwordSelector");
});

/* Begin checking errors with add */

add_task(function* test_add_missing_prop() {
  let recipesParent = yield RecipeHelpers.initNewParent();
  Assert.throws(() => recipesParent.add({}), /required/, "Some properties are required");
});

add_task(function* test_add_unknown_prop() {
  let recipesParent = yield RecipeHelpers.initNewParent();
  Assert.throws(() => recipesParent.add({
    unknownProp: true,
  }), /supported/, "Unknown properties should cause an error to help with typos");
});

add_task(function* test_add_invalid_hosts() {
  let recipesParent = yield RecipeHelpers.initNewParent();
  Assert.throws(() => recipesParent.add({
    hosts: 404,
  }), /array/, "hosts should be an array");
});

add_task(function* test_add_empty_host_array() {
  let recipesParent = yield RecipeHelpers.initNewParent();
  Assert.throws(() => recipesParent.add({
    hosts: [],
  }), /array/, "hosts should be a non-empty array");
});

add_task(function* test_add_pathRegex_non_regexp() {
  let recipesParent = yield RecipeHelpers.initNewParent();
  Assert.throws(() => recipesParent.add({
    hosts: ["example.com"],
    pathRegex: "foo",
  }), /regular expression/, "pathRegex should be a RegExp");
});

add_task(function* test_add_usernameSelector_non_string() {
  let recipesParent = yield RecipeHelpers.initNewParent();
  Assert.throws(() => recipesParent.add({
    hosts: ["example.com"],
    usernameSelector: 404,
  }), /string/, "usernameSelector should be a string");
});

add_task(function* test_add_passwordSelector_non_string() {
  let recipesParent = yield RecipeHelpers.initNewParent();
  Assert.throws(() => recipesParent.add({
    hosts: ["example.com"],
    passwordSelector: 404,
  }), /string/, "passwordSelector should be a string");
});

/* End checking errors with add */
