#ifdef __cplusplus
extern "C" {
#endif
	void initPcsx();

	void startPcsx(char * game);

	void runFramePcsx();

	void shutdownPcsx();
	void resetPcsx();

#ifdef __cplusplus
}
#endif