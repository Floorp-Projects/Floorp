use crate::cdsl::cpu_modes::CpuMode;
use crate::cdsl::instructions::{InstructionGroupBuilder, InstructionPredicateMap};
use crate::cdsl::isa::TargetIsa;
use crate::cdsl::recipes::Recipes;
use crate::cdsl::regs::{IsaRegs, IsaRegsBuilder, RegBankBuilder, RegClassBuilder};
use crate::cdsl::settings::{SettingGroup, SettingGroupBuilder};

use crate::shared::Definitions as SharedDefinitions;

fn define_settings(_shared: &SettingGroup) -> SettingGroup {
    let setting = SettingGroupBuilder::new("arm32");
    setting.build()
}

fn define_regs() -> IsaRegs {
    let mut regs = IsaRegsBuilder::new();

    let builder = RegBankBuilder::new("FloatRegs", "s")
        .units(64)
        .track_pressure(true);
    let float_regs = regs.add_bank(builder);

    let builder = RegBankBuilder::new("IntRegs", "r")
        .units(16)
        .track_pressure(true);
    let int_regs = regs.add_bank(builder);

    let builder = RegBankBuilder::new("FlagRegs", "")
        .units(1)
        .names(vec!["nzcv"])
        .track_pressure(false);
    let flag_reg = regs.add_bank(builder);

    let builder = RegClassBuilder::new_toplevel("S", float_regs).count(32);
    regs.add_class(builder);

    let builder = RegClassBuilder::new_toplevel("D", float_regs).width(2);
    regs.add_class(builder);

    let builder = RegClassBuilder::new_toplevel("Q", float_regs).width(4);
    regs.add_class(builder);

    let builder = RegClassBuilder::new_toplevel("GPR", int_regs);
    regs.add_class(builder);

    let builder = RegClassBuilder::new_toplevel("FLAG", flag_reg);
    regs.add_class(builder);

    regs.build()
}

pub(crate) fn define(shared_defs: &mut SharedDefinitions) -> TargetIsa {
    let settings = define_settings(&shared_defs.settings);
    let regs = define_regs();

    let inst_group = InstructionGroupBuilder::new(&mut shared_defs.all_instructions).build();

    // CPU modes for 32-bit ARM and Thumb2.
    let mut a32 = CpuMode::new("A32");
    let mut t32 = CpuMode::new("T32");

    // TODO refine these.
    let narrow_flags = shared_defs.transform_groups.by_name("narrow_flags");
    a32.legalize_default(narrow_flags);
    t32.legalize_default(narrow_flags);

    // Make sure that the expand code is used, thus generated.
    let expand = shared_defs.transform_groups.by_name("expand");
    a32.legalize_monomorphic(expand);

    let cpu_modes = vec![a32, t32];

    // TODO implement arm32 recipes.
    let recipes = Recipes::new();

    // TODO implement arm32 encodings and predicates.
    let encodings_predicates = InstructionPredicateMap::new();

    TargetIsa::new(
        "arm32",
        inst_group,
        settings,
        regs,
        recipes,
        cpu_modes,
        encodings_predicates,
    )
}
