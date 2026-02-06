// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#include <phaseshift/utils.h>
#include <phaseshift/audio_block/sndfile.h>

#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <fstream>

TEST_CASE("libsndfile_read_write_amplify", "[libsndfile]") {
    #ifdef PHASESHIFT_SUPPORT_SNDFILE
        // Get the test source directory
        std::string test_source_dir = PHASESHIFT_TEST_SOURCE_DIR;
        
        // Input file path
        std::string input_file = test_source_dir + "/test_data/wav/arctic_b0518.wav";
        
        // Create output file path in build directory
        std::string output_file = std::string(PHASESHIFT_TEST_SOURCE_DIR) + "/build/test_data/totest/arctic_b0518_amplified.wav";
        
        // Create output directory if it doesn't exist
        std::filesystem::create_directories(std::filesystem::path(output_file).parent_path());
        
        std::cout << "Reading audio file: " << input_file << std::endl;
        std::cout << "Output file: " << output_file << std::endl;
        
        // Read file info
        float fs = phaseshift::sndfile_reader::get_fs(input_file);
        int nbchannels = phaseshift::sndfile_reader::get_nbchannels(input_file);
        int nbframes = phaseshift::sndfile_reader::get_nbframes(input_file);
        
        REQUIRE(fs > 0);
        REQUIRE(nbchannels > 0);
        REQUIRE(nbframes > 0);
        
        std::cout << "File info - Sample rate: " << fs << " Hz, Channels: " << nbchannels 
                  << ", Frames: " << nbframes << std::endl;
        
        // Open reader and read audio data using acbench ringbuffer
        acbench::ringbuffer<float> audio_buffer;
        audio_buffer.resize_allocation(nbframes * nbchannels);
        int frames_read = phaseshift::sndfile_reader::read(input_file, &audio_buffer);
        
        REQUIRE(frames_read > 0);
        std::cout << "Read " << frames_read << " frames (" << audio_buffer.size() << " samples)" << std::endl;
        
        // Calculate -6dB gain (10^(-6/20) ≈ 0.501)
        float gain = std::pow(10.0f, -6.0f / 20.0f);
        
        std::cout << "Applying -6dB gain: " << gain << std::endl;
        
        // Amplify the audio
        for (size_t i = 0; i < audio_buffer.size(); ++i) {
            audio_buffer[i] *= gain;
        }
        
        // Write amplified audio
        int frames_written = phaseshift::sndfile_writer::write(output_file, fs, audio_buffer);
        REQUIRE(frames_written > 0);
        std::cout << "Wrote " << frames_written << " frames to: " << output_file << std::endl;
        
        // Verify the output file was created
        std::ifstream output_check(output_file);
        REQUIRE(output_check.good());
        
        std::cout << "Test completed successfully!" << std::endl;
        
    #else
        SKIP("libsndfile support not enabled. Set PHASESHIFT_SUPPORT_SNDFILE=ON to run this test.");
    #endif
}

TEST_CASE("libsndfile_version_check", "[libsndfile]") {
    #ifdef PHASESHIFT_SUPPORT_SNDFILE
        std::string version = phaseshift::sndfile::version();
        REQUIRE(!version.empty());
        std::cout << "libsndfile version: " << version << std::endl;
    #else
        SKIP("libsndfile support not enabled. Set PHASESHIFT_SUPPORT_SNDFILE=ON to run this test.");
    #endif
}
