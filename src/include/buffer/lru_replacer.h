#ifndef MINISQL_LRU_REPLACER_H
#define MINISQL_LRU_REPLACER_H

#include <list>
#include <mutex>
#include <unordered_set>
#include <vector>
#include <unordered_map>

#include "buffer/replacer.h"
#include "common/config.h"
#include "common/macros.h"

using namespace std;

/**
 * LRUReplacer implements the Least Recently Used replacement policy.
 */
class LRUReplacer : public Replacer {
public:
    /**
     * Create a new LRUReplacer.
     * @param num_pages the maximum number of pages the LRUReplacer will be required to store
     */
    explicit LRUReplacer(size_t num_pages);

    /**
     * Destroys the LRUReplacer.
     */
    ~LRUReplacer() override;

    bool Victim(frame_id_t *frame_id) override;

    void Pin(frame_id_t frame_id) override;

    void Unpin(frame_id_t frame_id) override;

    size_t Size() override;

private:
    // add your own private member variables here
    size_t capacity;
    std::list<frame_id_t> lru_list_; // 双向链表
    std::unordered_map<frame_id_t, std::list<frame_id_t>::iterator> buf; // 快速定位frame_id的位置
};

#endif  // MINISQL_LRU_REPLACER_H
