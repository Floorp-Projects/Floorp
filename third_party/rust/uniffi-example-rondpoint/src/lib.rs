/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::collections::HashMap;

#[derive(Debug, Clone)]
pub struct Dictionnaire {
    un: Enumeration,
    deux: bool,
    petit_nombre: u8,
    gros_nombre: u64,
}

#[derive(Debug, Clone)]
pub struct DictionnaireNombres {
    petit_nombre: u8,
    court_nombre: u16,
    nombre_simple: u32,
    gros_nombre: u64,
}

#[derive(Debug, Clone)]
pub struct DictionnaireNombresSignes {
    petit_nombre: i8,
    court_nombre: i16,
    nombre_simple: i32,
    gros_nombre: i64,
}

#[derive(Debug, Clone)]
pub enum Enumeration {
    Un,
    Deux,
    Trois,
}

#[derive(Debug, Clone)]
pub enum EnumerationAvecDonnees {
    Zero,
    Un { premier: u32 },
    Deux { premier: u32, second: String },
}

#[allow(non_camel_case_types)]
#[allow(non_snake_case)]
pub struct minusculeMAJUSCULEDict {
    minusculeMAJUSCULEField: bool,
}

#[allow(non_camel_case_types)]
pub enum minusculeMAJUSCULEEnum {
    minusculeMAJUSCULEVariant,
}

fn copie_enumeration(e: Enumeration) -> Enumeration {
    e
}

fn copie_enumerations(e: Vec<Enumeration>) -> Vec<Enumeration> {
    e
}

fn copie_carte(
    e: HashMap<String, EnumerationAvecDonnees>,
) -> HashMap<String, EnumerationAvecDonnees> {
    e
}

fn copie_dictionnaire(d: Dictionnaire) -> Dictionnaire {
    d
}

fn switcheroo(b: bool) -> bool {
    !b
}

// Test that values can traverse both ways across the FFI.
// Even if roundtripping works, it's possible we have
// symmetrical errors that cancel each other out.
#[derive(Debug, Clone)]
struct Retourneur;
impl Retourneur {
    fn new() -> Self {
        Retourneur
    }
    fn identique_i8(&self, value: i8) -> i8 {
        value
    }
    fn identique_u8(&self, value: u8) -> u8 {
        value
    }
    fn identique_i16(&self, value: i16) -> i16 {
        value
    }
    fn identique_u16(&self, value: u16) -> u16 {
        value
    }
    fn identique_i32(&self, value: i32) -> i32 {
        value
    }
    fn identique_u32(&self, value: u32) -> u32 {
        value
    }
    fn identique_i64(&self, value: i64) -> i64 {
        value
    }
    fn identique_u64(&self, value: u64) -> u64 {
        value
    }
    fn identique_float(&self, value: f32) -> f32 {
        value
    }
    fn identique_double(&self, value: f64) -> f64 {
        value
    }
    fn identique_boolean(&self, value: bool) -> bool {
        value
    }
    fn identique_string(&self, value: String) -> String {
        value
    }
    fn identique_nombres_signes(
        &self,
        value: DictionnaireNombresSignes,
    ) -> DictionnaireNombresSignes {
        value
    }
    fn identique_nombres(&self, value: DictionnaireNombres) -> DictionnaireNombres {
        value
    }
    fn identique_optionneur_dictionnaire(
        &self,
        value: OptionneurDictionnaire,
    ) -> OptionneurDictionnaire {
        value
    }
}

#[derive(Debug, Clone)]
struct Stringifier;

#[allow(dead_code)]
impl Stringifier {
    fn new() -> Self {
        Stringifier
    }
    fn to_string_i8(&self, value: i8) -> String {
        value.to_string()
    }
    fn to_string_u8(&self, value: u8) -> String {
        value.to_string()
    }
    fn to_string_i16(&self, value: i16) -> String {
        value.to_string()
    }
    fn to_string_u16(&self, value: u16) -> String {
        value.to_string()
    }
    fn to_string_i32(&self, value: i32) -> String {
        value.to_string()
    }
    fn to_string_u32(&self, value: u32) -> String {
        value.to_string()
    }
    fn to_string_i64(&self, value: i64) -> String {
        value.to_string()
    }
    fn to_string_u64(&self, value: u64) -> String {
        value.to_string()
    }
    fn to_string_float(&self, value: f32) -> String {
        value.to_string()
    }
    fn to_string_double(&self, value: f64) -> String {
        value.to_string()
    }
    fn to_string_boolean(&self, value: bool) -> String {
        value.to_string()
    }
    fn well_known_string(&self, value: String) -> String {
        format!("uniffi 💚 {value}!")
    }
}

#[derive(Debug, Clone)]
struct Optionneur;
impl Optionneur {
    fn new() -> Self {
        Optionneur
    }
    fn sinon_string(&self, value: String) -> String {
        value
    }
    fn sinon_null(&self, value: Option<String>) -> Option<String> {
        value
    }
    fn sinon_boolean(&self, value: bool) -> bool {
        value
    }
    fn sinon_sequence(&self, value: Vec<String>) -> Vec<String> {
        value
    }

    fn sinon_zero(&self, value: Option<i32>) -> Option<i32> {
        value
    }

    fn sinon_u8_dec(&self, value: u8) -> u8 {
        value
    }
    fn sinon_i8_dec(&self, value: i8) -> i8 {
        value
    }
    fn sinon_u16_dec(&self, value: u16) -> u16 {
        value
    }
    fn sinon_i16_dec(&self, value: i16) -> i16 {
        value
    }
    fn sinon_u32_dec(&self, value: u32) -> u32 {
        value
    }
    fn sinon_i32_dec(&self, value: i32) -> i32 {
        value
    }
    fn sinon_u64_dec(&self, value: u64) -> u64 {
        value
    }
    fn sinon_i64_dec(&self, value: i64) -> i64 {
        value
    }

    fn sinon_u8_hex(&self, value: u8) -> u8 {
        value
    }
    fn sinon_i8_hex(&self, value: i8) -> i8 {
        value
    }
    fn sinon_u16_hex(&self, value: u16) -> u16 {
        value
    }
    fn sinon_i16_hex(&self, value: i16) -> i16 {
        value
    }
    fn sinon_u32_hex(&self, value: u32) -> u32 {
        value
    }
    fn sinon_i32_hex(&self, value: i32) -> i32 {
        value
    }
    fn sinon_u64_hex(&self, value: u64) -> u64 {
        value
    }
    fn sinon_i64_hex(&self, value: i64) -> i64 {
        value
    }

    fn sinon_u32_oct(&self, value: u32) -> u32 {
        value
    }

    fn sinon_f32(&self, value: f32) -> f32 {
        value
    }
    fn sinon_f64(&self, value: f64) -> f64 {
        value
    }

    fn sinon_enum(&self, value: Enumeration) -> Enumeration {
        value
    }
}

pub struct OptionneurDictionnaire {
    i8_var: i8,
    u8_var: u8,
    i16_var: i16,
    u16_var: u16,
    i32_var: i32,
    u32_var: u32,
    i64_var: i64,
    u64_var: u64,
    float_var: f32,
    double_var: f64,
    boolean_var: bool,
    string_var: String,
    list_var: Vec<String>,
    enumeration_var: Enumeration,
    dictionnaire_var: Option<minusculeMAJUSCULEEnum>,
}

include!(concat!(env!("OUT_DIR"), "/rondpoint.uniffi.rs"));
