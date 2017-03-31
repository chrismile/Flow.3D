cbuffer PerFrame
{
	//float4x4 g_mWorldView;
	//float4x4 g_mProj;
	float4x4 g_mWorldViewProj;

	float3   g_vLightPos;

	float    g_fRibbonHalfWidth;
	float    g_fTubeRadius; //tube radius + particle size

	float    g_fParticleSize;
	float    g_fScreenAspectRatio;
	float    g_fParticleTransparency;
	float4   g_vParticleClipPlane;

	bool     g_bTubeRadiusFromVelocity;
	float    g_fReferenceVelocity;

	int      g_iColorMode;
	float4   g_vColor0;
	float4   g_vColor1;
	float    g_fTimeMin;
	float    g_fTimeMax;

	bool     g_bTimeStripes;
	float    g_fTimeStripeLength;
}

cbuffer Balls
{
	float3   g_vBoxMin;
	float3   g_vBoxSize;
	float3   g_vCamPos;
	float3   g_vCamRight;
	float    g_fBallRadius = 0.011718750051;
}

texture1D<float4> g_texColors;
texture2D<float4> g_seedColors;


RasterizerState CullFront
{
	CullMode = Front;
};

RasterizerState CullBack
{
	CullMode = Back;
};

RasterizerState CullNone
{
	CullMode = None;
};

DepthStencilState DepthDefault
{
};

DepthStencilState DepthDisable
{
	DepthEnable    = false;
	DepthWriteMask = 0;
};

DepthStencilState DepthEqual
{
	DepthEnable    = true;
	DepthWriteMask = 0;
	DepthFunc      = EQUAL;
};

BlendState BlendDisable
{
};

BlendState BlendBehind
{
	BlendEnable[0]    = true;
	SrcBlend[0]       = INV_DEST_ALPHA;
	SrcBlendAlpha[0]  = INV_DEST_ALPHA;
	DestBlend[0]      = ONE;
	DestBlendAlpha[0] = ONE;
};


// for Particles
// [
DepthStencilState dsNoDepth
{
	DepthEnable = true;
	DepthWriteMask = 0;
};

RasterizerState SpriteRS
{
	CullMode = None;
};

BlendState AlphaBlending
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

BlendState AdditionBlending
{
	AlphaToCoverageEnable = FALSE;
	BlendEnable[0] = TRUE;
	SrcBlend = ONE;
	DestBlend = ONE;
	BlendOp = ADD;
	SrcBlendAlpha = ONE;
	DestBlendAlpha = ONE;
	BlendOpAlpha = ADD;
	RenderTargetWriteMask[0] = 0x0F;
};

BlendState MultiplicationBlending
{
	AlphaToCoverageEnable = FALSE;
	BlendEnable[0] = TRUE;
	SrcBlend = DEST_COLOR;
	DestBlend = ZERO;
	BlendOp = ADD;
	SrcBlendAlpha = ONE;
	DestBlendAlpha = ONE;
	BlendOpAlpha = ADD;
	RenderTargetWriteMask[0] = 0x0F;
};

SamplerState SamplerLinear
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Clamp;
};

// ]


struct LineVertex
{
	float3   pos     : POSITION;
	float    time    : TIME;
	float3   normal  : NORMAL;
	float3   vel     : VELOCITY;
	uint     lineID  : LINE_ID;
	float3   seedPos : SEED_POSITION;
	//float3x3 jac     : JACOBIAN;
};

struct BallVertex
{
	float3   pos     : POSITION;
	//float    radius  : RADIUS;
};


float4 getColor(uint lineID, float time, float3 seedPos)
{
	float4 color;
	if(g_iColorMode == 1) {
		//color by age
		float timeFactor = saturate((time - g_fTimeMin) / (g_fTimeMax - g_fTimeMin));
		color = (1.0 - timeFactor) * g_vColor0 + timeFactor * g_vColor1;
	} else if (g_iColorMode == 0) {
		//color by line id
		color = g_texColors.Load(int2(lineID % 1024, 0));
	}
	else {
		//color by texture
		//assume xy-plane is in the bounds [-1, 1], y-flipped
		float2 texCoord = (seedPos.xy + float2(1, 1)) / 2;
		texCoord.y = 1 - texCoord.y;
		color = g_seedColors.Sample(SamplerLinear, texCoord);
	}

	if(g_bTimeStripes) {
		float segment = floor(time / g_fTimeStripeLength);
		float evenOdd = (segment % 2.0) * 2.0 - 1.0;
		color.rgb *= 1.0 + 0.1 * evenOdd;
		color.rgb += 0.25 * evenOdd;
	}

	return color;
}

float3 getVorticity(float3x3 jacobian)
{
	return float3(jacobian[2][1] - jacobian[1][2], jacobian[0][2] - jacobian[2][0], jacobian[1][0] - jacobian[0][1]);
}

struct LinePSIn
{
	float4 pos  : SV_Position;
	float  time : TIME;
	float3 seedPos : SEED_POS;
	nointerpolation uint lineID : LINE_ID;
};

void vsLine(LineVertex input, out LinePSIn output)
{
	output.seedPos = input.seedPos;
	output.lineID = input.lineID;
	output.time = input.time;
	output.pos = mul(g_mWorldViewProj, float4(input.pos, 1.0));
}

float4 psLine(LinePSIn input) : SV_Target
{
	return getColor(input.lineID, input.time, input.seedPos);
}



struct RibbonGSIn
{
	float3 pos  : POSITION;
	float  time : TIME;
	float3 vel  : VELOCITY;
	float3 vort : VORTICITY;
	float3 seedPos : SEED_POS;
	nointerpolation uint lineID : LINE_ID;
};

void vsRibbon(LineVertex input, out RibbonGSIn output)
{
	output.pos  = input.pos;
	output.time = input.time;
	output.vel  = input.vel;
	output.vort = float3(1.0, 0.0, 0.0); //getVorticity(input.jac);
	output.seedPos = input.seedPos;
	output.lineID = input.lineID;
}

struct RibbonPSIn
{
	float4 pos    : SV_Position;
	float3 posWorld   : POS_WORLD;
	float3 normal : NORMAL;
	float  time   : TIME;
	float3 seedPos : SEED_POS;
	nointerpolation uint lineID : LINE_ID;
};

[maxvertexcount(4)]
void gsExtrudeRibbon(in line RibbonGSIn input[2], inout TriangleStream<RibbonPSIn> stream)
{
	// displace in direction of vorticity, but orthogonalize wrt velocity
	float3 front0 = normalize(input[0].vel);
	float3 front1 = normalize(input[1].vel);

	float3 right0 = normalize(input[0].vort);
	float3 right1 = normalize(input[1].vort);

	float3 normal0 = cross(front0, right0);
	float3 normal1 = cross(front1, right1);

	float3 displace0 = g_fRibbonHalfWidth * cross(normal0, front0);
	float3 displace1 = g_fRibbonHalfWidth * cross(normal1, front1);


	RibbonPSIn output;
	output.seedPos = input[0].seedPos;
	output.lineID = input[0].lineID;

	output.normal = normalize(cross(input[0].vel, displace0));
	output.time = input[0].time;
	output.posWorld = input[0].pos - displace0;
	output.pos = mul(g_mWorldViewProj, float4(output.posWorld, 1.0));
	stream.Append(output);
	output.posWorld = input[0].pos + displace0;
	output.pos = mul(g_mWorldViewProj, float4(output.posWorld, 1.0));
	stream.Append(output);

	output.normal = normalize(cross(input[1].vel, displace1));
	output.time = input[1].time;
	output.posWorld = input[1].pos - displace1;
	output.pos = mul(g_mWorldViewProj, float4(output.posWorld, 1.0));
	stream.Append(output);
	output.posWorld = input[1].pos + displace1;
	output.pos = mul(g_mWorldViewProj, float4(output.posWorld, 1.0));
	stream.Append(output);
}

float4 psRibbon(float4 pos : SV_Position, RibbonPSIn input) : SV_Target
{
	float3 lightDir = normalize(g_vLightPos - input.posWorld);
	float4 color = getColor(input.lineID, input.time, input.seedPos);
	float diffuse = abs(dot(lightDir, normalize(input.normal)));
	return float4(diffuse * color.rgb, color.a);
}



struct TubeGSIn
{
	float3 pos    : POSITION;
	float  time   : TIME;
	float3 normal : NORMAL;
	float3 vel    : VELOCITY;
	float3 vort   : VORTICITY;
	nointerpolation uint lineID : LINE_ID;
	float3 seedPos : SEED_POS;
};

void vsTube(LineVertex input, out TubeGSIn output)
{
	output.pos    = input.pos;
	output.time   = input.time;
	output.normal = input.normal;
	output.vel    = input.vel;
	output.vort   = float3(1.0, 0.0, 0.0); //getVorticity(input.jac);
	output.lineID = input.lineID;
	output.seedPos = input.seedPos;
}

struct TubePSIn
{
	float4 pos        : SV_Position;
	float3 posWorld   : POS_WORLD;
	float3 tubeCenter : TUBE_CENTER;
	float  time       : TIME;
	float3 normal     : NORMAL;
	nointerpolation uint lineID : LINE_ID;
	float3 seedPos : SEED_POS;
};

#define TUBE_SEGMENT_COUNT 16
//[maxvertexcount(2*(TUBE_SEGMENT_COUNT+1)]
[maxvertexcount(34)]
void gsExtrudeTube(in line TubeGSIn input[2], inout TriangleStream<TubePSIn> stream)
{
	float3 tangent0 = normalize(input[0].vel);
	float3 tangent1 = normalize(input[1].vel);

	float3 normal0 = input[0].normal;
	float3 normal1 = input[1].normal;

	float3 binormal0 = cross(tangent0, normal0);
	float3 binormal1 = cross(tangent1, normal1);


	float radius0 = g_fTubeRadius;
	float radius1 = g_fTubeRadius;
	if(g_bTubeRadiusFromVelocity) {
		radius0 *= clamp(sqrt(g_fReferenceVelocity / length(input[0].vel)), 0.2, 3.0);
		radius1 *= clamp(sqrt(g_fReferenceVelocity / length(input[1].vel)), 0.2, 3.0);
	}

	TubePSIn output;
	output.lineID = input[0].lineID;
	output.seedPos = input[0].seedPos;

	output.tubeCenter = input[1].pos;
	output.normal = normal1;
	output.posWorld = output.tubeCenter + radius1 * output.normal;
	output.pos = mul(g_mWorldViewProj, float4(output.posWorld, 1.0));
	output.time = input[1].time;
	stream.Append(output);

	output.tubeCenter = input[0].pos;
	output.normal = normal0;
	output.posWorld = output.tubeCenter + radius0 * output.normal;
	output.pos = mul(g_mWorldViewProj, float4(output.posWorld, 1.0));
	output.time = input[0].time;
	stream.Append(output);

	for(int i = 1; i < TUBE_SEGMENT_COUNT; i++) {
		//TODO sin/cos lookup table?
		float t = float(i) / float(TUBE_SEGMENT_COUNT);
		float angle = radians(t * 360.0);
		float s,c;
		sincos(angle, s, c);

		output.tubeCenter = input[1].pos;
		output.normal = c * normal1 + s * binormal1;
		output.posWorld = output.tubeCenter + radius1 * output.normal;
		output.pos = mul(g_mWorldViewProj, float4(output.posWorld, 1.0));
		output.time = input[1].time;
		stream.Append(output);

		output.tubeCenter = input[0].pos;
		output.normal = c * normal0 + s * binormal0;
		output.posWorld = output.tubeCenter + radius0 * output.normal;
		output.pos = mul(g_mWorldViewProj, float4(output.posWorld, 1.0));
		output.time = input[0].time;
		stream.Append(output);
	}

	output.tubeCenter = input[1].pos;
	output.normal = normal1;
	output.posWorld = output.tubeCenter + radius1 * output.normal;
	output.pos = mul(g_mWorldViewProj, float4(output.posWorld, 1.0));
	output.time = input[1].time;
	stream.Append(output);

	output.tubeCenter = input[0].pos;
	output.normal = normal0;
	output.posWorld = output.tubeCenter + radius0 * output.normal;
	output.pos = mul(g_mWorldViewProj, float4(output.posWorld, 1.0));
	output.time = input[0].time;
	stream.Append(output);
}

float4 psTube(TubePSIn input) : SV_Target
{
	float3 lightDir = normalize(g_vLightPos - input.posWorld);
	float4 color = getColor(input.lineID, input.time, input.seedPos);
	float diffuse = saturate(dot(lightDir, normalize(input.normal)));
	return float4(diffuse * color.rgb, color.a);
}


void vsSphere(BallVertex input, out float3 outPos : POSITION)
{
	outPos = g_vBoxMin + input.pos * g_vBoxSize;
}

struct SphereGSIn
{
	float3 pos      : POSITION;
};

struct SpherePSIn
{
	float4 pos      : SV_Position;
	float3 posWorld : POS_WORLD;
	float3 center   : CENTER;
};

[maxvertexcount(4)]
void gsExtrudeSphereQuad(in point SphereGSIn input[1], inout TriangleStream<SpherePSIn> stream)
{
	float3 view = normalize(input[0].pos - g_vCamPos);
	float3 up = normalize(cross(view, g_vCamRight));
	float3 right = cross(up, view);

	SpherePSIn output;

	output.center = input[0].pos;

	float3 centerFront = output.center - g_fBallRadius * view;
	float3 vec0 = g_fBallRadius * up;
	float3 vec1 = g_fBallRadius * right;

	output.posWorld = centerFront + vec0 + vec1;
	output.pos = mul(g_mWorldViewProj, float4(output.posWorld, 1.0));
	stream.Append(output);

	output.posWorld = centerFront + vec0 - vec1;
	output.pos = mul(g_mWorldViewProj, float4(output.posWorld, 1.0));
	stream.Append(output);

	output.posWorld = centerFront - vec0 + vec1;
	output.pos = mul(g_mWorldViewProj, float4(output.posWorld, 1.0));
	stream.Append(output);

	output.posWorld = centerFront - vec0 - vec1;
	output.pos = mul(g_mWorldViewProj, float4(output.posWorld, 1.0));
	stream.Append(output);
}

float4 psSphere(SpherePSIn input, out float depth : SV_Depth) : SV_Target
{
	float3 dir = normalize(input.posWorld - g_vCamPos);
	float3 c2p = input.posWorld - input.center;
	float a = dot(dir, dir);
	float b = 2.0 * dot(dir, c2p);
	float c = dot(c2p, c2p) - g_fBallRadius * g_fBallRadius;
	float d = b * b - 4.0 * a * c;
	if(d <= 0.0) discard;

	float t = (-b - sqrt(d)) / (2.0 * a);
	float3 intersection = input.posWorld + dir * t;

	float4 projected = mul(g_mWorldViewProj, float4(intersection, 1.0));
	depth = projected.z / projected.w;

	float3 normal = (intersection - input.center) / g_fBallRadius;
	float3 lightDir = normalize(g_vLightPos - intersection);
	float diffuse = saturate(dot(lightDir, normal));
	return float4((0.3 + 0.7 * diffuse) * g_vColor0.rgb, g_vColor0.a);
}



struct ParticleGSIn
{
	float3 pos    : POSITION;
	float  time : TIME;
	float3 normal : NORMAL;
	float3 vel    : VELOCITY;
	float3 vort   : VORTICITY;
	float3 seedPos    : SEED_POSITION;
	nointerpolation uint lineID : LINE_ID;
};

struct ParticlePSIn
{
	float4 pos        : SV_Position;
	float2 tex        : TEXTURE;
	float3 posWorld   : POS_WORLD;
	float3 tubeCenter : TUBE_CENTER;
	float  time       : TIME;
	float3 normal     : NORMAL;
	float3 seedPos    : SEED_POSITION;
	nointerpolation uint lineID : LINE_ID;
};

void vsParticle(LineVertex input, out ParticleGSIn output)
{
	output.pos = input.pos;
	output.time = input.time;
	output.normal = input.normal;
	output.vel = input.vel;
	output.vort = float3(1.0, 0.0, 0.0); //getVorticity(input.jac);
	output.lineID = input.lineID;
	output.seedPos = input.seedPos;
}

[maxvertexcount(6)]
void gsParticle(in point ParticleGSIn input[1], inout TriangleStream<ParticlePSIn> outStream)
{
	if (input[0].time < 0) return; //discard particle, it is invalid / out of bounds

	//clipping
	float clipDistance = dot(g_vParticleClipPlane.xyz, input[0].pos) + g_vParticleClipPlane.w;
	if (clipDistance < 0) return;

	ParticlePSIn o;
	o.time = input[0].time;
	o.tubeCenter = input[0].pos;
	o.posWorld = o.tubeCenter;
	o.normal = input[0].normal;
	o.seedPos = input[0].seedPos;
	o.lineID = input[0].lineID;
	float4 pos = mul(g_mWorldViewProj, float4(input[0].pos, 1.0));

	float size = sqrt(1 - saturate((o.time - g_fTimeMin) / (g_fTimeMax - g_fTimeMin)))
		* g_fParticleSize * 5;
	//	* g_fTubeRadius * 5;
	float spriteSizeW = size;
	float spriteSizeH = size * g_fScreenAspectRatio;

	float2 xTransform = float2(1, 0);
	float2 yTransform = float2(0, 1);
	if (g_bTubeRadiusFromVelocity) {
		float3 inVel = input[0].vel;
		float4 vel = mul(g_mWorldViewProj, float4(inVel, 0.0));
		float shear = 1 + min(sqrt(length(vel.xy) / g_fReferenceVelocity), 2.0);
		float sizeZ = 1 + min(sqrt(abs(vel.z) / g_fReferenceVelocity), 2.0);
		spriteSizeW = spriteSizeW * shear / sizeZ;
		spriteSizeH = spriteSizeH / shear / sizeZ;
		float2 dir = normalize(vel.xy);
		xTransform = float2(dir.x, -dir.y);
		yTransform = float2(dir.y, dir.x);
		//radius0 *= clamp(sqrt(g_fReferenceVelocity / length(input[0].vel)), 0.2, 3.0);
		//radius1 *= clamp(sqrt(g_fReferenceVelocity / length(input[1].vel)), 0.2, 3.0);
	}
	float2 offset;

	offset = float2(-spriteSizeW, spriteSizeH);
	o.pos = float4(pos.x + dot(xTransform, offset), pos.y + dot(yTransform, offset), pos.z, pos.w);
	o.tex = float2(0, 1);
	outStream.Append(o);
	offset = float2(-spriteSizeW, -spriteSizeH);
	o.pos = float4(pos.x + dot(xTransform, offset), pos.y + dot(yTransform, offset), pos.z, pos.w);
	o.tex = float2(0, 0);
	outStream.Append(o);
	offset = float2(spriteSizeW, spriteSizeH);
	o.pos = float4(pos.x + dot(xTransform, offset), pos.y + dot(yTransform, offset), pos.z, pos.w);
	o.tex = float2(1, 1);
	outStream.Append(o);
	outStream.RestartStrip();

	offset = float2(spriteSizeW, spriteSizeH);
	o.pos = float4(pos.x + dot(xTransform, offset), pos.y + dot(yTransform, offset), pos.z, pos.w);
	o.tex = float2(1, 1);
	outStream.Append(o);
	offset = float2(-spriteSizeW, -spriteSizeH);
	o.pos = float4(pos.x + dot(xTransform, offset), pos.y + dot(yTransform, offset), pos.z, pos.w);
	o.tex = float2(0, 0);
	outStream.Append(o);
	offset = float2(spriteSizeW, -spriteSizeH);
	o.pos = float4(pos.x + dot(xTransform, offset), pos.y + dot(yTransform, offset), pos.z, pos.w);
	o.tex = float2(1, 0);
	outStream.Append(o);
	outStream.RestartStrip();
}

float4 psParticleAdditive(ParticlePSIn input) : SV_Target
{
	float4 color = getColor(input.lineID, input.time, input.seedPos);

	float dist = length(input.tex.xy - float2 (0.5f, 0.5f)) * 2;
	float alpha = g_fParticleTransparency; //smoothstep(0, 0.3, 1 - dist);
	if (dist > 0.5) discard;

	return float4(color.rgb * alpha * color.a, color.a * alpha);
}

float4 psParticleMultiplicative(ParticlePSIn input) : SV_Target
{
	float4 color = getColor(input.lineID, input.time, input.seedPos);

	float dist = length(input.tex.xy - float2 (0.5f, 0.5f)) * 2;
	float alpha = g_fParticleTransparency; //smoothstep(0, 0.3, 1 - dist);
	if (dist > 0.5) discard;

	//return float4(float3(1,1,1) - (color.rgb * alpha * color.a), color.a * alpha);
	return float4(color.rgb * (1 - (alpha * color.a)), color.a * alpha);
}

float4 psParticleAlpha(ParticlePSIn input, uniform bool colorFromTexture) : SV_Target
{
	float4 color;
	color = getColor(input.lineID, input.time, input.seedPos);

	float dist = length(input.tex.xy - float2 (0.5f, 0.5f)) * 2;
	float alpha = g_fParticleTransparency; //smoothstep(0, 0.3, 1 - dist);
	if (dist > 0.5) discard;

	return float4(color.rgb, color.a * alpha);
}


technique11 tLine
{
	pass P0_Line
	{
		SetVertexShader( CompileShader( vs_5_0, vsLine() ) );
		SetGeometryShader( NULL );
		SetPixelShader( CompileShader( ps_5_0, psLine() ) );
		SetRasterizerState( CullNone );
		SetDepthStencilState( DepthDefault, 0 );
		SetBlendState( BlendDisable, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
	}

	pass P1_LineBehind
	{
		SetVertexShader( CompileShader( vs_5_0, vsLine() ) );
		SetGeometryShader( NULL );
		SetPixelShader( CompileShader( ps_5_0, psLine() ) );
		SetRasterizerState( CullNone );
		SetDepthStencilState( DepthEqual, 0 );
		SetBlendState( BlendBehind, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
	}

	pass P2_Ribbon
	{
		SetVertexShader( CompileShader( vs_5_0, vsRibbon() ) );
		SetGeometryShader( CompileShader( gs_5_0, gsExtrudeRibbon() ) );
		SetPixelShader( CompileShader( ps_5_0, psRibbon() ) );
		SetRasterizerState( CullNone );
		SetDepthStencilState( DepthDefault, 0 );
		SetBlendState( BlendDisable, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
	}

	pass P3_RibbonBehind
	{
		SetVertexShader( CompileShader( vs_5_0, vsRibbon() ) );
		SetGeometryShader( CompileShader( gs_5_0, gsExtrudeRibbon() ) );
		SetPixelShader( CompileShader( ps_5_0, psRibbon() ) );
		SetRasterizerState( CullNone );
		SetDepthStencilState( DepthEqual, 0 );
		SetBlendState( BlendBehind, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
	}

	pass P4_Tube
	{
		SetVertexShader( CompileShader( vs_5_0, vsTube() ) );
		SetGeometryShader( CompileShader( gs_5_0, gsExtrudeTube() ) );
		SetPixelShader( CompileShader( ps_5_0, psTube() ) );
		SetRasterizerState( CullNone );
		SetDepthStencilState( DepthDefault, 0 );
		SetBlendState( BlendDisable, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
	}

	pass P5_TubeBehind
	{
		SetVertexShader( CompileShader( vs_5_0, vsTube() ) );
		SetGeometryShader( CompileShader( gs_5_0, gsExtrudeTube() ) );
		SetPixelShader( CompileShader( ps_5_0, psTube() ) );
		SetRasterizerState( CullNone );
		SetDepthStencilState( DepthEqual, 0 );
		SetBlendState( BlendBehind, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
	}

	pass P6_Particle
	{
		SetVertexShader(CompileShader(vs_5_0, vsParticle()));
		SetGeometryShader(CompileShader(gs_5_0, gsParticle()));
		SetPixelShader(CompileShader(ps_5_0, psParticleAdditive()));
		SetRasterizerState(SpriteRS);
		SetDepthStencilState(dsNoDepth, 0);
		SetBlendState(AdditionBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
	}
	
	pass P7_ParticleMult
	{
		SetVertexShader(CompileShader(vs_5_0, vsParticle()));
		SetGeometryShader(CompileShader(gs_5_0, gsParticle()));
		SetPixelShader(CompileShader(ps_5_0, psParticleMultiplicative()));
		SetRasterizerState(SpriteRS);
		SetDepthStencilState(dsNoDepth, 0);
		SetBlendState(MultiplicationBlending, float4(1.0f, 1.0f, 1.0f, 0.0f), 0xFFFFFFFF);
	}

	pass P8_ParticleAlpha
	{
		SetVertexShader(CompileShader(vs_5_0, vsParticle()));
		SetGeometryShader(CompileShader(gs_5_0, gsParticle()));
		SetPixelShader(CompileShader(ps_5_0, psParticleAlpha(false)));
		SetRasterizerState(SpriteRS);
		SetDepthStencilState(dsNoDepth, 0);
		SetBlendState(AlphaBlending, float4(1.0f, 1.0f, 1.0f, 0.0f), 0xFFFFFFFF);
	}
}


technique11 tBall
{
	pass P0_Ball
	{
		SetVertexShader( CompileShader( vs_5_0, vsSphere() ) );
		SetGeometryShader( CompileShader( gs_5_0, gsExtrudeSphereQuad() ) );
		SetPixelShader( CompileShader( ps_5_0, psSphere() ) );
		SetRasterizerState( CullNone );
		SetDepthStencilState( DepthDefault, 0 );
		SetBlendState( BlendDisable, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
	}
}
