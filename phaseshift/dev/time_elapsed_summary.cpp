// Copyright (C) 2024 Gilles Degottex <gilles.degottex@gmail.com> All Rights Reserved.
//
// You may use, distribute and modify this code under the
// terms of the Apache 2.0 license.
// If you don't have a copy of this license, please visit:
//     https://github.com/gillesdegottex/phaseshift

#include <phaseshift/dev/time_elapsed_summary.h>

#ifdef PHASESHIFT_DEV_PROFILING

#include <iomanip>

void phaseshift::dev::time_elapsed_summary::print(std::ostream& out) const {
    out << "INFO: Audio blocks usages:" << std::endl;

    out << "    Initialize: " << initialize.stats(3) << std::endl;

    if (m_loop_tes.size() > 0) {
        out << "    Loop:" << std::endl;

        for (const auto& pair : m_loop_tes)
            out << "    " << std::setw(28) << pair.first << ": " << pair.second->stats(6) << std::endl;

        // Compute untracked time (assuming all added blocks by loop_add cover the Loop)
        {
            // The overall time taken to execute the full loop.
            double full_duration = loop.sum();

            // The sum of the time taken by each element running inside the loop.
            // Thus, ideally as close as possible to `full_duration`.
            double summed_duration = 0.0;
            bool max_reached = false;
            for (const auto& pair : m_loop_tes) {
                summed_duration += pair.second->sum();
                if (pair.second->size() == pair.second->size_max()) {
                    max_reached = true;
                }
            }

            if (full_duration >= summed_duration) {
                double duration_untracked = full_duration - summed_duration;
                out << "        Assuming all of the blocks listed above are in series (none being embedded in any other):" << std::endl;

                for (const auto& pair : m_loop_tes)
                    out << "    " << std::setw(28) << pair.first << ": " << acbench::to_string(100*pair.second->sum()/summed_duration, "%4.1f") << "%" << std::endl;

                if (max_reached) {
                    out << "    (maximum capacity of some of the element above has been reached, measure of the untracked time cannot be estimated)" << std::endl;
                } else {
                    out << "    \033[3m" << std::setw(28) << "untracked" << "\033[23m: " << acbench::to_string(100*duration_untracked/full_duration, "%4.1f") << "%" << std::endl;
                }
            }
        }

    } else {
        out << "    Loop:       " << loop.stats(3) << std::endl;
    }

    out << "    Finalize:   " << finalize.stats(3) << std::endl;
}

#endif
