![workflow](https://github.com/gillesdegottex/phaseshift/actions/workflows/test-multi-platform.yml/badge.svg)

The goal of phaseshift library is to provide basic construction blocks to build technologies about audio signal processing.
It will stay low-level (no compilation of plugins, graphical interface, etc. that's not the goal here).

For example:
* `audio_block/ol.h` provides a class to split an audio signal into frames.
* `audio_block/ola.h` provides a class to split an audio signal into frames and reconstruct a new one based on the processed frames.


## Properties
* No dynamic memory allocation
* No memory drift: Memory usage has to be constant, not linear to the length the audio signal.
* Constant latency, and known a priori.
* Neutral params -> Still perfect resynthesis
* No implicit conversions
* Language: C++17
* Multi-platform

These properties should be enforced by the API as much as possible. I.e. there should be no means to use the library that would not satisfy any of these properties.

`dev` namespace has exception to those properties.
Thus, any code used from the namespace `phaseshift::dev` is not meant for production.

## Compilation

If you're using conda, start with:

    $ export PKG_CONFIG_PATH=$CONDA_PREFIX/lib/pkgconfig/

Then:

    $ mkdir build
    $ cd build
    $ cmake .. -DPHASESHIFT_DEV_TESTS=ON -DPHASESHIFT_SUPPORT_SNDFILE=ON
    $ make
    $ ctest


## Builder usage

The construction of processing blocks follows a builder design pattern.
You first have to instanciate a builder for the audio block you want to create:

    phaseshift::ola_builder b;

Then, you set the parameters of `b` that will be constant over the full use of the audio processing block to come.

    b.set_timestep(sr*0.05);
    b.set_winlen(sr*0.020);

Then you build the audio processing block:

    phaseshift::ola p = b.build();

And finally you can use `p`:

    p.proc(input, &output);
    p.flush(&output);

In doing so, we ensure through the API, that it is not possible to change some parameters of `phaseshift::ola` that are supposed to stay constant through all the processing.


## Conventions
### `int` vs. `size_t`
Why choosing `int` instead of `size_t`:
https://eigen.tuxfamily.narkive.com/ZhXOa82S/signed-or-unsigned-indexing

### Coding style
cpplint from Google

## Attributions

* Test files of speech recordings in the directory `test_data/wav` are taken from the Carnegie Mellon University ARCTIC speech synthesis databases:
    * Website: http://festvox.org/cmu_arctic
    * Publication: https://www.isca-archive.org/ssw_2004/kominek04b_ssw.pdf
    * License text: test_data/wav/arctic_COPYING

* Eigen
    * Website: https://gitlab.com/libeigen/eigen.git
    * Authors: By a good bunch of contributors https://gitlab.com/libeigen/eigen/-/project_members
    * License:*Mozilla Public License Version 2.0* https://gitlab.com/libeigen/eigen/-/blob/master/LICENSE

* libsndfile
    * Website: https://github.com/libsndfile/libsndfile
    * Authors: By Erik de Castro Lopo and a solid amount of contributors https://github.com/libsndfile/libsndfile/graphs/contributors
    * License: *Gnu Lesser General Public License* https://github.com/libsndfile/libsndfile/blob/master/COPYING> (thus, dynamic linking is compulsory!)

* Catch2
    * Website: https://github.com/catchorg/Catch2.git
    * Authors: By a great amount of contributors https://github.com/catchorg/Catch2/graphs/contributors
    * License: *Boost Software License - Version 1.0* https://github.com/catchorg/Catch2/blob/devel/LICENSE.txt

* nanobind
    * Website: https://github.com/wjakob/nanobind
    * Authors: Wenzel Jakob and a solid band of code conjurers: https://github.com/wjakob/nanobind/graphs/contributors
    * License: *BSD 3-Clause License* https://github.com/wjakob/nanobind/blob/master/LICENSE.txt

## Legal

Copyright (c) 2024-2025 Gilles Degottex gilles.degottex@gmail.com

Licensed under Apache 2.0 License.
See the LICENSE.txt file at the root of this repository.

### Disclaimer

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. ALSO, THE COPYRIGHT HOLDERS AND CONTRIBUTORS DO NOT TAKE ANY LEGAL RESPONSIBILITY REGARDING THE IMPLEMENTATIONS OF THE PROCESSING TECHNIQUES OR ALGORITHMS (E.G. CONSEQUENCES OF BUGS OR ERRONEOUS IMPLEMENTATIONS).

