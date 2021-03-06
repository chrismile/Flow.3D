cbuffer PerFrame
{
	float4x4 g_mWorldViewProj;
	float3 g_vCenter;
	float3 g_vTangent;
	float3 g_vBitangent;
	float2 g_vSize;
	float g_fAlpha;
}

Texture2D g_tex;


RasterizerState CullNone
{
	CullMode = None;
};

DepthStencilState DepthDisable
{
	DepthEnable = false;
	DepthWriteMask = 0;
};

DepthStencilState DepthDefault
{
};

BlendState BlendDisable
{
};

BlendState BlendAlpha
{
	AlphaToCoverageEnable = FALSE;
	BlendEnable[0] = TRUE;
	SrcBlend = SRC_ALPHA;
	DestBlend = INV_SRC_ALPHA;
	BlendOp = ADD;
	SrcBlendAlpha = ONE;
	DestBlendAlpha = ONE;
	BlendOpAlpha = ADD;
	RenderTargetWriteMask[0] = 0x0F;
};

SamplerState SamplerLinear
{
	Filter         = MIN_MAG_MIP_LINEAR;
	AddressU       = Clamp;
	AddressV       = Clamp;
};



void vsQuad(uint index : SV_VertexID, out float4 outPos : SV_Position, out float2 outTexCoord : TEXCOORD)
{
	switch(index) {
		case 0:
			outPos = mul(g_mWorldViewProj, float4(g_vCenter - 0.5*g_vTangent*g_vSize.x - 0.5*g_vBitangent*g_vSize.y, 1.0f));
			outTexCoord = float2(0, 1);
			break;
		case 1:
			outPos = mul(g_mWorldViewProj, float4(g_vCenter - 0.5*g_vTangent*g_vSize.x + 0.5*g_vBitangent*g_vSize.y, 1.0f));
			outTexCoord = float2(0, 0);
			break;
		case 2:
			outPos = mul(g_mWorldViewProj, float4(g_vCenter + 0.5*g_vTangent*g_vSize.x - 0.5*g_vBitangent*g_vSize.y, 1.0f));
			outTexCoord = float2(1, 1);
			break;
		case 3:
		default:
			outPos = mul(g_mWorldViewProj, float4(g_vCenter + 0.5*g_vTangent*g_vSize.x + 0.5*g_vBitangent*g_vSize.y, 1.0f));
			outTexCoord = float2(1, 0);
			break;
	}
}

float4 psQuad(float4 pos : SV_Position, float2 texCoord : TEXCOORD) : SV_Target
{
	//return float4(1, 0, 0, 1);
	float4 col = g_tex.Sample(SamplerLinear, texCoord);
	return float4(col.rgb, col.a * g_fAlpha);
}


technique11 tQuad
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, vsQuad()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psQuad()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDisable, 0);
		SetBlendState(BlendAlpha, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}
	pass P1
	{
		SetVertexShader(CompileShader(vs_5_0, vsQuad()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, psQuad()));
		SetRasterizerState(CullNone);
		SetDepthStencilState(DepthDefault, 0);
		SetBlendState(BlendAlpha, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}
}
