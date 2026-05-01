#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <semaphore.h>
#include <pthread.h>

// Global semaphore for limiting concurrent bids
extern sem_t active_bids_semaphore;

// Maximum number of bids that can be processed simultaneously
#define MAX_CONCURRENT_BIDS 3

// Function prototypes
void initialize_semaphore(void);
void wait_for_bidding_slot(void);
void release_bidding_slot(void);
void destroy_semaphore(void);

#endif