(function(f){if(typeof exports==="object"&&typeof module!=="undefined"){module.exports=f()}else if(typeof define==="function"&&define.amd){define([],f)}else{var g;if(typeof window!=="undefined"){g=window}else if(typeof global!=="undefined"){g=global}else if(typeof self!=="undefined"){g=self}else{g=globalThis}g.asn1js = f()}})(function(){var define,module,exports;return (function(){function r(e,n,t){function o(i,f){if(!n[i]){if(!e[i]){var c="function"==typeof require&&require;if(!f&&c)return c(i,!0);if(u)return u(i,!0);var a=new Error("Cannot find module '"+i+"'");throw a.code="MODULE_NOT_FOUND",a}var p=n[i]={exports:{}};e[i][0].call(p.exports,function(r){var n=e[i][1][r];return o(n||r)},p,p.exports,r,e,n,t)}return n[i].exports}for(var u="function"==typeof require&&require,i=0;i<t.length;i++)o(t[i]);return o}return r})()({1:[function(require,module,exports){
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 const asn1js = require("asn1js"); // version 2.0.22
 
 module.exports = {
   asn1js,
 };
 
},{"asn1js":2}],2:[function(require,module,exports){
"use strict";

Object.defineProperty(exports, "__esModule", {
	value: true
});
exports.RawData = exports.Repeated = exports.Any = exports.Choice = exports.TIME = exports.Duration = exports.DateTime = exports.TimeOfDay = exports.DATE = exports.GeneralizedTime = exports.UTCTime = exports.CharacterString = exports.GeneralString = exports.VisibleString = exports.GraphicString = exports.IA5String = exports.VideotexString = exports.TeletexString = exports.PrintableString = exports.NumericString = exports.UniversalString = exports.BmpString = exports.Utf8String = exports.ObjectIdentifier = exports.Enumerated = exports.Integer = exports.BitString = exports.OctetString = exports.Null = exports.Set = exports.Sequence = exports.Boolean = exports.EndOfContent = exports.Constructed = exports.Primitive = exports.BaseBlock = undefined;
exports.fromBER = fromBER;
exports.compareSchema = compareSchema;
exports.verifySchema = verifySchema;
exports.fromJSON = fromJSON;

var _pvutils = require("pvutils");

//**************************************************************************************
//region Declaration of global variables
//**************************************************************************************
const powers2 = [new Uint8Array([1])]; /* eslint-disable indent */
/*
 * Copyright (c) 2016-2018, Peculiar Ventures
 * All rights reserved.
 *
 * Author 2016-2018, Yury Strozhevsky <www.strozhevsky.com>.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 */
//**************************************************************************************

const digitsString = "0123456789";
//**************************************************************************************
//endregion
//**************************************************************************************
//region Declaration for "LocalBaseBlock" class
//**************************************************************************************
/**
 * Class used as a base block for all remaining ASN.1 classes
 * @typedef LocalBaseBlock
 * @interface
 * @property {number} blockLength
 * @property {string} error
 * @property {Array.<string>} warnings
 * @property {ArrayBuffer} valueBeforeDecode
 */
class LocalBaseBlock {
	//**********************************************************************************
	/**
  * Constructor for "LocalBaseBlock" class
  * @param {Object} [parameters={}]
  * @property {ArrayBuffer} [valueBeforeDecode]
  */
	constructor(parameters = {}) {
		/**
   * @type {number} blockLength
   */
		this.blockLength = (0, _pvutils.getParametersValue)(parameters, "blockLength", 0);
		/**
   * @type {string} error
   */
		this.error = (0, _pvutils.getParametersValue)(parameters, "error", "");
		/**
   * @type {Array.<string>} warnings
   */
		this.warnings = (0, _pvutils.getParametersValue)(parameters, "warnings", []);
		//noinspection JSCheckFunctionSignatures
		/**
   * @type {ArrayBuffer} valueBeforeDecode
   */
		if ("valueBeforeDecode" in parameters) this.valueBeforeDecode = parameters.valueBeforeDecode.slice(0);else this.valueBeforeDecode = new ArrayBuffer(0);
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "baseBlock";
	}
	//**********************************************************************************
	/**
  * Convertion for the block to JSON object
  * @returns {{blockName: string, blockLength: number, error: string, warnings: Array.<string>, valueBeforeDecode: string}}
  */
	toJSON() {
		return {
			blockName: this.constructor.blockName(),
			blockLength: this.blockLength,
			error: this.error,
			warnings: this.warnings,
			valueBeforeDecode: (0, _pvutils.bufferToHexCodes)(this.valueBeforeDecode, 0, this.valueBeforeDecode.byteLength)
		};
	}
	//**********************************************************************************
}
//**************************************************************************************
//endregion
//**************************************************************************************
//region Description for "LocalHexBlock" class
//**************************************************************************************
/**
 * Class used as a base block for all remaining ASN.1 classes
 * @extends LocalBaseBlock
 * @typedef LocalHexBlock
 * @property {number} blockLength
 * @property {string} error
 * @property {Array.<string>} warnings
 * @property {ArrayBuffer} valueBeforeDecode
 * @property {boolean} isHexOnly
 * @property {ArrayBuffer} valueHex
 */
//noinspection JSUnusedLocalSymbols
const LocalHexBlock = BaseClass => class LocalHexBlockMixin extends BaseClass {
	//**********************************************************************************
	//noinspection JSUnusedGlobalSymbols
	/**
  * Constructor for "LocalHexBlock" class
  * @param {Object} [parameters={}]
  * @property {ArrayBuffer} [valueHex]
  */
	constructor(parameters = {}) {
		super(parameters);

		/**
   * @type {boolean}
   */
		this.isHexOnly = (0, _pvutils.getParametersValue)(parameters, "isHexOnly", false);
		/**
   * @type {ArrayBuffer}
   */
		if ("valueHex" in parameters) this.valueHex = parameters.valueHex.slice(0);else this.valueHex = new ArrayBuffer(0);
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "hexBlock";
	}
	//**********************************************************************************
	/**
  * Base function for converting block from BER encoded array of bytes
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  * @param {!number} inputOffset Offset in ASN.1 BER encoded array where decoding should be started
  * @param {!number} inputLength Maximum length of array of bytes which can be using in this function
  * @returns {number} Offset after least decoded byte
  */
	fromBER(inputBuffer, inputOffset, inputLength) {
		//region Basic check for parameters
		//noinspection JSCheckFunctionSignatures
		if ((0, _pvutils.checkBufferParams)(this, inputBuffer, inputOffset, inputLength) === false) return -1;
		//endregion

		//region Getting Uint8Array from ArrayBuffer
		const intBuffer = new Uint8Array(inputBuffer, inputOffset, inputLength);
		//endregion

		//region Initial checks
		if (intBuffer.length === 0) {
			this.warnings.push("Zero buffer length");
			return inputOffset;
		}
		//endregion

		//region Copy input buffer to internal buffer
		this.valueHex = inputBuffer.slice(inputOffset, inputOffset + inputLength);
		//endregion

		this.blockLength = inputLength;

		return inputOffset + inputLength;
	}
	//**********************************************************************************
	/**
  * Encoding of current ASN.1 block into ASN.1 encoded array (BER rules)
  * @param {boolean} [sizeOnly=false] Flag that we need only a size of encoding, not a real array of bytes
  * @returns {ArrayBuffer}
  */
	toBER(sizeOnly = false) {
		if (this.isHexOnly !== true) {
			this.error = "Flag \"isHexOnly\" is not set, abort";
			return new ArrayBuffer(0);
		}

		if (sizeOnly === true) return new ArrayBuffer(this.valueHex.byteLength);

		//noinspection JSCheckFunctionSignatures
		return this.valueHex.slice(0);
	}
	//**********************************************************************************
	/**
  * Convertion for the block to JSON object
  * @returns {Object}
  */
	toJSON() {
		let object = {};

		//region Seems at the moment (Sep 2016) there is no way how to check method is supported in "super" object
		try {
			object = super.toJSON();
		} catch (ex) {}
		//endregion

		object.blockName = this.constructor.blockName();
		object.isHexOnly = this.isHexOnly;
		object.valueHex = (0, _pvutils.bufferToHexCodes)(this.valueHex, 0, this.valueHex.byteLength);

		return object;
	}
	//**********************************************************************************
};
//**************************************************************************************
//endregion
//**************************************************************************************
//region Declaration of identification block class
//**************************************************************************************
class LocalIdentificationBlock extends LocalHexBlock(LocalBaseBlock) {
	//**********************************************************************************
	/**
  * Constructor for "LocalBaseBlock" class
  * @param {Object} [parameters={}]
  * @property {Object} [idBlock]
  */
	constructor(parameters = {}) {
		super();

		if ("idBlock" in parameters) {
			//region Properties from hexBlock class
			this.isHexOnly = (0, _pvutils.getParametersValue)(parameters.idBlock, "isHexOnly", false);
			this.valueHex = (0, _pvutils.getParametersValue)(parameters.idBlock, "valueHex", new ArrayBuffer(0));
			//endregion

			this.tagClass = (0, _pvutils.getParametersValue)(parameters.idBlock, "tagClass", -1);
			this.tagNumber = (0, _pvutils.getParametersValue)(parameters.idBlock, "tagNumber", -1);
			this.isConstructed = (0, _pvutils.getParametersValue)(parameters.idBlock, "isConstructed", false);
		} else {
			this.tagClass = -1;
			this.tagNumber = -1;
			this.isConstructed = false;
		}
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "identificationBlock";
	}
	//**********************************************************************************
	/**
  * Encoding of current ASN.1 block into ASN.1 encoded array (BER rules)
  * @param {boolean} [sizeOnly=false] Flag that we need only a size of encoding, not a real array of bytes
  * @returns {ArrayBuffer}
  */
	toBER(sizeOnly = false) {
		//region Initial variables
		let firstOctet = 0;
		let retBuf;
		let retView;
		//endregion

		switch (this.tagClass) {
			case 1:
				firstOctet |= 0x00; // UNIVERSAL
				break;
			case 2:
				firstOctet |= 0x40; // APPLICATION
				break;
			case 3:
				firstOctet |= 0x80; // CONTEXT-SPECIFIC
				break;
			case 4:
				firstOctet |= 0xC0; // PRIVATE
				break;
			default:
				this.error = "Unknown tag class";
				return new ArrayBuffer(0);
		}

		if (this.isConstructed) firstOctet |= 0x20;

		if (this.tagNumber < 31 && !this.isHexOnly) {
			retBuf = new ArrayBuffer(1);
			retView = new Uint8Array(retBuf);

			if (!sizeOnly) {
				let number = this.tagNumber;
				number &= 0x1F;
				firstOctet |= number;

				retView[0] = firstOctet;
			}

			return retBuf;
		}

		if (this.isHexOnly === false) {
			const encodedBuf = (0, _pvutils.utilToBase)(this.tagNumber, 7);
			const encodedView = new Uint8Array(encodedBuf);
			const size = encodedBuf.byteLength;

			retBuf = new ArrayBuffer(size + 1);
			retView = new Uint8Array(retBuf);
			retView[0] = firstOctet | 0x1F;

			if (!sizeOnly) {
				for (let i = 0; i < size - 1; i++) retView[i + 1] = encodedView[i] | 0x80;

				retView[size] = encodedView[size - 1];
			}

			return retBuf;
		}

		retBuf = new ArrayBuffer(this.valueHex.byteLength + 1);
		retView = new Uint8Array(retBuf);

		retView[0] = firstOctet | 0x1F;

		if (sizeOnly === false) {
			const curView = new Uint8Array(this.valueHex);

			for (let i = 0; i < curView.length - 1; i++) retView[i + 1] = curView[i] | 0x80;

			retView[this.valueHex.byteLength] = curView[curView.length - 1];
		}

		return retBuf;
	}
	//**********************************************************************************
	/**
  * Base function for converting block from BER encoded array of bytes
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  * @param {!number} inputOffset Offset in ASN.1 BER encoded array where decoding should be started
  * @param {!number} inputLength Maximum length of array of bytes which can be using in this function
  * @returns {number}
  */
	fromBER(inputBuffer, inputOffset, inputLength) {
		//region Basic check for parameters
		//noinspection JSCheckFunctionSignatures
		if ((0, _pvutils.checkBufferParams)(this, inputBuffer, inputOffset, inputLength) === false) return -1;
		//endregion

		//region Getting Uint8Array from ArrayBuffer
		const intBuffer = new Uint8Array(inputBuffer, inputOffset, inputLength);
		//endregion

		//region Initial checks
		if (intBuffer.length === 0) {
			this.error = "Zero buffer length";
			return -1;
		}
		//endregion

		//region Find tag class
		const tagClassMask = intBuffer[0] & 0xC0;

		switch (tagClassMask) {
			case 0x00:
				this.tagClass = 1; // UNIVERSAL
				break;
			case 0x40:
				this.tagClass = 2; // APPLICATION
				break;
			case 0x80:
				this.tagClass = 3; // CONTEXT-SPECIFIC
				break;
			case 0xC0:
				this.tagClass = 4; // PRIVATE
				break;
			default:
				this.error = "Unknown tag class";
				return -1;
		}
		//endregion

		//region Find it's constructed or not
		this.isConstructed = (intBuffer[0] & 0x20) === 0x20;
		//endregion

		//region Find tag number
		this.isHexOnly = false;

		const tagNumberMask = intBuffer[0] & 0x1F;

		//region Simple case (tag number < 31)
		if (tagNumberMask !== 0x1F) {
			this.tagNumber = tagNumberMask;
			this.blockLength = 1;
		}
		//endregion
		//region Tag number bigger or equal to 31
		else {
				let count = 1;

				this.valueHex = new ArrayBuffer(255);
				let tagNumberBufferMaxLength = 255;
				let intTagNumberBuffer = new Uint8Array(this.valueHex);

				//noinspection JSBitwiseOperatorUsage
				while (intBuffer[count] & 0x80) {
					intTagNumberBuffer[count - 1] = intBuffer[count] & 0x7F;
					count++;

					if (count >= intBuffer.length) {
						this.error = "End of input reached before message was fully decoded";
						return -1;
					}

					//region In case if tag number length is greater than 255 bytes (rare but possible case)
					if (count === tagNumberBufferMaxLength) {
						tagNumberBufferMaxLength += 255;

						const tempBuffer = new ArrayBuffer(tagNumberBufferMaxLength);
						const tempBufferView = new Uint8Array(tempBuffer);

						for (let i = 0; i < intTagNumberBuffer.length; i++) tempBufferView[i] = intTagNumberBuffer[i];

						this.valueHex = new ArrayBuffer(tagNumberBufferMaxLength);
						intTagNumberBuffer = new Uint8Array(this.valueHex);
					}
					//endregion
				}

				this.blockLength = count + 1;
				intTagNumberBuffer[count - 1] = intBuffer[count] & 0x7F; // Write last byte to buffer

				//region Cut buffer
				const tempBuffer = new ArrayBuffer(count);
				const tempBufferView = new Uint8Array(tempBuffer);

				for (let i = 0; i < count; i++) tempBufferView[i] = intTagNumberBuffer[i];

				this.valueHex = new ArrayBuffer(count);
				intTagNumberBuffer = new Uint8Array(this.valueHex);
				intTagNumberBuffer.set(tempBufferView);
				//endregion

				//region Try to convert long tag number to short form
				if (this.blockLength <= 9) this.tagNumber = (0, _pvutils.utilFromBase)(intTagNumberBuffer, 7);else {
					this.isHexOnly = true;
					this.warnings.push("Tag too long, represented as hex-coded");
				}
				//endregion
			}
		//endregion
		//endregion

		//region Check if constructed encoding was using for primitive type
		if (this.tagClass === 1 && this.isConstructed) {
			switch (this.tagNumber) {
				case 1: // Boolean
				case 2: // REAL
				case 5: // Null
				case 6: // OBJECT IDENTIFIER
				case 9: // REAL
				case 14: // Time
				case 23:
				case 24:
				case 31:
				case 32:
				case 33:
				case 34:
					this.error = "Constructed encoding used for primitive type";
					return -1;
				default:
			}
		}
		//endregion

		return inputOffset + this.blockLength; // Return current offset in input buffer
	}
	//**********************************************************************************
	/**
  * Convertion for the block to JSON object
  * @returns {{blockName: string,
  *  tagClass: number,
  *  tagNumber: number,
  *  isConstructed: boolean,
  *  isHexOnly: boolean,
  *  valueHex: ArrayBuffer,
  *  blockLength: number,
  *  error: string, warnings: Array.<string>,
  *  valueBeforeDecode: string}}
  */
	toJSON() {
		let object = {};

		//region Seems at the moment (Sep 2016) there is no way how to check method is supported in "super" object
		try {
			object = super.toJSON();
		} catch (ex) {}
		//endregion

		object.blockName = this.constructor.blockName();
		object.tagClass = this.tagClass;
		object.tagNumber = this.tagNumber;
		object.isConstructed = this.isConstructed;

		return object;
	}
	//**********************************************************************************
}
//**************************************************************************************
//endregion
//**************************************************************************************
//region Declaration of length block class
//**************************************************************************************
class LocalLengthBlock extends LocalBaseBlock {
	//**********************************************************************************
	/**
  * Constructor for "LocalLengthBlock" class
  * @param {Object} [parameters={}]
  * @property {Object} [lenBlock]
  */
	constructor(parameters = {}) {
		super();

		if ("lenBlock" in parameters) {
			this.isIndefiniteForm = (0, _pvutils.getParametersValue)(parameters.lenBlock, "isIndefiniteForm", false);
			this.longFormUsed = (0, _pvutils.getParametersValue)(parameters.lenBlock, "longFormUsed", false);
			this.length = (0, _pvutils.getParametersValue)(parameters.lenBlock, "length", 0);
		} else {
			this.isIndefiniteForm = false;
			this.longFormUsed = false;
			this.length = 0;
		}
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "lengthBlock";
	}
	//**********************************************************************************
	/**
  * Base function for converting block from BER encoded array of bytes
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  * @param {!number} inputOffset Offset in ASN.1 BER encoded array where decoding should be started
  * @param {!number} inputLength Maximum length of array of bytes which can be using in this function
  * @returns {number}
  */
	fromBER(inputBuffer, inputOffset, inputLength) {
		//region Basic check for parameters
		//noinspection JSCheckFunctionSignatures
		if ((0, _pvutils.checkBufferParams)(this, inputBuffer, inputOffset, inputLength) === false) return -1;
		//endregion

		//region Getting Uint8Array from ArrayBuffer
		const intBuffer = new Uint8Array(inputBuffer, inputOffset, inputLength);
		//endregion

		//region Initial checks
		if (intBuffer.length === 0) {
			this.error = "Zero buffer length";
			return -1;
		}

		if (intBuffer[0] === 0xFF) {
			this.error = "Length block 0xFF is reserved by standard";
			return -1;
		}
		//endregion

		//region Check for length form type
		this.isIndefiniteForm = intBuffer[0] === 0x80;
		//endregion

		//region Stop working in case of indefinite length form
		if (this.isIndefiniteForm === true) {
			this.blockLength = 1;
			return inputOffset + this.blockLength;
		}
		//endregion

		//region Check is long form of length encoding using
		this.longFormUsed = !!(intBuffer[0] & 0x80);
		//endregion

		//region Stop working in case of short form of length value
		if (this.longFormUsed === false) {
			this.length = intBuffer[0];
			this.blockLength = 1;
			return inputOffset + this.blockLength;
		}
		//endregion

		//region Calculate length value in case of long form
		const count = intBuffer[0] & 0x7F;

		if (count > 8) // Too big length value
			{
				this.error = "Too big integer";
				return -1;
			}

		if (count + 1 > intBuffer.length) {
			this.error = "End of input reached before message was fully decoded";
			return -1;
		}

		const lengthBufferView = new Uint8Array(count);

		for (let i = 0; i < count; i++) lengthBufferView[i] = intBuffer[i + 1];

		if (lengthBufferView[count - 1] === 0x00) this.warnings.push("Needlessly long encoded length");

		this.length = (0, _pvutils.utilFromBase)(lengthBufferView, 8);

		if (this.longFormUsed && this.length <= 127) this.warnings.push("Unneccesary usage of long length form");

		this.blockLength = count + 1;
		//endregion

		return inputOffset + this.blockLength; // Return current offset in input buffer
	}
	//**********************************************************************************
	/**
  * Encoding of current ASN.1 block into ASN.1 encoded array (BER rules)
  * @param {boolean} [sizeOnly=false] Flag that we need only a size of encoding, not a real array of bytes
  * @returns {ArrayBuffer}
  */
	toBER(sizeOnly = false) {
		//region Initial variables
		let retBuf;
		let retView;
		//endregion

		if (this.length > 127) this.longFormUsed = true;

		if (this.isIndefiniteForm) {
			retBuf = new ArrayBuffer(1);

			if (sizeOnly === false) {
				retView = new Uint8Array(retBuf);
				retView[0] = 0x80;
			}

			return retBuf;
		}

		if (this.longFormUsed === true) {
			const encodedBuf = (0, _pvutils.utilToBase)(this.length, 8);

			if (encodedBuf.byteLength > 127) {
				this.error = "Too big length";
				return new ArrayBuffer(0);
			}

			retBuf = new ArrayBuffer(encodedBuf.byteLength + 1);

			if (sizeOnly === true) return retBuf;

			const encodedView = new Uint8Array(encodedBuf);
			retView = new Uint8Array(retBuf);

			retView[0] = encodedBuf.byteLength | 0x80;

			for (let i = 0; i < encodedBuf.byteLength; i++) retView[i + 1] = encodedView[i];

			return retBuf;
		}

		retBuf = new ArrayBuffer(1);

		if (sizeOnly === false) {
			retView = new Uint8Array(retBuf);

			retView[0] = this.length;
		}

		return retBuf;
	}
	//**********************************************************************************
	/**
  * Convertion for the block to JSON object
  * @returns {{blockName, blockLength, error, warnings, valueBeforeDecode}|{blockName: string, blockLength: number, error: string, warnings: Array.<string>, valueBeforeDecode: string}}
  */
	toJSON() {
		let object = {};

		//region Seems at the moment (Sep 2016) there is no way how to check method is supported in "super" object
		try {
			object = super.toJSON();
		} catch (ex) {}
		//endregion

		object.blockName = this.constructor.blockName();
		object.isIndefiniteForm = this.isIndefiniteForm;
		object.longFormUsed = this.longFormUsed;
		object.length = this.length;

		return object;
	}
	//**********************************************************************************
}
//**************************************************************************************
//endregion
//**************************************************************************************
//region Declaration of value block class
//**************************************************************************************
class LocalValueBlock extends LocalBaseBlock {
	//**********************************************************************************
	/**
  * Constructor for "LocalValueBlock" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters);
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "valueBlock";
	}
	//**********************************************************************************
	//noinspection JSUnusedLocalSymbols,JSUnusedLocalSymbols,JSUnusedLocalSymbols
	/**
  * Base function for converting block from BER encoded array of bytes
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  * @param {!number} inputOffset Offset in ASN.1 BER encoded array where decoding should be started
  * @param {!number} inputLength Maximum length of array of bytes which can be using in this function
  * @returns {number}
  */
	fromBER(inputBuffer, inputOffset, inputLength) {
		//region Throw an exception for a function which needs to be specified in extended classes
		throw TypeError("User need to make a specific function in a class which extends \"LocalValueBlock\"");
		//endregion
	}
	//**********************************************************************************
	//noinspection JSUnusedLocalSymbols
	/**
  * Encoding of current ASN.1 block into ASN.1 encoded array (BER rules)
  * @param {boolean} [sizeOnly=false] Flag that we need only a size of encoding, not a real array of bytes
  * @returns {ArrayBuffer}
  */
	toBER(sizeOnly = false) {
		//region Throw an exception for a function which needs to be specified in extended classes
		throw TypeError("User need to make a specific function in a class which extends \"LocalValueBlock\"");
		//endregion
	}
	//**********************************************************************************
}
//**************************************************************************************
//endregion
//**************************************************************************************
//region Declaration of basic ASN.1 block class
//**************************************************************************************
class BaseBlock extends LocalBaseBlock {
	//**********************************************************************************
	/**
  * Constructor for "BaseBlock" class
  * @param {Object} [parameters={}]
  * @property {Object} [primitiveSchema]
  * @property {string} [name]
  * @property {boolean} [optional]
  * @param valueBlockType Type of value block
  */
	constructor(parameters = {}, valueBlockType = LocalValueBlock) {
		super(parameters);

		if ("name" in parameters) this.name = parameters.name;
		if ("optional" in parameters) this.optional = parameters.optional;
		if ("primitiveSchema" in parameters) this.primitiveSchema = parameters.primitiveSchema;

		this.idBlock = new LocalIdentificationBlock(parameters);
		this.lenBlock = new LocalLengthBlock(parameters);
		this.valueBlock = new valueBlockType(parameters);
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "BaseBlock";
	}
	//**********************************************************************************
	/**
  * Base function for converting block from BER encoded array of bytes
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  * @param {!number} inputOffset Offset in ASN.1 BER encoded array where decoding should be started
  * @param {!number} inputLength Maximum length of array of bytes which can be using in this function
  * @returns {number}
  */
	fromBER(inputBuffer, inputOffset, inputLength) {
		const resultOffset = this.valueBlock.fromBER(inputBuffer, inputOffset, this.lenBlock.isIndefiniteForm === true ? inputLength : this.lenBlock.length);
		if (resultOffset === -1) {
			this.error = this.valueBlock.error;
			return resultOffset;
		}

		if (this.idBlock.error.length === 0) this.blockLength += this.idBlock.blockLength;

		if (this.lenBlock.error.length === 0) this.blockLength += this.lenBlock.blockLength;

		if (this.valueBlock.error.length === 0) this.blockLength += this.valueBlock.blockLength;

		return resultOffset;
	}
	//**********************************************************************************
	/**
  * Encoding of current ASN.1 block into ASN.1 encoded array (BER rules)
  * @param {boolean} [sizeOnly=false] Flag that we need only a size of encoding, not a real array of bytes
  * @returns {ArrayBuffer}
  */
	toBER(sizeOnly = false) {
		let retBuf;

		const idBlockBuf = this.idBlock.toBER(sizeOnly);
		const valueBlockSizeBuf = this.valueBlock.toBER(true);

		this.lenBlock.length = valueBlockSizeBuf.byteLength;
		const lenBlockBuf = this.lenBlock.toBER(sizeOnly);

		retBuf = (0, _pvutils.utilConcatBuf)(idBlockBuf, lenBlockBuf);

		let valueBlockBuf;

		if (sizeOnly === false) valueBlockBuf = this.valueBlock.toBER(sizeOnly);else valueBlockBuf = new ArrayBuffer(this.lenBlock.length);

		retBuf = (0, _pvutils.utilConcatBuf)(retBuf, valueBlockBuf);

		if (this.lenBlock.isIndefiniteForm === true) {
			const indefBuf = new ArrayBuffer(2);

			if (sizeOnly === false) {
				const indefView = new Uint8Array(indefBuf);

				indefView[0] = 0x00;
				indefView[1] = 0x00;
			}

			retBuf = (0, _pvutils.utilConcatBuf)(retBuf, indefBuf);
		}

		return retBuf;
	}
	//**********************************************************************************
	/**
  * Convertion for the block to JSON object
  * @returns {{blockName, blockLength, error, warnings, valueBeforeDecode}|{blockName: string, blockLength: number, error: string, warnings: Array.<string>, valueBeforeDecode: string}}
  */
	toJSON() {
		let object = {};

		//region Seems at the moment (Sep 2016) there is no way how to check method is supported in "super" object
		try {
			object = super.toJSON();
		} catch (ex) {}
		//endregion

		object.idBlock = this.idBlock.toJSON();
		object.lenBlock = this.lenBlock.toJSON();
		object.valueBlock = this.valueBlock.toJSON();

		if ("name" in this) object.name = this.name;
		if ("optional" in this) object.optional = this.optional;
		if ("primitiveSchema" in this) object.primitiveSchema = this.primitiveSchema.toJSON();

		return object;
	}
	//**********************************************************************************
}
exports.BaseBlock = BaseBlock; //**************************************************************************************
//endregion
//**************************************************************************************
//region Declaration of basic block for all PRIMITIVE types
//**************************************************************************************

class LocalPrimitiveValueBlock extends LocalValueBlock {
	//**********************************************************************************
	/**
  * Constructor for "LocalPrimitiveValueBlock" class
  * @param {Object} [parameters={}]
  * @property {ArrayBuffer} [valueBeforeDecode]
  */
	constructor(parameters = {}) {
		super(parameters);

		//region Variables from "hexBlock" class
		if ("valueHex" in parameters) this.valueHex = parameters.valueHex.slice(0);else this.valueHex = new ArrayBuffer(0);

		this.isHexOnly = (0, _pvutils.getParametersValue)(parameters, "isHexOnly", true);
		//endregion
	}
	//**********************************************************************************
	/**
  * Base function for converting block from BER encoded array of bytes
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  * @param {!number} inputOffset Offset in ASN.1 BER encoded array where decoding should be started
  * @param {!number} inputLength Maximum length of array of bytes which can be using in this function
  * @returns {number}
  */
	fromBER(inputBuffer, inputOffset, inputLength) {
		//region Basic check for parameters
		//noinspection JSCheckFunctionSignatures
		if ((0, _pvutils.checkBufferParams)(this, inputBuffer, inputOffset, inputLength) === false) return -1;
		//endregion

		//region Getting Uint8Array from ArrayBuffer
		const intBuffer = new Uint8Array(inputBuffer, inputOffset, inputLength);
		//endregion

		//region Initial checks
		if (intBuffer.length === 0) {
			this.warnings.push("Zero buffer length");
			return inputOffset;
		}
		//endregion

		//region Copy input buffer into internal buffer
		this.valueHex = new ArrayBuffer(intBuffer.length);
		const valueHexView = new Uint8Array(this.valueHex);

		for (let i = 0; i < intBuffer.length; i++) valueHexView[i] = intBuffer[i];
		//endregion

		this.blockLength = inputLength;

		return inputOffset + inputLength;
	}
	//**********************************************************************************
	//noinspection JSUnusedLocalSymbols
	/**
  * Encoding of current ASN.1 block into ASN.1 encoded array (BER rules)
  * @param {boolean} [sizeOnly=false] Flag that we need only a size of encoding, not a real array of bytes
  * @returns {ArrayBuffer}
  */
	toBER(sizeOnly = false) {
		return this.valueHex.slice(0);
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "PrimitiveValueBlock";
	}
	//**********************************************************************************
	/**
  * Convertion for the block to JSON object
  * @returns {{blockName, blockLength, error, warnings, valueBeforeDecode}|{blockName: string, blockLength: number, error: string, warnings: Array.<string>, valueBeforeDecode: string}}
  */
	toJSON() {
		let object = {};

		//region Seems at the moment (Sep 2016) there is no way how to check method is supported in "super" object
		try {
			object = super.toJSON();
		} catch (ex) {}
		//endregion

		object.valueHex = (0, _pvutils.bufferToHexCodes)(this.valueHex, 0, this.valueHex.byteLength);
		object.isHexOnly = this.isHexOnly;

		return object;
	}
	//**********************************************************************************
}
//**************************************************************************************
class Primitive extends BaseBlock {
	//**********************************************************************************
	/**
  * Constructor for "Primitive" class
  * @param {Object} [parameters={}]
  * @property {ArrayBuffer} [valueHex]
  */
	constructor(parameters = {}) {
		super(parameters, LocalPrimitiveValueBlock);

		this.idBlock.isConstructed = false;
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "PRIMITIVE";
	}
	//**********************************************************************************
}
exports.Primitive = Primitive; //**************************************************************************************
//endregion
//**************************************************************************************
//region Declaration of basic block for all CONSTRUCTED types
//**************************************************************************************

class LocalConstructedValueBlock extends LocalValueBlock {
	//**********************************************************************************
	/**
  * Constructor for "LocalConstructedValueBlock" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters);

		this.value = (0, _pvutils.getParametersValue)(parameters, "value", []);
		this.isIndefiniteForm = (0, _pvutils.getParametersValue)(parameters, "isIndefiniteForm", false);
	}
	//**********************************************************************************
	/**
  * Base function for converting block from BER encoded array of bytes
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  * @param {!number} inputOffset Offset in ASN.1 BER encoded array where decoding should be started
  * @param {!number} inputLength Maximum length of array of bytes which can be using in this function
  * @returns {number}
  */
	fromBER(inputBuffer, inputOffset, inputLength) {
		//region Store initial offset and length
		const initialOffset = inputOffset;
		const initialLength = inputLength;
		//endregion

		//region Basic check for parameters
		//noinspection JSCheckFunctionSignatures
		if ((0, _pvutils.checkBufferParams)(this, inputBuffer, inputOffset, inputLength) === false) return -1;
		//endregion

		//region Getting Uint8Array from ArrayBuffer
		const intBuffer = new Uint8Array(inputBuffer, inputOffset, inputLength);
		//endregion

		//region Initial checks
		if (intBuffer.length === 0) {
			this.warnings.push("Zero buffer length");
			return inputOffset;
		}
		//endregion

		//region Aux function
		function checkLen(indefiniteLength, length) {
			if (indefiniteLength === true) return 1;

			return length;
		}
		//endregion

		let currentOffset = inputOffset;

		while (checkLen(this.isIndefiniteForm, inputLength) > 0) {
			const returnObject = LocalFromBER(inputBuffer, currentOffset, inputLength);
			if (returnObject.offset === -1) {
				this.error = returnObject.result.error;
				this.warnings.concat(returnObject.result.warnings);
				return -1;
			}

			currentOffset = returnObject.offset;

			this.blockLength += returnObject.result.blockLength;
			inputLength -= returnObject.result.blockLength;

			this.value.push(returnObject.result);

			if (this.isIndefiniteForm === true && returnObject.result.constructor.blockName() === EndOfContent.blockName()) break;
		}

		if (this.isIndefiniteForm === true) {
			if (this.value[this.value.length - 1].constructor.blockName() === EndOfContent.blockName()) this.value.pop();else this.warnings.push("No EndOfContent block encoded");
		}

		//region Copy "inputBuffer" to "valueBeforeDecode"
		this.valueBeforeDecode = inputBuffer.slice(initialOffset, initialOffset + initialLength);
		//endregion

		return currentOffset;
	}
	//**********************************************************************************
	/**
  * Encoding of current ASN.1 block into ASN.1 encoded array (BER rules)
  * @param {boolean} [sizeOnly=false] Flag that we need only a size of encoding, not a real array of bytes
  * @returns {ArrayBuffer}
  */
	toBER(sizeOnly = false) {
		let retBuf = new ArrayBuffer(0);

		for (let i = 0; i < this.value.length; i++) {
			const valueBuf = this.value[i].toBER(sizeOnly);
			retBuf = (0, _pvutils.utilConcatBuf)(retBuf, valueBuf);
		}

		return retBuf;
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "ConstructedValueBlock";
	}
	//**********************************************************************************
	/**
  * Convertion for the block to JSON object
  * @returns {{blockName, blockLength, error, warnings, valueBeforeDecode}|{blockName: string, blockLength: number, error: string, warnings: Array.<string>, valueBeforeDecode: string}}
  */
	toJSON() {
		let object = {};

		//region Seems at the moment (Sep 2016) there is no way how to check method is supported in "super" object
		try {
			object = super.toJSON();
		} catch (ex) {}
		//endregion

		object.isIndefiniteForm = this.isIndefiniteForm;
		object.value = [];
		for (let i = 0; i < this.value.length; i++) object.value.push(this.value[i].toJSON());

		return object;
	}
	//**********************************************************************************
}
//**************************************************************************************
class Constructed extends BaseBlock {
	//**********************************************************************************
	/**
  * Constructor for "Constructed" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters, LocalConstructedValueBlock);

		this.idBlock.isConstructed = true;
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "CONSTRUCTED";
	}
	//**********************************************************************************
	/**
  * Base function for converting block from BER encoded array of bytes
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  * @param {!number} inputOffset Offset in ASN.1 BER encoded array where decoding should be started
  * @param {!number} inputLength Maximum length of array of bytes which can be using in this function
  * @returns {number}
  */
	fromBER(inputBuffer, inputOffset, inputLength) {
		this.valueBlock.isIndefiniteForm = this.lenBlock.isIndefiniteForm;

		const resultOffset = this.valueBlock.fromBER(inputBuffer, inputOffset, this.lenBlock.isIndefiniteForm === true ? inputLength : this.lenBlock.length);
		if (resultOffset === -1) {
			this.error = this.valueBlock.error;
			return resultOffset;
		}

		if (this.idBlock.error.length === 0) this.blockLength += this.idBlock.blockLength;

		if (this.lenBlock.error.length === 0) this.blockLength += this.lenBlock.blockLength;

		if (this.valueBlock.error.length === 0) this.blockLength += this.valueBlock.blockLength;

		return resultOffset;
	}
	//**********************************************************************************
}
exports.Constructed = Constructed; //**************************************************************************************
//endregion
//**************************************************************************************
//region Declaration of ASN.1 EndOfContent type class
//**************************************************************************************

class LocalEndOfContentValueBlock extends LocalValueBlock {
	//**********************************************************************************
	/**
  * Constructor for "LocalEndOfContentValueBlock" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters);
	}
	//**********************************************************************************
	//noinspection JSUnusedLocalSymbols,JSUnusedLocalSymbols
	/**
  * Base function for converting block from BER encoded array of bytes
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  * @param {!number} inputOffset Offset in ASN.1 BER encoded array where decoding should be started
  * @param {!number} inputLength Maximum length of array of bytes which can be using in this function
  * @returns {number}
  */
	fromBER(inputBuffer, inputOffset, inputLength) {
		//region There is no "value block" for EndOfContent type and we need to return the same offset
		return inputOffset;
		//endregion
	}
	//**********************************************************************************
	//noinspection JSUnusedLocalSymbols
	/**
  * Encoding of current ASN.1 block into ASN.1 encoded array (BER rules)
  * @param {boolean} [sizeOnly=false] Flag that we need only a size of encoding, not a real array of bytes
  * @returns {ArrayBuffer}
  */
	toBER(sizeOnly = false) {
		return new ArrayBuffer(0);
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "EndOfContentValueBlock";
	}
	//**********************************************************************************
}
//**************************************************************************************
class EndOfContent extends BaseBlock {
	//**********************************************************************************
	constructor(paramaters = {}) {
		super(paramaters, LocalEndOfContentValueBlock);

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 0; // EndOfContent
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "EndOfContent";
	}
	//**********************************************************************************
}
exports.EndOfContent = EndOfContent; //**************************************************************************************
//endregion
//**************************************************************************************
//region Declaration of ASN.1 Boolean type class
//**************************************************************************************

class LocalBooleanValueBlock extends LocalValueBlock {
	//**********************************************************************************
	/**
  * Constructor for "LocalBooleanValueBlock" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters);

		this.value = (0, _pvutils.getParametersValue)(parameters, "value", false);
		this.isHexOnly = (0, _pvutils.getParametersValue)(parameters, "isHexOnly", false);

		if ("valueHex" in parameters) this.valueHex = parameters.valueHex.slice(0);else {
			this.valueHex = new ArrayBuffer(1);
			if (this.value === true) {
				const view = new Uint8Array(this.valueHex);
				view[0] = 0xFF;
			}
		}
	}
	//**********************************************************************************
	/**
  * Base function for converting block from BER encoded array of bytes
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  * @param {!number} inputOffset Offset in ASN.1 BER encoded array where decoding should be started
  * @param {!number} inputLength Maximum length of array of bytes which can be using in this function
  * @returns {number} Offset after least decoded byte
  */
	fromBER(inputBuffer, inputOffset, inputLength) {
		//region Basic check for parameters
		//noinspection JSCheckFunctionSignatures
		if ((0, _pvutils.checkBufferParams)(this, inputBuffer, inputOffset, inputLength) === false) return -1;
		//endregion

		//region Getting Uint8Array from ArrayBuffer
		const intBuffer = new Uint8Array(inputBuffer, inputOffset, inputLength);
		//endregion

		if (inputLength > 1) this.warnings.push("Boolean value encoded in more then 1 octet");

		this.isHexOnly = true;

		//region Copy input buffer to internal array
		this.valueHex = new ArrayBuffer(intBuffer.length);
		const view = new Uint8Array(this.valueHex);

		for (let i = 0; i < intBuffer.length; i++) view[i] = intBuffer[i];
		//endregion

		if (_pvutils.utilDecodeTC.call(this) !== 0) this.value = true;else this.value = false;

		this.blockLength = inputLength;

		return inputOffset + inputLength;
	}
	//**********************************************************************************
	//noinspection JSUnusedLocalSymbols
	/**
  * Encoding of current ASN.1 block into ASN.1 encoded array (BER rules)
  * @param {boolean} [sizeOnly=false] Flag that we need only a size of encoding, not a real array of bytes
  * @returns {ArrayBuffer}
  */
	toBER(sizeOnly = false) {
		return this.valueHex;
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "BooleanValueBlock";
	}
	//**********************************************************************************
	/**
  * Convertion for the block to JSON object
  * @returns {{blockName, blockLength, error, warnings, valueBeforeDecode}|{blockName: string, blockLength: number, error: string, warnings: Array.<string>, valueBeforeDecode: string}}
  */
	toJSON() {
		let object = {};

		//region Seems at the moment (Sep 2016) there is no way how to check method is supported in "super" object
		try {
			object = super.toJSON();
		} catch (ex) {}
		//endregion

		object.value = this.value;
		object.isHexOnly = this.isHexOnly;
		object.valueHex = (0, _pvutils.bufferToHexCodes)(this.valueHex, 0, this.valueHex.byteLength);

		return object;
	}
	//**********************************************************************************
}
//**************************************************************************************
class Boolean extends BaseBlock {
	//**********************************************************************************
	/**
  * Constructor for "Boolean" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters, LocalBooleanValueBlock);

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 1; // Boolean
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "Boolean";
	}
	//**********************************************************************************
}
exports.Boolean = Boolean; //**************************************************************************************
//endregion
//**************************************************************************************
//region Declaration of ASN.1 Sequence and Set type classes
//**************************************************************************************

class Sequence extends Constructed {
	//**********************************************************************************
	/**
  * Constructor for "Sequence" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters);

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 16; // Sequence
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "Sequence";
	}
	//**********************************************************************************
}
exports.Sequence = Sequence; //**************************************************************************************

class Set extends Constructed {
	//**********************************************************************************
	/**
  * Constructor for "Set" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters);

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 17; // Set
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "Set";
	}
	//**********************************************************************************
}
exports.Set = Set; //**************************************************************************************
//endregion
//**************************************************************************************
//region Declaration of ASN.1 Null type class
//**************************************************************************************

class Null extends BaseBlock {
	//**********************************************************************************
	/**
  * Constructor for "Null" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters, LocalBaseBlock); // We will not have a call to "Null value block" because of specified "fromBER" and "toBER" functions

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 5; // Null
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "Null";
	}
	//**********************************************************************************
	//noinspection JSUnusedLocalSymbols
	/**
  * Base function for converting block from BER encoded array of bytes
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  * @param {!number} inputOffset Offset in ASN.1 BER encoded array where decoding should be started
  * @param {!number} inputLength Maximum length of array of bytes which can be using in this function
  * @returns {number} Offset after least decoded byte
  */
	fromBER(inputBuffer, inputOffset, inputLength) {
		if (this.lenBlock.length > 0) this.warnings.push("Non-zero length of value block for Null type");

		if (this.idBlock.error.length === 0) this.blockLength += this.idBlock.blockLength;

		if (this.lenBlock.error.length === 0) this.blockLength += this.lenBlock.blockLength;

		this.blockLength += inputLength;

		if (inputOffset + inputLength > inputBuffer.byteLength) {
			this.error = "End of input reached before message was fully decoded (inconsistent offset and length values)";
			return -1;
		}

		return inputOffset + inputLength;
	}
	//**********************************************************************************
	/**
  * Encoding of current ASN.1 block into ASN.1 encoded array (BER rules)
  * @param {boolean} [sizeOnly=false] Flag that we need only a size of encoding, not a real array of bytes
  * @returns {ArrayBuffer}
  */
	toBER(sizeOnly = false) {
		const retBuf = new ArrayBuffer(2);

		if (sizeOnly === true) return retBuf;

		const retView = new Uint8Array(retBuf);
		retView[0] = 0x05;
		retView[1] = 0x00;

		return retBuf;
	}
	//**********************************************************************************
}
exports.Null = Null; //**************************************************************************************
//endregion
//**************************************************************************************
//region Declaration of ASN.1 OctetString type class
//**************************************************************************************

class LocalOctetStringValueBlock extends LocalHexBlock(LocalConstructedValueBlock) {
	//**********************************************************************************
	/**
  * Constructor for "LocalOctetStringValueBlock" class
  * @param {Object} [parameters={}]
  * @property {ArrayBuffer} [valueHex]
  */
	constructor(parameters = {}) {
		super(parameters);

		this.isConstructed = (0, _pvutils.getParametersValue)(parameters, "isConstructed", false);
	}
	//**********************************************************************************
	/**
  * Base function for converting block from BER encoded array of bytes
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  * @param {!number} inputOffset Offset in ASN.1 BER encoded array where decoding should be started
  * @param {!number} inputLength Maximum length of array of bytes which can be using in this function
  * @returns {number} Offset after least decoded byte
  */
	fromBER(inputBuffer, inputOffset, inputLength) {
		let resultOffset = 0;

		if (this.isConstructed === true) {
			this.isHexOnly = false;

			resultOffset = LocalConstructedValueBlock.prototype.fromBER.call(this, inputBuffer, inputOffset, inputLength);
			if (resultOffset === -1) return resultOffset;

			for (let i = 0; i < this.value.length; i++) {
				const currentBlockName = this.value[i].constructor.blockName();

				if (currentBlockName === EndOfContent.blockName()) {
					if (this.isIndefiniteForm === true) break;else {
						this.error = "EndOfContent is unexpected, OCTET STRING may consists of OCTET STRINGs only";
						return -1;
					}
				}

				if (currentBlockName !== OctetString.blockName()) {
					this.error = "OCTET STRING may consists of OCTET STRINGs only";
					return -1;
				}
			}
		} else {
			this.isHexOnly = true;

			resultOffset = super.fromBER(inputBuffer, inputOffset, inputLength);
			this.blockLength = inputLength;
		}

		return resultOffset;
	}
	//**********************************************************************************
	/**
  * Encoding of current ASN.1 block into ASN.1 encoded array (BER rules)
  * @param {boolean} [sizeOnly=false] Flag that we need only a size of encoding, not a real array of bytes
  * @returns {ArrayBuffer}
  */
	toBER(sizeOnly = false) {
		if (this.isConstructed === true) return LocalConstructedValueBlock.prototype.toBER.call(this, sizeOnly);

		let retBuf = new ArrayBuffer(this.valueHex.byteLength);

		if (sizeOnly === true) return retBuf;

		if (this.valueHex.byteLength === 0) return retBuf;

		retBuf = this.valueHex.slice(0);

		return retBuf;
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "OctetStringValueBlock";
	}
	//**********************************************************************************
	toJSON() {
		let object = {};

		//region Seems at the moment (Sep 2016) there is no way how to check method is supported in "super" object
		try {
			object = super.toJSON();
		} catch (ex) {}
		//endregion

		object.isConstructed = this.isConstructed;
		object.isHexOnly = this.isHexOnly;
		object.valueHex = (0, _pvutils.bufferToHexCodes)(this.valueHex, 0, this.valueHex.byteLength);

		return object;
	}
	//**********************************************************************************
}
//**************************************************************************************
class OctetString extends BaseBlock {
	//**********************************************************************************
	/**
  * Constructor for "OctetString" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters, LocalOctetStringValueBlock);

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 4; // OctetString
	}
	//**********************************************************************************
	/**
  * Base function for converting block from BER encoded array of bytes
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  * @param {!number} inputOffset Offset in ASN.1 BER encoded array where decoding should be started
  * @param {!number} inputLength Maximum length of array of bytes which can be using in this function
  * @returns {number} Offset after least decoded byte
  */
	fromBER(inputBuffer, inputOffset, inputLength) {
		this.valueBlock.isConstructed = this.idBlock.isConstructed;
		this.valueBlock.isIndefiniteForm = this.lenBlock.isIndefiniteForm;

		//region Ability to encode empty OCTET STRING
		if (inputLength === 0) {
			if (this.idBlock.error.length === 0) this.blockLength += this.idBlock.blockLength;

			if (this.lenBlock.error.length === 0) this.blockLength += this.lenBlock.blockLength;

			return inputOffset;
		}
		//endregion

		return super.fromBER(inputBuffer, inputOffset, inputLength);
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "OctetString";
	}
	//**********************************************************************************
	//noinspection JSUnusedGlobalSymbols
	/**
  * Checking that two OCTETSTRINGs are equal
  * @param {OctetString} octetString
  */
	isEqual(octetString) {
		//region Check input type
		if (octetString instanceof OctetString === false) return false;
		//endregion

		//region Compare two JSON strings
		if (JSON.stringify(this) !== JSON.stringify(octetString)) return false;
		//endregion

		return true;
	}
	//**********************************************************************************
}
exports.OctetString = OctetString; //**************************************************************************************
//endregion
//**************************************************************************************
//region Declaration of ASN.1 BitString type class
//**************************************************************************************

class LocalBitStringValueBlock extends LocalHexBlock(LocalConstructedValueBlock) {
	//**********************************************************************************
	/**
  * Constructor for "LocalBitStringValueBlock" class
  * @param {Object} [parameters={}]
  * @property {ArrayBuffer} [valueHex]
  */
	constructor(parameters = {}) {
		super(parameters);

		this.unusedBits = (0, _pvutils.getParametersValue)(parameters, "unusedBits", 0);
		this.isConstructed = (0, _pvutils.getParametersValue)(parameters, "isConstructed", false);
		this.blockLength = this.valueHex.byteLength;
	}
	//**********************************************************************************
	/**
  * Base function for converting block from BER encoded array of bytes
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  * @param {!number} inputOffset Offset in ASN.1 BER encoded array where decoding should be started
  * @param {!number} inputLength Maximum length of array of bytes which can be using in this function
  * @returns {number} Offset after least decoded byte
  */
	fromBER(inputBuffer, inputOffset, inputLength) {
		//region Ability to decode zero-length BitString value
		if (inputLength === 0) return inputOffset;
		//endregion

		let resultOffset = -1;

		//region If the BISTRING supposed to be a constructed value
		if (this.isConstructed === true) {
			resultOffset = LocalConstructedValueBlock.prototype.fromBER.call(this, inputBuffer, inputOffset, inputLength);
			if (resultOffset === -1) return resultOffset;

			for (let i = 0; i < this.value.length; i++) {
				const currentBlockName = this.value[i].constructor.blockName();

				if (currentBlockName === EndOfContent.blockName()) {
					if (this.isIndefiniteForm === true) break;else {
						this.error = "EndOfContent is unexpected, BIT STRING may consists of BIT STRINGs only";
						return -1;
					}
				}

				if (currentBlockName !== BitString.blockName()) {
					this.error = "BIT STRING may consists of BIT STRINGs only";
					return -1;
				}

				if (this.unusedBits > 0 && this.value[i].valueBlock.unusedBits > 0) {
					this.error = "Usign of \"unused bits\" inside constructive BIT STRING allowed for least one only";
					return -1;
				}

				this.unusedBits = this.value[i].valueBlock.unusedBits;
				if (this.unusedBits > 7) {
					this.error = "Unused bits for BitString must be in range 0-7";
					return -1;
				}
			}

			return resultOffset;
		}
		//endregion
		//region If the BitString supposed to be a primitive value
		//region Basic check for parameters
		//noinspection JSCheckFunctionSignatures
		if ((0, _pvutils.checkBufferParams)(this, inputBuffer, inputOffset, inputLength) === false) return -1;
		//endregion

		const intBuffer = new Uint8Array(inputBuffer, inputOffset, inputLength);

		this.unusedBits = intBuffer[0];

		if (this.unusedBits > 7) {
			this.error = "Unused bits for BitString must be in range 0-7";
			return -1;
		}

		//region Copy input buffer to internal buffer
		this.valueHex = new ArrayBuffer(intBuffer.length - 1);
		const view = new Uint8Array(this.valueHex);
		for (let i = 0; i < inputLength - 1; i++) view[i] = intBuffer[i + 1];
		//endregion

		this.blockLength = intBuffer.length;

		return inputOffset + inputLength;
		//endregion
	}
	//**********************************************************************************
	/**
  * Encoding of current ASN.1 block into ASN.1 encoded array (BER rules)
  * @param {boolean} [sizeOnly=false] Flag that we need only a size of encoding, not a real array of bytes
  * @returns {ArrayBuffer}
  */
	toBER(sizeOnly = false) {
		if (this.isConstructed === true) return LocalConstructedValueBlock.prototype.toBER.call(this, sizeOnly);

		if (sizeOnly === true) return new ArrayBuffer(this.valueHex.byteLength + 1);

		if (this.valueHex.byteLength === 0) return new ArrayBuffer(0);

		const curView = new Uint8Array(this.valueHex);

		const retBuf = new ArrayBuffer(this.valueHex.byteLength + 1);
		const retView = new Uint8Array(retBuf);

		retView[0] = this.unusedBits;

		for (let i = 0; i < this.valueHex.byteLength; i++) retView[i + 1] = curView[i];

		return retBuf;
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "BitStringValueBlock";
	}
	//**********************************************************************************
	/**
  * Convertion for the block to JSON object
  * @returns {{blockName, blockLength, error, warnings, valueBeforeDecode}|{blockName: string, blockLength: number, error: string, warnings: Array.<string>, valueBeforeDecode: string}}
  */
	toJSON() {
		let object = {};

		//region Seems at the moment (Sep 2016) there is no way how to check method is supported in "super" object
		try {
			object = super.toJSON();
		} catch (ex) {}
		//endregion

		object.unusedBits = this.unusedBits;
		object.isConstructed = this.isConstructed;
		object.isHexOnly = this.isHexOnly;
		object.valueHex = (0, _pvutils.bufferToHexCodes)(this.valueHex, 0, this.valueHex.byteLength);

		return object;
	}
	//**********************************************************************************
}
//**************************************************************************************
class BitString extends BaseBlock {
	//**********************************************************************************
	/**
  * Constructor for "BitString" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters, LocalBitStringValueBlock);

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 3; // BitString
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "BitString";
	}
	//**********************************************************************************
	/**
  * Base function for converting block from BER encoded array of bytes
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  * @param {!number} inputOffset Offset in ASN.1 BER encoded array where decoding should be started
  * @param {!number} inputLength Maximum length of array of bytes which can be using in this function
  * @returns {number} Offset after least decoded byte
  */
	fromBER(inputBuffer, inputOffset, inputLength) {
		//region Ability to encode empty BitString
		if (inputLength === 0) return inputOffset;
		//endregion

		this.valueBlock.isConstructed = this.idBlock.isConstructed;
		this.valueBlock.isIndefiniteForm = this.lenBlock.isIndefiniteForm;

		return super.fromBER(inputBuffer, inputOffset, inputLength);
	}
	//**********************************************************************************
	/**
  * Checking that two BITSTRINGs are equal
  * @param {BitString} bitString
  */
	isEqual(bitString) {
		//region Check input type
		if (bitString instanceof BitString === false) return false;
		//endregion

		//region Compare two JSON strings
		if (JSON.stringify(this) !== JSON.stringify(bitString)) return false;
		//endregion

		return true;
	}
	//**********************************************************************************
}
exports.BitString = BitString; //**************************************************************************************
//endregion
//**************************************************************************************
//region Declaration of ASN.1 Integer type class
//**************************************************************************************
/**
 * @extends LocalValueBlock
 */

class LocalIntegerValueBlock extends LocalHexBlock(LocalValueBlock) {
	//**********************************************************************************
	/**
  * Constructor for "LocalIntegerValueBlock" class
  * @param {Object} [parameters={}]
  * @property {ArrayBuffer} [valueHex]
  */
	constructor(parameters = {}) {
		super(parameters);

		if ("value" in parameters) this.valueDec = parameters.value;
	}
	//**********************************************************************************
	/**
  * Setter for "valueHex"
  * @param {ArrayBuffer} _value
  */
	set valueHex(_value) {
		this._valueHex = _value.slice(0);

		if (_value.byteLength >= 4) {
			this.warnings.push("Too big Integer for decoding, hex only");
			this.isHexOnly = true;
			this._valueDec = 0;
		} else {
			this.isHexOnly = false;

			if (_value.byteLength > 0) this._valueDec = _pvutils.utilDecodeTC.call(this);
		}
	}
	//**********************************************************************************
	/**
  * Getter for "valueHex"
  * @returns {ArrayBuffer}
  */
	get valueHex() {
		return this._valueHex;
	}
	//**********************************************************************************
	/**
  * Getter for "valueDec"
  * @param {number} _value
  */
	set valueDec(_value) {
		this._valueDec = _value;

		this.isHexOnly = false;
		this._valueHex = (0, _pvutils.utilEncodeTC)(_value);
	}
	//**********************************************************************************
	/**
  * Getter for "valueDec"
  * @returns {number}
  */
	get valueDec() {
		return this._valueDec;
	}
	//**********************************************************************************
	/**
  * Base function for converting block from DER encoded array of bytes
  * @param {!ArrayBuffer} inputBuffer ASN.1 DER encoded array
  * @param {!number} inputOffset Offset in ASN.1 DER encoded array where decoding should be started
  * @param {!number} inputLength Maximum length of array of bytes which can be using in this function
  * @param {number} [expectedLength=0] Expected length of converted "valueHex" buffer
  * @returns {number} Offset after least decoded byte
  */
	fromDER(inputBuffer, inputOffset, inputLength, expectedLength = 0) {
		const offset = this.fromBER(inputBuffer, inputOffset, inputLength);
		if (offset === -1) return offset;

		const view = new Uint8Array(this._valueHex);

		if (view[0] === 0x00 && (view[1] & 0x80) !== 0) {
			const updatedValueHex = new ArrayBuffer(this._valueHex.byteLength - 1);
			const updatedView = new Uint8Array(updatedValueHex);

			updatedView.set(new Uint8Array(this._valueHex, 1, this._valueHex.byteLength - 1));

			this._valueHex = updatedValueHex.slice(0);
		} else {
			if (expectedLength !== 0) {
				if (this._valueHex.byteLength < expectedLength) {
					if (expectedLength - this._valueHex.byteLength > 1) expectedLength = this._valueHex.byteLength + 1;

					const updatedValueHex = new ArrayBuffer(expectedLength);
					const updatedView = new Uint8Array(updatedValueHex);

					updatedView.set(view, expectedLength - this._valueHex.byteLength);

					this._valueHex = updatedValueHex.slice(0);
				}
			}
		}

		return offset;
	}
	//**********************************************************************************
	/**
  * Encoding of current ASN.1 block into ASN.1 encoded array (DER rules)
  * @param {boolean} [sizeOnly=false] Flag that we need only a size of encoding, not a real array of bytes
  * @returns {ArrayBuffer}
  */
	toDER(sizeOnly = false) {
		const view = new Uint8Array(this._valueHex);

		switch (true) {
			case (view[0] & 0x80) !== 0:
				{
					const updatedValueHex = new ArrayBuffer(this._valueHex.byteLength + 1);
					const updatedView = new Uint8Array(updatedValueHex);

					updatedView[0] = 0x00;
					updatedView.set(view, 1);

					this._valueHex = updatedValueHex.slice(0);
				}
				break;
			case view[0] === 0x00 && (view[1] & 0x80) === 0:
				{
					const updatedValueHex = new ArrayBuffer(this._valueHex.byteLength - 1);
					const updatedView = new Uint8Array(updatedValueHex);

					updatedView.set(new Uint8Array(this._valueHex, 1, this._valueHex.byteLength - 1));

					this._valueHex = updatedValueHex.slice(0);
				}
				break;
			default:
		}

		return this.toBER(sizeOnly);
	}
	//**********************************************************************************
	/**
  * Base function for converting block from BER encoded array of bytes
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  * @param {!number} inputOffset Offset in ASN.1 BER encoded array where decoding should be started
  * @param {!number} inputLength Maximum length of array of bytes which can be using in this function
  * @returns {number} Offset after least decoded byte
  */
	fromBER(inputBuffer, inputOffset, inputLength) {
		const resultOffset = super.fromBER(inputBuffer, inputOffset, inputLength);
		if (resultOffset === -1) return resultOffset;

		this.blockLength = inputLength;

		return inputOffset + inputLength;
	}
	//**********************************************************************************
	/**
  * Encoding of current ASN.1 block into ASN.1 encoded array (BER rules)
  * @param {boolean} [sizeOnly=false] Flag that we need only a size of encoding, not a real array of bytes
  * @returns {ArrayBuffer}
  */
	toBER(sizeOnly = false) {
		//noinspection JSCheckFunctionSignatures
		return this.valueHex.slice(0);
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "IntegerValueBlock";
	}
	//**********************************************************************************
	//noinspection JSUnusedGlobalSymbols
	/**
  * Convertion for the block to JSON object
  * @returns {Object}
  */
	toJSON() {
		let object = {};

		//region Seems at the moment (Sep 2016) there is no way how to check method is supported in "super" object
		try {
			object = super.toJSON();
		} catch (ex) {}
		//endregion

		object.valueDec = this.valueDec;

		return object;
	}
	//**********************************************************************************
	/**
  * Convert current value to decimal string representation
  */
	toString() {
		//region Aux functions
		function viewAdd(first, second) {
			//region Initial variables
			const c = new Uint8Array([0]);

			let firstView = new Uint8Array(first);
			let secondView = new Uint8Array(second);

			let firstViewCopy = firstView.slice(0);
			const firstViewCopyLength = firstViewCopy.length - 1;
			let secondViewCopy = secondView.slice(0);
			const secondViewCopyLength = secondViewCopy.length - 1;

			let value = 0;

			const max = secondViewCopyLength < firstViewCopyLength ? firstViewCopyLength : secondViewCopyLength;

			let counter = 0;
			//endregion

			for (let i = max; i >= 0; i--, counter++) {
				switch (true) {
					case counter < secondViewCopy.length:
						value = firstViewCopy[firstViewCopyLength - counter] + secondViewCopy[secondViewCopyLength - counter] + c[0];
						break;
					default:
						value = firstViewCopy[firstViewCopyLength - counter] + c[0];
				}

				c[0] = value / 10;

				switch (true) {
					case counter >= firstViewCopy.length:
						firstViewCopy = (0, _pvutils.utilConcatView)(new Uint8Array([value % 10]), firstViewCopy);
						break;
					default:
						firstViewCopy[firstViewCopyLength - counter] = value % 10;
				}
			}

			if (c[0] > 0) firstViewCopy = (0, _pvutils.utilConcatView)(c, firstViewCopy);

			return firstViewCopy.slice(0);
		}

		function power2(n) {
			if (n >= powers2.length) {
				for (let p = powers2.length; p <= n; p++) {
					const c = new Uint8Array([0]);
					let digits = powers2[p - 1].slice(0);

					for (let i = digits.length - 1; i >= 0; i--) {
						const newValue = new Uint8Array([(digits[i] << 1) + c[0]]);
						c[0] = newValue[0] / 10;
						digits[i] = newValue[0] % 10;
					}

					if (c[0] > 0) digits = (0, _pvutils.utilConcatView)(c, digits);

					powers2.push(digits);
				}
			}

			return powers2[n];
		}

		function viewSub(first, second) {
			//region Initial variables
			let b = 0;

			let firstView = new Uint8Array(first);
			let secondView = new Uint8Array(second);

			let firstViewCopy = firstView.slice(0);
			const firstViewCopyLength = firstViewCopy.length - 1;
			let secondViewCopy = secondView.slice(0);
			const secondViewCopyLength = secondViewCopy.length - 1;

			let value;

			let counter = 0;
			//endregion

			for (let i = secondViewCopyLength; i >= 0; i--, counter++) {
				value = firstViewCopy[firstViewCopyLength - counter] - secondViewCopy[secondViewCopyLength - counter] - b;

				switch (true) {
					case value < 0:
						b = 1;
						firstViewCopy[firstViewCopyLength - counter] = value + 10;
						break;
					default:
						b = 0;
						firstViewCopy[firstViewCopyLength - counter] = value;
				}
			}

			if (b > 0) {
				for (let i = firstViewCopyLength - secondViewCopyLength + 1; i >= 0; i--, counter++) {
					value = firstViewCopy[firstViewCopyLength - counter] - b;

					if (value < 0) {
						b = 1;
						firstViewCopy[firstViewCopyLength - counter] = value + 10;
					} else {
						b = 0;
						firstViewCopy[firstViewCopyLength - counter] = value;
						break;
					}
				}
			}

			return firstViewCopy.slice();
		}
		//endregion

		//region Initial variables
		const firstBit = this._valueHex.byteLength * 8 - 1;

		let digits = new Uint8Array(this._valueHex.byteLength * 8 / 3);
		let bitNumber = 0;
		let currentByte;

		const asn1View = new Uint8Array(this._valueHex);

		let result = "";

		let flag = false;
		//endregion

		//region Calculate number
		for (let byteNumber = this._valueHex.byteLength - 1; byteNumber >= 0; byteNumber--) {
			currentByte = asn1View[byteNumber];

			for (let i = 0; i < 8; i++) {
				if ((currentByte & 1) === 1) {
					switch (bitNumber) {
						case firstBit:
							digits = viewSub(power2(bitNumber), digits);
							result = "-";
							break;
						default:
							digits = viewAdd(digits, power2(bitNumber));
					}
				}

				bitNumber++;
				currentByte >>= 1;
			}
		}
		//endregion

		//region Print number
		for (let i = 0; i < digits.length; i++) {
			if (digits[i]) flag = true;

			if (flag) result += digitsString.charAt(digits[i]);
		}

		if (flag === false) result += digitsString.charAt(0);
		//endregion

		return result;
	}
	//**********************************************************************************
}
//**************************************************************************************
class Integer extends BaseBlock {
	//**********************************************************************************
	/**
  * Constructor for "Integer" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters, LocalIntegerValueBlock);

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 2; // Integer
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "Integer";
	}
	//**********************************************************************************
	//noinspection JSUnusedGlobalSymbols
	/**
  * Compare two Integer object, or Integer and ArrayBuffer objects
  * @param {!Integer|ArrayBuffer} otherValue
  * @returns {boolean}
  */
	isEqual(otherValue) {
		if (otherValue instanceof Integer) {
			if (this.valueBlock.isHexOnly && otherValue.valueBlock.isHexOnly) // Compare two ArrayBuffers
				return (0, _pvutils.isEqualBuffer)(this.valueBlock.valueHex, otherValue.valueBlock.valueHex);

			if (this.valueBlock.isHexOnly === otherValue.valueBlock.isHexOnly) return this.valueBlock.valueDec === otherValue.valueBlock.valueDec;

			return false;
		}

		if (otherValue instanceof ArrayBuffer) return (0, _pvutils.isEqualBuffer)(this.valueBlock.valueHex, otherValue);

		return false;
	}
	//**********************************************************************************
	/**
  * Convert current Integer value from BER into DER format
  * @returns {Integer}
  */
	convertToDER() {
		const integer = new Integer({ valueHex: this.valueBlock.valueHex });
		integer.valueBlock.toDER();

		return integer;
	}
	//**********************************************************************************
	/**
  * Convert current Integer value from DER to BER format
  * @returns {Integer}
  */
	convertFromDER() {
		const expectedLength = this.valueBlock.valueHex.byteLength % 2 ? this.valueBlock.valueHex.byteLength + 1 : this.valueBlock.valueHex.byteLength;
		const integer = new Integer({ valueHex: this.valueBlock.valueHex });
		integer.valueBlock.fromDER(integer.valueBlock.valueHex, 0, integer.valueBlock.valueHex.byteLength, expectedLength);

		return integer;
	}
	//**********************************************************************************
}
exports.Integer = Integer; //**************************************************************************************
//endregion
//**************************************************************************************
//region Declaration of ASN.1 Enumerated type class
//**************************************************************************************

class Enumerated extends Integer {
	//**********************************************************************************
	/**
  * Constructor for "Enumerated" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters);

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 10; // Enumerated
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "Enumerated";
	}
	//**********************************************************************************
}
exports.Enumerated = Enumerated; //**************************************************************************************
//endregion
//**************************************************************************************
//region Declaration of ASN.1 ObjectIdentifier type class
//**************************************************************************************

class LocalSidValueBlock extends LocalHexBlock(LocalBaseBlock) {
	//**********************************************************************************
	/**
  * Constructor for "LocalSidValueBlock" class
  * @param {Object} [parameters={}]
  * @property {number} [valueDec]
  * @property {boolean} [isFirstSid]
  */
	constructor(parameters = {}) {
		super(parameters);

		this.valueDec = (0, _pvutils.getParametersValue)(parameters, "valueDec", -1);
		this.isFirstSid = (0, _pvutils.getParametersValue)(parameters, "isFirstSid", false);
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "sidBlock";
	}
	//**********************************************************************************
	/**
  * Base function for converting block from BER encoded array of bytes
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  * @param {!number} inputOffset Offset in ASN.1 BER encoded array where decoding should be started
  * @param {!number} inputLength Maximum length of array of bytes which can be using in this function
  * @returns {number} Offset after least decoded byte
  */
	fromBER(inputBuffer, inputOffset, inputLength) {
		if (inputLength === 0) return inputOffset;

		//region Basic check for parameters
		//noinspection JSCheckFunctionSignatures
		if ((0, _pvutils.checkBufferParams)(this, inputBuffer, inputOffset, inputLength) === false) return -1;
		//endregion

		const intBuffer = new Uint8Array(inputBuffer, inputOffset, inputLength);

		this.valueHex = new ArrayBuffer(inputLength);
		let view = new Uint8Array(this.valueHex);

		for (let i = 0; i < inputLength; i++) {
			view[i] = intBuffer[i] & 0x7F;

			this.blockLength++;

			if ((intBuffer[i] & 0x80) === 0x00) break;
		}

		//region Ajust size of valueHex buffer
		const tempValueHex = new ArrayBuffer(this.blockLength);
		const tempView = new Uint8Array(tempValueHex);

		for (let i = 0; i < this.blockLength; i++) tempView[i] = view[i];

		//noinspection JSCheckFunctionSignatures
		this.valueHex = tempValueHex.slice(0);
		view = new Uint8Array(this.valueHex);
		//endregion

		if ((intBuffer[this.blockLength - 1] & 0x80) !== 0x00) {
			this.error = "End of input reached before message was fully decoded";
			return -1;
		}

		if (view[0] === 0x00) this.warnings.push("Needlessly long format of SID encoding");

		if (this.blockLength <= 8) this.valueDec = (0, _pvutils.utilFromBase)(view, 7);else {
			this.isHexOnly = true;
			this.warnings.push("Too big SID for decoding, hex only");
		}

		return inputOffset + this.blockLength;
	}
	//**********************************************************************************
	/**
  * Encoding of current ASN.1 block into ASN.1 encoded array (BER rules)
  * @param {boolean} [sizeOnly=false] Flag that we need only a size of encoding, not a real array of bytes
  * @returns {ArrayBuffer}
  */
	toBER(sizeOnly = false) {
		//region Initial variables
		let retBuf;
		let retView;
		//endregion

		if (this.isHexOnly) {
			if (sizeOnly === true) return new ArrayBuffer(this.valueHex.byteLength);

			const curView = new Uint8Array(this.valueHex);

			retBuf = new ArrayBuffer(this.blockLength);
			retView = new Uint8Array(retBuf);

			for (let i = 0; i < this.blockLength - 1; i++) retView[i] = curView[i] | 0x80;

			retView[this.blockLength - 1] = curView[this.blockLength - 1];

			return retBuf;
		}

		const encodedBuf = (0, _pvutils.utilToBase)(this.valueDec, 7);
		if (encodedBuf.byteLength === 0) {
			this.error = "Error during encoding SID value";
			return new ArrayBuffer(0);
		}

		retBuf = new ArrayBuffer(encodedBuf.byteLength);

		if (sizeOnly === false) {
			const encodedView = new Uint8Array(encodedBuf);
			retView = new Uint8Array(retBuf);

			for (let i = 0; i < encodedBuf.byteLength - 1; i++) retView[i] = encodedView[i] | 0x80;

			retView[encodedBuf.byteLength - 1] = encodedView[encodedBuf.byteLength - 1];
		}

		return retBuf;
	}
	//**********************************************************************************
	/**
  * Create string representation of current SID block
  * @returns {string}
  */
	toString() {
		let result = "";

		if (this.isHexOnly === true) result = (0, _pvutils.bufferToHexCodes)(this.valueHex, 0, this.valueHex.byteLength);else {
			if (this.isFirstSid) {
				let sidValue = this.valueDec;

				if (this.valueDec <= 39) result = "0.";else {
					if (this.valueDec <= 79) {
						result = "1.";
						sidValue -= 40;
					} else {
						result = "2.";
						sidValue -= 80;
					}
				}

				result += sidValue.toString();
			} else result = this.valueDec.toString();
		}

		return result;
	}
	//**********************************************************************************
	//noinspection JSUnusedGlobalSymbols
	/**
  * Convertion for the block to JSON object
  * @returns {Object}
  */
	toJSON() {
		let object = {};

		//region Seems at the moment (Sep 2016) there is no way how to check method is supported in "super" object
		try {
			object = super.toJSON();
		} catch (ex) {}
		//endregion

		object.valueDec = this.valueDec;
		object.isFirstSid = this.isFirstSid;

		return object;
	}
	//**********************************************************************************
}
//**************************************************************************************
class LocalObjectIdentifierValueBlock extends LocalValueBlock {
	//**********************************************************************************
	/**
  * Constructor for "LocalObjectIdentifierValueBlock" class
  * @param {Object} [parameters={}]
  * @property {ArrayBuffer} [valueHex]
  */
	constructor(parameters = {}) {
		super(parameters);

		this.fromString((0, _pvutils.getParametersValue)(parameters, "value", ""));
	}
	//**********************************************************************************
	/**
  * Base function for converting block from BER encoded array of bytes
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  * @param {!number} inputOffset Offset in ASN.1 BER encoded array where decoding should be started
  * @param {!number} inputLength Maximum length of array of bytes which can be using in this function
  * @returns {number} Offset after least decoded byte
  */
	fromBER(inputBuffer, inputOffset, inputLength) {
		let resultOffset = inputOffset;

		while (inputLength > 0) {
			const sidBlock = new LocalSidValueBlock();
			resultOffset = sidBlock.fromBER(inputBuffer, resultOffset, inputLength);
			if (resultOffset === -1) {
				this.blockLength = 0;
				this.error = sidBlock.error;
				return resultOffset;
			}

			if (this.value.length === 0) sidBlock.isFirstSid = true;

			this.blockLength += sidBlock.blockLength;
			inputLength -= sidBlock.blockLength;

			this.value.push(sidBlock);
		}

		return resultOffset;
	}
	//**********************************************************************************
	/**
  * Encoding of current ASN.1 block into ASN.1 encoded array (BER rules)
  * @param {boolean} [sizeOnly=false] Flag that we need only a size of encoding, not a real array of bytes
  * @returns {ArrayBuffer}
  */
	toBER(sizeOnly = false) {
		let retBuf = new ArrayBuffer(0);

		for (let i = 0; i < this.value.length; i++) {
			const valueBuf = this.value[i].toBER(sizeOnly);
			if (valueBuf.byteLength === 0) {
				this.error = this.value[i].error;
				return new ArrayBuffer(0);
			}

			retBuf = (0, _pvutils.utilConcatBuf)(retBuf, valueBuf);
		}

		return retBuf;
	}
	//**********************************************************************************
	/**
  * Create "LocalObjectIdentifierValueBlock" class from string
  * @param {string} string Input string to convert from
  * @returns {boolean}
  */
	fromString(string) {
		this.value = []; // Clear existing SID values

		let pos1 = 0;
		let pos2 = 0;

		let sid = "";

		let flag = false;

		do {
			pos2 = string.indexOf(".", pos1);
			if (pos2 === -1) sid = string.substr(pos1);else sid = string.substr(pos1, pos2 - pos1);

			pos1 = pos2 + 1;

			if (flag) {
				const sidBlock = this.value[0];

				let plus = 0;

				switch (sidBlock.valueDec) {
					case 0:
						break;
					case 1:
						plus = 40;
						break;
					case 2:
						plus = 80;
						break;
					default:
						this.value = []; // clear SID array
						return false; // ???
				}

				const parsedSID = parseInt(sid, 10);
				if (isNaN(parsedSID)) return true;

				sidBlock.valueDec = parsedSID + plus;

				flag = false;
			} else {
				const sidBlock = new LocalSidValueBlock();
				sidBlock.valueDec = parseInt(sid, 10);
				if (isNaN(sidBlock.valueDec)) return true;

				if (this.value.length === 0) {
					sidBlock.isFirstSid = true;
					flag = true;
				}

				this.value.push(sidBlock);
			}
		} while (pos2 !== -1);

		return true;
	}
	//**********************************************************************************
	/**
  * Converts "LocalObjectIdentifierValueBlock" class to string
  * @returns {string}
  */
	toString() {
		let result = "";
		let isHexOnly = false;

		for (let i = 0; i < this.value.length; i++) {
			isHexOnly = this.value[i].isHexOnly;

			let sidStr = this.value[i].toString();

			if (i !== 0) result = `${result}.`;

			if (isHexOnly) {
				sidStr = `{${sidStr}}`;

				if (this.value[i].isFirstSid) result = `2.{${sidStr} - 80}`;else result += sidStr;
			} else result += sidStr;
		}

		return result;
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "ObjectIdentifierValueBlock";
	}
	//**********************************************************************************
	/**
  * Convertion for the block to JSON object
  * @returns {Object}
  */
	toJSON() {
		let object = {};

		//region Seems at the moment (Sep 2016) there is no way how to check method is supported in "super" object
		try {
			object = super.toJSON();
		} catch (ex) {}
		//endregion

		object.value = this.toString();
		object.sidArray = [];
		for (let i = 0; i < this.value.length; i++) object.sidArray.push(this.value[i].toJSON());

		return object;
	}
	//**********************************************************************************
}
//**************************************************************************************
/**
 * @extends BaseBlock
 */
class ObjectIdentifier extends BaseBlock {
	//**********************************************************************************
	/**
  * Constructor for "ObjectIdentifier" class
  * @param {Object} [parameters={}]
  * @property {ArrayBuffer} [valueHex]
  */
	constructor(parameters = {}) {
		super(parameters, LocalObjectIdentifierValueBlock);

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 6; // OBJECT IDENTIFIER
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "ObjectIdentifier";
	}
	//**********************************************************************************
}
exports.ObjectIdentifier = ObjectIdentifier; //**************************************************************************************
//endregion
//**************************************************************************************
//region Declaration of all string's classes
//**************************************************************************************

class LocalUtf8StringValueBlock extends LocalHexBlock(LocalBaseBlock) {
	//**********************************************************************************
	//noinspection JSUnusedGlobalSymbols
	/**
  * Constructor for "LocalUtf8StringValueBlock" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters);

		this.isHexOnly = true;
		this.value = ""; // String representation of decoded ArrayBuffer
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "Utf8StringValueBlock";
	}
	//**********************************************************************************
	//noinspection JSUnusedGlobalSymbols
	/**
  * Convertion for the block to JSON object
  * @returns {Object}
  */
	toJSON() {
		let object = {};

		//region Seems at the moment (Sep 2016) there is no way how to check method is supported in "super" object
		try {
			object = super.toJSON();
		} catch (ex) {}
		//endregion

		object.value = this.value;

		return object;
	}
	//**********************************************************************************
}
//**************************************************************************************
/**
 * @extends BaseBlock
 */
class Utf8String extends BaseBlock {
	//**********************************************************************************
	/**
  * Constructor for "Utf8String" class
  * @param {Object} [parameters={}]
  * @property {ArrayBuffer} [valueHex]
  */
	constructor(parameters = {}) {
		super(parameters, LocalUtf8StringValueBlock);

		if ("value" in parameters) this.fromString(parameters.value);

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 12; // Utf8String
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "Utf8String";
	}
	//**********************************************************************************
	/**
  * Base function for converting block from BER encoded array of bytes
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  * @param {!number} inputOffset Offset in ASN.1 BER encoded array where decoding should be started
  * @param {!number} inputLength Maximum length of array of bytes which can be using in this function
  * @returns {number} Offset after least decoded byte
  */
	fromBER(inputBuffer, inputOffset, inputLength) {
		const resultOffset = this.valueBlock.fromBER(inputBuffer, inputOffset, this.lenBlock.isIndefiniteForm === true ? inputLength : this.lenBlock.length);
		if (resultOffset === -1) {
			this.error = this.valueBlock.error;
			return resultOffset;
		}

		this.fromBuffer(this.valueBlock.valueHex);

		if (this.idBlock.error.length === 0) this.blockLength += this.idBlock.blockLength;

		if (this.lenBlock.error.length === 0) this.blockLength += this.lenBlock.blockLength;

		if (this.valueBlock.error.length === 0) this.blockLength += this.valueBlock.blockLength;

		return resultOffset;
	}
	//**********************************************************************************
	/**
  * Function converting ArrayBuffer into ASN.1 internal string
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  */
	fromBuffer(inputBuffer) {
		this.valueBlock.value = String.fromCharCode.apply(null, new Uint8Array(inputBuffer));

		try {
			//noinspection JSDeprecatedSymbols
			this.valueBlock.value = decodeURIComponent(escape(this.valueBlock.value));
		} catch (ex) {
			this.warnings.push(`Error during "decodeURIComponent": ${ex}, using raw string`);
		}
	}
	//**********************************************************************************
	/**
  * Function converting JavaScript string into ASN.1 internal class
  * @param {!string} inputString ASN.1 BER encoded array
  */
	fromString(inputString) {
		//noinspection JSDeprecatedSymbols
		const str = unescape(encodeURIComponent(inputString));
		const strLen = str.length;

		this.valueBlock.valueHex = new ArrayBuffer(strLen);
		const view = new Uint8Array(this.valueBlock.valueHex);

		for (let i = 0; i < strLen; i++) view[i] = str.charCodeAt(i);

		this.valueBlock.value = inputString;
	}
	//**********************************************************************************
}
exports.Utf8String = Utf8String; //**************************************************************************************
/**
 * @extends LocalBaseBlock
 * @extends LocalHexBlock
 */

class LocalBmpStringValueBlock extends LocalHexBlock(LocalBaseBlock) {
	//**********************************************************************************
	/**
  * Constructor for "LocalBmpStringValueBlock" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters);

		this.isHexOnly = true;
		this.value = "";
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "BmpStringValueBlock";
	}
	//**********************************************************************************
	//noinspection JSUnusedGlobalSymbols
	/**
  * Convertion for the block to JSON object
  * @returns {Object}
  */
	toJSON() {
		let object = {};

		//region Seems at the moment (Sep 2016) there is no way how to check method is supported in "super" object
		try {
			object = super.toJSON();
		} catch (ex) {}
		//endregion

		object.value = this.value;

		return object;
	}
	//**********************************************************************************
}
//**************************************************************************************
/**
 * @extends BaseBlock
 */
class BmpString extends BaseBlock {
	//**********************************************************************************
	/**
  * Constructor for "BmpString" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters, LocalBmpStringValueBlock);

		if ("value" in parameters) this.fromString(parameters.value);

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 30; // BmpString
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "BmpString";
	}
	//**********************************************************************************
	/**
  * Base function for converting block from BER encoded array of bytes
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  * @param {!number} inputOffset Offset in ASN.1 BER encoded array where decoding should be started
  * @param {!number} inputLength Maximum length of array of bytes which can be using in this function
  * @returns {number} Offset after least decoded byte
  */
	fromBER(inputBuffer, inputOffset, inputLength) {
		const resultOffset = this.valueBlock.fromBER(inputBuffer, inputOffset, this.lenBlock.isIndefiniteForm === true ? inputLength : this.lenBlock.length);
		if (resultOffset === -1) {
			this.error = this.valueBlock.error;
			return resultOffset;
		}

		this.fromBuffer(this.valueBlock.valueHex);

		if (this.idBlock.error.length === 0) this.blockLength += this.idBlock.blockLength;

		if (this.lenBlock.error.length === 0) this.blockLength += this.lenBlock.blockLength;

		if (this.valueBlock.error.length === 0) this.blockLength += this.valueBlock.blockLength;

		return resultOffset;
	}
	//**********************************************************************************
	/**
  * Function converting ArrayBuffer into ASN.1 internal string
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  */
	fromBuffer(inputBuffer) {
		//noinspection JSCheckFunctionSignatures
		const copyBuffer = inputBuffer.slice(0);
		const valueView = new Uint8Array(copyBuffer);

		for (let i = 0; i < valueView.length; i += 2) {
			const temp = valueView[i];

			valueView[i] = valueView[i + 1];
			valueView[i + 1] = temp;
		}

		this.valueBlock.value = String.fromCharCode.apply(null, new Uint16Array(copyBuffer));
	}
	//**********************************************************************************
	/**
  * Function converting JavaScript string into ASN.1 internal class
  * @param {!string} inputString ASN.1 BER encoded array
  */
	fromString(inputString) {
		const strLength = inputString.length;

		this.valueBlock.valueHex = new ArrayBuffer(strLength * 2);
		const valueHexView = new Uint8Array(this.valueBlock.valueHex);

		for (let i = 0; i < strLength; i++) {
			const codeBuf = (0, _pvutils.utilToBase)(inputString.charCodeAt(i), 8);
			const codeView = new Uint8Array(codeBuf);
			if (codeView.length > 2) continue;

			const dif = 2 - codeView.length;

			for (let j = codeView.length - 1; j >= 0; j--) valueHexView[i * 2 + j + dif] = codeView[j];
		}

		this.valueBlock.value = inputString;
	}
	//**********************************************************************************
}
exports.BmpString = BmpString; //**************************************************************************************

class LocalUniversalStringValueBlock extends LocalHexBlock(LocalBaseBlock) {
	//**********************************************************************************
	/**
  * Constructor for "LocalUniversalStringValueBlock" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters);

		this.isHexOnly = true;
		this.value = "";
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "UniversalStringValueBlock";
	}
	//**********************************************************************************
	//noinspection JSUnusedGlobalSymbols
	/**
  * Convertion for the block to JSON object
  * @returns {Object}
  */
	toJSON() {
		let object = {};

		//region Seems at the moment (Sep 2016) there is no way how to check method is supported in "super" object
		try {
			object = super.toJSON();
		} catch (ex) {}
		//endregion

		object.value = this.value;

		return object;
	}
	//**********************************************************************************
}
//**************************************************************************************
/**
 * @extends BaseBlock
 */
class UniversalString extends BaseBlock {
	//**********************************************************************************
	/**
  * Constructor for "UniversalString" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters, LocalUniversalStringValueBlock);

		if ("value" in parameters) this.fromString(parameters.value);

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 28; // UniversalString
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "UniversalString";
	}
	//**********************************************************************************
	/**
  * Base function for converting block from BER encoded array of bytes
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  * @param {!number} inputOffset Offset in ASN.1 BER encoded array where decoding should be started
  * @param {!number} inputLength Maximum length of array of bytes which can be using in this function
  * @returns {number} Offset after least decoded byte
  */
	fromBER(inputBuffer, inputOffset, inputLength) {
		const resultOffset = this.valueBlock.fromBER(inputBuffer, inputOffset, this.lenBlock.isIndefiniteForm === true ? inputLength : this.lenBlock.length);
		if (resultOffset === -1) {
			this.error = this.valueBlock.error;
			return resultOffset;
		}

		this.fromBuffer(this.valueBlock.valueHex);

		if (this.idBlock.error.length === 0) this.blockLength += this.idBlock.blockLength;

		if (this.lenBlock.error.length === 0) this.blockLength += this.lenBlock.blockLength;

		if (this.valueBlock.error.length === 0) this.blockLength += this.valueBlock.blockLength;

		return resultOffset;
	}
	//**********************************************************************************
	/**
  * Function converting ArrayBuffer into ASN.1 internal string
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  */
	fromBuffer(inputBuffer) {
		//noinspection JSCheckFunctionSignatures
		const copyBuffer = inputBuffer.slice(0);
		const valueView = new Uint8Array(copyBuffer);

		for (let i = 0; i < valueView.length; i += 4) {
			valueView[i] = valueView[i + 3];
			valueView[i + 1] = valueView[i + 2];
			valueView[i + 2] = 0x00;
			valueView[i + 3] = 0x00;
		}

		this.valueBlock.value = String.fromCharCode.apply(null, new Uint32Array(copyBuffer));
	}
	//**********************************************************************************
	/**
  * Function converting JavaScript string into ASN.1 internal class
  * @param {!string} inputString ASN.1 BER encoded array
  */
	fromString(inputString) {
		const strLength = inputString.length;

		this.valueBlock.valueHex = new ArrayBuffer(strLength * 4);
		const valueHexView = new Uint8Array(this.valueBlock.valueHex);

		for (let i = 0; i < strLength; i++) {
			const codeBuf = (0, _pvutils.utilToBase)(inputString.charCodeAt(i), 8);
			const codeView = new Uint8Array(codeBuf);
			if (codeView.length > 4) continue;

			const dif = 4 - codeView.length;

			for (let j = codeView.length - 1; j >= 0; j--) valueHexView[i * 4 + j + dif] = codeView[j];
		}

		this.valueBlock.value = inputString;
	}
	//**********************************************************************************
}
exports.UniversalString = UniversalString; //**************************************************************************************

class LocalSimpleStringValueBlock extends LocalHexBlock(LocalBaseBlock) {
	//**********************************************************************************
	/**
  * Constructor for "LocalSimpleStringValueBlock" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters);

		this.value = "";
		this.isHexOnly = true;
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "SimpleStringValueBlock";
	}
	//**********************************************************************************
	//noinspection JSUnusedGlobalSymbols
	/**
  * Convertion for the block to JSON object
  * @returns {Object}
  */
	toJSON() {
		let object = {};

		//region Seems at the moment (Sep 2016) there is no way how to check method is supported in "super" object
		try {
			object = super.toJSON();
		} catch (ex) {}
		//endregion

		object.value = this.value;

		return object;
	}
	//**********************************************************************************
}
//**************************************************************************************
/**
 * @extends BaseBlock
 */
class LocalSimpleStringBlock extends BaseBlock {
	//**********************************************************************************
	/**
  * Constructor for "LocalSimpleStringBlock" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters, LocalSimpleStringValueBlock);

		if ("value" in parameters) this.fromString(parameters.value);
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "SIMPLESTRING";
	}
	//**********************************************************************************
	/**
  * Base function for converting block from BER encoded array of bytes
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  * @param {!number} inputOffset Offset in ASN.1 BER encoded array where decoding should be started
  * @param {!number} inputLength Maximum length of array of bytes which can be using in this function
  * @returns {number} Offset after least decoded byte
  */
	fromBER(inputBuffer, inputOffset, inputLength) {
		const resultOffset = this.valueBlock.fromBER(inputBuffer, inputOffset, this.lenBlock.isIndefiniteForm === true ? inputLength : this.lenBlock.length);
		if (resultOffset === -1) {
			this.error = this.valueBlock.error;
			return resultOffset;
		}

		this.fromBuffer(this.valueBlock.valueHex);

		if (this.idBlock.error.length === 0) this.blockLength += this.idBlock.blockLength;

		if (this.lenBlock.error.length === 0) this.blockLength += this.lenBlock.blockLength;

		if (this.valueBlock.error.length === 0) this.blockLength += this.valueBlock.blockLength;

		return resultOffset;
	}
	//**********************************************************************************
	/**
  * Function converting ArrayBuffer into ASN.1 internal string
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  */
	fromBuffer(inputBuffer) {
		this.valueBlock.value = String.fromCharCode.apply(null, new Uint8Array(inputBuffer));
	}
	//**********************************************************************************
	/**
  * Function converting JavaScript string into ASN.1 internal class
  * @param {!string} inputString ASN.1 BER encoded array
  */
	fromString(inputString) {
		const strLen = inputString.length;

		this.valueBlock.valueHex = new ArrayBuffer(strLen);
		const view = new Uint8Array(this.valueBlock.valueHex);

		for (let i = 0; i < strLen; i++) view[i] = inputString.charCodeAt(i);

		this.valueBlock.value = inputString;
	}
	//**********************************************************************************
}
//**************************************************************************************
/**
 * @extends LocalSimpleStringBlock
 */
class NumericString extends LocalSimpleStringBlock {
	//**********************************************************************************
	/**
  * Constructor for "NumericString" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters);

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 18; // NumericString
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "NumericString";
	}
	//**********************************************************************************
}
exports.NumericString = NumericString; //**************************************************************************************
/**
 * @extends LocalSimpleStringBlock
 */

class PrintableString extends LocalSimpleStringBlock {
	//**********************************************************************************
	/**
  * Constructor for "PrintableString" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters);

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 19; // PrintableString
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "PrintableString";
	}
	//**********************************************************************************
}
exports.PrintableString = PrintableString; //**************************************************************************************
/**
 * @extends LocalSimpleStringBlock
 */

class TeletexString extends LocalSimpleStringBlock {
	//**********************************************************************************
	/**
  * Constructor for "TeletexString" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters);

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 20; // TeletexString
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "TeletexString";
	}
	//**********************************************************************************
}
exports.TeletexString = TeletexString; //**************************************************************************************
/**
 * @extends LocalSimpleStringBlock
 */

class VideotexString extends LocalSimpleStringBlock {
	//**********************************************************************************
	/**
  * Constructor for "VideotexString" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters);

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 21; // VideotexString
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "VideotexString";
	}
	//**********************************************************************************
}
exports.VideotexString = VideotexString; //**************************************************************************************
/**
 * @extends LocalSimpleStringBlock
 */

class IA5String extends LocalSimpleStringBlock {
	//**********************************************************************************
	/**
  * Constructor for "IA5String" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters);

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 22; // IA5String
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "IA5String";
	}
	//**********************************************************************************
}
exports.IA5String = IA5String; //**************************************************************************************
/**
 * @extends LocalSimpleStringBlock
 */

class GraphicString extends LocalSimpleStringBlock {
	//**********************************************************************************
	/**
  * Constructor for "GraphicString" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters);

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 25; // GraphicString
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "GraphicString";
	}
	//**********************************************************************************
}
exports.GraphicString = GraphicString; //**************************************************************************************
/**
 * @extends LocalSimpleStringBlock
 */

class VisibleString extends LocalSimpleStringBlock {
	//**********************************************************************************
	/**
  * Constructor for "VisibleString" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters);

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 26; // VisibleString
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "VisibleString";
	}
	//**********************************************************************************
}
exports.VisibleString = VisibleString; //**************************************************************************************
/**
 * @extends LocalSimpleStringBlock
 */

class GeneralString extends LocalSimpleStringBlock {
	//**********************************************************************************
	/**
  * Constructor for "GeneralString" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters);

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 27; // GeneralString
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "GeneralString";
	}
	//**********************************************************************************
}
exports.GeneralString = GeneralString; //**************************************************************************************
/**
 * @extends LocalSimpleStringBlock
 */

class CharacterString extends LocalSimpleStringBlock {
	//**********************************************************************************
	/**
  * Constructor for "CharacterString" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters);

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 29; // CharacterString
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "CharacterString";
	}
	//**********************************************************************************
}
exports.CharacterString = CharacterString; //**************************************************************************************
//endregion
//**************************************************************************************
//region Declaration of all date and time classes
//**************************************************************************************
/**
 * @extends VisibleString
 */

class UTCTime extends VisibleString {
	//**********************************************************************************
	/**
  * Constructor for "UTCTime" class
  * @param {Object} [parameters={}]
  * @property {string} [value] String representatio of the date
  * @property {Date} [valueDate] JavaScript "Date" object
  */
	constructor(parameters = {}) {
		super(parameters);

		this.year = 0;
		this.month = 0;
		this.day = 0;
		this.hour = 0;
		this.minute = 0;
		this.second = 0;

		//region Create UTCTime from ASN.1 UTC string value
		if ("value" in parameters) {
			this.fromString(parameters.value);

			this.valueBlock.valueHex = new ArrayBuffer(parameters.value.length);
			const view = new Uint8Array(this.valueBlock.valueHex);

			for (let i = 0; i < parameters.value.length; i++) view[i] = parameters.value.charCodeAt(i);
		}
		//endregion
		//region Create GeneralizedTime from JavaScript Date type
		if ("valueDate" in parameters) {
			this.fromDate(parameters.valueDate);
			this.valueBlock.valueHex = this.toBuffer();
		}
		//endregion

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 23; // UTCTime
	}
	//**********************************************************************************
	/**
  * Base function for converting block from BER encoded array of bytes
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  * @param {!number} inputOffset Offset in ASN.1 BER encoded array where decoding should be started
  * @param {!number} inputLength Maximum length of array of bytes which can be using in this function
  * @returns {number} Offset after least decoded byte
  */
	fromBER(inputBuffer, inputOffset, inputLength) {
		const resultOffset = this.valueBlock.fromBER(inputBuffer, inputOffset, this.lenBlock.isIndefiniteForm === true ? inputLength : this.lenBlock.length);
		if (resultOffset === -1) {
			this.error = this.valueBlock.error;
			return resultOffset;
		}

		this.fromBuffer(this.valueBlock.valueHex);

		if (this.idBlock.error.length === 0) this.blockLength += this.idBlock.blockLength;

		if (this.lenBlock.error.length === 0) this.blockLength += this.lenBlock.blockLength;

		if (this.valueBlock.error.length === 0) this.blockLength += this.valueBlock.blockLength;

		return resultOffset;
	}
	//**********************************************************************************
	/**
  * Function converting ArrayBuffer into ASN.1 internal string
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  */
	fromBuffer(inputBuffer) {
		this.fromString(String.fromCharCode.apply(null, new Uint8Array(inputBuffer)));
	}
	//**********************************************************************************
	/**
  * Function converting ASN.1 internal string into ArrayBuffer
  * @returns {ArrayBuffer}
  */
	toBuffer() {
		const str = this.toString();

		const buffer = new ArrayBuffer(str.length);
		const view = new Uint8Array(buffer);

		for (let i = 0; i < str.length; i++) view[i] = str.charCodeAt(i);

		return buffer;
	}
	//**********************************************************************************
	/**
  * Function converting "Date" object into ASN.1 internal string
  * @param {!Date} inputDate JavaScript "Date" object
  */
	fromDate(inputDate) {
		this.year = inputDate.getUTCFullYear();
		this.month = inputDate.getUTCMonth() + 1;
		this.day = inputDate.getUTCDate();
		this.hour = inputDate.getUTCHours();
		this.minute = inputDate.getUTCMinutes();
		this.second = inputDate.getUTCSeconds();
	}
	//**********************************************************************************
	//noinspection JSUnusedGlobalSymbols
	/**
  * Function converting ASN.1 internal string into "Date" object
  * @returns {Date}
  */
	toDate() {
		return new Date(Date.UTC(this.year, this.month - 1, this.day, this.hour, this.minute, this.second));
	}
	//**********************************************************************************
	/**
  * Function converting JavaScript string into ASN.1 internal class
  * @param {!string} inputString ASN.1 BER encoded array
  */
	fromString(inputString) {
		//region Parse input string
		const parser = /(\d{2})(\d{2})(\d{2})(\d{2})(\d{2})(\d{2})Z/ig;
		const parserArray = parser.exec(inputString);
		if (parserArray === null) {
			this.error = "Wrong input string for convertion";
			return;
		}
		//endregion

		//region Store parsed values
		const year = parseInt(parserArray[1], 10);
		if (year >= 50) this.year = 1900 + year;else this.year = 2000 + year;

		this.month = parseInt(parserArray[2], 10);
		this.day = parseInt(parserArray[3], 10);
		this.hour = parseInt(parserArray[4], 10);
		this.minute = parseInt(parserArray[5], 10);
		this.second = parseInt(parserArray[6], 10);
		//endregion
	}
	//**********************************************************************************
	/**
  * Function converting ASN.1 internal class into JavaScript string
  * @returns {string}
  */
	toString() {
		const outputArray = new Array(7);

		outputArray[0] = (0, _pvutils.padNumber)(this.year < 2000 ? this.year - 1900 : this.year - 2000, 2);
		outputArray[1] = (0, _pvutils.padNumber)(this.month, 2);
		outputArray[2] = (0, _pvutils.padNumber)(this.day, 2);
		outputArray[3] = (0, _pvutils.padNumber)(this.hour, 2);
		outputArray[4] = (0, _pvutils.padNumber)(this.minute, 2);
		outputArray[5] = (0, _pvutils.padNumber)(this.second, 2);
		outputArray[6] = "Z";

		return outputArray.join("");
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "UTCTime";
	}
	//**********************************************************************************
	/**
  * Convertion for the block to JSON object
  * @returns {Object}
  */
	toJSON() {
		let object = {};

		//region Seems at the moment (Sep 2016) there is no way how to check method is supported in "super" object
		try {
			object = super.toJSON();
		} catch (ex) {}
		//endregion

		object.year = this.year;
		object.month = this.month;
		object.day = this.day;
		object.hour = this.hour;
		object.minute = this.minute;
		object.second = this.second;

		return object;
	}
	//**********************************************************************************
}
exports.UTCTime = UTCTime; //**************************************************************************************
/**
 * @extends VisibleString
 */

class GeneralizedTime extends VisibleString {
	//**********************************************************************************
	/**
  * Constructor for "GeneralizedTime" class
  * @param {Object} [parameters={}]
  * @property {string} [value] String representatio of the date
  * @property {Date} [valueDate] JavaScript "Date" object
  */
	constructor(parameters = {}) {
		super(parameters);

		this.year = 0;
		this.month = 0;
		this.day = 0;
		this.hour = 0;
		this.minute = 0;
		this.second = 0;
		this.millisecond = 0;

		//region Create UTCTime from ASN.1 UTC string value
		if ("value" in parameters) {
			this.fromString(parameters.value);

			this.valueBlock.valueHex = new ArrayBuffer(parameters.value.length);
			const view = new Uint8Array(this.valueBlock.valueHex);

			for (let i = 0; i < parameters.value.length; i++) view[i] = parameters.value.charCodeAt(i);
		}
		//endregion
		//region Create GeneralizedTime from JavaScript Date type
		if ("valueDate" in parameters) {
			this.fromDate(parameters.valueDate);
			this.valueBlock.valueHex = this.toBuffer();
		}
		//endregion

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 24; // GeneralizedTime
	}
	//**********************************************************************************
	/**
  * Base function for converting block from BER encoded array of bytes
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  * @param {!number} inputOffset Offset in ASN.1 BER encoded array where decoding should be started
  * @param {!number} inputLength Maximum length of array of bytes which can be using in this function
  * @returns {number} Offset after least decoded byte
  */
	fromBER(inputBuffer, inputOffset, inputLength) {
		const resultOffset = this.valueBlock.fromBER(inputBuffer, inputOffset, this.lenBlock.isIndefiniteForm === true ? inputLength : this.lenBlock.length);
		if (resultOffset === -1) {
			this.error = this.valueBlock.error;
			return resultOffset;
		}

		this.fromBuffer(this.valueBlock.valueHex);

		if (this.idBlock.error.length === 0) this.blockLength += this.idBlock.blockLength;

		if (this.lenBlock.error.length === 0) this.blockLength += this.lenBlock.blockLength;

		if (this.valueBlock.error.length === 0) this.blockLength += this.valueBlock.blockLength;

		return resultOffset;
	}
	//**********************************************************************************
	/**
  * Function converting ArrayBuffer into ASN.1 internal string
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  */
	fromBuffer(inputBuffer) {
		this.fromString(String.fromCharCode.apply(null, new Uint8Array(inputBuffer)));
	}
	//**********************************************************************************
	/**
  * Function converting ASN.1 internal string into ArrayBuffer
  * @returns {ArrayBuffer}
  */
	toBuffer() {
		const str = this.toString();

		const buffer = new ArrayBuffer(str.length);
		const view = new Uint8Array(buffer);

		for (let i = 0; i < str.length; i++) view[i] = str.charCodeAt(i);

		return buffer;
	}
	//**********************************************************************************
	/**
  * Function converting "Date" object into ASN.1 internal string
  * @param {!Date} inputDate JavaScript "Date" object
  */
	fromDate(inputDate) {
		this.year = inputDate.getUTCFullYear();
		this.month = inputDate.getUTCMonth() + 1;
		this.day = inputDate.getUTCDate();
		this.hour = inputDate.getUTCHours();
		this.minute = inputDate.getUTCMinutes();
		this.second = inputDate.getUTCSeconds();
		this.millisecond = inputDate.getUTCMilliseconds();
	}
	//**********************************************************************************
	//noinspection JSUnusedGlobalSymbols
	/**
  * Function converting ASN.1 internal string into "Date" object
  * @returns {Date}
  */
	toDate() {
		return new Date(Date.UTC(this.year, this.month - 1, this.day, this.hour, this.minute, this.second, this.millisecond));
	}
	//**********************************************************************************
	/**
  * Function converting JavaScript string into ASN.1 internal class
  * @param {!string} inputString ASN.1 BER encoded array
  */
	fromString(inputString) {
		//region Initial variables
		let isUTC = false;

		let timeString = "";
		let dateTimeString = "";
		let fractionPart = 0;

		let parser;

		let hourDifference = 0;
		let minuteDifference = 0;
		//endregion

		//region Convert as UTC time
		if (inputString[inputString.length - 1] === "Z") {
			timeString = inputString.substr(0, inputString.length - 1);

			isUTC = true;
		}
		//endregion
		//region Convert as local time
		else {
				//noinspection JSPrimitiveTypeWrapperUsage
				const number = new Number(inputString[inputString.length - 1]);

				if (isNaN(number.valueOf())) throw new Error("Wrong input string for convertion");

				timeString = inputString;
			}
		//endregion

		//region Check that we do not have a "+" and "-" symbols inside UTC time
		if (isUTC) {
			if (timeString.indexOf("+") !== -1) throw new Error("Wrong input string for convertion");

			if (timeString.indexOf("-") !== -1) throw new Error("Wrong input string for convertion");
		}
		//endregion
		//region Get "UTC time difference" in case of local time
		else {
				let multiplier = 1;
				let differencePosition = timeString.indexOf("+");
				let differenceString = "";

				if (differencePosition === -1) {
					differencePosition = timeString.indexOf("-");
					multiplier = -1;
				}

				if (differencePosition !== -1) {
					differenceString = timeString.substr(differencePosition + 1);
					timeString = timeString.substr(0, differencePosition);

					if (differenceString.length !== 2 && differenceString.length !== 4) throw new Error("Wrong input string for convertion");

					//noinspection JSPrimitiveTypeWrapperUsage
					let number = new Number(differenceString.substr(0, 2));

					if (isNaN(number.valueOf())) throw new Error("Wrong input string for convertion");

					hourDifference = multiplier * number;

					if (differenceString.length === 4) {
						//noinspection JSPrimitiveTypeWrapperUsage
						number = new Number(differenceString.substr(2, 2));

						if (isNaN(number.valueOf())) throw new Error("Wrong input string for convertion");

						minuteDifference = multiplier * number;
					}
				}
			}
		//endregion

		//region Get position of fraction point
		let fractionPointPosition = timeString.indexOf("."); // Check for "full stop" symbol
		if (fractionPointPosition === -1) fractionPointPosition = timeString.indexOf(","); // Check for "comma" symbol
		//endregion

		//region Get fraction part
		if (fractionPointPosition !== -1) {
			//noinspection JSPrimitiveTypeWrapperUsage
			const fractionPartCheck = new Number(`0${timeString.substr(fractionPointPosition)}`);

			if (isNaN(fractionPartCheck.valueOf())) throw new Error("Wrong input string for convertion");

			fractionPart = fractionPartCheck.valueOf();

			dateTimeString = timeString.substr(0, fractionPointPosition);
		} else dateTimeString = timeString;
		//endregion

		//region Parse internal date
		switch (true) {
			case dateTimeString.length === 8:
				// "YYYYMMDD"
				parser = /(\d{4})(\d{2})(\d{2})/ig;
				if (fractionPointPosition !== -1) throw new Error("Wrong input string for convertion"); // Here we should not have a "fraction point"
				break;
			case dateTimeString.length === 10:
				// "YYYYMMDDHH"
				parser = /(\d{4})(\d{2})(\d{2})(\d{2})/ig;

				if (fractionPointPosition !== -1) {
					let fractionResult = 60 * fractionPart;
					this.minute = Math.floor(fractionResult);

					fractionResult = 60 * (fractionResult - this.minute);
					this.second = Math.floor(fractionResult);

					fractionResult = 1000 * (fractionResult - this.second);
					this.millisecond = Math.floor(fractionResult);
				}
				break;
			case dateTimeString.length === 12:
				// "YYYYMMDDHHMM"
				parser = /(\d{4})(\d{2})(\d{2})(\d{2})(\d{2})/ig;

				if (fractionPointPosition !== -1) {
					let fractionResult = 60 * fractionPart;
					this.second = Math.floor(fractionResult);

					fractionResult = 1000 * (fractionResult - this.second);
					this.millisecond = Math.floor(fractionResult);
				}
				break;
			case dateTimeString.length === 14:
				// "YYYYMMDDHHMMSS"
				parser = /(\d{4})(\d{2})(\d{2})(\d{2})(\d{2})(\d{2})/ig;

				if (fractionPointPosition !== -1) {
					const fractionResult = 1000 * fractionPart;
					this.millisecond = Math.floor(fractionResult);
				}
				break;
			default:
				throw new Error("Wrong input string for convertion");
		}
		//endregion

		//region Put parsed values at right places
		const parserArray = parser.exec(dateTimeString);
		if (parserArray === null) throw new Error("Wrong input string for convertion");

		for (let j = 1; j < parserArray.length; j++) {
			switch (j) {
				case 1:
					this.year = parseInt(parserArray[j], 10);
					break;
				case 2:
					this.month = parseInt(parserArray[j], 10);
					break;
				case 3:
					this.day = parseInt(parserArray[j], 10);
					break;
				case 4:
					this.hour = parseInt(parserArray[j], 10) + hourDifference;
					break;
				case 5:
					this.minute = parseInt(parserArray[j], 10) + minuteDifference;
					break;
				case 6:
					this.second = parseInt(parserArray[j], 10);
					break;
				default:
					throw new Error("Wrong input string for convertion");
			}
		}
		//endregion

		//region Get final date
		if (isUTC === false) {
			const tempDate = new Date(this.year, this.month, this.day, this.hour, this.minute, this.second, this.millisecond);

			this.year = tempDate.getUTCFullYear();
			this.month = tempDate.getUTCMonth();
			this.day = tempDate.getUTCDay();
			this.hour = tempDate.getUTCHours();
			this.minute = tempDate.getUTCMinutes();
			this.second = tempDate.getUTCSeconds();
			this.millisecond = tempDate.getUTCMilliseconds();
		}
		//endregion
	}
	//**********************************************************************************
	/**
  * Function converting ASN.1 internal class into JavaScript string
  * @returns {string}
  */
	toString() {
		const outputArray = [];

		outputArray.push((0, _pvutils.padNumber)(this.year, 4));
		outputArray.push((0, _pvutils.padNumber)(this.month, 2));
		outputArray.push((0, _pvutils.padNumber)(this.day, 2));
		outputArray.push((0, _pvutils.padNumber)(this.hour, 2));
		outputArray.push((0, _pvutils.padNumber)(this.minute, 2));
		outputArray.push((0, _pvutils.padNumber)(this.second, 2));
		if (this.millisecond !== 0) {
			outputArray.push(".");
			outputArray.push((0, _pvutils.padNumber)(this.millisecond, 3));
		}
		outputArray.push("Z");

		return outputArray.join("");
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "GeneralizedTime";
	}
	//**********************************************************************************
	/**
  * Convertion for the block to JSON object
  * @returns {Object}
  */
	toJSON() {
		let object = {};

		//region Seems at the moment (Sep 2016) there is no way how to check method is supported in "super" object
		try {
			object = super.toJSON();
		} catch (ex) {}
		//endregion

		object.year = this.year;
		object.month = this.month;
		object.day = this.day;
		object.hour = this.hour;
		object.minute = this.minute;
		object.second = this.second;
		object.millisecond = this.millisecond;

		return object;
	}
	//**********************************************************************************
}
exports.GeneralizedTime = GeneralizedTime; //**************************************************************************************
/**
 * @extends Utf8String
 */

class DATE extends Utf8String {
	//**********************************************************************************
	/**
  * Constructor for "DATE" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters);

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 31; // DATE
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "DATE";
	}
	//**********************************************************************************
}
exports.DATE = DATE; //**************************************************************************************
/**
 * @extends Utf8String
 */

class TimeOfDay extends Utf8String {
	//**********************************************************************************
	/**
  * Constructor for "TimeOfDay" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters);

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 32; // TimeOfDay
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "TimeOfDay";
	}
	//**********************************************************************************
}
exports.TimeOfDay = TimeOfDay; //**************************************************************************************
/**
 * @extends Utf8String
 */

class DateTime extends Utf8String {
	//**********************************************************************************
	/**
  * Constructor for "DateTime" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters);

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 33; // DateTime
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "DateTime";
	}
	//**********************************************************************************
}
exports.DateTime = DateTime; //**************************************************************************************
/**
 * @extends Utf8String
 */

class Duration extends Utf8String {
	//**********************************************************************************
	/**
  * Constructor for "Duration" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters);

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 34; // Duration
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "Duration";
	}
	//**********************************************************************************
}
exports.Duration = Duration; //**************************************************************************************
/**
 * @extends Utf8String
 */

class TIME extends Utf8String {
	//**********************************************************************************
	/**
  * Constructor for "Time" class
  * @param {Object} [parameters={}]
  */
	constructor(parameters = {}) {
		super(parameters);

		this.idBlock.tagClass = 1; // UNIVERSAL
		this.idBlock.tagNumber = 14; // Time
	}
	//**********************************************************************************
	/**
  * Aux function, need to get a block name. Need to have it here for inhiritence
  * @returns {string}
  */
	static blockName() {
		return "TIME";
	}
	//**********************************************************************************
}
exports.TIME = TIME; //**************************************************************************************
//endregion
//**************************************************************************************
//region Declaration of special ASN.1 schema type Choice
//**************************************************************************************

class Choice {
	//**********************************************************************************
	/**
  * Constructor for "Choice" class
  * @param {Object} [parameters={}]
  * @property {Array} [value] Array of ASN.1 types for make a choice from
  * @property {boolean} [optional]
  */
	constructor(parameters = {}) {
		this.value = (0, _pvutils.getParametersValue)(parameters, "value", []);
		this.optional = (0, _pvutils.getParametersValue)(parameters, "optional", false);
	}
	//**********************************************************************************
}
exports.Choice = Choice; //**************************************************************************************
//endregion
//**************************************************************************************
//region Declaration of special ASN.1 schema type Any
//**************************************************************************************

class Any {
	//**********************************************************************************
	/**
  * Constructor for "Any" class
  * @param {Object} [parameters={}]
  * @property {string} [name]
  * @property {boolean} [optional]
  */
	constructor(parameters = {}) {
		this.name = (0, _pvutils.getParametersValue)(parameters, "name", "");
		this.optional = (0, _pvutils.getParametersValue)(parameters, "optional", false);
	}
	//**********************************************************************************
}
exports.Any = Any; //**************************************************************************************
//endregion
//**************************************************************************************
//region Declaration of special ASN.1 schema type Repeated
//**************************************************************************************

class Repeated {
	//**********************************************************************************
	/**
  * Constructor for "Repeated" class
  * @param {Object} [parameters={}]
  * @property {string} [name]
  * @property {boolean} [optional]
  */
	constructor(parameters = {}) {
		this.name = (0, _pvutils.getParametersValue)(parameters, "name", "");
		this.optional = (0, _pvutils.getParametersValue)(parameters, "optional", false);
		this.value = (0, _pvutils.getParametersValue)(parameters, "value", new Any());
		this.local = (0, _pvutils.getParametersValue)(parameters, "local", false); // Could local or global array to store elements
	}
	//**********************************************************************************
}
exports.Repeated = Repeated; //**************************************************************************************
//endregion
//**************************************************************************************
//region Declaration of special ASN.1 schema type RawData
//**************************************************************************************
/**
 * @description Special class providing ability to have "toBER/fromBER" for raw ArrayBuffer
 */

class RawData {
	//**********************************************************************************
	/**
  * Constructor for "Repeated" class
  * @param {Object} [parameters={}]
  * @property {string} [name]
  * @property {boolean} [optional]
  */
	constructor(parameters = {}) {
		this.data = (0, _pvutils.getParametersValue)(parameters, "data", new ArrayBuffer(0));
	}
	//**********************************************************************************
	/**
  * Base function for converting block from BER encoded array of bytes
  * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
  * @param {!number} inputOffset Offset in ASN.1 BER encoded array where decoding should be started
  * @param {!number} inputLength Maximum length of array of bytes which can be using in this function
  * @returns {number} Offset after least decoded byte
  */
	fromBER(inputBuffer, inputOffset, inputLength) {
		this.data = inputBuffer.slice(inputOffset, inputLength);
		return inputOffset + inputLength;
	}
	//**********************************************************************************
	/**
  * Encoding of current ASN.1 block into ASN.1 encoded array (BER rules)
  * @param {boolean} [sizeOnly=false] Flag that we need only a size of encoding, not a real array of bytes
  * @returns {ArrayBuffer}
  */
	toBER(sizeOnly = false) {
		return this.data;
	}
	//**********************************************************************************
}
exports.RawData = RawData; //**************************************************************************************
//endregion
//**************************************************************************************
//region Major ASN.1 BER decoding function
//**************************************************************************************
/**
 * Internal library function for decoding ASN.1 BER
 * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array
 * @param {!number} inputOffset Offset in ASN.1 BER encoded array where decoding should be started
 * @param {!number} inputLength Maximum length of array of bytes which can be using in this function
 * @returns {{offset: number, result: Object}}
 */

function LocalFromBER(inputBuffer, inputOffset, inputLength) {
	const incomingOffset = inputOffset; // Need to store initial offset since "inputOffset" is changing in the function

	//region Local function changing a type for ASN.1 classes
	function localChangeType(inputObject, newType) {
		if (inputObject instanceof newType) return inputObject;

		const newObject = new newType();
		newObject.idBlock = inputObject.idBlock;
		newObject.lenBlock = inputObject.lenBlock;
		newObject.warnings = inputObject.warnings;
		//noinspection JSCheckFunctionSignatures
		newObject.valueBeforeDecode = inputObject.valueBeforeDecode.slice(0);

		return newObject;
	}
	//endregion

	//region Create a basic ASN.1 type since we need to return errors and warnings from the function
	let returnObject = new BaseBlock({}, Object);
	//endregion

	//region Basic check for parameters
	if ((0, _pvutils.checkBufferParams)(new LocalBaseBlock(), inputBuffer, inputOffset, inputLength) === false) {
		returnObject.error = "Wrong input parameters";
		return {
			offset: -1,
			result: returnObject
		};
	}
	//endregion

	//region Getting Uint8Array from ArrayBuffer
	const intBuffer = new Uint8Array(inputBuffer, inputOffset, inputLength);
	//endregion

	//region Initial checks
	if (intBuffer.length === 0) {
		this.error = "Zero buffer length";
		return {
			offset: -1,
			result: returnObject
		};
	}
	//endregion

	//region Decode indentifcation block of ASN.1 BER structure
	let resultOffset = returnObject.idBlock.fromBER(inputBuffer, inputOffset, inputLength);
	returnObject.warnings.concat(returnObject.idBlock.warnings);
	if (resultOffset === -1) {
		returnObject.error = returnObject.idBlock.error;
		return {
			offset: -1,
			result: returnObject
		};
	}

	inputOffset = resultOffset;
	inputLength -= returnObject.idBlock.blockLength;
	//endregion

	//region Decode length block of ASN.1 BER structure
	resultOffset = returnObject.lenBlock.fromBER(inputBuffer, inputOffset, inputLength);
	returnObject.warnings.concat(returnObject.lenBlock.warnings);
	if (resultOffset === -1) {
		returnObject.error = returnObject.lenBlock.error;
		return {
			offset: -1,
			result: returnObject
		};
	}

	inputOffset = resultOffset;
	inputLength -= returnObject.lenBlock.blockLength;
	//endregion

	//region Check for usign indefinite length form in encoding for primitive types
	if (returnObject.idBlock.isConstructed === false && returnObject.lenBlock.isIndefiniteForm === true) {
		returnObject.error = "Indefinite length form used for primitive encoding form";
		return {
			offset: -1,
			result: returnObject
		};
	}
	//endregion

	//region Switch ASN.1 block type
	let newASN1Type = BaseBlock;

	switch (returnObject.idBlock.tagClass) {
		//region UNIVERSAL
		case 1:
			//region Check for reserved tag numbers
			if (returnObject.idBlock.tagNumber >= 37 && returnObject.idBlock.isHexOnly === false) {
				returnObject.error = "UNIVERSAL 37 and upper tags are reserved by ASN.1 standard";
				return {
					offset: -1,
					result: returnObject
				};
			}
			//endregion

			switch (returnObject.idBlock.tagNumber) {
				//region EndOfContent type
				case 0:
					//region Check for EndOfContent type
					if (returnObject.idBlock.isConstructed === true && returnObject.lenBlock.length > 0) {
						returnObject.error = "Type [UNIVERSAL 0] is reserved";
						return {
							offset: -1,
							result: returnObject
						};
					}
					//endregion

					newASN1Type = EndOfContent;

					break;
				//endregion
				//region Boolean type
				case 1:
					newASN1Type = Boolean;
					break;
				//endregion
				//region Integer type
				case 2:
					newASN1Type = Integer;
					break;
				//endregion
				//region BitString type
				case 3:
					newASN1Type = BitString;
					break;
				//endregion
				//region OctetString type
				case 4:
					newASN1Type = OctetString;
					break;
				//endregion
				//region Null type
				case 5:
					newASN1Type = Null;
					break;
				//endregion
				//region OBJECT IDENTIFIER type
				case 6:
					newASN1Type = ObjectIdentifier;
					break;
				//endregion
				//region Enumerated type
				case 10:
					newASN1Type = Enumerated;
					break;
				//endregion
				//region Utf8String type
				case 12:
					newASN1Type = Utf8String;
					break;
				//endregion
				//region Time type
				case 14:
					newASN1Type = TIME;
					break;
				//endregion
				//region ASN.1 reserved type
				case 15:
					returnObject.error = "[UNIVERSAL 15] is reserved by ASN.1 standard";
					return {
						offset: -1,
						result: returnObject
					};
				//endregion
				//region Sequence type
				case 16:
					newASN1Type = Sequence;
					break;
				//endregion
				//region Set type
				case 17:
					newASN1Type = Set;
					break;
				//endregion
				//region NumericString type
				case 18:
					newASN1Type = NumericString;
					break;
				//endregion
				//region PrintableString type
				case 19:
					newASN1Type = PrintableString;
					break;
				//endregion
				//region TeletexString type
				case 20:
					newASN1Type = TeletexString;
					break;
				//endregion
				//region VideotexString type
				case 21:
					newASN1Type = VideotexString;
					break;
				//endregion
				//region IA5String type
				case 22:
					newASN1Type = IA5String;
					break;
				//endregion
				//region UTCTime type
				case 23:
					newASN1Type = UTCTime;
					break;
				//endregion
				//region GeneralizedTime type
				case 24:
					newASN1Type = GeneralizedTime;
					break;
				//endregion
				//region GraphicString type
				case 25:
					newASN1Type = GraphicString;
					break;
				//endregion
				//region VisibleString type
				case 26:
					newASN1Type = VisibleString;
					break;
				//endregion
				//region GeneralString type
				case 27:
					newASN1Type = GeneralString;
					break;
				//endregion
				//region UniversalString type
				case 28:
					newASN1Type = UniversalString;
					break;
				//endregion
				//region CharacterString type
				case 29:
					newASN1Type = CharacterString;
					break;
				//endregion
				//region BmpString type
				case 30:
					newASN1Type = BmpString;
					break;
				//endregion
				//region DATE type
				case 31:
					newASN1Type = DATE;
					break;
				//endregion
				//region TimeOfDay type
				case 32:
					newASN1Type = TimeOfDay;
					break;
				//endregion
				//region Date-Time type
				case 33:
					newASN1Type = DateTime;
					break;
				//endregion
				//region Duration type
				case 34:
					newASN1Type = Duration;
					break;
				//endregion
				//region default
				default:
					{
						let newObject;

						if (returnObject.idBlock.isConstructed === true) newObject = new Constructed();else newObject = new Primitive();

						newObject.idBlock = returnObject.idBlock;
						newObject.lenBlock = returnObject.lenBlock;
						newObject.warnings = returnObject.warnings;

						returnObject = newObject;

						resultOffset = returnObject.fromBER(inputBuffer, inputOffset, inputLength);
					}
				//endregion
			}
			break;
		//endregion
		//region All other tag classes
		case 2: // APPLICATION
		case 3: // CONTEXT-SPECIFIC
		case 4: // PRIVATE
		default:
			{
				if (returnObject.idBlock.isConstructed === true) newASN1Type = Constructed;else newASN1Type = Primitive;
			}
		//endregion
	}
	//endregion

	//region Change type and perform BER decoding
	returnObject = localChangeType(returnObject, newASN1Type);
	resultOffset = returnObject.fromBER(inputBuffer, inputOffset, returnObject.lenBlock.isIndefiniteForm === true ? inputLength : returnObject.lenBlock.length);
	//endregion

	//region Coping incoming buffer for entire ASN.1 block
	returnObject.valueBeforeDecode = inputBuffer.slice(incomingOffset, incomingOffset + returnObject.blockLength);
	//endregion

	return {
		offset: resultOffset,
		result: returnObject
	};
}
//**************************************************************************************
/**
 * Major function for decoding ASN.1 BER array into internal library structuries
 * @param {!ArrayBuffer} inputBuffer ASN.1 BER encoded array of bytes
 */
function fromBER(inputBuffer) {
	if (inputBuffer.byteLength === 0) {
		const result = new BaseBlock({}, Object);
		result.error = "Input buffer has zero length";

		return {
			offset: -1,
			result
		};
	}

	return LocalFromBER(inputBuffer, 0, inputBuffer.byteLength);
}
//**************************************************************************************
//endregion
//**************************************************************************************
//region Major scheme verification function
//**************************************************************************************
/**
 * Compare of two ASN.1 object trees
 * @param {!Object} root Root of input ASN.1 object tree
 * @param {!Object} inputData Input ASN.1 object tree
 * @param {!Object} inputSchema Input ASN.1 schema to compare with
 * @return {{verified: boolean}|{verified:boolean, result: Object}}
 */
function compareSchema(root, inputData, inputSchema) {
	//region Special case for Choice schema element type
	if (inputSchema instanceof Choice) {
		const choiceResult = false;

		for (let j = 0; j < inputSchema.value.length; j++) {
			const result = compareSchema(root, inputData, inputSchema.value[j]);
			if (result.verified === true) {
				return {
					verified: true,
					result: root
				};
			}
		}

		if (choiceResult === false) {
			const _result = {
				verified: false,
				result: {
					error: "Wrong values for Choice type"
				}
			};

			if (inputSchema.hasOwnProperty("name")) _result.name = inputSchema.name;

			return _result;
		}
	}
	//endregion

	//region Special case for Any schema element type
	if (inputSchema instanceof Any) {
		//region Add named component of ASN.1 schema
		if (inputSchema.hasOwnProperty("name")) root[inputSchema.name] = inputData;
		//endregion

		return {
			verified: true,
			result: root
		};
	}
	//endregion

	//region Initial check
	if (root instanceof Object === false) {
		return {
			verified: false,
			result: { error: "Wrong root object" }
		};
	}

	if (inputData instanceof Object === false) {
		return {
			verified: false,
			result: { error: "Wrong ASN.1 data" }
		};
	}

	if (inputSchema instanceof Object === false) {
		return {
			verified: false,
			result: { error: "Wrong ASN.1 schema" }
		};
	}

	if ("idBlock" in inputSchema === false) {
		return {
			verified: false,
			result: { error: "Wrong ASN.1 schema" }
		};
	}
	//endregion

	//region Comparing idBlock properties in ASN.1 data and ASN.1 schema
	//region Encode and decode ASN.1 schema idBlock
	/// <remarks>This encoding/decoding is neccessary because could be an errors in schema definition</remarks>
	if ("fromBER" in inputSchema.idBlock === false) {
		return {
			verified: false,
			result: { error: "Wrong ASN.1 schema" }
		};
	}

	if ("toBER" in inputSchema.idBlock === false) {
		return {
			verified: false,
			result: { error: "Wrong ASN.1 schema" }
		};
	}

	const encodedId = inputSchema.idBlock.toBER(false);
	if (encodedId.byteLength === 0) {
		return {
			verified: false,
			result: { error: "Error encoding idBlock for ASN.1 schema" }
		};
	}

	const decodedOffset = inputSchema.idBlock.fromBER(encodedId, 0, encodedId.byteLength);
	if (decodedOffset === -1) {
		return {
			verified: false,
			result: { error: "Error decoding idBlock for ASN.1 schema" }
		};
	}
	//endregion

	//region tagClass
	if (inputSchema.idBlock.hasOwnProperty("tagClass") === false) {
		return {
			verified: false,
			result: { error: "Wrong ASN.1 schema" }
		};
	}

	if (inputSchema.idBlock.tagClass !== inputData.idBlock.tagClass) {
		return {
			verified: false,
			result: root
		};
	}
	//endregion
	//region tagNumber
	if (inputSchema.idBlock.hasOwnProperty("tagNumber") === false) {
		return {
			verified: false,
			result: { error: "Wrong ASN.1 schema" }
		};
	}

	if (inputSchema.idBlock.tagNumber !== inputData.idBlock.tagNumber) {
		return {
			verified: false,
			result: root
		};
	}
	//endregion
	//region isConstructed
	if (inputSchema.idBlock.hasOwnProperty("isConstructed") === false) {
		return {
			verified: false,
			result: { error: "Wrong ASN.1 schema" }
		};
	}

	if (inputSchema.idBlock.isConstructed !== inputData.idBlock.isConstructed) {
		return {
			verified: false,
			result: root
		};
	}
	//endregion
	//region isHexOnly
	if ("isHexOnly" in inputSchema.idBlock === false) // Since 'isHexOnly' is an inhirited property
		{
			return {
				verified: false,
				result: { error: "Wrong ASN.1 schema" }
			};
		}

	if (inputSchema.idBlock.isHexOnly !== inputData.idBlock.isHexOnly) {
		return {
			verified: false,
			result: root
		};
	}
	//endregion
	//region valueHex
	if (inputSchema.idBlock.isHexOnly === true) {
		if ("valueHex" in inputSchema.idBlock === false) // Since 'valueHex' is an inhirited property
			{
				return {
					verified: false,
					result: { error: "Wrong ASN.1 schema" }
				};
			}

		const schemaView = new Uint8Array(inputSchema.idBlock.valueHex);
		const asn1View = new Uint8Array(inputData.idBlock.valueHex);

		if (schemaView.length !== asn1View.length) {
			return {
				verified: false,
				result: root
			};
		}

		for (let i = 0; i < schemaView.length; i++) {
			if (schemaView[i] !== asn1View[1]) {
				return {
					verified: false,
					result: root
				};
			}
		}
	}
	//endregion
	//endregion

	//region Add named component of ASN.1 schema
	if (inputSchema.hasOwnProperty("name")) {
		inputSchema.name = inputSchema.name.replace(/^\s+|\s+$/g, "");
		if (inputSchema.name !== "") root[inputSchema.name] = inputData;
	}
	//endregion

	//region Getting next ASN.1 block for comparition
	if (inputSchema.idBlock.isConstructed === true) {
		let admission = 0;
		let result = { verified: false };

		let maxLength = inputSchema.valueBlock.value.length;

		if (maxLength > 0) {
			if (inputSchema.valueBlock.value[0] instanceof Repeated) maxLength = inputData.valueBlock.value.length;
		}

		//region Special case when constructive value has no elements
		if (maxLength === 0) {
			return {
				verified: true,
				result: root
			};
		}
		//endregion

		//region Special case when "inputData" has no values and "inputSchema" has all optional values
		if (inputData.valueBlock.value.length === 0 && inputSchema.valueBlock.value.length !== 0) {
			let _optional = true;

			for (let i = 0; i < inputSchema.valueBlock.value.length; i++) _optional = _optional && (inputSchema.valueBlock.value[i].optional || false);

			if (_optional === true) {
				return {
					verified: true,
					result: root
				};
			}

			//region Delete early added name of block
			if (inputSchema.hasOwnProperty("name")) {
				inputSchema.name = inputSchema.name.replace(/^\s+|\s+$/g, "");
				if (inputSchema.name !== "") delete root[inputSchema.name];
			}
			//endregion

			root.error = "Inconsistent object length";

			return {
				verified: false,
				result: root
			};
		}
		//endregion

		for (let i = 0; i < maxLength; i++) {
			//region Special case when there is an "optional" element of ASN.1 schema at the end
			if (i - admission >= inputData.valueBlock.value.length) {
				if (inputSchema.valueBlock.value[i].optional === false) {
					const _result = {
						verified: false,
						result: root
					};

					root.error = "Inconsistent length between ASN.1 data and schema";

					//region Delete early added name of block
					if (inputSchema.hasOwnProperty("name")) {
						inputSchema.name = inputSchema.name.replace(/^\s+|\s+$/g, "");
						if (inputSchema.name !== "") {
							delete root[inputSchema.name];
							_result.name = inputSchema.name;
						}
					}
					//endregion

					return _result;
				}
			}
			//endregion
			else {
					//region Special case for Repeated type of ASN.1 schema element
					if (inputSchema.valueBlock.value[0] instanceof Repeated) {
						result = compareSchema(root, inputData.valueBlock.value[i], inputSchema.valueBlock.value[0].value);
						if (result.verified === false) {
							if (inputSchema.valueBlock.value[0].optional === true) admission++;else {
								//region Delete early added name of block
								if (inputSchema.hasOwnProperty("name")) {
									inputSchema.name = inputSchema.name.replace(/^\s+|\s+$/g, "");
									if (inputSchema.name !== "") delete root[inputSchema.name];
								}
								//endregion

								return result;
							}
						}

						if ("name" in inputSchema.valueBlock.value[0] && inputSchema.valueBlock.value[0].name.length > 0) {
							let arrayRoot = {};

							if ("local" in inputSchema.valueBlock.value[0] && inputSchema.valueBlock.value[0].local === true) arrayRoot = inputData;else arrayRoot = root;

							if (typeof arrayRoot[inputSchema.valueBlock.value[0].name] === "undefined") arrayRoot[inputSchema.valueBlock.value[0].name] = [];

							arrayRoot[inputSchema.valueBlock.value[0].name].push(inputData.valueBlock.value[i]);
						}
					}
					//endregion
					else {
							result = compareSchema(root, inputData.valueBlock.value[i - admission], inputSchema.valueBlock.value[i]);
							if (result.verified === false) {
								if (inputSchema.valueBlock.value[i].optional === true) admission++;else {
									//region Delete early added name of block
									if (inputSchema.hasOwnProperty("name")) {
										inputSchema.name = inputSchema.name.replace(/^\s+|\s+$/g, "");
										if (inputSchema.name !== "") delete root[inputSchema.name];
									}
									//endregion

									return result;
								}
							}
						}
				}
		}

		if (result.verified === false) // The situation may take place if last element is "optional" and verification failed
			{
				const _result = {
					verified: false,
					result: root
				};

				//region Delete early added name of block
				if (inputSchema.hasOwnProperty("name")) {
					inputSchema.name = inputSchema.name.replace(/^\s+|\s+$/g, "");
					if (inputSchema.name !== "") {
						delete root[inputSchema.name];
						_result.name = inputSchema.name;
					}
				}
				//endregion

				return _result;
			}

		return {
			verified: true,
			result: root
		};
	}
	//endregion
	//region Ability to parse internal value for primitive-encoded value (value of OctetString, for example)
	if ("primitiveSchema" in inputSchema && "valueHex" in inputData.valueBlock) {
		//region Decoding of raw ASN.1 data
		const asn1 = fromBER(inputData.valueBlock.valueHex);
		if (asn1.offset === -1) {
			const _result = {
				verified: false,
				result: asn1.result
			};

			//region Delete early added name of block
			if (inputSchema.hasOwnProperty("name")) {
				inputSchema.name = inputSchema.name.replace(/^\s+|\s+$/g, "");
				if (inputSchema.name !== "") {
					delete root[inputSchema.name];
					_result.name = inputSchema.name;
				}
			}
			//endregion

			return _result;
		}
		//endregion

		return compareSchema(root, asn1.result, inputSchema.primitiveSchema);
	}

	return {
		verified: true,
		result: root
	};
	//endregion
}
//**************************************************************************************
//noinspection JSUnusedGlobalSymbols
/**
 * ASN.1 schema verification for ArrayBuffer data
 * @param {!ArrayBuffer} inputBuffer Input BER-encoded ASN.1 data
 * @param {!Object} inputSchema Input ASN.1 schema to verify against to
 * @return {{verified: boolean}|{verified:boolean, result: Object}}
 */
function verifySchema(inputBuffer, inputSchema) {
	//region Initial check
	if (inputSchema instanceof Object === false) {
		return {
			verified: false,
			result: { error: "Wrong ASN.1 schema type" }
		};
	}
	//endregion

	//region Decoding of raw ASN.1 data
	const asn1 = fromBER(inputBuffer);
	if (asn1.offset === -1) {
		return {
			verified: false,
			result: asn1.result
		};
	}
	//endregion

	//region Compare ASN.1 struct with input schema
	return compareSchema(asn1.result, asn1.result, inputSchema);
	//endregion
}
//**************************************************************************************
//endregion
//**************************************************************************************
//region Major function converting JSON to ASN.1 objects
//**************************************************************************************
//noinspection JSUnusedGlobalSymbols
/**
 * Converting from JSON to ASN.1 objects
 * @param {string|Object} json JSON string or object to convert to ASN.1 objects
 */
function fromJSON(json) {}
// TODO Implement

//**************************************************************************************
//endregion
//**************************************************************************************

},{"pvutils":3}],3:[function(require,module,exports){
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

},{}]},{},[1])(1)
});
var asn1js = globalThis.asn1js;
var EXPORTED_SYMBOLS = ["asn1js"];
