use spirv_cross_internal::{hlsl as lang, spirv};

mod common;
use crate::common::words_from_bytes;

#[test]
fn ast_gets_multiple_entry_points() {
    let module = spirv::Module::from_words(words_from_bytes(include_bytes!(
        "shaders/multiple_entry_points.cl.spv"
    )));
    let entry_points = spirv::Ast::<lang::Target>::parse(&module)
        .unwrap()
        .get_entry_points()
        .unwrap();

    assert_eq!(entry_points.len(), 2);
    assert!(entry_points.iter().any(|e| e.name == "entry_1"));
    assert!(entry_points.iter().any(|e| e.name == "entry_2"));
}

#[test]
fn ast_gets_shader_resources() {
    let module =
        spirv::Module::from_words(words_from_bytes(include_bytes!("shaders/simple.vert.spv")));
    let shader_resources = spirv::Ast::<lang::Target>::parse(&module)
        .unwrap()
        .get_shader_resources()
        .unwrap();

    let spirv::ShaderResources {
        uniform_buffers,
        stage_inputs,
        stage_outputs,
        ..
    } = shader_resources;

    assert_eq!(uniform_buffers.len(), 1);
    assert_eq!(uniform_buffers[0].name, "uniform_buffer_object");
    assert_eq!(shader_resources.storage_buffers.len(), 0);
    assert_eq!(stage_inputs.len(), 2);
    assert!(stage_inputs
        .iter()
        .any(|stage_input| stage_input.name == "a_normal"));
    assert!(stage_inputs
        .iter()
        .any(|stage_input| stage_input.name == "a_position"));
    assert_eq!(stage_outputs.len(), 1);
    assert!(stage_outputs
        .iter()
        .any(|stage_output| stage_output.name == "v_normal"));
    assert_eq!(shader_resources.subpass_inputs.len(), 0);
    assert_eq!(shader_resources.storage_images.len(), 0);
    assert_eq!(shader_resources.sampled_images.len(), 0);
    assert_eq!(shader_resources.atomic_counters.len(), 0);
    assert_eq!(shader_resources.push_constant_buffers.len(), 0);
    assert_eq!(shader_resources.separate_images.len(), 0);
    assert_eq!(shader_resources.separate_samplers.len(), 0);
}

#[test]
fn ast_gets_decoration() {
    let module =
        spirv::Module::from_words(words_from_bytes(include_bytes!("shaders/simple.vert.spv")));
    let ast = spirv::Ast::<lang::Target>::parse(&module).unwrap();

    let stage_inputs = ast.get_shader_resources().unwrap().stage_inputs;
    let decoration = ast
        .get_decoration(stage_inputs[0].id, spirv::Decoration::DescriptorSet)
        .unwrap();
    assert_eq!(decoration, 0);
}

#[test]
fn ast_sets_decoration() {
    let module =
        spirv::Module::from_words(words_from_bytes(include_bytes!("shaders/simple.vert.spv")));
    let mut ast = spirv::Ast::<lang::Target>::parse(&module).unwrap();

    let stage_inputs = ast.get_shader_resources().unwrap().stage_inputs;
    let updated_value = 3;
    ast.set_decoration(
        stage_inputs[0].id,
        spirv::Decoration::DescriptorSet,
        updated_value,
    )
    .unwrap();
    assert_eq!(
        ast.get_decoration(stage_inputs[0].id, spirv::Decoration::DescriptorSet)
            .unwrap(),
        updated_value
    );
}

#[test]
fn ast_gets_type_member_types_and_array() {
    let module =
        spirv::Module::from_words(words_from_bytes(include_bytes!("shaders/simple.vert.spv")));
    let ast = spirv::Ast::<lang::Target>::parse(&module).unwrap();

    let uniform_buffers = ast.get_shader_resources().unwrap().uniform_buffers;

    let is_struct = match ast.get_type(uniform_buffers[0].base_type_id).unwrap() {
        spirv::Type::Struct {
            member_types,
            array,
        } => {
            assert_eq!(member_types.len(), 2);
            assert_eq!(array.len(), 0);
            true
        }
        _ => false,
    };

    assert!(is_struct);
}

#[test]
fn ast_gets_array_dimensions() {
    let module =
        spirv::Module::from_words(words_from_bytes(include_bytes!("shaders/array.vert.spv")));
    let ast = spirv::Ast::<lang::Target>::parse(&module).unwrap();

    let uniform_buffers = ast.get_shader_resources().unwrap().uniform_buffers;

    let is_struct = match ast.get_type(uniform_buffers[0].base_type_id).unwrap() {
        spirv::Type::Struct { member_types, .. } => {
            assert_eq!(member_types.len(), 3);
            let is_float = match ast.get_type(member_types[2]).unwrap() {
                spirv::Type::Float { array } => {
                    assert_eq!(array.len(), 1);
                    assert_eq!(array[0], 3);
                    true
                }
                _ => false,
            };
            assert!(is_float);
            true
        }
        _ => false,
    };

    assert!(is_struct);
}

#[test]
fn ast_gets_declared_struct_size_and_struct_member_size() {
    let module =
        spirv::Module::from_words(words_from_bytes(include_bytes!("shaders/simple.vert.spv")));
    let ast = spirv::Ast::<lang::Target>::parse(&module).unwrap();
    let uniform_buffers = ast.get_shader_resources().unwrap().uniform_buffers;
    let mat4_size = 4 * 16;
    let float_size = 4;
    assert_eq!(
        ast.get_declared_struct_size(uniform_buffers[0].base_type_id)
            .unwrap(),
        mat4_size + float_size
    );
    assert_eq!(
        ast.get_declared_struct_member_size(uniform_buffers[0].base_type_id, 0)
            .unwrap(),
        mat4_size
    );
    assert_eq!(
        ast.get_declared_struct_member_size(uniform_buffers[0].base_type_id, 1)
            .unwrap(),
        float_size
    );
}

#[test]
fn ast_gets_member_name() {
    let module =
        spirv::Module::from_words(words_from_bytes(include_bytes!("shaders/simple.vert.spv")));
    let ast = spirv::Ast::<lang::Target>::parse(&module).unwrap();

    let uniform_buffers = ast.get_shader_resources().unwrap().uniform_buffers;

    assert_eq!(
        ast.get_member_name(uniform_buffers[0].base_type_id, 0)
            .unwrap(),
        "u_model_view_projection"
    );
}

#[test]
fn ast_gets_member_decoration() {
    let module =
        spirv::Module::from_words(words_from_bytes(include_bytes!("shaders/simple.vert.spv")));
    let ast = spirv::Ast::<lang::Target>::parse(&module).unwrap();

    let uniform_buffers = ast.get_shader_resources().unwrap().uniform_buffers;

    assert_eq!(
        ast.get_member_decoration(
            uniform_buffers[0].base_type_id,
            1,
            spirv::Decoration::Offset
        )
        .unwrap(),
        64
    );
}

#[test]
fn ast_sets_member_decoration() {
    let module =
        spirv::Module::from_words(words_from_bytes(include_bytes!("shaders/simple.vert.spv")));
    let mut ast = spirv::Ast::<lang::Target>::parse(&module).unwrap();

    let uniform_buffers = ast.get_shader_resources().unwrap().uniform_buffers;

    let new_offset = 128;

    ast.set_member_decoration(
        uniform_buffers[0].base_type_id,
        1,
        spirv::Decoration::Offset,
        new_offset,
    )
    .unwrap();

    assert_eq!(
        ast.get_member_decoration(
            uniform_buffers[0].base_type_id,
            1,
            spirv::Decoration::Offset
        )
        .unwrap(),
        new_offset
    );
}

#[test]
fn ast_gets_specialization_constants() {
    let comp = spirv::Module::from_words(words_from_bytes(include_bytes!(
        "shaders/specialization.comp.spv"
    )));
    let comp_ast = spirv::Ast::<lang::Target>::parse(&comp).unwrap();
    let specialization_constants = comp_ast.get_specialization_constants().unwrap();
    assert_eq!(specialization_constants[0].constant_id, 10);
}

#[test]
fn ast_gets_work_group_size_specialization_constants() {
    let comp = spirv::Module::from_words(words_from_bytes(include_bytes!(
        "shaders/workgroup.comp.spv"
    )));
    let comp_ast = spirv::Ast::<lang::Target>::parse(&comp).unwrap();
    let work_group_size = comp_ast
        .get_work_group_size_specialization_constants()
        .unwrap();
    assert_eq!(
        work_group_size,
        spirv::WorkGroupSizeSpecializationConstants {
            x: spirv::SpecializationConstant {
                id: 7,
                constant_id: 5,
            },
            y: spirv::SpecializationConstant {
                id: 8,
                constant_id: 10,
            },
            z: spirv::SpecializationConstant {
                id: 9,
                constant_id: 15,
            },
        }
    );
}

#[test]
fn ast_gets_active_buffer_ranges() {
    let module =
        spirv::Module::from_words(words_from_bytes(include_bytes!("shaders/two_ubo.vert.spv")));
    let ast = spirv::Ast::<lang::Target>::parse(&module).unwrap();

    let uniform_buffers = ast.get_shader_resources().unwrap().uniform_buffers;
    assert_eq!(uniform_buffers.len(), 2);

    let ubo1 = ast.get_active_buffer_ranges(uniform_buffers[0].id).unwrap();
    assert_eq!(
        ubo1,
        [
            spirv::BufferRange {
                index: 0,
                offset: 0,
                range: 64,
            },
            spirv::BufferRange {
                index: 1,
                offset: 64,
                range: 16,
            },
            spirv::BufferRange {
                index: 2,
                offset: 80,
                range: 32,
            }
        ]
    );

    let ubo2 = ast.get_active_buffer_ranges(uniform_buffers[1].id).unwrap();
    assert_eq!(
        ubo2,
        [
            spirv::BufferRange {
                index: 0,
                offset: 0,
                range: 16,
            },
            spirv::BufferRange {
                index: 1,
                offset: 16,
                range: 16,
            },
            spirv::BufferRange {
                index: 2,
                offset: 32,
                range: 12,
            }
        ]
    );
}
