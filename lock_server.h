// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"
#include <map>

class lock_server {

 protected:
  int nacquire;
  pthread_mutex_t mutex;
  std::map<lock_protocol::lockid_t, bool> lock_list;
  std::map<lock_protocol::lockid_t, pthread_cond_t> lock_cond_list;

 public:
  lock_server();
  ~lock_server() {};
  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, lock_protocol::status &);
  lock_protocol::status release(int clt, lock_protocol::lockid_t lid, lock_protocol::status &);
};

#endif 







