#include "pch.h"
#include "DataConsumer.h"

using namespace Windows::Storage;
using namespace Windows::System::Threading;
using namespace Platform;
using namespace LibAudio;
using namespace concurrency;
using namespace TimeDelayEstimation;

DataConsumer::DataConsumer(size_t nDevices, DataCollector^ collector, UIDelegate^ func, TDEDevices^ devices, TDEParameters^ parameters) :
	m_numberOfDevices(nDevices),
	m_collector(collector),
	m_uiHandler(func),
	m_tick(0),
	m_packetCounter(0),
	m_discontinuityCounter(0),
	m_dataRemovalCounter(0),
	m_devParams(devices),
	m_params(parameters),
	m_delayTimer(nullptr)
{
	m_devices = std::vector<DeviceInfo>(m_numberOfDevices);
	m_audioDataFirst = std::vector<AudioDataPacket*>(m_numberOfDevices, NULL);
	m_audioDataLast = std::vector<AudioDataPacket*>(m_numberOfDevices, NULL);

	m_buffer = std::vector<std::vector<std::vector<TimeDelayEstimation::AudioDataItem>>>(m_numberOfDevices);
}

DataConsumer::~DataConsumer()
{
	Stop();
	FlushBuffer();
	FlushPackets();
}

void DataConsumer::Start()
{
	TimeSpan delay;
	delay.Duration = 1000000; // 0.1 s	(10,000,000 ticks per second)

	m_delayTimer = ThreadPoolTimer::CreatePeriodicTimer(
		ref new TimerElapsedHandler([this](ThreadPoolTimer^ source)
		{
			if (Task == nullptr)
			{
				AudioTask();
			}
		}
	), delay);
}

void DataConsumer::Stop()
{
	if (m_delayTimer != nullptr)
	{
		m_delayTimer->Cancel();
		if (Task != nullptr) Task->Cancel();

		Task = nullptr;
		m_delayTimer = nullptr;
	}
}

HRESULT DataConsumer::Finish()
{
	Stop();
	return S_OK;
}

void DataConsumer::AudioTask()
{
	auto workItemDelegate = [this](Windows::Foundation::IAsyncAction^ action) 
	{ 
		bool error = false;

		for (size_t i = 0; i < m_numberOfDevices; i++)
		{
			AudioDataPacket *first, *last;
			size_t count;

			m_devices[i] = m_collector->RemoveData(i, &first, &last, &count, &error);
			m_packetCounter += count;

			if (m_audioDataLast[i] != NULL)
			{
				m_audioDataLast[i]->SetNext(first);
				if (last != NULL) m_audioDataLast[i] = last;
			}
			else
			{
				m_audioDataFirst[i] = first;
				m_audioDataLast[i] = last;
			}
		}
		if (error) HeartBeat(0, HeartBeatType::DEVICE_ERROR);
		bool loop = true;
		bool msg = false;
		while (loop)
		{
			switch (HandlePackets())
			{
			case Status::ONLY_ONE_SAMPLE:
				loop = false;
				break;
			case Status::EXCESS_DATA:
			case Status::SILENCE:
				m_dataRemovalCounter++;
				break;
			case Status::DISCONTINUITY:
				FlushBuffer();
				m_discontinuityCounter++;
				break;
			case Status::DATA_AVAILABLE:
				loop = ProcessData(msg);
				break;
			}
			if (action->Status == Windows::Foundation::AsyncStatus::Canceled) break;
		}
		if (!msg) HeartBeat(2000, HeartBeatType::BUFFERING, m_devices[m_devParams->Device0()].GetPosition(), m_devices[m_devParams->Device1()].GetPosition(), (int)m_packetCounter, (int)m_discontinuityCounter, (int)m_dataRemovalCounter);
	};

	auto completionDelegate = [this](Windows::Foundation::IAsyncAction^ action, Windows::Foundation::AsyncStatus status)
	{
		switch (action->Status)
		{
		case Windows::Foundation::AsyncStatus::Completed:
		case Windows::Foundation::AsyncStatus::Error:
		case Windows::Foundation::AsyncStatus::Canceled: Task = nullptr;
		}
	};

	auto workItemHandler = ref new Windows::System::Threading::WorkItemHandler(workItemDelegate);
	auto completionHandler = ref new Windows::Foundation::AsyncActionCompletedHandler(completionDelegate, Platform::CallbackContext::Same);

	Task = Windows::System::Threading::ThreadPool::RunAsync(workItemHandler, Windows::System::Threading::WorkItemPriority::Low);
	Task->Completed = completionHandler;
}

DataConsumer::Status DataConsumer::HandlePackets()
{
	for (size_t i = 0; i < m_numberOfDevices; i++)
	{
		// Only one data item
		if (m_audioDataFirst[i] == m_audioDataLast[i]) return Status::ONLY_ONE_SAMPLE;
	}
	UINT64* pos1 = new UINT64[m_numberOfDevices];
	UINT64* pos2 = new UINT64[m_numberOfDevices];
	UINT64 maxPos1 = 0;

	for (size_t i = 0; i < m_numberOfDevices; i++)
	{
		// Find latest starting point
		pos1[i] = m_audioDataFirst[i]->Position();
		pos2[i] = m_audioDataFirst[i]->Next()->Position();
		if (pos1[i] > maxPos1) maxPos1 = pos1[i];
	}

	bool removedData = false;

	for (size_t i = 0; i < m_numberOfDevices; i++)
	{
		// Remove data before latest starting point
		if (pos2[i] < maxPos1)
		{
			AudioDataPacket* packet = m_audioDataFirst[i];
			m_audioDataFirst[i] = packet->Next();
			delete packet;
			removedData = true;
		}
	}
	delete [] pos1;
	delete [] pos2;
	if (removedData) return Status::EXCESS_DATA;

	for (size_t i = 0; i < m_numberOfDevices; i++)
	{
		// Remove data before discontinuity
		if (m_audioDataFirst[i]->Discontinuity() || m_audioDataFirst[i]->Next()->Discontinuity())
		{
			AudioDataPacket* packet = m_audioDataFirst[i];
			m_audioDataFirst[i] = packet->Next();
			delete packet;
			removedData = true;
		}
	}
	if (removedData) return Status::DISCONTINUITY;

	for (size_t i = 0; i < m_numberOfDevices; i++)
	{
		// Remove silent data
		if (m_audioDataFirst[i]->Silence())
		{
			AudioDataPacket* packet = m_audioDataFirst[i];
			m_audioDataFirst[i] = packet->Next();
			delete packet;
			removedData = true;
		}
	}
	if (removedData) return Status::SILENCE;

	for (size_t i = 0; i < m_numberOfDevices; i++)
	{
		AudioDataPacket* packet = m_audioDataFirst[i];
		m_audioDataFirst[i] = packet->Next();
		AddData(i, packet->Bytes(), packet->Data(), packet->Position(), packet->Next()->Position());
		delete packet;
	}
	return Status::DATA_AVAILABLE;
}

bool DataConsumer::ProcessData(bool& msg)
{
	size_t smallestBuffer = m_buffer[m_devParams->Device0()][m_devParams->Channel0()].size();
	for (size_t i = 0; i < m_numberOfDevices; i++)
	{
		for (size_t j = 0; j < m_devices[i].GetChannels(); j++)
		{
			if (m_buffer[i][j].size() < smallestBuffer) smallestBuffer = m_buffer[i][j].size();
		}
	}

	if (smallestBuffer > m_params->BufferSize())
	{
		if (m_params->Interrupt()) m_collector->StoreData(false);
		UINT64 latestBegin = m_buffer[m_devParams->Device0()][m_devParams->Channel0()][0].timestamp;

		for (size_t i = 0; i < m_numberOfDevices; i++)
		{
			for (size_t j = 0; j < m_devices[i].GetChannels(); j++)
			{
				if (m_buffer[i][j][0].timestamp > latestBegin) latestBegin = m_buffer[i][j][0].timestamp;
			}
		}

		bool sample = false;
		size_t pos = 0, sample_pos = 0;
		uint32 threshold = m_params->AudioThreshold();
		uint32 threshold0 = m_params->AudioThreshold()/2;

		while (1) 
		{ 
			if (m_buffer[m_devParams->Device0()][m_devParams->Channel0()][pos].timestamp < latestBegin) pos++; else break;
			if (pos == m_buffer[m_devParams->Device0()][m_devParams->Channel0()].size()) break;
		}

		ULONG64 sum = 0;
		uint32 idx1 = m_devParams->Device0();
		uint32 idx2 = m_devParams->Channel0();

		uint32 maximum = 0;

		while (pos < m_buffer[m_devParams->Device0()][m_devParams->Channel0()].size())
		{
			uint32 val = abs(m_buffer[idx1][idx2][pos].value);
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
		uint32 average = (uint32)(sum / (ULONG64)m_buffer[m_devParams->Device0()][m_devParams->Channel0()].size());

		UINT64 fts = m_buffer[m_devParams->Device0()][m_devParams->Channel0()][0].timestamp;
		UINT64 lts = m_buffer[m_devParams->Device0()][m_devParams->Channel0()][m_buffer[m_devParams->Device0()][m_devParams->Channel0()].size() - 1].timestamp;

		if (sample) // Calculate direction 
		{
			if (!CalculateTDE(min(smallestBuffer-(m_params->DelayWindow()+m_params->TDEWindow()),max(m_params->DelayWindow(),sample_pos + m_params->StartOffset())), average, maximum, fts, lts))
			{
				// Invalid data for direction calculation
				HeartBeat(0, HeartBeatType::INVALID, fts, lts, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, maximum, 0, 0, 0, average);
			}
		}
		else HeartBeat(0, HeartBeatType::SILENCE, fts, lts, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, maximum, 0, 0, 0, average);
			
		FlushBuffer();

		if (m_params->Interrupt())
		{
			FlushCollector();
			m_collector->StoreData(true);
		}
		msg = true;
		return false;
	}
	return true;
}

void DataConsumer::FlushPackets()
{
	for (size_t i = 0; i < m_numberOfDevices; i++)
	{
		while (m_audioDataFirst[i] != NULL) 
		{ 
			AudioDataPacket* ptr = m_audioDataFirst[i];
			m_audioDataFirst[i] = ptr->Next();
			delete ptr; 
		}
		m_audioDataLast[i] = NULL;
	}
}

void DataConsumer::FlushBuffer()
{
	for (size_t i = 0; i < m_numberOfDevices; i++)
	{
		if (m_buffer[i].size() != 0)
		{
			for (size_t j = 0; j < m_devices[i].GetChannels(); j++)
			{
				m_buffer[i][j].clear();
			}
		}
	}
}

void DataConsumer::FlushCollector()
{
	for (size_t i = 0; i < m_numberOfDevices; i++)
	{
		AudioDataPacket *first, *last;
		size_t count;
		bool error;
		m_devices[i] = m_collector->RemoveData(i, &first, &last, &count, &error);
		while (first != NULL) 
		{	
			AudioDataPacket* ptr = first;
			first = ptr->Next(); 
			delete ptr; 
		}
	}
}

void DataConsumer::AddData(size_t device, DWORD cbBytes, const BYTE* pData, UINT64 pos1, UINT64 pos2)
{
	DWORD numPoints = cbBytes / (DWORD)(m_devices[device].GetChannels() * m_devices[device].GetBytesPerSample());
	INT16 *pi16 = (INT16*)pData;
	UINT64 delta = pos2 - pos1;
	double d = (double)delta / numPoints;

	size_t sz = m_devices[device].GetChannels();

	if (m_buffer[device].size() == 0)
	{		
		for (size_t ii = 0; ii < sz; ii++)
		{
			m_buffer[device].push_back(std::vector<TimeDelayEstimation::AudioDataItem>(2 * m_params->BufferSize()));
		}
	}
	for (DWORD i = 0; i < numPoints; i++)
	{
		UINT64 time_delta = UINT64((double)i * d);
		for (size_t j = 0; j < sz; j++)
		{
			TimeDelayEstimation::AudioDataItem item(*pi16, pos1 + time_delta, i);
			m_buffer[device][j].push_back(item);
			pi16++;
		}
	}
}

bool DataConsumer::CalculatePair(SignalData& data, int& delay1, int& delay2, int& delay3, CalcType& max0, CalcType& max1, CalcType& ave0, CalcType& ave1, DelayType& align)
{
	DelayType align0, align1;

	if (!data.CalculateAlignment(data.First(), &align0, NULL)) return false;
	if (!data.CalculateAlignment(data.Last(), &align1, NULL)) return false;

	align = (align0 + align1) / 2;
	data.SetAlignment(align);

	TimeDelayEstimation::TDE tde(m_params->DelayWindow(), data);

	delay1 = tde.FindDelay(TimeDelayEstimation::Algorithm::CC);
	delay2 = tde.FindDelay(TimeDelayEstimation::Algorithm::ASDF);
	delay3 = tde.FindDelay(TimeDelayEstimation::Algorithm::PEAK);

	tde.SampleInfo(max0, max1, ave0, ave1);

	return true;
}

bool DataConsumer::CalculateTDE(size_t pos, uint32 average, uint32 maximum, UINT64 fts, UINT64 lts)
{
	if (m_devParams->MinDevices() == 3)
	{
		return CalculateTDE3(pos, average, maximum, fts, lts);
	}
	else return CalculateTDE2(pos, average, maximum, fts, lts);
}

bool DataConsumer::CalculateTDE2(size_t pos, uint32 average, uint32 maximum, UINT64 fts, UINT64 lts)
{
	int delay[3];
	CalcType max[2];
	CalcType ave[2];
	DelayType align;

	SignalData data0 = SignalData(&m_buffer[m_devParams->Device0()][m_devParams->Channel0()], &m_buffer[m_devParams->Device1()][m_devParams->Channel1()], pos, pos + m_params->TDEWindow(), false);

	bool b0 = CalculatePair(data0, delay[0], delay[1], delay[2], max[0], max[1], ave[0], ave[1], align);

	if (!b0) return false;

	HeartBeat(0, HeartBeatType::DATA, fts, lts, delay[0], delay[1], delay[2], (int)align, 0, 0, 0, 0, 0, 0, 0, 0, maximum, (uint32)max[0], (uint32)max[1], 0, average, (uint32)ave[0], (uint32)ave[1], 0, m_devices[m_devParams->Device0()].GetSamplesPerSec());

	if (StoreTask == nullptr && m_params->StoreSample() && (max[0] >= m_params->StoreThreshold() || max[1] >= m_params->StoreThreshold()))
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

		for (size_t i = data0.First() - m_params->DelayWindow(); i <= data0.Last() + m_params->StoreExtra() + m_params->DelayWindow(); i++)
		{
			TimeDelayEstimation::AudioDataItem item0, item1;
			data0.DataItem0(i, &item0);
			data0.DataItem1(i + align, &item1, item0.timestamp);

			Platform::String^ s =
				item0.timestamp.ToString() + "\t" + item0.value.ToString() + "\t" +
				item1.timestamp.ToString() + "\t" + item1.value.ToString() + "\r\n";
			str = Platform::String::Concat(str, s);
		}
		StorageTask(str);
	}
	return true;
}

bool DataConsumer::CalculateTDE3(size_t pos, uint32 average, uint32 maximum, UINT64 fts, UINT64 lts)
{
	int delay[9];
	CalcType max[3];
	CalcType ave[3];
	DelayType align[3];

	SignalData data0 = SignalData(&m_buffer[m_devParams->Device0()][m_devParams->Channel0()], &m_buffer[m_devParams->Device1()][m_devParams->Channel1()], pos, pos + m_params->TDEWindow(), false);
	bool b0 = CalculatePair(data0, delay[0], delay[1], delay[2], max[0], max[1], ave[0], ave[1], align[0]);
	if (!b0) return false;

	SignalData data1 = SignalData(&m_buffer[m_devParams->Device0()][m_devParams->Channel0()], &m_buffer[m_devParams->Device2()][m_devParams->Channel2()], pos, pos + m_params->TDEWindow(), false);
	SignalData data2 = SignalData(&m_buffer[m_devParams->Device1()][m_devParams->Channel1()], &m_buffer[m_devParams->Device2()][m_devParams->Channel2()], pos + align[0], pos + align[0] + m_params->TDEWindow(), false);
	
	bool b1 = CalculatePair(data1, delay[3], delay[4], delay[5], max[0], max[2], ave[0], ave[2], align[1]);
	bool b2 = CalculatePair(data2, delay[6], delay[7], delay[8], max[1], max[2], ave[1], ave[2], align[2]);

	if (!b0 || !b1 || !b2) return false;

	HeartBeat(0, HeartBeatType::DATA, fts, lts, 
		delay[0], delay[1], delay[2], (int)align[0], 
		delay[3], delay[4], delay[5], (int)align[1], 
		delay[6], delay[7], delay[8], (int)align[2], 
		maximum, (uint32)max[0], (uint32)max[1], (uint32)max[2], 
		average, (uint32)ave[0], (uint32)ave[1], (uint32)ave[2], 
		m_devices[m_devParams->Device0()].GetSamplesPerSec());

	if (StoreTask == nullptr && m_params->StoreSample() && (max[0] >= m_params->StoreThreshold() || max[1] >= m_params->StoreThreshold() || max[2] >= m_params->StoreThreshold()))
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

		for (size_t i = data0.First() - m_params->DelayWindow(); i <= data0.Last() + m_params->StoreExtra() + m_params->DelayWindow(); i++)
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
		StorageTask(str);
	}
	return true;
}

void DataConsumer::HeartBeat(int delta, HeartBeatType status, UINT64 fts, UINT64 lts,
	int i0, int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8, int i9, int i10, int i11,
	uint32 ui0, uint32 ui1, uint32 ui2, uint32 ui3, uint32 ui4, uint32 ui5, uint32 ui6, uint32 ui7, uint32 ui8)
{
	ULONGLONG tick = GetTickCount64();
	if (tick - m_tick > delta)
	{
		m_uiHandler(status, fts, lts, i0, i1, i2, i3, i4, i5, i6, i7, i8, i9, i10, i11,	ui0, ui1, ui2, ui3, ui4, ui5, ui6, ui7, ui8);
		m_tick = tick;
		m_packetCounter = 0;
		m_discontinuityCounter = 0;
		m_dataRemovalCounter = 0;
	}
}

void DataConsumer::StorageTask(String^ data)
{	
	auto workItemDelegate = [this,data](Windows::Foundation::IAsyncAction^ action)
	{
		String^ str = data;
		auto store = StoreData(str);
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

concurrency::task<Windows::Storage::StorageFile^> DataConsumer::StoreData(String^ data)
{
	StorageFolder^ localFolder = ApplicationData::Current->LocalFolder;
	auto createFileTask = create_task(localFolder->CreateFileAsync(m_params->SampleFile(), Windows::Storage::CreationCollisionOption::GenerateUniqueName));
	createFileTask.then([data](StorageFile^ newFile)
	{
		auto append = create_task(FileIO::WriteTextAsync(newFile, data));
		append.wait();
	});
	return createFileTask;
}
