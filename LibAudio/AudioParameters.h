#pragma once

namespace LibAudio
{
	public ref class AudioParameters sealed
	{
	public:
		AudioParameters(size_t BufferSize, size_t AnalysisWindowSize, size_t MaxDelay, uint32 AudioThreshold, 
			int StartOffset, bool Interrupt, bool StoreSample, Platform::String^ FileName, 
			uint32 StoreThreshold, uint32 StoreExtra);

		AudioParameters(size_t BufferSize, uint32 AudioThreshold, bool SilenceCheck, bool Interrupt);

		inline bool DataOnly();

		size_t BufferSize();
		size_t TDEWindow();
		size_t DelayWindow();
		int StartOffset();

		uint32 AudioThreshold();

		bool Interrupt();
		bool SilenceCheck();
		bool StoreSample();

		Platform::String^ SampleFile();

		uint32 StoreThreshold();
		uint32 StoreExtra();

	private:
		bool m_dataOnly;

		size_t m_bufferSize;
		size_t m_tdeWindow;
		size_t m_delayWindow;

		uint32 m_audioThreshold;

		uint32 m_startOffset;

		bool m_interrupt;
		bool m_storeSample;
		bool m_silenceCheck;

		Platform::String^ m_sampleFile;

		uint32 m_storeThreshold;
		uint32 m_storeExtra;
	};
}
