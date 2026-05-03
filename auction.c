#include "auction.h"
#include "semaphore.h"
#include <unistd.h>
#include <errno.h> // ETIMEOUT USED 

//global definitions

auction_item items[MAX_ITEMS];

bidder bidders[MAX_BIDDERS];
int num_bidders = 0;

bid_history auction_history[MAX_HISTORY];
int history_count = 0;

auction_stats stats = {0};

pthread_mutex_t log_mutex     = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t history_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t stats_mutex   = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t bidder_mutex  = PTHREAD_MUTEX_INITIALIZER;

// Protected by stats_mutex
static int current_active_bidders = 0;

//initialization fuction:

void initialize_auction_items(void) {
    printf("\n=== Initializing Auction Items ===\n");

    const char* item_names[] = {
        "Vintage Rolex Watch",
        "Original Picasso Painting",
        "Ancient Egyptian Vase"
    };

    for (int i = 0; i < MAX_ITEMS; i++) {
        items[i].item_id          = i;
        strcpy(items[i].item_name, item_names[i]);
        items[i].current_price    = 100 + (i * 100);
        items[i].highest_bidder_id = -1;
        items[i].is_active        = 1;
        items[i].end_time         = time(NULL) + 30;

        pthread_mutex_init(&items[i].mutex, NULL);
        pthread_cond_init(&items[i].cond, NULL);
        printf("\033[32mItem %d: %s - Starting price: $%d (Auction ends in 30 seconds)\n",
               i, items[i].item_name, items[i].current_price);
    }
    printf("Auction ready. %d items available.\033[0m\n\n", MAX_ITEMS);
}

void initialize_bidders(void) {
    printf("=== Initializing Bidders ===\n");
    num_bidders = MAX_BIDDERS;

    for (int i = 0; i < num_bidders; i++) {
        bidders[i].bidder_id            = i;
        sprintf(bidders[i].bidder_name, "Bidder_%d", i);
        bidders[i].budget               = 500 + (rand() % 800);
        bidders[i].auto_bid_increment   = 10  + (rand() % 30);
        bidders[i].is_active            = 1;
        bidders[i].current_item_bidding = -1;

        printf("Bidder %d: %s - Budget: $%d (Auto-bid increment: $%d)\n",
               i, bidders[i].bidder_name,
               bidders[i].budget, bidders[i].auto_bid_increment);
    }
    printf("\033[32mAll bidders ready.\033[0m\n\n");
}

//Auto-bidding function:

/*
 * check_and_place_auto_bid
 *
 * Called OUTSIDE any item mutex lock. Checks whether the outbid bidder
 * can afford to counter-bid and, if so, spawns a new thread to process
 * the auto-bid.
 *
 * FIX: previously called process_bid() directly, which caused two bugs:
 *   1. Deadlock — process_bid re-acquires items[item_id].mutex, which
 *      was still held by the calling thread via notify_outbid.
 *   2. Semaphore starvation — the calling thread already held a semaphore
 *      slot; the recursive call would block on wait_for_bidding_slot()
 *      and never release, starving all other threads when slots ran out.
 * Spawning a detached thread lets the auto-bid acquire its own slot
 * independently, with no recursive locking.
 */
int check_and_place_auto_bid(int bidder_id, int item_id, int current_price) {
    pthread_mutex_lock(&bidder_mutex);

    if (bidder_id < 0 || bidder_id >= num_bidders) {
        pthread_mutex_unlock(&bidder_mutex);
        return 0;
    }

    bidder* b = &bidders[bidder_id];
    int next_bid = current_price + b->auto_bid_increment;

    if (next_bid <= b->budget && b->is_active) {
        // Build the auto-bid action while we still hold bidder_mutex
        bid_action* auto_bid = (bid_action*)malloc(sizeof(bid_action));
        if (!auto_bid) {
            pthread_mutex_unlock(&bidder_mutex);
            return 0;
        }
        auto_bid->bidder_id  = bidder_id;
        auto_bid->item_id    = item_id;
        auto_bid->bid_amount = next_bid;
        auto_bid->is_auto_bid = 1;

        pthread_mutex_lock(&log_mutex);
        printf("\033[32m[AUTO-BID] %s automatically bidding $%d on Item %d\033[0m\n",
               b->bidder_name, next_bid, item_id);
        pthread_mutex_unlock(&log_mutex);

        pthread_mutex_unlock(&bidder_mutex);

        // Spawn a new thread so the auto-bid acquires its own semaphore
        // slot and does not recurse into the current call stack.
        pthread_t auto_thread;
        if (pthread_create(&auto_thread, NULL, process_bid, auto_bid) != 0) {
            free(auto_bid);
            return 0;
        }
        pthread_detach(auto_thread);
        return 1;

    } else if (next_bid > b->budget && b->is_active) {
        printf("\033[31m[OUT OF BUDGET] %s cannot auto-bid beyond $%d (need $%d)\033[0m\n",
               b->bidder_name, b->budget, next_bid);
        b->is_active = 0;
    }

    pthread_mutex_unlock(&bidder_mutex);
    return 0;
}

/*
 * notify_outbid
 *
 * Must be called OUTSIDE items[item_id].mutex (see process_bid for details).
 */
void notify_outbid(int bidder_id, int item_id, int new_price, int new_highest_bidder) {
    if (bidder_id < 0 || bidder_id >= num_bidders) return;

    char notification[256];
    sprintf(notification,
            "\033[31mYou were outbid on Item %d. New price: $%d (Highest bidder: %d)\033[0m",
            item_id, new_price, new_highest_bidder);
    send_private_notification(bidder_id, notification);

    // Trigger auto-bid for the outbid bidder (safe: called outside item mutex)
    check_and_place_auto_bid(bidder_id, item_id, new_price);
}

// ==================== NOTIFICATION FUNCTIONS ====================

void broadcast_notification(const char* message, int exclude_bidder) {
    (void)exclude_bidder;
    pthread_mutex_lock(&log_mutex);
    printf("\n\033[33m[BROADCAST] %s\033[0m\n", message);
    pthread_mutex_unlock(&log_mutex);
}

void send_private_notification(int bidder_id, const char* message) {
    if (bidder_id < 0 || bidder_id >= num_bidders) return;

    pthread_mutex_lock(&log_mutex);
    printf("\033[34m[PRIVATE] To %s: %s\033[0m\n", bidders[bidder_id].bidder_name, message);
    pthread_mutex_unlock(&log_mutex);
}

// ==================== TIMER THREAD (BID TIMEOUT) ====================

void* auction_timer_thread(void* arg) {
    int item_id = *(int*)arg;
    free(arg);

    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 30;

    pthread_mutex_lock(&items[item_id].mutex);

    int result = pthread_cond_timedwait(&items[item_id].cond,
                                        &items[item_id].mutex,
                                        &timeout);

    if (result == ETIMEDOUT && items[item_id].is_active) {
        items[item_id].is_active = 0;

        char notification[256];
        sprintf(notification,
                "\033[31mTIME'S UP! Item %d (%s) auction closed. Final price: $%d\033[0m",
                item_id, items[item_id].item_name, items[item_id].current_price);

        pthread_mutex_unlock(&items[item_id].mutex);

        broadcast_notification(notification, -1);

        pthread_mutex_lock(&log_mutex);
        printf("\033[31m[AUCTION ENDED] Item %d: %s sold for $%d to Bidder %d\033[0m\n",
               item_id, items[item_id].item_name,
               items[item_id].current_price, items[item_id].highest_bidder_id);
        pthread_mutex_unlock(&log_mutex);
    } else {
        pthread_mutex_unlock(&items[item_id].mutex);
    }

    return NULL;
}

// ==================== HISTORY FUNCTIONS ====================

void log_to_history(int bidder_id, int item_id, int bid_amount,
                    int accepted, const char* notification) {
    pthread_mutex_lock(&history_mutex);

    if (history_count < MAX_HISTORY) {
        auction_history[history_count].bid_id     = history_count;
        auction_history[history_count].bidder_id  = bidder_id;
        auction_history[history_count].item_id    = item_id;
        auction_history[history_count].bid_amount = bid_amount;
        auction_history[history_count].timestamp  = time(NULL);
        auction_history[history_count].was_accepted = accepted;
        if (notification) {
            strncpy(auction_history[history_count].notification_sent,
                    notification, 127);
            auction_history[history_count].notification_sent[127] = '\0';
        } else {
            strcpy(auction_history[history_count].notification_sent, "None");
        }
        history_count++;
    }

    pthread_mutex_unlock(&history_mutex);
}

void save_history_to_file(void) {
    FILE* file = fopen("auction_history.txt", "w");
    if (!file) {
        printf("[ERROR] Could not create auction_history.txt\n");
        return;
    }

    fprintf(file,
            "==========================================================================\n"
            "                         AUCTION HISTORY LOG\n"
            "==========================================================================\n\n");
    fprintf(file, "%-5s | %-8s | %-6s | %-8s | %-20s | %-10s | %s\n",
            "ID", "Bidder", "Item", "Amount", "Timestamp", "Status", "Notification");
    fprintf(file,
            "-----|----------|--------|----------|"
            "----------------------|------------|"
            "----------------------------------\n");

    for (int i = 0; i < history_count; i++) {
        char time_str[64];
        struct tm* tm_info = localtime(&auction_history[i].timestamp);
        strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);

        fprintf(file, "%-5d | %-8d | %-6d | $%-7d | %-20s | %-10s | %.50s\n",
                auction_history[i].bid_id,
                auction_history[i].bidder_id,
                auction_history[i].item_id,
                auction_history[i].bid_amount,
                time_str,
                auction_history[i].was_accepted ? "ACCEPTED" : "REJECTED",
                auction_history[i].notification_sent);
    }

    fclose(file);
    printf("\n[HISTORY] Saved %d bids to auction_history.txt\n", history_count);
}

void print_history(void) {
    printf("\n========================================\n");
    printf("        RECENT BID HISTORY (Last 10)\n");
    printf("========================================\n");

    int start = (history_count > 10) ? history_count - 10 : 0;
    for (int i = start; i < history_count; i++) {
        char time_str[64];
        struct tm* tm_info = localtime(&auction_history[i].timestamp);
        strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);

        printf("%s | Bidder %d -> Item %d: $%d [%s]\n",
               time_str,
               auction_history[i].bidder_id,
               auction_history[i].item_id,
               auction_history[i].bid_amount,
               auction_history[i].was_accepted ? "ACCEPTED" : "REJECTED");
    }
    printf("========================================\n");
}

// ==================== STATISTICS FUNCTIONS ====================

void update_peak_concurrent_bidders(int current_active) {
    pthread_mutex_lock(&stats_mutex);
    if (current_active > stats.active_bidders_count) {
        stats.active_bidders_count = current_active;
    }
    pthread_mutex_unlock(&stats_mutex);
}

void update_statistics(int item_id, int bid_amount, int accepted, int is_auto_bid) {
    pthread_mutex_lock(&stats_mutex);

    stats.total_bids_placed++;

    if (is_auto_bid) stats.auto_bids_placed++;

    if (accepted) {
        stats.total_accepted_bids++;
        stats.total_bid_sum += bid_amount;
        stats.bids_per_item[item_id]++;
        stats.revenue_per_item[item_id] = bid_amount;

        if (bid_amount > stats.highest_bid_ever)
            stats.highest_bid_ever = bid_amount;
        if (stats.lowest_bid_ever == 0 || bid_amount < stats.lowest_bid_ever)
            stats.lowest_bid_ever = bid_amount;
    } else {
        stats.total_rejected_bids++;
    }

    if (stats.total_accepted_bids > 0) {
        stats.average_bid = (float)stats.total_bid_sum / stats.total_accepted_bids;
    }

    pthread_mutex_unlock(&stats_mutex);
}

void print_statistics(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                      AUCTION STATISTICS                        ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║  Total Bids Placed:     %-30d ║\n", stats.total_bids_placed);
    printf("║  Accepted Bids:         %-30d ║\n", stats.total_accepted_bids);
    printf("║  Rejected Bids:         %-30d ║\n", stats.total_rejected_bids);
    printf("║  Auto-Bids Placed:      %-30d ║\n", stats.auto_bids_placed);
    printf("║  Highest Bid Ever:      $%-29d ║\n", stats.highest_bid_ever);
    printf("║  Lowest Bid Ever:       $%-29d ║\n", stats.lowest_bid_ever);
    printf("║  Average Bid Amount:    $%-29.2f ║\n", stats.average_bid);
    printf("║  Peak Concurrent Bidders: %-25d ║\n", stats.active_bidders_count);
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║  Per-Item Statistics:                                          ║\n");
    for (int i = 0; i < MAX_ITEMS; i++) {
        printf("║    Item %d: %-38s ║\n", i, items[i].item_name);
        printf("║      Bids Received:  %-30d ║\n", stats.bids_per_item[i]);
        printf("║      Final Price:    $%-29d ║\n", stats.revenue_per_item[i]);
    }
    printf("╚════════════════════════════════════════════════════════════════╝\n");
}

// ==================== CORE BID PROCESSING ====================

/*
 * process_bid  (mutex-protected, atomic bid update)
 *
 * DEADLOCK FIX:
 *   Previously, notify_outbid() was called INSIDE the items[item_id].mutex
 *   critical section.  notify_outbid -> check_and_place_auto_bid ->
 *   process_bid -> pthread_mutex_lock(items[item_id].mutex) = deadlock.
 *
 *   Fix: record which bidder was outbid inside the lock, release the mutex,
 *   THEN issue notifications and trigger auto-bids.
 */
void* process_bid(void* arg) {
    bid_action* bid = (bid_action*)arg;

    // Acquire semaphore slot (limits concurrent bids to MAX_CONCURRENT_BIDS)
    wait_for_bidding_slot();

    // FIX: protect current_active_bidders with stats_mutex to avoid data race
    pthread_mutex_lock(&stats_mutex);
    current_active_bidders++;
    int snapshot = current_active_bidders;
    pthread_mutex_unlock(&stats_mutex);
    update_peak_concurrent_bidders(snapshot);

    char notification[256];

    pthread_mutex_lock(&log_mutex);
    printf("\n[THREAD %lu] Processing: Bidder %d -> Item %d -> $%d %s\n",
           (unsigned long)pthread_self(),
           bid->bidder_id,
           bid->item_id,
           bid->bid_amount,
           bid->is_auto_bid ? "(AUTO-BID)" : "");
    pthread_mutex_unlock(&log_mutex);

    // Validate item index
    if (bid->item_id < 0 || bid->item_id >= MAX_ITEMS) {
        sprintf(notification, "ERROR: Invalid item ID %d", bid->item_id);
        log_to_history(bid->bidder_id, bid->item_id, bid->bid_amount, 0, notification);
        free(bid);
        pthread_mutex_lock(&stats_mutex);
        current_active_bidders--;
        pthread_mutex_unlock(&stats_mutex);
        release_bidding_slot();
        return NULL;
    }

    // Variables to carry outbid info out of the critical section
    int accepted          = 0;
    int old_price         = 0;
    int outbid_bidder_id  = -1;
    int winning_bidder_id = bid->bidder_id;
    int bid_item_id       = bid->item_id;
    int bid_amount        = bid->bid_amount;

    // ---- CRITICAL SECTION START ----
    pthread_mutex_lock(&items[bid->item_id].mutex);

    // Check for timeout
    if (time(NULL) > items[bid->item_id].end_time) {
        items[bid->item_id].is_active = 0;
        sprintf(notification, "\033[31mREJECTED: Auction for Item %d has ended\033[0m", bid->item_id);

        pthread_mutex_lock(&log_mutex);
        printf("\033[31m[TIMEOUT] Item %d auction has ended. Bid rejected.\033[0m\n", bid->item_id);
        pthread_mutex_unlock(&log_mutex);

        log_to_history(bid->bidder_id, bid->item_id, bid->bid_amount, 0, notification);
        update_statistics(bid->item_id, bid->bid_amount, 0, bid->is_auto_bid);

        pthread_mutex_unlock(&items[bid->item_id].mutex);
        free(bid);
        pthread_mutex_lock(&stats_mutex);
        current_active_bidders--;
        pthread_mutex_unlock(&stats_mutex);
        release_bidding_slot();
        return NULL;
    }

    if (!items[bid->item_id].is_active) {
        sprintf(notification, "\033[31mREJECTED: Item %d auction is closed\033[0m", bid->item_id);
        log_to_history(bid->bidder_id, bid->item_id, bid->bid_amount, 0, notification);
        update_statistics(bid->item_id, bid->bid_amount, 0, bid->is_auto_bid);

        pthread_mutex_unlock(&items[bid->item_id].mutex);
        free(bid);
        pthread_mutex_lock(&stats_mutex);
        current_active_bidders--;
        pthread_mutex_unlock(&stats_mutex);
        release_bidding_slot();
        return NULL;
    }

    if (bid->bid_amount > items[bid->item_id].current_price) {
        old_price            = items[bid->item_id].current_price;
        outbid_bidder_id     = items[bid->item_id].highest_bidder_id;

        // Atomic update
        items[bid->item_id].current_price      = bid->bid_amount;
        items[bid->item_id].highest_bidder_id  = bid->bidder_id;
        accepted = 1;

        sprintf(notification, "ACCEPTED: $%d bid on Item %d", bid->bid_amount, bid->item_id);
        log_to_history(bid->bidder_id, bid->item_id, bid->bid_amount, 1, notification);
        update_statistics(bid->item_id, bid->bid_amount, 1, bid->is_auto_bid);

        // Signal the timer thread (can be done while holding the mutex)
        pthread_cond_signal(&items[bid->item_id].cond);

        pthread_mutex_lock(&log_mutex);
        printf("\033[32m[ACCEPTED] Item %d (%s): $%d -> $%d (Bidder %d)\033[0m\n",
               bid->item_id, items[bid->item_id].item_name,
               old_price, bid->bid_amount, bid->bidder_id);
        pthread_mutex_unlock(&log_mutex);

    } else {
        sprintf(notification, "REJECTED: $%d not > current $%d",
                bid->bid_amount, items[bid->item_id].current_price);
        log_to_history(bid->bidder_id, bid->item_id, bid->bid_amount, 0, notification);
        update_statistics(bid->item_id, bid->bid_amount, 0, bid->is_auto_bid);

        pthread_mutex_lock(&log_mutex);
        printf("\033[31m[REJECTED] Item %d: $%d is not > current $%d (Bidder %d)\033[0m\n",
               bid->item_id, bid->bid_amount,
               items[bid->item_id].current_price, bid->bidder_id);
        pthread_mutex_unlock(&log_mutex);
    }

    // ---- CRITICAL SECTION END ----
    pthread_mutex_unlock(&items[bid->item_id].mutex);

    // Notifications are issued AFTER the mutex is released.
    // notify_outbid -> check_and_place_auto_bid -> (new thread) -> process_bid
    // now safely acquires items[item_id].mutex without deadlocking.
    if (accepted) {
        char broadcast_msg[256];
        sprintf(broadcast_msg,
                "NEW HIGH BID! Bidder %d bid $%d on '%s'! (was $%d)",
                winning_bidder_id, bid_amount,
                items[bid_item_id].item_name, old_price);
        broadcast_notification(broadcast_msg, winning_bidder_id);

        if (outbid_bidder_id != -1 && outbid_bidder_id != winning_bidder_id) {
            notify_outbid(outbid_bidder_id, bid_item_id, bid_amount, winning_bidder_id);
        }
    }

    free(bid);
    pthread_mutex_lock(&stats_mutex);
    current_active_bidders--;
    pthread_mutex_unlock(&stats_mutex);
    release_bidding_slot();
    return NULL;
}

// ==================== RESULT PRINTING & CLEANUP ====================

void print_auction_results(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                    FINAL AUCTION RESULTS                       ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");

    for (int i = 0; i < MAX_ITEMS; i++) {
        printf("║  Item %d: %-40s ║\n", i, items[i].item_name);
        printf("║    Final Price:     $%-30d ║\n", items[i].current_price);

        if (items[i].highest_bidder_id != -1 &&
            items[i].highest_bidder_id < num_bidders) {
            printf("║    Winning Bidder:  %s (ID: %d)%-16s ║\n",
                   bidders[items[i].highest_bidder_id].bidder_name,
                   items[i].highest_bidder_id, "");
            printf("║    Status:          SOLD                                ║\n");
        } else {
            printf("║    Winning Bidder:  None                                ║\n");
            printf("║    Status:          UNSOLD                              ║\n");
        }
        if (i < MAX_ITEMS - 1) {
            printf("║                                                            ║\n");
        }
    }
    printf("╚════════════════════════════════════════════════════════════════╝\n");
}

void cleanup_auction_items(void) {
    for (int i = 0; i < MAX_ITEMS; i++) {
        pthread_mutex_destroy(&items[i].mutex);
        pthread_cond_destroy(&items[i].cond);
    }
    pthread_mutex_destroy(&log_mutex);
    pthread_mutex_destroy(&history_mutex);
    pthread_mutex_destroy(&stats_mutex);
    pthread_mutex_destroy(&bidder_mutex);

    printf("\nCleanup complete. All mutexes and condition variables destroyed.\n");
}