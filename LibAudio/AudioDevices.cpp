#include "pch.h"
#include "AudioDevices.h"

using namespace LibAudio;

AudioDevices::AudioDevices(uint32 devices) : m_devices(devices)
{
	m_channels = ref new Platform::Array<AudioChannel^>(devices);
	for (unsigned int i = 0; i < devices; i++)
	{
		m_channels->set(i, ref new AudioChannel(i, 0, ""));
	}
}

AudioDevices::AudioDevices(const Platform::Array<AudioChannel^>^ channels, uint32 devices) : m_devices(devices)
{
	m_channels = ref new Platform::Array<AudioChannel^>(channels);
}

uint32 AudioDevices::Device(uint32 index)
{
	if (index < m_channels->Length)
	{
		return m_channels->get(index)->Device();
	}
	else return 0;
}

uint32 AudioDevices::Channel(uint32 index)
{
	if (index < m_channels->Length)
	{
		return m_channels->get(index)->Channel();
	}
	else return 0;
}

int AudioDevices::GetIndex(Platform::String^ id, size_t index)
{
	if (m_channels->Length > index && m_channels->get(0)->ID()->Length() == 0) return (int)index;

	for (unsigned int idx = 0; idx < m_channels->Length; idx++)
	{
		if (wcswcs(id->Data(), m_channels->get(idx)->ID()->Data()) != NULL) return (int)idx;
	}
	return -1;
}

uint32 AudioDevices::Channels() 
{ 
	return m_channels->Length; 
}

uint32 AudioDevices::Devices() 
{
	return m_devices; 
}
