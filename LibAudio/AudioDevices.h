#pragma once

namespace LibAudio
{
	public ref class AudioChannel sealed
	{
	public:
		AudioChannel(uint32 device, uint32 channel, Platform::String^ id, uint32 frequency) : 
			m_device(device), m_channel(channel), m_id(id), m_frequency(frequency) {}
	
		uint32 Device() { return m_device; }
		uint32 Channel() { return m_channel; }
		uint32 Frequency() { return m_frequency; }
		Platform::String^ ID() { return m_id; }

	private:
		Platform::String^ m_id;
		uint32 m_device;
		uint32 m_channel;
		uint32 m_frequency;
	};

	public ref class AudioDevices sealed
	{
	public:
		AudioDevices(uint32 devices);
		AudioDevices(const Platform::Array<AudioChannel^>^ channels, uint32 devices);

		inline uint32 Channels();

		uint32 Devices();

		inline uint32 Device(uint32 index);
		inline uint32 Channel(uint32 index);
		inline int GetIndex(Platform::String^ id, size_t index);
		inline uint32 GetFrequency(Platform::String^ id);

	private:
		uint32 m_devices;
		Platform::Array<AudioChannel^>^ m_channels;
	};
}
