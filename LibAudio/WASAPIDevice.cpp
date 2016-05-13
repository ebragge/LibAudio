#include "pch.h"
#include "WASAPIDevice.h"

using namespace LibAudio;

WASAPIDevice::WASAPIDevice() : m_initialized(false)
{
}

void WASAPIDevice::StopAsync()
{
	if (Capture != nullptr)
	{
		Capture->StopCaptureAsync();
	}
}

void WASAPIDevice::InitCaptureDevice(size_t id, DataCollector^ collector)
{
	Number = id;
	if (Capture)
	{
		Capture = nullptr;
	}

	Capture = Make<WASAPICapture>();

	StateChangedEvent = Capture->GetDeviceStateEvent();
	DeviceStateChangeToken = StateChangedEvent->StateChangedEvent += ref new DeviceStateChangedHandler(this, &WASAPIDevice::OnDeviceStateChange);

	Capture->InitializeAudioDeviceAsync(ID, Number, collector);
	m_initialized = true;
}

void WASAPIDevice::InitToneDevice(size_t id, DataCollector^ collector, uint32 frequency)
{
    HRESULT hr = S_OK;

    if (Renderer)
    {
		Renderer = nullptr;
	}
	
    Renderer = Make<WASAPIRenderer>();
	
    StateChangedEvent = Renderer->GetDeviceStateEvent();
	DeviceStateChangeToken = StateChangedEvent->StateChangedEvent += ref new DeviceStateChangedHandler(this, &WASAPIDevice::OnDeviceStateChange);

	
	DEVICEPROPS props;
	props.IsTonePlayback = true;
	props.Frequency = (DWORD)frequency;
	
	props.IsHWOffload = false;
	props.IsBackground = false;

	Renderer->SetProperties(props);
	
	Renderer->InitializeAudioDeviceAsync(ID, Number, collector);
	m_initialized = true;
}

void WASAPIDevice::OnDeviceStateChange(Object^ sender, DeviceStateChangedEventArgs^ e)
{
	// Get the current time for messages
	auto t = Windows::Globalization::DateTimeFormatting::DateTimeFormatter::LongTime;
	Windows::Globalization::Calendar^ calendar = ref new Windows::Globalization::Calendar();
	calendar->SetToNow();

	// Handle state specific messages
	switch (e->State)
	{
		case DeviceState::DeviceStateActivated:
		{
			String^ str = String::Concat(ID, "-DeviceStateActivated\n");
			OutputDebugString(str->Data());
			break;
		}
		case DeviceState::DeviceStateInitialized:
		{
			String^ str = String::Concat(ID, "-DeviceStateInitialized\n");
			OutputDebugString(str->Data());
			if (Capture != nullptr)
			{
				Capture->StartCaptureAsync();
			}
			if (Renderer != nullptr)
			{
				Renderer->StartPlaybackAsync();
			}
			break;
		}
		case DeviceState::DeviceStateCapturing:
		{
			String^ str = String::Concat(ID, "-DeviceStateCapturing\n");
			OutputDebugString(str->Data());
			break;
		}
		case DeviceState::DeviceStateDiscontinuity:
		{
			String^ str = String::Concat(ID, "-DeviceStateDiscontinuity\n");
			OutputDebugString(str->Data());
			break;
		}
		case DeviceState::DeviceStateInError:
		{
			String^ str = String::Concat(ID, "-DeviceStateInError\n");
			OutputDebugString(str->Data());
			break;
		}
	}
}
