struct Transform {
  column_major matrix<float, 4, 4> modelMatrix;
  column_major matrix<float, 4, 4> normalMatrix;
};

struct Camera {
  column_major matrix<float, 4, 4> projView; // 64 bytes
  float4                           pos;      // 16 bytes
  float4                           front;    // 16 bytes
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

struct PushConstants {
  uint64_t pVertexBufferAddress : VERTEX_BUFFER_ADDRESS; // 8 bytes
  uint     transformIndex;                               // 4 bytes;
  uint     materialIndex;                                // 4 bytes
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
StructuredBuffer<Material> gMaterials;

struct Vertex {
  float4 pos : POSITION;  // 16 bytes
  float4 normal : NORMAL; // 16 bytes
  float2 uv : UV;         // 8 bytes
};

struct VertexOutput {
  float4 pos : SV_Position;     // 16 bytes
  float4 world_pos : WORLD_POS; // 16 bytes
  float4 normal : NORMAL;       // 16 bytes
  float2 uv : UV;               // 8 bytes
};

[shader("vertex")]
VertexOutput vertMain(uint id: SV_VertexID, uniform PushConstants pc) {
  Vertex    v = ((Vertex *)pc.pVertexBufferAddress)[id];
  Transform t = gTransforms[pc.transformIndex];

  VertexOutput output;
  output.pos       = mul(mul(gCamera.projView, t.modelMatrix), v.pos);
  output.world_pos = mul(t.modelMatrix, v.pos);
  output.normal    = float4(mul((column_major float3x3)t.normalMatrix, v.normal.xyz), 0.f);
  output.uv        = v.uv;
  return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput input, uniform PushConstants pc) : SV_Target {
  Material material = gMaterials[pc.materialIndex];
  return gTextures[material.baseColorTexture].Sample(gSamplers[0], input.uv) * material.baseColorFactor;
}
