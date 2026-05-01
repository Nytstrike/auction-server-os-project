#include "auction.h"
#include "semaphore.h"
#include <unistd.h>

// Global definitions
auction_item items[MAX_ITEMS];
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void initialize_auction_items(void) {
    printf("\n=== Initializing Auction Items ===\n");
    
    for (int i = 0; i < MAX_ITEMS; i++) {
        items[i].item_id = i;
        items[i].current_price = 100 + (i * 50);  // Item 0: $100, Item 1: $150, Item 2: $200
        items[i].highest_bidder_id = -1;
        items[i].is_active = 1;
        
        // Initialize mutex for each item
        if (pthread_mutex_init(&items[i].mutex, NULL) != 0) {
            printf("Error initializing mutex for item %d\n", i);
        }
        
        printf("Item %d: Starting price = $%d\n", i, items[i].current_price);
    }
    printf("Auction Ready! %d items available\n\n", MAX_ITEMS);
}

void* process_bid(void* arg) {
    bid_action* bid = (bid_action*)arg;
    
    // Wait for a semaphore slot before processing (LIMITS CONCURRENT BIDS)
    wait_for_bidding_slot();
    
    printf("[BID THREAD %lu] Processing bid: Bidder %d -> Item %d -> $%d\n",
           pthread_self(), bid->bidder_id, bid->item_id, bid->bid_amount);
    
    // Validate item index
    if (bid->item_id < 0 || bid->item_id >= MAX_ITEMS) {
        pthread_mutex_lock(&log_mutex);
        printf("[ERROR] Invalid item ID: %d\n", bid->item_id);
        pthread_mutex_unlock(&log_mutex);
        free(bid);
        release_bidding_slot();  // Release semaphore before exiting
        return NULL;
    }
    
    // Lock the specific item being bid on (CRITICAL SECTION)
    pthread_mutex_lock(&items[bid->item_id].mutex);
    
    // Simulate bid processing time (makes semaphore effect visible)
    usleep(100000);  // 100ms delay
    
    // Check if auction item is still active
    if (!items[bid->item_id].is_active) {
        pthread_mutex_lock(&log_mutex);
        printf("[BID REJECTED] Item %d auction has ended!\n", bid->item_id);
        pthread_mutex_unlock(&log_mutex);
        
        pthread_mutex_unlock(&items[bid->item_id].mutex);
        free(bid);
        release_bidding_slot();  // Release semaphore
        return NULL;
    }
    
    // Check if bid amount is higher than current price (BID VALIDATION)
    if (bid->bid_amount > items[bid->item_id].current_price) {
        // ATOMIC UPDATE: Update the shared auction state
        int old_price = items[bid->item_id].current_price;
        items[bid->item_id].current_price = bid->bid_amount;
        items[bid->item_id].highest_bidder_id = bid->bidder_id;
        
        // Log the successful bid (protected by log_mutex to avoid interleaved output)
        pthread_mutex_lock(&log_mutex);
        printf("\n✅ [BID ACCEPTED] Item %d: $%d -> $%d (Bidder %d)\n",
               bid->item_id, old_price, bid->bid_amount, bid->bidder_id);
        pthread_mutex_unlock(&log_mutex);
        
        pthread_mutex_unlock(&items[bid->item_id].mutex);
    } else {
        // Bid too low
        pthread_mutex_lock(&log_mutex);
        printf("❌ [BID REJECTED] Item %d: $%d is not > current $%d (Bidder %d)\n",
               bid->item_id, bid->bid_amount, 
               items[bid->item_id].current_price, bid->bidder_id);
        pthread_mutex_unlock(&log_mutex);
        
        pthread_mutex_unlock(&items[bid->item_id].mutex);
    }
    
    free(bid);  // Clean up the bid action structure
    
    // Release semaphore slot when done processing
    release_bidding_slot();
    
    return NULL;
}

void print_auction_results(void) {
    printf("\n========================================\n");
    printf("           FINAL AUCTION RESULTS\n");
    printf("========================================\n");
    
    for (int i = 0; i < MAX_ITEMS; i++) {
        printf("\nItem %d:\n", i);
        printf("  Final Price: $%d\n", items[i].current_price);
        
        if (items[i].highest_bidder_id != -1) {
            printf("  Winning Bidder: %d\n", items[i].highest_bidder_id);
            printf("  Status: ✅ SOLD\n");
        } else {
            printf("  Winning Bidder: None\n");
            printf("  Status: ❌ UNSOLD (No bids placed)\n");
        }
    }
    printf("\n========================================\n");
}

void cleanup_auction_items(void) {
    for (int i = 0; i < MAX_ITEMS; i++) {
        pthread_mutex_destroy(&items[i].mutex);
    }
    pthread_mutex_destroy(&log_mutex);
    printf("Cleanup complete. Mutexes destroyed.\n");
}