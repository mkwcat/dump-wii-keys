#include "KeyReader.hpp"
#include "FAT.h"
#include "IOS.hpp"
#include "SDCard.hpp"
#include "Util.hpp"
#include <cstdio>
#include <ogc/lwp.h>
#include <unistd.h>

static KeyReader::State s_state = KeyReader::State::SEARCHING_DISK;
static bool s_shutdown = false;
static FATFS s_fat;
static bool s_fatMounted = false;

KeyReader::State KeyReader::GetState()
{
    return s_state;
}

struct BackupMiiKeys {
    /* 0x000 */ char magic[0x100];
    /* 0x100 */ u32 otp[0x80 / 4];
    /* 0x180 */ u32 padding1[0x80 / 4];
    /* 0x200 */ u16 seeprom[0x100 / 2];
    /* 0x300 */ u8 padding2[0x100];
};

static_assert(sizeof(BackupMiiKeys) == 0x400);

bool DumpOTP(BackupMiiKeys* keys)
{
    volatile u32* const HW_EFUSEADDR =
        reinterpret_cast<volatile u32*>(0xCD8001EC);
    volatile u32* const HW_EFUSEDATA =
        reinterpret_cast<volatile u32*>(0xCD8001F0);

    for (u32 i = 0; i < 0x80 / 4; i++) {
        *HW_EFUSEADDR = (1 << 31) | i;
        asm volatile("eieio");
        keys->otp[i] = *HW_EFUSEDATA;
        asm volatile("eieio");
    }

    return true;
}

extern "C" void udelay(int us);

static inline void PPCEieio()
{
    asm volatile("eieio");
}

bool DumpSEEPROMRvl(BackupMiiKeys* keys)
{
    volatile u32* const HW_GPIOPPCOUT =
        reinterpret_cast<volatile u32*>(0xCD8000C0);
    volatile u32* const HW_GPIOPPCDIR =
        reinterpret_cast<volatile u32*>(0xCD8000C4);
    volatile u32* const HW_GPIOPPCIN =
        reinterpret_cast<volatile u32*>(0xCD8000C8);
    volatile u32* const HW_GPIOPPCINTEN =
        reinterpret_cast<volatile u32*>(0xCD8000D4);

    volatile u32* const HW_GPIOIOPPPCOWNER =
        reinterpret_cast<volatile u32*>(0xCD8000FC);

    static constexpr u32 EEP_CS = 0x0400;
    static constexpr u32 EEP_CLK = 0x0800;
    static constexpr u32 EEP_MOSI = 0x1000;
    static constexpr u32 EEP_MISO = 0x2000;

    // Set defaults
    *HW_GPIOPPCOUT &= ~(EEP_CS | EEP_CLK | EEP_MOSI | EEP_MISO);
    *HW_GPIOPPCDIR |= EEP_CS | EEP_CLK | EEP_MOSI;
    *HW_GPIOPPCDIR &= ~EEP_MISO;
    *HW_GPIOPPCINTEN &= ~(EEP_CS | EEP_CLK | EEP_MOSI | EEP_MISO);

    u32 gpioOwnerSave = *HW_GPIOIOPPPCOWNER;

    PPCEieio();

    // Switch SEEPROM owner to PPC
    *HW_GPIOIOPPPCOWNER =
        gpioOwnerSave | EEP_CS | EEP_CLK | EEP_MOSI | EEP_MISO;

    udelay(5);

    for (int i = 0; i < 0x100 / 2; i++) {
        *HW_GPIOPPCOUT |= EEP_CS;
        udelay(5);

        // Send read command
        u16 sendData = 0x600 | (i & 0x7F);
        for (int j = 10; j >= 0; j--) {
            if (sendData & (1 << j)) {
                *HW_GPIOPPCOUT |= EEP_MOSI;
            } else {
                *HW_GPIOPPCOUT &= ~EEP_MOSI;
            }
            udelay(5);

            *HW_GPIOPPCOUT |= EEP_CLK;
            udelay(5);
            *HW_GPIOPPCOUT &= ~EEP_CLK;
            udelay(5);
        }

        // Read response
        u16 recvData = 0;
        for (int j = 15; j >= 0; j--) {
            *HW_GPIOPPCOUT |= EEP_CLK;
            udelay(5);
            *HW_GPIOPPCOUT &= ~EEP_CLK;
            udelay(5);

            recvData |= ((*HW_GPIOPPCIN & EEP_MISO) ? 1 : 0) << j;
        }

        keys->seeprom[i] = recvData;

        *HW_GPIOPPCOUT &= ~EEP_CS;
        udelay(5);
    }

    // Reset SEEPROM
    *HW_GPIOPPCOUT &= ~(EEP_CS | EEP_CLK | EEP_MOSI);
    udelay(5);

    // Restore GPIO owner
    u32 gpioOwnerRestore = *HW_GPIOIOPPPCOWNER;
    gpioOwnerRestore &= ~(EEP_CS | EEP_CLK | EEP_MOSI | EEP_MISO);
    *HW_GPIOIOPPPCOWNER =
        gpioOwnerRestore |
        (gpioOwnerSave & (EEP_CS | EEP_CLK | EEP_MOSI | EEP_MISO));
    PPCEieio();

    return true;
}

bool DumpSEEPROMCafe(BackupMiiKeys* keys)
{
    // On Wii U the SEEPROM is emulated by the IOS kernel, with a copy of the
    // data stored in SRAM.
    const u32* const SEEPROM_DATA = reinterpret_cast<u32*>(0xCD4E7F00);

    for (int i = 0; i < 0x100 / 4; i++) {
        u32 data = SEEPROM_DATA[i];
        keys->seeprom[i] = data >> 16;
        keys->seeprom[i + 1] = data & 0xFFFF;
    }

    return true;
}

BackupMiiKeys DumpKeys()
{
    BackupMiiKeys keys alignas(32) = {};

    if (IOS::IsDolphin()) {
        s32 fd = IOS_Open("/keys.bin", IPC_OPEN_READ);
        if (fd >= 0) {
            s32 ret = IOS_Read(fd, &keys, sizeof(keys));
            IOS_Close(fd);
            if (ret == sizeof(keys)) {
                return keys;
            }
        }
        std::printf("Failed to read keys.bin\n");
        return {};
    }

    if (!DumpOTP(&keys)) {
        std::printf("Failed to dump OTP\n");
        return {};
    }

    if (!IOS::IsCafe()) {
        if (!DumpSEEPROMRvl(&keys)) {
            std::printf("Failed to dump serial EEPROM\n");
            return {};
        }
    } else {
        if (!DumpSEEPROMCafe(&keys)) {
            std::printf("Failed to dump fake serial EEPROM\n");
            return {};
        }
    }

    u32 deviceId = keys.otp[9];
    if (deviceId == 0) {
        std::printf("Device ID in OTP is blank\n");
        return {};
    }

    std::printf("Device ID: %08X\n", deviceId);

    std::snprintf(
        keys.magic, sizeof(keys.magic), "BackupMii v1, ConsoleID: %08x\n",
        deviceId
    );

    return keys;
}

static bool s_waitForInsert = false;
static bool s_waitForInsertType = false;

static void* KeyDumpProcess(void* arg)
{
#define CHECK_SHUTDOWN()                                                       \
    if (s_shutdown) {                                                          \
        s_state = KeyReader::State::SHUTTING_DOWN;                             \
        return nullptr;                                                        \
    }

    while (!s_shutdown) {
        if (s_fatMounted) {
            s_fatMounted = false;
            f_unmount("0:");
        }

        if (s_state == KeyReader::State::DISK_ERROR &&
            SDCard::Slot0().IsInserted()) {

            s_waitForInsertType = false;
            s_waitForInsert = true;
            if (!SDCard::Slot0().WaitForInsert(false)) {
                s_waitForInsert = false;
                CHECK_SHUTDOWN();

                s_state = KeyReader::State::FATAL_ERROR;
                return nullptr;
            }
            s_waitForInsert = false;

            CHECK_SHUTDOWN();
        }

        s_state = KeyReader::State::SEARCHING_DISK;

        while (!SDCard::Slot0().IsInserted()) {
            CHECK_SHUTDOWN();

            s_state = KeyReader::State::NO_DISK;

            s_waitForInsertType = false;
            s_waitForInsert = true;
            if (!SDCard::Slot0().WaitForInsert(true)) {
                s_waitForInsert = false;
                CHECK_SHUTDOWN();

                s_state = KeyReader::State::FATAL_ERROR;
                return nullptr;
            }
            s_waitForInsert = false;
        }

        CHECK_SHUTDOWN();

        s_state = KeyReader::State::WRITING;

        if (!SDCard::Slot0().Init()) {
            s_state = KeyReader::State::DISK_ERROR;
            continue;
        }

        CHECK_SHUTDOWN();

        auto fret = f_mount(&s_fat, "0:", 0);
        if (fret != FR_OK) {
            std::printf("Failed to mount SD Card: %i\n", fret);
            s_state = KeyReader::State::DISK_ERROR;
            continue;
        }
        s_fatMounted = true;

        CHECK_SHUTDOWN();

        BackupMiiKeys keys alignas(32) = DumpKeys();
        if (keys.magic[0] == '\0') {
            s_state = KeyReader::State::FATAL_ERROR;
            continue;
        }

        CHECK_SHUTDOWN();

        FIL fil;
        fret =
            f_open(&fil, "0:/keys.bin", FA_CREATE_ALWAYS | FA_WRITE | FA_READ);
        if (fret != FR_OK) {
            std::printf("Failed to open keys.bin: %i\n", fret);
            s_state = KeyReader::State::WRITE_ERROR;
            continue;
        }

        u32 bytesWritten;
        fret = f_write(&fil, &keys, sizeof(keys), &bytesWritten);
        if (fret != FR_OK || bytesWritten != sizeof(keys)) {
            std::printf("Failed to write keys.bin: %i\n", fret);
            s_state = KeyReader::State::WRITE_ERROR;
            f_close(&fil);
            continue;
        }

        fret = f_close(&fil);
        if (fret != FR_OK) {
            std::printf("Failed to close keys.bin: %i\n", fret);
            s_state = KeyReader::State::WRITE_ERROR;
            continue;
        }

        std::printf("Keys written to SD:/keys.bin\n");

        s_state = KeyReader::State::FINISHED;

        for (int i = 0; i < 50; i++) {
            CHECK_SHUTDOWN();

            usleep(100000);
        }

        KeyReader::ShutdownAsync();
    }

    s_state = KeyReader::State::SHUTTING_DOWN;
    return nullptr;
#undef CHECK_SHUTDOWN
}

static lwp_t s_thread;
static bool s_threadStarted = false;
static lwp_t s_cancelThread;

void KeyReader::StartThread()
{
    if (s_threadStarted) {
        return;
    }

    s_threadStarted = true;
    LWP_CreateThread(&s_thread, KeyDumpProcess, nullptr, nullptr, 0x40000, 20);
}

static void* CancelWaitForInsert(void*)
{
    if (s_waitForInsert) {
        SDCard::Slot0().CancelWaitForInsert(s_waitForInsertType);
    }

    return nullptr;
}

void KeyReader::ShutdownAsync()
{
    if (!s_shutdown) {
        s_shutdown = true;
        if (s_waitForInsert) {
            // Cannot call on this thread
            LWP_CreateThread(
                &s_cancelThread, CancelWaitForInsert, nullptr, nullptr, 0x8000,
                120
            );
        }
    }
}

void KeyReader::Shutdown()
{
    if (!s_shutdown) {
        s_shutdown = true;
        CancelWaitForInsert(nullptr);
        if (s_threadStarted) {
            LWP_JoinThread(s_thread, nullptr);
        }
    }
}