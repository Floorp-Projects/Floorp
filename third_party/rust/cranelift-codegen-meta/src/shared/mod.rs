//! Shared definitions for the Cranelift intermediate language.

pub mod entities;
pub mod formats;
pub mod immediates;
pub mod instructions;
pub mod legalize;
pub mod settings;
pub mod types;

use crate::cdsl::formats::{FormatStructure, InstructionFormat};
use crate::cdsl::instructions::{AllInstructions, InstructionGroup};
use crate::cdsl::settings::SettingGroup;
use crate::cdsl::xform::TransformGroups;

use crate::shared::entities::EntityRefs;
use crate::shared::formats::Formats;
use crate::shared::immediates::Immediates;

use std::collections::HashMap;
use std::iter::FromIterator;
use std::rc::Rc;

pub(crate) struct Definitions {
    pub settings: SettingGroup,
    pub all_instructions: AllInstructions,
    pub instructions: InstructionGroup,
    pub imm: Immediates,
    pub formats: Formats,
    pub transform_groups: TransformGroups,
    pub entities: EntityRefs,
}

pub(crate) fn define() -> Definitions {
    let mut all_instructions = AllInstructions::new();

    let immediates = Immediates::new();
    let entities = EntityRefs::new();
    let formats = Formats::new(&immediates, &entities);
    let instructions =
        instructions::define(&mut all_instructions, &formats, &immediates, &entities);
    let transform_groups = legalize::define(&instructions, &immediates);

    Definitions {
        settings: settings::define(),
        all_instructions,
        instructions,
        imm: immediates,
        formats,
        transform_groups,
        entities,
    }
}

impl Definitions {
    /// Verifies certain properties of formats.
    ///
    /// - Formats must be uniquely named: if two formats have the same name, they must refer to the
    /// same data. Otherwise, two format variants in the codegen crate would have the same name.
    /// - Formats must be structurally different from each other. Otherwise, this would lead to
    /// code duplicate in the codegen crate.
    ///
    /// Returns a list of all the instruction formats effectively used.
    pub fn verify_instruction_formats(&self) -> Vec<&InstructionFormat> {
        let mut format_names: HashMap<&'static str, &Rc<InstructionFormat>> = HashMap::new();

        // A structure is: number of input value operands / whether there's varargs or not / names
        // of immediate fields.
        let mut format_structures: HashMap<FormatStructure, &InstructionFormat> = HashMap::new();

        for inst in self.all_instructions.values() {
            // Check name.
            if let Some(existing_format) = format_names.get(&inst.format.name) {
                assert!(
                    Rc::ptr_eq(&existing_format, &inst.format),
                    "formats must uniquely named; there's a\
                     conflict on the name '{}', please make sure it is used only once.",
                    existing_format.name
                );
            } else {
                format_names.insert(inst.format.name, &inst.format);
            }

            // Check structure.
            let key = inst.format.structure();
            if let Some(existing_format) = format_structures.get(&key) {
                assert_eq!(
                    existing_format.name, inst.format.name,
                    "duplicate instruction formats {} and {}; please remove one.",
                    existing_format.name, inst.format.name
                );
            } else {
                format_structures.insert(key, &inst.format);
            }
        }

        let mut result = Vec::from_iter(format_structures.into_iter().map(|(_, v)| v));
        result.sort_by_key(|format| format.name);
        result
    }
}
