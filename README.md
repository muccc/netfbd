netfbd
======

framebuffer streaming daemon

### Prerequisites

Make sure you have loaded the kernel module `vfb` with the option `vfb_enable=1`:

    $ modprobe vfb vfb_enable=1
    
### Build

    $ cd src
    $ make
