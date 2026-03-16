// sigslot.h: 信号/槽 类实现
// sigslot.h: Signal/Slot classes
//
// 原始作者: Sarah Thompson (sarah@telergy.com) 2002。
//
// 许可: Public domain。你可以自由使用此代码，但作者不对因使用本代码
// 导致的任何责任或损失承担责任。
//
// 快速说明
//
// （完整文档见: http://sigslot.sourceforge.net/）
//
// 可配置的宏说明：
//   SIGSLOT_PURE_ISO:
//     定义此宏以强制使用 ISO C++（纯 ISO 模式）。这会在支持线程的
//     平台上禁用所有线程安全相关代码。
//
//   SIGSLOT_USE_POSIX_THREADS:
//     在使用非 gcc 的 C++ 编译器，但运行平台支持 Posix 线程时，强制
//     使用 Posix 线程。（当使用 gcc 时，通常默认启用 Posix 线程；若要
//     禁用可定义 SIGSLOT_PURE_ISO）
//
//   SIGSLOT_DEFAULT_MT_POLICY:
//     当启用线程支持时，默认值为 multi_threaded_global；否则默认为
//     single_threaded。可通过 #define 覆盖该默认值。在纯 ISO 模式下，除
//     single_threaded 之外的选项会导致编译错误。
//
// 平台说明：
//   Win32:
//     在 Win32 平台上，需定义 WEBRTC_WIN 符号以启用 Win32 线程支持。多数
//     主流编译器会默认定义该符号；但在不常见或自定义的构建环境中，可能
//     需要手动定义该宏以启用 Win32 线程支持。
//
//   Unix/Linux/BSD 等：
//     当使用 gcc 时，默认假定平台提供 Posix 线程并会启用它们。你可以用
//     SIGSLOT_PURE_ISO 来禁用线程支持；若使用其他编译器但希望启用 Posix
//     线程，则需定义 SIGSLOT_USE_POSIX_THREADS。
//
//   ISO C++:
//     如果未检测到受支持的平台，或定义了 SIGSLOT_PURE_ISO，则会关闭所有
//     多线程支持，并移除可能导致纯 ISO C++ 环境报错的代码。注意：在某些
//     编译器选项下（例如 gcc 的 -ansi -pedantic），可能会出现编译问题；通常
//     使用 gcc -ansi 可以通过编译；如需深入探讨，请联系原作者。
//
// 线程模式说明：
//   single_threaded:
//     假定程序在信号/槽使用方面为单线程（即所有使用 signal/slot 的对象
//     在同一线程内创建和销毁）。若对象在多线程下并发销毁，则行为未定
//     义（可能导致段错误或内存异常）。
//
//   multi_threaded_global:
//     假定程序多线程运行。通过一个全局互斥体（Windows 上为临界区）来
//     保护连接操作，使对象能在任意线程中被创建或销毁。该策略占用较少
//     系统资源，但更可能发生锁竞争，从而增加上下文切换。
//
//   multi_threaded_local:
//     与 multi_threaded_global 类似，但每个信号对象及继承 has_slots 的
//     对象都维护各自的互斥体/临界区，从而减少不同对象间的锁竞争；然
//     而在某些平台上创建大量互斥对象可能影响性能，应谨慎使用。
//
// 使用说明：
//   详细用法请参考项目文档和示例代码。
//
// Libjingle 特定修改：
//   此文件已被修改以允许 has_slots 与 signal 使用不同的线程策略，例
//   如可将 has_slots<single_threaded> 与 signal0<multi_threaded_local> 相连。
//   使用时需确保：当 has_slots 为 single_threaded 时，用户不得在并发情
//   况下对该对象执行 connect 或 disconnect；当 signal 为 single_threaded
//   时，用户也需保证不会并发执行 disconnect、connect 或 signal 操作，
//   否则可能发生数据竞争。
#define RTC_BASE_THIRD_PARTY_SIGSLOT_SIGSLOT_H_

#include <cstring>
#include <list>
#include <set>

// On our copy of sigslot.h, we set single threading as default.
#define SIGSLOT_DEFAULT_MT_POLICY single_threaded

/**
 * @file sigslot.h
 * @brief 轻量级的 signal/slot 实现（支持可选多线程策略）。
 *
 * 该头文件提供了信号/槽机制的实现，包含：
 * - 多线程策略（single_threaded、multi_threaded_global、multi_threaded_local）
 * - 信号基类与连接管理
 * - 槽对象基类（用于跟踪已连接的信号）
 *
 * 注：使用前请参考项目中的线程策略宏定义以确保线程安全性。
 */

#if defined(SIGSLOT_PURE_ISO) ||                   \
    (!defined(WEBRTC_WIN) && !defined(__GNUG__) && \
     !defined(SIGSLOT_USE_POSIX_THREADS))
#define _SIGSLOT_SINGLE_THREADED
#elif defined(WEBRTC_WIN)
#define _SIGSLOT_HAS_WIN32_THREADS
#include "windows.h"
#elif defined(__GNUG__) || defined(SIGSLOT_USE_POSIX_THREADS)
#define _SIGSLOT_HAS_POSIX_THREADS
#include <pthread.h>
#else
#define _SIGSLOT_SINGLE_THREADED
#endif

#ifndef SIGSLOT_DEFAULT_MT_POLICY
#ifdef _SIGSLOT_SINGLE_THREADED
#define SIGSLOT_DEFAULT_MT_POLICY single_threaded
#else
#define SIGSLOT_DEFAULT_MT_POLICY multi_threaded_local
#endif
#endif

// TODO: change this namespace to rtc?
namespace sigslot {

class single_threaded {
 public:
  /**
   * @brief 单线程策略：锁操作为空实现，不做同步。
   *
   * 在单线程模式下，为了避免额外开销，lock()/unlock() 不执行任何操作。
   */
  void lock() {}
  /**
   * @brief 单线程策略：解锁为空实现。
   */
  void unlock() {}
};

#ifdef _SIGSLOT_HAS_WIN32_THREADS
// 多线程策略仅在启用时编译
/**
 * @brief 全局互斥多线程策略（Windows）：使用一个进程范围的临界区保护操作。
 *
 * 当使用此策略时，所有信号/槽共享同一个全局锁，从而保证跨线程的连接/触发安全性。
 */
class multi_threaded_global {
 public:
  multi_threaded_global() {
    static bool isinitialised = false;

    if (!isinitialised) {
      InitializeCriticalSection(get_critsec());
      isinitialised = true;
    }
  }

  void lock() { EnterCriticalSection(get_critsec()); }

  void unlock() { LeaveCriticalSection(get_critsec()); }

 private:
  CRITICAL_SECTION* get_critsec() {
    static CRITICAL_SECTION g_critsec;
    return &g_critsec;
  }
};

/**
 * @brief 局部互斥多线程策略（Windows）：每个对象使用独立的临界区。
 *
 * 该策略减少了不同对象之间的锁争用，但会为每个对象维护一个临界区。
 */
class multi_threaded_local {
 public:
  multi_threaded_local() { InitializeCriticalSection(&m_critsec); }

  multi_threaded_local(const multi_threaded_local&) {
    InitializeCriticalSection(&m_critsec);
  }

  ~multi_threaded_local() { DeleteCriticalSection(&m_critsec); }

  void lock() { EnterCriticalSection(&m_critsec); }

  void unlock() { LeaveCriticalSection(&m_critsec); }

 private:
  CRITICAL_SECTION m_critsec;
};
#endif  // _SIGSLOT_HAS_WIN32_THREADS

#ifdef _SIGSLOT_HAS_POSIX_THREADS
// 多线程策略仅在启用时编译
/**
 * @brief 全局互斥多线程策略（POSIX）：使用进程内的静态互斥体（pthread_mutex_t）。
 */
class multi_threaded_global {
 public:
  void lock() { pthread_mutex_lock(get_mutex()); }
  void unlock() { pthread_mutex_unlock(get_mutex()); }

 private:
  static pthread_mutex_t* get_mutex();
};

/**
 * @brief 局部互斥多线程策略（POSIX）：每个对象维护自己的 pthread_mutex_t。
 */
class multi_threaded_local {
 public:
  multi_threaded_local() { pthread_mutex_init(&m_mutex, nullptr); }
  multi_threaded_local(const multi_threaded_local&) {
    pthread_mutex_init(&m_mutex, nullptr);
  }
  ~multi_threaded_local() { pthread_mutex_destroy(&m_mutex); }
  void lock() { pthread_mutex_lock(&m_mutex); }
  void unlock() { pthread_mutex_unlock(&m_mutex); }

 private:
  pthread_mutex_t m_mutex;
};
#endif  // _SIGSLOT_HAS_POSIX_THREADS

template <class mt_policy>
/**
 * @brief RAII 锁辅助类，在构造时持有锁，析构时释放锁。
 * @tparam mt_policy 互斥策略类型，需提供 lock()/unlock() 方法。
 */
class lock_block {
 public:
  mt_policy* m_mutex;

  /**
   * @brief 构造时对传入的互斥对象加锁。
   * @param mtx 要加锁的互斥策略对象指针。
   */
  lock_block(mt_policy* mtx) : m_mutex(mtx) { m_mutex->lock(); }

  /**
   * @brief 析构时释放锁。
   */
  ~lock_block() { m_mutex->unlock(); }
};

class _signal_base_interface;

/**
 * @brief 槽对象接口基类。
 *
 * 任何希望成为槽（slot）目标的类应该通过继承该接口来接收来自
 * 信号（signal）的连接/断开通知。该类仅提供内部回调机制，
 * 具体接口由 has_slots 模板类实现并暴露给用户类继承。
 */
class has_slots_interface {
 private:
  typedef void (*signal_connect_t)(has_slots_interface* self,
                                   _signal_base_interface* sender);
  typedef void (*signal_disconnect_t)(has_slots_interface* self,
                                      _signal_base_interface* sender);
  typedef void (*disconnect_all_t)(has_slots_interface* self);

  const signal_connect_t m_signal_connect;
  const signal_disconnect_t m_signal_disconnect;
  const disconnect_all_t m_disconnect_all;

 protected:
  has_slots_interface(signal_connect_t conn,
                      signal_disconnect_t disc,
                      disconnect_all_t disc_all)
      : m_signal_connect(conn),
        m_signal_disconnect(disc),
        m_disconnect_all(disc_all) {}

  /**
   * @brief 虚析构函数（为向后兼容保留为 virtual）。
   *
   * 该类作为内部接口使用，析构时会确保派生类的析构函数被正确调用。
   */
  virtual ~has_slots_interface() {}

 public:
  void signal_connect(_signal_base_interface* sender) {
    m_signal_connect(this, sender);
  }
  /**
   * @brief 通知槽对象有新的信号与之连接。
   * @param sender 触发此连接的信号对象接口指针。
   *
   * 该方法为内部回调，由信号在建立连接时调用，用于让槽对象记录发送者。
   */
  void signal_disconnect(_signal_base_interface* sender) {
    m_signal_disconnect(this, sender);
  }

  /**
   * @brief 通知槽对象与指定信号断开连接。
   * @param sender 触发此断开操作的信号对象接口指针。
   */
  void disconnect_all() { m_disconnect_all(this); }
};

/**
 * @brief 信号基类接口。
 *
 * _signal_base_interface 提供了用于断开槽和复制槽连接的回调接口，
 * 由具体的 _signal_base 实现并传递给槽对象以便管理连接关系。
 */
class _signal_base_interface {
 private:
  typedef void (*slot_disconnect_t)(_signal_base_interface* self,
                                    has_slots_interface* pslot);
  typedef void (*slot_duplicate_t)(_signal_base_interface* self,
                                   const has_slots_interface* poldslot,
                                   has_slots_interface* pnewslot);

  const slot_disconnect_t m_slot_disconnect;
  const slot_duplicate_t m_slot_duplicate;

 protected:
    /**
     * @brief 构造
     * @param disc 槽断开回调函数
     * @param dupl 槽复制回调函数
     */
    _signal_base_interface(slot_disconnect_t disc, slot_duplicate_t dupl)
      : m_slot_disconnect(disc), m_slot_duplicate(dupl) {}

    /**
     * @brief 非虚析构，接口仅用于内部回调，不进行多态删除。
     */
    ~_signal_base_interface() {}

 public:
  void slot_disconnect(has_slots_interface* pslot) {
    m_slot_disconnect(this, pslot);
  }
  /**
   * @brief 当某个槽对象被复制或替换时，通知信号复制对应的连接。
   * @param poldslot 旧的槽对象（源）
   * @param pnewslot 新的槽对象（目标）
   */
  void slot_duplicate(const has_slots_interface* poldslot,
                      has_slots_interface* pnewslot) {
    m_slot_duplicate(this, poldslot, pnewslot);
  }
};

/**
 * @brief 不透明连接对象（封装对槽对象与成员函数的引用）。
 *
 * _opaque_connection 保存了目标对象指针和成员函数指针的二进制拷贝，
 * 并提供通用的 emit 调用机制以在信号触发时调用对应的成员函数。
 */
class _opaque_connection {
 private:
  typedef void (*emit_t)(const _opaque_connection*);
  template <typename FromT, typename ToT>
  union union_caster {
    FromT from;
    ToT to;
  };

  emit_t pemit;
  has_slots_interface* pdest;
  // 成员函数指针在虚拟类中可能长度可达 16 字节，
  // 因此为其分配足够的存储空间。
  unsigned char pmethod[16];

 public:
  /**
   * @brief 构造一个不透明连接，绑定目标对象与成员函数。
   * @tparam DestT 目标对象类型
   * @tparam Args 成员函数参数类型包
   * @param pd 目标对象指针
   * @param pm 目标对象的成员函数指针
   */
  template <typename DestT, typename... Args>
  _opaque_connection(DestT* pd, void (DestT::*pm)(Args...)) : pdest(pd) {
    typedef void (DestT::*pm_t)(Args...);
    static_assert(sizeof(pm_t) <= sizeof(pmethod),
                  "Size of slot function pointer too large.");

    std::memcpy(pmethod, &pm, sizeof(pm_t));

    typedef void (*em_t)(const _opaque_connection* self, Args...);
    union_caster<em_t, emit_t> caster2;
    caster2.from = &_opaque_connection::emitter<DestT, Args...>;
    pemit = caster2.to;
  }

  /**
   * @brief 返回目标槽对象指针。
   * @return 目标 `has_slots_interface*`。
   */
  has_slots_interface* getdest() const { return pdest; }

  /**
   * @brief 复制连接并替换目标对象指针。
   * @param newtarget 新的目标槽对象指针。
   * @return 返回一个新的 _opaque_connection，其 pdest 指向 newtarget。
   *
   * 在复制 has_slots 对象时使用，用以将旧目标的连接复制到新目标上。
   */
  _opaque_connection duplicate(has_slots_interface* newtarget) const {
    _opaque_connection res = *this;
    res.pdest = newtarget;
    return res;
  }

  // 仅调用在构造时保存的 "emitter" 函数指针以触发槽。
  /**
   * @brief 触发该连接对应的槽调用，转发参数包。
   * @tparam Args 参数类型包
   * @param args 调用时传递的参数列表
   */
  template <typename... Args>
  void emit(Args... args) const {
    typedef void (*em_t)(const _opaque_connection*, Args...);
    union_caster<emit_t, em_t> caster;
    caster.from = pemit;
    (caster.to)(this, args...);
  }

 private:
  /**
   * @brief 真正调用目标成员函数的静态模板函数，被 emit() 间接调用。
   * @tparam DestT 目标对象类型
   * @tparam Args 参数类型包
   * @param self 指向当前连接对象
   * @param args 参数转发
   */
  template <typename DestT, typename... Args>
  static void emitter(const _opaque_connection* self, Args... args) {
    typedef void (DestT::*pm_t)(Args...);
    pm_t pm;
    std::memcpy(&pm, self->pmethod, sizeof(pm_t));
    (static_cast<DestT*>(self->pdest)->*(pm))(args...);
  }
};

/**
 * @brief 信号基类模板，负责管理已连接的槽列表并在触发时调用它们。
 * @tparam mt_policy 互斥策略类型，用于在多线程环境下保护连接列表。
 */
template <class mt_policy>
class _signal_base : public _signal_base_interface, public mt_policy {
 protected:
  typedef std::list<_opaque_connection> connections_list;

  _signal_base()
      : _signal_base_interface(&_signal_base::do_slot_disconnect,
                               &_signal_base::do_slot_duplicate),
        m_current_iterator(m_connected_slots.end()) {}
  /**
   * @brief 构造函数：初始化基类回调并将当前迭代器设置为 end。
   */
  _signal_base()
      : _signal_base_interface(&_signal_base::do_slot_disconnect,
                               &_signal_base::do_slot_duplicate),
        m_current_iterator(m_connected_slots.end()) {}

  /**
   * @brief 析构函数：断开所有连接以避免悬挂引用。
   */
  ~_signal_base() { disconnect_all(); }

 private:
  _signal_base& operator=(_signal_base const& that);

 public:
  _signal_base(const _signal_base& o)
      : _signal_base_interface(&_signal_base::do_slot_disconnect,
                               &_signal_base::do_slot_duplicate),
        m_current_iterator(m_connected_slots.end()) {
    lock_block<mt_policy> lock(this);
    for (const auto& connection : o.m_connected_slots) {
      connection.getdest()->signal_connect(this);
      m_connected_slots.push_back(connection);
    }
  }

  /**
   * @brief 判断信号是否有连接的槽。
   * @return 若无连接则返回 true。
   */
  bool is_empty() {
    lock_block<mt_policy> lock(this);
    return m_connected_slots.empty();
  }

  /**
   * @brief 断开所有已连接的槽，并通知每个槽对象执行相应的清理。
   *
   * 该函数在锁保护下逐一弹出连接并调用槽的 signal_disconnect 回调。
   */
  void disconnect_all() {
    lock_block<mt_policy> lock(this);

    while (!m_connected_slots.empty()) {
      has_slots_interface* pdest = m_connected_slots.front().getdest();
      m_connected_slots.pop_front();
      pdest->signal_disconnect(static_cast<_signal_base_interface*>(this));
    }
    // 若在信号触发（遍历连接列表）期间调用 disconnect_all，
    // 将当前迭代器置为 end，避免悬垂迭代器被解引用。
    m_current_iterator = m_connected_slots.end();
  }

#if !defined(NDEBUG)
  bool connected(has_slots_interface* pclass) {
    lock_block<mt_policy> lock(this);
    connections_list::const_iterator it = m_connected_slots.begin();
    connections_list::const_iterator itEnd = m_connected_slots.end();
    while (it != itEnd) {
      if (it->getdest() == pclass)
        return true;
      ++it;
    }
    return false;
  }
#endif

  void disconnect(has_slots_interface* pclass) {
    lock_block<mt_policy> lock(this);
    connections_list::iterator it = m_connected_slots.begin();
    connections_list::iterator itEnd = m_connected_slots.end();

    while (it != itEnd) {
      if (it->getdest() == pclass) {
        // 如果当前正在遍历该迭代器（信号正在 firing），需要移动当前迭代器
        // 以避免其被删除后仍被解引用。
        if (m_current_iterator == it) {
          m_current_iterator = m_connected_slots.erase(it);
        } else {
          m_connected_slots.erase(it);
        }
        pclass->signal_disconnect(static_cast<_signal_base_interface*>(this));
        return;
      }
      ++it;
    }
  }

 private:
  static void do_slot_disconnect(_signal_base_interface* p,
                                 has_slots_interface* pslot) {
    _signal_base* const self = static_cast<_signal_base*>(p);
    lock_block<mt_policy> lock(self);
    connections_list::iterator it = self->m_connected_slots.begin();
    connections_list::iterator itEnd = self->m_connected_slots.end();

    while (it != itEnd) {
      connections_list::iterator itNext = it;
      ++itNext;

      if (it->getdest() == pslot) {
        // 如果在信号触发过程中槽被断开，确保当前迭代器不会被悬空。
        if (self->m_current_iterator == it) {
          self->m_current_iterator = self->m_connected_slots.erase(it);
        } else {
          self->m_connected_slots.erase(it);
        }
      }

      it = itNext;
    }
  }

  static void do_slot_duplicate(_signal_base_interface* p,
                                const has_slots_interface* oldtarget,
                                has_slots_interface* newtarget) {
    _signal_base* const self = static_cast<_signal_base*>(p);
    lock_block<mt_policy> lock(self);
    connections_list::iterator it = self->m_connected_slots.begin();
    connections_list::iterator itEnd = self->m_connected_slots.end();

    while (it != itEnd) {
      if (it->getdest() == oldtarget) {
        self->m_connected_slots.push_back(it->duplicate(newtarget));
      }

      ++it;
    }
  }

 protected:
  connections_list m_connected_slots;

  // Used to handle a slot being disconnected while a signal is
  // firing (iterating m_connected_slots).
  connections_list::iterator m_current_iterator;
  bool m_erase_current_iterator = false;
};

/**
 * @brief 槽对象模板基类。
 * @tparam mt_policy 互斥策略类型，用来保护发送者集合（m_senders）的并发访问。
 *
 * 继承自该模板的类可以作为槽对象，自动维护与其相连的信号集合，
 * 析构时会断开所有连接。
 */
template <class mt_policy = SIGSLOT_DEFAULT_MT_POLICY>
class has_slots : public has_slots_interface, public mt_policy {
 private:
  typedef std::set<_signal_base_interface*> sender_set;
  typedef sender_set::const_iterator const_iterator;

 public:
  has_slots()
      : has_slots_interface(&has_slots::do_signal_connect,
                            &has_slots::do_signal_disconnect,
                            &has_slots::do_disconnect_all) {}

  has_slots(has_slots const& o)
      : has_slots_interface(&has_slots::do_signal_connect,
                            &has_slots::do_signal_disconnect,
                            &has_slots::do_disconnect_all) {
    lock_block<mt_policy> lock(this);
    for (auto* sender : o.m_senders) {
      sender->slot_duplicate(&o, this);
      m_senders.insert(sender);
    }
  }

  /**
   * @brief 析构：断开所有与该槽对象相关联的信号连接。
   */
  ~has_slots() { this->disconnect_all(); }

 private:
  has_slots& operator=(has_slots const&);

  static void do_signal_connect(has_slots_interface* p,
                                _signal_base_interface* sender) {
    has_slots* const self = static_cast<has_slots*>(p);
    lock_block<mt_policy> lock(self);
    self->m_senders.insert(sender);
  }

  static void do_signal_disconnect(has_slots_interface* p,
                                   _signal_base_interface* sender) {
    has_slots* const self = static_cast<has_slots*>(p);
    lock_block<mt_policy> lock(self);
    self->m_senders.erase(sender);
  }

  static void do_disconnect_all(has_slots_interface* p) {
    has_slots* const self = static_cast<has_slots*>(p);
    lock_block<mt_policy> lock(self);
    while (!self->m_senders.empty()) {
      std::set<_signal_base_interface*> senders;
      senders.swap(self->m_senders);
      const_iterator it = senders.begin();
      const_iterator itEnd = senders.end();

      while (it != itEnd) {
        _signal_base_interface* s = *it;
        ++it;
        s->slot_disconnect(p);
      }
    }
  }

 private:
  sender_set m_senders;
};

/**
 * @brief 可带线程策略的信号模板。
 * @tparam mt_policy 互斥策略类型
 * @tparam Args 信号携带的参数类型包
 *
 * 提供连接（connect）与发射（emit）接口，支持在触发时逐个调用已连接槽。
 */
template <class mt_policy, typename... Args>
class signal_with_thread_policy : public _signal_base<mt_policy> {
 private:
  typedef _signal_base<mt_policy> base;

 protected:
  typedef typename base::connections_list connections_list;

 public:
  signal_with_thread_policy() {}

  template <class desttype>
  void connect(desttype* pclass, void (desttype::*pmemfun)(Args...)) {
    lock_block<mt_policy> lock(this);
    this->m_connected_slots.push_back(_opaque_connection(pclass, pmemfun));
    pclass->signal_connect(static_cast<_signal_base_interface*>(this));
  }

  void emit(Args... args) {
    lock_block<mt_policy> lock(this);
    this->m_current_iterator = this->m_connected_slots.begin();
    while (this->m_current_iterator != this->m_connected_slots.end()) {
      _opaque_connection const& conn = *this->m_current_iterator;
      ++(this->m_current_iterator);
      conn.emit<Args...>(args...);
    }
  }

  void operator()(Args... args) { emit(args...); }
};

// Alias with default thread policy. Needed because both default arguments
// and variadic template arguments must go at the end of the list, so we
// can't have both at once.
/**
 * @brief 默认线程策略的信号别名，使用 SIGSLOT_DEFAULT_MT_POLICY。
 *
 * 示例：`sigslot::signal<int, std::string>` 表示携带两个参数的信号。
 */
template <typename... Args>
using signal = signal_with_thread_policy<SIGSLOT_DEFAULT_MT_POLICY, Args...>;

// The previous verion of sigslot didn't use variadic templates, so you would
// need to write "sigslot::signal2<Arg1, Arg2>", for example.
// Now you can just write "sigslot::signal<Arg1, Arg2>", but these aliases
// exist for backwards compatibility.
template <typename mt_policy = SIGSLOT_DEFAULT_MT_POLICY>
using signal0 = signal_with_thread_policy<mt_policy>;

template <typename A1, typename mt_policy = SIGSLOT_DEFAULT_MT_POLICY>
using signal1 = signal_with_thread_policy<mt_policy, A1>;

template <typename A1,
          typename A2,
          typename mt_policy = SIGSLOT_DEFAULT_MT_POLICY>
using signal2 = signal_with_thread_policy<mt_policy, A1, A2>;

template <typename A1,
          typename A2,
          typename A3,
          typename mt_policy = SIGSLOT_DEFAULT_MT_POLICY>
using signal3 = signal_with_thread_policy<mt_policy, A1, A2, A3>;

template <typename A1,
          typename A2,
          typename A3,
          typename A4,
          typename mt_policy = SIGSLOT_DEFAULT_MT_POLICY>
using signal4 = signal_with_thread_policy<mt_policy, A1, A2, A3, A4>;

template <typename A1,
          typename A2,
          typename A3,
          typename A4,
          typename A5,
          typename mt_policy = SIGSLOT_DEFAULT_MT_POLICY>
using signal5 = signal_with_thread_policy<mt_policy, A1, A2, A3, A4, A5>;

template <typename A1,
          typename A2,
          typename A3,
          typename A4,
          typename A5,
          typename A6,
          typename mt_policy = SIGSLOT_DEFAULT_MT_POLICY>
using signal6 = signal_with_thread_policy<mt_policy, A1, A2, A3, A4, A5, A6>;

template <typename A1,
          typename A2,
          typename A3,
          typename A4,
          typename A5,
          typename A6,
          typename A7,
          typename mt_policy = SIGSLOT_DEFAULT_MT_POLICY>
using signal7 =
    signal_with_thread_policy<mt_policy, A1, A2, A3, A4, A5, A6, A7>;

template <typename A1,
          typename A2,
          typename A3,
          typename A4,
          typename A5,
          typename A6,
          typename A7,
          typename A8,
          typename mt_policy = SIGSLOT_DEFAULT_MT_POLICY>
using signal8 =
    signal_with_thread_policy<mt_policy, A1, A2, A3, A4, A5, A6, A7, A8>;

}  // namespace sigslot

#endif /* RTC_BASE_THIRD_PARTY_SIGSLOT_SIGSLOT_H_ */
