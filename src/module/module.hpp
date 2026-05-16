#pragma once

#include <functional>
#include <variant>
#include <vector>

namespace mod {

namespace events {

struct TickPhysics {};

struct LevelRestart {
    bool intentional = false;
};

}  // namespace events

template <class... Ts>
struct Match : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
Match(Ts...) -> Match<Ts...>;

class Module {
   public:
    using Event = std::variant<events::TickPhysics, events::LevelRestart>;

    virtual ~Module() = default;
    virtual void onEvent(Event) {}
};

inline void emit(const Module::Event&) {
    // Module event dispatch (currently unused)
}

}  // namespace mod

#define INIT_MODULE(T)                 \
   public:                             \
    static T* get() {                  \
        static T instance;             \
        return &instance;              \
    }                                  \
   private:

#define REGISTER_MODULE(T)
