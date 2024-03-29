//
// Created by wwww on 2023/8/23.
//

#ifndef CPP_SIMPLE_WEB_SERVER_THREAD_SAFE_QUEUE_HPP
#define CPP_SIMPLE_WEB_SERVER_THREAD_SAFE_QUEUE_HPP
#include <condition_variable>
#include <mutex>
#include <queue>
namespace my::thread {
template <typename T> class ThreadSafeQueue {
private:
  struct node {
    std::unique_ptr<T> data;
    std::unique_ptr<node> next;
  };
  std::mutex head_mutex;
  std::unique_ptr<node> head;
  std::mutex tail_mutex;
  node *tail;
  std::condition_variable data_cond;

  node *get_tail() {
    std::lock_guard<std::mutex> tail_lock(tail_mutex);
    return tail;
  }
  std::unique_ptr<node> pop_head() {
    std::unique_ptr<node> old_head = std::move(head);
    head = std::move(old_head->next);
    return old_head;
  }
  std::unique_lock<std::mutex> wait_for_data() {
    std::unique_lock<std::mutex> head_lock(head_mutex);
    data_cond.wait(head_lock, [&] { return head.get() != get_tail(); });
    return std::move(head_lock);
  }
  std::unique_ptr<node> wait_pop_head() {
    std::unique_lock<std::mutex> head_lock(wait_for_data());
    return pop_head();
  }
  std::unique_ptr<node> wait_pop_head(T &value) {
    std::unique_lock<std::mutex> head_lock(wait_for_data());
    value = std::move(*head->data);
    return pop_head();
  }
  std::unique_ptr<node> try_pop_head() {
    std::lock_guard<std::mutex> head_lock(head_mutex);
    if (head.get() == get_tail()) {
      return std::unique_ptr<node>();
    }
    return pop_head();
  }
  std::unique_ptr<node> try_pop_head(T &value) {
    std::lock_guard<std::mutex> head_lock(head_mutex);
    if (head.get() == get_tail()) {
      return std::unique_ptr<node>();
    }
    value = std::move(*head->data);
    return pop_head();
  }

public:
  ThreadSafeQueue() : head(new node), tail(head.get()) {}
  ThreadSafeQueue(const ThreadSafeQueue &other) = delete;
  ThreadSafeQueue &operator=(const ThreadSafeQueue &other) = delete;
  std::unique_ptr<T> try_pop() {
    std::unique_ptr<node> old_head = try_pop_head();
    return old_head ? old_head->data : std::unique_ptr<T>();
  }
  bool try_pop(T &value) {
    std::unique_ptr<node> const old_head = try_pop_head(value);
    return old_head != nullptr;
  }
  std::unique_ptr<T> wait_and_pop() {
    std::unique_ptr<node>  old_head = wait_pop_head();
    return std::move(old_head->data);
  };
  void wait_and_pop(T &value) {
    std::unique_ptr<node> const old_head = wait_pop_head(value);
  }
  void push(T new_value) {
    auto new_data = std::make_unique<T>(std::move(new_value));
    std::unique_ptr<node> p(new node);
    {
      std::lock_guard<std::mutex> tail_lock(tail_mutex);
      tail->data = std::move(new_data);
      node *const new_tail = p.get();
      tail->next = std::move(p);
      tail = new_tail;
    }
    data_cond.notify_one();
  }
  bool empty() {
    std::lock_guard<std::mutex> head_lock(head_mutex);
    return (head.get() == get_tail());
  }
};

} // namespace my::thread
#endif // CPP_SIMPLE_WEB_SERVER_THREAD_SAFE_QUEUE_HPP
