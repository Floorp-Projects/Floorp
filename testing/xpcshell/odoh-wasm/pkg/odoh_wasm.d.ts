/* tslint:disable */
/* eslint-disable */
/**
* @returns {Uint8Array}
*/
export function get_odoh_config(): Uint8Array;
/**
* @param {Uint8Array} odoh_encrypted_query_msg
* @returns {Uint8Array}
*/
export function decrypt_query(odoh_encrypted_query_msg: Uint8Array): Uint8Array;
/**
* @param {Uint8Array} response
* @returns {Uint8Array}
*/
export function create_response(response: Uint8Array): Uint8Array;
