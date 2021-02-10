/*
 * Constant frame reader -> pushes to proxy server
 * Butchered protonect basically
 *
 * Compilation instructions:
 * cmake .. -G "Visual Studio 15 2017 Win64"
 * cmake --build . --config RelWithDebInfo --target install
 */

/** @file Protonect.cpp Main application file. */

/// ADDED definitions for sockets
#undef UNICODE
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x501
#define DEFAULT_BUFLEN 512    // TODO CHANGE THIS to libfreenect size
#define DEFAULT_ADDR "127.0.0.1"
#define DEFAULT_PORT "27015"

#include <iostream>
#include <cstdlib>
#include <signal.h>

/// [headers]
#include <libfreenect2/libfreenect2.hpp>
#include <libfreenect2/frame_listener_impl.h>
#include <libfreenect2/packet_pipeline.h>
#include <libfreenect2/logger.h>

/// ADDED [socket headers]
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

bool protonect_shutdown = false; ///< Whether the running application should shut down.

void sigint_handler(int s)
{
  protonect_shutdown = true;
}

bool protonect_paused = false;
libfreenect2::Freenect2Device *devtopause;

void sigusr1_handler(int s)
{
  if (devtopause == 0)
    return;
/// [pause]
  if (protonect_paused)
    devtopause->start();
  else
    devtopause->stop();
  protonect_paused = !protonect_paused;
/// [pause]
}

/// [main]
/**
 * Main application entry point.
 */
int main(int argc, char *argv[])
/// [main]
{
/// [context]
  libfreenect2::Freenect2 freenect2;
  libfreenect2::Freenect2Device *dev = 0;
  libfreenect2::PacketPipeline *pipeline = 0;
/// [context]

  std::string serial = "";
  bool enable_depth = true;
  int deviceId = -1;
  pipeline = new libfreenect2::OpenCLKdePacketPipeline(deviceId);

/// [discovery]
  if(freenect2.enumerateDevices() == 0)
  {
    std::cout << "no device connected!" << std::endl;
    return -1;
  }
  if (serial == "")
  {
    serial = freenect2.getDefaultDeviceSerialNumber();
  }
/// [discovery]

/// [open]
  dev = freenect2.openDevice(serial, pipeline);
/// [open]

  if(dev == 0)
  {
    std::cout << "failure opening device!" << std::endl;
    return -1;
  }

  devtopause = dev;
  signal(SIGINT,sigint_handler);
#ifdef SIGUSR1
  signal(SIGUSR1, sigusr1_handler);
#endif
  protonect_shutdown = false;

/// [listeners]
  int types = 0;
  types |= libfreenect2::Frame::Depth;
  libfreenect2::SyncMultiFrameListener listener(types);
  libfreenect2::FrameMap frames;
  libfreenect2::Freenect2Device::Config config;
  config.MaxDepth = 18.5;

  dev->setConfiguration(config);
  dev->setColorFrameListener(&listener);
  dev->setIrAndDepthFrameListener(&listener);
/// [listeners]

/// [start]
  if (!dev->startStreams(0, enable_depth))
    return -1;

  std::cout << "device serial: " << dev->getSerialNumber() << std::endl;
  std::cout << "device firmware: " << dev->getFirmwareVersion() << std::endl;
/// [start]

  size_t framecount = 0;

/// Added INITIALIZE SOCKET HERE
  char* bwValues = (char*)malloc(512*424);
  WSADATA wsaData;
  SOCKET ConnectSocket = INVALID_SOCKET;
  struct addrinfo *result = NULL,
                  *ptr = NULL,
                  hints;
  int iResult;
  iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
  if (iResult != 0) {
      printf("WSAStartup failed with error: %d\n", iResult);
      return 1;
  }
  ZeroMemory(&hints, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  // Resolve the server address and port
  iResult = getaddrinfo(DEFAULT_ADDR, DEFAULT_PORT, &hints, &result);
  if ( iResult != 0 ) {
      printf("getaddrinfo failed with error: %d\n", iResult);
      WSACleanup();
      return 1;
  }

  // Attempt to connect to an address until one succeeds
  for(ptr=result; ptr != NULL ;ptr=ptr->ai_next) {
    // Create a SOCKET for connecting to server
    ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
        ptr->ai_protocol);
    if (ConnectSocket == INVALID_SOCKET) {
      printf("socket failed with error: %ld\n", WSAGetLastError());
      WSACleanup();
      return 1;
    }

    // Connect to server.
    iResult = connect( ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
      closesocket(ConnectSocket);
      ConnectSocket = INVALID_SOCKET;
      continue;
    }
    break;
  }

  freeaddrinfo(result);

/// [loop start]
  while(!protonect_shutdown)
  {
    if (!listener.waitForNewFrame(frames, 10*1000)) // 10 sconds
    {
      std::cout << "timeout!" << std::endl;
      return -1;
    }
    libfreenect2::Frame *depth = frames[libfreenect2::Frame::Depth];
/// [loop start]

    /// ADDED socket logic test
    // Send stuff
    float *frame_data = (float *)depth->data;
    for (int i = 0; i < depth->height; i++){
      for (int j=0; j < depth->width; j++){
        bwValues[i*depth->width + j] = static_cast<char>(frame_data[i * depth->width + j]);
      }
    }
    /// reinterpret_cast<const char*>(depth->data)
    // iResult = send( ConnectSocket, bwValues, (depth->width)*(depth->height), 0 );
    iResult = send( ConnectSocket, (char *)frame_data, (depth->width)*(depth->height)*4, 0 );
    if (iResult == SOCKET_ERROR) {
        printf("send failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    framecount++;
    if (framecount % 100 == 0)
      std::cout << "Received " << framecount << " frames. Ctrl-C to stop." << std::endl;

/// [loop end]
    listener.release(frames);
    /** libfreenect2::this_thread::sleep_for(libfreenect2::chrono::milliseconds(100)); */
  }
/// [loop end]


/// [stop]
  dev->stop();
  dev->close();
/// [stop]
  return 0;
}
