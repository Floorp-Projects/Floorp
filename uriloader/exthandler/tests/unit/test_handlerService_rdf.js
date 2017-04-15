/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests the nsIHandlerService interface using the RDF backend.
 */

let gHandlerService = gHandlerServiceRDF;
let unloadHandlerStore = unloadHandlerStoreRDF;
let deleteHandlerStore = deleteHandlerStoreRDF;
let copyTestDataToHandlerStore = copyTestDataToHandlerStoreRDF;

var scriptFile = do_get_file("common_test_handlerService.js");
Services.scriptloader.loadSubScript(NetUtil.newURI(scriptFile).spec);
