#include "pch.h"
#include "AudioParameters.h"

using namespace LibAudio;

AudioParameters::AudioParameters(size_t BufferSize, size_t AnalysisWindowSize, size_t MaxDelay,
	uint32 AudioThreshold, int StartOffset, bool Interrupt,
	bool StoreSample, Platform::String^ FileName, uint32 StoreThreshold, uint32 StoreExtra) :
	m_bufferSize(BufferSize),
	m_tdeWindow(AnalysisWindowSize),
	m_delayWindow(MaxDelay),
	m_audioThreshold(AudioThreshold),
	m_startOffset(StartOffset),
	m_interrupt(Interrupt),
	m_storeSample(StoreSample),
	m_sampleFile(FileName),
	m_storeThreshold(StoreThreshold),
	m_storeExtra(StoreExtra),
	m_silenceCheck(true),
	m_dataOnly(false)
{}

AudioParameters::AudioParameters(size_t BufferSize, uint32 AudioThreshold, bool SilenceCheck, bool Interrupt) :
	m_bufferSize(BufferSize),
	m_audioThreshold(AudioThreshold),
	m_silenceCheck(SilenceCheck),
	m_interrupt(Interrupt),
	m_dataOnly(true)
{}

bool AudioParameters::DataOnly() { return m_dataOnly;  }

size_t AudioParameters::BufferSize() { return m_bufferSize; }
size_t AudioParameters::TDEWindow() { return m_tdeWindow; }
size_t AudioParameters::DelayWindow() { return m_delayWindow; }
int AudioParameters::StartOffset() { return m_startOffset; }

uint32 AudioParameters::AudioThreshold() { return m_audioThreshold; }

bool AudioParameters::Interrupt() { return m_interrupt; }
bool AudioParameters::StoreSample() { return m_storeSample; }
bool AudioParameters::SilenceCheck() { return m_silenceCheck; }

Platform::String^ AudioParameters::SampleFile() { return m_sampleFile; }

uint32 AudioParameters::StoreThreshold() { return m_storeThreshold; }
uint32 AudioParameters::StoreExtra() { return m_storeExtra; }
