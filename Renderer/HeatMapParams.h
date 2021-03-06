#ifndef __TUM3D__HEATMAPPARAMS_H__
#define __TUM3D__HEATMAPPARAMS_H__

#include <global.h>

#include <ConfigFile.h>
#include <Vec.h>
#include <D3D11.h>

#include "ParticleTraceParams.h"
#include "HeatMapNormalizationMode.h"

class HeatMapParams
{
public:
	HeatMapParams();
	~HeatMapParams();

	bool m_enableRecording;
	bool m_enableRendering;
	bool m_autoReset;

	SeedTexture m_recordTexture;
	int32 m_renderedChannels[2];

	eHeatMapNormalizationMode  m_normalizationMode;
	float m_stepSize;
	float m_densityScale;
	float m_tfAlphaScale;
	float m_tfRangeMin;
	float m_tfRangeMax;
	ID3D11ShaderResourceView* m_pTransferFunction;

	bool m_isosurface;
	float m_isovalue;

	bool HasChangesForRetracing(const HeatMapParams& other, const ParticleTraceParams& traceParams);
	bool HasChangesForRedrawing(const HeatMapParams& other);
};


#endif