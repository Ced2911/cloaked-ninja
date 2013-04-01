#define PSX_WIDTH 1024
#define PSX_HEIGHT 512

#ifdef __cplusplus
extern "C" {
#endif
	void XbDispUpdate();
	unsigned int VideoInit();
	void UnlockLockDisplay();
	void UpdateScrenRes(int x,int y);
#ifdef __cplusplus
}
#endif
