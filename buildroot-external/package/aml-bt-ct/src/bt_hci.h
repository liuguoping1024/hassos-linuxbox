#ifndef _BT_HCI_H__
#define _BT_HCI_H__
#define W1               				0
#define T9026				   			1
#define ICCM_RAM_BASE										(0x000000)
#define DCCM_RAM_BASE										(0xd00000)
#define RW_OPERTION_SIZE									(248)
#define TCI_READ_REG								0xfef0
#define TCI_WRITE_REG								0xfef1
#define TCI_UPDATE_UART_BAUDRATE					0xfef2
#define TCI_DOWNLOAD_BT_FW							0xfef3
#define TCI_HOPPING_COMMAND						0xfc38
#define ALIGN(x,a)											(((x)+(a)-1)&~((a)-1))
#define _SYS_GET_CHAR_8_BITS(x) (x)

#define HCI_pduCOMMAND										0x01
#define HCI_pduACLDATA										0x02
#define HCI_pduSCODATA										0x03
#define HCI_pduEVENT										0x04


typedef unsigned char uint_8;
typedef unsigned short uint_16;
typedef unsigned int uint_32;

void hci_send_cmd(uint_8 *buf, int len);
int read_event(int fd, uint_8 *buffer);

int TCI_Write_Register(int opcode, uint_32 address, uint_32 value);
int start_download_fw();
int download_fw_img();
void _Insert32_Uint32(uint_8* p_buffer, uint_32 value_32_bit);
uint_32 _MakeUint32(uint_8* bytes);
uint_16 _MakeUint16(uint_8* bytes);
void dump(uint_8 *out, int len);
void change_uart_baud(int _baud);
int TCI_Read_Register(int opcode, uint_32 address, uint_32 *value);
int TCI_Generate_uart_load_bt_fw_Command(int opcode, uint_8 data_len, uint_32 addr, uint_8* data);
void proc_baudrate();
int TCI_get_reg_data_event(int opcode, uint_32 address, uint_32 *value);
int download_fw_event(int fd, uint_8 *buffer);
void proc_reset();
void write_data_for_download_fw(uint_8 *buf, int len);


#endif
