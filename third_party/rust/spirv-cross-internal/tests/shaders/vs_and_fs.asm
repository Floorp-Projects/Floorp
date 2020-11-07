; SPIR-V
; Version: 1.0
; Generator: Khronos SPIR-V Tools Assembler; 0
; Bound: 25
; Schema: 0
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main_vs "main_vs" %_
               OpEntryPoint Fragment %main_fs "main_fs" %color
               OpExecutionMode %main_fs OriginUpperLeft
               OpSource GLSL 330
               OpName %main_vs "main_vs"
               OpName %main_fs "main_fs"
               OpName %gl_PerVertex "gl_PerVertex"
               OpMemberName %gl_PerVertex 0 "gl_Position"
               OpMemberName %gl_PerVertex 1 "gl_PointSize"
               OpMemberName %gl_PerVertex 2 "gl_ClipDistance"
               OpName %_ ""
               OpName %color "color"
               OpDecorate %color Location 0
               OpMemberDecorate %gl_PerVertex 0 BuiltIn Position
               OpMemberDecorate %gl_PerVertex 1 BuiltIn PointSize
               OpMemberDecorate %gl_PerVertex 2 BuiltIn ClipDistance
               OpDecorate %gl_PerVertex Block
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
       %uint = OpTypeInt 32 0
     %uint_1 = OpConstant %uint 1
%_arr_float_uint_1 = OpTypeArray %float %uint_1
%gl_PerVertex = OpTypeStruct %v4float %float %_arr_float_uint_1
%_ptr_Output_gl_PerVertex = OpTypePointer Output %gl_PerVertex
          %_ = OpVariable %_ptr_Output_gl_PerVertex Output
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
    %float_1 = OpConstant %float 1
         %19 = OpConstantComposite %v4float %float_1 %float_1 %float_1 %float_1
%_ptr_Output_v4float = OpTypePointer Output %v4float
    %main_vs = OpFunction %void None %3
          %5 = OpLabel
         %21 = OpAccessChain %_ptr_Output_v4float %_ %int_0
               OpStore %21 %19
               OpReturn
               OpFunctionEnd
          %4 = OpTypeFunction %void
      %color = OpVariable %_ptr_Output_v4float Output
         %13 = OpConstantComposite %v4float %float_1 %float_1 %float_1 %float_1
    %main_fs = OpFunction %void None %4
          %6 = OpLabel
               OpStore %color %13
               OpReturn
               OpFunctionEnd
