#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <termios.h>
#include <string.h>
#include <signal.h>
//#include <cutils/properties.h>
//#include <cutils/log.h>
#include <malloc.h>
#include "bt_hci.h"
#ifdef FW_IN_H
#include "bt_fucode_em4.h"
#endif

extern int binfile_fd;
extern int infofile_fd;
extern int chip_type;
extern int uart_fd;
extern int debug;
const uint_32 T9026_DCCMaddress = 0x800000;
const uint_32 T9026_ICCMaddress = 0x000000;
const uint_32 T9026_SRAMaddress = 0x900000;

const uint_32 T9026_DCCMLEN = 64 * 1024;
const uint_32 T9026_ICCMLEN = (256) * 1024;
const uint_32 T9026_SRAMLEN = 32 * 1024;

uint_32 W1_DCCMaddress = 0x800000;
uint_32 W1_ICCMaddress = 0x3ff9c;
uint_32 W1_SRAMaddress = 0x900000;

uint_32 W1_DCCMLEN = 64 * 1024;
uint_32 W1_ICCMLEN = 64 * 1024;
uint_32 W1_SRAMLEN = 0;//32 * 1024;
#ifndef FW_IN_H
unsigned char BT_fwICCM[256*1024];
unsigned char BT_fwDCCM[96*1024];
unsigned char BT_fwSRAM[32*1024];
#endif


uint_32 DCCMaddress = 0x800000;//T9026_DCCMaddress;
uint_32 ICCMaddress = 0x000000;//T9026_ICCMaddress;
uint_32 SRAMaddress = 0x900000;//T9026_SRAMaddress;

uint_32 DCCMLEN = 64 * 1024;   //T9026_DCCMLEN;
uint_32 ICCMLEN = (256) * 1024;//T9026_ICCMLEN;
uint_32 SRAMMLEN = 0;
int start_download_fw(){
	struct stat bin_file_info;
	struct stat info_file_info;
	uint_32 verify_address[] = {0xa70014, 0x00afe008};
	char* binfile_data;
	char* infofile_data;
	uint_32 value = 0;
	off_t binfile_size = 0;
	off_t infofile_size = 0;
	off_t bin_read_len = 0;
	off_t info_read_len = 0;
	uint_32 dcclen_valid = 0, icclen_valid =0;
	int result = 0;
	char state =0, ch =0, is_next_value=0;
	long i = 0;
	int iccm_read_off = 0;
	uint_32 reg_data = 0x0;
	uint_32 cmp_data = 0x0;
	int ret = 0;
	if (fstat(binfile_fd,&bin_file_info) < 0) {
		printf("fstat binfile failed!\n");
		exit(1);
	}
	if (fstat(infofile_fd,&info_file_info) < 0) {
		printf("fstat infofile failed!\n");
		exit(1);
	}
	binfile_size = bin_file_info.st_size;
	infofile_size = info_file_info.st_size;
	binfile_data = (char*) malloc(binfile_size);
	infofile_data = (char*) malloc(infofile_size);
	if ((!binfile_data) || (!infofile_data)) {
		printf("Malloc failed!\n");
		exit(1);
	}
	bin_read_len = read(binfile_fd, binfile_data,bin_file_info.st_size);
	info_read_len = read(infofile_fd, infofile_data,info_file_info.st_size);
	if (bin_read_len != binfile_size) {
		printf("Failed to read bin file data!\n");
		exit(2);
	}
	if (info_read_len != infofile_size) {
		printf("Failed to read info file data!\n");
		exit(3);
	}
	switch (chip_type) {
		case W1:
			#ifndef FW_IN_H

			for (i=0, state = 0; i < infofile_size;i++)
			{
				ch = infofile_data[i];
				if (ch == ':') {
					is_next_value = 1;
					value = 0;
				}
				else if (ch <= '9' && ch >= '0') {
					value = value*10 + ch-'0';
				}
				else if (ch == '\n')
				{
					is_next_value = 0;
					if (state == 0)
						ICCMaddress = value;
					else if (state == 1)
						icclen_valid = value;
					else if (state == 2)
						DCCMaddress = value;
					else if (state == 3)
						dcclen_valid = value;
					state++;
				}
			}
			//free(infofile_data);
			memset(BT_fwICCM, 0x00, sizeof(BT_fwICCM));
			memset(BT_fwDCCM, 0x00, sizeof(BT_fwDCCM));
			memcpy(BT_fwICCM, binfile_data+ICCMaddress, icclen_valid);
			memcpy(BT_fwDCCM, binfile_data+DCCMaddress, dcclen_valid);
			DCCMLEN = W1_DCCMLEN;
			ICCMLEN = W1_DCCMLEN;
			#else
			DCCMLEN = sizeof(BT_fwICCM) - 256*1024;
			ICCMLEN = W1_DCCMLEN;
			#endif
			break;
		case T9026:
			memcpy(BT_fwICCM, binfile_data+ICCMaddress, ICCMLEN);
			if (binfile_size > (off_t)(DCCMaddress+DCCMLEN))
			{
				memcpy(BT_fwDCCM, binfile_data+DCCMaddress, DCCMLEN);
			}
			else
			{
				memcpy(BT_fwDCCM, binfile_data+DCCMaddress, binfile_size-DCCMaddress);
				DCCMLEN = binfile_size-DCCMaddress;
			}
			break;
			break;
		default:
			break;

	}
	printf("Chip type is %d\n",chip_type);
	result = TCI_Write_Register(TCI_WRITE_REG, 0xf03050, 0x00000000); //ICCM DCCM Memory power on

    if (!result) {
        fprintf(stderr, "BT Power on enable!\n");
    }
    else {
        fprintf(stderr, "BT Power on disable!\n");
    }

	if (chip_type == W1) {
		result = TCI_Write_Register(TCI_UPDATE_UART_BAUDRATE,0xa30128, 0x7013);
	}
	else if (chip_type == T9026) {
		result = TCI_Write_Register(TCI_UPDATE_UART_BAUDRATE,0xa30128, 0x700b);
	}
	if (!result) {
		change_uart_baud(2000000);//2M
		result = TCI_Read_Register(TCI_UPDATE_UART_BAUDRATE,0xa30128,&value);
		if (!result) printf("Update bt firmware uart baud successfully!\n");
		else printf("Failed to update bt firmware uart baud!\n");
	}

	if (chip_type == T9026) {
		printf("Start verify write or read register!\n");
		for (uint_32 i = 0;i < sizeof(verify_address) / sizeof(verify_address[0]);i++) {
			result = TCI_Read_Register(TCI_READ_REG,verify_address[i],&value);
			if (result) {
				printf("Read register address-->0x%08x fail!\n",verify_address[i]);
				ret = -1;
				goto failed;
			}
		}
		printf("Read verify address successfully!\n");
		result = TCI_Write_Register(TCI_WRITE_REG,0x00afe008,0xffff);
		if (!result)
			result = TCI_Read_Register(TCI_READ_REG,0x00afe008,&value);
		if (!result)
		{
			if (value == 0xffff)
				printf("Write register address 0x00afe008 successfully!\n");
			else {
				printf("Failed to write register address 0x00afe008!\n");
				ret = -1;
				goto failed;

			}
		}

	}
	else if (chip_type == W1) {
		printf("Start enable download event!\n");
		result = TCI_Write_Register(TCI_WRITE_REG,0xa70014, 0x1000000);
		if (result) {
			printf("Failed to enable download fw event!\n");
			ret = -1;
			goto failed;

		}
	}
	result = download_fw_img();
	if (result) {
		printf("Download BT Fw img fail!\n");
		ret = -1;
		goto failed;

	}
	if (chip_type == T9026) {
		printf("Start iccm/dccm compare\n");
		for (int j = 0; j < 10; j++)
		{
			iccm_read_off = j * 10240;	// read once per 10K.
			result = TCI_Read_Register(TCI_READ_REG, ICCM_RAM_BASE + iccm_read_off, &value);
			if (!result)
			{
				TCI_get_reg_data_event(TCI_READ_REG, ICCM_RAM_BASE + iccm_read_off, &reg_data);
				printf("reg_data = 0x%x\n", reg_data);
			}
			cmp_data = (BT_fwICCM[iccm_read_off]) + (BT_fwICCM[iccm_read_off + 1] << 8)
				+ (BT_fwICCM[iccm_read_off + 2] << 16) + (BT_fwICCM[iccm_read_off + 3] << 24);
			//printf("cmp_data = 0x%x,reg_data = 0x%x\r\n", cmp_data, reg_data);
			//str.Format(L"cmp_data = 0x%x,reg_data = 0x%x\r\n", cmp_data, reg_data);
			//strTmp += str;
			if (cmp_data == reg_data)
			{
				printf("read iccm OK\n");
			}
			else
			{
				printf("read iccm Fail\n");
			}
			reg_data = 0;
		}
	}
	if (chip_type == W1)
	{
		value = 0x08000000;
		result = TCI_Write_Register(TCI_WRITE_REG, 0xa7000c, value);

		if (!result)
		{
			printf("write 0xa7000c success!\n");
		}
		else
		{
			printf("write 0xa7000c failed!\n");
		}
	}
	result = TCI_Write_Register(TCI_WRITE_REG, 0xa70014, 0x0000000);
	if (!result) {
		printf("close fw load!\n");
	}
	else
	{
		printf("close fw load failed!\n");
		ret = -1;
		goto failed;

	}
	if (chip_type == T9026)
	{
		result = TCI_Write_Register(TCI_WRITE_REG, 0xa0000c, 0x1000000);
		if (!result) {
			printf("CPU start work!\n");
		}
	}
	else if (chip_type == W1)
	{
		result = TCI_Write_Register(TCI_UPDATE_UART_BAUDRATE, 0xf03058, 0x700); // power on
		if (result)
		{
			printf("Patch power on failed!\n");
			ret = -1;
			goto failed;

		}
		else
		{
			printf("Patch power on!\n");
		}
	}
	change_uart_baud(4000000);//4M
	usleep(2000000);
	proc_reset();
	usleep(2000);
failed:
	free(infofile_data);
	free(binfile_data);
	return 0;
}
int download_fw_img(){
	int count = 0;
	int result = 0;
	unsigned char* bufferICCM = BT_fwICCM;
	unsigned char* bufferDCCM = BT_fwDCCM;
	int len = 0, offset = 0, offset_in_bt =0;
	unsigned int cmd_len = 0;
	unsigned int data_len = 0;
	int cnt = 1;
	len = ICCMLEN;//ALIGN(sizeof(BT_fwICCM), 4);
	printf("BT start iccm copy, total=0x%x \n", len);

#ifndef FW_IN_H
	offset = 0;
	if (chip_type == T9026)
		offset_in_bt = 0;
	else if (chip_type == W1)
		offset_in_bt = 256*1024;
#else
	if (chip_type == W1)
		offset = 256*1024;
	else
		offset = 0;
	offset_in_bt = 0;
#endif
	if (len == 0)
		goto DCC_START;
	printf("BT iccm base 0x%x \r\n", ICCM_RAM_BASE);
	do {
		data_len = (len > RW_OPERTION_SIZE) ? RW_OPERTION_SIZE : len;
		cmd_len = data_len + 4;
		result = TCI_Generate_uart_load_bt_fw_Command(TCI_DOWNLOAD_BT_FW, cmd_len, ICCM_RAM_BASE + offset_in_bt+offset, bufferICCM + offset);
		if (result) {
			printf("Download iccm data error!\n");
			return -1;
		}
		cnt++;
		offset += data_len;
		len -= data_len;
		count++;

	}while (len > 0);

DCC_START:
	len = DCCMLEN;//ALIGN(sizeof(BT_fwDCCM), 4);
	offset = 0;
	cnt = 1;
	data_len = 0;
	cmd_len = 0;
	printf("start dccm copy, total=0x%x\n", len);
	do
	{
		data_len = (len > RW_OPERTION_SIZE) ? RW_OPERTION_SIZE : len;
		cmd_len = data_len + 4;
		result = TCI_Generate_uart_load_bt_fw_Command(TCI_DOWNLOAD_BT_FW, cmd_len, DCCM_RAM_BASE + offset, bufferDCCM + offset);
		if (result) {
			printf("Download dccm data error!\n");
			return -1;
		}
		cnt++;
		offset += data_len;
		len -= data_len;
		count++;
	} while (len > 0);
	return 0;
}
int TCI_get_reg_data_event(int opcode, uint_32 address, uint_32 *value)
{
	uint_8 pdu[30] = { 0 };
	uint_16 length = 0;
	uint_8 tmp_buff[4096] = { 0 };
	int rx_len = 0;
	int status = 0;
	uint_8 *data = (uint_8*)malloc(30);
	if (!data) {
		printf("New space fail!\n");
		return -1;
	}
	pdu[0] = (uint_8)(opcode & 0xFF);
	pdu[1] = (uint_8)((opcode >> 8) & 0xFF);
	pdu[2] = 4;
	length = pdu[2] + 3;
	_Insert32_Uint32(pdu + 3, address);
	data[0] = HCI_pduCOMMAND;
	memcpy(data + 1, pdu, length);
	write_data_for_download_fw(data, length + 1);
	rx_len = download_fw_event(uart_fd,tmp_buff);
	*value = _MakeUint32(tmp_buff + 7);
	free(data);
	return status;
}

int TCI_Generate_uart_load_bt_fw_Command(int opcode, uint_8 data_len,
	uint_32 addr, uint_8* data)
{
	uint_8 pdu[300] = { 0 };
	uint_16 length = 0;
	int i = 0;
	uint_8 rx_buff[1024] = {0};
	uint_8 *temp_data = (uint_8*)malloc(300);
	if (!temp_data) {
		printf("New space fail!\n");
		return -1;
	}
	pdu[0] = (unsigned char)(opcode & 0xFF);
	pdu[1] = (unsigned char)((opcode >> 8) & 0xFF);
	pdu[2] = data_len;
	length = pdu[2] + 3;
	_Insert32_Uint32(pdu + 3, addr);
	for (i = 0; i < data_len; i += 1)
	{
		pdu[i + 7] = *(data + i);
	}
	temp_data[0] = HCI_pduCOMMAND;
	memcpy(temp_data + 1, pdu, length);
	write_data_for_download_fw(temp_data, length + 1);
	download_fw_event(uart_fd,rx_buff);
	free(temp_data);
	return 0;

}

int TCI_Read_Register(int opcode, uint_32 address, uint_32 *value)
{
	uint_8 pdu[30] = { 0 };
	uint_8 *data = (uint_8*)malloc(30);
	uint_8 rx_buff[4096] = { 0 };
	int rx_len = 0;
	uint_16 length = 0;
	int status = 0;
	if (!data) {
		printf("New space fail!\n");
		return -1;
	}

	pdu[0] = (uint_8)(opcode & 0xFF);
	pdu[1] = (uint_8)((opcode >> 8) & 0xFF);
	pdu[2] = 2;
	length = pdu[2] + 3;
	_Insert32_Uint32(pdu + 3, address);
	data[0] = HCI_pduCOMMAND;
	memcpy(data + 1, pdu, length);
	write_data_for_download_fw(data, length + 1);
	rx_len = download_fw_event(uart_fd,rx_buff);
	if (rx_len) {
		if ((rx_buff[0] == HCI_pduEVENT) &&
			(rx_buff[4]==(opcode & 0xFF)) &&
			(rx_buff[5] == ((opcode>>8) & 0xFF)))
		{
			if (rx_buff[2] > 0x4)
				*value = _MakeUint32(rx_buff + 7);
		}
		else {
			status = -1;
		}
	}
	free(data);
	return status;
}
int TCI_Write_Register(int opcode, uint_32 address, uint_32 value)
{
	uint_8 pdu[30] = { 0 };
	uint_8 *data = (uint_8*)malloc(30);
	uint_8 tmp_buff[4096] = { 0 };
	int rx_len = 0;
	uint_16 length = 0;
	if (!data) {
		printf("New space fail!\n");
		return -1;
	}

	pdu[0] = (uint_8)(opcode & 0xFF);
	pdu[1] = (uint_8)((opcode >> 8) & 0xFF);
	pdu[2] = 8;
	length = pdu[2] + 3;
	_Insert32_Uint32(pdu + 3, address);
	_Insert32_Uint32(pdu + 7, value);
	data[0] = HCI_pduCOMMAND;
	memcpy(data + 1, pdu, length);
	write_data_for_download_fw(data, length + 1);
	rx_len = read_event(uart_fd, tmp_buff);
	free(data);
	return 0;

}

void _Insert32_Uint32(uint_8* p_buffer, uint_32 value_32_bit)
{
	p_buffer[0] = (uint_8)_SYS_GET_CHAR_8_BITS(value_32_bit);
	p_buffer[1] = (uint_8)_SYS_GET_CHAR_8_BITS(value_32_bit >> 8);
	p_buffer[2] = (uint_8)_SYS_GET_CHAR_8_BITS(value_32_bit >> 16);
	p_buffer[3] = (uint_8)_SYS_GET_CHAR_8_BITS(value_32_bit >> 24);
}
uint_32 _MakeUint32(uint_8* bytes)
{
	uint_32 temp = bytes[0] +(bytes[1] << 8) +(bytes[2] << 16) + (bytes[3] << 24);
	return temp;
}
uint_16 _MakeUint16(uint_8* bytes)
{
	uint_16 temp = 0;
	temp = bytes[0] + (bytes[1] << 8);

	return temp;
}
