#pragma once

#include "common.h"
#include "d2map.h"
#include "regaccess.h"
#include "bcmdma.h"
#include "bcmpwm.h"
#include "bcmpcm.h"
#include "bcmcm.h"
#include "bcmgpio.h"
#include "MMap.h"
#include "SwPwm.h"

namespace Wi2Pi
{
	class Fx
	{
	public:

		static Fx& Inst()
		{
			static Fx inst;
			return inst;
		}

		bool MapPrephirals()
		{
			PwmReg = (PBCM_PWM_REGISTERS)MMap::Inst().Map((PVOID)BCM_PWM_CPU_BASE, BCM_PWM_REG_LEN).UserAddress;

			if (!PwmReg)
			{
				LogError("D2Map::MMap PWM registers failed");
				return false;
			}

			LogInfo("PWM Direct Access Acquired @VA:0x%08x @PA:0x%08x @BA:0x%08x", PwmReg, BCM_PWM_CPU_BASE, BCM_CPU_TO_BUS_PERIPH_ADDR(BCM_PWM_CPU_BASE));

			PcmReg = (PBCM_PCM_REGISTERS)MMap::Inst().Map((PVOID)BCM_PCM_CPU_BASE, BCM_PCM_REG_LEN).UserAddress;

			if (!PcmReg)
			{
				LogError("D2Map::MMap PCM registers failed");
				return false;
			}

			LogInfo("PCM Direct Access Acquired @VA:0x%08x @PA:0x%08x @BA:0x%08x", PcmReg, BCM_PCM_CPU_BASE, BCM_CPU_TO_BUS_PERIPH_ADDR(BCM_PCM_CPU_BASE));

			GpioReg = (PBCM_GPIO_REGISTERS)MMap::Inst().Map((PVOID)BCM_GPIO_CPU_BASE, BCM_GPIO_REG_LEN).UserAddress;

			if (!GpioReg)
			{
				LogError("D2Map::MMap GPIO registers failed");
				return false;
			}

			LogInfo("GPIO Direct Access Acquired @VA:0x%08x @PA:0x%08x @BA:0x%08x", GpioReg, BCM_GPIO_CPU_BASE, BCM_CPU_TO_BUS_PERIPH_ADDR(BCM_GPIO_CPU_BASE));

			DmaReg = (PBCM_DMA_REGISTERS)MMap::Inst().Map((PVOID)BCM_DMA_CPU_BASE, BCM_DMA_REG_LEN).UserAddress;

			if (!DmaReg)
			{
				LogError("D2Map::MMap DMA registers failed");
				return false;
			}

			LogInfo("DMA Direct Access Acquired @VA:0x%08x @PA:0x%08x @BA:0x%08x", DmaReg, BCM_DMA_CPU_BASE, BCM_CPU_TO_BUS_PERIPH_ADDR(BCM_DMA_CPU_BASE));

			CmReg = (PBCM_CM_REGISTERS)MMap::Inst().Map((PVOID)BCM_CM_CPU_BASE, BCM_CM_REG_LEN).UserAddress;

			if (!CmReg)
			{
				LogError("D2Map::MMap CM registers failed");
				return false;
			}

			LogInfo("Clock Manager Direct Access Acquired @VA:0x%08x @PA:0x%08x @BA:0x%08x", CmReg, BCM_CM_CPU_BASE, BCM_CPU_TO_BUS_PERIPH_ADDR(BCM_CM_CPU_BASE));

			return true;
		}

		bool Init()
		{
			GlobalShutdownEvt = CreateEvent(NULL, TRUE, FALSE, NULL);

			(void)QueryPerformanceCounter(&Wi2PiT0);
			(void)QueryPerformanceFrequency(&HpcFreq);

			HpcPerdiodNs = 1000000000.0 / HpcFreq.QuadPart;
			HpcMicroNumTicks = (double)HpcFreq.QuadPart / 1000000.0;

			LogInfo("Initializing Wi2Pi Library");

			if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS))
			{
				LogError("SetPriorityClass REALTIME_PRIORITY_CLASS failed, %d", GetLastError());
				return false;
			}

			LogInfo("HPC Info: Freq=%lldHz, Period=%fns 1MicroNumTicks=%f", HpcFreq.QuadPart, HpcPerdiodNs, HpcMicroNumTicks);

#ifdef DEBUG_TIMING

			LARGE_INTEGER t0, t1, t2, t3;
			QueryPerformanceCounter(&t0);
			QueryPerformanceCounter(&t1);
			QueryPerformanceCounter(&t2);
			QueryPerformanceCounter(&t3);
			LogInfo("t0=%lld\nt1=%lld\nt2=%lld\nt3=%lld\n",
				t0.QuadPart, t1.QuadPart, t2.QuadPart, t3.QuadPart);

			LogInfo("t1-t0=%lld~%lldns\nt2-t1=%lld~%lldns\nt3-t2=%lld~%lldns",
				t1.QuadPart - t0.QuadPart, ((t1.QuadPart - t0.QuadPart) * 1000000000) / HpcFreq.QuadPart,
				t2.QuadPart - t1.QuadPart, ((t2.QuadPart - t1.QuadPart) * 1000000000) / HpcFreq.QuadPart,
				t3.QuadPart - t2.QuadPart, ((t3.QuadPart - t2.QuadPart) * 1000000000) / HpcFreq.QuadPart);

			LogInfo("\nStart Testing HPC");
			MicroDelay(1);
			MicroDelay(10);
			MicroDelay(100);
			MicroDelay(1000);
			LogInfo("End Testing HPC\n");

			ArrowResult arrowTestRes;
			arrowTestRes.ThrowBackTimeUs = 0;
			arrowTestRes.ThrowTimeUs = 0;
			const int ArrowNumSamples = 100;

			for (int i = 0; i < ArrowNumSamples; ++i)
			{
				ArrowResult currRes = MMap::Inst().Arrow();

				LogVerbose(
					"Sample%d: User->Kernel=%lldus, Kernel->User=%lldus",
					i,
					currRes.ThrowTimeUs,
					currRes.ThrowBackTimeUs);

				arrowTestRes.ThrowTimeUs += currRes.ThrowTimeUs;
				arrowTestRes.ThrowBackTimeUs += currRes.ThrowBackTimeUs;
			}

			arrowTestRes.ThrowTimeUs /= (LONGLONG)ArrowNumSamples;
			arrowTestRes.ThrowBackTimeUs /= (LONGLONG)ArrowNumSamples;

			LogInfo(
				"KMDF IOCTL Timing: User->Kernel=%lldus, Kernel->User=%lldus, User<->Kernel= %lldus over %d samples",
				arrowTestRes.ThrowTimeUs,
				arrowTestRes.ThrowBackTimeUs,
				arrowTestRes.ThrowTimeUs + arrowTestRes.ThrowBackTimeUs,
				ArrowNumSamples);

#endif
			LogInfo("Mapping low level prephirals");

			if (!MapPrephirals())
			{
				LogError("Failed to map prephirals registers");
				return false;
			}

			return true;
		}

		void Deinit()
		{
			GlobalShutdownFlag = true;
			SetEvent(GlobalShutdownEvt);
		}

		volatile bool GlobalShutdownFlag;
		HANDLE GlobalShutdownEvt;

	private:
		Fx() {}
	};
}