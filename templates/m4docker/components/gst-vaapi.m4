dnl BSD 3-Clause License
dnl
dnl Copyright (c) 2020, Intel Corporation
dnl All rights reserved.
dnl
dnl Redistribution and use in source and binary forms, with or without
dnl modification, are permitted provided that the following conditions are met:
dnl
dnl * Redistributions of source code must retain the above copyright notice, this
dnl   list of conditions and the following disclaimer.
dnl
dnl * Redistributions in binary form must reproduce the above copyright notice,
dnl   this list of conditions and the following disclaimer in the documentation
dnl   and/or other materials provided with the distribution.
dnl
dnl * Neither the name of the copyright holder nor the names of its
dnl   contributors may be used to endorse or promote products derived from
dnl   this software without specific prior written permission.
dnl
dnl THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
dnl AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
dnl IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
dnl DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
dnl FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
dnl DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
dnl SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
dnl CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
dnl OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
dnl OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
dnl
include(begin.m4)

DECLARE(`GST_VAAPI_WITH_DRM',yes)
DECLARE(`GST_VAAPI_WITH_X11',no)
DECLARE(`GST_VAAPI_WITH_GLX',no)
DECLARE(`GST_VAAPI_WITH_WAYLAND',no)
DECLARE(`GST_VAAPI_WITH_EGL',no)
DECLARE(`GST_VAAPI_WITH_GVA_PATCH',false)

ifelse(GST_VAAPI_WITH_GVA_PATCH,true,dnl
`DECLARE(`GST_VAAPI_DRM_CHOOSE_PATCH_URL',dnl
dnl Commits from: https://gitlab.freedesktop.org/gstreamer/gstreamer-vaapi/-/merge_requests/409
https://gitlab.freedesktop.org/gstreamer/gstreamer-vaapi/-/commit/85284f4aacc9cfc059c59aa033166fedc247e117.patch dnl
https://gitlab.freedesktop.org/gstreamer/gstreamer-vaapi/-/commit/4ff4bcd725290c8f1a52e791496f164c3090ecaa.patch dnl
dnl Commits from: https://gitlab.freedesktop.org/gstreamer/gstreamer-vaapi/-/merge_requests/422
https://gitlab.freedesktop.org/gstreamer/gstreamer-vaapi/-/commit/0193751ce844776ef1823f99aff053ee423f7004.patch)'
`DECLARE(`GST_VAAPI_GVA_PATCH_VER',v1.5.2)'
`DECLARE(`GST_VAAPI_GVA_PATCH_URL',https://raw.githubusercontent.com/openvinotoolkit/dlstreamer_gst/${GSTREAMER_VAAPI_PATCH_VER}/patches/gstreamer-vaapi/vasurface_qdata.patch)'
`DECLARE(`GST_VAAPI_PATCH_URL',GST_VAAPI_DRM_CHOOSE_PATCH_URL GST_VAAPI_GVA_PATCH_URL)'
)

ifdef(`ENABLE_INTEL_GFX_REPO',`dnl
pushdef(`LIBVA_DEV_BUILD_DEP',`ifelse(OS_NAME,ubuntu,libva-dev)')
pushdef(`LIBVA_INSTALL_DEP',`ifelse(OS_NAME,ubuntu,libva2 libva-drm2 libva-x11-2 libva-wayland2)')
',`dnl
pushdef(`LIBVA_DEV_BUILD_DEP',)
pushdef(`LIBVA_INSTALL_DEP',)
include(libva2.m4)
')

include(gst-plugins-bad.m4)

ifelse(OS_NAME,ubuntu,dnl
`define(`GSTVAAPI_BUILD_DEPS',ca-certificates meson tar g++ wget pkg-config libdrm-dev libglib2.0-dev libudev-dev flex bison LIBVA_DEV_BUILD_DEP)'
`define(`GSTVAAPI_INSTALL_DEPS',libdrm2 libglib2.0-0 libpciaccess0 libgl1-mesa-glx LIBVA_INSTALL_DEP)'
)

ifelse(OS_NAME,centos,dnl
`define(`GSTVAAPI_BUILD_DEPS',meson wget tar gcc-c++ glib2-devel libdrm-devel LIBVA_DEV_BUILD_DEP bison flex)'
`define(`GSTVAAPI_INSTALL_DEPS',glib2 libdrm libpciaccess LIBVA_INSTALL_DEP)'
)

popdef(`LIBVA_DEV_BUILD_DEP')

define(`BUILD_GSTVAAPI',
ARG GSTVAAPI_REPO=https://github.com/GStreamer/gstreamer-vaapi/archive/GSTCORE_VER.tar.gz
RUN cd BUILD_HOME && \
  wget -O - ${GSTVAAPI_REPO} | tar xz

ifelse(GST_VAAPI_WITH_GVA_PATCH,true,`dnl
ARG GSTREAMER_VAAPI_PATCH_VER=GST_VAAPI_GVA_PATCH_VER
ARG GSTREAMER_VAAPI_PATCH_URL="GST_VAAPI_PATCH_URL"
RUN cd BUILD_HOME/gstreamer-vaapi-GSTCORE_VER && \
  ( for url in ${GSTREAMER_VAAPI_PATCH_URL}; do \
    curl -nfsSL ${url} | git apply || exit 1; \
  done )
')dnl

RUN cd BUILD_HOME/gstreamer-vaapi-GSTCORE_VER && \
  meson build \
    --prefix=BUILD_PREFIX \
    --libdir=BUILD_LIBDIR \
    --libexecdir=BUILD_LIBDIR \
    --buildtype=release \
    -Dgtk_doc=disabled \
    -Dexamples=disabled \
    -Dtests=disabled \
    -Ddoc=disabled \
    -Dwith_drm=GST_VAAPI_WITH_DRM \
    -Dwith_x11=GST_VAAPI_WITH_X11 \
    -Dwith_glx=GST_VAAPI_WITH_GLX \
    -Dwith_wayland=GST_VAAPI_WITH_WAYLAND \
    -Dwith_egl=GST_VAAPI_WITH_EGL && \
  cd build && \
  ninja install && \
  DESTDIR=BUILD_DESTDIR ninja install
)

define(`ENV_VARS_GSTVAAPI',
ENV GST_VAAPI_ALL_DRIVERS=1
)

REG(GSTVAAPI)

include(end.m4)
