struct Vertex {
  float4 mPos : POSITION;
  float4 mNormal : NORMAL;
  float2 mTex : TEXTURE_COORDINATES;
};

struct VertexOutput {
  float4 mPos : SV_Position;
  float4 mNormal : NORMAL;
  float2 mTex : TEXTURE_COORDINATES;
};

struct PushConstants {
  column_major matrix<float, 4, 4> mModel : MODEL_MATRIX;             // 64 bytes
  column_major matrix<float, 4, 4> mTransformedModel : NORMAL_MATRIX; // 64 bytes
  uint64_t                         pVertexBufferAddress : VERTEX_BUFFER_ADDRESS;
};

struct Camera {
  column_major matrix<float, 4, 4> mProjView; // 64 bytes
  float4                           mPos;      // 16 bytes
  float4                           mFront;    // 16 bytes
};

[[vk::binding(0, 0)]]
ConstantBuffer<Camera> gCamera;

[shader("vertex")]
VertexOutput vertMain(uint id: SV_VertexID, uniform PushConstants pc) {
  Vertex v = ((Vertex *)pc.pVertexBufferAddress)[id];

  VertexOutput output;
  output.mPos    = mul(mul(gCamera.mProjView, pc.mModel), v.mPos);
  output.mNormal = float4(mul((column_major float3x3)pc.mTransformedModel, v.mNormal.xyz), 0.f);
  output.mTex    = v.mTex;
  return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput input) : SV_Target { return input.mNormal; }
