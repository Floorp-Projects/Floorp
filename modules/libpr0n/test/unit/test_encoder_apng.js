/*
 * Test for APNG encoding in libpr0n
 *
 */


const Ci = Components.interfaces;
const Cc = Components.classes;

  // dispose=[none|background|previous]
  // blend=[source|over]

var apng1A = {
	// A 3x3 image with 3 frames, alternating red, green, blue. RGB format.
	width  : 3, height : 3, skipFirstFrame : false,
	format : Ci.imgIEncoder.INPUT_FORMAT_RGB,
	transparency : null,
	plays : 0,

	frames  : [
		{ // frame #1
			width  : 3,    height : 3,
			x_offset : 0,  y_offset : 0,
			dispose : "none",   blend : "source", delay : 500,

			format : Ci.imgIEncoder.INPUT_FORMAT_RGB, stride : 9,
			transparency : null,

			pixels : [
				255,0,0,  255,0,0,  255,0,0,
				255,0,0,  255,0,0,  255,0,0,
				255,0,0,  255,0,0,  255,0,0
				]
		},

		{ // frame #2
			width  : 3,    height : 3,
			x_offset : 0,  y_offset : 0,
			dispose : "none",   blend : "source", delay : 500,

			format : Ci.imgIEncoder.INPUT_FORMAT_RGB, stride : 9,
			transparency : null,

			pixels : [
				0,255,0,  0,255,0,  0,255,0,
				0,255,0,  0,255,0,  0,255,0,
				0,255,0,  0,255,0,  0,255,0
				],
		},

		{ // frame #3
			width  : 3,    height : 3,
			x_offset : 0,  y_offset : 0,
			dispose : "none",   blend : "source", delay : 500,

			format : Ci.imgIEncoder.INPUT_FORMAT_RGB, stride : 9,
			transparency : null,

			pixels : [
				0,0,255,  0,0,255,  0,0,255,
				0,0,255,  0,0,255,  0,0,255,
				0,0,255,  0,0,255,  0,0,255
				],
		}

		],
	expected : "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAMAAAADCAIAAADZSiLoAAAACGFjVEwAAAADAAAAAM7tusAAAAAaZmNUTAAAAAAAAAADAAAAAwAAAAAAAAAAAfQD6AAAdRYgGAAAABBJREFUCJlj/M8ABUwMmCwAHVsBBdmfmJ4AAAAaZmNUTAAAAAEAAAADAAAAAwAAAAAAAAAAAfQD6AAA7mXKzAAAABZmZEFUAAAAAgiZY2T4zwABTAwMGCwAHFwBBVnd/YYAAAAaZmNUTAAAAAMAAAADAAAAAwAAAAAAAAAAAfQD6AAAA/MZJQAAABtmZEFUAAAABAiZY2Rg+M/AwMDAwMDEAAMIFgAbXQEFA/mlawAAAABJRU5ErkJggg=="
};


var apng1B = {
	// A 3x3 image with 3 frames, alternating red, green, blue. RGBA format.
	width  : 3, height : 3, skipFirstFrame : false,
	format : Ci.imgIEncoder.INPUT_FORMAT_RGBA,
	transparency : null,
	plays : 0,

	frames  : [
		{ // frame #1
			width  : 3,    height : 3,
			x_offset : 0,  y_offset : 0,
			dispose : "none",   blend : "source", delay : 500,

			format : Ci.imgIEncoder.INPUT_FORMAT_RGBA, stride : 12,

			pixels : [
				255,0,0,255,  255,0,0,255,  255,0,0,255,
				255,0,0,255,  255,0,0,255,  255,0,0,255,
				255,0,0,255,  255,0,0,255,  255,0,0,255
				]
		},

		{ // frame #2
			width  : 3,    height : 3,
			x_offset : 0,  y_offset : 0,
			dispose : "none",   blend : "source", delay : 500,

			format : Ci.imgIEncoder.INPUT_FORMAT_RGBA, stride : 12,

			pixels : [
				0,255,0,255,  0,255,0,255,  0,255,0,255,
				0,255,0,255,  0,255,0,255,  0,255,0,255,
				0,255,0,255,  0,255,0,255,  0,255,0,255
				],
		},

		{ // frame #3
			width  : 3,    height : 3,
			x_offset : 0,  y_offset : 0,
			dispose : "none",   blend : "source", delay : 500,

			format : Ci.imgIEncoder.INPUT_FORMAT_RGBA, stride : 12,

			pixels : [
				0,0,255,255,  0,0,255,255,  0,0,255,255,
				0,0,255,255,  0,0,255,255,  0,0,255,255,
				0,0,255,255,  0,0,255,255,  0,0,255,255
				],
		}

		],
	expected : "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAMAAAADCAYAAABWKLW/AAAACGFjVEwAAAADAAAAAM7tusAAAAAaZmNUTAAAAAAAAAADAAAAAwAAAAAAAAAAAfQD6AAAdRYgGAAAABVJREFUCJlj/M/A8J8BCpgYkAAKBwBJUwIE1lTE1gAAABpmY1RMAAAAAQAAAAMAAAADAAAAAAAAAAAB9APoAADuZcrMAAAAGWZkQVQAAAACCJljZPjP8J8BCpgYkAAKBwBIVAIEn70e9AAAABpmY1RMAAAAAwAAAAMAAAADAAAAAAAAAAAB9APoAAAD8xklAAAAGWZkQVQAAAAECJljZGD4/58BCpgYkAAKBwBHVQIEp0ldFQAAAABJRU5ErkJggg=="
};


var apng1C = {
	// A 3x3 image with 3 frames, alternating red, green, blue. RGBA format.
	// The first frame is skipped, so it will only flash green/blue (or static red in an APNG-unaware viewer)
	width  : 3, height : 3, skipFirstFrame : true,
	format : Ci.imgIEncoder.INPUT_FORMAT_RGBA,
	transparency : null,
	plays : 0,

	frames  : [
		{ // frame #1
			width  : 3,    height : 3,
			x_offset : 0,  y_offset : 0,
			dispose : "none",   blend : "source", delay : 500,

			format : Ci.imgIEncoder.INPUT_FORMAT_RGBA, stride : 12,

			pixels : [
				255,0,0,255,  255,0,0,255,  255,0,0,255,
				255,0,0,255,  255,0,0,255,  255,0,0,255,
				255,0,0,255,  255,0,0,255,  255,0,0,255
				]
		},

		{ // frame #2
			width  : 3,    height : 3,
			x_offset : 0,  y_offset : 0,
			dispose : "none",   blend : "source", delay : 500,

			format : Ci.imgIEncoder.INPUT_FORMAT_RGBA, stride : 12,

			pixels : [
				0,255,0,255,  0,255,0,255,  0,255,0,255,
				0,255,0,255,  0,255,0,255,  0,255,0,255,
				0,255,0,255,  0,255,0,255,  0,255,0,255
				],
		},

		{ // frame #3
			width  : 3,    height : 3,
			x_offset : 0,  y_offset : 0,
			dispose : "none",   blend : "source", delay : 500,

			format : Ci.imgIEncoder.INPUT_FORMAT_RGBA, stride : 12,

			pixels : [
				0,0,255,255,  0,0,255,255,  0,0,255,255,
				0,0,255,255,  0,0,255,255,  0,0,255,255,
				0,0,255,255,  0,0,255,255,  0,0,255,255
				],
		}

		],
	expected : "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAMAAAADCAYAAABWKLW/AAAACGFjVEwAAAACAAAAAPONk3AAAAAVSURBVAiZY/zPwPCfAQqYGJAACgcASVMCBNZUxNYAAAAaZmNUTAAAAAAAAAADAAAAAwAAAAAAAAAAAfQD6AAAdRYgGAAAABlmZEFUAAAAAQiZY2T4z/CfAQqYGJAACgcASFQCBKbFs7QAAAAaZmNUTAAAAAIAAAADAAAAAwAAAAAAAAAAAfQD6AAAmIDz8QAAABlmZEFUAAAAAwiZY2Rg+P+fAQqYGJAACgcAR1UCBMKQY1UAAAAASUVORK5CYII="
};


var apng2A = {
	// A 3x3 image with 3 frames, alternating red, green, blue. RGBA format.
	// blend = over mode
	width  : 3, height : 3, skipFirstFrame : false,
	format : Ci.imgIEncoder.INPUT_FORMAT_RGBA,
	transparency : null,
	plays : 0,

	frames  : [
		{ // frame #1
			width  : 3,    height : 3,
			x_offset : 0,  y_offset : 0,
			dispose : "none",   blend : "source", delay : 500,

			format : Ci.imgIEncoder.INPUT_FORMAT_RGBA, stride : 12,

			pixels : [
				255,0,0,255,  255,0,0,255,  255,0,0,255,
				255,0,0,255,  255,0,0,255,  255,0,0,255,
				255,0,0,255,  255,0,0,255,  255,0,0,255
				]
		},

		{ // frame #2
			width  : 3,    height : 3,
			x_offset : 0,  y_offset : 0,
			dispose : "none",   blend : "over", delay : 500,

			format : Ci.imgIEncoder.INPUT_FORMAT_RGBA, stride : 12,

			pixels : [
				0,255,0,255,  0,255,0,180,  0,255,0,75,
				0,255,0,255,  0,255,0,180,  0,255,0,75,
				0,255,0,255,  0,255,0,180,  0,255,0,75
				],
		},

		{ // frame #3
			width  : 3,    height : 3,
			x_offset : 0,  y_offset : 0,
			dispose : "none",   blend : "over", delay : 500,

			format : Ci.imgIEncoder.INPUT_FORMAT_RGBA, stride : 12,

			pixels : [
				0,0,255,75,   0,0,255,75,   0,0,255,75,
				0,0,255,180,  0,0,255,180,  0,0,255,180,
				0,0,255,255,  0,0,255,255,  0,0,255,255
				],
		}

		],
	expected : "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAMAAAADCAYAAABWKLW/AAAACGFjVEwAAAADAAAAAM7tusAAAAAaZmNUTAAAAAAAAAADAAAAAwAAAAAAAAAAAfQD6AAAdRYgGAAAABVJREFUCJlj/M/A8J8BCpgYkAAKBwBJUwIE1lTE1gAAABpmY1RMAAAAAQAAAAMAAAADAAAAAAAAAAAB9APoAAGZYvpaAAAAH2ZkQVQAAAACCJljYPjP8J/hP8MWhv8M3kwMSACFAwCjpAUABsIwXQAAABpmY1RMAAAAAwAAAAMAAAADAAAAAAAAAAAB9APoAAF09CmzAAAAHWZkQVQAAAAECJljZGD4780ABYwMDP+3IHP+wzgAZ+AE/2rZOPoAAAAASUVORK5CYII="
};


var apng2B = {
	// A 3x3 image with 3 frames, alternating red, green, blue. RGBA format.
	// blend = over, dispose = background
	width  : 3, height : 3, skipFirstFrame : false,
	format : Ci.imgIEncoder.INPUT_FORMAT_RGBA,
	transparency : null,
	plays : 0,

	frames  : [
		{ // frame #1
			width  : 3,    height : 3,
			x_offset : 0,  y_offset : 0,
			dispose : "background",   blend : "source", delay : 500,

			format : Ci.imgIEncoder.INPUT_FORMAT_RGBA, stride : 12,

			pixels : [
				255,0,0,255,  255,0,0,255,  255,0,0,255,
				255,0,0,255,  255,0,0,255,  255,0,0,255,
				255,0,0,255,  255,0,0,255,  255,0,0,255
				]
		},

		{ // frame #2
			width  : 3,    height : 3,
			x_offset : 0,  y_offset : 0,
			dispose : "background",   blend : "over", delay : 500,

			format : Ci.imgIEncoder.INPUT_FORMAT_RGBA, stride : 12,

			pixels : [
				0,255,0,255,  0,255,0,180,  0,255,0,75,
				0,255,0,255,  0,255,0,180,  0,255,0,75,
				0,255,0,255,  0,255,0,180,  0,255,0,75
				],
		},

		{ // frame #3
			width  : 3,    height : 3,
			x_offset : 0,  y_offset : 0,
			dispose : "background",   blend : "over", delay : 500,

			format : Ci.imgIEncoder.INPUT_FORMAT_RGBA, stride : 12,

			pixels : [
				0,0,255,75,   0,0,255,75,   0,0,255,75,
				0,0,255,180,  0,0,255,180,  0,0,255,180,
				0,0,255,255,  0,0,255,255,  0,0,255,255
				],
		}

		],
	expected : "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAMAAAADCAYAAABWKLW/AAAACGFjVEwAAAADAAAAAM7tusAAAAAaZmNUTAAAAAAAAAADAAAAAwAAAAAAAAAAAfQD6AEAbA0RWQAAABVJREFUCJlj/M/A8J8BCpgYkAAKBwBJUwIE1lTE1gAAABpmY1RMAAAAAQAAAAMAAAADAAAAAAAAAAAB9APoAQGAecsbAAAAH2ZkQVQAAAACCJljYPjP8J/hP8MWhv8M3kwMSACFAwCjpAUABsIwXQAAABpmY1RMAAAAAwAAAAMAAAADAAAAAAAAAAAB9APoAQFt7xjyAAAAHWZkQVQAAAAECJljZGD4780ABYwMDP+3IHP+wzgAZ+AE/2rZOPoAAAAASUVORK5CYII="
};


var apng3 = {
	// A 3x3 image with 4 frames. First frame is white, then 1x1 frames draw a diagonal line
	width  : 3, height : 3, skipFirstFrame : false,
	format : Ci.imgIEncoder.INPUT_FORMAT_RGBA,
	transparency : null,
	plays : 0,

	frames  : [
		{ // frame #1
			width  : 3,    height : 3,
			x_offset : 0,  y_offset : 0,
			dispose : "none",   blend : "source", delay : 500,

			format : Ci.imgIEncoder.INPUT_FORMAT_RGBA, stride : 12,

			pixels : [

				255,255,255,255,  255,255,255,255,  255,255,255,255,
				255,255,255,255,  255,255,255,255,  255,255,255,255,
				255,255,255,255,  255,255,255,255,  255,255,255,255
				]
		},

		{ // frame #2
			width  : 1,    height : 1,
			x_offset : 0,  y_offset : 0,
			dispose : "none",   blend : "source", delay : 500,

			format : Ci.imgIEncoder.INPUT_FORMAT_RGBA, stride : 12,

			pixels : [
				0,0,0,255
				],
		},

		{ // frame #3
			width  : 1,    height : 1,
			x_offset : 1,  y_offset : 1,
			dispose : "none",   blend : "source", delay : 500,

			format : Ci.imgIEncoder.INPUT_FORMAT_RGBA, stride : 12,

			pixels : [
				0,0,0,255
				],
		},

		{ // frame #4
			width  : 1,    height : 1,
			x_offset : 2,  y_offset : 2,
			dispose : "none",   blend : "source", delay : 500,

			format : Ci.imgIEncoder.INPUT_FORMAT_RGBA, stride : 12,

			pixels : [
				0,0,0,255
				],
		}
		],

	expected : "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAMAAAADCAYAAABWKLW/AAAACGFjVEwAAAAEAAAAAHzNZtAAAAAaZmNUTAAAAAAAAAADAAAAAwAAAAAAAAAAAfQD6AAAdRYgGAAAABVJREFUCJlj/P///38GKGBiQAIoHACSCgQC2nCLJAAAABpmY1RMAAAAAQAAAAEAAAABAAAAAAAAAAAB9APoAAAyV32sAAAAEWZkQVQAAAACCJljYGBg+A8AAQQBAG6+ZqYAAAAaZmNUTAAAAAMAAAABAAAAAQAAAAEAAAABAfQD6AAAuDh6MQAAABFmZEFUAAAABAiZY2BgYPgPAAEEAQCWfC0QAAAAGmZjVEwAAAAFAAAAAQAAAAEAAAACAAAAAgH0A+gAAP34dNcAAAARZmRBVAAAAAYImWNgYGD4DwABBAEAdxLpvQAAAABJRU5ErkJggg=="
};

// Main test entry point.
function run_test() {
	dump("Checking apng1A...\n");
	run_test_for(apng1A);
	dump("Checking apng1B...\n");
	run_test_for(apng1B);
	dump("Checking apng1C...\n");
	run_test_for(apng1C);

	dump("Checking apng2A...\n");
	run_test_for(apng2A);
	dump("Checking apng2B...\n");
	run_test_for(apng2B);

	dump("Checking apng3...\n");
	run_test_for(apng3);
}; 


function run_test_for(input) {
	var encoder, dataURL;

	encoder = encodeImage(input);
	dataURL = makeDataURL(encoder, "image/png");
	do_check_eq(dataURL, input.expected);
};


function encodeImage(input) {
	var encoder = Cc["@mozilla.org/image/encoder;2?type=image/png"].createInstance();
	encoder.QueryInterface(Ci.imgIEncoder);

	var options = "";
	if (input.transparency) { options += "transparency=" + input.transparency; }
	options += ";frames=" + input.frames.length;
	options += ";skipfirstframe=" + (input.skipFirstFrame ? "yes" : "no");
	options += ";plays=" + input.plays;
	encoder.startImageEncode(input.width, input.height, input.format, options);

	for (var i = 0; i < input.frames.length; i++) {
		var frame = input.frames[i];

		options = "";
		if (frame.transparency) { options += "transparency=" + input.transparency; }
		options += ";delay=" + frame.delay;
		options += ";dispose=" + frame.dispose;
		options += ";blend="   + frame.blend;
		if (frame.x_offset > 0) { options += ";xoffset=" + frame.x_offset; }
		if (frame.y_offset > 0) { options += ";yoffset=" + frame.y_offset; }

		encoder.addImageFrame(frame.pixels, frame.pixels.length,
			frame.width, frame.height, frame.stride, frame.format, options);
	}

	encoder.endImageEncode();

	return encoder;
}


function makeDataURL(encoder, mimetype) {
	var rawStream = encoder.QueryInterface(Ci.nsIInputStream);

	var stream = Cc["@mozilla.org/binaryinputstream;1"].createInstance();
	stream.QueryInterface(Ci.nsIBinaryInputStream);

	stream.setInputStream(rawStream);

	var bytes = stream.readByteArray(stream.available()); // returns int[]

	var base64String = toBase64(bytes);

	return "data:" + mimetype + ";base64," + base64String;
}

/* toBase64 copied from extensions/xml-rpc/src/nsXmlRpcClient.js */

/* Convert data (an array of integers) to a Base64 string. */
const toBase64Table = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz' +
    '0123456789+/';
const base64Pad = '=';
function toBase64(data) {
    var result = '';
    var length = data.length;
    var i;
    // Convert every three bytes to 4 ascii characters.
    for (i = 0; i < (length - 2); i += 3) {
        result += toBase64Table[data[i] >> 2];
        result += toBase64Table[((data[i] & 0x03) << 4) + (data[i+1] >> 4)];
        result += toBase64Table[((data[i+1] & 0x0f) << 2) + (data[i+2] >> 6)];
        result += toBase64Table[data[i+2] & 0x3f];
    }

    // Convert the remaining 1 or 2 bytes, pad out to 4 characters.
    if (length%3) {
        i = length - (length%3);
        result += toBase64Table[data[i] >> 2];
        if ((length%3) == 2) {
            result += toBase64Table[((data[i] & 0x03) << 4) + (data[i+1] >> 4)];
            result += toBase64Table[(data[i+1] & 0x0f) << 2];
            result += base64Pad;
        } else {
            result += toBase64Table[(data[i] & 0x03) << 4];
            result += base64Pad + base64Pad;
        }
    }

    return result;
}
