//
// Created by LaoLittle on 2022/12/6.
//

#ifndef ATRI_CPP_ATRI_PLUGIN_H
#define ATRI_CPP_ATRI_PLUGIN_H

#include <cstddef>
#include <cstdint>
#include <string>

struct RustManaged {
    void *pointer;

    void (*drop_fn)(void *);
};

struct RustManagedCloneable {
    RustManaged value;

    RustManagedCloneable (*clone_fn)(const void *);
};

class Managed {
public:
    explicit Managed(RustManaged rs) : pointer(rs.pointer), drop(rs.drop_fn) {
    }

    virtual ~Managed() {
        drop(pointer);
    };

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
    RustManagedCloneable (*clone_fn)(const void *);
};

struct RustStr {
    const char8_t *slice;
    const size_t len;
};

RustStr from_u8str(const char8_t *str) {
    size_t len = strlen(reinterpret_cast<const char *>(str));
    return RustStr{
            str,
            len
    };
}

RustStr from_u8string(std::u8string &str) {
    return RustStr{
            str.data(),
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
    const void *intercepted;
    RustManagedCloneable base;
};

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
};

struct FFIMessageElement {
    uint8_t type;
    MessageElementUnion inner;
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

#define ATRI_EXPORT extern "C" [[maybe_unused]]

struct AtriVTable {
    RustManaged (*new_listener_c_func)(bool, bool (*fn)(FFIEvent), uint8_t);

    RustManaged (*new_listener_closure)(bool, FFIHandler, uint8_t);

    void (*event_intercept)(const void *);

    bool (*event_is_intercepted)(const void *);

    int64_t (*group_get_id)(const void *);

    RustManagedCloneable (*group_message_event_get_group)(const void *);

    FFIMessageChain (*group_message_event_get_message)(const void *);

    FFIMember (*group_message_event_get_sender)(const void *);

    void (*log)(size_t, const void *, uint8_t, RustStr);
};

struct AtriManager {
    void *manager_ptr;
    size_t handle;

    void *(*get_fun)(uint16_t);
};

AtriManager ATRI_MANAGER;
AtriVTable ATRI_VTABLE;

void *get_plugin_manager() {
    return ATRI_MANAGER.manager_ptr;
}

size_t get_plugin_handle() {
    return ATRI_MANAGER.handle;
}

ATRI_EXPORT void atri_manager_init(AtriManager manager) {
    ATRI_MANAGER = manager;

    // listener
    ATRI_VTABLE.new_listener_c_func = reinterpret_cast<RustManaged (*)(bool, bool (*fn)(FFIEvent),
                                                                       uint8_t)>(manager.get_fun(150));
    ATRI_VTABLE.new_listener_closure = reinterpret_cast<RustManaged (*)(bool, FFIHandler,
                                                                        uint8_t)>(manager.get_fun(151));
    // event
    ATRI_VTABLE.event_intercept = reinterpret_cast<void (*)(const void *)>(manager.get_fun(200));
    ATRI_VTABLE.event_is_intercepted = reinterpret_cast<bool (*)(const void *)>(manager.get_fun(201));

    // group
    ATRI_VTABLE.group_get_id = reinterpret_cast<int64_t (*)(const void *)>(manager.get_fun(400));

    // group_message_event
    ATRI_VTABLE.group_message_event_get_group = reinterpret_cast<RustManagedCloneable (*)(
            const void *)>(manager.get_fun(10000));
    ATRI_VTABLE.group_message_event_get_message = reinterpret_cast<FFIMessageChain (*)(
            const void *)>(manager.get_fun(10001));
    ATRI_VTABLE.group_message_event_get_sender = reinterpret_cast<FFIMember (*)(
            const void *)>(manager.get_fun(10002));

    ATRI_VTABLE.log = reinterpret_cast<void (*)(size_t, const void *, uint8_t, RustStr)>(manager.get_fun(20000));
}

#define ATRI_PLUGIN(class) \
void *new_fn() {           \
return new class();        \
}                          \
void enable_fn(void *plug) { \
reinterpret_cast<class *>(plug)->_safe_enable(); \
}                          \
void disable_fn(void *plug) {\
reinterpret_cast<class *>(plug)->_safe_disable();\
}                          \
void drop_fn(void *plug) { \
delete reinterpret_cast<class *>(plug);    \
}                          \
ATRI_EXPORT PluginInstance on_init() {     \
auto plug = new class();   \
                           \
return PluginInstance {    \
RustManaged {              \
plug, drop_fn                           \
}, true, PluginVTable {new_fn,enable_fn,disable_fn}, \
from_u8str(plug->name())\
};\
}

struct PluginVTable {
    void *(*new_fn)();

    void (*enable)(void *);

    void (*disable)(void *);
};

struct PluginInstance {
    RustManaged instance;
    bool should_drop;
    PluginVTable vtb;
    RustStr name;
};

namespace logger {
    static void log(const char8_t *str, int level) {
        ATRI_VTABLE.log(get_plugin_handle(), get_plugin_manager(), level, from_u8str(str));
    }

    void trace(const char8_t *str) {
        log(str, 0);
    }

    void debug(const char8_t *str) {
        log(str, 1);
    }

    void info(const char8_t *str) {
        log(str, 2);
    }

    void warn(const char8_t *str) {
        log(str, 3);
    }

    void error(const char8_t *str) {
        log(str, 4);
    }
}

namespace Atri {
    class Plugin {
    public:
        explicit Plugin() = default;

        virtual ~Plugin() = default;

    public:
        virtual const char8_t *name() = 0;

        virtual void enable() = 0;

        virtual void disable() = 0;

    public:
        void _safe_enable() {
            try {
                enable();
            } catch (std::exception &e) {
                logger::error((const char8_t *) e.what());
            }
        }

        void _safe_disable() {
            try {
                disable();
            } catch (std::exception &e) {
                logger::error((const char8_t *) e.what());
            }
        }
    };

    class result : public std::exception {
    public:
        result() = default;

    public:
        [[nodiscard]] const char *what() const noexcept override {
            return (const char *) why;
        }

    private:
        const char8_t *why = nullptr;
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

    class Group : public Contact, public ManagedCloneable {
    public:
        ~Group() override {
            logger::info(u8"group drop");
        }

        explicit Group(RustManagedCloneable rs) : ManagedCloneable(rs) {

        }

    public:
        int64_t id() override {
            return ATRI_VTABLE.group_get_id(pointer);
        }
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
        explicit Event(const void *intercepted) :
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
        const void *intercepted;
    };

    class MessageEvent : public Event {
    public:
        explicit MessageEvent(const void *intercepted) : Event(intercepted) {

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
            RustManagedCloneable rs = ATRI_VTABLE.group_message_event_get_group(pointer);
            return contact::Group(rs);
        }

        contact::Contact *contact() override {
            RustManagedCloneable rs = ATRI_VTABLE.group_message_event_get_group(pointer);

            return new contact::Group(rs);
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

    class FriendMessageEvent : public MessageEvent, public ManagedCloneable {
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
    };

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
            auto new_fn = new std::function<bool(FFIEvent)>;
            *new_fn = [fn](FFIEvent ffi) -> bool {
                Event *e;

                switch (ffi.type) {
                    case 0:
                        e = new ClientLoginEvent(ffi);
                        break;
                    case 1:
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
                    logger::error((const char8_t *) e.what());

                    return false;
                }
            };

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
