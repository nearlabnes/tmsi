#include <iostream>
#include <bitset>
#include <unistd.h>
#include <libusb-1.0/libusb.h>

#include "tmsiDevice.hpp"


using namespace std;


int main() {

	uint16_t freqFactor = 0; // for highest frequency
	int32_t aux = 29; // for the porti device the aux channel is 29
	int32_t digital = 30; // for the porti device the digital channels start from 30
	int32_t outSize = 200; // vector size of the outputed data
	int32_t ch[2] = {17, 18};
	int32_t numOfCh = 2;

	// configure, open and start tmsi
    tmsiDevice* tmsi = new tmsiDevice(freqFactor, ch, numOfCh, outSize, aux, digital);
    tmsi->open();
    tmsi->startDataSend();

    int i = 0;
    while(i < 2){
    	sleep(1);
    	// here the data gets buffered inside tmsi->outputMatrix
    	// the current count inside the matrix can be viewed with tmsi->matCount
    	// if the matrix index overflows there will be an error message.
    	// This message indicates that the matrix was not read and the index gets reseted.
    	// So there has to be a thread/task whatever which listens on the matCount and copies the data
    	// or directly works inside the class.
    	i++;
    }

	tmsi->stopDataSend(); // has to be called always at the end otherwise the tmsi get stuck.
	// if this runs there will be a lot of error messages due to the closed
	// callbacks on the libusb but nothing to be worry about
	tmsi->close();
    delete tmsi;
}
