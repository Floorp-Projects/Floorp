/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// HeapSnapshot.prototype.takeCensus: test by: 'count' breakdown
//
// Ported from js/src/jit-test/tests/debug/Memory-takeCensus-08.js

function run_test() {
  let g = newGlobal();
  let dbg = new Debugger(g);

  g.eval(`
         var stuff = [];
         function add(n, c) {
           for (let i = 0; i < n; i++)
             stuff.push(c());
         }

         let count = 0;

         function obj() { return { count: count++ }; }
         obj.factor = 1;

         // This creates a closure (a function JSObject) that has captured
         // a Call object. So each call creates two items.
         function fun() { let v = count; return () => { return v; } }
         fun.factor = 2;

         function str() { return 'perambulator' + count++; }
         str.factor = 1;

         // Eval a fresh text each time, allocating:
         // - a fresh ScriptSourceObject
         // - a new JSScripts, not an eval cache hits
         // - a fresh prototype object
         // - a fresh Call object, since the eval makes 'ev' heavyweight
         // - the new function itself
         function ev()  {
           return eval(\`(function () { return \${ count++ } })\`);
         }
         ev.factor = 5;

         // A new object (1) with a new shape (2) with a new atom (3)
         function shape() { return { [ 'theobroma' + count++ ]: count }; }
         shape.factor = 3;
         `);

  let baseline = 0;
  function countIncreasedByAtLeast(n) {
    let oldBaseline = baseline;

    // Since a census counts only reachable objects, one might assume that calling
    // GC here would have no effect on the census results. But GC also throws away
    // JIT code and any objects it might be holding (template objects, say);
    // takeCensus reaches those. Shake everything loose that we can, to make the
    // census approximate reachability a bit more closely, and make our results a
    // bit more predictable.
    gc(g, 'shrinking');

    baseline = saveHeapSnapshotAndTakeCensus(dbg, { breakdown: { by: 'count' } }).count;
    return baseline >= oldBaseline + n;
  }

  countIncreasedByAtLeast(0);

  g.add(100, g.obj);
  ok(countIncreasedByAtLeast(g.obj.factor * 100));

  g.add(100, g.fun);
  ok(countIncreasedByAtLeast(g.fun.factor * 100));

  g.add(100, g.str);
  ok(countIncreasedByAtLeast(g.str.factor * 100));

  g.add(100, g.ev);
  ok(countIncreasedByAtLeast(g.ev.factor * 100));

  g.add(100, g.shape);
  ok(countIncreasedByAtLeast(g.shape.factor * 100));

  do_test_finished();
}
