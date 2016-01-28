#pragma once

#include <Windows.h>
#include <wrl\implements.h>
#include <mfapi.h>

#include "HeartBeatSender.h"
#include "DataCollector.h"
#include "AudioDevices.h"
#include "AudioParameters.h"
#include "IAnalyzer.h"

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace TimeDelayEstimation;

namespace LibAudio
{
	const int MAX_TASKS = 10;
	const int TRANSFER_BUFFER = 1000000;

	static Windows::Foundation::IAsyncAction^ Task = nullptr;

	ref class DataConsumer sealed
	{
	public:
		DataConsumer(
			size_t nDevices, 
			DataCollector^ collector, 
			UIDelegate1^ func1, 
			UIDelegate2^ func2, 
			AudioDevices^ devices, 
			AudioParameters^ parameters);

		void Start();
		void Stop();
		void Continue();

	internal:
		HRESULT Finish();

	private:
		enum class Status
		{
			ONLY_ONE_SAMPLE,
			EXCESS_DATA,
			DISCONTINUITY,
			SILENCE,
			DATA_AVAILABLE
		};

	private:
		~DataConsumer();

		void AudioTask();
		
		Status HandlePackets(bool silenceCheck);
		bool AddData(size_t device, DWORD cbBytes, const BYTE* pData, UINT64 pos1, UINT64 pos2, bool silenceCheck);
		
		bool ProcessData(bool& msg);

		void HandleProcessTasks(size_t smallestBuffer);	

		void FlushPackets();
		void FlushBuffer();
		void FlushCollector();

		void ProcessTask(
			int idx,
			uint32 beginR,
			uint32 endR,
			uint32 beginL,
			uint32 endL,
			const Array<int32>^ memoryR,
			const Array<int32>^ memoryL,
			const Array<UINT64>^ timeStampR, 
			const Array<UINT64>^ timeStampL);

		void ProcessTask(
			int idx,
			size_t numberOfDevices,
			AudioDevices^ devParams,
			AudioParameters^ params,
			std::vector<DeviceInfo> devices,
			AudioBuffer* buffer,
			size_t smallestBuffer);

	private:
		bool m_running;

		HeartBeatSender^ m_sender;
		IAnalyzer* m_analyzer;

		size_t m_numberOfDevices;
		DataCollector^ m_collector;

		std::vector<DeviceInfo> m_devices;
		std::vector<AudioDataPacket*> m_audioDataFirst;
		std::vector<AudioDataPacket*> m_audioDataLast;

		AudioBuffer* m_buffer;

		AudioDevices^ m_devParams;
		AudioParameters^ m_params;

		Windows::System::Threading::ThreadPoolTimer ^ m_delayTimer;

		Windows::Foundation::IAsyncAction^ m_processTasks[MAX_TASKS];
		bool m_silenceCheck;

		uint32 m_beginR;
		uint32 m_endR;

		uint32 m_beginL;
		uint32 m_endL;

		Array<int32>^ m_memoryR;
		Array<int32>^ m_memoryL;

		Array<UINT64>^ m_timestampR;
		Array<UINT64>^ m_timestampL;
	};
}
