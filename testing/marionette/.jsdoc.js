"use strict";

module.exports = {
	templates: {
		cleverLinks: true,
	},
	plugins: ["plugins/markdown"],
	opts: {
		destination: "doc",
		readme: "README.html",
		private: true,
	}
};
