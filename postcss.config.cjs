/** @type {import('postcss-load-config').Config} */
const config = {
  plugins: [require("autoprefixer"), require("postcss-nested"), require("postcss-sorting")],
};

module.exports = config;
