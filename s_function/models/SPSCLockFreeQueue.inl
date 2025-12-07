// SPSCLockFreeQueue.inl - 模板实现文件

template<typename T, size_t Size>
SPSCLockFreeQueue<T, Size>::SPSCLockFreeQueue() {
    // 初始化所有槽位为未就绪状态
    for (size_t i = 0; i < Size; ++i) {
        buffer[i].ready.store(false, std::memory_order_relaxed);
    }
}

template<typename T, size_t Size>
bool SPSCLockFreeQueue<T, Size>::push(const T& item) {
    size_t current_write = write_pos.load(std::memory_order_relaxed);
    size_t index = current_write & MASK;
    Slot& slot = buffer[index];
    
    // 检查槽位是否可用（消费者是否已读取）
    // 使用 acquire 确保看到消费者对 ready 的修改
    if (slot.ready.load(std::memory_order_acquire)) {
        // 队列满，无法写入
        return false;
    }
    
    // 写入数据（在标记 ready 之前完成）
    slot.data = item;
    
    // 标记为就绪（使用 release 确保数据写入完成后再标记）
    // 这确保了消费者看到 ready=true 时，数据已经完整写入
    slot.ready.store(true, std::memory_order_release);
    
    // 更新写位置（使用 relaxed，因为只有生产者线程修改）
    write_pos.store(current_write + 1, std::memory_order_relaxed);
    
    return true;
}

template<typename T, size_t Size>
bool SPSCLockFreeQueue<T, Size>::push(T&& item) {
    size_t current_write = write_pos.load(std::memory_order_relaxed);
    size_t index = current_write & MASK;
    Slot& slot = buffer[index];
    
    if (slot.ready.load(std::memory_order_acquire)) {
        return false;
    }
    
    slot.data = std::move(item);
    slot.ready.store(true, std::memory_order_release);
    write_pos.store(current_write + 1, std::memory_order_relaxed);
    
    return true;
}

template<typename T, size_t Size>
bool SPSCLockFreeQueue<T, Size>::pop(T& item) {
    size_t current_read = read_pos.load(std::memory_order_relaxed);
    size_t index = current_read & MASK;
    Slot& slot = buffer[index];
    
    // 检查数据是否就绪
    // 使用 acquire 确保看到生产者对 ready 的修改
    if (!slot.ready.load(std::memory_order_acquire)) {
        // 队列为空
        return false;
    }
    
    // 读取数据（使用移动语义，避免不必要的拷贝）
    item = std::move(slot.data);
    
    // 标记为未就绪（使用 release 确保数据读取完成后再标记）
    // 这确保了生产者看到 ready=false 时，数据已经被读取
    slot.ready.store(false, std::memory_order_release);
    
    // 更新读位置（使用 relaxed，因为只有消费者线程修改）
    read_pos.store(current_read + 1, std::memory_order_relaxed);
    
    return true;
}

template<typename T, size_t Size>
bool SPSCLockFreeQueue<T, Size>::tryPop(T& item) {
    return pop(item);
}

template<typename T, size_t Size>
bool SPSCLockFreeQueue<T, Size>::front(T& item) const {
    // 获取当前读位置（但不修改，因为这是 const 函数）
    size_t current_read = read_pos.load(std::memory_order_acquire);
    size_t index = current_read & MASK;
    const Slot& slot = buffer[index];
    
    // 检查数据是否就绪
    // 使用 acquire 确保看到生产者对 ready 的修改
    if (!slot.ready.load(std::memory_order_acquire)) {
        // 队列为空
        return false;
    }
    
    // 读取数据（复制，不移动，因为不修改队列状态）
    // 注意：这里使用复制而不是移动，因为 front 不应该修改原始数据
    item = slot.data;
    
    return true;
}

template<typename T, size_t Size>
bool SPSCLockFreeQueue<T, Size>::empty() const {
    size_t current_read = read_pos.load(std::memory_order_acquire);
    size_t index = current_read & MASK;
    const Slot& slot = buffer[index];
    return !slot.ready.load(std::memory_order_acquire);
}

template<typename T, size_t Size>
bool SPSCLockFreeQueue<T, Size>::full() const {
    size_t current_write = write_pos.load(std::memory_order_relaxed);
    size_t index = current_write & MASK;
    const Slot& slot = buffer[index];
    return slot.ready.load(std::memory_order_acquire);
}

template<typename T, size_t Size>
size_t SPSCLockFreeQueue<T, Size>::size() const {
    size_t w = write_pos.load(std::memory_order_acquire);
    size_t r = read_pos.load(std::memory_order_acquire);
    
    if (w >= r) {
        return w - r;
    }
    // 不应该发生，但如果发生了，返回 0
    return 0;
}

template<typename T, size_t Size>
constexpr size_t SPSCLockFreeQueue<T, Size>::capacity() const {
    return Size;
}

template<typename T, size_t Size>
void SPSCLockFreeQueue<T, Size>::clear() {
    T dummy;
    while (pop(dummy)) {
        // 清空所有数据
    }
}

template<typename T, size_t Size>
void SPSCLockFreeQueue<T, Size>::reset() {
    // 清空所有数据
    clear();
    
    // 重置位置
    write_pos.store(0, std::memory_order_relaxed);
    read_pos.store(0, std::memory_order_relaxed);
    
    // 重置所有槽位
    for (size_t i = 0; i < Size; ++i) {
        buffer[i].ready.store(false, std::memory_order_relaxed);
    }
}

