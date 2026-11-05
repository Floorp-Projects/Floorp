// Set FLOORP_SOURCE to the fix checkout and ESBUILD_MODULE if esbuild is not
// resolvable from its node_modules. QA_CSS_OLD=1 builds the baseline CSS fixture.
const fs = require('node:fs');
const path = require('node:path');
const cp = require('node:child_process');
const source = path.resolve(process.env.FLOORP_SOURCE || process.cwd());
const esbuild = require(process.env.ESBUILD_MODULE || require.resolve('esbuild', { paths: [source] }));
const relativeCss = 'browser-features/chrome/common/split-view/styles/split-view.css';
const baseline = process.env.QA_BASE_SHA || '984509d2053be7bae9027133bda7259cd706b208';
esbuild.build({
  entryPoints: [path.join(source, 'browser-features/chrome/common/split-view/test/splitViewAdoption.test.ts')],
  bundle: true,
  format: 'iife',
  globalName: 'qaKan3Test',
  outfile: process.argv[2] || path.join(__dirname, 'regression-new.js'),
  plugins: [{ name: 'inline-css', setup(build) {
    build.onResolve({ filter: /\.css\?inline$/ }, args => ({
      path: path.resolve(args.resolveDir, args.path.replace(/\?inline$/, '')),
      namespace: 'inline-css',
    }));
    build.onLoad({ filter: /.*/, namespace: 'inline-css' }, args => ({
      contents: process.env.QA_CSS_OLD
        ? cp.execFileSync('git', ['-C', source, 'show', `${baseline}:${relativeCss}`], { encoding: 'utf8' })
        : fs.readFileSync(args.path, 'utf8'),
      loader: 'text',
    }));
  } }],
}).catch(() => { process.exitCode = 1; });
