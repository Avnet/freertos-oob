#ifndef STTS22HTR_H_
#define STTS22HTR_H_

#include "stdint.h"

int32_t stts22htr_setup(void);
int32_t stts22htr_get_temp(float *temp);
int32_t stts22htr_run_example(void);

#endif /* STTS22HTR_H_ */
