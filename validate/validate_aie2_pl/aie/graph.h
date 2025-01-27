/*
 * Copyright 2022 Xilinx, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <adf.h>
#include "aie/kernels/include.h"
#include "kernels.h"

using namespace adf;

class simpleGraph : public graph {
   private:
    kernel first;
    pktcontrol ctrl_first;

   public:
    input_port in0;
    output_port out0;
    input_port ctrl_in;

    simpleGraph() {
        first = kernel::create(simple);
        source(first) = "aie/kernels/kernels.cc";
        runtime<ratio>(first) = 0.9;

        ctrl_first = pktcontrol::create(); // true => Response port also

        // plctrl = kernel::create(pl_controller);
        // source(plctrl) = "aie/kernels/pl_controller.cpp";
        // fabric<pl>(plctrl);

        // Kernel connections
        connect<window<128> > n0(in0, first.in[0]);
        connect<window<128> > n1(first.out[0], out0);
        // Control connections
        connect<pktstream, pktstream> n3(ctrl_in, ctrl_first.in[0]);

        // Constrain AIE kernels to the same row, otherwise tiles may be flipped.
        // Constrain buffers such that addressmap is identical.
        location<kernel>(first) = tile(AIE_CORE_COL, AIE_CORE_ROW);
        location<buffer>(first.in[0]) = {address(AIE_CORE_COL, AIE_CORE_ROW, 0x0),
                                         address(AIE_CORE_COL, AIE_CORE_ROW, 0x4000)}; // {0,16} KB
        location<buffer>(first.out[0]) = {address(AIE_CORE_COL, AIE_CORE_ROW, 0x400),
                                          address(AIE_CORE_COL, AIE_CORE_ROW, 0x4400)}; // {1,17} KB
        location<stack>(first) = location<kernel>(first);

        location<interconnect>(ctrl_first) = location<kernel>(first);
    }
};
