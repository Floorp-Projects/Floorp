const fontFace = new FontFace("dummy-font", "url(../resources/dummy.font)");
fonts.add(fontFace);
fontFace.load();

fonts.ready.then(() => { postMessage("Font loaded"); })
