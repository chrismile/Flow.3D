#ifndef __TUM3D__MEASURE_H__
#define __TUM3D__MEASURE_H__


#include <global.h>

#include <string>


enum eMeasure
{
	MEASURE_VELOCITY=0,
	MEASURE_VELOCITY_Z,
	MEASURE_VORTICITY,
	MEASURE_LAMBDA2,
	MEASURE_QHUNT,
	MEASURE_DELTACHONG,
	MEASURE_ENSTROPHY_PRODUCTION,
	MEASURE_STRAIN_PRODUCTION,
	MEASURE_SQUARE_ROTATION,
	MEASURE_SQUARE_RATE_OF_STRAIN,
	MEASURE_TRACE_JJT,
	MEASURE_PVA,
	MEASURE_COUNT,
	MEASURE_FORCE32 =  0xFFFFFFFF
};
std::string GetMeasureName(eMeasure mode);
eMeasure GetMeasureFromName(const std::string& name);
float GetDefaultMeasureScale(eMeasure mode);
float GetDefaultMeasureQuantStep(eMeasure mode);
bool MeasureIsFromJacobian(eMeasure mode);


enum eMeasureComputeMode
{
	MEASURE_COMPUTE_ONTHEFLY=0,
	MEASURE_COMPUTE_PRECOMP_DISCARD,
	MEASURE_COMPUTE_PRECOMP_STORE_GPU,
	//? MEASURE_COMPUTE_PRECOMP_STORE_CPU,
	MEASURE_COMPUTE_PRECOMP_COMPRESS,
	MEASURE_COMPUTE_COUNT,
	MEASURE_COMPUTE_FORCE32 = 0xFFFFFFFF
};
std::string GetMeasureComputeModeName(eMeasureComputeMode mode);
eMeasureComputeMode GetMeasureComputeModeFromName(const std::string& name);


#endif
