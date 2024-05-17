#ifndef DLT_ENDPOINTS_H_
#define DLT_ENDPOINTS_H_

#include <zephyr/kernel.h>

/* Set the number of endpoints */
#define DLT_NUM_ENDPOINTS 1 

/* Define your endpoints*/
enum dlt_endpoints {
    NRF_NUS = 0,
};

#endif // DLT_ENDPOINTS_H_