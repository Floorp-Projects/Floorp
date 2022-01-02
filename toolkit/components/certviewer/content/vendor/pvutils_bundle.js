(function(f){if(typeof exports==="object"&&typeof module!=="undefined"){module.exports=f()}else if(typeof define==="function"&&define.amd){define([],f)}else{var g;if(typeof window!=="undefined"){g=window}else if(typeof global!=="undefined"){g=global}else if(typeof self!=="undefined"){g=self}else{g=globalThis}g.pvutils = f()}})(function(){var define,module,exports;return (function(){function r(e,n,t){function o(i,f){if(!n[i]){if(!e[i]){var c="function"==typeof require&&require;if(!f&&c)return c(i,!0);if(u)return u(i,!0);var a=new Error("Cannot find module '"+i+"'");throw a.code="MODULE_NOT_FOUND",a}var p=n[i]={exports:{}};e[i][0].call(p.exports,function(r){var n=e[i][1][r];return o(n||r)},p,p.exports,r,e,n,t)}return n[i].exports}for(var u="function"==typeof require&&require,i=0;i<t.length;i++)o(t[i]);return o}return r})()({1:[function(require,module,exports){
"use strict";

Object.defineProperty(exports, "__esModule", {
	value: true
});
exports.getUTCDate = getUTCDate;
exports.getParametersValue = getParametersValue;
exports.bufferToHexCodes = bufferToHexCodes;
exports.checkBufferParams = checkBufferParams;
exports.utilFromBase = utilFromBase;
exports.utilToBase = utilToBase;
exports.utilConcatBuf = utilConcatBuf;
exports.utilConcatView = utilConcatView;
exports.utilDecodeTC = utilDecodeTC;
exports.utilEncodeTC = utilEncodeTC;
exports.isEqualBuffer = isEqualBuffer;
exports.padNumber = padNumber;
exports.toBase64 = toBase64;
exports.fromBase64 = fromBase64;
exports.arrayBufferToString = arrayBufferToString;
exports.stringToArrayBuffer = stringToArrayBuffer;
exports.nearestPowerOf2 = nearestPowerOf2;
exports.clearProps = clearProps;
//**************************************************************************************
/**
 * Making UTC date from local date
 * @param {Date} date Date to convert from
 * @returns {Date}
 */
function getUTCDate(date) {
	// noinspection NestedFunctionCallJS, MagicNumberJS
	return new Date(date.getTime() + date.getTimezoneOffset() * 60000);
}
//**************************************************************************************
// noinspection FunctionWithMultipleReturnPointsJS
/**
 * Get value for input parameters, or set a default value
 * @param {Object} parameters
 * @param {string} name
 * @param defaultValue
 */
function getParametersValue(parameters, name, defaultValue) {
	// noinspection ConstantOnRightSideOfComparisonJS, NonBlockStatementBodyJS
	if (parameters instanceof Object === false) return defaultValue;

	// noinspection NonBlockStatementBodyJS
	if (name in parameters) return parameters[name];

	return defaultValue;
}
//**************************************************************************************
/**
 * Converts "ArrayBuffer" into a hexdecimal string
 * @param {ArrayBuffer} inputBuffer
 * @param {number} [inputOffset=0]
 * @param {number} [inputLength=inputBuffer.byteLength]
 * @param {boolean} [insertSpace=false]
 * @returns {string}
 */
function bufferToHexCodes(inputBuffer, inputOffset = 0, inputLength = inputBuffer.byteLength - inputOffset, insertSpace = false) {
	let result = "";

	var _iteratorNormalCompletion = true;
	var _didIteratorError = false;
	var _iteratorError = undefined;

	try {
		for (var _iterator = new Uint8Array(inputBuffer, inputOffset, inputLength)[Symbol.iterator](), _step; !(_iteratorNormalCompletion = (_step = _iterator.next()).done); _iteratorNormalCompletion = true) {
			const item = _step.value;

			// noinspection ChainedFunctionCallJS
			const str = item.toString(16).toUpperCase();

			// noinspection ConstantOnRightSideOfComparisonJS, NonBlockStatementBodyJS
			if (str.length === 1) result += "0";

			result += str;

			// noinspection NonBlockStatementBodyJS
			if (insertSpace) result += " ";
		}
	} catch (err) {
		_didIteratorError = true;
		_iteratorError = err;
	} finally {
		try {
			if (!_iteratorNormalCompletion && _iterator.return) {
				_iterator.return();
			}
		} finally {
			if (_didIteratorError) {
				throw _iteratorError;
			}
		}
	}

	return result.trim();
}
//**************************************************************************************
// noinspection JSValidateJSDoc, FunctionWithMultipleReturnPointsJS
/**
 * Check input "ArrayBuffer" for common functions
 * @param {LocalBaseBlock} baseBlock
 * @param {ArrayBuffer} inputBuffer
 * @param {number} inputOffset
 * @param {number} inputLength
 * @returns {boolean}
 */
function checkBufferParams(baseBlock, inputBuffer, inputOffset, inputLength) {
	// noinspection ConstantOnRightSideOfComparisonJS
	if (inputBuffer instanceof ArrayBuffer === false) {
		// noinspection JSUndefinedPropertyAssignment
		baseBlock.error = "Wrong parameter: inputBuffer must be \"ArrayBuffer\"";
		return false;
	}

	// noinspection ConstantOnRightSideOfComparisonJS
	if (inputBuffer.byteLength === 0) {
		// noinspection JSUndefinedPropertyAssignment
		baseBlock.error = "Wrong parameter: inputBuffer has zero length";
		return false;
	}

	// noinspection ConstantOnRightSideOfComparisonJS
	if (inputOffset < 0) {
		// noinspection JSUndefinedPropertyAssignment
		baseBlock.error = "Wrong parameter: inputOffset less than zero";
		return false;
	}

	// noinspection ConstantOnRightSideOfComparisonJS
	if (inputLength < 0) {
		// noinspection JSUndefinedPropertyAssignment
		baseBlock.error = "Wrong parameter: inputLength less than zero";
		return false;
	}

	// noinspection ConstantOnRightSideOfComparisonJS
	if (inputBuffer.byteLength - inputOffset - inputLength < 0) {
		// noinspection JSUndefinedPropertyAssignment
		baseBlock.error = "End of input reached before message was fully decoded (inconsistent offset and length values)";
		return false;
	}

	return true;
}
//**************************************************************************************
// noinspection FunctionWithMultipleReturnPointsJS
/**
 * Convert number from 2^base to 2^10
 * @param {Uint8Array} inputBuffer
 * @param {number} inputBase
 * @returns {number}
 */
function utilFromBase(inputBuffer, inputBase) {
	let result = 0;

	// noinspection ConstantOnRightSideOfComparisonJS, NonBlockStatementBodyJS
	if (inputBuffer.length === 1) return inputBuffer[0];

	// noinspection ConstantOnRightSideOfComparisonJS, NonBlockStatementBodyJS
	for (let i = inputBuffer.length - 1; i >= 0; i--) result += inputBuffer[inputBuffer.length - 1 - i] * Math.pow(2, inputBase * i);

	return result;
}
//**************************************************************************************
// noinspection FunctionWithMultipleLoopsJS, FunctionWithMultipleReturnPointsJS
/**
 * Convert number from 2^10 to 2^base
 * @param {!number} value The number to convert
 * @param {!number} base The base for 2^base
 * @param {number} [reserved=0] Pre-defined number of bytes in output array (-1 = limited by function itself)
 * @returns {ArrayBuffer}
 */
function utilToBase(value, base, reserved = -1) {
	const internalReserved = reserved;
	let internalValue = value;

	let result = 0;
	let biggest = Math.pow(2, base);

	// noinspection ConstantOnRightSideOfComparisonJS
	for (let i = 1; i < 8; i++) {
		if (value < biggest) {
			let retBuf;

			// noinspection ConstantOnRightSideOfComparisonJS
			if (internalReserved < 0) {
				retBuf = new ArrayBuffer(i);
				result = i;
			} else {
				// noinspection NonBlockStatementBodyJS
				if (internalReserved < i) return new ArrayBuffer(0);

				retBuf = new ArrayBuffer(internalReserved);

				result = internalReserved;
			}

			const retView = new Uint8Array(retBuf);

			// noinspection ConstantOnRightSideOfComparisonJS
			for (let j = i - 1; j >= 0; j--) {
				const basis = Math.pow(2, j * base);

				retView[result - j - 1] = Math.floor(internalValue / basis);
				internalValue -= retView[result - j - 1] * basis;
			}

			return retBuf;
		}

		biggest *= Math.pow(2, base);
	}

	return new ArrayBuffer(0);
}
//**************************************************************************************
// noinspection FunctionWithMultipleLoopsJS
/**
 * Concatenate two ArrayBuffers
 * @param {...ArrayBuffer} buffers Set of ArrayBuffer
 */
function utilConcatBuf(...buffers) {
	//region Initial variables
	let outputLength = 0;
	let prevLength = 0;
	//endregion

	//region Calculate output length

	// noinspection NonBlockStatementBodyJS
	var _iteratorNormalCompletion2 = true;
	var _didIteratorError2 = false;
	var _iteratorError2 = undefined;

	try {
		for (var _iterator2 = buffers[Symbol.iterator](), _step2; !(_iteratorNormalCompletion2 = (_step2 = _iterator2.next()).done); _iteratorNormalCompletion2 = true) {
			const buffer = _step2.value;

			outputLength += buffer.byteLength;
		} //endregion
	} catch (err) {
		_didIteratorError2 = true;
		_iteratorError2 = err;
	} finally {
		try {
			if (!_iteratorNormalCompletion2 && _iterator2.return) {
				_iterator2.return();
			}
		} finally {
			if (_didIteratorError2) {
				throw _iteratorError2;
			}
		}
	}

	const retBuf = new ArrayBuffer(outputLength);
	const retView = new Uint8Array(retBuf);

	var _iteratorNormalCompletion3 = true;
	var _didIteratorError3 = false;
	var _iteratorError3 = undefined;

	try {
		for (var _iterator3 = buffers[Symbol.iterator](), _step3; !(_iteratorNormalCompletion3 = (_step3 = _iterator3.next()).done); _iteratorNormalCompletion3 = true) {
			const buffer = _step3.value;

			// noinspection NestedFunctionCallJS
			retView.set(new Uint8Array(buffer), prevLength);
			prevLength += buffer.byteLength;
		}
	} catch (err) {
		_didIteratorError3 = true;
		_iteratorError3 = err;
	} finally {
		try {
			if (!_iteratorNormalCompletion3 && _iterator3.return) {
				_iterator3.return();
			}
		} finally {
			if (_didIteratorError3) {
				throw _iteratorError3;
			}
		}
	}

	return retBuf;
}
//**************************************************************************************
// noinspection FunctionWithMultipleLoopsJS
/**
 * Concatenate two Uint8Array
 * @param {...Uint8Array} views Set of Uint8Array
 */
function utilConcatView(...views) {
	//region Initial variables
	let outputLength = 0;
	let prevLength = 0;
	//endregion

	//region Calculate output length
	// noinspection NonBlockStatementBodyJS
	var _iteratorNormalCompletion4 = true;
	var _didIteratorError4 = false;
	var _iteratorError4 = undefined;

	try {
		for (var _iterator4 = views[Symbol.iterator](), _step4; !(_iteratorNormalCompletion4 = (_step4 = _iterator4.next()).done); _iteratorNormalCompletion4 = true) {
			const view = _step4.value;

			outputLength += view.length;
		} //endregion
	} catch (err) {
		_didIteratorError4 = true;
		_iteratorError4 = err;
	} finally {
		try {
			if (!_iteratorNormalCompletion4 && _iterator4.return) {
				_iterator4.return();
			}
		} finally {
			if (_didIteratorError4) {
				throw _iteratorError4;
			}
		}
	}

	const retBuf = new ArrayBuffer(outputLength);
	const retView = new Uint8Array(retBuf);

	var _iteratorNormalCompletion5 = true;
	var _didIteratorError5 = false;
	var _iteratorError5 = undefined;

	try {
		for (var _iterator5 = views[Symbol.iterator](), _step5; !(_iteratorNormalCompletion5 = (_step5 = _iterator5.next()).done); _iteratorNormalCompletion5 = true) {
			const view = _step5.value;

			retView.set(view, prevLength);
			prevLength += view.length;
		}
	} catch (err) {
		_didIteratorError5 = true;
		_iteratorError5 = err;
	} finally {
		try {
			if (!_iteratorNormalCompletion5 && _iterator5.return) {
				_iterator5.return();
			}
		} finally {
			if (_didIteratorError5) {
				throw _iteratorError5;
			}
		}
	}

	return retView;
}
//**************************************************************************************
// noinspection FunctionWithMultipleLoopsJS
/**
 * Decoding of "two complement" values
 * The function must be called in scope of instance of "hexBlock" class ("valueHex" and "warnings" properties must be present)
 * @returns {number}
 */
function utilDecodeTC() {
	const buf = new Uint8Array(this.valueHex);

	// noinspection ConstantOnRightSideOfComparisonJS
	if (this.valueHex.byteLength >= 2) {
		//noinspection JSBitwiseOperatorUsage, ConstantOnRightSideOfComparisonJS, LocalVariableNamingConventionJS, MagicNumberJS, NonShortCircuitBooleanExpressionJS
		const condition1 = buf[0] === 0xFF && buf[1] & 0x80;
		// noinspection ConstantOnRightSideOfComparisonJS, LocalVariableNamingConventionJS, MagicNumberJS, NonShortCircuitBooleanExpressionJS
		const condition2 = buf[0] === 0x00 && (buf[1] & 0x80) === 0x00;

		// noinspection NonBlockStatementBodyJS
		if (condition1 || condition2) this.warnings.push("Needlessly long format");
	}

	//region Create big part of the integer
	const bigIntBuffer = new ArrayBuffer(this.valueHex.byteLength);
	const bigIntView = new Uint8Array(bigIntBuffer);
	// noinspection NonBlockStatementBodyJS
	for (let i = 0; i < this.valueHex.byteLength; i++) bigIntView[i] = 0;

	// noinspection MagicNumberJS, NonShortCircuitBooleanExpressionJS
	bigIntView[0] = buf[0] & 0x80; // mask only the biggest bit

	const bigInt = utilFromBase(bigIntView, 8);
	//endregion

	//region Create small part of the integer
	const smallIntBuffer = new ArrayBuffer(this.valueHex.byteLength);
	const smallIntView = new Uint8Array(smallIntBuffer);
	// noinspection NonBlockStatementBodyJS
	for (let j = 0; j < this.valueHex.byteLength; j++) smallIntView[j] = buf[j];

	// noinspection MagicNumberJS
	smallIntView[0] &= 0x7F; // mask biggest bit

	const smallInt = utilFromBase(smallIntView, 8);
	//endregion

	return smallInt - bigInt;
}
//**************************************************************************************
// noinspection FunctionWithMultipleLoopsJS, FunctionWithMultipleReturnPointsJS
/**
 * Encode integer value to "two complement" format
 * @param {number} value Value to encode
 * @returns {ArrayBuffer}
 */
function utilEncodeTC(value) {
	// noinspection ConstantOnRightSideOfComparisonJS, ConditionalExpressionJS
	const modValue = value < 0 ? value * -1 : value;
	let bigInt = 128;

	// noinspection ConstantOnRightSideOfComparisonJS
	for (let i = 1; i < 8; i++) {
		if (modValue <= bigInt) {
			// noinspection ConstantOnRightSideOfComparisonJS
			if (value < 0) {
				const smallInt = bigInt - modValue;

				const retBuf = utilToBase(smallInt, 8, i);
				const retView = new Uint8Array(retBuf);

				// noinspection MagicNumberJS
				retView[0] |= 0x80;

				return retBuf;
			}

			let retBuf = utilToBase(modValue, 8, i);
			let retView = new Uint8Array(retBuf);

			//noinspection JSBitwiseOperatorUsage, MagicNumberJS, NonShortCircuitBooleanExpressionJS
			if (retView[0] & 0x80) {
				//noinspection JSCheckFunctionSignatures
				const tempBuf = retBuf.slice(0);
				const tempView = new Uint8Array(tempBuf);

				retBuf = new ArrayBuffer(retBuf.byteLength + 1);
				// noinspection ReuseOfLocalVariableJS
				retView = new Uint8Array(retBuf);

				// noinspection NonBlockStatementBodyJS
				for (let k = 0; k < tempBuf.byteLength; k++) retView[k + 1] = tempView[k];

				// noinspection MagicNumberJS
				retView[0] = 0x00;
			}

			return retBuf;
		}

		bigInt *= Math.pow(2, 8);
	}

	return new ArrayBuffer(0);
}
//**************************************************************************************
// noinspection FunctionWithMultipleReturnPointsJS, ParameterNamingConventionJS
/**
 * Compare two array buffers
 * @param {!ArrayBuffer} inputBuffer1
 * @param {!ArrayBuffer} inputBuffer2
 * @returns {boolean}
 */
function isEqualBuffer(inputBuffer1, inputBuffer2) {
	// noinspection NonBlockStatementBodyJS
	if (inputBuffer1.byteLength !== inputBuffer2.byteLength) return false;

	// noinspection LocalVariableNamingConventionJS
	const view1 = new Uint8Array(inputBuffer1);
	// noinspection LocalVariableNamingConventionJS
	const view2 = new Uint8Array(inputBuffer2);

	for (let i = 0; i < view1.length; i++) {
		// noinspection NonBlockStatementBodyJS
		if (view1[i] !== view2[i]) return false;
	}

	return true;
}
//**************************************************************************************
// noinspection FunctionWithMultipleReturnPointsJS
/**
 * Pad input number with leade "0" if needed
 * @returns {string}
 * @param {number} inputNumber
 * @param {number} fullLength
 */
function padNumber(inputNumber, fullLength) {
	const str = inputNumber.toString(10);

	// noinspection NonBlockStatementBodyJS
	if (fullLength < str.length) return "";

	const dif = fullLength - str.length;

	const padding = new Array(dif);
	// noinspection NonBlockStatementBodyJS
	for (let i = 0; i < dif; i++) padding[i] = "0";

	const paddingString = padding.join("");

	return paddingString.concat(str);
}
//**************************************************************************************
const base64Template = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
const base64UrlTemplate = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_=";
//**************************************************************************************
// noinspection FunctionWithMultipleLoopsJS, OverlyComplexFunctionJS, FunctionTooLongJS, FunctionNamingConventionJS
/**
 * Encode string into BASE64 (or "base64url")
 * @param {string} input
 * @param {boolean} useUrlTemplate If "true" then output would be encoded using "base64url"
 * @param {boolean} skipPadding Skip BASE-64 padding or not
 * @param {boolean} skipLeadingZeros Skip leading zeros in input data or not
 * @returns {string}
 */
function toBase64(input, useUrlTemplate = false, skipPadding = false, skipLeadingZeros = false) {
	let i = 0;

	// noinspection LocalVariableNamingConventionJS
	let flag1 = 0;
	// noinspection LocalVariableNamingConventionJS
	let flag2 = 0;

	let output = "";

	// noinspection ConditionalExpressionJS
	const template = useUrlTemplate ? base64UrlTemplate : base64Template;

	if (skipLeadingZeros) {
		let nonZeroPosition = 0;

		for (let i = 0; i < input.length; i++) {
			// noinspection ConstantOnRightSideOfComparisonJS
			if (input.charCodeAt(i) !== 0) {
				nonZeroPosition = i;
				// noinspection BreakStatementJS
				break;
			}
		}

		// noinspection AssignmentToFunctionParameterJS
		input = input.slice(nonZeroPosition);
	}

	while (i < input.length) {
		// noinspection LocalVariableNamingConventionJS, IncrementDecrementResultUsedJS
		const chr1 = input.charCodeAt(i++);
		// noinspection NonBlockStatementBodyJS
		if (i >= input.length) flag1 = 1;
		// noinspection LocalVariableNamingConventionJS, IncrementDecrementResultUsedJS
		const chr2 = input.charCodeAt(i++);
		// noinspection NonBlockStatementBodyJS
		if (i >= input.length) flag2 = 1;
		// noinspection LocalVariableNamingConventionJS, IncrementDecrementResultUsedJS
		const chr3 = input.charCodeAt(i++);

		// noinspection LocalVariableNamingConventionJS
		const enc1 = chr1 >> 2;
		// noinspection LocalVariableNamingConventionJS, MagicNumberJS, NonShortCircuitBooleanExpressionJS
		const enc2 = (chr1 & 0x03) << 4 | chr2 >> 4;
		// noinspection LocalVariableNamingConventionJS, MagicNumberJS, NonShortCircuitBooleanExpressionJS
		let enc3 = (chr2 & 0x0F) << 2 | chr3 >> 6;
		// noinspection LocalVariableNamingConventionJS, MagicNumberJS, NonShortCircuitBooleanExpressionJS
		let enc4 = chr3 & 0x3F;

		// noinspection ConstantOnRightSideOfComparisonJS
		if (flag1 === 1) {
			// noinspection NestedAssignmentJS, AssignmentResultUsedJS, MagicNumberJS
			enc3 = enc4 = 64;
		} else {
			// noinspection ConstantOnRightSideOfComparisonJS
			if (flag2 === 1) {
				// noinspection MagicNumberJS
				enc4 = 64;
			}
		}

		// noinspection NonBlockStatementBodyJS
		if (skipPadding) {
			// noinspection ConstantOnRightSideOfComparisonJS, NonBlockStatementBodyJS, MagicNumberJS
			if (enc3 === 64) output += `${template.charAt(enc1)}${template.charAt(enc2)}`;else {
				// noinspection ConstantOnRightSideOfComparisonJS, NonBlockStatementBodyJS, MagicNumberJS
				if (enc4 === 64) output += `${template.charAt(enc1)}${template.charAt(enc2)}${template.charAt(enc3)}`;else output += `${template.charAt(enc1)}${template.charAt(enc2)}${template.charAt(enc3)}${template.charAt(enc4)}`;
			}
		} else output += `${template.charAt(enc1)}${template.charAt(enc2)}${template.charAt(enc3)}${template.charAt(enc4)}`;
	}

	return output;
}
//**************************************************************************************
// noinspection FunctionWithMoreThanThreeNegationsJS, FunctionWithMultipleLoopsJS, OverlyComplexFunctionJS, FunctionNamingConventionJS
/**
 * Decode string from BASE64 (or "base64url")
 * @param {string} input
 * @param {boolean} [useUrlTemplate=false] If "true" then output would be encoded using "base64url"
 * @param {boolean} [cutTailZeros=false] If "true" then cut tailing zeroz from function result
 * @returns {string}
 */
function fromBase64(input, useUrlTemplate = false, cutTailZeros = false) {
	// noinspection ConditionalExpressionJS
	const template = useUrlTemplate ? base64UrlTemplate : base64Template;

	//region Aux functions
	// noinspection FunctionWithMultipleReturnPointsJS, NestedFunctionJS
	function indexof(toSearch) {
		// noinspection ConstantOnRightSideOfComparisonJS, MagicNumberJS
		for (let i = 0; i < 64; i++) {
			// noinspection NonBlockStatementBodyJS
			if (template.charAt(i) === toSearch) return i;
		}

		// noinspection MagicNumberJS
		return 64;
	}

	// noinspection NestedFunctionJS
	function test(incoming) {
		// noinspection ConstantOnRightSideOfComparisonJS, ConditionalExpressionJS, MagicNumberJS
		return incoming === 64 ? 0x00 : incoming;
	}
	//endregion

	let i = 0;

	let output = "";

	while (i < input.length) {
		// noinspection NestedFunctionCallJS, LocalVariableNamingConventionJS, IncrementDecrementResultUsedJS
		const enc1 = indexof(input.charAt(i++));
		// noinspection NestedFunctionCallJS, LocalVariableNamingConventionJS, ConditionalExpressionJS, MagicNumberJS, IncrementDecrementResultUsedJS
		const enc2 = i >= input.length ? 0x00 : indexof(input.charAt(i++));
		// noinspection NestedFunctionCallJS, LocalVariableNamingConventionJS, ConditionalExpressionJS, MagicNumberJS, IncrementDecrementResultUsedJS
		const enc3 = i >= input.length ? 0x00 : indexof(input.charAt(i++));
		// noinspection NestedFunctionCallJS, LocalVariableNamingConventionJS, ConditionalExpressionJS, MagicNumberJS, IncrementDecrementResultUsedJS
		const enc4 = i >= input.length ? 0x00 : indexof(input.charAt(i++));

		// noinspection LocalVariableNamingConventionJS, NonShortCircuitBooleanExpressionJS
		const chr1 = test(enc1) << 2 | test(enc2) >> 4;
		// noinspection LocalVariableNamingConventionJS, MagicNumberJS, NonShortCircuitBooleanExpressionJS
		const chr2 = (test(enc2) & 0x0F) << 4 | test(enc3) >> 2;
		// noinspection LocalVariableNamingConventionJS, MagicNumberJS, NonShortCircuitBooleanExpressionJS
		const chr3 = (test(enc3) & 0x03) << 6 | test(enc4);

		output += String.fromCharCode(chr1);

		// noinspection ConstantOnRightSideOfComparisonJS, NonBlockStatementBodyJS, MagicNumberJS
		if (enc3 !== 64) output += String.fromCharCode(chr2);

		// noinspection ConstantOnRightSideOfComparisonJS, NonBlockStatementBodyJS, MagicNumberJS
		if (enc4 !== 64) output += String.fromCharCode(chr3);
	}

	if (cutTailZeros) {
		const outputLength = output.length;
		let nonZeroStart = -1;

		// noinspection ConstantOnRightSideOfComparisonJS
		for (let i = outputLength - 1; i >= 0; i--) {
			// noinspection ConstantOnRightSideOfComparisonJS
			if (output.charCodeAt(i) !== 0) {
				nonZeroStart = i;
				// noinspection BreakStatementJS
				break;
			}
		}

		// noinspection NonBlockStatementBodyJS, NegatedIfStatementJS
		if (nonZeroStart !== -1) output = output.slice(0, nonZeroStart + 1);else output = "";
	}

	return output;
}
//**************************************************************************************
function arrayBufferToString(buffer) {
	let resultString = "";
	const view = new Uint8Array(buffer);

	// noinspection NonBlockStatementBodyJS
	var _iteratorNormalCompletion6 = true;
	var _didIteratorError6 = false;
	var _iteratorError6 = undefined;

	try {
		for (var _iterator6 = view[Symbol.iterator](), _step6; !(_iteratorNormalCompletion6 = (_step6 = _iterator6.next()).done); _iteratorNormalCompletion6 = true) {
			const element = _step6.value;

			resultString += String.fromCharCode(element);
		}
	} catch (err) {
		_didIteratorError6 = true;
		_iteratorError6 = err;
	} finally {
		try {
			if (!_iteratorNormalCompletion6 && _iterator6.return) {
				_iterator6.return();
			}
		} finally {
			if (_didIteratorError6) {
				throw _iteratorError6;
			}
		}
	}

	return resultString;
}
//**************************************************************************************
function stringToArrayBuffer(str) {
	const stringLength = str.length;

	const resultBuffer = new ArrayBuffer(stringLength);
	const resultView = new Uint8Array(resultBuffer);

	// noinspection NonBlockStatementBodyJS
	for (let i = 0; i < stringLength; i++) resultView[i] = str.charCodeAt(i);

	return resultBuffer;
}
//**************************************************************************************
const log2 = Math.log(2);
//**************************************************************************************
// noinspection FunctionNamingConventionJS
/**
 * Get nearest to input length power of 2
 * @param {number} length Current length of existing array
 * @returns {number}
 */
function nearestPowerOf2(length) {
	const base = Math.log(length) / log2;

	const floor = Math.floor(base);
	const round = Math.round(base);

	// noinspection ConditionalExpressionJS
	return floor === round ? floor : round;
}
//**************************************************************************************
/**
 * Delete properties by name from specified object
 * @param {Object} object Object to delete properties from
 * @param {Array.<string>} propsArray Array of properties names
 */
function clearProps(object, propsArray) {
	var _iteratorNormalCompletion7 = true;
	var _didIteratorError7 = false;
	var _iteratorError7 = undefined;

	try {
		for (var _iterator7 = propsArray[Symbol.iterator](), _step7; !(_iteratorNormalCompletion7 = (_step7 = _iterator7.next()).done); _iteratorNormalCompletion7 = true) {
			const prop = _step7.value;

			delete object[prop];
		}
	} catch (err) {
		_didIteratorError7 = true;
		_iteratorError7 = err;
	} finally {
		try {
			if (!_iteratorNormalCompletion7 && _iterator7.return) {
				_iterator7.return();
			}
		} finally {
			if (_didIteratorError7) {
				throw _iteratorError7;
			}
		}
	}
}
//**************************************************************************************

},{}],2:[function(require,module,exports){
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const pvutils = require("pvutils"); // version 1.0.17

module.exports = {
  pvutils,
};
 
},{"pvutils":1}]},{},[2])(2)
});
var pvutils = globalThis.pvutils;
var EXPORTED_SYMBOLS = ["pvutils"];
