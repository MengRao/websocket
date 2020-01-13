#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <signal.h>
#include <string>
#include "../websocket.h"

class Client
{
public:
  using WSClient = websocket::WSClient<Client>;
  using WSConn = WSClient::Connection;

  void run() {
    if (!wsclient.wsConnect(3000, "127.0.0.1", 1234, "/", "127.0.0.1:1234")) {
      std::cout << "wsclient connect failed: " << wsclient.getLastError() << std::endl;
      return;
    }
    running = true;
    ws_thr = std::thread([this]() {
      while (running.load(std::memory_order_relaxed) && wsclient.isConnected()) {
        {
          std::lock_guard<std::mutex> lck(mtx);
          wsclient.poll(this);
        }
        std::this_thread::yield();
      }
    });

    std::cout << "Client running..." << std::endl;
    std::string line;
    while (running.load(std::memory_order_relaxed) && std::getline(std::cin, line)) {
      std::lock_guard<std::mutex> lck(mtx);
      wsclient.send(websocket::OPCODE_TEXT, (const uint8_t*)line.data(), line.size());
    }
    stop();

    ws_thr.join();
    std::cout << "Client stopped..." << std::endl;
  }

  void stop() { running = false; }

  void onWSClose(WSConn& conn, uint16_t status_code, const char* reason) {
    std::cout << "ws close, status_code: " << status_code << ", reason: " << reason << std::endl;
  }

  void onWSMsg(WSConn& conn, uint8_t opcode, const uint8_t* payload, uint32_t pl_len) {
    std::cout.write((const char*)payload, pl_len);
    std::cout << std::endl;
  }

  // no need to define onWSSegment if using c++17
  void onWSSegment(WSConn& conn, uint8_t opcode, const uint8_t* payload, uint32_t pl_len, uint32_t pl_start_idx,
                   bool fin) {
    std::cout << "error: onWSSegment should not be called" << std::endl;
  }

private:
  WSClient wsclient;
  std::mutex mtx;
  std::thread ws_thr;
  std::string admincmd_help;
  std::atomic<bool> running;
};

Client client;

void my_handler(int s) {
  client.stop();
}

int main(int argc, char** argv) {
  struct sigaction sigIntHandler;

  sigIntHandler.sa_handler = my_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;

  sigaction(SIGINT, &sigIntHandler, NULL);

  client.run();
}

