#include "auction.h"
#include "semaphore.h"
#include <time.h>
#include <unistd.h>

int main() {
    pthread_t threads[MAX_BIDS];
    pthread_t timer_threads[MAX_ITEMS];
    int num_bids = 0;

    srand((unsigned int)time(NULL));

    // Initialize semaphore
    initialize_semaphore();

    // Initialize auction items and bidders
    initialize_auction_items();
    initialize_bidders();   // FIX: was never called; num_bidders stayed 0

    // Start timer threads for each item (bid timeout feature)
    for (int i = 0; i < MAX_ITEMS; i++) {
        int* item_id = malloc(sizeof(int));
        *item_id = i;
        pthread_create(&timer_threads[i], NULL, auction_timer_thread, item_id);
    }

    // Create bid threads
    for (int i = 0; i < MAX_BIDS; i++) {
        bid_action* new_bid = malloc(sizeof(bid_action));
        new_bid->bidder_id  = rand() % MAX_BIDDERS;
        new_bid->item_id    = rand() % MAX_ITEMS;
        new_bid->bid_amount = items[new_bid->item_id].current_price + (rand() % 100) + 1;
        // FIX: removed new_bid->max_budget and new_bid->auto_bid_increment —
        //      those fields do not exist in bid_action; auto-bid parameters
        //      are tracked per-bidder in the bidders[] array instead.
        new_bid->is_auto_bid = 0;

        pthread_create(&threads[num_bids], NULL, process_bid, new_bid);
        num_bids++;
        usleep(10000);
    }

    // Wait for all bid threads
    for (int i = 0; i < num_bids; i++) {
        pthread_join(threads[i], NULL);
    }

    // Wait for timer threads
    for (int i = 0; i < MAX_ITEMS; i++) {
        pthread_join(timer_threads[i], NULL);
    }

    // Print results and save history
    print_auction_results();
    print_statistics();
    save_history_to_file();

    // Cleanup
    cleanup_auction_items();
    destroy_semaphore();

    return 0;
}