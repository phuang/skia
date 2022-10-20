OpCapability Shader
%1 = OpExtInstImport "GLSL.std.450"
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %main "main" %sk_Clockwise %sk_FragColor %c
OpExecutionMode %main OriginUpperLeft
OpName %sk_Clockwise "sk_Clockwise"
OpName %sk_FragColor "sk_FragColor"
OpName %s "s"
OpName %t "t"
OpName %c "c"
OpName %bottom_helper_h4Tss "bottom_helper_h4Tss"
OpName %helpers_helper_h4Tss "helpers_helper_h4Tss"
OpName %color "color"
OpName %helper_h4Tss "helper_h4Tss"
OpName %main "main"
OpDecorate %sk_Clockwise BuiltIn FrontFacing
OpDecorate %sk_FragColor RelaxedPrecision
OpDecorate %sk_FragColor Location 0
OpDecorate %sk_FragColor Index 0
OpDecorate %s Binding 0
OpDecorate %s DescriptorSet 0
OpDecorate %t Binding 1
OpDecorate %t DescriptorSet 0
OpDecorate %c Location 1
OpDecorate %28 RelaxedPrecision
OpDecorate %29 RelaxedPrecision
OpDecorate %color RelaxedPrecision
OpDecorate %39 RelaxedPrecision
OpDecorate %40 RelaxedPrecision
OpDecorate %42 RelaxedPrecision
OpDecorate %44 RelaxedPrecision
%bool = OpTypeBool
%_ptr_Input_bool = OpTypePointer Input %bool
%sk_Clockwise = OpVariable %_ptr_Input_bool Input
%float = OpTypeFloat 32
%v4float = OpTypeVector %float 4
%_ptr_Output_v4float = OpTypePointer Output %v4float
%sk_FragColor = OpVariable %_ptr_Output_v4float Output
%14 = OpTypeSampler
%_ptr_UniformConstant_14 = OpTypePointer UniformConstant %14
%s = OpVariable %_ptr_UniformConstant_14 UniformConstant
%17 = OpTypeImage %float 2D 0 0 0 1 Unknown
%_ptr_UniformConstant_17 = OpTypePointer UniformConstant %17
%t = OpVariable %_ptr_UniformConstant_17 UniformConstant
%v2float = OpTypeVector %float 2
%_ptr_Input_v2float = OpTypePointer Input %v2float
%c = OpVariable %_ptr_Input_v2float Input
%22 = OpTypeFunction %v4float %_ptr_UniformConstant_17 %_ptr_UniformConstant_14
%30 = OpTypeSampledImage %17
%_ptr_Function_v4float = OpTypePointer Function %v4float
%void = OpTypeVoid
%50 = OpTypeFunction %void
%bottom_helper_h4Tss = OpFunction %v4float None %22
%23 = OpFunctionParameter %_ptr_UniformConstant_17
%24 = OpFunctionParameter %_ptr_UniformConstant_14
%25 = OpLabel
%28 = OpLoad %17 %23
%29 = OpLoad %14 %24
%27 = OpSampledImage %30 %28 %29
%31 = OpLoad %v2float %c
%26 = OpImageSampleImplicitLod %v4float %27 %31
OpReturnValue %26
OpFunctionEnd
%helpers_helper_h4Tss = OpFunction %v4float None %22
%32 = OpFunctionParameter %_ptr_UniformConstant_17
%33 = OpFunctionParameter %_ptr_UniformConstant_14
%34 = OpLabel
%color = OpVariable %_ptr_Function_v4float Function
%39 = OpLoad %17 %32
%40 = OpLoad %14 %33
%38 = OpSampledImage %30 %39 %40
%41 = OpLoad %v2float %c
%37 = OpImageSampleImplicitLod %v4float %38 %41
OpStore %color %37
%42 = OpVectorShuffle %v4float %37 %37 2 1 0 3
%43 = OpFunctionCall %v4float %bottom_helper_h4Tss %32 %33
%44 = OpFMul %v4float %42 %43
OpReturnValue %44
OpFunctionEnd
%helper_h4Tss = OpFunction %v4float None %22
%45 = OpFunctionParameter %_ptr_UniformConstant_17
%46 = OpFunctionParameter %_ptr_UniformConstant_14
%47 = OpLabel
%48 = OpFunctionCall %v4float %helpers_helper_h4Tss %45 %46
OpReturnValue %48
OpFunctionEnd
%main = OpFunction %void None %50
%51 = OpLabel
%52 = OpFunctionCall %v4float %helper_h4Tss %t %s
OpStore %sk_FragColor %52
OpReturn
OpFunctionEnd
