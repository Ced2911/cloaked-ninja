#ifndef __GPU_H__
#define __GPU_H__

#ifdef __cplusplus
extern "C"
{
#endif
	// New !
	void gpuDmaThreadInit();
	void gpuWriteDataMem(uint32_t *, int);
	void gpuWriteData(uint32_t);
	void gpuUpdateLace();

	int gpuReadStatus();

	void psxDma2(u32 madr, u32 bcr, u32 chcr);
	void gpuInterrupt();

#ifdef __cplusplus
}
#endif

#endif
