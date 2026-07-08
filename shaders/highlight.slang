struct Transform {
  column_major matrix<float, 4, 4> modelMatrix;
  column_major matrix<float, 4, 4> normalMatrix;
};

struct Camera {
  column_major matrix<float, 4, 4> projView; // 64 bytes
  float4                           pos;      // 16 bytes
  float4                           front;    // 16 bytes
};

struct PushConstants {
  uint64_t pVertexBufferAddress : VERTEX_BUFFER_ADDRESS; // 8 bytes
  uint     transformIndex;                               // 4 bytes
  float4   color;
  float    scale;
};

struct Vertex {
  float4 pos : POSITION;  // 16 bytes
  float4 normal : NORMAL; // 16 bytes
  float2 uv : UV;         // 8 bytes
};

[[vk::binding(0, 0)]]
Texture2D gTextures[];
[[vk::binding(1, 0)]]
SamplerState gSamplers[];

[[vk::binding(0, 1)]]
StructuredBuffer<Transform> gTransforms;
[[vk::binding(1, 1)]]
ConstantBuffer<Camera> gCamera;

[shader("vertex")]
float4 vertMain(uint id: SV_VertexID, uniform PushConstants pc) : SV_Position {
  Vertex v   = ((Vertex *)pc.pVertexBufferAddress)[id];
  float4 pos = v.pos;
  pos.xyz *= pc.scale;
  return mul(mul(gCamera.projView, gTransforms[pc.transformIndex].modelMatrix), pos);
}

[shader("fragment")]
float4 fragStencil(uniform PushConstants pc) : SV_Target { return pc.color; }
