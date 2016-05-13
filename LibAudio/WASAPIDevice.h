#pragma once
#include "WASAPICapture.h"
#include "WASAPIRenderer.h"

#include "DeviceState.h"
#include "DataCollector.h"

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Media::Devices;
using namespace Windows::Devices::Enumeration;
using namespace Platform::Collections;

namespace LibAudio
{
	ref class WASAPIDevice sealed
	{
	public:
		WASAPIDevice();
		void InitCaptureDevice(size_t id, DataCollector^ collector);
		void InitToneDevice(size_t id, DataCollector^ collector, uint32 frequency);

		bool Initialized() { return m_initialized; }
		void StopAsync();

		property String^ ID;
		property String^ Name;
		property size_t Number;

	private:
		void OnDeviceStateChange(Object^ sender, DeviceStateChangedEventArgs^ e);
		
	internal:
		property ComPtr<WASAPICapture>		Capture;
		property ComPtr<WASAPIRenderer>		Renderer;

		property DeviceStateChangedEvent^   StateChangedEvent;
		property EventRegistrationToken		DeviceStateChangeToken;		

		bool m_initialized;
	};
};
