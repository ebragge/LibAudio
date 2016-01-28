#pragma once

#include "..\LibTDE\LibTDE\TimeDelayEstimation.h"

using namespace Platform;

namespace LibAudio
{
	public enum class HeartBeatType
	{
		DATA,
		BUFFERING,
		DEVICE_ERROR,
		SILENCE,
		INVALID,
		NODEVICE,
		STARTING,
		STATUS
	};

	public ref class TimeWindow sealed
	{
	public:
		TimeWindow(UINT64 b, UINT64 e, uint32 s) : begin(b), end(e), samples(s) {}

		UINT64 Begin() { return begin; }
		UINT64 End() { return end; }
		uint32 Samples() { return samples; }

	private:
		UINT64 begin;
		UINT64 end;
		uint32 samples;
	};

	public ref class TDESample sealed
	{
	public:
		TDESample(int v, uint32 a, uint32 m0, uint32 m1) : value(v), algorithm(a), mic0(m0), mic1(m1) {}

		int Value() { return value; }
		uint32 Algorithm() { return algorithm; }
		uint32 Microphone0() { return mic0; }
		uint32 Microphone1() { return mic1; }
        
	private:
		int value;
		uint32 algorithm;
		uint32 mic0;
		uint32 mic1;
	};

	public ref class AudioSample sealed
	{
	public:
		AudioSample(int a, uint32 m, uint32 ave) : align(a), maxAmp(m), average(ave) {}
	
		int Align() { return align; }
		uint32 MaxAmplitude() { return maxAmp; }
		uint32 Average() { return average; }
    
	private:
		int align;
		uint32 maxAmp;
		uint32 average;
	};

	public delegate void UIDelegate1(HeartBeatType, TimeWindow^, const Array<TDESample^>^, const Array<AudioSample^>^);
	public delegate void UIDelegate2(HeartBeatType, uint32, uint32, uint32, uint32, 
		const Array<int32>^, const Array<int32>^, const Array<UINT64>^, const Array<UINT64>^);
	public delegate void UIDelegate3(Platform::String^);

	ref class HeartBeatSender sealed
	{
	public:
		HeartBeatSender(UIDelegate1^ func);
		HeartBeatSender(UIDelegate2^ func);
		virtual ~HeartBeatSender();

		void HeartBeat(int delta, HeartBeatType status);

		void HeartBeat(int delta, HeartBeatType status, UINT64 fts, UINT64 lts = 0,
			int i0 = 0, int i1 = 0, int i2 = 0, int i3 = 0, int i4 = 0, int i5 = 0,
			int i6 = 0, int i7 = 0, int i8 = 0, int i9 = 0, int i10 = 0, int i11 = 0,
			uint32 ui0 = 0, uint32 ui1 = 0, uint32 ui2 = 0, uint32 ui3 = 0, uint32 ui4 = 0,
			uint32 ui5 = 0, uint32 ui6 = 0, uint32 ui7 = 0, uint32 ui8 = 0);

		void HeartBeat(int delta, HeartBeatType status, 
			uint32 beginR, uint32 endR, 
			uint32 beginL, uint32 endL, 
			const Array<int32>^ memoryR, const Array<int32>^ memoryL,
			const Array<UINT64>^ timeStampR, const Array<UINT64>^ timeStampL);

	private:
		CRITICAL_SECTION m_CritSec;
		UIDelegate1^ m_uiHandler1;
		UIDelegate2^ m_uiHandler2;
		ULONGLONG m_tick;
	};
}
