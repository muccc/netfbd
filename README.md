netfbd
======

`netfbd` is a simple framebuffer streaming daemon. Small panels that form a full
display can connect to the daemon, which is managing a virtual framebuffer device
of the full display size. It streams each part of the virtual framebuffer to the
respective panel.

It was originally written for the Flipdot-Matrix of the MuCCC: https://wiki.muc.ccc.de/flipdot:start

### Prerequisites

Make sure you have loaded the kernel module `vfb` with the option `vfb_enable=1`:

    $ modprobe vfb vfb_enable=1

### Build

    $ cd src
    $ make
