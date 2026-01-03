// SDCard.hpp
//
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <gctypes.h>
#include <utility>

class SDCard
{
public:
    SDCard(SDCard&) = delete;

    SDCard();
    ~SDCard();

private:
    bool ResetCard();
    bool GetStatus(u32* status);
    bool ReadHCR(u8 reg, u8 size, u32* val);
    bool WriteHCR(u8 reg, u8 size, u32 val);
    bool SetClock(u32 clock);
    bool SendCommand(
        u32 command, u32 commandType, u32 responseType, u32 arg, u32 blockCount,
        u32 blockSize, void* buffer, u32* response
    );
    bool Enable4BitBus();
    bool Select();
    bool Deselect();
    bool SetCardBlockLength(u32 blockLength);
    bool EnableCard4BitBus();
    bool TransferAligned(
        bool isWrite, u32 firstSector, u32 sectorCount, void* buffer
    );
    bool Transfer(bool isWrite, u32 firstSector, u32 sectorCount, void* buffer);

public:
    /**
     * Get SD Card instance.
     */
    static SDCard& Slot0();

    /**
     * Is an SD Card inserted.
     */
    bool IsInserted();

    /**
     * Wait for SD Card insert state.
     */
    bool WaitForInsert(bool state = true);

    /*
     * Cancels the WaitForInsert call.
     */
    void CancelWaitForInsert(bool state = true);

    /**
     * Initialize the SD Card.
     */
    bool Init();

    /**
     * Get the sector size of the inserted SD Card.
     */
    u32 GetSectorSize();

    /**
     * Read sectors from the inserted SD Card.
     */
    bool ReadSectors(u32 firstSector, u32 sectorCount, void* buffer);

    /**
     * Write sectors to the inserted SD Card.
     */
    bool WriteSectors(u32 firstSector, u32 sectorCount, const void* buffer);

private:
    bool m_ok = false;

public:
    static constexpr u32 SECTOR_SIZE = 512;
    static constexpr u32 TMP_SECTOR_COUNT = 8;
    static constexpr u32 TMP_BUFFER_SIZE = TMP_SECTOR_COUNT * SECTOR_SIZE;

private:
    void* m_tmpBuffer = nullptr;
    s32 m_fd = -1;
    u16 m_rca = 0;
    bool m_isSdhc = false;
};
