#include "game/gamedata.hpp"

#include "game/aob.hpp"
#include "game/chr_call.hpp"
#include "game/item_names.hpp"
#include "game/location.hpp"
#include "game/offsets.hpp"
#include "game/progress.hpp"
#include "util.hpp"

#include <Windows.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace erstats {
namespace {

bool readable(uintptr_t address, size_t size) {
    if (address == 0 || size == 0) {
        return false;
    }
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &info, sizeof(info)) == 0) {
        return false;
    }
    if (info.State != MEM_COMMIT) {
        return false;
    }
    const uintptr_t start = reinterpret_cast<uintptr_t>(info.BaseAddress);
    const uintptr_t end = start + info.RegionSize;
    if (address < start || address + size > end) {
        return false;
    }
    const DWORD protect = info.Protect;
    return protect == PAGE_READONLY || protect == PAGE_READWRITE || protect == PAGE_WRITECOPY
        || protect == PAGE_EXECUTE_READ || protect == PAGE_EXECUTE_READWRITE
        || protect == PAGE_EXECUTE_WRITECOPY;
}

template <typename T>
std::optional<T> read_pod(std::span<const uint8_t> bytes, size_t offset) {
    if (offset + sizeof(T) > bytes.size()) {
        return std::nullopt;
    }
    T value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    return value;
}

template <typename T>
bool safe_read(uintptr_t address, T& out) {
    if (!readable(address, sizeof(T))) {
        return false;
    }
    std::memcpy(&out, reinterpret_cast<const void*>(address), sizeof(T));
    return true;
}

bool safe_read_bytes(uintptr_t address, uint8_t* dest, size_t size) {
    if (dest == nullptr || !readable(address, size)) {
        return false;
    }
    std::memcpy(dest, reinterpret_cast<const void*>(address), size);
    return true;
}

std::string hex_u64(uint64_t value) {
    char buf[19]{};
    std::snprintf(buf, sizeof(buf), "0x%llX", static_cast<unsigned long long>(value));
    return buf;
}

std::string hex_i32(int32_t value) {
    char buf[11]{};
    std::snprintf(buf, sizeof(buf), "0x%X", static_cast<unsigned>(value));
    return buf;
}

bool pointer_in_module(uintptr_t address, uintptr_t module_base, size_t module_size) {
    return module_size != 0 && address >= module_base && address < module_base + module_size;
}

bool csfeman_object_looks_valid(uintptr_t fe, uintptr_t module_base, size_t module_size) {
    const auto& off = current_offsets();
    uintptr_t vftable = 0;
    if (!safe_read(fe, vftable) || !pointer_in_module(vftable, module_base, module_size)) {
        return false;
    }
    uintptr_t menu_man = 0;
    uintptr_t front_end = 0;
    if (!safe_read(fe + off.csfeman_menu_man, menu_man) || menu_man == 0) {
        return false;
    }
    if (!safe_read(fe + off.csfeman_front_end_view, front_end) || front_end == 0) {
        return false;
    }
    if (pointer_in_module(menu_man, module_base, module_size)
        || pointer_in_module(front_end, module_base, module_size)) {
        return false;
    }
    uint8_t hud = 0xFF;
    if (!safe_read(fe + off.csfeman_hud_state, hud) || hud > 3) {
        return false;
    }
    const size_t bars_bytes =
        static_cast<size_t>(off.boss_bar_count) * off.boss_bar_stride;
    std::vector<uint8_t> bars(bars_bytes);
    if (!safe_read_bytes(fe + off.csfeman_boss_bars, bars.data(), bars.size())) {
        return false;
    }
    for (int i = 0; i < off.boss_bar_count; ++i) {
        const size_t entry = static_cast<size_t>(i) * off.boss_bar_stride;
        const int32_t fmg = read_pod<int32_t>(bars, entry + off.boss_bar_display_id).value_or(0);
        const uint64_t handle = read_pod<uint64_t>(bars, entry + off.boss_bar_handle).value_or(0);
        if (!boss_bar_ids_plausible(fmg, handle)) {
            return false;
        }
    }
    return true;
}

}  // namespace

std::optional<uintptr_t> scan_module_patterns(
    std::span<const uint8_t> image,
    uintptr_t image_base,
    const std::vector<AobPattern>& patterns) {
    for (const auto& pattern : patterns) {
        if (const auto ptr = find_rip_pointer(
                image, pattern.ida, pattern.rel32_offset, pattern.instruction_size, image_base)) {
            return ptr;
        }
    }
    return std::nullopt;
}

std::optional<uintptr_t> scan_gamedataman(std::span<const uint8_t> image, uintptr_t image_base) {
    return scan_module_patterns(image, image_base, gamedataman_patterns());
}

std::optional<uintptr_t> scan_worldchrman(std::span<const uint8_t> image, uintptr_t image_base) {
    return scan_module_patterns(image, image_base, worldchrman_patterns());
}

std::optional<uintptr_t> scan_gameman(std::span<const uint8_t> image, uintptr_t image_base) {
    return scan_module_patterns(image, image_base, gameman_patterns());
}

std::optional<uintptr_t> scan_eventflag(std::span<const uint8_t> image, uintptr_t image_base) {
    return scan_module_patterns(image, image_base, eventflag_patterns());
}

std::optional<uintptr_t> scan_csfeman(std::span<const uint8_t> image, uintptr_t image_base) {
    return scan_module_patterns(image, image_base, csfeman_patterns());
}

bool csfeman_layout_plausible(std::span<const uint8_t> csfeman) {
    const auto& off = current_offsets();
    const auto hud = read_pod<uint8_t>(csfeman, off.csfeman_hud_state);
    if (!hud || *hud > 3) {
        return false;
    }
    for (int i = 0; i < off.boss_bar_count; ++i) {
        const size_t entry =
            off.csfeman_boss_bars + static_cast<size_t>(i) * off.boss_bar_stride;
        const auto fmg = read_pod<int32_t>(csfeman, entry + off.boss_bar_display_id);
        const auto handle = read_pod<uint64_t>(csfeman, entry + off.boss_bar_handle);
        if (!fmg || !handle || !boss_bar_ids_plausible(*fmg, *handle)) {
            return false;
        }
    }
    return true;
}

std::optional<uintptr_t> pick_csfeman_slot(std::span<const CsFeManCandidate> candidates) {
    for (const auto& candidate : candidates) {
        if (candidate.unique_pattern && candidate.slot != 0) {
            return candidate.slot;
        }
    }
    for (const auto& candidate : candidates) {
        if (candidate.slot != 0 && candidate.instance != 0 && candidate.layout_ok) {
            return candidate.slot;
        }
    }
    return std::nullopt;
}

std::optional<uintptr_t> scan_get_chr_ins_from_handle(
    std::span<const uint8_t> image, uintptr_t image_base) {
    for (const auto& pattern : get_chr_ins_from_handle_patterns()) {
        if (const auto addr = find_function(image, pattern.ida, image_base)) {
            return addr;
        }
    }
    return std::nullopt;
}

namespace {

CharacterStats read_stats(uintptr_t player, const Offsets& off) {
    CharacterStats stats;
    safe_read(player + off.vigor, stats.vigor);
    safe_read(player + off.mind, stats.mind);
    safe_read(player + off.endurance, stats.endurance);
    safe_read(player + off.strength, stats.strength);
    safe_read(player + off.dexterity, stats.dexterity);
    safe_read(player + off.intelligence, stats.intelligence);
    safe_read(player + off.faith, stats.faith);
    safe_read(player + off.arcane, stats.arcane);
    return stats;
}

FlaskInfo read_flasks(uintptr_t player, const Offsets& off) {
    uint8_t hp = 0;
    uint8_t fp = 0;
    FlaskInfo allocated;
    if (safe_read(player + off.flask_hp, hp) && safe_read(player + off.flask_fp, fp)) {
        allocated = flasks_from_allocation(hp, fp, 0);
    }

    uintptr_t inventory = 0;
    if (!safe_read(player + off.inventory, inventory) || inventory == 0) {
        return allocated;
    }
    uintptr_t list = 0;
    uint32_t count = 0;
    if (!safe_read(inventory + off.inventory_list, list) || list == 0) {
        return allocated;
    }
    if (!safe_read(inventory + off.inventory_count, count) || count == 0 || count > 2688) {
        return allocated;
    }
    std::vector<InventoryItem> items;
    items.reserve(32);
    uint32_t seen = 0;
    for (uint32_t i = 0; i < 2688 && seen < count; ++i) {
        const uintptr_t entry = list + static_cast<uintptr_t>(i) * off.inventory_stride;
        uint32_t id = 0;
        uint32_t qty = 0;
        if (!safe_read(entry + off.item_id, id)) {
            break;
        }
        if (id == 0xFFFFFFFFu) {
            continue;
        }
        ++seen;
        if (!safe_read(entry + off.item_qty, qty)) {
            continue;
        }
        items.push_back({id, qty});
    }
    return prefer_allocated_flasks(allocated, flasks_from_inventory(items));
}

std::string resolve_last_grace(uintptr_t game_man, const Offsets& off, uint32_t& grace_id) {
    grace_id = 0;
    if (game_man == 0) {
        return {};
    }
    uint32_t primary = 0;
    uint32_t alt = 0;
    safe_read(game_man + off.last_grace, primary);
    safe_read(game_man + off.last_grace_alt, alt);
    // GameMan+0xB60 holds a bonfire warp id (e.g. 1042362950). Fall back to
    // the event-flag table in case another game version stores that instead.
    if (const auto name = grace_name_from_bonfire_id(primary); !name.empty()) {
        grace_id = primary;
        return name;
    }
    if (const auto name = grace_name_from_bonfire_id(alt); !name.empty()) {
        grace_id = alt;
        return name;
    }
    if (const auto name = grace_name_from_id(primary); !name.empty()) {
        grace_id = primary;
        return name;
    }
    if (const auto name = grace_name_from_id(alt); !name.empty()) {
        grace_id = alt;
        return name;
    }
    return {};
}

bool read_event_flag(uintptr_t virtual_memory_flag, uint32_t flag) {
    if (virtual_memory_flag == 0 || flag == 0) {
        return false;
    }
    uint32_t divisor = 0;
    if (!safe_read(virtual_memory_flag + 0x1C, divisor) || divisor == 0) {
        return false;
    }
    const uint32_t category = flag / divisor;
    const uint32_t remainder = flag - category * divisor;

    uintptr_t first = 0;
    if (!safe_read(virtual_memory_flag + 0x38, first) || first == 0) {
        return false;
    }
    uintptr_t current_element = first;
    uintptr_t current_sub = 0;
    if (!safe_read(first + 0x8, current_sub) || current_sub == 0) {
        return false;
    }

    uint8_t tag = 1;
    int guard = 0;
    while (safe_read(current_sub + 0x19, tag) && tag == 0 && guard++ < 256) {
        uint32_t key = 0;
        if (!safe_read(current_sub + 0x20, key)) {
            return false;
        }
        if (key < category) {
            if (!safe_read(current_sub + 0x10, current_sub) || current_sub == 0) {
                return false;
            }
        } else {
            current_element = current_sub;
            if (!safe_read(current_sub, current_sub) || current_sub == 0) {
                return false;
            }
        }
    }

    if (current_element == first || current_element == current_sub) {
        uint32_t key = 0;
        if (!safe_read(current_element + 0x20, key) || category < key) {
            current_element = current_sub;
        }
    }
    if (current_element == 0 || current_element == first || current_element == current_sub) {
        return false;
    }

    uint32_t mystery = 0;
    if (!safe_read(current_element + 0x28, mystery)) {
        return false;
    }
    mystery -= 1;
    uintptr_t calculated = 0;
    if (mystery == 0) {
        uint32_t scale = 0;
        uint32_t index = 0;
        uintptr_t base = 0;
        safe_read(virtual_memory_flag + 0x20, scale);
        safe_read(current_element + 0x30, index);
        safe_read(virtual_memory_flag + 0x28, base);
        calculated = static_cast<uintptr_t>(scale) * index + base;
    } else if (mystery == 1) {
        return false;
    } else {
        safe_read(current_element + 0x30, calculated);
    }
    if (calculated == 0) {
        return false;
    }
    uint8_t packed = 0;
    if (!safe_read(calculated + (remainder >> 3), packed)) {
        return false;
    }
    return event_flag_bit(remainder, packed);
}

std::vector<std::string> read_bosses_down(uintptr_t event_flag_man) {
    std::vector<std::string> down;
    if (event_flag_man == 0) {
        return down;
    }
    for (const auto& boss : journey_bosses()) {
        if (read_event_flag(event_flag_man, boss.flag)) {
            down.emplace_back(boss.name);
        }
    }
    return down;
}

uint32_t item_id_from_param(int32_t raw) {
    return static_cast<uint32_t>(raw);
}

uint32_t clamp_weapon_slot(uint32_t selected) {
    return selected <= 2 ? selected : 0;
}

uint32_t resolve_talisman_id(int32_t param_raw, uint32_t gaitem_raw) {
    const uint32_t from_param = talisman_param_id(item_id_from_param(param_raw));
    if (from_param != 0) {
        return from_param;
    }
    return talisman_param_id(gaitem_raw);
}

constexpr int kTalismanSlots[] = {17, 18, 19, 20, 16, 21};

void collect_talismans(
    EquippedLoadout& loadout,
    const std::array<int32_t, 6>& params,
    const std::array<uint32_t, 6>& gaitems) {
    size_t out = 0;
    for (size_t i = 0; i < params.size() && out < loadout.talisman_ids.size(); ++i) {
        const uint32_t id = resolve_talisman_id(params[i], gaitems[i]);
        if (id == 0) {
            continue;
        }
        bool seen = false;
        for (size_t j = 0; j < out; ++j) {
            if (loadout.talisman_ids[j] == id) {
                seen = true;
                break;
            }
        }
        if (seen) {
            continue;
        }
        loadout.talisman_ids[out++] = id;
    }
}

void apply_armaments(EquippedLoadout& loadout, uint32_t arm, uint32_t left_id, uint32_t right_id) {
    loadout.left_id = left_id;
    loadout.right_id = right_id;
    loadout.two_handing = arm == 2 || arm == 3;
    loadout.attack_id = arm == 2 ? left_id : right_id;
    if (item_slot_empty(loadout.attack_id)) {
        loadout.attack_id = item_slot_empty(right_id) ? left_id : right_id;
    }
}

}  // namespace

bool GameDataReader::locate() {
    const HMODULE module = GetModuleHandleW(L"eldenring.exe");
    if (module == nullptr) {
        return false;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
        reinterpret_cast<const uint8_t*>(module) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return false;
    }
    const auto size = nt->OptionalHeader.SizeOfImage;
    const auto* base = reinterpret_cast<const uint8_t*>(module);
    const auto image = std::span<const uint8_t>{base, size};
    const auto image_base = reinterpret_cast<uintptr_t>(module);

    if (game_data_man_ == 0) {
        const auto slot = scan_gamedataman(image, image_base);
        if (!slot) {
            log_error("GameDataMan AOB not found");
            return false;
        }
        uintptr_t pointer = 0;
        if (!safe_read(*slot, pointer) || pointer == 0) {
            log_error("GameDataMan pointer is null");
            return false;
        }
        game_data_man_ = pointer;
        log_info("GameDataMan located");
    }

    if (world_chr_man_slot_ == 0) {
        if (const auto world = scan_worldchrman(image, image_base)) {
            world_chr_man_slot_ = *world;
            log_info("WorldChrMan slot located");
        } else {
            static bool logged_missing = false;
            if (!logged_missing) {
                log_error("WorldChrMan AOB not found");
                logged_missing = true;
            }
        }
    }
    if (game_man_slot_ == 0) {
        if (const auto game_man = scan_gameman(image, image_base)) {
            game_man_slot_ = *game_man;
            log_info("GameMan slot located");
        }
    }
    if (event_flag_slot_ == 0) {
        if (const auto flags = scan_eventflag(image, image_base)) {
            event_flag_slot_ = *flags;
            log_info("EventFlag slot located");
        }
    }
    if (csfeman_slot_ == 0) {
        std::vector<CsFeManCandidate> candidates;
        const auto& patterns = csfeman_patterns();
        const auto& unique = patterns.front();
        if (const auto slot = find_rip_pointer(
                image, unique.ida, unique.rel32_offset, unique.instruction_size, image_base)) {
            candidates.push_back(CsFeManCandidate{
                .slot = *slot,
                .unique_pattern = true,
            });
        } else {
            int matches = 0;
            int rejected = 0;
            for (size_t i = 1; i < patterns.size(); ++i) {
                const auto& pattern = patterns[i];
                const auto compiled = compile_ida_pattern(pattern.ida);
                size_t start = 0;
                while (const auto match = find_pattern_from(image, compiled, start)) {
                    ++matches;
                    start = *match + 1;
                    const auto slot = resolve_rel32(
                        image, *match, pattern.rel32_offset, pattern.instruction_size, image_base);
                    if (!slot) {
                        continue;
                    }
                    uintptr_t fe = 0;
                    safe_read(*slot, fe);
                    const bool layout_ok =
                        fe != 0 && csfeman_object_looks_valid(fe, image_base, size);
                    if (fe != 0 && !layout_ok) {
                        ++rejected;
                    }
                    candidates.push_back(CsFeManCandidate{
                        .slot = *slot,
                        .unique_pattern = false,
                        .instance = fe,
                        .layout_ok = layout_ok,
                    });
                }
            }
            if (matches == 0) {
                static bool logged_missing = false;
                if (!logged_missing) {
                    log_error("CSFeMan AOB not found");
                    logged_missing = true;
                }
            } else if (rejected > 0) {
                static bool logged_rejected = false;
                if (!logged_rejected) {
                    log_error("CSFeMan AOB hits rejected (" + std::to_string(rejected) + ")");
                    logged_rejected = true;
                }
            }
        }
        if (const auto picked = pick_csfeman_slot(candidates)) {
            csfeman_slot_ = *picked;
            std::string_view name = "posturebar_csfeman";
            for (const auto& candidate : candidates) {
                if (candidate.slot == *picked && candidate.unique_pattern) {
                    name = unique.name;
                    break;
                }
            }
            log_info(std::string("CSFeMan slot located (") + std::string(name) + ")");
        }
    }
    if (get_chr_ins_fn_ == 0) {
        if (const auto fn = scan_get_chr_ins_from_handle(image, image_base)) {
            get_chr_ins_fn_ = *fn;
            log_info("GetChrInsFromHandle located");
        } else {
            static bool logged_missing = false;
            if (!logged_missing) {
                log_error("GetChrInsFromHandle AOB not found");
                logged_missing = true;
            }
        }
    }
    return true;
}

EquippedLoadout GameDataReader::loadout_from_player_bytes(
    std::span<const uint8_t> player_game_data) {
    const auto& off = current_offsets();
    EquippedLoadout loadout;
    const size_t chr_asm = off.equip_game_data + off.chr_asm;
    const uint32_t arm = read_pod<uint32_t>(player_game_data, chr_asm + off.chr_asm_arm_style)
        .value_or(1);
    const uint32_t left_sel = clamp_weapon_slot(
        read_pod<uint32_t>(player_game_data, chr_asm + off.chr_asm_left_slot).value_or(0));
    const uint32_t right_sel = clamp_weapon_slot(
        read_pod<uint32_t>(player_game_data, chr_asm + off.chr_asm_right_slot).value_or(0));
    const size_t ids = chr_asm + off.chr_asm_param_ids;
    const int32_t left_raw =
        read_pod<int32_t>(player_game_data, ids + (0 + left_sel * 2) * sizeof(int32_t))
            .value_or(0);
    const int32_t right_raw =
        read_pod<int32_t>(player_game_data, ids + (1 + right_sel * 2) * sizeof(int32_t))
            .value_or(0);
    apply_armaments(
        loadout, arm, item_id_from_param(left_raw), item_id_from_param(right_raw));
    const size_t gaitems = chr_asm + off.chr_asm_gaitem_handles;
    std::array<int32_t, 6> params{};
    std::array<uint32_t, 6> gaitem_ids{};
    for (size_t i = 0; i < 6; ++i) {
        params[i] = read_pod<int32_t>(
                        player_game_data, ids + static_cast<size_t>(kTalismanSlots[i]) * sizeof(int32_t))
                        .value_or(0);
        gaitem_ids[i] = read_pod<uint32_t>(
                            player_game_data,
                            gaitems + static_cast<size_t>(kTalismanSlots[i]) * sizeof(uint32_t))
                            .value_or(0);
    }
    collect_talismans(loadout, params, gaitem_ids);
    return loadout;
}

std::array<uint64_t, 3> GameDataReader::boss_handles_from_csfeman_bytes(
    std::span<const uint8_t> csfeman) {
    const auto& off = current_offsets();
    std::array<uint64_t, 3> handles{};
    for (int i = 0; i < off.boss_bar_count; ++i) {
        const size_t at =
            off.csfeman_boss_bars + static_cast<size_t>(i) * off.boss_bar_stride + off.boss_bar_handle;
        handles[static_cast<size_t>(i)] = read_pod<uint64_t>(csfeman, at).value_or(0);
    }
    return handles;
}

std::array<BossBarSlot, 3> GameDataReader::boss_slots_from_csfeman_bytes(
    std::span<const uint8_t> csfeman) {
    const auto& off = current_offsets();
    std::array<BossBarSlot, 3> slots{};
    for (int i = 0; i < off.boss_bar_count; ++i) {
        const size_t entry =
            off.csfeman_boss_bars + static_cast<size_t>(i) * off.boss_bar_stride;
        slots[static_cast<size_t>(i)].handle =
            read_pod<uint64_t>(csfeman, entry + off.boss_bar_handle).value_or(0);
        const int32_t display =
            read_pod<int32_t>(csfeman, entry + off.boss_bar_display_id).value_or(0);
        slots[static_cast<size_t>(i)].npc_param = prefer_mapped_npc_param(display, 0);
    }
    return slots;
}

CombatFrame GameDataReader::read_combat() const {
    CombatFrame frame;
    const auto& off = current_offsets();
    uint32_t arm = 1;
    std::array<int32_t, 6> talisman_raw{};
    std::array<uint32_t, 6> talisman_gaitem{};
    if (game_data_man_ != 0) {
        uintptr_t player = 0;
        if (safe_read(game_data_man_ + off.player_game_data, player) && player != 0) {
            const uintptr_t chr_asm = player + off.equip_game_data + off.chr_asm;
            uint32_t left_sel = 0;
            uint32_t right_sel = 0;
            safe_read(chr_asm + off.chr_asm_arm_style, arm);
            safe_read(chr_asm + off.chr_asm_left_slot, left_sel);
            safe_read(chr_asm + off.chr_asm_right_slot, right_sel);
            left_sel = clamp_weapon_slot(left_sel);
            right_sel = clamp_weapon_slot(right_sel);
            const uintptr_t ids = chr_asm + off.chr_asm_param_ids;
            const uintptr_t gaitems = chr_asm + off.chr_asm_gaitem_handles;
            int32_t left_raw = 0;
            int32_t right_raw = 0;
            safe_read(ids + (0 + left_sel * 2) * sizeof(int32_t), left_raw);
            safe_read(ids + (1 + right_sel * 2) * sizeof(int32_t), right_raw);
            apply_armaments(
                frame.loadout, arm, item_id_from_param(left_raw), item_id_from_param(right_raw));
            for (size_t i = 0; i < 6; ++i) {
                safe_read(
                    ids + static_cast<size_t>(kTalismanSlots[i]) * sizeof(int32_t), talisman_raw[i]);
                safe_read(
                    gaitems + static_cast<size_t>(kTalismanSlots[i]) * sizeof(uint32_t),
                    talisman_gaitem[i]);
            }
            collect_talismans(frame.loadout, talisman_raw, talisman_gaitem);
        }
    }

    uintptr_t fe_man = 0;
    if (csfeman_slot_ != 0) {
        safe_read(csfeman_slot_, fe_man);
    }
    uintptr_t world_chr_man = 0;
    if (world_chr_man_slot_ != 0) {
        safe_read(world_chr_man_slot_, world_chr_man);
    }
    if (fe_man == 0) {
        return frame;
    }
    static ULONGLONG last_bar_log_ms = 0;
    const ULONGLONG now_ms = GetTickCount64();
    const bool log_bars = now_ms - last_bar_log_ms >= 5000;
    for (int i = 0; i < off.boss_bar_count; ++i) {
        uint64_t handle = 0;
        const uintptr_t entry =
            fe_man + off.csfeman_boss_bars + static_cast<uintptr_t>(i) * off.boss_bar_stride;
        if (!safe_read(entry + off.boss_bar_handle, handle) || !boss_handle_occupied(handle)) {
            continue;
        }
        int32_t display_id = 0;
        safe_read(entry + off.boss_bar_display_id, display_id);
        if (!boss_bar_ids_plausible(display_id, handle)) {
            if (log_bars) {
                last_bar_log_ms = now_ms;
                log_info(
                    "boss bar skipped handle=" + hex_u64(handle) + " display="
                    + hex_i32(display_id));
            }
            continue;
        }
        BossBarSlot slot;
        slot.handle = handle;
        int32_t chr_npc = 0;
        uintptr_t chr = 0;
        if (get_chr_ins_fn_ != 0 && world_chr_man != 0) {
            chr = call_get_chr_ins_from_handle(get_chr_ins_fn_, world_chr_man, handle);
            if (chr != 0) {
                safe_read(chr + off.chr_npc_param, chr_npc);
                uintptr_t modules = 0;
                if (safe_read(chr + off.chr_modules, modules) && modules != 0) {
                    uintptr_t stat = 0;
                    if (safe_read(modules + off.stat_module, stat) && stat != 0) {
                        safe_read(stat + off.stat_hp, slot.hp);
                        safe_read(stat + off.stat_hp_max, slot.hp_max);
                    }
                }
            }
        }
        slot.npc_param = prefer_mapped_npc_param(display_id, chr_npc);
        if (log_bars) {
            last_bar_log_ms = now_ms;
            log_info(
                "boss bar handle=" + hex_u64(handle) + " display=" + hex_i32(display_id)
                + " chr=" + hex_u64(chr) + " npc=" + std::to_string(slot.npc_param) + " name="
                + npc_display_name(slot.npc_param) + " hp=" + std::to_string(slot.hp) + "/"
                + std::to_string(slot.hp_max));
        }
        frame.bars.push_back(slot);
    }
    if (log_bars) {
        last_bar_log_ms = now_ms;
        log_info(
            "loadout R=" + std::to_string(frame.loadout.right_id)
            + " L=" + std::to_string(frame.loadout.left_id)
            + " A=" + std::to_string(frame.loadout.attack_id)
            + " arm=" + std::to_string(arm)
            + " T=" + std::to_string(frame.loadout.talisman_ids[0]) + ","
            + std::to_string(frame.loadout.talisman_ids[1]) + ","
            + std::to_string(frame.loadout.talisman_ids[2]) + ","
            + std::to_string(frame.loadout.talisman_ids[3])
            + " raw17-20,16,21=" + std::to_string(talisman_raw[0]) + ","
            + std::to_string(talisman_raw[1]) + "," + std::to_string(talisman_raw[2]) + ","
            + std::to_string(talisman_raw[3]) + "," + std::to_string(talisman_raw[4]) + ","
            + std::to_string(talisman_raw[5])
            + " G=" + hex_u64(talisman_gaitem[0]) + "," + hex_u64(talisman_gaitem[1]) + ","
            + hex_u64(talisman_gaitem[2]) + "," + hex_u64(talisman_gaitem[3]) + ","
            + hex_u64(talisman_gaitem[4]) + "," + hex_u64(talisman_gaitem[5]));
    }
    return frame;
}

LiveSnapshot GameDataReader::read_from_buffers(
    std::span<const uint8_t> game_data_man,
    std::span<const uint8_t> player_game_data,
    uint32_t map_id) {
    const auto& off = current_offsets();
    LiveSnapshot snap;
    if (const auto deaths = read_pod<uint32_t>(game_data_man, off.deaths)) {
        snap.deaths = *deaths;
    }
    if (const auto igt = read_pod<uint32_t>(game_data_man, off.igt_ms)) {
        snap.igt_ms = *igt;
    }
    if (const auto ng = read_pod<uint32_t>(game_data_man, off.ng_cycle)) {
        snap.ng_cycle = *ng;
    }
    if (const auto boss = read_pod<uint32_t>(game_data_man, off.boss_fight)) {
        snap.in_boss_fight = boss_flag_active(*boss);
    }
    if (const auto vigor = read_pod<uint32_t>(player_game_data, off.vigor)) {
        snap.stats.vigor = *vigor;
    }
    if (const auto mind = read_pod<uint32_t>(player_game_data, off.mind)) {
        snap.stats.mind = *mind;
    }
    if (const auto endurance = read_pod<uint32_t>(player_game_data, off.endurance)) {
        snap.stats.endurance = *endurance;
    }
    if (const auto strength = read_pod<uint32_t>(player_game_data, off.strength)) {
        snap.stats.strength = *strength;
    }
    if (const auto dexterity = read_pod<uint32_t>(player_game_data, off.dexterity)) {
        snap.stats.dexterity = *dexterity;
    }
    if (const auto intelligence = read_pod<uint32_t>(player_game_data, off.intelligence)) {
        snap.stats.intelligence = *intelligence;
    }
    if (const auto faith = read_pod<uint32_t>(player_game_data, off.faith)) {
        snap.stats.faith = *faith;
    }
    if (const auto arcane = read_pod<uint32_t>(player_game_data, off.arcane)) {
        snap.stats.arcane = *arcane;
    }
    if (const auto level = read_pod<uint32_t>(player_game_data, off.level)) {
        snap.level = *level;
    }
    if (const auto runes = read_pod<uint32_t>(player_game_data, off.runes)) {
        snap.runes = *runes;
    }
    if (const auto memory = read_pod<uint32_t>(player_game_data, off.rune_memory)) {
        snap.rune_memory = *memory;
    }
    if (const auto scadu = read_pod<uint8_t>(player_game_data, off.scadutree_blessing)) {
        if (*scadu <= 20) {
            snap.scadutree_blessing = *scadu;
        }
    }
    if (const auto ash = read_pod<uint8_t>(player_game_data, off.revered_ash)) {
        if (*ash <= 10) {
            snap.revered_ash = *ash;
        }
    }
    uint8_t flask_hp = 0;
    uint8_t flask_fp = 0;
    if (const auto hp = read_pod<uint8_t>(player_game_data, off.flask_hp)) {
        flask_hp = *hp;
    }
    if (const auto fp = read_pod<uint8_t>(player_game_data, off.flask_fp)) {
        flask_fp = *fp;
    }
    snap.flasks = flasks_from_allocation(flask_hp, flask_fp, 0);
    if (off.name + off.name_bytes <= player_game_data.size()) {
        snap.character_name = read_utf16le(player_game_data.data() + off.name, off.name_bytes);
    }
    snap.map_id = map_id;
    snap.location = location_from_map_id(map_id);
    snap.character_loaded = !snap.character_name.empty();
    return snap;
}

LiveSnapshot GameDataReader::read() const {
    LiveSnapshot snap;
    if (game_data_man_ == 0) {
        return snap;
    }
    const auto& off = current_offsets();

    uint32_t deaths = 0;
    uint32_t igt = 0;
    uint32_t ng_cycle = 0;
    if (!safe_read(game_data_man_ + off.deaths, deaths)) {
        return snap;
    }
    safe_read(game_data_man_ + off.igt_ms, igt);
    safe_read(game_data_man_ + off.ng_cycle, ng_cycle);

    uintptr_t player = 0;
    if (!safe_read(game_data_man_ + off.player_game_data, player) || player == 0) {
        return snap;
    }

    uint32_t level = 0;
    uint32_t runes = 0;
    uint32_t rune_memory = 0;
    uint8_t scadutree = 0;
    uint8_t revered_ash = 0;
    safe_read(player + off.level, level);
    safe_read(player + off.runes, runes);
    safe_read(player + off.rune_memory, rune_memory);
    if (safe_read(player + off.scadutree_blessing, scadutree) && scadutree > 20) {
        scadutree = 0;
    }
    if (safe_read(player + off.revered_ash, revered_ash) && revered_ash > 10) {
        revered_ash = 0;
    }

    uint8_t name_bytes[0x20]{};
    if (!safe_read_bytes(player + off.name, name_bytes, sizeof(name_bytes))) {
        return snap;
    }

    uint32_t map_id = 0;
    struct Vec3 {
        float x = 0;
        float y = 0;
        float z = 0;
    } pos;
    bool has_position = false;
    uintptr_t world_chr_man = 0;
    if (world_chr_man_slot_ != 0) {
        safe_read(world_chr_man_slot_, world_chr_man);
    }
    if (world_chr_man != 0) {
        uintptr_t player_ins = 0;
        if (!safe_read(world_chr_man + off.player_ins, player_ins) || player_ins == 0) {
            safe_read(world_chr_man + off.player_ins_legacy, player_ins);
        }
        if (player_ins == 0) {
            uintptr_t net = 0;
            if (safe_read(world_chr_man + off.player_ins_net, net) && net != 0) {
                safe_read(net, player_ins);
            }
        }
        if (player_ins != 0) {
            safe_read(player_ins + off.map_id, map_id);
            if (safe_read(player_ins + off.position, pos)) {
                // An exact (0,0,0) is the unloaded/placeholder position, not a
                // real player location; treat it as missing so callers keep
                // the last known coordinates instead.
                has_position = pos.x != 0.f || pos.y != 0.f || pos.z != 0.f;
            }
        }
    }

    uintptr_t game_man = 0;
    if (game_man_slot_ != 0) {
        safe_read(game_man_slot_, game_man);
    }
    uintptr_t event_flags = 0;
    if (event_flag_slot_ != 0) {
        safe_read(event_flag_slot_, event_flags);
    }

    snap.deaths = deaths;
    snap.igt_ms = igt;
    snap.ng_cycle = ng_cycle;
    snap.level = level;
    snap.runes = runes;
    snap.rune_memory = rune_memory;
    snap.scadutree_blessing = scadutree;
    snap.revered_ash = revered_ash;
    snap.stats = read_stats(player, off);
    snap.flasks = read_flasks(player, off);
    snap.last_grace = resolve_last_grace(game_man, off, snap.last_grace_id);
    snap.bosses_down = read_bosses_down(event_flags);
    snap.map_id = map_id;
    snap.x = pos.x;
    snap.y = pos.y;
    snap.z = pos.z;
    snap.has_position = has_position;
    snap.location = location_from_map_id(map_id);
    snap.character_name = read_utf16le(name_bytes, sizeof(name_bytes));
    snap.character_loaded = !snap.character_name.empty() && deaths < 1000000;
    return snap;
}

}  // namespace erstats
