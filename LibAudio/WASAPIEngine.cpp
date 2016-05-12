#include "pch.h"
#include "WASAPIEngine.h"

using namespace concurrency;

WASAPIEngine::WASAPIEngine()
{
	m_deviceList = std::vector<WASAPIDevice^>();
	m_collector = nullptr;
	m_consumer = nullptr;
	HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
}

WASAPIEngine::~WASAPIEngine()
{
}

IAsyncAction^ WASAPIEngine::InitializeCaptureAsync(UIDelegate1^ func, AudioDevices^ devParams, AudioParameters^ params)
{
	return InitializeCaptureAsync(func, nullptr, devParams, params);
}

[Windows::Foundation::Metadata::DefaultOverloadAttribute]
IAsyncAction^ WASAPIEngine::InitializeCaptureAsync(UIDelegate2^ func, AudioDevices^ devParams, AudioParameters^ params)
{
	return InitializeCaptureAsync(nullptr, func, devParams, params);
}

IAsyncAction^ WASAPIEngine::InitializeCaptureAsync(UIDelegate1^ func1, UIDelegate2^ func2, AudioDevices^ devParams, AudioParameters^ params)
{
	return create_async([this ,func1, func2, devParams, params]
	{
		// Get the string identifier of the audio renderer
		auto AudioSelector = MediaDevice::GetAudioCaptureSelector();

		// Add custom properties to the query
		auto PropertyList = ref new Platform::Collections::Vector<String^>();
		PropertyList->Append(PKEY_AudioEndpoint_Supports_EventDriven_Mode);

		// Setup the asynchronous callback
		Concurrency::task<DeviceInformationCollection^> enumOperation(DeviceInformation::FindAllAsync(AudioSelector, PropertyList));
		enumOperation.then([this](DeviceInformationCollection^ DeviceInfoCollection)
		{
			if ((DeviceInfoCollection == nullptr) || (DeviceInfoCollection->Size == 0))
			{
				OutputDebugString(L"No Devices Found.\n");
			}
			else
			{
				try
				{
					// Enumerate through the devices and the custom properties
					for (unsigned int i = 0; i < DeviceInfoCollection->Size; i++)
					{ 
						auto deviceInfo = DeviceInfoCollection->GetAt(i);
						auto DeviceInfoString = deviceInfo->Name;
						auto DeviceIdString = deviceInfo->Id;

						if (deviceInfo->Properties->Size > 0)
						{
							auto device = ref new WASAPIDevice();
							device->Name = DeviceInfoString;
							device->ID = DeviceIdString;
							m_deviceList.push_back(device);
							OutputDebugString(device->Name->Data());
							OutputDebugString(device->ID->Data());
							OutputDebugString(L"\n");
						}
					}
				}
				catch (Platform::Exception^) {}
			}
		})
		.then([this, func1, func2, devParams, params]()
		{
			if (m_deviceList.size() >= devParams->Devices()) 
			{
				m_collector = ref new DataCollector(devParams->Devices());
				for (size_t i = 0; i < m_deviceList.size(); i++)
				{
					auto index = devParams->GetIndex(m_deviceList[i]->ID, i);
					if (index != -1)
					{
						m_deviceList[i]->InitCaptureDevice(index, m_collector);
					}
				}
				m_consumer = ref new DataConsumer(devParams->Devices(), m_collector, func1, func2, devParams, params);
				m_consumer->Start();
			}
			else
			{
				if (func1 == nullptr)
				{
					func2(LibAudio::HeartBeatType::NODEVICE, 0, 0, 0, 0, nullptr, nullptr, nullptr, nullptr);
				}
				else
				{
					func1(LibAudio::HeartBeatType::NODEVICE, nullptr, nullptr, nullptr);
				}
			}
		});
	});
}

IAsyncAction^ WASAPIEngine::InitializeRendererAsync(AudioDevices^ devParams, AudioParameters^ params)
{
	return create_async([this, devParams, params]
	{
		// Get the string identifier of the audio renderer
		auto AudioSelector = MediaDevice::GetAudioRenderSelector();

		// Add custom properties to the query
		auto PropertyList = ref new Platform::Collections::Vector<String^>();
		PropertyList->Append(PKEY_AudioEndpoint_Supports_EventDriven_Mode);

		// Setup the asynchronous callback
		Concurrency::task<DeviceInformationCollection^> enumOperation(DeviceInformation::FindAllAsync(AudioSelector, PropertyList));
		enumOperation.then([this](DeviceInformationCollection^ DeviceInfoCollection)
		{
			if ((DeviceInfoCollection == nullptr) || (DeviceInfoCollection->Size == 0))
			{
				OutputDebugString(L"No Devices Found.\n");
			}
			else
			{
				try
				{
					// Enumerate through the devices and the custom properties
					for (unsigned int i = 0; i < DeviceInfoCollection->Size; i++)
					{ 
						auto deviceInfo = DeviceInfoCollection->GetAt(i);
						auto DeviceInfoString = deviceInfo->Name;
						auto DeviceIdString = deviceInfo->Id;

						if (deviceInfo->Properties->Size > 0)
						{
							auto device = ref new WASAPIDevice();
							device->Name = DeviceInfoString;
							device->ID = DeviceIdString;
							m_deviceList.push_back(device);
							OutputDebugString(device->Name->Data());
							OutputDebugString(device->ID->Data());
							OutputDebugString(L"\n");
						}
					}
				}
				catch (Platform::Exception^) {}
			}
		})
		.then([this, devParams, params]()
		{
			if (m_deviceList.size() >= devParams->Devices()) 
			{
				m_collector = ref new DataCollector(devParams->Devices());
				for (size_t i = 0; i < m_deviceList.size(); i++)
				{
					auto index = devParams->GetIndex(m_deviceList[i]->ID, i);
					if (index != -1)
					{
						m_deviceList[i]->InitRendererDevice(index, nullptr);
					}
				}
			}
		});
	});
}

IAsyncAction^ WASAPIEngine::GetCaptureDevicesAsync(UIDelegate3^ func)
{
	return create_async([this, func]
	{
		// Get the string identifier of the audio renderer
		auto AudioSelector = MediaDevice::GetAudioCaptureSelector();

		// Add custom properties to the query
		auto PropertyList = ref new Platform::Collections::Vector<String^>();
		PropertyList->Append(PKEY_AudioEndpoint_Supports_EventDriven_Mode);

		// Setup the asynchronous callback
		Concurrency::task<DeviceInformationCollection^> enumOperation(DeviceInformation::FindAllAsync(AudioSelector, PropertyList));
		enumOperation.then([this, func](DeviceInformationCollection^ DeviceInfoCollection)
		{
			if ((DeviceInfoCollection == nullptr) || (DeviceInfoCollection->Size == 0))
			{
				func(L"No Devices Found.\n");
			}
			else
			{
				try
				{
					// Enumerate through the devices and the custom properties
					for (unsigned int i = 0; i < DeviceInfoCollection->Size; i++)
					{
						auto deviceInfo = DeviceInfoCollection->GetAt(i);
						func("C - " + deviceInfo->Id + (deviceInfo->IsDefault ? "*" : ""));
					}
				}
				catch (Platform::Exception^) {}
			}
		});
	});
}

IAsyncAction^ WASAPIEngine::GetRendererDevicesAsync(UIDelegate3^ func)
{
	return create_async([this, func]
	{
		// Get the string identifier of the audio renderer
		auto AudioSelector = MediaDevice::GetAudioRenderSelector();

		// Add custom properties to the query
		auto PropertyList = ref new Platform::Collections::Vector<String^>();
		PropertyList->Append(PKEY_AudioEndpoint_Supports_EventDriven_Mode);

		// Setup the asynchronous callback
		Concurrency::task<DeviceInformationCollection^> enumOperation(DeviceInformation::FindAllAsync(AudioSelector, PropertyList));
		enumOperation.then([this, func](DeviceInformationCollection^ DeviceInfoCollection)
		{
			if ((DeviceInfoCollection == nullptr) || (DeviceInfoCollection->Size == 0))
			{
				func(L"No Devices Found.\n");
			}
			else
			{
				try
				{
					// Enumerate through the devices and the custom properties
					for (unsigned int i = 0; i < DeviceInfoCollection->Size; i++)
					{
						auto deviceInfo = DeviceInfoCollection->GetAt(i);
						func("R - " + deviceInfo->Id + (deviceInfo->IsDefault ? "*" : ""));
					}
				}
				catch (Platform::Exception^) {}
			}
		});
	});
}

void WASAPIEngine::Finish()
{
	if (m_consumer != nullptr)
	{
		m_consumer->Finish();
		m_consumer = nullptr;
	}

	for (size_t i = 0; i < m_deviceList.size(); i++)
	{
		if (m_deviceList[i]->Initialized())
		{
			m_deviceList[i]->StopAsync();
		}		
		m_deviceList[i] = nullptr;
	}

	if (m_collector != nullptr)
	{
		m_collector->Finish();
		m_collector = nullptr;
	}		
}

void WASAPIEngine::Continue()
{
	if (m_consumer != nullptr)
	{
		m_consumer->Continue();
	}
}
