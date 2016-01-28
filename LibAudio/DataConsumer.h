#pragma once

#include <Windows.h>
#include <wrl\implements.h>
#include <mfapi.h>
#include "DataCollector.h"
#include "..\LibTDE\TimeDelayEstimation.h"

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace TimeDelayEstimation;

namespace LibAudio
{
	static Windows::Foundation::IAsyncAction^ Task = nullptr;
	static Windows::Foundation::IAsyncAction^ StoreTask = nullptr;

	public enum class HeartBeatType
	{
		DATA,
		BUFFERING,
		DEVICE_ERROR,
		SILENCE,
		INVALID,
		NODEVICE,
		STARTING,
		STATUS
	};

	public ref class TDEDevices sealed
	{
	public:

		TDEDevices(uint32 Devices) : minDevices(Devices), device0(0), device1(1), device2(2), channel0(0), channel1(0), channel2(0) {}
		TDEDevices(uint32 Devices, uint32 Device0, uint32 Device1, uint32 Device2, uint32 Channel0, uint32 Channel1, uint32 Channel2) :
			minDevices(Devices),
			device0(Device0), device1(Device1), device2(Device2),
			channel0(Channel0),	channel1(Channel1),	channel2(Channel2)	
		{}

		uint32 MinDevices() { return minDevices; }

		uint32 Device0() { return device0; }
		uint32 Device1() { return device1; }
		uint32 Device2() { return device2; }
		uint32 Channel0() { return channel0; }
		uint32 Channel1() { return channel1; }
		uint32 Channel2() { return channel2; }

	private:

		uint32 minDevices;

		uint32 device0;
		uint32 device1;
		uint32 device2;

		uint32 channel0;
		uint32 channel1;
		uint32 channel2;
	};

	public ref class TDEParameters sealed
	{
	public:

		TDEParameters(size_t BufferSize, size_t AnalysisWindowSize, size_t MaxDelay, 
			uint32 AudioThreshold, int StartOffset, bool Interrupt, 
			bool StoreSample, Platform::String^ FileName, uint32 StoreThreshold, uint32 StoreExtra) :
			bufferSize(BufferSize), 
			tdeWindow(AnalysisWindowSize),
			delayWindow(MaxDelay),
			audioThreshold(AudioThreshold),
			startOffset(StartOffset),
			interrupt(Interrupt),
			storeSample(StoreSample),
			sampleFile(FileName),
			storeThreshold(StoreThreshold),
			storeExtra(StoreExtra)
			{}

		size_t BufferSize() { return bufferSize; }
		size_t TDEWindow()  { return tdeWindow; }
		size_t DelayWindow() { return delayWindow; }
		int StartOffset() { return startOffset; }

		uint32 AudioThreshold() { return audioThreshold; }

		bool Interrupt() { return interrupt;  }
		bool StoreSample() { return storeSample; }

		Platform::String^ SampleFile() { return sampleFile; }

		uint32 StoreThreshold() { return storeThreshold; }
		uint32 StoreExtra() { return storeExtra; }
		 
	private:

		uint32 minDevices;
		
		uint32 device0;
		uint32 device1;
		uint32 device2;
		
		uint32 channel0;
		uint32 channel1;
		uint32 channel2;

		size_t bufferSize;
		size_t tdeWindow;
		size_t delayWindow;

		uint32 audioThreshold;

		uint32 startOffset;

		bool interrupt;
		bool storeSample;

		Platform::String^ sampleFile;

		uint32 storeThreshold;
		uint32 storeExtra;
	};

	public delegate void UIDelegate(
		HeartBeatType, 
		UINT64, /* BEGIN */
		UINT64, /* END */
		int, /*  CC0  */
		int, /* ASDF0 */
		int, /* PEAK0 */
		int, /* ALIGN0 */
		int, /*  CC1  */
		int, /* ASDF1 */
		int, /* PEAK1 */
		int, /* ALIGN1 */
		int, /*  CC2  */
		int, /* ASDF2 */
		int, /* PEAK2 */
		int, /* ALIGN2 */
		uint32, /* MAX ALL */		
		uint32, /* MAX0 */
		uint32, /* MAX1 */
		uint32, /* MAX2 */
		uint32, /* AVERAGE ALL */
		uint32, /* AVERAGE 0   */
		uint32, /* AVERAGE 1   */
		uint32, /* AVERAGE 2 */	
		uint32 /* SAMPLES PER SECOND */);

	ref class DataConsumer sealed
	{
	public:

		DataConsumer(size_t nDevices, DataCollector^ collector, UIDelegate^ func, TDEDevices^ devices, TDEParameters^ parameters);

		void Start();
		void Stop();

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

		void HeartBeat(int delta, HeartBeatType status, UINT64 fts = 0, UINT64 lts = 0,
			int i0 = 0, int i1 = 0, int i2 = 0, int i3 = 0, int i4 = 0, int i5 = 0, 
			int i6 = 0, int i7 = 0, int i8 = 0, int i9 = 0, int i10 = 0, int i11 = 0,
			uint32 ui0 = 0, uint32 ui1 = 0, uint32 ui2 = 0, uint32 ui3 = 0, uint32 ui4 = 0, 
			uint32 ui5 = 0, uint32 ui6 = 0, uint32 ui7 = 0, uint32 ui8 = 0);

		Status HandlePackets();
		bool ProcessData(bool& msg);

		void FlushPackets();
		void FlushBuffer();
		void FlushCollector();

		void AddData(size_t device, DWORD cbBytes, const BYTE* pData, UINT64 pos1, UINT64 pos2);
		
		bool CalculateTDE(size_t pos, uint32 average, uint32 maximum, UINT64 fts, UINT64 lts);
		bool CalculateTDE2(size_t pos, uint32 average, uint32 maximum, UINT64 fts, UINT64 lts);
		bool CalculateTDE3(size_t pos, uint32 average, uint32 maximum, UINT64 fts, UINT64 lts);
		
		bool CalculatePair(SignalData& data, int& delay1, int& delay2, int& delay3, 
			CalcType& max0, CalcType& max1, CalcType& ave0, CalcType& ave1, DelayType& align);

		void AudioTask();
		void StorageTask(String^ data);

		concurrency::task<Windows::Storage::StorageFile^> StoreData(String^ data);

	private:

		size_t m_numberOfDevices;
		DataCollector^ m_collector;

		std::vector<DeviceInfo> m_devices;
		std::vector<AudioDataPacket*> m_audioDataFirst;
		std::vector<AudioDataPacket*> m_audioDataLast;

		std::vector<std::vector<std::vector<TimeDelayEstimation::AudioDataItem>>> m_buffer;	

		UIDelegate^ m_uiHandler;
		ULONGLONG m_tick;

		size_t m_packetCounter;
		uint32 m_discontinuityCounter;
		uint32 m_dataRemovalCounter;

		TDEDevices^ m_devParams;
		TDEParameters^ m_params;

		Windows::System::Threading::ThreadPoolTimer ^ m_delayTimer;
	};
}
