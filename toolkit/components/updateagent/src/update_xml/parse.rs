/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use log::{debug, warn};
use std::{collections::BTreeMap, ffi::OsString, fs::File, io::BufReader};

pub struct Update {
    pub update_type: UpdateType,
    pub attributes: BTreeMap<String, String>,
    pub patches: Vec<UpdateXmlPatch>,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum UpdateType {
    Major,
    Minor,
}

pub struct UpdateXmlPatch {
    pub patch_type: UpdatePatchType,
    pub url: String,
    pub size: u64,
    pub attributes: BTreeMap<String, String>,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum UpdatePatchType {
    Complete,
    Partial,
}

pub fn parse_file(path: &OsString) -> Result<Vec<Update>, String> {
    let file = File::open(path)
        .map_err(|e| format!("Unable to open path \"{:?}\" for reading: {}", path, e))?;
    let buffered_reader = BufReader::new(file);
    let mut xml_reader = xml::EventReader::new(buffered_reader);
    loop {
        match get_next_xml_event(&mut xml_reader)? {
            xml::reader::XmlEvent::StartElement { name, .. } => {
                if name.local_name != "updates" || name.prefix.is_some() {
                    return Err(format!(
                        concat!(
                            "XML Parse Error: Expected top level element: <updates>. ",
                            "Instead found: <{}>"
                        ),
                        name
                    ));
                }
                break;
            }
            xml::reader::XmlEvent::EndElement { .. } => return Err(
                "XML Parse Error: Expected top level element. Instead found the end of an element"
                    .to_string(),
            ),
            _ => {}
        }
    }
    let mut updates: Vec<Update> = Vec::new();
    // Read tags within <updates> until we read the closing </updates> tag
    loop {
        match get_next_xml_event(&mut xml_reader)? {
            xml::reader::XmlEvent::StartElement {
                name, attributes, ..
            } => {
                // UpdateService.jsm's implementation ignores non-<update> tags, so this does
                // too.
                if name.local_name == "update" && name.prefix.is_none() {
                    let maybe_update = read_update(attributes, &mut xml_reader)?;
                    if let Some(update) = maybe_update {
                        updates.push(update);
                    }
                } else {
                    skip_tag(name, &mut xml_reader)?;
                }
            }
            xml::reader::XmlEvent::EndElement { name } => {
                if name.local_name != "updates" || name.prefix.is_some() {
                    return Err(format!(
                        "XML Parse Error: Expected: </updates>. Instead found: </{}>",
                        name
                    ));
                }
                break;
            }
            _ => {}
        }
    }
    // XML documents must have exactly one top-level element. Expect to read the end of the
    // document.
    loop {
        // This is the one place we don't want to use the get_next_xml_event wrapper, because
        // we are expecting to see EndDocument.
        let event = xml_reader
            .next()
            .map_err(|e| format!("XML Parser Error: {}", e))?;
        match event {
            xml::reader::XmlEvent::EndDocument => break,
            xml::reader::XmlEvent::StartElement { name, .. } => {
                return Err(format!(
                    "XML Parse Error: Expected end of document. Instead found: <{}>",
                    name
                ))
            }
            xml::reader::XmlEvent::EndElement { name } => {
                return Err(format!(
                    "XML Parse Error: Expected end of document. Instead found: </{}>",
                    name
                ))
            }
            _ => {}
        }
    }
    Ok(updates)
}

// This wrapper should be called to get the next XML event from the reader. This wrapper should
// be used whenever you are NOT expecting an EndDocument. The reason for the wrapper is that
// the standard pattern used in this file for reading XML events looks like this:
//   loop {
//       match reader.next()? {
//           cases_we_care_about => handling,
//           _ => {},
//       }
//   }
// This pattern has a potential footgun: If EndDocument is not one of the handled cases and we
// hit an unexpected EndDocument, the above pattern turns into an infinite loop. This wrapper
// avoids that footgun by converting any unexpected EndDocument to an error.
fn get_next_xml_event(
    reader: &mut xml::EventReader<BufReader<File>>,
) -> Result<xml::reader::XmlEvent, String> {
    let event = reader
        .next()
        .map_err(|e| format!("XML Parser Error: {}", e))?;
    if event == xml::reader::XmlEvent::EndDocument {
        return Err("XML Parse Error: Unexpected end of document".to_string());
    }
    Ok(event)
}

fn make_attr_map(
    attr_vec: Vec<xml::attribute::OwnedAttribute>,
) -> Result<BTreeMap<String, String>, String> {
    let mut attr_map = BTreeMap::new();
    for attr in attr_vec {
        let name = if let Some(mut prefixed_name) = attr.name.prefix {
            prefixed_name.push(':');
            prefixed_name.push_str(&attr.name.local_name);
            prefixed_name
        } else {
            attr.name.local_name
        };

        let prev_value = attr_map.insert(name.clone(), attr.value);
        if prev_value.is_some() {
            // It's invalid to have the same attribute twice in an element
            return Err(format!("XML Parse Error: Duplicate \"{}\" attribute", name));
        }
    }
    Ok(attr_map)
}

fn skip_tag(
    tag_to_skip: xml::name::OwnedName,
    reader: &mut xml::EventReader<BufReader<File>>,
) -> Result<(), String> {
    debug!("Skipping unexpected XML tag: <{}>", tag_to_skip);
    loop {
        match get_next_xml_event(reader)? {
            xml::reader::XmlEvent::StartElement { name, .. } => {
                skip_tag(name, reader)?;
            }
            xml::reader::XmlEvent::EndElement { name } => {
                if name == tag_to_skip {
                    return Ok(());
                } else {
                    return Err(format!(
                        "XML Parse Error: Expected </{}>. Instead found </{}>",
                        tag_to_skip, name
                    ));
                }
            }
            _ => {}
        }
    }
}

// Read an <update> tag. Updates with invalid values for the "type" attribute will be silently
// dropped by returning Ok(None).
fn read_update(
    attributes: Vec<xml::attribute::OwnedAttribute>,
    reader: &mut xml::EventReader<BufReader<File>>,
) -> Result<Option<Update>, String> {
    let mut attributes = make_attr_map(attributes)?;
    let maybe_update_type =
        attributes
            .remove("type")
            .and_then(|type_string| match type_string.as_str() {
                "major" => Some(UpdateType::Major),
                "minor" => Some(UpdateType::Minor),
                _ => None,
            });
    let mut maybe_update = if let Some(update_type) = maybe_update_type {
        Some(Update {
            update_type,
            attributes,
            patches: Vec::new(),
        })
    } else {
        warn!("Dropping malformed <update> XML element");
        None
    };
    // Read tags within the <update> tag until we reach the end of the tag
    loop {
        match get_next_xml_event(reader)? {
            xml::reader::XmlEvent::StartElement {
                name, attributes, ..
            } => {
                // UpdateService.jsm's implementation ignores unexpected tags, so this does too
                if let Some(update) = maybe_update.as_mut() {
                    if name.local_name == "patch" && name.prefix.is_none() {
                        let maybe_patch = read_patch(attributes, reader)?;
                        if let Some(patch) = maybe_patch {
                            update.patches.push(patch);
                        }
                    } else {
                        skip_tag(name, reader)?;
                    }
                } else {
                    skip_tag(name, reader)?;
                }
            }
            xml::reader::XmlEvent::EndElement { name } => {
                if name.local_name != "update" || name.prefix.is_some() {
                    return Err(format!(
                        "XML Parse Error: Expected </update>. Instead found </{}>",
                        name
                    ));
                }
                break;
            }
            _ => {}
        }
    }
    Ok(maybe_update)
}

// Read a <patch> tag. Patches with invalid values such as non-numeric size or types other than
// complete or partial will be silently dropped by returning Ok(None).
// Note for when this mockup gets turned into production code: We should probably log something
// in the cases when this function silently drops a patch.
fn read_patch(
    attributes: Vec<xml::attribute::OwnedAttribute>,
    reader: &mut xml::EventReader<BufReader<File>>,
) -> Result<Option<UpdateXmlPatch>, String> {
    let mut attributes = make_attr_map(attributes)?;
    let maybe_patch_type =
        attributes
            .remove("type")
            .and_then(|type_string| match type_string.as_str() {
                "partial" => Some(UpdatePatchType::Partial),
                "complete" => Some(UpdatePatchType::Complete),
                _ => None,
            });
    let maybe_url = attributes.remove("URL");
    let maybe_size = attributes
        .remove("size")
        .and_then(|size_string| size_string.parse::<u64>().ok());
    let maybe_patch = if let (Some(patch_type), Some(url), Some(size)) =
        (maybe_patch_type, maybe_url, maybe_size)
    {
        Some(UpdateXmlPatch {
            patch_type,
            url,
            size,
            attributes,
        })
    } else {
        warn!("Dropping malformed <patch> XML element");
        None
    };
    // Read tags within the <patch> tag until we reach the end of the tag
    loop {
        match get_next_xml_event(reader)? {
            xml::reader::XmlEvent::StartElement { name, .. } => {
                // UpdateService.jsm's implementation ignores unexpected tags, so this does too
                skip_tag(name, reader)?;
            }
            xml::reader::XmlEvent::EndElement { name } => {
                if name.local_name != "patch" || name.prefix.is_some() {
                    return Err(format!(
                        "XML Parse Error: Expected </patch>. Instead found </{}>",
                        name
                    ));
                }
                break;
            }
            _ => {}
        }
    }
    Ok(maybe_patch)
}
