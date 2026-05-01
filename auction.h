#ifndef AUCTION_H
#define AUCTION_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_ITEMS 3
#define MAX_BIDS 20

// Struct definitions
typedef struct {
    int item_id;
    int current_price;
    int highest_bidder_id;
    int is_active;
    pthread_mutex_t mutex;
} auction_item;

typedef struct {
    int bidder_id;
    int item_id;
    int bid_amount;
} bid_action;

// Global declarations
extern auction_item items[MAX_ITEMS];
extern pthread_mutex_t log_mutex;

// Function prototypes
void initialize_auction_items(void);
void* process_bid(void* arg);
void print_auction_results(void);
void cleanup_auction_items(void);

#endif