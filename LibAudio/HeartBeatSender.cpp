#include "pch.h"
#include "Common.h"
#include "HeartBeatSender.h"

using namespace LibAudio;

HeartBeatSender::HeartBeatSender(UIDelegate1^ func) : m_uiHandler1(func), m_uiHandler2(nullptr), m_tick(0) 
{
	if (!InitializeCriticalSectionEx(&m_CritSec, 0, 0))
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}
}

HeartBeatSender::HeartBeatSender(UIDelegate2^ func) : m_uiHandler1(nullptr), m_uiHandler2(func), m_tick(0)
{
	if (!InitializeCriticalSectionEx(&m_CritSec, 0, 0))
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}
}

HeartBeatSender::~HeartBeatSender() {}

void HeartBeatSender::HeartBeat(int delta, HeartBeatType status)
{
	if (m_uiHandler1 != nullptr) m_uiHandler1(status, nullptr, nullptr, nullptr);
	else if (m_uiHandler2 != nullptr) m_uiHandler2(status, 0, 0, 0, 0, nullptr, nullptr, nullptr, nullptr);
}

void HeartBeatSender::HeartBeat(int delta, HeartBeatType status, UINT64 fts, UINT64 lts,
	int i0, int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8, int i9, int i10, int i11,
	uint32 ui0, uint32 ui1, uint32 ui2, uint32 ui3, uint32 ui4, uint32 ui5, uint32 ui6, uint32 ui7, uint32 ui8)
{
	EnterCriticalSection(&m_CritSec);

	ULONGLONG tick = GetTickCount64();
	if (tick - m_tick > delta)
	{
		TimeWindow^ w = ref new TimeWindow(fts,lts,ui8);

		Platform::Array<TDESample^>^ t = ref new Platform::Array<TDESample^>(9);
		t->set(0, ref new TDESample(i0, (uint32)TimeDelayEstimation::Algorithm::CC, 0, 1));
		t->set(1, ref new TDESample(i1, (uint32)TimeDelayEstimation::Algorithm::ASDF, 0, 1));
		t->set(2, ref new TDESample(i2, (uint32)TimeDelayEstimation::Algorithm::PEAK, 0, 1));
		t->set(3, ref new TDESample(i4, (uint32)TimeDelayEstimation::Algorithm::CC, 0, 2));
		t->set(4, ref new TDESample(i5, (uint32)TimeDelayEstimation::Algorithm::ASDF, 0, 2));
		t->set(5, ref new TDESample(i6, (uint32)TimeDelayEstimation::Algorithm::PEAK, 0, 2));
		t->set(6, ref new TDESample(i8, (uint32)TimeDelayEstimation::Algorithm::CC, 1, 2));
		t->set(7, ref new TDESample(i9, (uint32)TimeDelayEstimation::Algorithm::ASDF, 1, 2));
		t->set(8, ref new TDESample(i10, (uint32)TimeDelayEstimation::Algorithm::PEAK, 1, 2));

		Platform::Array<AudioSample^>^ a = ref new Platform::Array<AudioSample^>(4);
		a->set(0, ref new AudioSample(0, ui0, ui4));
		a->set(1, ref new AudioSample(i3, ui1, ui5));
		a->set(2, ref new AudioSample(i7, ui2, ui6));
		a->set(3, ref new AudioSample(i11, ui3, ui7));

		const Array<TDESample^>^ t_ref = t;
		const Array<AudioSample^>^ a_ref = a;

		m_uiHandler1(status, w, t, a);
		m_tick = tick;
	}

	LeaveCriticalSection(&m_CritSec);
}

void HeartBeatSender::HeartBeat(
	int delta, 
	HeartBeatType status, 
	uint32 beginR, 
	uint32 endR, 
	uint32 beginL, 
	uint32 endL, 
	const Array<int32>^ memoryR, 
	const Array<int32>^ memoryL,
	const Array<UINT64>^ timeStampR,
	const Array<UINT64>^ timeStampL)
{
	EnterCriticalSection(&m_CritSec);

	ULONGLONG tick = GetTickCount64();
	if (tick - m_tick > delta)
	{
		m_uiHandler2(status, beginR, endR, beginL, endL, memoryR, memoryL, timeStampR, timeStampL);
		m_tick = tick;
	}

	LeaveCriticalSection(&m_CritSec);
}
