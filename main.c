#include "auction.h"
#include "semaphore.h"
#include <time.h>
#include <unistd.h>

int main() {
    pthread_t threads[MAX_BIDS];
    int num_bids = 0;
    
    printf("========================================\n");
    printf("   REAL-TIME AUCTION BIDDING SYSTEM\n");
    printf("   Multi-Threaded Auction Simulator\n");
    printf("   WITH SEMAPHORE CONCURRENCY CONTROL\n");
    printf("========================================\n");
    
    // Step 1: Initialize semaphore (limits concurrent bids)
    initialize_semaphore();
    
    // Step 2: Initialize auction items
    initialize_auction_items();
    
    // Step 3: Seed random number generator
    srand(time(NULL));
    
    // Step 4: Simulate random bids (each bid gets its own thread)
    printf("\n--- Starting Bidding Phase ---\n");
    printf("Creating %d bid threads...\n", MAX_BIDS);
    printf("Maximum %d bids can be processed concurrently\n\n", MAX_CONCURRENT_BIDS);
    
    for (int i = 0; i < MAX_BIDS; i++) {
        // Create a new bid action
        bid_action* new_bid = (bid_action*)malloc(sizeof(bid_action));
        
        if (new_bid == NULL) {
            printf("Memory allocation failed!\n");
            break;
        }
        
        // Generate random bid data
        new_bid->bidder_id = rand() % 10;           // Bidder ID: 0-9
        new_bid->item_id = rand() % MAX_ITEMS;      // Random item
        new_bid->bid_amount = items[new_bid->item_id].current_price + 
                              (rand() % 100) + 1;   // Increment by 1-100
        
        // Create a thread for this bid
        if (pthread_create(&threads[num_bids], NULL, process_bid, new_bid) != 0) {
            printf("Error creating thread for bid %d\n", i);
            free(new_bid);
            break;
        }
        
        num_bids++;
        
        // Small delay between creating threads
        usleep(10000);  // 10ms delay
    }
    
    // Step 5: Wait for all bid threads to complete
    printf("\n--- Waiting for all bids to process ---\n");
    for (int i = 0; i < num_bids; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Step 6: Display final results
    print_auction_results();
    
    // Step 7: Cleanup
    cleanup_auction_items();
    destroy_semaphore();
    
    printf("\nAuction completed successfully!\n");
    return 0;
}