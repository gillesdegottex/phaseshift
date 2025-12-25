// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#include <phaseshift/utils.h>

#include <iostream>

int phaseshift::music_note_to_semitone(const std::string& note) {
    assert(note.size() > 0);
    assert(note.size() <= 2);

    char note_key = note[0];
    char note_accidental = '\0';
    if (note.size() > 1) {
        note_accidental = note[1];
    }

    if (std::islower(note_key)) {
        note_key = std::toupper(note_key);
    }
    if (note.size() > 1 && std::isupper(note_accidental)) {
        note_accidental = std::tolower(note_accidental);
    }

    if (note_key == 'C' && note_accidental == '\0') {
        return 0;
    } else if ((note_key == 'C' && note_accidental == '#') || (note_key == 'D' && note_accidental == 'b')) {
        return 1;
    } else if (note_key == 'D' && note_accidental == '\0') {
        return 2;
    } else if ((note_key == 'D' && note_accidental == '#') || (note_key == 'E' && note_accidental == 'b')) {
        return 3;
    } else if (note_key == 'E' && note_accidental == '\0') {
        return 4;
    } else if (note_key == 'F' && note_accidental == '\0') {
        return 5;
    } else if ((note_key == 'F' && note_accidental == '#') || (note_key == 'G' && note_accidental == 'b')) {
        return 6;
    } else if (note_key == 'G' && note_accidental == '\0') {
        return 7;
    } else if ((note_key == 'G' && note_accidental == '#') || (note_key == 'A' && note_accidental == 'b')) {
        return 8;
    } else if (note_key == 'A' && note_accidental == '\0') {
        return 9;
    } else if ((note_key == 'A' && note_accidental == '#') || (note_key == 'B' && note_accidental == 'b')) {
        return 10;
    } else if (note_key == 'B' && note_accidental == '\0') {
        return 11;
    }

    assert(false && "phaseshift::music_note_to_semitone: Invalid note");

    return -1;  // -1 could be a semitone number, but it can't be returned by any other cases, so use it as an invalid value here.
}

int phaseshift::dev::check_compilation_options() {
    int ret = 0;

    #ifndef NDEBUG
    ret++;
    std::cerr << "WARNING: phaseshift library: C asserts are enabled. Maximum speed is not expected. (PHASESHIFT_DEV_ASSERT=ON)" << std::endl;
    #endif

    #ifdef PHASESHIFT_DEV_ASSERT
    ret++;
    std::cerr << "WARNING: phaseshift library: Removed -DNDEBUG from compilation flags in order to enable C asserts. Should be used for testing only. (PHASESHIFT_DEV_ASSERT=ON)" << std::endl;
    #endif

    #ifdef PHASESHIFT_DEV_PROFILING
    ret++;
    std::cerr << "WARNING: phaseshift library: Profiling is enabled. Extra time might be spent in measuring some function calls (ex. audio_block::proc(.)). (PHASESHIFT_DEV_PROFILING=ON)" << std::endl;
    #endif

    // Supress warnings
    (void)phaseshift::twopi;
    (void)phaseshift::oneover_twopi;
    (void)phaseshift::piovertwo;

    return ret;
}
