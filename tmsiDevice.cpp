
#include "tmsiDevice.hpp"
#include <stdlib.h>
#include <bitset>
#include <math.h>
#include <sstream>
#include <string>
#include <stdio.h>

template <class T>
const T abs_mod(const T& a, const T& b)
{
	return a - b*floor((const float)a/b);
}

tmsiDevice::tmsiDevice(int16_t freqFactor, int32_t* ch, int32_t numOfCh,
		   	   	   	   int32_t out_size, int32_t aux, int32_t digital)
{
#ifdef WITH_HW
	libusb_device **devs;
	libusb_device_descriptor desc;
	int deviceCount = 0;
	this->usb_dev = NULL;

	// initialize mutex
	pthread_mutex_init (&(this->mutexBulkRec), NULL);

	// initialize buffer
	this->bulk_recv_buffer = (unsigned char**) malloc(BULK_RECV_URBS * sizeof(unsigned char*));
	this->bulk_recv_transfer = (libusb_transfer**) malloc(BULK_RECV_URBS * sizeof(struct libusb_transfer*));
	this->isoc_recv_buffer = (unsigned char**) malloc(ISOC_RECV_URBS * sizeof(unsigned char*));
	this->isoc_recv_transfer = (libusb_transfer**) malloc(ISOC_RECV_URBS * sizeof(struct urb*));

	// set the endpoint descriptors
	this->ctx = NULL; //a libusb session
	int r; //for return values
	cout<<"Lets start " << endl;

	r = libusb_init(&(this->ctx)); //initialize the library for the session we just declared
	if(r < 0) {
		cout<<"Init Error " << r << endl; //there was an error
		throw 1;
	}
	libusb_set_debug(this->ctx, 0); //set verbosity level to 3, as suggested in the documentation

	// get device list
	int cnt = libusb_get_device_list(this->ctx, &devs); //get the list of devices
	if(cnt < 0) {
		cout<<"Get Device Error"<<endl; //there was an error
	}
	cout<<cnt<<" Devices in list."<<endl;

	// look for tmsi device
	for (int i = 0; i < cnt; i++){
		r = libusb_get_device_descriptor(devs[i], &desc);
	    if (r < 0) {
	        cout<<"failed to get device descriptor"<<endl;
	    }
	    // check vendor ID
	    if (desc.idVendor == 0x0c7c){
	    	cout << "TMSI device found!" << endl;
	    	this->deviceFound = true;
	    	deviceCount = i;
	    	break;
	    }
	}

	if (!this->deviceFound){
		cout << "No TMSI device found."<<endl;
		throw 1;
	}

	// check product ID
	if (desc.idProduct != 0x0004){
		cout << "fxload was not executed prior to use of the TMSI. It will be executed now."<<endl;

		// fxload has to be called here
		int bus_number = libusb_get_bus_number(devs[deviceCount]);
		int dev_address = libusb_get_device_address(devs[deviceCount]);

		std::stringstream path;
		path << "fxload -v -t fx2 -D /dev/bus/usb/";
		if (bus_number > 99){
			path << bus_number << "/";
		}
		else if (bus_number > 9){
			path << "0" << bus_number << "/";
		}
		else {
			path << "00" << bus_number << "/";
		}
		if (dev_address > 99){
			path << dev_address;
		} else if (dev_address > 9){
			path << "0" << dev_address;
		} else {
			path << "00" << dev_address;
		}
		path << " -I fusbi.hex";
		std::string command = path.str();

		int retCmd = system(command.c_str());
		if (retCmd < 0){
			cout << "Calling fxload failed!" << endl;
			throw 1;
		}
		libusb_free_device_list(devs, 1);

		libusb_exit(this->ctx);
		sleep(3);
        
        r = libusb_init(&(this->ctx)); //initialize the library for the session we just declared
        if(r < 0) {
            cout<<"Init Error " << r << endl; //there was an error
            throw 1;
        }

        libusb_set_debug(this->ctx, 0); //set verbosity level to 3, as suggested in the documentation
        // get device list
        cnt = libusb_get_device_list(this->ctx, &devs); //get the list of devices
        if(cnt < 0) {
            cout<<"Get Device Error"<<endl; //there was an error
        }
        cout<<cnt<<" Devices in list."<<endl;

        // look for tmsi device
        for (int i = 0; i < cnt; i++){
            r = libusb_get_device_descriptor(devs[i], &desc);
            if (r < 0) {
                cout<<"failed to get device descriptor"<<endl;
            }
            // check vendor ID
            if (desc.idVendor == 0x0c7c){
                cout << "TMSI device found!" << endl;
                this->deviceFound = true;
                deviceCount = i;
                break;
            }
        }

        if (!this->deviceFound){
            cout << "No TMSI device found."<<endl;
            throw 1;
        }
	}

	// check product ID
	if (desc.idProduct != 0x0004){
		cout << "Calling fxload did not affect device!"<<endl;
		throw 1;
	}


	libusb_config_descriptor *config;
	libusb_get_config_descriptor(devs[deviceCount], 0, &config);
	const libusb_interface *inter;
	const libusb_interface_descriptor *interdesc;
	const libusb_endpoint_descriptor *epdesc;
    for(int i=0; i<(int)config->bNumInterfaces; i++) {
        inter = &config->interface[i];
        for(int j=0; j<inter->num_altsetting; j++) {
            interdesc = &inter->altsetting[j];
            if (((int)interdesc->bNumEndpoints) != 0){
				for(int k=0; k<(int)interdesc->bNumEndpoints; k++) {
					epdesc = &interdesc->endpoint[k];
					if (((epdesc->bEndpointAddress & 0x80) == LIBUSB_ENDPOINT_OUT )
							&& ((epdesc->bmAttributes & 0x03) == LIBUSB_TRANSFER_TYPE_BULK)){
						cout << "BULK out endpoint found" << endl;
						this->bulk_send_endpoint = epdesc;
					} else if (((epdesc->bEndpointAddress  & 0x80) == LIBUSB_ENDPOINT_IN )
							&& ((epdesc->bmAttributes & 0x03) == LIBUSB_TRANSFER_TYPE_BULK)){
						cout << "BULK in endpoint found" << endl;
						this->bulk_recv_endpoint = epdesc;
						for(int j = 0; j < BULK_RECV_URBS; ++j) {
							this->bulk_recv_transfer[j] = libusb_alloc_transfer(0);
							this->bulk_recv_buffer[j] = (unsigned char*) malloc(epdesc->wMaxPacketSize);
						}
					} else if (((epdesc->bEndpointAddress & 0x80) == LIBUSB_ENDPOINT_IN )
							&& ((epdesc->bmAttributes & 0x03) == LIBUSB_TRANSFER_TYPE_ISOCHRONOUS)){
						cout << "ISOCHRONOUS in endpoint found" << endl;
						this->isoc_recv_endpoint = epdesc;
						for(int j = 0; j < BULK_RECV_URBS; ++j) {
							this->isoc_recv_transfer[j] = libusb_alloc_transfer(1);
							this->isoc_recv_buffer[j] = (unsigned char*) malloc(epdesc->wMaxPacketSize);
						}
					}
				}
            }
        }
    }

	// Check if all required endpoints are present
	if (!(this->bulk_recv_endpoint->bEndpointAddress && this->bulk_send_endpoint->bEndpointAddress && this->isoc_recv_endpoint->bEndpointAddress)) {
		cout << "Could not find the required USB endpoints (bulk in/out, isochronous in)" << endl;
		throw 1;
	}

	// Check if device is attached to an USB2 interface */
	if(libusb_get_device_speed(devs[deviceCount]) != LIBUSB_SPEED_HIGH) {
		cout << "Device is not attached to an USB2 bus" << endl;
		throw 1;
	}

	// initialize all attributes
	nrofuserchannels = 0;
	currentsampleratesetting = 0;
	mode = 0;
	maxRS232 = 0;
	serialnumber = 0;
	nrEXG = 0;
	nrAUX = 0;
	hwversion = 0;
	swversion = 0;
	cmdbufsize = 0;
	sendbufsize = 0;
	nrofswchannels = 0;
	basesamplerate = 0;
	verbose = 1;
	lastError=0x00;
	cur_packet = 0x00;
	power=0;
	hardwarecheck=0;
	lastError=0;
	last_packet = 0xFF;
	m_frontEndInfoRec = false;

	// initialize all user settings
	m_freqFactor = freqFactor;
	m_numOfCh = numOfCh;
	m_out_size = out_size;
	m_aux = aux;
	m_digital = digital;
	outputMatrix = new double*[m_numOfCh];
	matCount = 0;

	for (int i = 0; i < m_numOfCh; i++){
		outputMatrix[i] = new double[m_out_size];
		m_channels[i] = ch[i] - 1;
	}

    cout << "------------------Initialization done!" << endl;
	libusb_free_device_list(devs, 1);
	this->deviceOpen = false;
    //libusb_free_config_descriptor(config);
#endif
}


tmsiDevice::~tmsiDevice()
{
#ifdef WITH_HW
	for(int i = 0; i < BULK_RECV_URBS; i++){
		free(this->bulk_recv_buffer[i]);
		libusb_free_transfer(this->bulk_recv_transfer[i]);
		free(this->isoc_recv_buffer[i]);
		libusb_free_transfer(this->isoc_recv_transfer[i]);
	}
	// initialize buffer
	free(this->bulk_recv_buffer);
	free(this->bulk_recv_transfer);
	free(this->isoc_recv_buffer);
	free(this->isoc_recv_transfer);

	// free output buffer matrix
	for (int i = 0; i < m_numOfCh; i++){
		delete outputMatrix[i];
	}
	delete outputMatrix;

	// destroy mutex
	pthread_mutex_destroy(&(this->mutexBulkRec));
#endif
}

int tmsiDevice::write(int16_t *msg, int32_t n_write){
#ifdef WITH_HW
	int32_t n = n_write / 2; // n_write is the size
	// return value
	int r = 0;
	// Verify that we actually have some data to
	if (n == 0)
		return 0;

	// Create an transfer packet
	libusb_transfer* trans = NULL;
	trans = libusb_alloc_transfer(0);
	if (!trans) {
		return -1;
	}

	// put 16 bit words into unsigned character buffer
	unsigned char data[2 * n];
	for (int i = 0; i < n; i++){
		data[2*i] = static_cast<unsigned char>(msg[i] & 0x00FF);
		data[2*i+1] = static_cast<unsigned char>((msg[i] >> 8) & 0x00FF);
	}

	// Initialize the transfer packet
	libusb_fill_bulk_transfer(trans, this->usb_dev, this->bulk_send_endpoint->bEndpointAddress,
							  data, 2*n, tmsi_write_bulk_callback, NULL, 0);

	// Send the data out the bulk port
	r = libusb_submit_transfer(trans);
	if (r) {
		cout << "Failed submitting the transfer packet" << endl;
		return -1;
	}

	return n;
#endif
}

size_t tmsiDevice::readSample(int16_t *msg, uint32_t n){
#ifdef WITH_HW
	size_t true_count;

	pthread_mutex_lock(&mutexBulkRec);

	// Read from user buffer
	true_count = this->sample_buffer.size();
	if (true_count >= n){
		true_count = n;
	}
	for (unsigned int i = 0; i < true_count; i++){
		msg[i] = this->sample_buffer.front();
		this->sample_buffer.pop();
	}
	
	pthread_mutex_unlock(&mutexBulkRec);

	// return number of read values
	return true_count;
#endif
}

size_t tmsiDevice::readCommand(int16_t *msg, uint32_t n){
#ifdef WITH_HW
	size_t true_count;

	pthread_mutex_lock(&mutexBulkRec);

	// Read from user buffer
	true_count = this->command_buffer.size();
	if (true_count >= n){
		true_count = n;
	}
	for (uint32_t i = 0; i < true_count; i++){
		msg[i] = this->command_buffer.front();
		this->command_buffer.pop();
	}

	pthread_mutex_unlock(&mutexBulkRec);

	// return number of read values
	return true_count;
#endif
}

int tmsiDevice::open()
{
#ifdef WITH_HW
	int r = 0;

	// try to open device
	if (this->deviceFound){
		this->usb_dev = libusb_open_device_with_vid_pid(ctx, USB_TMSI_VENDOR_ID, USB_TMSI_PRODUCT_ID); //these are vendorID and productID I found for my usb device
	    if(this->usb_dev == NULL){
			cout<<"Cannot open device"<<endl;
			goto err;
	    }
		else{
			cout<<"Device Opened"<<endl;
			this->deviceOpen = true;
		}
	}

	cout << "Initializing the event handler thread" << endl;
	// set attributes
	pthread_attr_t attr;
	struct sched_param sched_param;

	// Prepare task attributes
	pthread_attr_init(&attr);
	pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&attr, SCHED_FIFO);

	// Starting the base rate thread
	sched_param.sched_priority = sched_get_priority_max(SCHED_FIFO);
	pthread_attr_setschedparam(&attr, &sched_param);
	pthread_create(&(this->eventhandler_thread), &attr, eventhandler, (void *) this);
	pthread_attr_destroy(&attr);

	// set configuration before claiming the interface
	r = libusb_set_configuration(this->usb_dev, 1);
	if (r != 0){
		cout << "Configuration error " << r << endl;
		goto err;
	}

    // claim interface
	if(libusb_kernel_driver_active(this->usb_dev, 0) == 1) { //find out if kernel driver is attached
		cout<<"Kernel Driver Active"<<endl;
		if(libusb_detach_kernel_driver(this->usb_dev, 0) == 0) { //detach it
			cout<<"Kernel Driver Detached!"<<endl;
		} else {
		  cout<<"Kernel Driver detach failed!"<<endl;
		  goto err;
		  
		}
	}
	r = libusb_claim_interface(this->usb_dev, 0); //claim interface 0 (the first) of device (mine had jsut 1)
	if(r < 0) {
		cout<<"Cannot Claim Interface "<< r << endl;
		goto err;
	}
	cout<<"Claimed Interface"<<endl;

	// set alternate settings of device
	r = libusb_set_interface_alt_setting(this->usb_dev, 0, 1);
	if (r != 0){
		cout << "Setting the alternate settings failed with " << r <<endl;
		goto err;
	}
	// Setup incoming databuffer
	pthread_mutex_unlock (&(this->mutexBulkRec));

	// Setup initial bulk receive URB and submit
	for(int i = 0; i < BULK_RECV_URBS; i++) {
        libusb_fill_bulk_transfer(this->bulk_recv_transfer[i], this->usb_dev, this->bulk_recv_endpoint->bEndpointAddress,
        						  this->bulk_recv_buffer[i] , this->bulk_recv_endpoint->wMaxPacketSize ,
								  tmsi_read_bulk_callback, (void *) this, 0U);

        r = libusb_submit_transfer(this->bulk_recv_transfer[i]);

		if(r != 0) {
			cout << "Failed submitting bulk read, error" << r << "\n";
			goto err;
		}
	}

	// Setup initial isochronous receive URB's and submit
	for(int i = 0; i < ISOC_RECV_URBS; i++) {
		libusb_fill_iso_transfer(this->isoc_recv_transfer[i], this->usb_dev, this->isoc_recv_endpoint->bEndpointAddress,
								 this->isoc_recv_buffer[i], this->isoc_recv_endpoint->wMaxPacketSize,
								 1, tmsi_read_isoc_callback, (void*) this, 0);
		libusb_set_iso_packet_lengths(this->isoc_recv_transfer[i], this->isoc_recv_endpoint->wMaxPacketSize);
		int r = libusb_submit_transfer(this->isoc_recv_transfer[i]);
		if(r) {
			cout << "Failed to submit isochronous read transfer " << i << " with " << r << endl;
			goto err;
		}
	}

	// request frontend information
	this->frontendInfoReq();

	return 0;
	
err:
	// HAHA, at this place everything that has been allocated above should be undone.
	// However, this makes no fun to implement this in C and hence it's skipped here.
	// It's time for modern programming languages.
	
	cerr << "TMSI initialisation has beed aborted" << "\n";
	
        return -1;
    
    
    
#endif
}


//
//                      R/W callback functions
//
void tmsi_write_bulk_callback(struct libusb_transfer* trans)
{
#ifdef WITH_HW
	// cout << "WRITE Callback entered!" << endl;

	switch(trans->status) {
		case LIBUSB_TRANSFER_COMPLETED: {
			// Packet written
//			cout << "WRITE Transfer completed" << endl;
		  
			libusb_free_transfer(trans);
			break;
		}

		case LIBUSB_TRANSFER_STALL: {
			// This error occurs when an endpoint has stalled.
			cout << "WRITE The bulk endpoint has stalled. (size = " << trans->actual_length << ")" << endl;
			break;
		}

		case LIBUSB_TRANSFER_CANCELLED: {
			// Unknown error
			cout << "WRITE Transfer cancelled." << endl;
			break;
		}

		case LIBUSB_TRANSFER_NO_DEVICE: {
			// Unknown error
			cout << "WRITE No device at callback." << endl;
			break;
		}

		case LIBUSB_TRANSFER_ERROR: {
			// Unknown error
			cout << "WRITE Transfer error occurred." << endl;
			break;
		}

		case LIBUSB_TRANSFER_OVERFLOW: {
			// Unknown error
			cout << "WRITE Transfer overflow occurred." << endl;
			break;
		}

		case LIBUSB_TRANSFER_TIMED_OUT: {
			// Unknown error
			cout << "WRITE Transfer time out elapsed." << endl;
			break;
		}


		default: {
			// Unknown error
			cout << "Unknown error occurred." << endl;
			break;
		}
	}
//
#endif
}


void tmsi_read_bulk_callback(struct libusb_transfer* trans)
{
#ifdef WITH_HW
	// reinterpret the user data to get all device information and buffer pointers
	tmsiDevice* tmsi = reinterpret_cast<tmsiDevice*>(trans->user_data);
	cout << "READ Callback entered!" << endl;

	switch(trans->status) {
		case LIBUSB_TRANSFER_COMPLETED: {
			cout << "READ Transfer completed with "<< trans->actual_length << endl;
			// Packet received is OK and cleared the buffer completely
			// Enqueue packet in user buffer

			if (trans->actual_length % 2){
				cout << "Odd number of bytes received." << endl;
			}

			unsigned char low;
			unsigned char high;
			int length = 0;

			pthread_mutex_lock(&(tmsi->mutexBulkRec));
			for (int i = 0; i < trans->actual_length; i++){
				if (i%2) {
					high = trans->buffer[i];
					tmsi->command_buffer.push(((static_cast<int16_t>(high)<<8) & 0xFF00)
							+ (static_cast<int16_t>(low) & 0x00FF));
					length++;
				} else {
					low = trans->buffer[i];
				}
			}
			pthread_mutex_unlock(&(tmsi->mutexBulkRec));

			// receive commands from tmsi device
			if (length > 4){
				tmsi->receiveNextCommand(length);
			}
			break;
		}

		case LIBUSB_TRANSFER_STALL: {
			// This error occurs when an endpoint has stalled.
			cout << "The bulk endpoint has stalled. (size = " << trans->actual_length << ")" << endl;
			break;
		}

		case LIBUSB_TRANSFER_CANCELLED: {
			// Unknown error
			//cout << "READ Transfer cancelled." << endl;
			break;
		}

		case LIBUSB_TRANSFER_NO_DEVICE: {
			// Unknown error
			//cout << "READ No device at callback." << endl;
			break;
		}

		case LIBUSB_TRANSFER_ERROR: {
			// Unknown error
			cout << "READ Transfer error occurred." << endl;
			break;
		}

		case LIBUSB_TRANSFER_OVERFLOW: {
			// Unknown error
			cout << "READ Transfer overflow occurred." << endl;
			break;
		}

		case LIBUSB_TRANSFER_TIMED_OUT: {
			// Unknown error
			cout << "READ Transfer time out elapsed." << endl;
			break;
		}


		default: {
			// Unknown error
			cout << "READ Unknown error occurred." << endl;
			break;
		}
	}

	int r = libusb_submit_transfer(trans);
	if (r != 0){
		//cout << "READ sensing the read request failed." << endl;
	}
#endif
}


void tmsi_read_isoc_callback(struct libusb_transfer* trans)
{
#ifdef WITH_HW
	// reinterpret the user data to get all device information and buffer pointers
	tmsiDevice* tmsi = reinterpret_cast<tmsiDevice*>(trans->user_data);
	// cout << "ISOC Callback entered!" << endl;
	switch(trans->status) {
		case LIBUSB_TRANSFER_COMPLETED: {
//			cout << "ISOC Transfer completed with "<< trans->actual_length << " and " <<
//				 trans->iso_packet_desc[0].actual_length << endl;
			// Packet received is OK and cleared the buffer completely
			// Enqueue packet in user buffer
//			if (trans->iso_packet_desc[0].actual_length % 2){
//				cout << "Odd number of bytes received." << endl;
//			}

			unsigned char low;
			unsigned char high;
			int length = 0;

			pthread_mutex_lock(&(tmsi->mutexBulkRec));
			for (unsigned int i = 0; i < trans->iso_packet_desc[0].actual_length; i++){
				if (i%2) {
					high = trans->buffer[i];
					tmsi->sample_buffer.push(((static_cast<int16_t>(high)<<8) & 0xFF00)
							+ (static_cast<int16_t>(low) & 0x00FF));
					length++;
				} else {
					low = trans->buffer[i];
				}
			}
			pthread_mutex_unlock(&(tmsi->mutexBulkRec));

			if (length > 4){
				tmsi->receiveNextSample(length);
			}

			break;
		}

		case LIBUSB_TRANSFER_STALL: {
			// This error occurs when an endpoint has stalled.
			cout << "ISOC The isoc endpoint has stalled. (size = " << trans->iso_packet_desc[0].actual_length << ")" << endl;
			break;
		}

		case LIBUSB_TRANSFER_CANCELLED: {
			// Unknown error
			//cout << "ISOC Transfer cancelled." << endl;
			break;
		}

		case LIBUSB_TRANSFER_NO_DEVICE: {
			// Unknown error
			//cout << "ISOC No device at callback." << endl;
			break;
		}

		case LIBUSB_TRANSFER_ERROR: {
			// Unknown error
			cout << "ISOC Transfer error occurred." << endl;
			break;
		}

		case LIBUSB_TRANSFER_OVERFLOW: {
			// Unknown error
			cout << "ISOC Transfer overflow occurred." << endl;
			break;
		}

		case LIBUSB_TRANSFER_TIMED_OUT: {
			// Unknown error
			cout << "ISOC Transfer time out elapsed." << endl;
			break;
		}


		default: {
			// Unknown error
			cout << "ISOC Unknown error occurred." << endl;
			break;
		}
	}

	int r = libusb_submit_transfer(trans);
	if (r != 0){
		// cout << "READ sensing the read request failed." << endl;
	}
#endif
}




int tmsiDevice::close()
{
#ifdef WITH_HW
	int r = 0;

	// Unlink current receiving bulk URB
	for(int i = 0; i < BULK_RECV_URBS; i++){
		r = libusb_cancel_transfer(this->bulk_recv_transfer[i]);
		if (r != 0){
			cout << "BULK Cancelation does not work and returned " << r << endl;
		}
	}
	// Unlink current receiving isochronous URB's
	for(int i = 0; i < ISOC_RECV_URBS; i++){
		r = libusb_cancel_transfer(this->isoc_recv_transfer[i]);
		if (r != 0){
			cout << "ISO Cancelation does not work and returned " << r << endl;
		}
	}

	if(this->deviceOpen){
		this->deviceOpen = false;
	}

	r = libusb_release_interface(this->usb_dev, 0);
	if (r){
		cout << "Release of interface was not successful and returned " << r << endl;
	}


	libusb_close(this->usb_dev);
	libusb_exit(this->ctx);
	return r;
#endif
}

bool tmsiDevice::isOpen(){
#ifdef WITH_HW
	return this->deviceOpen;
#endif
}

void* eventhandler(void* p){
#ifdef WITH_HW
	cout << "Event handler thread started" << endl;

	// reinterpret the user data to get all device information and buffer pointers
	tmsiDevice* tmsi = reinterpret_cast<tmsiDevice*>(p);
	int r = 0;

	while (tmsi->isOpen()){
		// handle events has to be called continiously
		r = libusb_handle_events(tmsi->ctx);
		if (r != 0){
			cout << "EVENT: The error " << r << " occurred." << endl;
		}
	}

	pthread_exit(NULL);
#endif
}


int tmsiDevice::writeCommand(int16_t descriptor,const std::vector<int16_t> &datablock){

	int16_t n = (descriptor&0x00FF);
	int16_t com[n+3];
	com[0] = BLOCKSYNC;
	int16_t sum = BLOCKSYNC;
	com[1] = descriptor;
	sum += descriptor;

	for (int i = 0; i < n; i++){
		sum += datablock.at(i);
		com[i+2] = datablock.at(i);
	}

	com[n+2] =- sum;

	return this->write(com,(int32_t)sizeof(com));
}

void tmsiDevice::frontendInfoReq(){

	int ret = writeCommand(0x0500, datablock);
	if (verbose){
		printf("Command  0x%X written (%d Bytes)!\n",0x0500,ret);
	}
}

int tmsiDevice::receiveNextCommand(int length){


	int16_t desc,size,type,sum;
	int packet=0;int nr=0;

	cout << "CMD State machine with " << length << " packets called." << endl;
	int16_t com[length];
	this->readCommand(com, length);

	while(nr < length) {
		//verify sync word
		if ((com[nr]&0xFFFF)!=BLOCKSYNC){
			if (verbose) printf("wrong beginning! %X (nr:%d length:%d)\n",com[nr]&0xFFFF,nr,length);
			while((com[nr]&0xFFFF)!=BLOCKSYNC && nr<length/2)
				nr++;
			if ((com[nr]&0xFFFF)!=BLOCKSYNC)
				return -1;
		}
		//read header
		desc=com[nr+1];
		size= desc & 0xFF;
		type=desc>>8;

		//verify checksum
		sum = 0;
		for(int i = 0; i < size+3; i++) sum += com[i];
		if (sum != 0) {
			if (verbose) printf("wrong checksum: %X (chksum word: %X)!\n",sum&0xFFFF,com[size+2]&0xFFFF);
			nr+=size+3;
			last_packet=0xFF;//reset last packet number
			continue;//go to next packet, if any
		}
		nr+=2; //BLOCKSYNC and desc


		int16_t lastDescr;
		switch(type){
			case 0x00: //Acknowledge
				lastDescr= com[nr];
				nr++;
				lastError=com[nr];
				nr++;
				printAcknowledge(lastDescr,lastError);
				break;
			case 0x02: //FrontendInfo
				if (!m_frontEndInfoRec){
					nrofuserchannels=com[nr];
					currentsampleratesetting=com[nr+1];
					mode=com[nr+2];
					maxRS232=com[nr+3];
					serialnumber=convertWORD2LONG(com[nr+4],com[nr+5]);
					nrEXG=com[nr+6];
					nrAUX=com[nr+7];
					hwversion=com[nr+8];
					swversion=com[nr+9];
					cmdbufsize=com[nr+10];
					sendbufsize=com[nr+11];
					nrofswchannels=com[nr+12];
					basesamplerate=com[nr+13];
					power=com[nr+14];
					hardwarecheck=com[nr+15];
					nr+=16;
					this->m_frontEndInfoRec = true;
					if (this->verbose){
						this->printFrontendInfo();
					}
				} else {
					cout << "Second front end info received!" << endl;
				}
				break;
			case 0x04: //FrondendIdle
				printf("FrondendIdle, size %d\n",size);
				break;
			case 0x07: //RTCData
				printf("RTCData, size %d\n",size);
				break;
			case 0x12: //StimStatus
				printf("StimStatus, size %d\n",size);
				break;
			case 0x16: //ImpData
				printf("ImpData, size %d\n",size);
				break;
			case 0x19: //SensorStatus
				printf("SensorStatus, size %d\n",size);
				break;
			case 0x1B: //DeltaData
				printf("DeltaData, size %d\n",size);
				break;
			case 0x1F: //RTCTimeData
				printf("RTCTimeData, size %d\n",size);
				break;
			case 0x21: //RTCAlarmData
				printf("RTCAlarmData, size %d\n",size);
				break;
			case 0x23: //IDData
				printf("IDDATA, size %d\n",size);
				break;
			case 0x25: //SignalCorrection
				printf("SignalCorrection, size %d\n",size);
				break;
			default:
				printf("Unrecognized message, size %d\n",size);
		}
		nr++; //checksum

		//printf("command %d finished (nr %d)\n",packet,nr);
		packet++;


	}
	return length;
}

int tmsiDevice::receiveNextSample(int length){

//	cout << "SAM State machine with " << length << " packets called." << endl;
	int16_t desc,size,type,sum,vecCount;
	int k=0;int packet=0;int nr=0;

	int16_t com[length];
	int ret = this->readSample(com, length);

	while(nr<length) {
		//verify sync word
		if ((com[nr]&0xFFFF)!=BLOCKSYNC){
			if (verbose) printf("wrong beginning! %X (nr:%d length:%d)\n",com[nr]&0xFFFF,nr,length);
			while((com[nr]&0xFFFF)!=BLOCKSYNC && nr<ret/2)
				nr++;
			if ((com[nr]&0xFFFF)!=BLOCKSYNC)
				return -1;
		}
		//read header
		desc=com[nr+1];
		size= desc & 0xFF;
		type=desc>>8;

		//verify checksum
		sum = 0;
		for(int i = 0; i < size+3; i++) sum += com[i];
		if (sum != 0) {
			if (verbose) printf("wrong checksum: %X (chksum word: %X)!\n",sum&0xFFFF,com[size+2]&0xFFFF);
			nr+=size+3;
			last_packet=0xFF;//reset last packet number
			continue;//go to next packet, if any
		}
		nr+=2; //BLOCKSYNC and desc

		switch(type){
			case 0x01: //ChannelData
//				if (verbose)
//					  printf("ChannelData no received\n");

				//verify if packet is in order (...,1,3,5,...,63,1,3,...)
				cur_packet = com[nr+75];
				if (last_packet == 0xFF) last_packet = cur_packet - 2;//first time
				//printf("diff %d.\n", abs_mod(cur_packet - last_packet,64));
				if ((abs_mod(cur_packet - last_packet,64)>2) &&(cur_packet!=0 && last_packet!=0)) {
					//one to three packets were skipped.
					/*printf("packet #%d arrived after #%d (%d skipped).\n",
							cur_packet, last_packet, abs_mod(cur_packet-last_packet,64)/2);*/
					//continue to next case
				}
					//packet received ok.
					last_packet = cur_packet;
					//read new measurement

					vecCount = 0;
					for (k=0; k<size-1;k=k+2){
						if (vecCount < nrofuserchannels) { // always only one sample vector is called: sizeof of array RcvSingleSample
							rcvSingleSample[vecCount] = convertWORD2LONG(com[nr+k+1],com[nr+k]);
							vecCount++;
						}
					}

					nr+=size;
					for (int i = 0; i < m_numOfCh; i++){
						double resolution = 1.0;
						if (m_channels[i] >= m_digital){
							resolution = 1.0;
						} else {
							if (m_channels[i] >= m_aux){
								resolution = 1.4305e-6;
							} else {
								resolution= 0.0715/1000;
							}
						}
						outputMatrix[i][matCount] = resolution * ((double) rcvSingleSample[m_channels[i]]);
					}
                    
					if (matCount+1 > this->m_out_size){
						matCount = 0;
						cout << "Buffer overflow catched. Reduce sample time!" << endl;
					} else {
                        matCount++;
                    }

//					// Run Callback
//					if (Callback != NULL) {
//					  (*Callback)(1, CallbackUserdata);
//					}


				break;
			default:
				printf("Unrecognized message, size %d\n",size);
		}
		nr++; //checksum

		packet++;


	}
	return ret;
}

int32_t tmsiDevice::convertWORD2LONG(int16_t wlow,int16_t whigh){
	int32_t l = wlow;
	if (wlow < 0){
		l += 65536;
	}
	l += whigh << 16;
	return l;
}

void tmsiDevice::writeFrontendInfo(){

	datablock.clear();
	datablock.reserve(16);
	datablock.push_back(nrofuserchannels);
	datablock.push_back(currentsampleratesetting);
	datablock.push_back(mode);
	datablock.push_back(maxRS232);
	datablock.push_back(serialnumber&0xFFFF);
	datablock.push_back((serialnumber>>16)&0xFFFF);
	datablock.push_back(nrEXG);
	datablock.push_back(nrAUX);
	datablock.push_back(hwversion);
	datablock.push_back(swversion);
	datablock.push_back(cmdbufsize);
	datablock.push_back(sendbufsize);
	datablock.push_back(nrofswchannels);
	datablock.push_back(basesamplerate);
	datablock.push_back(power);
	datablock.push_back(hardwarecheck);

	int ret=writeCommand(0x0210,datablock);
	if (verbose)
		printf("Command  0x%X written (%d Bytes)!\n",0x0210,ret);
}


void tmsiDevice::startDataSend(){
	while(!this->m_frontEndInfoRec){
		usleep(100);
	}
	this->currentsampleratesetting = m_freqFactor;
	this->mode = 0x02;
	this->writeFrontendInfo();
}

void tmsiDevice::stopDataSend(){
	this->mode=0x01;
	this->writeFrontendInfo();
}

void tmsiDevice::printFrontendInfo() {

	printf("nrofuserchannels %d\n",nrofuserchannels);
	printf("currentsampleratesetting %d\n",currentsampleratesetting);
	printf("mode %d\n",mode);
	printf("maxRS232 %d\n",maxRS232);
	printf("serialnumber %d\n",serialnumber);
	printf("nrEXG %d\n",nrEXG);
	printf("nrAUX %d\n",nrAUX);
	printf("hwversion %d\n",hwversion);
	printf("swversion %d\n",swversion);
	printf("cmdbufsize %d\n",cmdbufsize);
	printf("sendbufsize %d\n",sendbufsize);
	printf("nrofswchannels %d\n",nrofswchannels);
}

void tmsiDevice::printAcknowledge(int16_t descriptor,int16_t errorcode) {

	printf("# Ack: desc %04X err %04X", descriptor,errorcode);
	switch (errorcode) {
		case 0x01:
			printf(" unknown or not implemented blocktype\n"); break;
		case 0x02:
			printf(" CRC error in received block\n"); break;
		case 0x03:
			printf(" error in command data (can't do that)\n"); break;
		case 0x04:
			printf(" wrong blocksize (too large)\n"); break;
			// 0x0010 - 0xFFFF are reserved for user errors
		case 0x11:
			printf(" No external power supplied\n"); break;
		case 0x12:
			printf(" Not possible because the Front is recording\n"); break;
		case 0x13:
			printf(" Storage medium is busy\n"); break;
		case 0x14:
			printf(" Flash memory not present\n"); break;
		case 0x15:
			printf(" nr of words to read from flash memory out of range\n"); break;
		case 0x16:
			printf(" flash memory is write protected\n"); break;
		case 0x17:
			printf(" incorrect value for initial inflation pressure\n"); break;
		case 0x18:
			printf(" wrong size or values in BP cycle list\n"); break;
		case 0x19:
			printf(" sample frequency divider out of range (<0, >max)\n"); break;
		case 0x1A:
			printf(" wrong nr of user channels (<=0, >maxUSRchan)\n"); break;
		case 0x1B:
			printf(" adress flash memory out of range\n"); break;
		case 0x1C:
			printf(" Erasing not possible because battery low\n"); break;
		default:
			// 0x00 - no error, positive acknowledge
			printf("\n");
			break;
	}

}

int32_t tmsiDevice::getMatCount(){
	return this->matCount;
}
void tmsiDevice::resetMatCount(){
	this->matCount = 0;
}
