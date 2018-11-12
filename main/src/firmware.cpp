/*
 * displayMananger.cpp
 *
 *  Created on: Apr 18, 2017
 *      Author: RSN
 */

#include "firmware.h"
#define TAG "RSN"
using namespace std;
extern bool set_commonCmd(arg* pArg,bool check);
extern string getParameter(arg* argument,string cual);
extern void sendResponse(void* comm,int msgTipo,string que,int len,int errorcode,bool withHeaders, bool retain);
extern void ConfigSystem(void *pArg);
extern uint32_t IRAM_ATTR millis();
//string getParameter(arg* argument,string cual);
extern void postLog(int code,int code1);

void  task_fatal_error(arg *argument)
								{
	printf("Exiting task due to fatal error...");
	close(socket_id);
	string algo="Not authorized";
	sendResponse( argument->pComm,argument->typeMsg, algo,algo.length(),ERRORAUTH,false,false);
	(void)vTaskDelete(NULL);
								}

/*read buffer by byte still delim ,return read bytes counts*/
static int read_until(char *buffer, char delim, int len)
{
	//  /*TODO: delim check,buffer check,further: do an buffer length limited*/
	int i = 0;
	while (buffer[i] != delim && i < len) {
		++i;
	}
	return i + 1;
}

/* resolve a packet from http socket
 * return true if packet including \r\n\r\n that means http packet header finished,start to receive packet body
 * otherwise return false
 * */
static bool read_past_http_header(char text[], int total_len, esp_ota_handle_t update_handle)
{
	/* i means current position */
	int i = 0, i_read_len = 0;
	while (text[i] != 0 && i < total_len) {
		i_read_len = read_until(&text[i], '\n', total_len);
		// if we resolve \r\n line,we think packet header is finished
		if (i_read_len == 2) {
			int i_write_len = total_len - (i + 2);
			memset(ota_write_data, 0, BUFFSIZE);
			/*copy first http packet body to write buffer*/
			memcpy(ota_write_data, &(text[i + 2]), i_write_len);

			esp_err_t err = esp_ota_write( update_handle, (const void *)ota_write_data, i_write_len);
			if (err != ESP_OK) {
				printf( "Error: esp_ota_write failed! err=0x%x\n", err);
				return false;
			} else {
				printf( "esp_ota_write header OK\n");
				binary_file_length += i_write_len;
			}
			return true;
		}
		i += i_read_len;
	}
	return false;
}

static bool connect_to_http_server()
{
//	printf("RSNServer IP: %s Server Port:%s\n", FW_SERVER_IP, FW_SERVER_PORT);
	sprintf(http_request, "GET %s HTTP/1.1\r\nHost: %s:%s \r\n\r\n", FW_FILENAME, FW_SERVER_IP, FW_SERVER_PORT);
	int  http_connect_flag = -1;
	struct sockaddr_in sock_info;

	socket_id = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_id == -1) {
		printf( "Create socket failed!\n");
		return false;
	}

	// set connect info
	memset(&sock_info, 0, sizeof(struct sockaddr_in));
	sock_info.sin_family = AF_INET;
	sock_info.sin_addr.s_addr = inet_addr(FW_SERVER_IP);
	sock_info.sin_port = htons(atoi(FW_SERVER_PORT));

	// connect to http server
	http_connect_flag = connect(socket_id, (struct sockaddr *)&sock_info, sizeof(sock_info));
	if (http_connect_flag == -1) {
		printf( "Connect to server failed! errno=%d\n", errno);
		close(socket_id);
		return false;
	} else {
		printf( "Connected to server\n");
		return true;
	}
	return false;
}

void set_FirmUpdateCmd(void *pArg)
{
	arg *argument=(arg*)pArg;
	esp_err_t err;
	string algo;
	uint8_t como=1;

/*
	algo=getParameter(argument,"password");
	if(algo!="zipo")
	{
		algo="Not authorized";
		sendResponse( argument->pComm,argument->typeMsg, algo,algo.length(),NOERROR,false,false);            // send to someones browser when asked
		free(pArg);
		vTaskDelete(NULL);
	}
*/
	esp_ota_handle_t update_handle = 0 ;
	const esp_partition_t *update_partition = NULL;

	printf("Starting OTA...\n");

	const esp_partition_t *configured = esp_ota_get_boot_partition();
	const esp_partition_t *running = esp_ota_get_running_partition();

	assert(configured == running); /* fresh from reset, should be running from configured boot partition */
	printf("Running partition type %d subtype %d (offset 0x%08x)\n",
			configured->type, configured->subtype, configured->address);

	/*connect to http server*/
	if (connect_to_http_server()) {
		printf( "Connected to http server\n");
	} else {
		printf( "Connect to http server failed!\n");
		task_fatal_error(argument);
	}

	int res = -1;
	/*send GET request to http server*/
	res = send(socket_id, http_request, strlen(http_request), 0);
	if (res == -1) {
		printf("Send GET request to server failed\n");
		task_fatal_error(argument);
	} else {
		printf("Send GET request to server succeeded\n");
	}

	update_partition = esp_ota_get_next_update_partition(NULL);
	printf("Writing to partition %s subtype %d at offset 0x%x\n",
			update_partition->label,update_partition->subtype, update_partition->address);
	assert(update_partition != NULL);
	err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
	if (err != ESP_OK) {
		printf( "esp_ota_begin failed, error=%d\n", err);
		task_fatal_error(argument);
	}
	printf("rsnesp_ota_begin succeeded\n");
//	xTaskCreate(&ConfigSystem, "cfg", 512, (void*)20, 3, NULL);

	bool resp_body_start = false, flag = true;
	/*deal with all receive packet*/
	while (flag) {
		memset(text, 0, TEXT_BUFFSIZE);
		memset(ota_write_data, 0, BUFFSIZE);
	//	printf("Call recv\n");
		int buff_len = recv(socket_id, text, TEXT_BUFFSIZE, 0);
	//	printf("From recv\n");
		if (buff_len < 0) { /*receive error*/
			printf("Error: receive data error! errno=%d\n", errno);
			task_fatal_error(argument);
		} else
			if (buff_len > 0 && !resp_body_start) { /*deal with response header*/
				memcpy(ota_write_data, text, buff_len);
				resp_body_start = read_past_http_header(text, buff_len, update_handle);
				//printf("From respBody\n");
			} else
				if (buff_len > 0 && resp_body_start) { /*deal with response body*/
					memcpy(ota_write_data, text, buff_len);
					err = esp_ota_write( update_handle, (const void *)ota_write_data, buff_len);
					if (err != ESP_OK) {
						printf( "Error: esp_ota_write failed! err=0x%x\n", err);
						task_fatal_error(argument);
					}
					binary_file_length += buff_len;
					//   printf("Have written image length %d\n", binary_file_length);
					gpio_set_level((gpio_num_t)RXLED, como);
					gpio_set_level((gpio_num_t)SENDLED, !como);

					como = !como;
					printf(".");
					fflush(stdout);

				} else
					if (buff_len == 0) {  /*packet over*/
						flag = false;
						printf( "Connection closed, all packets received\n");
						close(socket_id);
					} else {
						printf("Unexpected recv result\n");
					}
	}

	printf("\nTotal Write binary data length : %d\n", binary_file_length);
	for (int a=0;a<15;a++)
	{
	gpio_set_level((gpio_num_t)WIFILED, 1);
	vTaskDelay(50 /  portTICK_RATE_MS);
	gpio_set_level((gpio_num_t)WIFILED, 0);
	vTaskDelay(50 /  portTICK_RATE_MS);
	}

	if (esp_ota_end(update_handle) != ESP_OK) {
		printf( "esp_ota_end failed!\n");
		task_fatal_error(argument);
	}
	err = esp_ota_set_boot_partition(update_partition);
	if (err != ESP_OK) {
		printf( "esp_ota_set_boot_partition failed! err=0x%x\n", err);
		task_fatal_error(argument);
	}
	postLog(FWUPDATE,0);
	printf("Prepare to restart system!\n");
	algo="OTA Loaded. Rebooting...";
	sendResponse( argument->pComm,argument->typeMsg, algo,algo.length(),MINFO,false,false);            // send to someones browser when asked
	algo="";
	vTaskDelay(3000 /  portTICK_RATE_MS);
	esp_restart();
	return ;
}
