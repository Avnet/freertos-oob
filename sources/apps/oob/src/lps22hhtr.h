#ifndef LPS22HHTR_H_
#define LPS22HHTR_H_

#include "stdint.h"

int32_t lps22hhtr_setup(void);
int32_t lps22hhtr_get_pressure(float *pressure);
int32_t lps22hhtr_run_example(void);

#endif /* LPS22HHTR_H_ */
