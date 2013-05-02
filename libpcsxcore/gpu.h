#ifndef __GPU_H__
#define __GPU_H__

#ifdef __cplusplus
extern "C"
{
#endif
	// New !
	void gpuDmaThreadInit();
	void gpuDmaThreadShutdown();
	void gpuThreadEnable(int enable);

	void gpuWriteDataMem(uint32_t *, int);
	void gpuWriteStatus(u32 data);
	void gpuWriteData(uint32_t);

	uint32_t gpuReadStatus(void);
	uint32_t gpuReadData(void);
	void gpuReadDataMem(uint32_t *, int);

	void gpuUpdateLace();
	

	void psxDma2(u32 madr, u32 bcr, u32 chcr);
	void gpuInterrupt();

#ifdef __cplusplus
}
#endif

#endif
