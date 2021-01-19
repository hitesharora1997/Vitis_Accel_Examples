/**
* Copyright (C) 2020 Xilinx, Inc
*
* Licensed under the Apache License, Version 2.0 (the "License"). You may
* not use this file except in compliance with the License. A copy of the
* License is located at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
* WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
* License for the specific language governing permissions and limitations
* under the License.
*/

extern "C" {
void krnl_cntr(unsigned int* a, // Vector a
               int reset        // Restart integer
               ) {
    static int cntr = 0;
    // reset execution counter to 0
    if (reset == 1) {
        cntr = 0;
    } else {
        cntr += 1;
        a[0] = cntr;
    }
}
}
