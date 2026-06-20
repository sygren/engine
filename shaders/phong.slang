struct Material {
  float shininess;
  float _padding[3];
};

struct PointLight {
  float4 pos;      // 16 bytes
  float4 ambient;  // 16 bytes
  float4 diffuse;  // 16 bytes
  float4 specular; // 16 bytes

  float constant;
  float linear;
  float quadratic;
  float _padding;
};

struct DirectionalLight {
  float4 direction; // 16 bytes
  float4 ambient;   // 16 bytes
  float4 diffuse;   // 16 bytes
  float4 specular;  // 16 bytes
};

struct SpotLight {
  float4 position;  // 16 bytes
  float4 direction; // 16 bytes
  float4 ambient;   // 16 bytes
  float4 diffuse;   // 16 bytes
  float4 specular;  // 16 bytes
  float4 cutOff;    // X : OuterCutOff, Y: InnerCutOff
};

struct LightData {
  PointLight       pointLights[64]; // 64*64 bytes
  DirectionalLight directionalLights[64];
  SpotLight        spotLights[64];
  uint             lightsCount; // 4 bytes
  uint             directionalLightsCount;
  uint             spotLightsCount;
};

struct PushConstants {
  column_major matrix<float, 4, 4> mModel : MODEL_MATRIX;                        // 64 bytes
  column_major matrix<float, 4, 4> mTransformedModel : NORMAL_MATRIX;            // 64 bytes
  Material                         mMaterial;                                    // 32 bytes
  uint64_t                         pVertexBufferAddress : VERTEX_BUFFER_ADDRESS; // 8 bytes
};

struct Camera {
  column_major matrix<float, 4, 4> mProjView; // 64 bytes
  float4                           mPos;      // 16 bytes
  float4                           mFront;    // 16 bytes
};

struct Vertex {
  float4 mPos : POSITION;  // 16 bytes
  float4 mColor : COLOR;   // 16 bytes
  float4 mNormal : NORMAL; // 16 bytes
  float2 mUv : UV;         // 8 bytes
};

struct VertexOutput {
  float4 mPos : SV_Position;    // 16 bytes
  float4 mWorldPos : WORLD_POS; // 16 bytes
  float4 mNormal : NORMAL;      // 16 bytes
  float2 mUv : UV;              // 8 bytes
};

[[vk::binding(0, 0)]]
ConstantBuffer<Camera> gCamera;

[[vk::binding(1, 0)]]
ConstantBuffer<LightData> gLightsBuffer;

[[vk::binding(2, 0)]]
Texture2D gDiffuse;

[[vk::binding(3, 0)]]
Texture2D gSpecular;

[[vk::binding(4, 0)]]
SamplerState gDiffuseSampler;

[shader("vertex")]
VertexOutput vertMain(uint id: SV_VertexID, uniform PushConstants pc) {
  Vertex       v = ((Vertex *)pc.pVertexBufferAddress)[id];
  VertexOutput output;
  output.mPos      = mul(mul(gCamera.mProjView, pc.mModel), v.mPos);
  output.mWorldPos = mul(pc.mModel, v.mPos);
  output.mNormal   = float4(mul((column_major float3x3)pc.mTransformedModel, v.mNormal.xyz), 0.f);
  output.mUv       = v.mUv;
  return output;
}

float3 processLight(float3 ambient, float3 diffuse, float3 specular, float3 light_dir, float3 normal, float2 uv, float3 view_dir, float shininess) {
  float3 reflect_dir = normalize(reflect(-light_dir, normal));

  float3 a = gDiffuse.Sample(gDiffuseSampler, uv).xyz * ambient;
  float3 d = gDiffuse.Sample(gDiffuseSampler, uv).xyz * max(dot(normal, light_dir), 0.f) * diffuse;
  float3 s = gSpecular.Sample(gDiffuseSampler, uv).xyz * pow(max(dot(reflect_dir, view_dir), 0.f), shininess) * specular;

  return a + d + s;
}

float3 processDirectionalLight(DirectionalLight light, float3 normal, float2 uv, float3 view_dir, float shininess) {
  float3 light_dir = normalize(-light.direction.xyz);
  return processLight(light.ambient.xyz, light.diffuse.xyz, light.specular.xyz, light_dir, normal, uv, view_dir, shininess);
}

float3 processSpotLight(SpotLight light, float3 position, float3 normal, float2 uv, float3 view_dir, float shininess) {
  float3 light_dir = normalize(light.position.xyz - position);
  float  theta     = dot(light.direction.xyz, -light_dir);

  if (theta > light.cutOff.x) {
    float3 res = processLight(light.ambient.xyz, light.diffuse.xyz, light.specular.xyz, light_dir, normal, uv, view_dir, shininess);
    res *= clamp((theta - light.cutOff.x)/(light.cutOff.y - light.cutOff.x), 0, 1);
    return res;
  }
  return float3(0);
}

float3 processPointLight(PointLight light, float3 position, float3 normal, float2 uv, float3 view_dir, float shininess) {
  float3 light_dir   = normalize(light.pos.xyz - position);
  float  distance    = length(light.pos.xyz - position);
  float  attenuation = 1.0 / (light.constant + distance * light.linear + distance * distance * light.quadratic);
  return attenuation * processLight(light.ambient.xyz, light.diffuse.xyz, light.specular.xyz, light_dir, normal, uv, view_dir, shininess);
}

[shader("fragment")]
float4 fragMain(VertexOutput input, uniform PushConstants pc) : SV_Target {
  float3 norm     = normalize(input.mNormal.xyz);
  float3 view_dir = normalize((gCamera.mPos - input.mWorldPos).xyz);

  float3 res = 0;
  for (uint i = 0; i < gLightsBuffer.directionalLightsCount; i++) {
    res += processDirectionalLight(gLightsBuffer.directionalLights[i], norm, input.mUv, view_dir, pc.mMaterial.shininess);
  }
  for (uint i = 0; i < gLightsBuffer.lightsCount; i++) {
    res += processPointLight(gLightsBuffer.pointLights[i], input.mWorldPos.xyz, norm, input.mUv, view_dir, pc.mMaterial.shininess);
  }
  for (uint i = 0; i < gLightsBuffer.spotLightsCount; i++) {
    res += processSpotLight(gLightsBuffer.spotLights[i], input.mWorldPos.xyz, norm, input.mUv, view_dir, pc.mMaterial.shininess);
  }

  return float4(res, 1.f);
}

[shader("fragment")]
float4 fragLight(VertexOutput input, uniform PushConstants pc) : SV_Target { return float4(1.f); }
