"use strict";

module.exports = {
	templates: {
		cleverLinks: true,
	},
	plugins: ["plugins/markdown"],
	opts: {
		destination: "doc/api",
		readme: "README.md",
		private: true,
	}
};
