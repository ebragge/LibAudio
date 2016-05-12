#pragma once
#include "WASAPICapture.h"
#include "DeviceState.h"
#include "WASAPIDevice.h"
#include "DataCollector.h"
#include "DataConsumer.h"

using namespace LibAudio;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Media::Devices;
using namespace Windows::Devices::Enumeration;
using namespace Platform::Collections;

namespace LibAudio
{
	// Custom properties defined in mmdeviceapi.h in the format "{GUID} PID"
	static Platform::String^ PKEY_AudioEndpoint_Supports_EventDriven_Mode = "{1da5d803-d492-4edd-8c23-e0c0ffee7f0e} 7";

	public ref class WASAPIEngine sealed
	{
	public:
		WASAPIEngine();
		virtual ~WASAPIEngine();
		IAsyncAction^ InitializeCaptureAsync(UIDelegate1^ func, AudioDevices^ devParams, AudioParameters^ params);
		IAsyncAction^ InitializeCaptureAsync(UIDelegate2^ func, AudioDevices^ devParams, AudioParameters^ params);

		IAsyncAction^ InitializeRendererAsync(AudioDevices^ devParams, AudioParameters^ params);

		IAsyncAction^ GetCaptureDevicesAsync(UIDelegate3^ func);
		IAsyncAction^ GetRendererDevicesAsync(UIDelegate3^ func);

		void Finish();
		void Continue();
		
	private:
		IAsyncAction^ InitializeCaptureAsync(UIDelegate1^ func1, UIDelegate2^ func2, AudioDevices^ devParams, AudioParameters^ params);
		std::vector<WASAPIDevice^> m_deviceList;
		DataCollector^	m_collector;
		DataConsumer^	m_consumer;
	};
}
