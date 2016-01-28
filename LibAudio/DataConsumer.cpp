#include "pch.h"
#include "DataConsumer.h"
#include "TDEAnalyzer.h"
#include "DataAnalyzer.h"

using namespace Windows::System::Threading;
using namespace Platform;
using namespace LibAudio;
using namespace concurrency;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Platform;

DataConsumer::DataConsumer(size_t nDevices, DataCollector^ collector, UIDelegate1^ func1, UIDelegate2^ func2, AudioDevices^ devices, AudioParameters^ parameters) :
	m_numberOfDevices(nDevices),
	m_collector(collector),
	m_devParams(devices),
	m_params(parameters),
	m_delayTimer(nullptr),
	m_running(true)
{
	m_devices = std::vector<DeviceInfo>(m_numberOfDevices);
	m_audioDataFirst = std::vector<AudioDataPacket*>(m_numberOfDevices, NULL);
	m_audioDataLast = std::vector<AudioDataPacket*>(m_numberOfDevices, NULL);

	m_buffer = new std::vector<std::vector<std::vector<TimeDelayEstimation::AudioDataItem>>>(m_numberOfDevices);

	for (int i = 0; i < MAX_TASKS; i++)
	{
		m_processTasks[i] = nullptr;
	}
	m_silenceCheck = m_params->SilenceCheck();

	if (parameters->DataOnly())
	{
		m_sender = ref new HeartBeatSender(func2);
		m_memoryR = ref new Array<int32>(TRANSFER_BUFFER);
		m_timestampR = ref new Array<UINT64>(TRANSFER_BUFFER);

		if (devices->Channels() > 1)
		{
			m_memoryL = ref new Array<int32>(TRANSFER_BUFFER);
			m_timestampL = ref new Array<UINT64>(TRANSFER_BUFFER);
		}
		
		m_analyzer = new DataAnalyzer(m_sender);
	}
	else
	{
		m_sender = ref new HeartBeatSender(func1);
		
		m_memoryR = nullptr;
		m_memoryL = nullptr;
		m_timestampR = nullptr;
		m_timestampL = nullptr;

		m_analyzer = new TDEAnalyzer(m_sender);
	}
}

DataConsumer::~DataConsumer()
{
	FlushBuffer();
	FlushPackets();

	delete m_analyzer;
	delete m_buffer;
}

void DataConsumer::Start()
{
	TimeSpan delay;
	delay.Duration = 1000000; // 0.1 s	(10,000,000 ticks per second)

	m_delayTimer = ThreadPoolTimer::CreatePeriodicTimer(
		ref new TimerElapsedHandler([this](ThreadPoolTimer^ source)
		{
			if (Task == nullptr && m_running)
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

		for (int i = 0; i < MAX_TASKS; i++)
		{
			if (m_processTasks[i] != nullptr) m_processTasks[i]->Cancel();
		}
	}
}

void DataConsumer::Continue()
{
	if (!m_running)
	{
		m_running = true;
		FlushCollector();
		m_collector->StoreData(true);
	}
}

HRESULT DataConsumer::Finish()
{
	Stop();
	return S_OK;
}

void DataConsumer::AudioTask()
{
	auto workItemDelegate = [this](IAsyncAction^ action) 
	{ 
		bool error = false;

		for (size_t i = 0; i < m_numberOfDevices; i++)
		{
			AudioDataPacket *first, *last;
			size_t count;

			m_devices[i] = m_collector->RemoveData(i, &first, &last, &count, &error);

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
		if (error) m_sender->HeartBeat(0, HeartBeatType::DEVICE_ERROR);
		bool loop = true;
		bool msg = false;
		
		while (loop)
		{
			switch (HandlePackets(m_silenceCheck))
			{
			case Status::ONLY_ONE_SAMPLE:
				loop = false;
				break;
			case Status::EXCESS_DATA:
			case Status::SILENCE:
			case Status::DISCONTINUITY:
				FlushBuffer();
				break;
			case Status::DATA_AVAILABLE:
				m_silenceCheck = false;
				loop = ProcessData(msg);
				break;
			}
			if (action->Status == AsyncStatus::Canceled) break;
		}
		if (!msg) m_sender->HeartBeat(5000, HeartBeatType::BUFFERING);
	};

	auto completionDelegate = [this](IAsyncAction^ action, Windows::Foundation::AsyncStatus status)
	{
		switch (action->Status)
		{
		case AsyncStatus::Completed:
		case AsyncStatus::Error:
		case AsyncStatus::Canceled: Task = nullptr;
		}
	};

	auto workItemHandler = ref new Windows::System::Threading::WorkItemHandler(workItemDelegate);
	auto completionHandler = ref new Windows::Foundation::AsyncActionCompletedHandler(completionDelegate, Platform::CallbackContext::Same);

	Task = Windows::System::Threading::ThreadPool::RunAsync(workItemHandler, Windows::System::Threading::WorkItemPriority::Normal);
	Task->Completed = completionHandler;
}

DataConsumer::Status DataConsumer::HandlePackets(bool silenceCheck)
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

	bool silence = true;
	for (size_t i = 0; i < m_numberOfDevices; i++)
	{
		AudioDataPacket* packet = m_audioDataFirst[i];
		m_audioDataFirst[i] = packet->Next();
		if (!AddData(i, packet->Bytes(), packet->Data(), packet->Position(), packet->Next()->Position(), silenceCheck))
		{	
			silence = false;
		}
		delete packet;
	}
	if (silence && silenceCheck) return Status::SILENCE;

	return Status::DATA_AVAILABLE;
}

bool DataConsumer::AddData(size_t device, DWORD cbBytes, const BYTE* pData, UINT64 pos1, UINT64 pos2, bool silenceCheck)
{
	bool silence = true;
	DWORD numPoints = cbBytes / (DWORD)(m_devices[device].GetChannels() * m_devices[device].GetBytesPerSample());
	INT16 *pi16 = (INT16*)pData;
	UINT64 delta = pos2 - pos1;
	double d = (double)delta / numPoints;

	size_t sz = m_devices[device].GetChannels();

	if ((*m_buffer)[device].size() == 0)
	{		
		for (size_t ii = 0; ii < sz; ii++)
		{
			(*m_buffer)[device].push_back(std::vector<TimeDelayEstimation::AudioDataItem>(0));
		}
	}
	for (DWORD i = 0; i < numPoints; i++)
	{
		UINT64 time_delta = UINT64((double)i * d);
		for (size_t channel = 0; channel < sz; channel++)
		{
			if (silenceCheck && abs(*pi16) > (int)m_params->AudioThreshold())
			{
				silence = false;
			}
			
			TimeDelayEstimation::AudioDataItem item(*pi16, pos1 + time_delta, i);
			(*m_buffer)[device][channel].push_back(item);
			pi16++;
			
			if (m_params->DataOnly())
			{
				if (device == m_devParams->Device(0) && channel == m_devParams->Channel(0))
				{
					m_memoryR[m_endR] = item.value;
					m_timestampR[m_endR++] = item.timestamp;

					if (m_endR == TRANSFER_BUFFER) m_endR = 0;
				}
				if (m_devParams->Channels() > 1 && device == m_devParams->Device(1) && channel == m_devParams->Channel(1))
				{
					m_memoryL[m_endL] = item.value;
					m_timestampL[m_endL++] = item.timestamp;
					if (m_endL == TRANSFER_BUFFER) m_endL = 0;
				}
			}
		}
	}
	return silenceCheck && silence;
}

bool DataConsumer::ProcessData(bool& msg)
{
	size_t smallestBuffer = (*m_buffer)[m_devParams->Device(0)][m_devParams->Channel(0)].size();
	for (size_t i = 0; i < m_numberOfDevices; i++)
	{
		for (size_t j = 0; j < m_devices[i].GetChannels(); j++)
		{
			if ((*m_buffer)[i][j].size() < smallestBuffer) smallestBuffer = (*m_buffer)[i][j].size();
		}
	}

	if (smallestBuffer > m_params->BufferSize())
	{
		if (m_params->Interrupt())
		{
			m_collector->StoreData(false);
			m_running = false;
		}

		HandleProcessTasks(smallestBuffer);
		m_buffer = new AudioBuffer(m_numberOfDevices);

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
		if ((*m_buffer)[i].size() != 0)
		{
			for (size_t j = 0; j < m_devices[i].GetChannels(); j++)
			{
				(*m_buffer)[i][j].clear();
			}
		}
	}

	m_endR = m_beginR;
	m_endL = m_beginL;

	m_silenceCheck = m_params->SilenceCheck();
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

void DataConsumer::HandleProcessTasks(size_t smallestBuffer)
{
	for (int i = 0; i < MAX_TASKS; i++)
	{
		if (m_processTasks[i] == nullptr)
		{
			m_silenceCheck = true;
			ProcessTask(i, m_numberOfDevices, m_devParams, m_params, m_devices, m_buffer, smallestBuffer);
			ProcessTask(i, m_beginR, m_endR, m_beginL, m_endL, m_memoryR, m_memoryL, m_timestampR, m_timestampL);

			m_beginR = m_endR;
			m_beginL = m_endL;
			
			break;	
		}
		else if (i == MAX_TASKS)
		{
			FlushBuffer(); // no free tasks
			delete m_buffer;
		}
	}
}

void DataConsumer::ProcessTask(int idx, size_t numberOfDevices, AudioDevices^ devParams, AudioParameters^ params, std::vector<DeviceInfo> devices, AudioBuffer* buffer, size_t smallestBuffer)
{
	auto workItemDelegate = [this, numberOfDevices, devParams, params, devices, buffer, smallestBuffer](IAsyncAction^ action)
	{
		m_analyzer->ProcessData(m_numberOfDevices, devParams, params, devices, buffer, smallestBuffer);
	};

	auto completionDelegate = [this, idx](IAsyncAction^ action, AsyncStatus status)
	{
		switch (action->Status)
		{
		case AsyncStatus::Completed:
		case AsyncStatus::Error:
		case AsyncStatus::Canceled: m_processTasks[idx] = nullptr;
		}
	};

	auto workItemHandler = ref new WorkItemHandler(workItemDelegate);
	auto completionHandler = ref new AsyncActionCompletedHandler(completionDelegate, CallbackContext::Same);

	m_processTasks[idx] = ThreadPool::RunAsync(workItemHandler, WorkItemPriority::High);
	m_processTasks[idx]->Completed = completionHandler;
}

void DataConsumer::ProcessTask(int idx, uint32 beginR, uint32 endR, 
	uint32 beginL, uint32 endL, 
	const Array<int32>^ memoryR, const Array<int32>^ memoryL,
	const Array<UINT64>^ timeStampR, const Array<UINT64>^ timeStampL)
{
	auto workItemDelegate = [this, beginR, endR, beginL, endL, memoryR, memoryL, timeStampR, timeStampL](IAsyncAction^ action)
	{
		m_analyzer->ProcessData(beginR, endR, beginL, endL, memoryR, memoryL, timeStampR, timeStampL);
	};

	auto completionDelegate = [this, idx](IAsyncAction^ action, AsyncStatus status)
	{
		switch (action->Status)
		{
		case AsyncStatus::Completed:
		case AsyncStatus::Error:
		case AsyncStatus::Canceled: m_processTasks[idx] = nullptr;
		}
	};

	auto workItemHandler = ref new WorkItemHandler(workItemDelegate);
	auto completionHandler = ref new AsyncActionCompletedHandler(completionDelegate, CallbackContext::Same);

	m_processTasks[idx] = ThreadPool::RunAsync(workItemHandler, WorkItemPriority::High);
	m_processTasks[idx]->Completed = completionHandler;
}
