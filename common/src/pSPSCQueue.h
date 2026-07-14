#pragma once
#include <memory>
#include "SPSCQueue.h"

template <typename T> class SPSCQueueHaveNextFront: public rigtorp::SPSCQueue<T>
{
public:
    /* 构造函数：调用父类构造，指定队列容量  */
    explicit SPSCQueueHaveNextFront(const size_t capacity)
        : rigtorp::SPSCQueue<T>(capacity)
    {}
    T* nextWrite() noexcept
    {
        auto const writeIdx = rigtorp::SPSCQueue<T>::writeIdx_.load(std::memory_order_relaxed);
        auto nextWriteIdx = writeIdx + 1;
        if (nextWriteIdx == rigtorp::SPSCQueue<T>::capacity_) {
            nextWriteIdx = 0;
        }
        if (nextWriteIdx == rigtorp::SPSCQueue<T>::readIdxCache_) {
            rigtorp::SPSCQueue<T>::readIdxCache_ = rigtorp::SPSCQueue<T>::readIdx_.load(std::memory_order_acquire);
            if (nextWriteIdx == rigtorp::SPSCQueue<T>::readIdxCache_) {
                return nullptr;
            }
        }
        return &rigtorp::SPSCQueue<T>::slots_[writeIdx + rigtorp::SPSCQueue<T>::kPadding];
    }
    void cPop() noexcept
    {
        static_assert(std::is_nothrow_destructible<T>::value,
                  "T must be nothrow destructible");
        auto const readIdx = rigtorp::SPSCQueue<T>::readIdx_.load(std::memory_order_relaxed);
        assert(rigtorp::SPSCQueue<T>::writeIdx_.load(std::memory_order_acquire) != readIdx);
        auto nextReadIdx = readIdx + 1;
        if (nextReadIdx == rigtorp::SPSCQueue<T>::capacity_) {
            nextReadIdx = 0;
        }
        rigtorp::SPSCQueue<T>::readIdx_.store(nextReadIdx, std::memory_order_release);
    }
    void push ()
    {
        auto const writeIdx = rigtorp::SPSCQueue<T>::writeIdx_.load(std::memory_order_relaxed);
        auto nextWriteIdx = writeIdx + 1;
        if (nextWriteIdx == rigtorp::SPSCQueue<T>::capacity_) {
            nextWriteIdx = 0;
        }
        while (nextWriteIdx == rigtorp::SPSCQueue<T>::readIdxCache_) {
            rigtorp::SPSCQueue<T>::readIdxCache_ = rigtorp::SPSCQueue<T>::readIdx_.load(std::memory_order_acquire);
        }
        rigtorp::SPSCQueue<T>::writeIdx_.store(nextWriteIdx, std::memory_order_release);
    }
    T *nextFront() noexcept 
    {
        auto const readIdx = rigtorp::SPSCQueue<T>::readIdx_.load(std::memory_order_relaxed);
        if (readIdx == rigtorp::SPSCQueue<T>::writeIdxCache_) {
            rigtorp::SPSCQueue<T>::writeIdxCache_ = rigtorp::SPSCQueue<T>::writeIdx_.load(std::memory_order_acquire);
            if (rigtorp::SPSCQueue<T>::writeIdxCache_ == readIdx) {
                return nullptr;
            }
        }
        auto nextReadIdx = readIdx + 1;
        if (nextReadIdx == rigtorp::SPSCQueue<T>::capacity_) {
            nextReadIdx = 0;
        }
        if (rigtorp::SPSCQueue<T>::writeIdxCache_ == nextReadIdx) {
            rigtorp::SPSCQueue<T>::writeIdxCache_ = rigtorp::SPSCQueue<T>::writeIdx_.load(std::memory_order_acquire);
            if (rigtorp::SPSCQueue<T>::writeIdxCache_ == nextReadIdx) {
                return nullptr;
            }
        }
        return &rigtorp::SPSCQueue<T>::slots_[nextReadIdx + rigtorp::SPSCQueue<T>::kPadding];
    }
    T *nextNextFront() noexcept 
    {
        auto const readIdx = rigtorp::SPSCQueue<T>::readIdx_.load(std::memory_order_relaxed);
        if (readIdx == rigtorp::SPSCQueue<T>::writeIdxCache_) {
            rigtorp::SPSCQueue<T>::writeIdxCache_ = rigtorp::SPSCQueue<T>::writeIdx_.load(std::memory_order_acquire);
            if (rigtorp::SPSCQueue<T>::writeIdxCache_ == readIdx) {
                return nullptr;
            }
        }
        auto nextReadIdx = readIdx + 1;
        if (nextReadIdx == rigtorp::SPSCQueue<T>::capacity_) {
            nextReadIdx = 0;
        }
        if (rigtorp::SPSCQueue<T>::writeIdxCache_ == nextReadIdx) {
            rigtorp::SPSCQueue<T>::writeIdxCache_ = rigtorp::SPSCQueue<T>::writeIdx_.load(std::memory_order_acquire);
            if (rigtorp::SPSCQueue<T>::writeIdxCache_ == nextReadIdx) {
                return nullptr;
            }
        }
        auto nextNextReadIdx = nextReadIdx + 1;
        if (nextNextReadIdx == rigtorp::SPSCQueue<T>::capacity_) {
            nextNextReadIdx = 0;
        }
        if (rigtorp::SPSCQueue<T>::writeIdxCache_ == nextNextReadIdx ) {
            rigtorp::SPSCQueue<T>::writeIdxCache_ = rigtorp::SPSCQueue<T>::writeIdx_.load(std::memory_order_acquire);
            if (rigtorp::SPSCQueue<T>::writeIdxCache_ == nextNextReadIdx) {
                return nullptr;
            }
        }
        return &rigtorp::SPSCQueue<T>::slots_[nextNextReadIdx + rigtorp::SPSCQueue<T>::kPadding];
    }
};

template <typename T> class SPSCQueue2: public SPSCQueueHaveNextFront<T> // rigtorp::SPSCQueue<T>
{
public:
    /* 构造函数：调用父类构造，指定队列容量  */
    explicit SPSCQueue2(size_t capacity)
        :SPSCQueueHaveNextFront<T>(capacity)
    {}
    /* 可能队列满了，非准确性判断 */
    bool mabeFull()
    {
        return rigtorp::SPSCQueue<T>::size() >= rigtorp::SPSCQueue<T>::capacity();
    }
    /* 可能需要生产了(已消耗过半)，非准确性判断 */
    bool mabeNeetPush()
    {
        return rigtorp::SPSCQueue<T>::size() <= rigtorp::SPSCQueue<T>::capacity()/2;
    }
};

template <typename T> class pSPSCQueue: public SPSCQueueHaveNextFront<std::unique_ptr<T>> // rigtorp::SPSCQueue<std::unique_ptr<T>>
{
public:
    /* 构造函数：调用父类构造，指定队列容量  */
    explicit pSPSCQueue(size_t capacity)
        :SPSCQueueHaveNextFront<std::unique_ptr<T>>(capacity)
    {}

    /* // ========== 对外暴露的核心接口（无需手动处理unique_ptr） ==========  */
    /**
     * 入队：接收videoFrameData右值，内部自动封装为unique_ptr
     * @param frame 待入队的videoFrameData（右值，会被移动）
     * @return 是否入队成功
     */
    bool try_push(T&& frame)
    {
        // 内部自动构造unique_ptr，调用父类的try_push（移动语义）
        return rigtorp::SPSCQueue<std::unique_ptr<T>>::try_push(
            std::make_unique<T>(std::move(frame))
        );
    }

    /**
     * 原地构造入队：直接传入参数，内部构造videoFrameData并封装为unique_ptr
     * 避免额外的移动操作，效率更高
     */
    template <typename... Args>
    bool try_emplace(Args&&... args)
    {
        return rigtorp::SPSCQueue<std::unique_ptr<T>>::try_emplace(
            std::make_unique<T>(std::forward<Args>(args)...)
        );
    }

    /**
     * 获取队首元素：返回裸指针（注意：弹出后指针悬空）
     * @return 队首videoFrameData*，空队列返回nullptr
     */
    T* front()
    {
        // 父类front返回std::unique_ptr<videoFrameData>*
        auto* ptr = rigtorp::SPSCQueue<std::unique_ptr<T>>::front();
        return ptr ? ptr->get() : nullptr;
    }
    T *nextFront() noexcept 
    {
        auto* ptr = SPSCQueueHaveNextFront<std::unique_ptr<T>>::nextFront();
        return ptr ? ptr->get() : nullptr;
    }
    /**
     * 弹出队首元素（释放unique_ptr，自动释放videoFrameData资源）
     */
    void pop()
    {
        rigtorp::SPSCQueue<std::unique_ptr<T>>::pop();
    }
    /* 可能队列满了，非准确性判断 */
    bool mabeFull()
    {
        return rigtorp::SPSCQueue<std::unique_ptr<T>>::size() >= rigtorp::SPSCQueue<std::unique_ptr<T>>::capacity();
    }
    /* 可能需要生产了(已消耗过半)，非准确性判断 */
    bool mabeNeetPush()
    {
        return rigtorp::SPSCQueue<std::unique_ptr<T>>::size() <= rigtorp::SPSCQueue<std::unique_ptr<T>>::capacity()/2;
    }
private:
    // 禁用父类的try_push（避免外部传入unique_ptr，保证接口统一）
    using rigtorp::SPSCQueue<std::unique_ptr<T>>::try_push;
    using SPSCQueueHaveNextFront<std::unique_ptr<T>>::nextFront;
};
