#include "LineEffect.h"

#ifndef V
#define V(x)           { hr = (x); }
#endif
#ifndef V_RETURN
#define V_RETURN(x)    { hr = (x); if( FAILED(hr) ) { return hr; } }
#endif


HRESULT LineEffect::GetVariables()
{
	HRESULT hr;

	// *** Variables ***

	//V_RETURN( GetMatrixVariable("g_mWorldView", m_pmWorldViewVariable) );
	//V_RETURN( GetMatrixVariable("g_mProj", m_pmProjVariable) );
	V_RETURN( GetMatrixVariable("g_mWorldViewProj", m_pmWorldViewProjVariable) );
	V_RETURN( GetMatrixVariable("g_mWorldViewRotation", m_pmWorldViewRotation) );

	V_RETURN( GetVectorVariable("g_vLightPos", m_pvLightPosVariable) );

	V_RETURN( GetScalarVariable("g_fRibbonHalfWidth", m_pfRibbonHalfWidthVariable) );
	V_RETURN( GetScalarVariable("g_fTubeRadius", m_pfTubeRadiusVariable) );

	V_RETURN(GetScalarVariable("g_fParticleSize", m_pfParticleSizeVariable));
	V_RETURN(GetScalarVariable("g_fScreenAspectRatio", m_pfScreenAspectRatioVariable));
	V_RETURN( GetScalarVariable("g_fParticleTransparency", m_pfParticleTransparencyVariable) );
	V_RETURN(GetVectorVariable("g_vParticleClipPlane", m_pvParticleClipPlane));

	V_RETURN( GetScalarVariable("g_bTubeRadiusFromVelocity", m_pbTubeRadiusFromVelocityVariable) );
	V_RETURN( GetScalarVariable("g_fReferenceVelocity", m_pfReferenceVelocityVariable) );

	V_RETURN( GetScalarVariable("g_iColorMode", m_piColorMode) );
	V_RETURN( GetVectorVariable("g_vColor0", m_pvColor0Variable) );
	V_RETURN( GetVectorVariable("g_vColor1", m_pvColor1Variable) );
	V_RETURN( GetScalarVariable("g_fTimeMin", m_pfTimeMinVariable) );
	V_RETURN( GetScalarVariable("g_fTimeMax", m_pfTimeMaxVariable) );
	V_RETURN( GetVectorVariable("g_vHalfSizeWorld", m_pvHalfSizeWorldVariable) );

	V_RETURN( GetScalarVariable("g_bTimeStripes", m_pbTimeStripesVariable) );
	V_RETURN( GetScalarVariable("g_fTimeStripeLength", m_pfTimeStripeLengthVariable) );

	V_RETURN( GetScalarVariable("g_iMeasureMode", m_piMeasureMode) );
	V_RETURN( GetScalarVariable("g_fMeasureScale", m_pfMeasureScale) );
	V_RETURN( GetVectorVariable("g_vTfRange", m_pvTfRange) );

	V_RETURN( GetShaderResourceVariable("g_texColors", m_ptexColors) );
	V_RETURN( GetShaderResourceVariable("g_seedColors", m_pseedColors) );
	V_RETURN( GetShaderResourceVariable("g_transferFunction", m_ptransferFunction) );

	V_RETURN( GetVectorVariable("g_vBoxMin", m_pvBoxMinVariable) );
	V_RETURN( GetVectorVariable("g_vBoxSize", m_pvBoxSizeVariable) );
	V_RETURN( GetVectorVariable("g_vCamPos", m_pvCamPosVariable) );
	V_RETURN( GetVectorVariable("g_vCamRight", m_pvCamRightVariable) );
	V_RETURN( GetScalarVariable("g_fBallRadius", m_pfBallRadiusVariable) );

	// *** Techniques ***

	V_RETURN( GetTechnique("tLine", m_pTechnique) );
	V_RETURN( GetTechnique("tBall", m_pTechniqueBalls) );

	// *** Input Layouts ***

	// This one matches TracingCommons::LineVertex 
	//specialization for MAX_RECORDED_CELLS = 4
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TIME",     0, DXGI_FORMAT_R32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "VELOCITY", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "LINE_ID",  0, DXGI_FORMAT_R32_UINT,        0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "SEED_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "RECORDED_CELL_INDICES", 0, DXGI_FORMAT_R32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "RECORDED_CELL_INDICES", 1, DXGI_FORMAT_R32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "RECORDED_CELL_INDICES", 2, DXGI_FORMAT_R32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "RECORDED_CELL_INDICES", 3, DXGI_FORMAT_R32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TIME_IN_CELL", 0, DXGI_FORMAT_R32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TIME_IN_CELL", 1, DXGI_FORMAT_R32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TIME_IN_CELL", 2, DXGI_FORMAT_R32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TIME_IN_CELL", 3, DXGI_FORMAT_R32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "HEAT",     0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "HEAT_CURRENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "JACOBIAN", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "JACOBIAN", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "JACOBIAN", 2, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	V_RETURN( CreateInputLayout(m_pTechnique->GetPassByIndex(0), layout, sizeof(layout)/sizeof(layout[0]), m_pInputLayout) );

	D3D11_INPUT_ELEMENT_DESC layoutBalls[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	V_RETURN( CreateInputLayout(m_pTechniqueBalls->GetPassByIndex(0), layoutBalls, sizeof(layoutBalls)/sizeof(layoutBalls[0]), m_pInputLayoutBalls) );

	return S_OK;
}
