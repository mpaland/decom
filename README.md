decom
==================


This is the decom (**DE**vice **COM**munication) library.  
decom is a universal, cross platform, NON BLOCKING, high performance c++ communication library.
It is intended to be a minimalistic abstraction of the OSI layer model.


## Design goals:
- CLEAN code without any warnings on highest compiler level (4), LINT checked
- Production/industrial/automotive code quality
- Cross platform and embedded support
- Header only, no modules
- ONLY usage of standard C++, (C++11) and the STL library
- ONLY usage of standard stdint.h (cstdint) and bool datatypes
- NO other external libraries nor other platform dependencies except for util and communication layers which have architecture dependent folders (windows, linux etc.) because they must touch the hardware, OS API or vendor libs
- Own static/pool message memory management - NO usage of any dynamic memory allocation
- Zero copy of message data - if possible

### Usage of STL containers
In the moment, some decom classes use STL containers like `<vector>`.  
On embedded systems special STL libs like "ustl" may be used.
In later versions, any usage of STL containers should be avoided due to dynamic memory management, which makes decom not suitable for automotive and small embedded applications.


## Class overview
The following layer classification is used:

![](https://cdn.rawgit.com/mpaland/decom/master/docs/layer.svg)

The decom stack consists of exactly one `com` (communicator), one `dev` (device) and any number of `prot`s (protocols).  
Interaction between the layers is done by the following 5 decom standard interface functions:

- `open()`
  Open this layer to send/receive data.
- `close()`
  Close this layer.
- `send()`
  Called by the upper layer to send data to this layer.
- `receive()`
  Called by the lower layer to pass received data to this layer.
- `indication()`
  Called by the lower layer to indicate a status/error condition to this layer.

These functions MUST be implemented by every layer.
Each layer can have further specific API functions to set layer specific protocol params like baudrate, timings, flow control etc. e.g. and can have a special param ctor(s).  
Why is there only one indication function in device direction? An indication function in com direction is not necessary because in many protocols incoming data can't be controlled or throtteled. The upper layers just have to deal with this and can't say the network to slow down or halt. 


### Device layer
This is the top most layer, often referred as the application layer (7) in OSI model.
This upper layer has only one interface to its lower layer and device specific functions which are used by the application (like read() or write()).
The namespace for devices is `decom::dev`.

### Protocol layer
This layer has two interfaces. One to the upper layer and one to the lower layer. It's layer (3) to (6) in OSI model.
Typically a protocol layer does clothing, stripping, checksum calculations, flow control etc.
Any desired count of protocol layers can be chained together.
Protocols may be inserted/deleted dynamically out of the stack. Dynamic object creation is supported.
Try to avoid using threads in protocol layers cause not all platforms may support threads, use `util::timer` instead.
The namespace for protocols is `decom::prot`.

### Communication layer
This is the lowest OSI layer (2). It has only one interface to an upper layer and sends/receives data to/from the hardware or OS API/HAL.
This layer is arcitecture dependent.
The namespace for communicators is `decom::com`.


### msg class
decom stores all communication data in a special `msg` class. The `msg` class itself stores data in one or more pages, provided by a static msg pool.
The `msg` class has all functions and iterators of a normal STL container class. It's very similar to the 'deque' container, but has some major advantages like static memory management, data copies by reference counting etc.


## Stack creation
The stack is always created bottom-top - meaning bottom (com) layer FIRST to top (dev) layer last.
Dynamic protocol generation, binding and unbinding (layer delete) is supported.
Due to this mechanism it is possible to insert, for example, a file transfer protocol like
xmodem between a device and a communication port.


The minimalistic stack is a device bound to a communication class without any protocol:

```c++
// include the necessary headers, include path must include the 'decom' folder
#include "prot/prot_debug.h"
#include "com/com_serial.h"
#include "dev/dev_generic.h"

// create a mini stack for RS232 communication
decom::com::serial  ser(9600U);   // use 9600 baud
decom::prot::debug  dbg(&ser);    // debug layer
decom::dev::generic dev(&dbg);

// then use the stack
dev.open("COM1");       // open the COM1
dev.write(...);         // write something to device
dev.read(...);          // read something from device
dev.close();            // close device (and implicit close the rest of the stack)
```

Example for a more complex stack creation with 4 chained protocols:

```c++
// create the stack
decom::com::serial   ser(115200);           // layer2 - 115 kBaud and default params
decom::prot::ppp     ppp1(&ser);            // layer2 - create layer 2 tranport protocol
decom::prot::test1   prot1(&ppp1);          // layer3 - create layer 3 routing protocol
decom::prot::test2   prot2(&prot1, p1, p2); // layer4 - create layer 4 transport protocol with additional params P1 and P2
decom::prot::session sess(&prot2);          // layer5 - create layer 5 session protocol
decom::dev::generic  dev(&sess);            // layer7 - create generic device and bind to session protcol

// use the stack
dev.open("COM2");        // open COM2 (and implicit open the rest of the stack)
dev.write(...);          // write something to device
dev.read(...);           // read something from device
dev.close();             // close device (and implicit close the rest of the stack)
```


## Contributing

1. Create an issue and describe your idea
2. [Fork it](https://github.com/mpaland/decom/fork)
3. Create your feature branch (`git checkout -b my-new-feature`)
4. Commit your changes (`git commit -am 'Add some feature'`)
5. Publish the branch (`git push origin my-new-feature`)
6. Create a new Pull Request
7. Profit! :white_check_mark:


## License

decom is released under the [MIT license](http://www.opensource.org/licenses/MIT).