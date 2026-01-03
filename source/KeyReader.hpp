#pragma once

class KeyReader {
public:
    enum class State {
        SEARCHING_DISK,
        WRITING,
        FINISHED,

        NO_DISK,
        DISK_ERROR,
        WRITE_ERROR,
        FATAL_ERROR,
        SHUTTING_DOWN,
    };

    static State GetState();

    static void StartThread();
    static void ShutdownAsync();
    static void Shutdown();
};