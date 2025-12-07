#ifndef SPSLOCKFREEQUEUE_H
#define SPSLOCKFREEQUEUE_H

#include <atomic>
#include <cstddef>

/**
 * @brief 单生产者单消费者无锁队列
 * 
 * 特点：
 * - 只有一个生产者线程写入
 * - 只有一个消费者线程读取
 * - 完全无锁，高性能
 * - 固定大小环形缓冲区
 * 
 * 使用场景：
 * - 音视频数据传递
 * - 日志队列
 * - 任务队列
 * 
 * 注意事项：
 * - 队列大小必须是2的幂（2, 4, 8, 16, 32, 64, 128, 256, 512, 1024...）
 * - 只能有一个生产者线程调用 push()
 * - 只能有一个消费者线程调用 pop()
 * - 队列满时 push() 返回 false，需要处理
 * - 队列空时 pop() 返回 false，需要处理
 */
template<typename T, size_t Size>
class SPSCLockFreeQueue {
    static_assert((Size & (Size - 1)) == 0, "Size must be power of 2");
    static_assert(Size >= 2, "Size must be at least 2");

private:
    // 缓存行对齐，避免伪共享
    struct alignas(64) Slot {
        std::atomic<bool> ready{false};
        T data;
        
        Slot() : ready(false) {}
    };

    Slot buffer[Size];
    
    // 写位置（生产者线程使用）
    alignas(64) std::atomic<size_t> write_pos{0};
    
    // 读位置（消费者线程使用）
    alignas(64) std::atomic<size_t> read_pos{0};
    
    // 用于快速计算索引的掩码（Size必须是2的幂）
    static constexpr size_t MASK = Size - 1;

public:
    /**
     * @brief 构造函数
     */
    SPSCLockFreeQueue();

    /**
     * @brief 入队（仅由生产者线程调用）
     * @param item 要入队的数据
     * @return true 成功，false 队列已满
     */
    bool push(const T& item);

    /**
     * @brief 移动语义版本的 push（更高效）
     * @param item 要入队的数据（将被移动）
     * @return true 成功，false 队列已满
     */
    bool push(T&& item);

    /**
     * @brief 出队（仅由消费者线程调用）
     * @param item 出队的数据（使用移动语义，更高效）
     * @return true 成功，false 队列为空
     */
    bool pop(T& item);

    /**
     * @brief 尝试出队，如果队列为空则返回 false（非阻塞）
     * @param item 出队的数据
     * @return true 成功，false 队列为空
     */
    bool tryPop(T& item);

    /**
     * @brief 获取队头元素（仅由消费者线程调用，不弹出）
     * @param item 队头数据的副本（仅在队列不为空时赋值）
     * @return true 成功（队列不为空），false 队列为空（不会修改 item）
     * @note 此函数只读取数据，不修改队列状态，可以安全地多次调用
     * @note 如果队列为空，item 参数不会被修改，函数直接返回 false
     */
    bool front(T& item) const;

    /**
     * @brief 检查队列是否为空（仅由消费者线程调用）
     * @return true 队列为空，false 队列有数据
     */
    bool empty() const;

    /**
     * @brief 检查队列是否已满（仅由生产者线程调用）
     * @return true 队列已满，false 队列未满
     */
    bool full() const;

    /**
     * @brief 获取队列中元素数量（近似值）
     * @note 由于生产者和消费者并发，这个值只是近似值
     * @return 队列中元素数量
     */
    size_t size() const;

    /**
     * @brief 获取队列容量
     * @return 队列容量
     */
    constexpr size_t capacity() const;

    /**
     * @brief 清空队列（仅由消费者线程调用）
     */
    void clear();

    /**
     * @brief 重置队列（清空并重置所有状态）
     * @note 调用此函数时，生产者和消费者都应该停止操作
     */
    void reset();
};

// 包含实现文件
#include "SPSCLockFreeQueue.inl"

#endif // SPSLOCKFREEQUEUE_H

