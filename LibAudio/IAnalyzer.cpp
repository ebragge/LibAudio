#include "pch.h"
#include "IAnalyzer.h"

using namespace LibAudio;

void IAnalyzer::FlushAudioData(size_t numberOfDevices, const std::vector<DeviceInfo>& deviceInfo, AudioBuffer* audioData)
{
	for (size_t i = 0; i < numberOfDevices; i++)
	{
		if ((*audioData)[i].size() != 0)
		{
			for (size_t j = 0; j < deviceInfo[i].GetChannels(); j++)
			{
				(*audioData)[i][j].clear();
			}
			(*audioData)[i].clear();
		}
	}
	audioData->clear();
	delete audioData;
}
