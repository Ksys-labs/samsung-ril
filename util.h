void bin2hex(const unsigned char *data, int length, char *buf);
void hex2bin(const char *data, int length, unsigned char *buf);

unsigned char registatus_ipc2ril(unsigned char status);
unsigned char act_ipc2ril(unsigned char act);
unsigned char modesel_ipc2ril(unsigned char mode);
unsigned char modesel_ril2ipc(unsigned char mode);

