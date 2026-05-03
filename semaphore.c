#include "semaphore.h"
#include <stdio.h>
#include <stdlib.h>
// Global semaphore definition
sem_t active_bids_semaphore;

void initialize_semaphore(void) {
    // Initialize semaphore with MAX_CONCURRENT_BIDS available slots
    // Second parameter 0 means semaphore is shared between threads (not processes)
    if (sem_init(&active_bids_semaphore, 0, MAX_CONCURRENT_BIDS) != 0) {
        printf("[ERROR] Failed to initialize semaphore!\n");
        exit(1);
    }
    printf("[SEMAPHORE] Initialized with %d concurrent bid slots\n", MAX_CONCURRENT_BIDS);
}

void wait_for_bidding_slot(void) {
    // Wait (block) until a bidding slot is available
    // This decrements the semaphore count
    sem_wait(&active_bids_semaphore);
    
    // Optional: Print current available slots (for debugging)
    int current_value;
    sem_getvalue(&active_bids_semaphore, &current_value);
    printf("[SEMAPHORE] Bid started. %d slots remaining\n", current_value);
}

void release_bidding_slot(void) {
    // Release a bidding slot back to the pool
    // This increments the semaphore count
    sem_post(&active_bids_semaphore);
    
    // optional: Print current available slots (for debugging)
    int current_value;
    sem_getvalue(&active_bids_semaphore, &current_value);
    printf("[SEMAPHORE] Bid finished. %d slots available\n", current_value);
}

void destroy_semaphore(void) {
    sem_destroy(&active_bids_semaphore);
    printf("[SEMAPHORE] Semaphore destroyed\n");
}
