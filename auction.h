#ifndef AUCTION_H
#define AUCTION_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define MAX_ITEMS 3
#define MAX_BIDS 20
#define MAX_BIDDERS 10
#define MAX_HISTORY 1000

// ==================== STRUCT DEFINITIONS ====================

// Auction Item Structure (Shared Resource)
typedef struct {
    int item_id;
    char item_name[64];
    int current_price;
    int highest_bidder_id;
    int is_active;
    time_t end_time;                    // Auction end timestamp for timeout
    pthread_mutex_t mutex;
    pthread_cond_t cond;                // Condition variable for timeout
} auction_item;

// Bidder Structure (for tracking auto-bidding)
typedef struct {
    int bidder_id;
    char bidder_name[32];
    int budget;                         // Maximum budget
    int auto_bid_increment;             // How much to increase when outbid
    int is_active;
    int current_item_bidding;           // Item they're currently bidding on (-1 if none)
} bidder;

// Bid Action Structure (passed to thread)
typedef struct {
    int bidder_id;
    int item_id;
    int bid_amount;
    int is_auto_bid;                    // Whether this is an auto-bid
} bid_action;

// Auction History Record
typedef struct {
    int bid_id;
    int bidder_id;
    int item_id;
    int bid_amount;
    time_t timestamp;
    int was_accepted;
    char notification_sent[128];        // Record of notification sent
} bid_history;

// Auction Statistics Structure
typedef struct {
    int total_bids_placed;
    int total_accepted_bids;
    int total_rejected_bids;
    int highest_bid_ever;
    int lowest_bid_ever;
    int total_bid_sum;
    float average_bid;
    int bids_per_item[MAX_ITEMS];
    int revenue_per_item[MAX_ITEMS];
    int auto_bids_placed;               // Count of auto-bids
    int active_bidders_count;           // Peak concurrent bidders
} auction_stats;

// ==================== GLOBAL DECLARATIONS ====================

extern auction_item items[MAX_ITEMS];
extern bidder bidders[MAX_BIDDERS];
extern bid_history auction_history[MAX_HISTORY];
extern auction_stats stats;
extern int history_count;
extern int num_bidders;

// Mutexes
extern pthread_mutex_t log_mutex;
extern pthread_mutex_t history_mutex;
extern pthread_mutex_t stats_mutex;
extern pthread_mutex_t bidder_mutex;

// ==================== FUNCTION PROTOTYPES ====================

// Core auction functions
void initialize_auction_items(void);
void initialize_bidders(void);
void* process_bid(void* arg);
void* auction_timer_thread(void* arg);

// Auto-bidding functions
int check_and_place_auto_bid(int bidder_id, int item_id, int current_price);
void notify_outbid(int bidder_id, int item_id, int new_price, int new_highest_bidder);

// History functions
void log_to_history(int bidder_id, int item_id, int bid_amount, int accepted, const char* notification);
void save_history_to_file(void);
void print_history(void);

// Statistics functions
void update_statistics(int item_id, int bid_amount, int accepted, int is_auto_bid);
void print_statistics(void);
void update_peak_concurrent_bidders(int current_active);

// Notification functions
void broadcast_notification(const char* message, int exclude_bidder);
void send_private_notification(int bidder_id, const char* message);

// Cleanup functions
void cleanup_auction_items(void);
void print_auction_results(void);

#endif