# !/bin/bash
set -xe

# grabbing and patching transformers.js for gecko
rm -rf tmp
mkdir tmp
pushd tmp
git clone --depth 1 --branch 2.16.1 https://github.com/xenova/transformers.js
cd transformers.js
git apply ../../gecko.patch
npm install
npm run build
cp dist/transformers.js ../../transformers-dev.js
cp dist/transformers.min.js ../../transformers.js
popd
rm -rf tmp

# grabbing and patching onnxruntime-web for gecko.

# fetch the tarball URL from npm
TARBALL_URL=$(npm view onnxruntime-web@1.14.0 dist.tarball)
wget "${TARBALL_URL}" -O dist.tgz

# grab the two files we need
tar -xzf dist.tgz
rm dist.tgz
cp package/dist/ort.js ort-dev.js
cp package/dist/ort.min.js ort.js
rm -rf package

# remove the eval() calls
sed -i '' '1855,1859d' ort-dev.js
sed -i '' 's/function inquire(moduleName){try{var mod=eval("quire".replace(\/^\/,"re"))(moduleName);if(mod&&(mod.length||Object.keys(mod).length))return mod}catch(t){}return null}/function inquire(moduleName){return null}/g' ort.js

# remove the last line of ort-dev.js (map)
sed -i '' '$d' ort-dev.js
