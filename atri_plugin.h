//
// Created by LaoLittle on 2022/12/6.
//

#ifndef ATRI_CPP_ATRI_PLUGIN_H
#define ATRI_CPP_ATRI_PLUGIN_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <cstring>
#include <functional>

#if defined(_WIN64) || defined(__CYGWIN__)
#ifdef _MSC_VER
//#pragma execution_character_set("utf-8")
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((dllexport))
#endif
#else
#if __GNUC__ >= 4
#define EXPORT __attribute__((visibility ("default")))
#else
#define EXPORT
#endif
#endif

#define EXTERN_C extern "C"

#define ATRI_EXPORT EXTERN_C EXPORT

#ifndef CDECL
#ifdef _MSC_VER
#define CDECL __cdecl
#else
#define CDECL
#endif
#endif

#define ATRI_PLUGIN_ABI_VERSION 2

inline namespace atri {
    inline namespace resource {
        class ResourceNotAvailable : public std::exception {
        public:
            ResourceNotAvailable() = default;

            [[nodiscard]] const char *what() const noexcept override {
                return "resource not available";
            }
        };

        typedef void *AtriHandle;
        typedef void **AtriPHandle;

        struct AtriHandleWrapper {
            AtriHandle ptr;

            [[nodiscard]] AtriHandle valid_handle() const {
                if (ptr) return ptr;
                else throw ResourceNotAvailable();
            }

            explicit operator bool() const {
                return ptr;
            }
        };
    }
}

struct RustManaged {
    void *pointer;

    void (CDECL *drop_fn)(void *);
};

struct RustManagedCloneable {
    RustManaged value;

    struct RustManagedCloneable (CDECL *clone_fn)(void *);
};

class Managed {
public:
    explicit Managed(RustManaged rs) :
            pointer(rs.pointer),
            drop(rs.drop_fn) {}

    virtual ~Managed() {
        if (pointer) {
            drop(pointer);
        }
    };

public:
    Managed(Managed &&r) noexcept:
            pointer(std::exchange(r.pointer, nullptr)),
            drop(std::exchange(r.drop, nullptr)) {

    }

protected:
    void *pointer;

private:
    void (*drop)(void *);

protected:
    Managed(Managed const &ma) = default;

private:
    Managed &operator=(Managed const &) = default;
};

class ManagedCloneable : public Managed {
public:
    explicit ManagedCloneable(RustManagedCloneable rs) : Managed(rs.value), clone_fn(rs.clone_fn) {
    }

    ManagedCloneable(ManagedCloneable const &ma) : Managed(ma), clone_fn(ma.clone_fn) {
        RustManagedCloneable rs = clone_fn(pointer);
        pointer = rs.value.pointer;
    }

public:
    ManagedCloneable &operator=(const ManagedCloneable &rhs) {
        if (this == &rhs) {
            return *this;
        }

        RustManagedCloneable rs = rhs.clone_fn(rhs.pointer);
        this->pointer = rs.value.pointer;

        return *this;
    }

protected:
    ManagedCloneable clone() {
        return ManagedCloneable(clone_fn(pointer));
    }

private:
    RustManagedCloneable (CDECL *clone_fn)(void *);
};

struct RustStr {
    char *slice;
    size_t len;
};

RustStr from_u8str(const char *str) {
    size_t len = strlen(str);
    return RustStr{
            (char *) str,
            len
    };
}

RustStr from_u8string(std::string &str) {
    return RustStr{
            (char *) str.data(),
            str.length(),
    };
}

struct RustString {
    char *slice;
    size_t len;
    size_t capacity;
};

template<typename T>
struct RustVec {
    T *ptr;
    size_t len;
    size_t capacity;
};

struct FFIEvent {
    uint8_t type;
    void *intercepted;
    RustManagedCloneable base;
};

#define CLIENT_LOGIN 0
#define GROUP_MESSAGE 1

struct FFIHandler {
    RustManaged closure;

    bool (*invoke)(void *, FFIEvent);
};

struct FFIAt {
    int64_t target;
    RustString display;
};

union MessageElementUnion {
    RustString text;
    ManagedCloneable image;
    FFIAt at;
    char at_all[0];
    ManagedCloneable unknown;

    ~MessageElementUnion() = delete;
};

struct FFIMessageElement {
    uint8_t type;
    MessageElementUnion inner;

    ~FFIMessageElement() = delete;
};

struct FFIAnonymous {
    RustVec<uint8_t> anon_id;
    RustString nick;
    int32_t portrait_index;
    int32_t bubble_index;
    int32_t expire_time;
    RustString color;
};

struct FFIReply {
    int32_t reply_seq;
    int64_t sender;
    int32_t time;
    RustVec<FFIMessageElement> elements;
};

struct FFIMessageMetadata {
    RustVec<int32_t> seqs;
    RustVec<int32_t> rands;
    int32_t time;
    int64_t sender;
    uint8_t flag;
};

struct FFIMessageChain {
    FFIMessageMetadata metadata;
    RustVec<FFIMessageElement> inner;
};

struct FFIMember {
    bool is_named;
    RustManagedCloneable inner;
};

struct AtriVTable {
    RustManaged (CDECL *new_listener_c_func)(bool, bool (*fn)(FFIEvent), uint8_t);

    RustManaged (CDECL *new_listener_closure)(bool, FFIHandler, uint8_t);

    void (CDECL *event_intercept)(void *);

    bool (CDECL *event_is_intercepted)(void *);

    int64_t (CDECL *group_get_id)(AtriHandle);

    AtriHandle (CDECL *group_clone)(AtriHandle);

    void (CDECL *group_drop)(AtriHandle);

    AtriPHandle (CDECL *group_message_event_get_group)(void *);

    FFIMessageChain (CDECL *group_message_event_get_message)(void *);

    FFIMember (CDECL *group_message_event_get_sender)(void *);

    void (CDECL *log)(size_t, void *, uint8_t, RustStr);
};

struct AtriManager {
    void *manager_ptr;
    size_t handle;

    void *(CDECL *get_fun)(uint16_t);
};

AtriManager ATRI_MANAGER;
AtriVTable ATRI_VTABLE;

void *get_plugin_manager() {
    return ATRI_MANAGER.manager_ptr;
}

size_t get_plugin_handle() {
    return ATRI_MANAGER.handle;
}

ATRI_EXPORT void CDECL atri_manager_init(AtriManager manager) {
    ATRI_MANAGER = manager;

    // listener
    ATRI_VTABLE.new_listener_c_func = reinterpret_cast<RustManaged (*)(bool, bool (*fn)(FFIEvent),
                                                                       uint8_t)>(manager.get_fun(150));
    ATRI_VTABLE.new_listener_closure = reinterpret_cast<RustManaged (*)(bool, FFIHandler,
                                                                        uint8_t)>(manager.get_fun(151));
    // event
    ATRI_VTABLE.event_intercept = reinterpret_cast<void (*)(void *)>(manager.get_fun(200));
    ATRI_VTABLE.event_is_intercepted = reinterpret_cast<bool (*)(void *)>(manager.get_fun(201));

    // group
    ATRI_VTABLE.group_get_id = reinterpret_cast<int64_t (*)(AtriHandle)>(manager.get_fun(400));
    ATRI_VTABLE.group_clone = reinterpret_cast<AtriHandle (*)(AtriHandle)>(manager.get_fun(420));
    ATRI_VTABLE.group_drop = reinterpret_cast<void (*)(AtriHandle)>(manager.get_fun(421));

    // group_message_event
    ATRI_VTABLE.group_message_event_get_group = reinterpret_cast<AtriPHandle (*)(
            void *)>(manager.get_fun(10000));
    ATRI_VTABLE.group_message_event_get_message = reinterpret_cast<FFIMessageChain (*)(
            void *)>(manager.get_fun(10001));
    ATRI_VTABLE.group_message_event_get_sender = reinterpret_cast<FFIMember (*)(
            void *)>(manager.get_fun(10002));

    ATRI_VTABLE.log = reinterpret_cast<void (*)(size_t, void *, uint8_t, RustStr)>(manager.get_fun(20000));
}

#define ATRI_PLUGIN(class, name) \
void * CDECL new_fn() {           \
return new class();        \
}                          \
void CDECL enable_fn(void *plug) { \
reinterpret_cast<class *>(plug)->_safe_enable(); \
}                          \
void CDECL disable_fn(void *plug) {\
reinterpret_cast<class *>(plug)->_safe_disable();\
}                          \
void CDECL drop_fn(void *plug) { \
delete reinterpret_cast<class *>(plug);    \
}                          \
ATRI_EXPORT PluginInstance CDECL on_init() {     \
return PluginInstance {    \
1, PluginVTable {new_fn,enable_fn,disable_fn, drop_fn}, ATRI_PLUGIN_ABI_VERSION, \
from_u8str(name)\
};\
}

struct PluginVTable {
    void *(CDECL *new_fn)();

    void (CDECL *enable)(void *);

    void (CDECL *disable)(void *);

    void (CDECL *drop)(void *);
};

struct PluginInstance {
    uint8_t should_drop;
    PluginVTable vtb;
    uint8_t abi_ver;
    RustStr name;
};

namespace logger {
    static void log(const char *str, int level) {
        ATRI_VTABLE.log(get_plugin_handle(), get_plugin_manager(), level, from_u8str(str));
    }

    void trace(const char *str) {
        log(str, 0);
    }

    void debug(const char *str) {
        log(str, 1);
    }

    void info(const char *str) {
        log(str, 2);
    }

    void warn(const char *str) {
        log(str, 3);
    }

    void error(const char *str) {
        log(str, 4);
    }
}

inline namespace atri {
    class Plugin {
    public:
        explicit Plugin() = default;

        virtual ~Plugin() = default;

    public:
        virtual void enable() = 0;

        virtual void disable() = 0;

    public:
        void _safe_enable() {
            try {
                enable();
            } catch (std::exception &e) {
                logger::error(e.what());
            }
        }

        void _safe_disable() {
            try {
                disable();
            } catch (std::exception &e) {
                logger::error(e.what());
            }
        }
    };
}

namespace message {
    class Message {

    };

    class MessageMetadata {
    public:
        explicit MessageMetadata(FFIMessageMetadata ffi) :
                seqs(ffi.seqs),
                rands(ffi.rands),
                time(ffi.time),
                sender(ffi.sender),
                flag(ffi.flag) {

        }

    private:
        RustVec<int32_t> seqs;
        RustVec<int32_t> rands;
        int32_t time;
        int64_t sender;
        uint8_t flag;
    };

    class MessageChain {
    private:
        MessageMetadata meta;

    };
}

namespace contact {
    class Contact {
    public:
        virtual int64_t id() = 0;
    };

    class Group : public Contact {
    public:
        ~Group() {
            if (handle && !is_ref) {
                ATRI_VTABLE.group_drop(handle.ptr);
            }
        }

        Group(Group &&rhs) noexcept:
                handle(std::exchange(rhs.handle, {})) {

        }

        explicit Group(AtriHandle handle) : handle({handle}), is_ref(false) {

        }

        explicit Group(AtriPHandle handle) : handle({*handle}), is_ref(true) {

        }

    public:
        int64_t id() override {
            return ATRI_VTABLE.group_get_id(handle.valid_handle());
        }

    private:
        AtriHandleWrapper handle;
        bool is_ref;
    };

    class Member : public Contact {
    };

    class NamedMember : public Member, public ManagedCloneable {
    public:
        explicit NamedMember(RustManagedCloneable rs) : ManagedCloneable(rs) {

        }

    public:
        int64_t id() override {
            return 0;
        }
    };

    class AnonymousMember : public Member, public ManagedCloneable {
    public:
        explicit AnonymousMember(RustManagedCloneable rs) : ManagedCloneable(rs) {

        }

    public:
        int64_t id() override {
            return 0;
        }
    };
}

namespace event {
    class Event {
    public:
        virtual ~Event() = default;

    public:
        explicit Event(void *intercepted) :
                intercepted(intercepted) {

        }

    public:
        void intercept() {
            ATRI_VTABLE.event_intercept(intercepted);
        }

        bool is_intercepted() {
            return ATRI_VTABLE.event_is_intercepted(intercepted);
        }

    private:
        void *intercepted;
    };

    class MessageEvent : public Event {
    public:
        explicit MessageEvent(void *intercepted) : Event(intercepted) {

        }

    public:
        virtual contact::Contact *contact() = 0;

        virtual contact::Contact *sender() = 0;
    };

    class ClientLoginEvent : public Event, public ManagedCloneable {
    public:
        explicit ClientLoginEvent(FFIEvent e) : Event(e.intercepted), ManagedCloneable(e.base) {

        }
    };

    class GroupMessageEvent : public MessageEvent, public ManagedCloneable {
    public:
        explicit GroupMessageEvent(FFIEvent e) : MessageEvent(e.intercepted), ManagedCloneable(e.base) {

        }

    public:
        contact::Group group() {
            AtriPHandle pHandle = ATRI_VTABLE.group_message_event_get_group(pointer);
            return contact::Group(pHandle);
        }

        contact::Contact *contact() override {
            AtriPHandle pHandle = ATRI_VTABLE.group_message_event_get_group(pointer);

            return new contact::Group(pHandle);
        }

        contact::Contact *sender() override {
            FFIMember rs = ATRI_VTABLE.group_message_event_get_sender(pointer);
            if (rs.is_named) {
                return new contact::NamedMember(rs.inner);
            } else {
                return new contact::AnonymousMember(rs.inner);
            }
        }
    };

    /*class FriendMessageEvent : public MessageEvent, public ManagedCloneable {
    public:
        explicit FriendMessageEvent(FFIEvent e) : MessageEvent(e.intercepted), ManagedCloneable(e.base) {

        }

    public:
        contact::Contact *contact() override {
            return new contact::Group({});
        }

        contact::Contact *sender() override {
            return new contact::Group({});
        }
    };*/

    class ListenerGuard : Managed {
    public:
        explicit ListenerGuard(RustManaged rs) : Managed(rs) {}

    public:
        void close() {
            delete this;
        };
    };

    static bool handle(void *closure, FFIEvent e) {
        auto fn = reinterpret_cast<std::function<bool(FFIEvent)> *>(closure);
        return fn->operator()(e);
    }

    static void drop_closure(void *closure) {
        auto fn = reinterpret_cast<std::function<bool(FFIEvent)> *>(closure);
        delete fn;
    }

    class Listener {
    public:
        /***
         * listen to a event
         * @tparam E: event type
         * @param fn: event handler, return false to close this listener
         * @return a listener guard, which can be use to stop listen events
         */
        template<class E>
        static ListenerGuard *listening_on(std::function<bool(E *)> fn) {
            static_assert(std::is_convertible<E *, Event *>());
            auto new_fn = new std::function<bool(FFIEvent)>([fn](FFIEvent ffi) -> bool {
                Event *e;

                switch (ffi.type) {
                    case CLIENT_LOGIN:
                        e = new ClientLoginEvent(ffi);
                        break;
                    case GROUP_MESSAGE:
                        e = new GroupMessageEvent(ffi);
                        break;

                    default:
                        ManagedCloneable(ffi.base); // drop it
                        return true;
                }

                E *need = dynamic_cast<E *>(e);
                if (!need) {
                    delete e;
                    return true;
                }

                try {
                    bool ret = fn(need);
                    delete need;
                    return ret;
                } catch (std::exception &e) {
                    logger::error(e.what());

                    return false;
                }
            });

            RustManaged ma = ATRI_VTABLE.new_listener_closure(true, {{new_fn, drop_closure}, handle}, 0);
            return new ListenerGuard(ma);
        }

        template<class E>
        static ListenerGuard *listening_on_always(std::function<void(E *)> fn) {
            return listening_on<E>([fn](E *e) -> bool {
                fn(e);
                return true;
            });
        }
    };
}

#endif //ATRI_CPP_ATRI_PLUGIN_H
