#pragma once

#include "AudioDevices.h"
#include "AudioParameters.h"
#include "..\LibTDE\LibTDE\AudioDataItem.h"
#include "..\LibTDE\LibTDE\TimeDelayEstimation.h"
#include "DeviceInfo.h"

using namespace TimeDelayEstimation;
using namespace Platform;

namespace LibAudio
{
	typedef std::vector<std::vector<std::vector<TimeDelayEstimation::AudioDataItem>>> AudioBuffer;

	class IAnalyzer
	{	
	public:
		virtual void ProcessData(
			size_t numberOfDevices,
			AudioDevices^ audioDevices,
			AudioParameters^ audioParameters,
			const std::vector<DeviceInfo>& deviceInfo,
			AudioBuffer* audioData,
			size_t smallestBuffer) = 0;

		virtual void ProcessData(
			uint32 beginR,
			uint32 endR,
			uint32 beginL,
			uint32 endL, 
			const Array<int32>^ memoryR, 
			const Array<int32>^ memoryL,
			const Array<UINT64>^ timeStampR, 
			const Array<UINT64>^ timeStampL) = 0;

	protected:

		void FlushAudioData(size_t numberOfDevices, const std::vector<DeviceInfo>& deviceInfo, AudioBuffer* audioData);

	};
}
