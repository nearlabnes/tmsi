#include "tmsiDevice.h"

#include <stdint.h>


static void init_tmsi(void** work, uint16_t freqFactor, int32_t aux, int32_t digital,
				int32_t out_size, int32_t* ch, int32_t numOfCh )
{
#ifdef WITH_HW
	tmsiDevice* tmsi = new tmsiDevice(freqFactor,  ch,  numOfCh,
			    					  out_size, aux, digital);

	tmsi->open();
	tmsi->startDataSend();
    
	*work = (void*) tmsi;
#endif
}


static void output_tmsi(void** work, double* y, int* numOfSam){
#ifdef WITH_HW
	tmsiDevice* tmsi = (tmsiDevice*) (*work);
 	int32_t count = tmsi->getMatCount();
    //printf("Count = %i\n", count);
 
	for (int j = 0; j < tmsi->m_numOfCh; j++){
		for (int i = 0; i < count; i++){
			y[j + tmsi->m_numOfCh * i] = tmsi->outputMatrix[j][i];
		}
		for (int i = count; i < tmsi->m_out_size; i++){
			y[j + tmsi->m_numOfCh * i] = -1.0;
		}
	}
	numOfSam[0] = count;
    tmsi->resetMatCount();
#endif
}

static void terminate_tmsi(void** work){
#ifdef WITH_HW
	tmsiDevice* tmsi = (tmsiDevice*) (*work);
    tmsi->stopDataSend();
    tmsi->close();
    delete tmsi;
#endif
}

