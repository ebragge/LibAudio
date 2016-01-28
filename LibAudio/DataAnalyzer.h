#pragma once

#include <Windows.h>
#include <wrl\implements.h>
#include <mfapi.h>

#include "HeartBeatSender.h"
#include "IAnalyzer.h"

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Storage;

namespace LibAudio
{
	class DataAnalyzer : public IAnalyzer
	{
	public:
		DataAnalyzer(HeartBeatSender^ sender);

		virtual void ProcessData(
			size_t numberOfDevices,
			AudioDevices^ audioDevices,
			AudioParameters^ audioParameters,
			const std::vector<DeviceInfo>& deviceInfo,
			AudioBuffer* audioData,
			size_t smallestBuffer) override;

		virtual void ProcessData(
			uint32 beginR,
			uint32 endR,
			uint32 beginL,
			uint32 endL,
			const Array<int32>^ memoryR,
			const Array<int32>^ memoryL,
			const Array<UINT64>^ timeStampR, 
			const Array<UINT64>^ timeStampL) override;

	private:
		HeartBeatSender^ m_sender;
	};
}
