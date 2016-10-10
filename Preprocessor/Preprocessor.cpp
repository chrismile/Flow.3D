#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iterator>
#include <conio.h>

#include <tclap/CmdLine.h>
#include <cudaCompress/Init.h>
#include <cudaUtil.h>

#include "Utils.h"
#include "SysTools.h"
#include "LargeArray3D.h"
#include "Statistics.h"

#include "TimeVolumeIO.h"

#include "CompressVolume.h"
#include "GPUResources.h"

using namespace std;


void e()
{
	system("pause");
}


void WriteStatsHeaderCSV(std::ostream& stream)
{
	stream << "ElemCount;OriginalSize;CompressedSize;BitsPerElem;CompressionRate;"
		<< "Min;Max;Range;Average;Variance;"
		<< "ReconstMin;ReconstMax;ReconstRange;ReconstAverage;ReconstVariance;"
		<< "QuantStep;AvgAbsError;MaxAbsError;AvgRelError;MaxRelError;RelErrorCount;"
		<< "RMSError;SNR;PSNR" << std::endl;
}

void WriteStatsCSV(std::ostream& stream, const Statistics::Stats& stats, float quantStep)
{
	stream << stats.ElemCount << ";" << stats.OriginalSize << ";" << stats.CompressedSize << ";" << stats.BitsPerElem << ";" << stats.CompressionRate << ";"
		<< stats.Min << ";" << stats.Max << ";" << stats.Range << ";" << stats.Average << ";" << stats.Variance << ";"
		<< stats.ReconstMin << ";" << stats.ReconstMax << ";" << stats.ReconstRange << ";" << stats.ReconstAverage << ";" << stats.ReconstVariance << ";"
		<< quantStep << ";" << stats.AvgAbsError << ";" << stats.MaxAbsError << ";" << stats.AvgRelError << ";" << stats.MaxRelError << ";" << stats.RelErrorCount << ";"
		<< stats.RMSError << ";" << stats.SNR << ";" << stats.PSNR << std::endl;
}


int main(int argc, char* argv[])
{
	#if defined(DEBUG) | defined(_DEBUG)
		_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
	#endif

	struct FileListItem
	{
		string fileMask;
		int channels;
		LA3D::LargeArray3D<float>* tmpArray;
	};

	//atexit(&e);

	// **** Parse command line ****

	vector<FileListItem> fileList;
	string outFile;
	string inPath;
	string outPath;
	string tmpPath;
	bool overwrite;
	int32 tMin, tMax, tStep, tOffset;
	Vec3i volumeSize;
	int channels = 0;
	bool periodic;
	float gridSpacing;
	float timeSpacing;
	int32 brickSize;
	int32 overlap;

	eCompressionType compression = COMPRESSION_NONE;
	string quantStepsString;
	vector<float> quantSteps;
	bool lessRLE;
	uint huffmanBits;

	bool autoBuild;
	bool keepLA3Ds;



	try
	{
		TCLAP::CmdLine cmd("", ' ');

		TCLAP::UnlabeledValueArg<string> outFileArg("outfile", "Output file name without extension (index file will be <filename>.timevol)", true, "", "String", cmd);
		TCLAP::UnlabeledMultiArg<string> inMaskArg("inmask", "Any number of input files in the format <filename>:<channels>, where <filename>"
			" must contain a decimal-formator like %d that will be replaced by the timestep index. For example, \"testdata_256_%d.raw:3\" adds"
			" a 3-channel-file.", true, "String", cmd);

		TCLAP::ValueArg<string> outPathArg("O", "outpath", "Output path", false, "", "String", cmd);
		TCLAP::ValueArg<string> inPathArg("I", "inpath", "Input path", false, "", "String", cmd);
		TCLAP::ValueArg<string> tmpPathArg("", "tmp", "Path for temporary la3d files; default outpath", false, "", "String", cmd);
		TCLAP::SwitchArg keepLA3DsArg("", "keepla3ds", "Don't delete temporary .la3d files after finishing", cmd);
		TCLAP::SwitchArg overwriteArg("", "overwrite", "Force overwriting of existing output files (default is trying to append)", cmd);

		TCLAP::ValueArg<int32> tMinArg("", "tmin", "First (inclusive) timestep to include", false, 0, "Integer", cmd);
		TCLAP::ValueArg<int32> tMaxArg("", "tmax", "Last (inclusive) timestep to include", false, 0, "Integer", cmd);
		TCLAP::ValueArg<int32> tStepArg("", "tstep", "Timestep increment", false, 1, "Integer", cmd);
		TCLAP::ValueArg<int32> tOffsetArg("", "toffset", "Index of the first timestep to write (for appending!)", false, 0, "Integer", cmd);

		TCLAP::ValueArg<int32> volumeSizeXArg("x", "volumesizex", "X dimension of the volume", true, 1024, "Integer", cmd);
		TCLAP::ValueArg<int32> volumeSizeYArg("y", "volumesizey", "Y dimension of the volume", true, 1024, "Integer", cmd);
		TCLAP::ValueArg<int32> volumeSizeZArg("z", "volumesizez", "Z dimension of the volume", true, 1024, "Integer", cmd);
		TCLAP::SwitchArg periodicArg("", "periodic", "Use periodic boundary, i.e. wrap (default is clamp)", cmd);
		TCLAP::ValueArg<float> gridSpacingArg("g", "gridspacing", "Distance between grid points", false, 1.0f, "Float", cmd);
		TCLAP::ValueArg<float> timeSpacingArg("t", "timespacing", "Distance between time steps", false, 1.0f, "Float", cmd);
		TCLAP::ValueArg<int32> brickSizeArg("b", "bricksize", "Brick size including overlap", false, 256, "Integer", cmd);
		TCLAP::ValueArg<int32> overlapArg("v", "overlap", "Brick overlap (size of halo)", false, 4, "Integer", cmd);

		TCLAP::ValueArg<string> quantStepsArg("q", "quantstep", "Enable compression with fixed quantization steps [%f %f ...]", false, "", "Float Array", cmd);
		TCLAP::SwitchArg quantFirstArg("", "quantfirst", "Perform quantization before DWT", cmd);
		TCLAP::SwitchArg lessRLEArg("", "lessrle", "Do RLE only on finest level (improves compression with fine to moderate quant steps)", cmd);
		TCLAP::ValueArg<int32> huffmanBitsArg("", "huffmanbits", "Max number of bits per symbol for Huffman coding", false, 0, "Integer", cmd);

		TCLAP::SwitchArg autoBuildArg("a", "autobuild", "Suppress all confirmation messages", cmd, false);


		cmd.parse(argc, argv);

		const vector<string>& inMask = inMaskArg.getValue();
		outFile = outFileArg.getValue();

		outPath = outPathArg.getValue();
		inPath = inPathArg.getValue();
		tmpPath = tmpPathArg.getValue();
		keepLA3Ds = keepLA3DsArg.getValue();
		overwrite = overwriteArg.getValue();

		tMin = tMinArg.getValue();
		tMax = tMaxArg.getValue();
		tStep = tStepArg.getValue();
		tOffset = tOffsetArg.getValue();

		volumeSize[0] = volumeSizeXArg.getValue();
		volumeSize[1] = volumeSizeYArg.getValue();
		volumeSize[2] = volumeSizeZArg.getValue();
		periodic = periodicArg.getValue();
		gridSpacing = gridSpacingArg.isSet() ? gridSpacingArg.getValue() : 2.0f / float(volumeSize.maximum());
		timeSpacing = timeSpacingArg.getValue();
		brickSize = brickSizeArg.getValue();
		overlap = overlapArg.getValue();

		if(quantStepsArg.isSet())
		{
			compression = quantFirstArg.isSet() ? COMPRESSION_FIXEDQUANT_QF : COMPRESSION_FIXEDQUANT;
			quantStepsString = quantStepsArg.getValue();
		}
		lessRLE = lessRLEArg.getValue();
		huffmanBits = huffmanBitsArg.getValue();

		autoBuild = autoBuildArg.getValue();


		// build input file descriptors from input file mask
		E_ASSERT("No input files given", inMask.size() > 0);
		for (auto it = inMask.cbegin(); it != inMask.cend(); ++it)
		{
			char tmp[1024];
			FileListItem desc;
			E_ASSERT("Invalid input file desc: " << *it, sscanf_s(it->c_str(), "%[^:]:%d", tmp, 1024, &desc.channels) == 2);
			desc.fileMask = tmp;
			desc.tmpArray = new LA3D::LargeArray3D<float>(desc.channels);

			fileList.push_back(desc);
			channels += desc.channels;
		}
	}
	catch (TCLAP::ArgException &e)
	{
		cout << "*** ERROR: " << e.error() << " for arg " << e.argId() << "\n";
		return -1;
	}

	// Sanity checks
	if (outPath != "")
	{
		outPath += "\\";
	}
	if (inPath != "")
	{
		inPath += "\\";
	}
	if (tmpPath == "")
	{
		tmpPath = outPath;
	}
	else
	{
		tmpPath += "\\";
	}

	if(compression == COMPRESSION_FIXEDQUANT || compression == COMPRESSION_FIXEDQUANT_QF)
	{
		// Parse quantization steps
		quantSteps.resize(channels);
		for (int32 i = 0; i < channels; ++i)
		{
			quantSteps[i] = 1.0f / 256.0f;
		}

		// Tokenize quant steps
		if (quantStepsString.length() > 0)
		{
			E_ASSERT("Invalid quantization steps parameter", (quantStepsString[0] == '[')
				&& (quantStepsString[quantStepsString.length() - 1] == ']'));
			std::istringstream iss(quantStepsString.substr(1, quantStepsString.length() - 2));
			vector<string> quantStepsTokens;
	
			copy(istream_iterator<string>(iss),
				istream_iterator<string>(),
				back_inserter<vector<string>>(quantStepsTokens));

			E_ASSERT("Invalid number of quantization steps: " << quantStepsTokens.size(), quantStepsTokens.size() == channels);
			for (int32 i = 0; i < quantStepsTokens.size(); ++i)
			{
				E_ASSERT("Invalid quantization step token: "/* << s */, sscanf_s(quantStepsTokens[i].c_str(), "%f", &quantSteps[i]) == 1);
			}
		}
	}


	E_ASSERT("BrickSize " << brickSize << " is not a power of 2", IsPow2(brickSize));
	E_ASSERT("No channels found", channels > 0);
	int32 brickDataSize = brickSize - 2 * overlap;


	cout << "Preprocessing started with the following parameters: \n" <<
		"Inpath: " << inPath << "\n" <<
		"Outpath: " << outPath << "\n" <<
		"Tmp path: " << tmpPath << "\n" <<
		"Outfile: " << outFile << "\n" <<
		"Volume size: [" << volumeSize[0] << ", " << volumeSize[1] << ", " << volumeSize[2] << "]\n"
		"Brick size: " << brickSize << "\n" <<
		"Overlap: " << overlap << "\n" <<
		"Compression: " << GetCompressionTypeName(compression) << "\n";
	if(compression == COMPRESSION_FIXEDQUANT || compression == COMPRESSION_FIXEDQUANT_QF)
	{
		cout << "Quantization Steps: [";
		for_each(quantSteps.cbegin(), quantSteps.cend() - 1, [](float f)
		{
			std::cout << f << ", ";
		});
		cout << *(quantSteps.cend() - 1) << "]\n";
		cout << "LessRLE: " << lessRLE << "\n";
	}

	cout << "\n\n";

	cout << "A total of " << channels << " channels was found. Channel files are: \n\n";
	for (auto it = fileList.cbegin(); it != fileList.cend(); ++it)
	{
		cout << "\t" << it->fileMask << " with " << it->channels << " channels\n";
	}

	cout << "\n\n";

	if (!autoBuild)
	{
		cout << "Press \"y\" to continue or any other key to abort: ";
		char c = _getch();

		if (c != 'y')
		{
			exit(0);
		}
	}


	const float REL_ERROR_EPSILON = 0.01f;


	// start global timer
	LARGE_INTEGER timestampGlobalStart;
	QueryPerformanceCounter(&timestampGlobalStart);

	LARGE_INTEGER perfFreq;
	QueryPerformanceFrequency(&perfFreq);

	// common quant step over all channels? (only for statistics output)
	float quantStepCommon = 0.0f;
	if(compression == COMPRESSION_FIXEDQUANT || compression == COMPRESSION_FIXEDQUANT_QF)
	{
		bool haveCommonQuantStep = true;
		for(size_t i = 1; i < quantSteps.size(); i++)
		{
			if(quantSteps[i] != quantSteps[i-1])
			{
				haveCommonQuantStep = false;
			}
		}
		quantStepCommon = haveCommonQuantStep ? quantSteps[0] : -1.0f;
	}

	// accumulated timings, in seconds
	float timeCreateLA3D = 0.0f;
	float timeBricking = 0.0f;
	float timeCompressGPU = 0.0f;
	float timeDecompressGPU = 0.0f;

	cudaEvent_t eventStart, eventEnd;
	cudaSafeCall(cudaEventCreate(&eventStart));
	cudaSafeCall(cudaEventCreate(&eventEnd));


	std::string statsPath = outPath + "stats\\";

	// Create directories
	cout << "\n\n";
	system((string("mkdir \"") + outPath + "\"").c_str());
	system((string("mkdir \"") + tmpPath + "\"").c_str());
	system((string("mkdir \"") + statsPath + "\"").c_str());



	// **** Build each timestep ****
	GPUResources compressShared;
	CompressVolumeResources compressVolume;
	if (compression)
	{
		compressShared.create(CompressVolumeResources::getRequiredResources(brickSize, brickSize, brickSize, 1, huffmanBits));
		compressVolume.create(compressShared.getConfig());
	}


	float *srcSlice = new float[volumeSize[0] * volumeSize[1] * 4];		// 4 = max number of channels
	std::vector<std::vector<float>> rawBrickChannelData(channels);
	std::vector<std::vector<float>> rawBrickChannelDataReconst(channels);
	std::vector<std::vector<uint32>> compressedBrickChannelData(channels);
	std::vector<float*> deviceBrickChannelData(channels);

	for (int i = 0; i < channels; ++i)
	{
		cudaSafeCall(cudaMalloc(&deviceBrickChannelData[i], Cube(brickSize) * sizeof(float)));
	}

	TimeVolumeIO out;

	if (tum3d::FileExists(outPath + outFile + ".timevol") && !overwrite)
	{
		out.Open(outPath + outFile + ".timevol", true);
		E_ASSERT("Cannot append to existing file: VolumeSize mismatch", out.GetVolumeSize() == volumeSize);
		E_ASSERT("Cannot append to existing file: BrickSize mismatch", out.GetBrickSizeWithOverlap() == brickSize);
		E_ASSERT("Cannot append to existing file: Overlap mismatch", out.GetBrickOverlap() == overlap);
		E_ASSERT("Cannot append to existing file: Channels mismatch", out.GetChannelCount() == channels);
		E_ASSERT("Cannot append to existing file: Compression mismatch", out.GetCompressionType() == compression);

		if(compression == COMPRESSION_FIXEDQUANT || compression == COMPRESSION_FIXEDQUANT_QF)
		{
			for (int32 i = 0; i < channels; ++i)
			{
				E_ASSERT("Cannot append to existing file: Channel " << i << " quantization mismatch", out.GetQuantStep(i) == quantSteps[i]);
			}
		}
		if(compression != COMPRESSION_NONE)
		{
			E_ASSERT("Cannot append to existing file: LessRLE mismatch", out.GetUseLessRLE() == lessRLE);
			E_ASSERT("Cannot append to existing file: Huffman bits mismatch", out.GetHuffmanBitsMax() == huffmanBits);
		}
	}
	else
	{
		out.Create(outPath + outFile + ".timevol", volumeSize, periodic, brickSize, overlap, channels, outFile);

		out.SetCompressionType(compression);
		if(compression == COMPRESSION_FIXEDQUANT || compression == COMPRESSION_FIXEDQUANT_QF)
		{
			for (int32 i = 0; i < channels; ++i)
			{
				out.SetQuantStep(i, quantSteps[i]);
			}
		}
		out.SetUseLessRLE(lessRLE);
		out.SetHuffmanBitsMax(huffmanBits);
	}

	out.SetGridSpacing(gridSpacing);
	out.SetTimeSpacing(timeSpacing);

	Vec3i brickCount = (volumeSize + brickDataSize - 1) / brickDataSize;

	char fn[1024];

	uint64 laMem = uint64(0.8f * float(GetAvailableMemory()) / float(fileList.size()));
	// don't use more than 4 GB total
	uint64 laMemMax = 4 * 1024ull * 1024ull * 1024ull / fileList.size();
	laMem = min(laMem, laMemMax);


	Statistics statsGlobal;
	std::vector<Statistics> statsGlobalChannels(channels);

	std::ofstream fileStatsGlobal(statsPath + outFile + "_stats_global.csv");
	std::ofstream fileStatsTimestep(statsPath + outFile + "_stats_timestep.csv");
	std::ofstream fileStatsBrick(statsPath + outFile + "_stats_brick.csv");

	WriteStatsHeaderCSV(fileStatsGlobal);
	fileStatsTimestep << "Timestep;";
	WriteStatsHeaderCSV(fileStatsTimestep);
	fileStatsBrick << "Timestep;BrickX;BrickY;BrickZ;";
	WriteStatsHeaderCSV(fileStatsBrick);

	std::vector<std::ofstream> fileStatsGlobalChannel(channels);
	std::vector<std::ofstream> fileStatsTimestepChannel(channels);
	std::vector<std::ofstream> fileStatsBrickChannel(channels);
	for(int c = 0; c < channels; c++) {
		std::stringstream chan; chan << c;
		fileStatsGlobalChannel[c].open(statsPath + outFile + "_stats_global_channel" + chan.str() + ".csv");
		fileStatsTimestepChannel[c].open(statsPath + outFile + "_stats_timestep_channel" + chan.str() + ".csv");
		fileStatsBrickChannel[c].open(statsPath + outFile + "_stats_brick_channel" + chan.str() + ".csv");

		WriteStatsHeaderCSV(fileStatsGlobalChannel[c]);
		fileStatsTimestepChannel[c] << "Timestep;";
		WriteStatsHeaderCSV(fileStatsTimestepChannel[c]);
		fileStatsBrickChannel[c] << "Timestep;BrickX;BrickY;BrickZ;";
		WriteStatsHeaderCSV(fileStatsBrickChannel[c]);
	}


	// Run through all timesteps
	for (int32 timestep = tMin; timestep <= tMax; timestep += tStep)
	{
		cout << "\n\n\n>>>> Processing timestep #" << timestep << " <<<<<\n\n";

		std::vector<Statistics> statsTimestepChannels(channels);

		LARGE_INTEGER timestampLA3DStart;
		QueryPerformanceCounter(&timestampLA3DStart);

		bool createdLA3Ds = true;

		for (auto fdesc = fileList.begin(); fdesc != fileList.end(); ++fdesc)
		{
			cout << "\n\n";

			sprintf_s(fn, fdesc->fileMask.c_str(), timestep);
			string fileName(fn);

			string tmpFilePath = tmpPath + "tmp_" + fileName + ".la3d";
			wstring wstrTmpFilePath(tmpFilePath.begin(), tmpFilePath.end());

			if(tum3d::FileExists(tmpFilePath))
			{
				//TODO check size etc?
				cout << "Using existing LA3D file " << tmpFilePath << "\n";
				fdesc->tmpArray->Open(wstrTmpFilePath.c_str(), true, laMem);
				createdLA3Ds = false;
				continue;
			}

			cout << "Loading channels from " << fileName << "...\n";

			// Check if file exists
			string filePath = inPath + fileName;
			E_ASSERT("File " << filePath << " does not exist", tum3d::FileExists(filePath));

			// Open file
			FILE *file;
			fopen_s(&file, filePath.c_str(), "rb");
			E_ASSERT("Could not open file \"" << filePath << "\"", file != nullptr);


			// Create output array
			fdesc->tmpArray->Create(volumeSize[0], volumeSize[1], volumeSize[2], 64, 64, 64, wstrTmpFilePath.c_str(), laMem);

			// Read slice by slice and write to target array
			cout << "Creating LA3D for " << fileName << "\n\n";

			for (int32 slice = 0; slice < volumeSize[2]; ++slice)
			{
				SimpleProgress(slice, volumeSize[2] - 1);

				fread(srcSlice, sizeof(float) * fdesc->channels, volumeSize[0] * volumeSize[1], file);
				fdesc->tmpArray->CopyFrom(srcSlice, 0, 0, slice, volumeSize[0], volumeSize[1], 1, volumeSize[0], volumeSize[1]);
			}
		}

		LARGE_INTEGER timestampLA3DEnd;
		QueryPerformanceCounter(&timestampLA3DEnd);
		timeCreateLA3D += float(timestampLA3DEnd.QuadPart - timestampLA3DStart.QuadPart) / float(perfFreq.QuadPart);

		cout << "\n\nBricking...\n";

		LARGE_INTEGER timestampBrickingStart;
		QueryPerformanceCounter(&timestampBrickingStart);

		// Brick and write
		for (int32 bz = 0; bz < brickCount[2]; ++bz)
		{
			for (int32 by = 0; by < brickCount[1]; ++by)
			{
				for (int32 bx = 0; bx < brickCount[0]; ++bx)
				{
					SimpleProgress(bx + by * brickCount[0] + bz * brickCount[0] * brickCount[1], brickCount[0] * brickCount[1] * brickCount[2] - 1);

					Vec3i spatialIndex(bx, by, bz);
					Vec3ui size(
						bx < brickCount[0] - 1 ? brickSize : volumeSize[0] - bx * brickDataSize + 2 * overlap,
						by < brickCount[1] - 1 ? brickSize : volumeSize[1] - by * brickDataSize + 2 * overlap,
						bz < brickCount[2] - 1 ? brickSize : volumeSize[2] - bz * brickDataSize + 2 * overlap);

					Statistics statsBrick;

					int32 absChannel = 0;
					for (auto fdesc = fileList.begin(); fdesc != fileList.end(); ++fdesc)
					{
						vector<float*> dstPtr(fdesc->channels);
						for (int32 localChannel = 0; localChannel < fdesc->channels; ++localChannel)
						{
							int32 channel = absChannel + localChannel;
							// Create target channel data
							rawBrickChannelData[channel].resize(size.volume());
							dstPtr[localChannel] = rawBrickChannelData[channel].data();
							rawBrickChannelDataReconst[channel].resize(size.volume());
						}

						for (uint32 z = 0; z < size.z(); ++z)
						{
							for (uint32 y = 0; y < size.y(); ++y)
							{
								for (uint32 x = 0; x < size.x(); ++x)
								{
									int32 volPos[3];
									volPos[0] = bx * brickDataSize - overlap + x;
									volPos[1] = by * brickDataSize - overlap + y;
									volPos[2] = bz * brickDataSize - overlap + z;

									if(periodic)
									{
										volPos[0] = (volumeSize[0] + volPos[0]) % volumeSize[0];
										volPos[1] = (volumeSize[1] + volPos[1]) % volumeSize[1];
										volPos[2] = (volumeSize[2] + volPos[2]) % volumeSize[2];
									}
									else
									{
										volPos[0] = clamp(volPos[0], 0, volumeSize[0] - 1);
										volPos[1] = clamp(volPos[1], 0, volumeSize[1] - 1);
										volPos[2] = clamp(volPos[2], 0, volumeSize[2] - 1);
									}

									for (int32 localChannel = 0; localChannel < fdesc->channels; ++localChannel)
									{
										*dstPtr[localChannel]++ = fdesc->tmpArray->Get(volPos[0], volPos[1], volPos[2])[localChannel];
									}
								}
							}
						}

						for (int32 localChannel = 0; localChannel < fdesc->channels; ++localChannel)
						{
							int32 channel = absChannel + localChannel;
							Statistics statsBrickChannel;
							float quantStep = (compression == COMPRESSION_FIXEDQUANT || compression == COMPRESSION_FIXEDQUANT_QF) ? quantSteps[channel] : 0.0f;
							if (compression != COMPRESSION_NONE)
							{
								float* pOrig = rawBrickChannelData[channel].data();
								float* pDevice = deviceBrickChannelData[channel];
								float* pReconst = rawBrickChannelDataReconst[channel].data();
								size_t sizeRaw = rawBrickChannelData[channel].size() * sizeof(float);

								std::vector<uint>& bitStream = compressedBrickChannelData[channel];

								// upload raw brick data
								cudaSafeCall(cudaMemcpy(pDevice, pOrig, sizeRaw, cudaMemcpyHostToDevice));

								// compress
								cudaSafeCall(cudaEventRecord(eventStart));
								if(compression == COMPRESSION_FIXEDQUANT) {
									compressVolumeFloat(compressShared, compressVolume, pDevice, size[0], size[1], size[2], 2, bitStream, quantStep, lessRLE);
								} else if(compression == COMPRESSION_FIXEDQUANT_QF) {
									compressVolumeFloatQuantFirst(compressShared, compressVolume, pDevice, size[0], size[1], size[2], 2, bitStream, quantStep, lessRLE);
								} else {
									printf("Unknown/unsupported compression mode %u\n", uint(compression));
									exit(42);
								}
								cudaSafeCall(cudaEventRecord(eventEnd));

								float t;
								cudaSafeCall(cudaEventSynchronize(eventEnd));
								cudaSafeCall(cudaEventElapsedTime(&t, eventStart, eventEnd));
								timeCompressGPU += t / 1000.0f;

								// decompress again
								cudaSafeCall(cudaEventRecord(eventStart));
								if(compression == COMPRESSION_FIXEDQUANT) {
									decompressVolumeFloat(compressShared, compressVolume, pDevice, size[0], size[1], size[2], 2, bitStream, quantStep, lessRLE);
								} else if(compression == COMPRESSION_FIXEDQUANT_QF) {
									decompressVolumeFloatQuantFirst(compressShared, compressVolume, pDevice, size[0], size[1], size[2], 2, bitStream, quantStep, lessRLE);
								} else {
									printf("Unknown/unsupported compression mode %u\n", uint(compression));
									exit(42);
								}
								cudaSafeCall(cudaEventRecord(eventEnd));

								cudaSafeCall(cudaEventSynchronize(eventEnd));
								cudaSafeCall(cudaEventElapsedTime(&t, eventStart, eventEnd));
								timeDecompressGPU += t / 1000.0f;

								// download reconstructed data
								cudaSafeCall(cudaMemcpy(pReconst, pDevice, sizeRaw, cudaMemcpyDeviceToHost));

								// compute statistics
								statsBrickChannel.AddData(pOrig, pReconst, size.volume(), bitStream.size() * sizeof(uint), REL_ERROR_EPSILON);
							}
							else
							{
								float* pData = rawBrickChannelData[channel].data();
								// compute statistics
								statsBrickChannel.AddData(pData, (const float*)nullptr, size.volume(), 0, REL_ERROR_EPSILON);
							}

							// write statsBrickChannel to file
							fileStatsBrickChannel[channel] << timestep << ";" << bx << ";" << by << ";" << bz << ";";
							WriteStatsCSV(fileStatsBrickChannel[channel], statsBrickChannel.GetStats(), quantStep);

							// update accumulated statistics
							statsBrick += statsBrickChannel;
							statsTimestepChannels[channel] += statsBrickChannel;
						}

						absChannel += fdesc->channels;
					}

					// write statsBrick to file
					fileStatsBrick << timestep << ";" << bx << ";" << by << ";" << bz << ";";
					WriteStatsCSV(fileStatsBrick, statsBrick.GetStats(), quantStepCommon);


					// Add brick to output
					if (compression != COMPRESSION_NONE)
					{
						out.AddBrick((timestep - tMin) / tStep + tOffset, spatialIndex, size, compressedBrickChannelData);
					}
					else
					{
						out.AddBrick((timestep - tMin) / tStep + tOffset, spatialIndex, size, rawBrickChannelData);
					}

				}	// For x
			}	// For y
		}	// For z

		LARGE_INTEGER timestampBrickingEnd;
		QueryPerformanceCounter(&timestampBrickingEnd);
		timeBricking += float(timestampBrickingEnd.QuadPart - timestampBrickingStart.QuadPart) / float(perfFreq.QuadPart);


		// write per-channel timestep stats to file, and accumulate stats over all channels
		Statistics statsTimestep;
		for(int c = 0; c < channels; c++) {
			float quantStep = (compression == COMPRESSION_FIXEDQUANT || compression == COMPRESSION_FIXEDQUANT_QF) ? quantSteps[c] : 0.0f;
			fileStatsTimestepChannel[c] << timestep << ";";
			WriteStatsCSV(fileStatsTimestepChannel[c], statsTimestepChannels[c].GetStats(), quantStep);

			statsTimestep += statsTimestepChannels[c];
			statsGlobalChannels[c] += statsTimestepChannels[c];
		}

		// write accumulated timestep stats to file
		fileStatsTimestep << timestep << ";";
		WriteStatsCSV(fileStatsTimestep, statsTimestep.GetStats(), quantStepCommon);


		// close and (optionally) delete temp files
		for (auto fdesc = fileList.begin(); fdesc != fileList.end(); ++fdesc)
		{
			// delete file only if we created it ourselves
			bool discard = createdLA3Ds && !keepLA3Ds;
			// don't bother saving anything if file will be deleted anyway
			fdesc->tmpArray->Close(discard);
			if(discard)
			{
				// delete the file
				sprintf_s(fn, fdesc->fileMask.c_str(), timestep);
				string fileName(fn);
				string tmpFilePath = tmpPath + "tmp_" + fileName + ".la3d";
				DeleteFileA(tmpFilePath.c_str());
			}
		}

	}	// For timestep

	// write accumulated global stats to file
	Statistics statsTimestep;
	for(int c = 0; c < channels; c++) {
		float quantStep = (compression == COMPRESSION_FIXEDQUANT || compression == COMPRESSION_FIXEDQUANT_QF) ? quantSteps[c] : 0.0f;
		WriteStatsCSV(fileStatsGlobalChannel[c], statsGlobalChannels[c].GetStats(), quantStep);

		statsGlobal += statsGlobalChannels[c];
	}
	WriteStatsCSV(fileStatsGlobal, statsGlobal.GetStats(), quantStepCommon);


	for (auto fdesc = fileList.begin(); fdesc != fileList.end(); ++fdesc)
	{
		delete fdesc->tmpArray;
	}

	delete[] srcSlice;

	for (int i = 0; i < channels; ++i)
	{
		cudaSafeCall(cudaFree(deviceBrickChannelData[i]));
	}

	if (compression)
	{
		compressVolume.destroy();
		compressShared.destroy();
	}

	cudaSafeCall(cudaEventDestroy(eventEnd));
	cudaSafeCall(cudaEventDestroy(eventStart));


	// stop global timer
	LARGE_INTEGER timestampGlobalEnd;
	QueryPerformanceCounter(&timestampGlobalEnd);
	float timeGlobal = float(timestampGlobalEnd.QuadPart - timestampGlobalStart.QuadPart) / float(perfFreq.QuadPart);

	// write timings to file
	std::ofstream fileTimings(outPath + outFile + "_timings.txt");
	fileTimings;
	fileTimings << "Total time:       " << fixed << setw(10) << setprecision(3) << timeGlobal << " s\n";
	fileTimings << "Create LA3Ds:     " << fixed << setw(10) << setprecision(3) << timeCreateLA3D << " s\n";
	fileTimings << "Bricking:         " << fixed << setw(10) << setprecision(3) << timeBricking << " s\n";
	fileTimings << "Compress (GPU):   " << fixed << setw(10) << setprecision(3) << timeCompressGPU << " s\n";
	fileTimings << "Decompress (GPU): " << fixed << setw(10) << setprecision(3) << timeDecompressGPU << " s\n";
	fileTimings.close();

	return 0;
}

