use crate::cdsl::cpu_modes::CpuMode;
use crate::cdsl::instructions::{InstructionGroupBuilder, InstructionPredicateMap};
use crate::cdsl::isa::TargetIsa;
use crate::cdsl::recipes::Recipes;
use crate::cdsl::regs::{IsaRegs, IsaRegsBuilder, RegBankBuilder, RegClassBuilder};
use crate::cdsl::settings::{SettingGroup, SettingGroupBuilder};

use crate::shared::Definitions as SharedDefinitions;

fn define_settings(_shared: &SettingGroup) -> SettingGroup {
    let mut setting = SettingGroupBuilder::new("arm64");
    let has_lse = setting.add_bool("has_lse", "Has Large System Extensions support.", "", false);

    setting.add_predicate("use_lse", predicate!(has_lse));
    setting.build()
}

fn define_registers() -> IsaRegs {
    let mut regs = IsaRegsBuilder::new();

    // The `x31` regunit serves as the stack pointer / zero register depending on context. We
    // reserve it and don't model the difference.
    let builder = RegBankBuilder::new("IntRegs", "x")
        .units(32)
        .track_pressure(true);
    let int_regs = regs.add_bank(builder);

    let builder = RegBankBuilder::new("FloatRegs", "v")
        .units(32)
        .track_pressure(true);
    let float_regs = regs.add_bank(builder);

    let builder = RegBankBuilder::new("FlagRegs", "")
        .units(1)
        .names(vec!["nzcv"])
        .track_pressure(false);
    let flag_reg = regs.add_bank(builder);

    let builder = RegClassBuilder::new_toplevel("GPR", int_regs);
    regs.add_class(builder);

    let builder = RegClassBuilder::new_toplevel("FPR", float_regs);
    regs.add_class(builder);

    let builder = RegClassBuilder::new_toplevel("FLAG", flag_reg);
    regs.add_class(builder);

    regs.build()
}

pub(crate) fn define(shared_defs: &mut SharedDefinitions) -> TargetIsa {
    let settings = define_settings(&shared_defs.settings);
    let regs = define_registers();

    let inst_group = InstructionGroupBuilder::new(&mut shared_defs.all_instructions).build();

    let mut a64 = CpuMode::new("A64");

    // TODO refine these.
    let expand_flags = shared_defs.transform_groups.by_name("expand_flags");
    let narrow_flags = shared_defs.transform_groups.by_name("narrow_flags");
    a64.legalize_monomorphic(expand_flags);
    a64.legalize_default(narrow_flags);

    let cpu_modes = vec![a64];

    // TODO implement arm64 recipes.
    let recipes = Recipes::new();

    // TODO implement arm64 encodings and predicates.
    let encodings_predicates = InstructionPredicateMap::new();

    TargetIsa::new(
        "arm64",
        inst_group,
        settings,
        regs,
        recipes,
        cpu_modes,
        encodings_predicates,
    )
}
