#include "pch.h"
#include "DataAnalyzer.h"

using namespace Windows::Storage;
using namespace Windows::System::Threading;
using namespace Platform;
using namespace LibAudio;

DataAnalyzer::DataAnalyzer(HeartBeatSender^ sender) : m_sender(sender) {}

void DataAnalyzer::ProcessData(
	size_t numberOfDevices,
	AudioDevices^ audioDevices,
	AudioParameters^ audioParameters,
	const std::vector<DeviceInfo>& deviceInfo,
	AudioBuffer* audioData,
	size_t smallestBuffer)
{
	FlushAudioData(numberOfDevices, deviceInfo, audioData);
}

void DataAnalyzer::ProcessData(
	uint32 beginR,
	uint32 endR, 
	uint32 beginL, 
	uint32 endL, 
	const Array<int32>^ memoryR, 
	const Array<int32>^ memoryL,
	const Array<UINT64>^ timeStampR, 
	const Array<UINT64>^ timeStampL)
{
	m_sender->HeartBeat(0, HeartBeatType::DATA, beginR, endR, beginL, endL, memoryR, memoryL, timeStampR, timeStampL);
}
