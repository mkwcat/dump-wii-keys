// FatFS.cpp - FatFS Disk I/O Layer
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0-only

#include "FATDiskIO.h"
#include "FAT.h"
#include "SDCard.hpp"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <limits>
#include <tuple>

DSTATUS disk_status([[maybe_unused]] BYTE pdrv)
{
    if (SDCard::Slot0().IsInserted()) {
        return 0;
    }

    return STA_NODISK;
}

DSTATUS disk_initialize([[maybe_unused]] BYTE pdrv)
{
    if (SDCard::Slot0().Init()) {
        return 0;
    }

    return STA_NOINIT;
}

DRESULT
disk_read([[maybe_unused]] BYTE pdrv, BYTE* buff, LBA_t sector, UINT count)
{
    if (SDCard::Slot0().ReadSectors(
            static_cast<u32>(sector), static_cast<u32>(count),
            reinterpret_cast<void*>(buff)
        )) {
        return RES_OK;
    }

    return RES_ERROR;
}

DRESULT disk_write(
    [[maybe_unused]] BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count
)
{
    if (SDCard::Slot0().WriteSectors(
            static_cast<u32>(sector), static_cast<u32>(count),
            reinterpret_cast<const void*>(buff)
        )) {
        return RES_OK;
    }

    return RES_ERROR;
}

DRESULT disk_ioctl([[maybe_unused]] BYTE pdrv, BYTE cmd, void* buff)
{
    switch (cmd) {
    case CTRL_SYNC:
        return RES_OK;

    case GET_SECTOR_SIZE:
        // Sectors are always 512 bytes
        *reinterpret_cast<WORD*>(buff) = 512;
        return RES_OK;

    default:
        std::printf("FATDiskIO: Unknown command: %d", cmd);
        return RES_PARERR;
    }
}

// From https://howardhinnant.github.io/date_algorithms.html#civil_from_days
template <class Int>
constexpr std::tuple<Int, unsigned, unsigned> civil_from_days(Int z) noexcept
{
    static_assert(
        std::numeric_limits<unsigned>::digits >= 18,
        "This algorithm has not been ported to a 16 bit unsigned integer"
    );
    static_assert(
        std::numeric_limits<Int>::digits >= 20,
        "This algorithm has not been ported to a 16 bit signed integer"
    );
    z += 719468;
    const Int era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097); // [0, 146096]
    const unsigned yoe =
        (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; // [0, 399]
    const Int y = static_cast<Int>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100); // [0, 365]
    const unsigned mp = (5 * doy + 2) / 153; // [0, 11]
    const unsigned d = doy - (153 * mp + 2) / 5 + 1; // [1, 31]
    const unsigned m = mp < 10 ? mp + 3 : mp - 9; // [1, 12]
    return std::tuple<Int, unsigned, unsigned>(y + (m <= 2), m, d);
}

DWORD get_fattime()
{
    union {
        struct {
            DWORD year : 7;
            DWORD month : 4;
            DWORD day : 5;
            DWORD hour : 5;
            DWORD minute : 6;
            DWORD second : 5;
        };

        DWORD value;
    } fattime;

    u64 time64 = std::time(nullptr);
    u32 time32 = time64;

    int days = time64 / 86400;
    auto date = civil_from_days<DWORD>(days);

    fattime = {
        .year = std::get<0>(date) - 1980,
        .month = std::get<1>(date),
        .day = std::get<2>(date),
        .hour = (time32 / 60 / 60) % 24,
        .minute = (time32 / 60) % 60,
        .second = time32 % 60,
    };

    return fattime.value;
}


void* ff_memalloc(UINT msize)
{
    return new u8[msize];
}

void ff_memfree(void* mblock)
{
    u8* data = reinterpret_cast<u8*>(mblock);
    delete[] data;
}

#if 0
int ff_cre_syncobj([[maybe_unused]] BYTE vol, FF_SYNC_t* sobj)
{
    Mutex* mutex = new Mutex;
    *sobj = reinterpret_cast<FF_SYNC_t>(mutex);
    return 1;
}

// Lock sync object
int ff_req_grant(FF_SYNC_t sobj)
{
    Mutex* mutex = reinterpret_cast<Mutex*>(sobj);
    mutex->Lock();
    return 1;
}

void ff_rel_grant(FF_SYNC_t sobj)
{
    Mutex* mutex = reinterpret_cast<Mutex*>(sobj);
    mutex->Unlock();
}

// Delete a sync object
int ff_del_syncobj(FF_SYNC_t sobj)
{
    Mutex* mutex = reinterpret_cast<Mutex*>(sobj);
    delete mutex;
    return 1;
}
#endif

void* ff_memcpy(void* dst, const void* src, UINT len)
{
    std::memcpy(dst, src, len);
    return dst;
}
