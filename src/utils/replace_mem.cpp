/*  Copyright 2023 Pretendo Network contributors <pretendo.network>
    Copyright 2023 Ash Logan <ash@heyquark.com>
    Copyright 2019 Maschell

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "replace_mem.h"
#include "utils/logger.h"

#include <kernel/kernel.h>
#include <coreinit/memorymap.h>
#include <algorithm>
#include <coreinit/cache.h>

bool replace(uint32_t start, uint32_t size, const char *original_val, size_t original_val_sz, const char *new_val,
             size_t new_val_sz) {
    for (uint32_t addr = start; addr < start + size - original_val_sz; addr++) {
        int ret = memcmp(original_val, (void *) addr, original_val_sz);
        if (ret == 0) {
            DEBUG_FUNCTION_LINE_VERBOSE("found str @%08x: %s", addr, (const char *) addr);
            KernelCopyData(OSEffectiveToPhysical(addr), OSEffectiveToPhysical((uint32_t) new_val), new_val_sz);
            DEBUG_FUNCTION_LINE_VERBOSE("new str   @%08x: %s", addr, (const char *) addr);
            return true;
        }
    }

    return false;
}

void replaceBulk(uint32_t start, uint32_t size, std::span<const replacement> replacements) {
    // work out the biggest input replacement
    auto max_sz = std::max_element(replacements.begin(), replacements.end(), [](auto &a, auto &b) {
        return a.orig.size_bytes() < b.orig.size_bytes();
    })->orig.size_bytes();

    int counts[replacements.size()];
    for (auto &c: counts) {
        c = 0;
    }

    for (uint32_t addr = start; addr < start + size - max_sz; addr++) {
        for (int i = 0; i < (int) replacements.size(); i++) {
            const auto &replacement = replacements[i];

            int ret = memcmp((void *) addr, replacement.orig.data(), replacement.orig.size_bytes());
            if (ret == 0) {
                KernelCopyData(
                        OSEffectiveToPhysical(addr),
                        OSEffectiveToPhysical((uint32_t) replacement.repl.data()),
                        replacement.repl.size_bytes()
                );
                counts[i]++;
                break; // don't check the other replacements
            }
        }
    }
#ifdef DEBUG
    for (auto c: counts) {
        DEBUG_FUNCTION_LINE("replaced %d times", c);
    }
#endif
}

template <typename U>
    requires std::integral<U>
bool replace_unsigned(U *addr, U original_value, U new_value) {
    if (*addr != original_value) return false;

    KernelCopyData(
            OSEffectiveToPhysical((uint32_t) addr),
            OSEffectiveToPhysical((uint32_t) &new_value),
            sizeof(new_value)
    );
    DCFlushRange(addr, sizeof(new_value));

    DEBUG_FUNCTION_LINE_VERBOSE("%08x is now %08x", inst, *inst);
    return *addr == new_value;
}
template bool replace_unsigned<uint64_t>(uint64_t *, uint64_t, uint64_t);
template bool replace_unsigned<uint32_t>(uint32_t *, uint32_t, uint32_t);
template bool replace_unsigned<uint16_t>(uint16_t *, uint16_t, uint16_t);
template bool replace_unsigned<uint8_t>(uint8_t *, uint8_t, uint8_t);

bool replace_instruction(uint32_t *inst, uint32_t original_value, uint32_t new_value) {
    bool res = replace_unsigned<uint32_t>(inst, original_value, new_value);
    if (!res) return res;

    ICInvalidateRange(inst, sizeof(new_value));
    return true;
}
