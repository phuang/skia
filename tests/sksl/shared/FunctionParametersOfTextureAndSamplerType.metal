#include <metal_stdlib>
#include <simd/simd.h>
using namespace metal;
struct Uniforms {
    sampler s;
};
struct Inputs {
    float2 c  [[user(locn1)]];
};
struct Outputs {
    half4 sk_FragColor [[color(0)]];
};
struct Globals {
    texture2d<half> t;
};
half4 bottom_helper_h4Tss(Inputs _in, texture2d<half> t_param, sampler s_param) {
    return sample(makeSampler2D(t_param, s_param), _in.c);
}
half4 helpers_helper_h4Tss(Inputs _in, texture2d<half> t_param, sampler s_param) {
    half4 color = sample(makeSampler2D(t_param, s_param), _in.c);
    return color.zyxw * bottom_helper_h4Tss(_in, t_param, s_param);
}
half4 helper_h4Tss(Inputs _in, texture2d<half> t_param, sampler s_param) {
    return helpers_helper_h4Tss(_in, t_param, s_param);
}
fragment Outputs fragmentMain(Inputs _in [[stage_in]], constant Uniforms& _uniforms [[buffer(0)]], texture2d<half> t [[texture(1)]], bool _frontFacing [[front_facing]], float4 _fragCoord [[position]]) {
    Globals _globals{t};
    (void)_globals;
    Outputs _out;
    (void)_out;
    _out.sk_FragColor = helper_h4Tss(_in, _uniforms.t, _uniforms.s);
    return _out;
}
