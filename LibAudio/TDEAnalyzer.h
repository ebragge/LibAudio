#pragma once

#include <Windows.h>
#include <wrl\implements.h>
#include <mfapi.h>

#include "HeartBeatSender.h"
#include "IAnalyzer.h"

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace TimeDelayEstimation;

namespace LibAudio
{
	class TDEAnalyzer : public IAnalyzer
	{
	public:
		TDEAnalyzer(HeartBeatSender^ sender);

		virtual void ProcessData(
			size_t numberOfDevices,
			AudioDevices^ audioDevices,
			AudioParameters^ audioParameters,
			const std::vector<DeviceInfo>& m_devices,
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
			const Array<UINT64>^ timeStampL) override {};

	private:

		bool CalculateTDE(size_t pos, uint32 average, uint32 maximum, UINT64 fts, UINT64 lts,
			size_t numberOfDevices,
			AudioDevices^ audioDevices,
			AudioParameters^ audioParameters,
			const std::vector<DeviceInfo>& deviceInfo,
			const AudioBuffer& audioData,
			size_t smallestBuffer);

		bool CalculateTDE2(size_t pos, uint32 average, uint32 maximum, UINT64 fts, UINT64 lts,
			size_t numberOfDevices,
			AudioDevices^ audioDevices,
			AudioParameters^ audioParameters,
			const std::vector<DeviceInfo>& deviceInfo,
			const AudioBuffer& audioData,
			size_t smallestBuffer);

		bool CalculateTDE3(size_t pos, uint32 average, uint32 maximum, UINT64 fts, UINT64 lts,
			size_t numberOfDevices,
			AudioDevices^ audioDevices,
			AudioParameters^ audioParameters,
			const std::vector<DeviceInfo>& deviceInfo,
			const AudioBuffer& audioData,
			size_t smallestBuffer);

		bool CalculatePair(AudioParameters^ audioParameters, SignalData& data, int& delay1, int& delay2, int& delay3,
			CalcType& max0, CalcType& max1, CalcType& ave0, CalcType& ave1, DelayType& align);

		void StorageTask(String^ data, AudioParameters^ audioParameters);
		concurrency::task<Windows::Storage::StorageFile^> StoreData(String^ data, AudioParameters^ audioParameters);

		HeartBeatSender^ m_sender;

		Windows::Foundation::IAsyncAction^ StoreTask;
	};
}
