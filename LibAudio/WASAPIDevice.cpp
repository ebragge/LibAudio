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

void WASAPIDevice::InitRendererDevice(size_t id, DataCollector^ collector)
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
	props.Frequency = static_cast<DWORD>(440);
	/*
	switch (m_ContentType)
	{
	case ContentType::ContentTypeTone:
		props.IsTonePlayback = true;
		props.Frequency = static_cast<DWORD>(sliderFrequency->Value);
		break;

	case ContentType::ContentTypeFile:
		props.IsTonePlayback = false;
		props.ContentStream = m_ContentStream;
		break;
	}

	m_IsMinimumLatency = static_cast<Platform::Boolean>(toggleMinimumLatency->IsOn);
	*/
	//props.IsLowLatency = m_IsMinimumLatency;
	props.IsHWOffload = false;
	props.IsBackground = false;
	//props.IsRawChosen = static_cast<Platform::Boolean>(toggleRawAudio->IsOn);
	//props.IsRawSupported = m_deviceSupportsRawMode;

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
			if (Renderer != nullptr)
			{
				Renderer->StartPlaybackAsync();
			}
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
