#include "pch.h"
#include "TDEAnalyzer.h"

using namespace Windows::Storage;
using namespace Windows::System::Threading;
using namespace Platform;
using namespace LibAudio;
using namespace concurrency;
using namespace TimeDelayEstimation;

TDEAnalyzer::TDEAnalyzer(HeartBeatSender^ sender) : m_sender(sender) 
{
	StoreTask = nullptr;
}

void TDEAnalyzer::ProcessData(
	size_t numberOfDevices, 
	AudioDevices^ audioDevices, 
	AudioParameters^ audioParameters, 
	const std::vector<DeviceInfo>& deviceInfo, 
	AudioBuffer* audioData,
	size_t smallestBuffer)
{
	UINT64 latestBegin = (*audioData)[audioDevices->Device(0)][audioDevices->Channel(0)][0].timestamp;

	for (size_t i = 0; i < numberOfDevices; i++)
	{
		for (size_t j = 0; j < deviceInfo[i].GetChannels(); j++)
		{
			if ((*audioData)[i][j][0].timestamp > latestBegin) latestBegin = (*audioData)[i][j][0].timestamp;
		}
	}

	bool sample = false;
	size_t pos = 0, sample_pos = 0;
	uint32 threshold = audioParameters->AudioThreshold();
	uint32 threshold0 = audioParameters->AudioThreshold() / 2;

	while (1)
	{
		if ((*audioData)[audioDevices->Device(0)][audioDevices->Channel(0)][pos].timestamp < latestBegin) pos++; else break;
		if (pos == (*audioData)[audioDevices->Device(0)][audioDevices->Channel(0)].size()) break;
	}

	ULONG64 sum = 0;
	uint32 idx1 = audioDevices->Device(0);
	uint32 idx2 = audioDevices->Channel(0);

	uint32 maximum = 0;

	while (pos < (*audioData)[audioDevices->Device(0)][audioDevices->Channel(0)].size())
	{
		uint32 val = abs((*audioData)[idx1][idx2][pos].value);
		if (val > maximum) maximum = val;

		sum += val;
		if (val > threshold)
		{
			sample_pos = pos;
			sample = true;
			while (val > threshold)
			{
				threshold0 = threshold;
				threshold *= 2;
			}
		}
		pos++;
	}
	uint32 average = (uint32)(sum / (ULONG64)(*audioData)[audioDevices->Device(0)][audioDevices->Channel(0)].size());

	UINT64 fts = (*audioData)[audioDevices->Device(0)][audioDevices->Channel(0)][0].timestamp;
	UINT64 lts = (*audioData)[audioDevices->Device(0)][audioDevices->Channel(0)][(*audioData)[audioDevices->Device(0)][audioDevices->Channel(0)].size() - 1].timestamp;

	if (sample) // Calculate direction 
	{
		if (!CalculateTDE(min(smallestBuffer - (audioParameters->DelayWindow() + audioParameters->TDEWindow()), max(audioParameters->DelayWindow(), sample_pos + audioParameters->StartOffset())), 
			average, maximum, fts, lts, numberOfDevices, audioDevices, audioParameters, deviceInfo, *audioData, smallestBuffer))
		{
			// Invalid data for direction calculation
			m_sender->HeartBeat(0, HeartBeatType::INVALID, fts, lts, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, maximum, 0, 0, 0, average);
		}
	}
	else m_sender->HeartBeat(1000, HeartBeatType::SILENCE, fts, lts, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, maximum, 0, 0, 0, average);

	FlushAudioData(numberOfDevices, deviceInfo, audioData);
}

bool TDEAnalyzer::CalculatePair(AudioParameters^ audioParameters, SignalData& data, int& delay1, int& delay2, int& delay3, CalcType& max0, CalcType& max1, CalcType& ave0, CalcType& ave1, DelayType& align)
{
	DelayType align0, align1;

	if (!data.CalculateAlignment(data.First(), &align0, NULL)) return false;
	if (!data.CalculateAlignment(data.Last(), &align1, NULL)) return false;

	align = (align0 + align1) / 2;
	data.SetAlignment(align);

	TimeDelayEstimation::TDE tde(audioParameters->DelayWindow(), data);

	delay1 = tde.FindDelay(TimeDelayEstimation::Algorithm::CC);
	delay2 = tde.FindDelay(TimeDelayEstimation::Algorithm::ASDF);
	delay3 = tde.FindDelay(TimeDelayEstimation::Algorithm::PEAK);
	//DelayType delay4 = tde.FindDelay(TimeDelayEstimation::Algorithm::PHAT);

	tde.SampleInfo(max0, max1, ave0, ave1);

	return true;
}

bool TDEAnalyzer::CalculateTDE(size_t pos, uint32 average, uint32 maximum, UINT64 fts, UINT64 lts,
	size_t numberOfDevices,
	AudioDevices^ audioDevices,
	AudioParameters^ audioParameters,
	const std::vector<DeviceInfo>& deviceInfo,
	const AudioBuffer& audioData,
	size_t smallestBuffer)
{
	if (audioDevices->Channels() == 3)
	{
		return CalculateTDE3(pos, average, maximum, fts, lts, numberOfDevices,
			audioDevices, audioParameters, deviceInfo, audioData, smallestBuffer);
	}
	else return CalculateTDE2(pos, average, maximum, fts, lts, numberOfDevices, 
		audioDevices, audioParameters, deviceInfo, audioData, smallestBuffer);
}

bool TDEAnalyzer::CalculateTDE2(size_t pos, uint32 average, uint32 maximum, UINT64 fts, UINT64 lts,
	size_t numberOfDevices,
	AudioDevices^ audioDevices,
	AudioParameters^ audioParameters,
	const std::vector<DeviceInfo>& deviceInfo,
	const AudioBuffer& audioData,
	size_t smallestBuffer)
{
	int delay[3];
	CalcType max[2];
	CalcType ave[2];
	DelayType align;

	SignalData data0 = SignalData(&audioData[audioDevices->Device(0)][audioDevices->Channel(0)], &audioData[audioDevices->Device(1)][audioDevices->Channel(1)], pos, pos + audioParameters->TDEWindow(), false);

	bool b0 = CalculatePair(audioParameters, data0, delay[0], delay[1], delay[2], max[0], max[1], ave[0], ave[1], align);

	if (!b0) return false;

	m_sender->HeartBeat(0, HeartBeatType::DATA, fts, lts, delay[0], delay[1], delay[2], (int)align, 0, 0, 0, 0, 0, 0, 0, 0, maximum, (uint32)max[0], (uint32)max[1], 0, average, (uint32)ave[0], (uint32)ave[1], 0, deviceInfo[audioDevices->Device(0)].GetSamplesPerSec());

	if (StoreTask == nullptr && audioParameters->StoreSample() && (max[0] >= audioParameters->StoreThreshold() || max[1] >= audioParameters->StoreThreshold()))
	{
		Windows::Globalization::Calendar^ c = ref new Windows::Globalization::Calendar;
		c->SetToNow();

		Platform::String^ str = "TIME: " + c->YearAsPaddedString(4) + ":" +
			c->MonthAsPaddedNumericString(2) + ":" +
			c->DayAsPaddedString(2) + ":" +
			c->HourAsPaddedString(2) + ":" +
			c->MinuteAsPaddedString(2) + ":" +
			c->SecondAsPaddedString(2) + "\r\n" +

			"MAX: " + maximum + " MAX0: " + max[0] + " MAX1: " + max[1] +
			" AVE: " + average + " AVE0: " + ave[0] + " AVE1: " + ave[1] +
			" POS: " + pos.ToString() + " ALIGN: " + align.ToString() +
			" CC: " + delay[0].ToString() + " ASDF: " + delay[1].ToString() + " PEAK: " + delay[2].ToString() +
			"\r\n";

		for (size_t i = data0.First() - audioParameters->DelayWindow(); i <= data0.Last() + audioParameters->StoreExtra() + audioParameters->DelayWindow(); i++)
		{
			TimeDelayEstimation::AudioDataItem item0, item1;
			data0.DataItem0(i, &item0);
			data0.DataItem1(i + align, &item1, item0.timestamp);

			Platform::String^ s =
				item0.timestamp.ToString() + "\t" + item0.value.ToString() + "\t" +
				item1.timestamp.ToString() + "\t" + item1.value.ToString() + "\r\n";
			str = Platform::String::Concat(str, s);
		}
		StorageTask(str, audioParameters);
	}
	return true;
}

bool TDEAnalyzer::CalculateTDE3(size_t pos, uint32 average, uint32 maximum, UINT64 fts, UINT64 lts,
	size_t numberOfDevices,
	AudioDevices^ audioDevices,
	AudioParameters^ audioParameters,
	const std::vector<DeviceInfo>& deviceInfo,
	const AudioBuffer& audioData,
	size_t smallestBuffer)
{
	int delay[9];
	CalcType max[3];
	CalcType ave[3];
	DelayType align[3];

	SignalData data0 = SignalData(&audioData[audioDevices->Device(0)][audioDevices->Channel(0)], &audioData[audioDevices->Device(1)][audioDevices->Channel(1)], pos, pos + audioParameters->TDEWindow(), false);
	bool b0 = CalculatePair(audioParameters, data0, delay[0], delay[1], delay[2], max[0], max[1], ave[0], ave[1], align[0]);
	if (!b0) return false;

	SignalData data1 = SignalData(&audioData[audioDevices->Device(0)][audioDevices->Channel(0)], &audioData[audioDevices->Device(2)][audioDevices->Channel(2)], pos, pos + audioParameters->TDEWindow(), false);
	SignalData data2 = SignalData(&audioData[audioDevices->Device(1)][audioDevices->Channel(1)], &audioData[audioDevices->Device(2)][audioDevices->Channel(2)], pos + align[0], pos + align[0] + audioParameters->TDEWindow(), false);

	bool b1 = CalculatePair(audioParameters, data1, delay[3], delay[4], delay[5], max[0], max[2], ave[0], ave[2], align[1]);
	bool b2 = CalculatePair(audioParameters, data2, delay[6], delay[7], delay[8], max[1], max[2], ave[1], ave[2], align[2]);

	if (!b0 || !b1 || !b2) return false;

	m_sender->HeartBeat(0, HeartBeatType::DATA, fts, lts,
		delay[0], delay[1], delay[2], (int)align[0],
		delay[3], delay[4], delay[5], (int)align[1],
		delay[6], delay[7], delay[8], (int)align[2],
		maximum, (uint32)max[0], (uint32)max[1], (uint32)max[2],
		average, (uint32)ave[0], (uint32)ave[1], (uint32)ave[2],
		deviceInfo[audioDevices->Device(0)].GetSamplesPerSec());

	if (StoreTask == nullptr && audioParameters->StoreSample() && (max[0] >= audioParameters->StoreThreshold() || max[1] >= audioParameters->StoreThreshold() || max[2] >= audioParameters->StoreThreshold()))
	{
		Windows::Globalization::Calendar^ c = ref new Windows::Globalization::Calendar;
		c->SetToNow();

		Platform::String^ str = "TIME: " + c->YearAsPaddedString(4) + ":" +
			c->MonthAsPaddedNumericString(2) + ":" +
			c->DayAsPaddedString(2) + ":" +
			c->HourAsPaddedString(2) + ":" +
			c->MinuteAsPaddedString(2) + ":" +
			c->SecondAsPaddedString(2) + "\r\n" +

			"MAX: " + maximum + " MAX0: " + max[0] + " MAX1: " + max[1] + " MAX2: " + max[2] +
			" AVE: " + average + " AVE0: " + ave[0] + " AVE1: " + ave[1] + " AVE2: " + ave[2] +
			" POS: " + pos.ToString() + " ALIGN0: " + align[0].ToString() + " ALIGN1: " + align[1].ToString() + " ALIGN2: " + align[2].ToString() +
			" CC0: " + delay[0].ToString() + " ASDF0: " + delay[1].ToString() + " PEAK0: " + delay[2].ToString() +
			" CC1: " + delay[3].ToString() + " ASDF1: " + delay[4].ToString() + " PEAK1: " + delay[5].ToString() +
			" CC2: " + delay[6].ToString() + " ASDF2: " + delay[7].ToString() + " PEAK2: " + delay[8].ToString() +
			"\r\n";

		for (size_t i = data0.First() - audioParameters->DelayWindow(); i <= data0.Last() + audioParameters->StoreExtra() + audioParameters->DelayWindow(); i++)
		{
			TimeDelayEstimation::AudioDataItem item0, item1, item2, item3;
			data0.DataItem0(i, &item0);
			data0.DataItem1(i + align[0], &item1, item0.timestamp);
			data1.DataItem0(i, &item2);
			data1.DataItem1(i + align[1], &item3, item2.timestamp);

			Platform::String^ s =
				item0.timestamp.ToString() + "\t" + item0.value.ToString() + "\t" +
				item1.timestamp.ToString() + "\t" + item1.value.ToString() + "\t" +
				item3.timestamp.ToString() + "\t" + item3.value.ToString() + "\t" +
				item2.timestamp.ToString() + "\t" + item2.value.ToString() + "\r\n";

			str = Platform::String::Concat(str, s);
		}
		StorageTask(str, audioParameters);
	}
	return true;
}

void TDEAnalyzer::StorageTask(String^ data, AudioParameters^ audioParameters)
{
	auto workItemDelegate = [this, data, audioParameters](Windows::Foundation::IAsyncAction^ action)
	{
		String^ str = data;
		auto store = StoreData(str, audioParameters);
		store.wait();
	};

	auto completionDelegate = [this](Windows::Foundation::IAsyncAction^ action, Windows::Foundation::AsyncStatus status)
	{
		switch (action->Status)
		{
		case Windows::Foundation::AsyncStatus::Completed:
		case Windows::Foundation::AsyncStatus::Error:
		case Windows::Foundation::AsyncStatus::Canceled: StoreTask = nullptr;
		}
	};

	auto workItemHandler = ref new Windows::System::Threading::WorkItemHandler(workItemDelegate);
	auto completionHandler = ref new Windows::Foundation::AsyncActionCompletedHandler(completionDelegate, Platform::CallbackContext::Same);

	StoreTask = Windows::System::Threading::ThreadPool::RunAsync(workItemHandler, Windows::System::Threading::WorkItemPriority::Low);
	StoreTask->Completed = completionHandler;
}

concurrency::task<Windows::Storage::StorageFile^> TDEAnalyzer::StoreData(String^ data, AudioParameters^ audioParameters)
{
	StorageFolder^ localFolder = ApplicationData::Current->LocalFolder;
	auto createFileTask = create_task(localFolder->CreateFileAsync(audioParameters->SampleFile(), Windows::Storage::CreationCollisionOption::GenerateUniqueName));
	createFileTask.then([data](StorageFile^ newFile)
	{
		auto append = create_task(FileIO::WriteTextAsync(newFile, data));
		append.wait();
	});
	return createFileTask;
}
