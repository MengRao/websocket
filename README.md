# websocket
A single header c++ websocket client/server lib for linux that implements [rfc6455](https://tools.ietf.org/html/rfc6455).

## Server class `WSServer`
As this lib uses template callback functions, user need to define 3 event handlers in his own class: `onWSConnect()`, `onWSClose()` and `onWSMsg()`:
```c++
  // called when a new websocket connection is about to open
  // optional: origin, protocol, extensions will be nullptr if not exist in the request headers
  // optional: fill resp_protocol[resp_protocol_size] to add protocol to response headers
  // optional: fill resp_extensions[resp_extensions_size] to add extensions to response headers
  // return true if accept this new connection
  bool onWSConnect(WSConnection& conn, const char* request_uri, const char* host, const char* origin, const char* protocol,
                   const char* extensions, char* resp_protocol, uint32_t resp_protocol_size, char* resp_extensions,
                   uint32_t resp_extensions_size) {
    ...
  }
                   
  // called when a websocket connection is closed
  // status_code 1005 means no status code in the close msg
  // status_code 1006 means not a clean close(tcp connection closed without a close msg)
  void onWSClose(WSConnection& conn, uint16_t status_code, const char* reason) {
    ...
  }
  
  // onWSMsg is used if RecvSegment == false(by default), called when a whole msg is received
  void onWSMsg(WSConnection& conn, uint8_t opcode, const uint8_t* payload, uint32_t pl_len) {
    ...
  }
```

To get the server running, user calls `init()` once to start the server and `poll()` repetitively to trigger events defined above:
```c++
  // newconn_timeout: new tcp connection max inactive time in milliseconds, 0 means no limit
  // openconn_timeout: open ws connection max inactive time in milliseconds, 0 means no limit
  // if failed, call getLastError() for the reason
  bool init(const char* server_ip, uint16_t server_port, uint64_t newconn_timeout = 0, uint64_t openconn_timeout = 0);
  
  // non-blocking
  void poll(EventHandler* handler);
```

## Connection class `WSConnection`
User can't create `WSConnection` objects himself, but only get `WSConnection` references from event handler parameters. However user is allowed to save the reference as long as the connection is not closed. `WSConnection` exposes below functions to use:
```c++
  // get remote network address
  bool getPeername(struct sockaddr_in& addr);
  
  // if not closed
  bool isConnected();
  
  // send a msg or a segment
  // if sending a msg of multiple segments, only set fin to true for the last one
  void send(uint8_t opcode, const uint8_t* payload, uint32_t pl_len, bool fin = true);
  
  // clean close the connection with optional status_code and reason
  // status_code 1005 means don't include status_code in close msg
  void close(uint16_t status_code = 1005, const char* reason = "");
```
It also allows to attach user-defined data structure to a `WSConnection` for user to operate on:
```c++
ConnUserData user_data;
```

## Client class `WSClient`
`WSClient` is actually a subclass of `WSConnection` with one additional connect function `wsConnect` which is blocking: 
```c++
  // timeout: connect timeout in milliseconds, 0 means no limit
  // if failed, call getLastError() for the reason
  bool wsConnect(uint64_t timeout, const char* server_ip, uint16_t server_port, const char* request_uri,
                 const char* host, const char* origin = nullptr, const char* protocol = nullptr,
                 const char* extensions = nullptr, char* resp_protocol = nullptr, uint32_t resp_protocol_size = 0,
                 char* resp_extensions = nullptr, uint32_t resp_extensions_size = 0)
```
And similar to `WSServer`, user need to define event handler function `onWSClose()` and `onWSMsg()`(but not `onWSConnect()`), and call the non-blocking `poll()` to trigger events.   

## Configurations and Limitations
Most of the server and client configurations are defined as class template parameters as below:
```c++
// EventHandler: user defined type defining the required event handler functions
// ConnUserData: user defined type attached to a WSConnection as member name `user_data`
// RecvSegment: switch to use segment handler function `onWSSegment` instead of `onWSMsg`
// RecvBufSize: msg/segment receive buffer size, too long msgs will cause connection being closed with status code 1009
// MaxConns: for WSServer only, max number of active connections
template<typename EventHandler, typename ConnUserData = char, bool RecvSegment = false, uint32_t RecvBufSize = 4096, uint32_t MaxConns = 10>
```
Note that if `RecvSegment` is set to true, `onWSSegment()` will be called in replace of `onWSMsg()` with 2 additional parameters:
```c++
// pl_start_idx: index in the whole msg for the 1st byte of payload
// fin: whether it's the last segment
void onWSSegment(WSConnection& conn, uint8_t opcode, const uint8_t* payload, uint32_t pl_len, uint32_t pl_start_idx, bool fin);
```
And if the c++ standard is older than c++17, user need to define both event handlers even if only one will be called(c++17 introduces `if constexpr` which allows only one function to be defined).

One implementation issue for client: client is not fully conformant to rfc6455 in that handshaking key and msg masking key are of constant values instead of random ones, for the purpose of efficiency and simplicity.

One performance consideration for server: as the library is using a simple busy-polling model instead of things like `epoll`, the number of active connections should be limited, as the default value of `MaxConns` implies.

Thread safety: this lib is not thread safe.

TLS support: this lib does not support TLS.

## Examples
Example codes implement a [admincmd](https://github.com/MengRao/admincmd) client and server on top of websocket layer, allowing the author to add a web client and reusing all of the existing admin commands provided by the c++ server.
