#ifndef TMSIDEVICE_H
#define TMSIDEVICE_H


#include <libusb-1.0/libusb.h>
#include <iostream>
#include <queue>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

using namespace std;

// Define these values to match your devices
static const uint16_t USB_TMSI_VENDOR_ID = 0x0C7C;
static const uint16_t USB_TMSI_PRODUCT_ID =	0x0004;

// Buffer structure
static const uint16_t BULK_RECV_URBS = 100;
static const uint16_t ISOC_RECV_URBS = 100;
static const uint16_t STOP_REQUEST = 0x0210;

// Get a minor range for your devices from the usb maintainer
static const uint16_t USB_TMSI_MINOR_BASE = 192;

// Protocol data
static const uint16_t BLOCKSYNC = 0xAAAA;
static const uint8_t  TMSACKNOWLEDGE = 0x00;

class tmsiDevice
{
public:
	tmsiDevice(int16_t freqFactor, int32_t* ch, int32_t numOfCh,
			   int32_t out_size, int32_t aux, int32_t digital);
	~tmsiDevice();
	int open();
	int close();
	void printFrontendInfo();
	void startDataSend();
	void stopDataSend();
	bool isOpen();
	int receiveNextCommand(int length);
	int receiveNextSample(int length);
	int32_t getMatCount();
	void resetMatCount();

	// mutex for thread safety of buffer access
	pthread_mutex_t mutexBulkRec;

	// FIFO buffer
	std::queue<int16_t> sample_buffer;
	std::queue<int16_t> command_buffer;
	double** outputMatrix;
	int32_t m_numOfCh;
	int32_t m_out_size;

	// libusb context
	struct libusb_context* ctx;

	const libusb_endpoint_descriptor* bulk_recv_endpoint;
	struct libusb_device_handle* usb_dev;

protected:
	int writeCommand(int16_t descriptor,const std::vector<int16_t> &datablock);

private:
	// attributes
	int16_t nrofuserchannels;
	int16_t currentsampleratesetting;
	int16_t mode;
	int16_t maxRS232;
	int32_t serialnumber;
	int16_t nrEXG;
	int16_t nrAUX;
	int16_t hwversion;
	int16_t swversion;
	int16_t cmdbufsize;
	int16_t sendbufsize;
	int16_t nrofswchannels;
	int16_t basesamplerate;
	int verbose;
	std::vector<int16_t> datablock;
	uint16_t power;
	uint16_t hardwarecheck;
	int16_t lastError;
	unsigned char cur_packet, last_packet;
	int32_t rcvSingleSample[64];
	bool m_frontEndInfoRec;

	// settings for user interface
	int16_t m_freqFactor;
	int32_t m_aux;
	int32_t m_digital;
	int32_t m_channels[38];
	int32_t matCount;

	// basic read and write routines
	int write(int16_t *msg, int32_t n);
	size_t readCommand(int16_t *msg, uint32_t n);
	size_t readSample(int16_t *msg, uint32_t n);

	// tmsi device routines
	void frontendInfoReq();
	void writeFrontendInfo();

	void printAcknowledge(int16_t descriptor,int16_t errorcode);
	int32_t convertWORD2LONG(int16_t wlow,int16_t whigh);


	// Receive buffers
	unsigned char** bulk_recv_buffer;
	unsigned char** isoc_recv_buffer;

	// Receive URB's
	struct libusb_transfer** bulk_recv_transfer;
	struct libusb_transfer** isoc_recv_transfer;

	// Endpoints
	//const libusb_endpoint_descriptor* bulk_recv_endpoint;
	const libusb_endpoint_descriptor* bulk_send_endpoint;
	const libusb_endpoint_descriptor* isoc_recv_endpoint;

	// flags
	bool deviceFound;
	bool deviceOpen;

	// thread for libusb event handling
	pthread_t eventhandler_thread;
};

// R/W callback functions
void tmsi_write_bulk_callback(struct libusb_transfer* trans);
void tmsi_read_bulk_callback(struct libusb_transfer* trans);
void tmsi_read_isoc_callback(struct libusb_transfer* trans);

// event handler thread function
void* eventhandler(void* data);


#endif
