/* Samsung RIL Socket protocol defines */

#define SRS_COMMAND(f)  ((f->group << 8) | f->index)
#define SRS_GROUP(m)    (m >> 8)
#define SRS_INDEX(m)    (m & 0xff)

#define SRS_CONTROL			0x01
#define SRS_CONTROL_GET_HELO		0x0102
#define SRS_CONTROL_LINK_CLOSE		0x0103

#define SRS_SND				0x02
#define SRS_SND_SET_CALL_VOLUME		0x0201
#define SRS_SND_SET_CALL_AUDIO_PATH	0x0202
#define SRS_SND_SET_CALL_CLOCK_SYNC	0x0203

#define SRS_CONTROL_HELO		0xCAFFE

#define SRS_CONTROL_LINK_STATUS_OPEN	0x01
#define SRS_CONTROL_LINK_STATUS_CLOSE	0x02

#define SRS_SOCKET_NAME	"samsung-ril-socket"
#define SRS_DATA_MAX_SIZE		0x1000

struct srs_header {
	unsigned int length;
	unsigned char group;
	unsigned char index;
	unsigned char msg_id;
} __attribute__((__packed__));

struct srs_message {
	unsigned short command;
	unsigned char msg_id;
	int data_len;
	void *data;
} __attribute__((__packed__));

