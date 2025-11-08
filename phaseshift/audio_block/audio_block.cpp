// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#include <phaseshift/audio_block/audio_block.h>

#include <string>
#include <atomic>
#include <functional>


struct audio_block_builder_multithread_sync_data {
    std::atomic<bool> go = false;
    std::atomic<int> nb_ready = 0;
};

void audio_block_builder_test_multithread_thread(std::function<void(void)> fn, audio_block_builder_multithread_sync_data* psync_data) {
    // Wait for all threads to be ready before starting init and proc
    psync_data->nb_ready++;
    while ( !psync_data->go ) {
        std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(0.001*1e6)));
    }

    fn();
}

void phaseshift::dev::audio_block_builder_test(std::function<void(void)> fn, int nb_threads) {

    audio_block_builder_multithread_sync_data sync_data;

    std::vector<std::thread> threads;
    for (int nt=0; nt < nb_threads; ++nt) {
        threads.push_back(std::thread(audio_block_builder_test_multithread_thread, fn, &sync_data));

        // Wait for the new thread to start before starting the next one
        // to be sure that
        while ( !(threads[nt].joinable()) ) {
            std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(0.001*1e6)));
        }
    }

    // Wait for all to get ready...
    while ( sync_data.nb_ready < int(threads.size()) ) {
        std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(0.001*1e6)));
    }
    sync_data.go = true;

    // Wait for all to finish
    for (auto&& thread : threads) {
        thread.join();
    }
}
