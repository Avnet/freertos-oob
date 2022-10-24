#ifndef LPS22HHTR_H_
#define LPS22HHTR_H_

#include "stdint.h"

int32_t lps22hhtr_setup(void);
int32_t lps22hhtr_get_pressure(float *pressure);

#endif /* LPS22HHTR_H_ */
