#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <semaphore.h>
#include <pthread.h>

// a global semaphore for limiting concurrent bids
extern sem_t active_bids_semaphore;

// Max bids that can be processed simultaneously
#define MAX_CONCURRENT_BIDS 3

// Function prototypes
void initialize_semaphore(void);
void wait_for_bidding_slot(void);
void release_bidding_slot(void);
void destroy_semaphore(void);

#endif
