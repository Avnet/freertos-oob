#ifndef QSPI_H_
#define QSPI_H_

#include "stdint.h"

#define MAX_LOGFILE_SIZE 0x8000
#define QSPI_USER_PART_OFFSET 0x01c00000

int32_t qspi_setup(void);
int32_t qspi_retrieve_logfile(uint8_t *ReadBfrPtr, uint32_t *logfile_size);

#endif /* QSPI_H_ */
