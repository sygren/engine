struct Transform {
  column_major matrix<float, 4, 4> modelMatrix;
  column_major matrix<float, 4, 4> normalMatrix;
};

struct PointLight {
  float4 pos;      // 16 bytes
  float4 ambient;  // 16 bytes
  float4 diffuse;  // 16 bytes
  float4 specular; // 16 bytes

  float constant;
  float linear;
  float quadratic;
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
  float  OuterCutOff;
  float  InnerCutOff;
};

struct LightCounts {
  uint lightsCount;
  uint directionalLightsCount;
  uint spotLightsCount;
};

struct Material {
  uint   baseColorTexture;
  uint   metallicRoughnessTexture;
  uint   normalTexture;
  uint   occlusionTexture;
  uint   emissiveTexture;
  float4 baseColorFactor;
  float4 emissiveFactor;
  float  metallicFactor;
  float  roughnessFactor;
};

struct PhongMaterial {
  float3 diffuse;
  float3 specular;
  float  shininess;
}

struct PushConstants {
  uint64_t pVertexBufferAddress : VERTEX_BUFFER_ADDRESS; // 8 bytes
  uint     transformIndex;                               // 4 bytes;
  uint     materialIndex;                                // 4 bytes
};

struct Camera {
  column_major matrix<float, 4, 4> projView; // 64 bytes
  float4                           pos;      // 16 bytes
  float4                           front;    // 16 bytes
};

[[vk::binding(0, 0)]]
Texture2D gTextures[];
[[vk::binding(1, 0)]]
SamplerState gSamplers[];

[[vk::binding(0, 1)]]
StructuredBuffer<Transform> gTransforms;
[[vk::binding(1, 1)]]
ConstantBuffer<Camera> gCamera;
[[vk::binding(2, 1)]]
ConstantBuffer<LightCounts> gLightsCount;
[[vk::binding(3, 1)]]
StructuredBuffer<PointLight> gPointLights;
[[vk::binding(4, 1)]]
StructuredBuffer<DirectionalLight> gDirectionnalLights;
[[vk::binding(5, 1)]]
StructuredBuffer<SpotLight> gSpotLights;
[[vk::binding(6, 1)]]
StructuredBuffer<Material> gMaterials;

struct Vertex {
  float4 pos : POSITION;  // 16 bytes
  float4 normal : NORMAL; // 16 bytes
  float2 uv : UV;         // 8 bytes
};

struct VertexOutput {
  float4 pos : SV_Position;    // 16 bytes
  float4 worldPos : WORLD_POS; // 16 bytes
  float4 normal : NORMAL;      // 16 bytes
  float2 uv : UV;              // 8 bytes
};

[shader("vertex")]
VertexOutput vertMain(uint id: SV_VertexID, uniform PushConstants pc) {
  Vertex       v = ((Vertex *)pc.pVertexBufferAddress)[id];
  VertexOutput output;
  Transform    t  = gTransforms[pc.transformIndex];
  output.pos      = mul(mul(gCamera.projView, t.modelMatrix), v.pos);
  output.worldPos = mul(t.modelMatrix, v.pos);
  output.normal   = float4(mul((column_major float3x3)t.normalMatrix, v.normal.xyz), 0.f);
  output.uv       = v.uv;
  return output;
}

PhongMaterial PBRtoPhong(Material pbr_material, float2 uv) {

  PhongMaterial material;

  float3 albedo = gTextures[pbr_material.baseColorTexture].Sample(gSamplers[pbr_material.baseColorTexture], uv).xyz * pbr_material.baseColorFactor.xyz;

  float3 metallic_roughness = gTextures[pbr_material.metallicRoughnessTexture].Sample(gSamplers[pbr_material.metallicRoughnessTexture], uv).xyz;
  float  metallic           = metallic_roughness.z * pbr_material.metallicFactor;
  float  roughness          = metallic_roughness.y * pbr_material.roughnessFactor;

  material.diffuse   = albedo * (1.0f - metallic);
  material.specular  = lerp(float3(0.04), albedo, metallic);
  material.shininess = pow((1.f - roughness), 2.f) * 256.f;

  return material;
}

float3 processLight(float3 ambient, float3 diffuse, float3 specular, float3 light_dir, float3 normal, float3 view_dir, PhongMaterial material) {
  float3 reflect_dir = normalize(reflect(-light_dir, normal));

  float3 a = material.diffuse * ambient;
  float3 d = material.diffuse * max(dot(normal, light_dir), 0.f) * diffuse;
  float3 s = material.specular * pow(max(dot(reflect_dir, view_dir), 0.f), material.shininess) * specular;

  return a + d + s;
}

float3 processDirectionalLight(DirectionalLight light, float3 normal, float3 view_dir, PhongMaterial material) {
  float3 light_dir = normalize(-light.direction.xyz);
  return processLight(light.ambient.xyz, light.diffuse.xyz, light.specular.xyz, light_dir, normal, view_dir, material);
}

float3 processSpotLight(SpotLight light, float3 position, float3 normal, float3 view_dir, PhongMaterial material) {
  float3 light_dir = normalize(light.position.xyz - position);
  float  theta     = dot(light.direction.xyz, -light_dir);

  if (theta > light.OuterCutOff) {
    float3 res = processLight(light.ambient.xyz, light.diffuse.xyz, light.specular.xyz, light_dir, normal, view_dir, material);
    res *= clamp((theta - light.OuterCutOff) / (light.InnerCutOff - light.OuterCutOff), 0, 1);
    return res;
  }
  return float3(0);
}

float3 processPointLight(PointLight light, float3 position, float3 normal, float3 view_dir, PhongMaterial material) {
  float3 light_dir   = normalize(light.pos.xyz - position);
  float  distance    = length(light.pos.xyz - position);
  float  attenuation = 1.0 / (light.constant + distance * light.linear + distance * distance * light.quadratic);
  return attenuation * processLight(light.ambient.xyz, light.diffuse.xyz, light.specular.xyz, light_dir, normal, view_dir, material);
}

[shader("fragment")]
float4 fragMain(VertexOutput input, uniform PushConstants pc) : SV_Target {
  float3        norm         = normalize(input.normal.xyz);
  float3        view_dir     = normalize((gCamera.pos - input.worldPos).xyz);
  Material      pbr_material = gMaterials[pc.materialIndex];
  PhongMaterial material     = PBRtoPhong(pbr_material, input.uv);

  float3 res = float3(0.f, 0.f, 0.f);
  for (uint i = 0; i < gLightsCount.directionalLightsCount; i++) {
    res += processDirectionalLight(gDirectionnalLights[i], norm, view_dir, material);
  }
  for (uint i = 0; i < gLightsCount.lightsCount; i++) {
    res += processPointLight(gPointLights[i], input.worldPos.xyz, norm, view_dir, material);
  }
  for (uint i = 0; i < gLightsCount.spotLightsCount; i++) {
    res += processSpotLight(gSpotLights[i], input.worldPos.xyz, norm, view_dir, material);
  }

  return float4(res, 1.f);
}

[shader("fragment")]
float4 fragLight(VertexOutput input, uniform PushConstants pc) : SV_Target { return float4(1.f); }

[shader("fragment")]
float4 fragDepth(float4 fragCoord: SV_Position, VertexOutput input, uniform PushConstants pc) : SV_Target { return float4(float3(fragCoord.z), 1.f); }
