#include "game/chr_call.hpp"

#include <Windows.h>

namespace erstats {

uintptr_t call_get_chr_ins_from_handle(
    uintptr_t fn, uintptr_t world_chr_man, uint64_t handle) {
    if (fn == 0 || world_chr_man == 0 || handle == 0 || handle == 0xFFFFFFFFFFFFFFFFULL) {
        return 0;
    }
    using Fn = void* (*)(void*, uint64_t*);
    const auto typed = reinterpret_cast<Fn>(fn);
    auto* world = reinterpret_cast<void*>(world_chr_man);
    __try {
        void* chr = typed(world, &handle);
        return reinterpret_cast<uintptr_t>(chr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

}  // namespace erstats
