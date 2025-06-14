#include "buffer/buffer_pool_manager.h"

#include "glog/logging.h"
#include "page/bitmap_page.h"

static const char EMPTY_PAGE_DATA[PAGE_SIZE] = {0};

BufferPoolManager::BufferPoolManager(size_t pool_size, DiskManager *disk_manager)
        : pool_size_(pool_size), disk_manager_(disk_manager) {
    pages_ = new Page[pool_size_];
    replacer_ = new LRUReplacer(pool_size_);
    for (size_t i = 0; i < pool_size_; i++) {
        free_list_.emplace_back(i);
    }
}

BufferPoolManager::~BufferPoolManager() {
    for (auto page : page_table_) {
        FlushPage(page.first);
    }
    delete[] pages_;
    delete replacer_;
}

/**
 * TODO: Student Implement
 */
Page *BufferPoolManager::FetchPage(page_id_t page_id) {
    if (page_id >= MAX_VALID_PAGE_ID || page_id <= INVALID_PAGE_ID) {
        LOG(ERROR) << "Invalid page id: " << page_id;
        return nullptr;
    }

    // 这个时候page_id已经在buffer pool中了，不用从磁盘读取
    if (page_table_.count(page_id) != 0) {
        frame_id_t frame_id = page_table_[page_id];
        Page &page = pages_[frame_id];
        page.pin_count_++;
        replacer_->Pin(frame_id);
        return &page;
    }

    // 开始分配
    frame_id_t frame_id = FindFreePage();
    if (frame_id == INVALID_FRAME_ID) return nullptr;

    Page &page = pages_[frame_id];
    // 如果page是脏的，需要写回磁盘
    if (page.IsDirty()) {
        disk_manager_->WritePage(page.page_id_, page.data_);
    }

    // 替换旧的page，并更新page_table_
    page_table_.erase(page.page_id_);
    page_table_[page_id] = frame_id;
    disk_manager_->ReadPage(page_id, page.data_);
    page.page_id_ = page_id;
    page.pin_count_ = 1;
    page.is_dirty_ = false;

    return &page;
}

/**
 * TODO: Student Implement
 */
Page *BufferPoolManager::NewPage(page_id_t &page_id) {
    frame_id_t frame_id = FindFreePage();
    if (frame_id == INVALID_PAGE_ID) return nullptr;

    // 如果page是脏的，需要写回磁盘
    Page &page = pages_[frame_id];
    if (page.IsDirty()) {
        disk_manager_->WritePage(page.page_id_, page.data_);
    }

    page_id = AllocatePage();

    // 替换旧的page，并更新page_table_
    page_table_.erase(page.page_id_);
    page_table_[page_id] = frame_id;
    page.ResetMemory();
    page.page_id_ = page_id;
    page.pin_count_ = 1;
    page.is_dirty_ = false;

    return &page;
}

/**
 * TODO: Student Implement
 */
bool BufferPoolManager::DeletePage(page_id_t page_id) {
    auto it = page_table_.find(page_id);

    // 不再buffer pool中
    if (it == page_table_.end()) return true;

    frame_id_t frame_id = it->second;
    Page &page = pages_[frame_id];
    if (page.pin_count_ > 0) return false;  // 有锁，没办法被删

    DeallocatePage(page_id);
    page_table_.erase(it);
    replacer_->Pin(frame_id); // 需要从LRU中删除，这页已经不可被替换
    page.ResetMemory();
    page.page_id_ = INVALID_PAGE_ID;
    page.pin_count_ = 0;
    page.is_dirty_ = false;
    free_list_.emplace_back(frame_id);

    return true;
}

/**
 * TODO: Student Implement
 */
bool BufferPoolManager::UnpinPage(page_id_t page_id, bool is_dirty) {
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) return false;

    frame_id_t frame_id = it->second;
    Page &page =pages_[frame_id];

    if (page.pin_count_ == 0) {
        LOG(WARNING) << "Page " << page_id << " is already unpinned";
        return false; // ？不需要unpin是应该返回失败了呢，还是成功呢
    }

    if (is_dirty) page.is_dirty_ = true;
    page.pin_count_--;

    // 如果pin_count_ == 0，说明可以在LRU中被替换了
    if (page.pin_count_ == 0) {
        replacer_->Unpin(frame_id);
    }

    return true;
}

/**
 * TODO: Student Implement
 */
bool BufferPoolManager::FlushPage(page_id_t page_id) {
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) return false;

    frame_id_t frame_id = it->second;
    Page &page = pages_[frame_id];

    // 写入磁盘
    disk_manager_->WritePage(page.page_id_, page.data_);
    page.is_dirty_ = false;
    return true;
}

// 利用LRU尝试寻找空闲页
frame_id_t BufferPoolManager::FindFreePage() {
    if (!free_list_.empty()) {
        frame_id_t frame_id = free_list_.front();
        free_list_.pop_front();
        return frame_id;
    }
    frame_id_t victim;
    if (replacer_->Victim(&victim)) return victim;
    return INVALID_FRAME_ID;
}

page_id_t BufferPoolManager::AllocatePage() {
    int next_page_id = disk_manager_->AllocatePage();
    return next_page_id;
}

void BufferPoolManager::DeallocatePage(__attribute__((unused)) page_id_t page_id) {
    disk_manager_->DeAllocatePage(page_id);
}

bool BufferPoolManager::IsPageFree(page_id_t page_id) {
    return disk_manager_->IsPageFree(page_id);
}

// Only used for debug
bool BufferPoolManager::CheckAllUnpinned() {
    bool res = true;
    for (size_t i = 0; i < pool_size_; i++) {
        if (pages_[i].pin_count_ != 0) {
            res = false;
            LOG(ERROR) << "page " << pages_[i].page_id_ << " pin count:" << pages_[i].pin_count_ << endl;
        }
    }
    return res;
}