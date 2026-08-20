#pragma once

#include <cstdint>

namespace erstats {

// SEH-guarded call into the game's GetChrInsFromHandle. Returns 0 on fault.
uintptr_t call_get_chr_ins_from_handle(
    uintptr_t fn, uintptr_t world_chr_man, uint64_t handle);

}  // namespace erstats
