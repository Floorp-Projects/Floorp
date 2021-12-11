try {
 await import('./bad_local_export.js');
} catch(ns) {
  throw Error("Fails as expected");
};
