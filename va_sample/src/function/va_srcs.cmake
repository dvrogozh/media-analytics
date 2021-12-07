#
# Copyright (c) 2019, Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#

set(VA_SOURCES
    ${VA_SOURCES}
    )

set(DECODE_SOURCES
    ${DECODE_SOURCES}
    ${CMAKE_CURRENT_LIST_DIR}/MfxSessionMgr.cpp
    ${CMAKE_CURRENT_LIST_DIR}/CropThreadBlock.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Statistics.cpp
    )

set(DECODE_SOURCES2
    ${DECODE_SOURCES}
    ${CMAKE_CURRENT_LIST_DIR}/DecodeThreadBlock2.cpp
)
set(DECODE_SOURCES
    ${DECODE_SOURCES}
    ${CMAKE_CURRENT_LIST_DIR}/DecodeThreadBlock.cpp
)

set(ENCODE_SOURCES
    ${ENCODE_SOURCES}
    ${CMAKE_CURRENT_LIST_DIR}/EncodeThreadBlock.cpp
    )

set(INFER_SOURCES
    ${INFER_SOURCES}
    ${CMAKE_CURRENT_LIST_DIR}/InferenceThreadBlock.cpp
    )

set(DISPLAY_SOURCES
    ${DISPLAY_SOURCES}
    ${CMAKE_CURRENT_LIST_DIR}/DisplayThreadBlock.cpp
    )

include_directories(${CMAKE_CURRENT_LIST_DIR})
