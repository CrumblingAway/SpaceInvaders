struct VSInput {
    float3 Pos;
	float2 UV;
};

Sampler2D texture;

struct ShaderData {
    float4x4 projection;
    float4x4 view;
    float4x4 model;
};

struct VSOutput {
    float4 Pos : SV_POSITION;
    float2 UV;
};

[shader("vertex")]
VSOutput main(
    VSInput input,
    uniform ShaderData *shaderData
)
{
    VSOutput output;
    float4x4 modelMat = shaderData.model;
    output.UV = input.UV;
    output.Pos = mul(shaderData->projection, mul(shaderData->view, mul(modelMat, float4(input.Pos.xyz, 1.0))));
    return output;
}

[shader("fragment")]
float4 main(VSOutput input)
{
    // Sample from texture
    float3 color = texture.Sample(input.UV).rgb;
    return float4(color.rgb, 1.0);
}