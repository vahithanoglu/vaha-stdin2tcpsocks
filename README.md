# vaha-stdin2tcpsocks

#### What is it?
A command line program to relay/multiplex stream from the standard input to the clients connected via TCP sockets.

#### What is its benefit?
Without requiring complex and/or heavy programs on your device, you can easily multiplex your streams to your network. It also supports to relay/multiplex streams as **HTTP application/octet-stream** in addition to raw TCP.

#### How is it built?
You can straightforwardly compile it with **gcc**.
```
gcc main.c -lpthread -o vaha-stdin2tcpsocks
```

#### How is its usage?
The generic command line usage is as follow:
```
./vaha-stdin2tcpsocks {IPv4_TO_BIND} {PORT_TO_BIND} [--enable-http]

IPv4_TO_BIND  : One of the device IPs (including loopback) to bind the server socket to (0.0.0.0 for all IPs)
PORT_TO_BIND  : The port number to bind the server socket to
--enable-http : The optional flag to enable "HTTP application/octet-stream"
 ```
 
 #### Does it have any calibration?
 Currently, there are two ```#define```s in the codebase for small calibrations. Based on your requirements and/or based on the processing power of your device, you can calibrate the program by modifying those values before its compilation:
 - [```#define MAX_CONN_COUNT 4```](https://github.com/vahithanoglu/vaha-stdin2tcpsocks/blob/main/main.c#L36): Defines the maximum number of concurrent connections allowed.
 - [```#define BUFFER_SIZE 1024```](https://github.com/vahithanoglu/vaha-stdin2tcpsocks/blob/main/main.c#L37): Defines the size of the buffer used while reading from the standard input and while writing to the clients connected via TCP sockets.
 
#### Is there any practical usage example?
Yes, there is :) Let's assume you have a Raspberry Pi Zero W on which a Pi Camera is installed; and you want to stream your camera on your network, so multiple clients can connect and watch the stream with the capability of disconnecting and connecting again at any time.

To do so, you can;
 - Stream the camera with ```raspivid``` to the standard output (*raspivid outputs raw H264*)
 - Pipe the stream to the standard input of ```ffmpeg``` which encapsulates it with MPEG-TS and streams to the standard output
 - Pipe the encapsulated stream to the standard input of ```vaha-stdin2tcpsocks``` which relays/multiplexes it to the clients when they are connected

Here is the command for the example explained above:
```
raspivid --timeout 0 --width 640 --height 480 --framerate 25 --nopreview --output - \
| ffmpeg -i - -acodec none -vcodec copy -loglevel quiet -f mpegts - \
| vaha-stdin2tcpsocks 0.0.0.0 8080 --enable-http
```
