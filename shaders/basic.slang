// struct Camera {
//   matrix<float, 4, 4> mViewProj : VIEW_PROJ;
// };

struct Vertex {
  float4 mPos : POSITION;
  float4 mColor : COLOR;
  float2 mTex : TEXTURE_COORDINATES;
};

struct VertexOutput {
  float4 mPos : SV_Position;
  float4 mColor : COLOR;
  float2 mTex : TEXTURE_COORDINATES;
};

struct PushConstants {
  matrix<float, 4, 4> mProjViewModel : IZEY;
  uint64_t            pVertexBufferAddress : VERTEX_BUFFER_ADDRESS;
};

// [[vk::binding(0, 0)]]
// ConstantBuffer<Camera> gCamera;

[[vk::binding(0, 0)]]
Sampler2D gTexture;

[[vk::binding(1, 0)]]
Sampler2D gTexture2;

[shader("vertex")]
VertexOutput vertMain(uint id: SV_VertexID, uniform PushConstants pc) {
  Vertex v = ((Vertex *)pc.pVertexBufferAddress)[id];

  VertexOutput output;
  output.mPos   = mul(v.mPos, pc.mProjViewModel);
  output.mColor = v.mColor;
  output.mTex   = v.mTex;
  return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput input) : SV_Target { return lerp(gTexture.Sample(input.mTex), gTexture2.Sample(input.mTex), 0.7); }
